// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldVerifyInternal.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ExternalPackageHelper.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "Utilities/ProjectSha256.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionMiniMap.h"

namespace ProjectWorldVerifyWorldDetail
{
	const TCHAR* MountRoot = TEXT("/ProjectWorldVerify/");
	const TCHAR* MapName = TEXT("L_Verify");

	FString MountDirectory()
	{
		return FPaths::Combine(ProjectWorldVerifyPaths::ProjectDirectory(), TEXT("tmp/world/realization_verify/Content/"));
	}

	TArray<UPackage*> LoadedPackagesUnder(const TArray<FString>& Roots)
	{
		TArray<UPackage*> Packages;
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			const FString Name = It->GetName();
			if (Roots.ContainsByPredicate([&Name](const FString& Root) { return Name.StartsWith(Root, ESearchCase::CaseSensitive); }))
			{
				Packages.Add(*It);
			}
		}
		return Packages;
	}

	bool Unload(const TArray<FString>& Roots, FString& OutError)
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
		TArray<UPackage*> Packages = LoadedPackagesUnder(Roots);
		if (!Packages.IsEmpty())
		{
			UPackageTools::FUnloadPackageParams Parameters(Packages);
			Parameters.bUnloadDirtyPackages = true;
			UPackageTools::UnloadPackages(Parameters);
		}
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		TArray<FString> Remaining;
		for (const UPackage* Package : LoadedPackagesUnder(Roots))
		{
			Remaining.Add(Package->GetName());
		}
		if (!Remaining.IsEmpty())
		{
			OutError = TEXT("Packages under the verify root are still loaded, so a reload would not read disk: ") +
				FString::Join(Remaining, TEXT(", "));
			return false;
		}
		return true;
	}

	// The editor adds this preview actor with a time-based identity; production removes it before saving.
	bool DestroyMiniMaps(UWorld* World)
	{
		TArray<AWorldPartitionMiniMap*> MiniMaps;
		for (TActorIterator<AWorldPartitionMiniMap> It(World); It; ++It)
		{
			MiniMaps.Add(*It);
		}
		for (AWorldPartitionMiniMap* MiniMap : MiniMaps)
		{
			if (!World->EditorDestroyActor(MiniMap, true))
			{
				return false;
			}
		}
		return true;
	}
}

struct FProjectWorldVerifyWorld::FImpl
{
	FString Root;
	// The map root plus the map's external actor and object roots, which the engine places
	// beside it under the mount; each with its directory.
	TArray<FString> PackageRoots;
	TArray<FString> Directories;
	FString MapPackage;
	FString MapFilename;
	UWorld* World = nullptr;
	bool bMounted = false;
	FString Error;
	TArray<FWorldPartitionReference> References;
};

FProjectWorldVerifyWorld::FProjectWorldVerifyWorld(const FString& Producer)
	: Impl(MakeUnique<FImpl>())
{
	using namespace ProjectWorldVerifyWorldDetail;
	bool bPlainName = !Producer.IsEmpty();
	for (const TCHAR Character : Producer)
	{
		bPlainName &= FChar::IsAlnum(Character);
	}
	if (!bPlainName || GEditor == nullptr)
	{
		Impl->Error = TEXT("Verify world needs a plain producer folder name and an editor.");
		return;
	}
	Impl->Root = FString(MountRoot) + Producer + TEXT("/");
	Impl->MapPackage = Impl->Root + MapName;
	if (!FPackageName::MountPointExists(MountRoot))
	{
		FPackageName::RegisterMountPoint(MountRoot, MountDirectory());
		Impl->bMounted = true;
	}
	// Both paths resolve through the mount, so they are computed after it exists.
	Impl->PackageRoots = {
		Impl->Root,
		ULevel::GetExternalActorsPath(Impl->MapPackage) + TEXT("/"),
		FExternalPackageHelper::GetExternalObjectsPath(Impl->MapPackage) + TEXT("/")};
	for (const FString& PackageRoot : Impl->PackageRoots)
	{
		FString Directory;
		if (!PackageRoot.StartsWith(MountRoot, ESearchCase::CaseSensitive) ||
			!FPackageName::TryConvertLongPackageNameToFilename(PackageRoot, Directory))
		{
			Impl->Error = FString::Printf(TEXT("Verify package root does not resolve under the mount: %s"), *PackageRoot);
			return;
		}
		Impl->Directories.Add(FPaths::ConvertRelativePathToFull(Directory));
	}
	if (!Unload(Impl->PackageRoots, Impl->Error))
	{
		return;
	}
	for (const FString& Directory : Impl->Directories)
	{
		IFileManager::Get().DeleteDirectory(*Directory, false, true);
	}
	IFileManager::Get().MakeDirectory(*Impl->Directories[0], true);
	Impl->MapFilename = FPackageName::LongPackageNameToFilename(Impl->MapPackage, FPackageName::GetMapPackageExtension());
	Impl->World = GEditor->NewMap(true);
	if (Impl->World == nullptr || !Impl->World->IsPartitionedWorld() || !DestroyMiniMaps(Impl->World) ||
		!FEditorFileUtils::SaveLevel(Impl->World->PersistentLevel, Impl->MapFilename) ||
		Impl->World->GetPackage()->GetName() != Impl->MapPackage)
	{
		Impl->Error = FString::Printf(TEXT("Cannot create the verify World Partition map %s."), *Impl->MapPackage);
		Impl->World = nullptr;
	}
}

FProjectWorldVerifyWorld::~FProjectWorldVerifyWorld()
{
	using namespace ProjectWorldVerifyWorldDetail;
	if (Impl->Root.IsEmpty())
	{
		return;
	}
	Impl->References.Reset();
	Impl->World = nullptr;
	if (GEditor != nullptr)
	{
		GEditor->NewMap(false);
	}
	FString Error;
	if (!Unload(Impl->PackageRoots, Error))
	{
		UE_LOG(LogProjectWorldVerify, Warning, TEXT("[ProjectWorldVerify] Cleanup kept loaded packages - %s"), *Error);
	}
	for (const FString& Directory : Impl->Directories)
	{
		IFileManager::Get().DeleteDirectory(*Directory, false, true);
	}
	if (Impl->bMounted)
	{
		FPackageName::UnRegisterMountPoint(MountRoot, MountDirectory());
	}
}

bool FProjectWorldVerifyWorld::IsReady(FString& OutError) const
{
	OutError = Impl->Error;
	return Impl->World != nullptr && Impl->Error.IsEmpty();
}

UWorld* FProjectWorldVerifyWorld::GetWorld() const
{
	return Impl->World;
}

const FString& FProjectWorldVerifyWorld::GetRoot() const
{
	return Impl->Root;
}

bool FProjectWorldVerifyWorld::SaveOwned(FString& OutError)
{
	using namespace ProjectWorldVerifyWorldDetail;
	if (Impl->World == nullptr || !DestroyMiniMaps(Impl->World))
	{
		OutError = TEXT("Verify world is unavailable for saving.");
		return false;
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	TArray<UPackage*> Dirty;
	for (UPackage* Package : LoadedPackagesUnder(Impl->PackageRoots))
	{
		if (Package->IsDirty())
		{
			Dirty.Add(Package);
		}
	}
	if (!Dirty.IsEmpty() && !UEditorLoadingAndSavingUtils::SavePackages(Dirty, true))
	{
		OutError = TEXT("Cannot save the verify world packages.");
		return false;
	}
	TArray<FString> StillDirty;
	for (const UPackage* Package : LoadedPackagesUnder(Impl->PackageRoots))
	{
		if (Package->IsDirty())
		{
			StillDirty.Add(Package->GetName());
		}
	}
	if (!StillDirty.IsEmpty())
	{
		OutError = TEXT("Verify world packages stay dirty after save: ") + FString::Join(StillDirty, TEXT(", "));
		return false;
	}
	UE_LOG(LogProjectWorldVerify, Display, TEXT("[ProjectWorldVerify] Saved verify world - root=%s saved=%d"),
		*Impl->Root, Dirty.Num());
	return true;
}

bool FProjectWorldVerifyWorld::Reload(FString& OutError)
{
	using namespace ProjectWorldVerifyWorldDetail;
	Impl->References.Reset();
	Impl->World = nullptr;
	GEditor->NewMap(false);
	if (!Unload(Impl->PackageRoots, OutError))
	{
		return false;
	}
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FString> Files;
	for (const FString& Directory : Impl->Directories)
	{
		IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*.uasset"), true, false, false);
	}
	AssetRegistry.ScanFilesSynchronous(Files, true);
	if (!FEditorFileUtils::LoadMap(Impl->MapFilename, false, false))
	{
		OutError = FString::Printf(TEXT("Cannot reload the verify map %s."), *Impl->MapPackage);
		return false;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	UWorldPartition* WorldPartition = World != nullptr ? World->GetWorldPartition() : nullptr;
	if (World == nullptr || World->GetPackage()->GetName() != Impl->MapPackage ||
		WorldPartition == nullptr || !WorldPartition->IsInitialized())
	{
		OutError = FString::Printf(TEXT("The reloaded editor world is not the verify map %s."), *Impl->MapPackage);
		return false;
	}
	WorldPartition->LoadAllActors(Impl->References);
	FAssetCompilingManager::Get().FinishAllCompilation();
	Impl->World = World;
	UE_LOG(LogProjectWorldVerify, Display, TEXT("[ProjectWorldVerify] Reloaded verify world - map=%s actors=%d"),
		*Impl->MapPackage, Impl->References.Num());
	return true;
}

TMap<FString, FString> FProjectWorldVerifyWorld::PackageDigests() const
{
	TMap<FString, FString> Digests;
	TArray<FString> Files;
	for (const FString& Directory : Impl->Directories)
	{
		IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*"), true, false, false);
	}
	const FString Mount = FPaths::ConvertRelativePathToFull(ProjectWorldVerifyWorldDetail::MountDirectory());
	for (const FString& File : Files)
	{
		FString Relative = File;
		FPaths::MakePathRelativeTo(Relative, *Mount);
		FString Digest;
		Digests.Add(Relative.Replace(TEXT("\\"), TEXT("/")),
			FProjectSha256::HashFile(File, Digest) ? Digest : FString(TEXT("unreadable")));
	}
	return Digests;
}
