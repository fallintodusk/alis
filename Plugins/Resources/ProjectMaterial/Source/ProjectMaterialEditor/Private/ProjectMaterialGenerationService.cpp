// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialGenerationService.h"

#include "ProjectMaterialManifest.h"
#include "ProjectMaterialRecipe.h"
#include "ProjectMaterialTerrainBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMaterialGeneration, Log, All);

namespace ProjectMaterialGenerationPrivate
{
constexpr TCHAR ProductionPackageRoot[] = TEXT("/ProjectMaterial/Generated");
constexpr TCHAR TestPackageRoot[] = TEXT("/ProjectMaterialTest/Generated");
constexpr TCHAR ManifestName[] = TEXT("accepted.material-manifest.json");

bool IsUnderDirectory(const FString& Child, const FString& Parent)
{
	FString AbsoluteChild = FPaths::ConvertRelativePathToFull(Child);
	FString AbsoluteParent = FPaths::ConvertRelativePathToFull(Parent);
	FPaths::NormalizeDirectoryName(AbsoluteChild);
	FPaths::NormalizeDirectoryName(AbsoluteParent);
	return AbsoluteChild.Equals(AbsoluteParent, ESearchCase::IgnoreCase) ||
		FPaths::IsUnderDirectory(AbsoluteChild, AbsoluteParent);
}

bool ValidateRequest(
	const FProjectMaterialGenerationRequest& Request,
	bool bMutating,
	bool bCommandletOwner,
	FString& OutError)
{
	if (Request.RecipeRoot.IsEmpty() || Request.OutputContentRoot.IsEmpty() || Request.ManifestRoot.IsEmpty())
	{
		OutError = TEXT("Recipe, output-content, and manifest roots are required.");
		return false;
	}
	const bool bProduction = Request.OutputPackageRoot == ProductionPackageRoot;
	const bool bTest = Request.OutputPackageRoot == TestPackageRoot;
	if (!bProduction && !bTest)
	{
		OutError = TEXT("Output package root is not an admitted ProjectMaterial owner mount.");
		return false;
	}
	if (bMutating && bProduction && (!bCommandletOwner || !IsRunningCommandlet()))
	{
		OutError = TEXT("Production ProjectMaterial mutation requires the dedicated one-shot commandlet.");
		return false;
	}
	if (bMutating && bTest)
	{
		const FString AllowedRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/material/generation"));
		if (!IsUnderDirectory(Request.RecipeRoot, AllowedRoot) ||
			!IsUnderDirectory(Request.OutputContentRoot, AllowedRoot) ||
			!IsUnderDirectory(Request.ManifestRoot, AllowedRoot))
		{
			OutError = FString::Printf(
				TEXT("Test mutation is confined to project tmp/material/generation - allowed=%s recipes=%s output=%s manifests=%s"),
				*FPaths::ConvertRelativePathToFull(AllowedRoot),
				*FPaths::ConvertRelativePathToFull(Request.RecipeRoot),
				*FPaths::ConvertRelativePathToFull(Request.OutputContentRoot),
				*FPaths::ConvertRelativePathToFull(Request.ManifestRoot));
			return false;
		}
	}
	return true;
}

bool HashFile(const FString& Path, FString& OutHash, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutError = FString::Printf(TEXT("Could not read generated package: %s"), *Path);
		return false;
	}
	OutHash = FProjectMaterialRecipeContract::ComputeSha256(Bytes);
	if (OutHash.Len() != 64)
	{
		OutError = FString::Printf(TEXT("Could not hash generated package: %s"), *Path);
		return false;
	}
	return true;
}

bool DiscoverRecipes(
	const FProjectMaterialGenerationRequest& Request,
	TArray<FProjectMaterialRecipe>& OutRecipes,
	FString& OutError)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(
		Files, *Request.RecipeRoot, TEXT("*.material.json"), true, false, false);
	Files.Sort();
	TSet<FString> MaterialIds;
	TSet<FString> OutputPaths;
	for (const FString& File : Files)
	{
		FString Json;
		FProjectMaterialRecipe Recipe;
		if (!FFileHelper::LoadFileToString(Json, *File) ||
			!FProjectMaterialRecipeContract::Parse(Json, File, Request.RecipeRoot, Recipe, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Could not read material recipe: %s"), *File);
			}
			return false;
		}
		FString PackageName;
		FString ObjectPath;
		if (!FProjectMaterialRecipeContract::ResolveOutputIdentity(
			Recipe, Request.OutputPackageRoot, PackageName, ObjectPath, OutError))
		{
			return false;
		}
		if (MaterialIds.Contains(Recipe.MaterialId) || OutputPaths.Contains(ObjectPath))
		{
			OutError = FString::Printf(TEXT("Duplicate material identity or output collision: %s"), *ObjectPath);
			return false;
		}
		MaterialIds.Add(Recipe.MaterialId);
		OutputPaths.Add(ObjectPath);
		OutRecipes.Add(MoveTemp(Recipe));
	}
	OutRecipes.Sort([](const auto& Left, const auto& Right)
	{
		if (Left.ArtifactKind != Right.ArtifactKind)
		{
			return Left.ArtifactKind == EProjectMaterialArtifactKind::Parent;
		}
		return Left.MaterialId < Right.MaterialId;
	});
	return true;
}

bool ValidateParentRecipeAuthority(
	const FProjectMaterialGenerationRequest& Request,
	const TArray<FProjectMaterialRecipe>& Recipes,
	FString& OutError)
{
	TSet<FString> DeclaredParentObjectPaths;
	for (const FProjectMaterialRecipe& Recipe : Recipes)
	{
		if (Recipe.ArtifactKind != EProjectMaterialArtifactKind::Parent)
		{
			continue;
		}
		FString PackageName;
		FString ObjectPath;
		if (!FProjectMaterialRecipeContract::ResolveOutputIdentity(
			Recipe, Request.OutputPackageRoot, PackageName, ObjectPath, OutError))
		{
			return false;
		}
		DeclaredParentObjectPaths.Add(ObjectPath);
	}
	for (const FProjectMaterialRecipe& Recipe : Recipes)
	{
		if (Recipe.ArtifactKind != EProjectMaterialArtifactKind::Instance)
		{
			continue;
		}
		if (!Recipe.ParentObjectPath.StartsWith(Request.OutputPackageRoot + TEXT("/")))
		{
			OutError = FString::Printf(
				TEXT("Instance dependency escapes the generated owner: %s"), *Recipe.ParentObjectPath);
			return false;
		}
		if (!DeclaredParentObjectPaths.Contains(Recipe.ParentObjectPath))
		{
			OutError = FString::Printf(
				TEXT("Instance parent is not produced by a current declared parent recipe: %s"),
				*Recipe.ParentObjectPath);
			return false;
		}
	}
	return true;
}

bool ResolvePackageFile(const FString& PackageName, FString& OutFilename, FString& OutError)
{
	OutFilename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());
	if (OutFilename.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Could not resolve package filename: %s"), *PackageName);
		return false;
	}
	return true;
}

bool SaveAndReload(
	const FProjectMaterialRecipe& Recipe,
	const FString& PackageName,
	const FString& ObjectPath,
	UObject* Asset,
	FString& OutPackageHash,
	FString& OutError)
{
	UPackage* Package = Asset != nullptr ? Asset->GetOutermost() : nullptr;
	if (Package == nullptr)
	{
		OutError = TEXT("Generated asset has no package.");
		return false;
	}
	FString Filename;
	if (!ResolvePackageFile(PackageName, Filename, OutError))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.bWarnOfLongFilename = true;
	SaveArgs.bSlowTask = false;
	if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
	{
		OutError = FString::Printf(TEXT("Could not save generated package: %s"), *Filename);
		return false;
	}
	Package->SetDirtyFlag(false);
	UPackageTools::FlushAsyncCompilation({Package});
	if (!UPackageTools::UnloadPackages({Package}))
	{
		OutError = FString::Printf(TEXT("Could not release generated package before verification: %s"), *PackageName);
		return false;
	}
	CollectGarbage(RF_NoFlags);
	UObject* Reloaded = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if (Reloaded == nullptr || !ProjectMaterialTerrainBuilder::Verify(Recipe, Reloaded, OutError))
	{
		return false;
	}
	Reloaded->GetOutermost()->SetDirtyFlag(false);
	return HashFile(Filename, OutPackageHash, OutError);
}

bool BuildRecord(
	const FProjectMaterialRecipe& Recipe,
	const FString& ObjectPath,
	const FString& DependencyHash,
	const FString& PackageHash,
	FProjectMaterialManifestRecord& OutRecord)
{
	OutRecord.RecipePath = Recipe.SourcePath;
	OutRecord.RecipeSha256 = Recipe.RecipeSha256;
	OutRecord.Family = Recipe.Family;
	OutRecord.Archetype = Recipe.Archetype;
	OutRecord.CompilerVersion = Recipe.CompilerVersion;
	OutRecord.DependencyObjectPath = Recipe.ParentObjectPath;
	OutRecord.DependencyPackageSha256 = DependencyHash;
	OutRecord.OutputObjectPath = ObjectPath;
	OutRecord.CompilerFingerprint = FProjectMaterialRecipeContract::GetCompilerFingerprint();
	OutRecord.EngineIdentity = FProjectMaterialRecipeContract::GetEngineIdentity();
	OutRecord.SemanticIdentity = FProjectMaterialRecipeContract::ComputeArtifactSemanticIdentity(
		Recipe, ObjectPath, DependencyHash);
	OutRecord.PackageSha256 = PackageHash;
	return OutRecord.SemanticIdentity.Len() == 64;
}

bool IsAcceptedSkip(
	const FProjectMaterialManifestRecord* Existing,
	const FProjectMaterialManifestRecord& Candidate,
	const FString& PackageFile,
	FString& OutError)
{
	if (Existing == nullptr || Existing->SemanticIdentity != Candidate.SemanticIdentity ||
		!FPaths::FileExists(PackageFile))
	{
		return false;
	}
	return FProjectMaterialGenerationService::HasAcceptedOutputIntegrity(
		PackageFile, Existing->PackageSha256);
}

bool ReportAndCleanOrphans(
	const FProjectMaterialGenerationRequest& Request,
	const TSet<FString>& AcceptedPackages,
	FProjectMaterialGenerationResult& OutResult)
{
	TArray<FString> PackageFiles;
	IFileManager::Get().FindFilesRecursive(
		PackageFiles, *Request.OutputContentRoot, TEXT("*.uasset"), true, false, false);
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanFilesSynchronous(PackageFiles, true);
	for (const FString& PackageFile : PackageFiles)
	{
		FString PackageName;
		if (!FPackageName::TryConvertFilenameToLongPackageName(PackageFile, PackageName) ||
			AcceptedPackages.Contains(PackageName))
		{
			continue;
		}
		OutResult.OrphanPackageNames.Add(PackageName);
		if (!Request.bCleanupOrphans)
		{
			continue;
		}
		TArray<FName> Referencers;
		Registry.GetReferencers(FName(PackageName), Referencers);
		if (!Referencers.IsEmpty())
		{
			UE_LOG(LogProjectMaterialGeneration, Warning,
				TEXT("[GenerationService::Orphans] Referenced orphan retained - package=%s referencers=%d"),
				*PackageName, Referencers.Num());
			continue;
		}
		IFileManager::Get().Delete(*PackageFile, false, true, true);
	}
	OutResult.OrphanPackageNames.Sort();
	return true;
}
}

bool FProjectMaterialGenerationService::Validate(
	const FProjectMaterialGenerationRequest& Request,
	FProjectMaterialGenerationResult& OutResult)
{
	OutResult = {};
	if (!ProjectMaterialGenerationPrivate::ValidateRequest(Request, false, false, OutResult.Error))
	{
		return false;
	}
	TArray<FProjectMaterialRecipe> Recipes;
	if (!ProjectMaterialGenerationPrivate::DiscoverRecipes(Request, Recipes, OutResult.Error))
	{
		return false;
	}
	OutResult.Validated = Recipes.Num();
	return ProjectMaterialGenerationPrivate::ValidateParentRecipeAuthority(
		Request, Recipes, OutResult.Error);
}

bool FProjectMaterialGenerationService::RegenerateTestMount(
	const FProjectMaterialGenerationRequest& Request,
	FProjectMaterialGenerationResult& OutResult)
{
	return RegenerateInternal(Request, false, OutResult);
}

bool FProjectMaterialGenerationService::HasAcceptedOutputIntegrity(
	const FString& PackageFile,
	const FString& AcceptedSha256)
{
	if (AcceptedSha256.Len() != 64 || !FPaths::FileExists(PackageFile))
	{
		return false;
	}
	FString CurrentHash;
	FString Error;
	return ProjectMaterialGenerationPrivate::HashFile(PackageFile, CurrentHash, Error) &&
		CurrentHash == AcceptedSha256;
}

bool FProjectMaterialGenerationService::RegenerateForCommandlet(
	const FProjectMaterialGenerationRequest& Request,
	FProjectMaterialGenerationResult& OutResult)
{
	return RegenerateInternal(Request, true, OutResult);
}

bool FProjectMaterialGenerationService::RegenerateInternal(
	const FProjectMaterialGenerationRequest& Request,
	bool bCommandletOwner,
	FProjectMaterialGenerationResult& OutResult)
{
	using namespace ProjectMaterialGenerationPrivate;
	OutResult = {};
	if (!ValidateRequest(Request, true, bCommandletOwner, OutResult.Error))
	{
		return false;
	}
	TArray<FProjectMaterialRecipe> Recipes;
	if (!DiscoverRecipes(Request, Recipes, OutResult.Error))
	{
		return false;
	}
	OutResult.Validated = Recipes.Num();
	if (!ValidateParentRecipeAuthority(Request, Recipes, OutResult.Error))
	{
		return false;
	}
	const FString ManifestPath = FPaths::Combine(Request.ManifestRoot, ManifestName);
	TMap<FString, FProjectMaterialManifestRecord> Existing;
	if (!ProjectMaterialManifest::Load(ManifestPath, Existing, OutResult.Error))
	{
		return false;
	}
	TArray<FProjectMaterialManifestRecord> Accepted;
	TMap<FString, FString> AcceptedPackageHashes;
	TSet<FString> AcceptedPackageNames;
	for (const FProjectMaterialRecipe& Recipe : Recipes)
	{
		FString PackageName;
		FString ObjectPath;
		if (!FProjectMaterialRecipeContract::ResolveOutputIdentity(
			Recipe, Request.OutputPackageRoot, PackageName, ObjectPath, OutResult.Error))
		{
			return false;
		}
		FString PackageFile;
		if (!ResolvePackageFile(PackageName, PackageFile, OutResult.Error))
		{
			return false;
		}
		FString DependencyHash;
		if (Recipe.ArtifactKind == EProjectMaterialArtifactKind::Instance)
		{
			const FString ParentPackage = FPackageName::ObjectPathToPackageName(Recipe.ParentObjectPath);
			if (const FString* AcceptedHash = AcceptedPackageHashes.Find(ParentPackage))
			{
				DependencyHash = *AcceptedHash;
			}
			else
			{
				OutResult.Error = FString::Printf(
					TEXT("Instance parent was not accepted from the current declared recipe set: %s"),
					*Recipe.ParentObjectPath);
				return false;
			}
		}
		FProjectMaterialManifestRecord Candidate;
		BuildRecord(Recipe, ObjectPath, DependencyHash, TEXT("pending"), Candidate);
		FString SkipError;
		if (IsAcceptedSkip(Existing.Find(ObjectPath), Candidate, PackageFile, SkipError))
		{
			Candidate.PackageSha256 = Existing[ObjectPath].PackageSha256;
			Accepted.Add(Candidate);
			AcceptedPackageHashes.Add(PackageName, Candidate.PackageSha256);
			AcceptedPackageNames.Add(PackageName);
			OutResult.OutputObjectPaths.Add(ObjectPath);
			++OutResult.Skipped;
			continue;
		}
		if (FPaths::FileExists(PackageFile))
		{
			if (!Request.bHostTransactionOwnsReplacement)
			{
				OutResult.Error = FString::Printf(
					TEXT("Existing output replacement requires the host transaction owner: %s"), *ObjectPath);
				return false;
			}
			if (!IFileManager::Get().Delete(*PackageFile, false, true, true))
			{
				OutResult.Error = FString::Printf(
					TEXT("Could not clear existing output for clean reconstruction: %s"), *PackageFile);
				return false;
			}
		}
		UObject* Asset = nullptr;
		TArray<FString> CompileErrors;
		const bool bBuilt = Recipe.ArtifactKind == EProjectMaterialArtifactKind::Parent
			? ProjectMaterialTerrainBuilder::BuildParent(
				Recipe, PackageName, Asset, CompileErrors, OutResult.Error)
			: ProjectMaterialTerrainBuilder::BuildInstance(
				Recipe, PackageName, Asset, OutResult.Error);
		if (!bBuilt)
		{
			return false;
		}
		if (Recipe.ArtifactKind == EProjectMaterialArtifactKind::Parent)
		{
			++OutResult.ShaderCompiles;
		}
		FString PackageHash;
		if (!SaveAndReload(Recipe, PackageName, ObjectPath, Asset, PackageHash, OutResult.Error))
		{
			return false;
		}
		if (Request.FailureInjection == TEXT("post-save"))
		{
			OutResult.Error = TEXT("Injected failure after asset save and before manifest promotion.");
			return false;
		}
		BuildRecord(Recipe, ObjectPath, DependencyHash, PackageHash, Candidate);
		Accepted.Add(Candidate);
		AcceptedPackageHashes.Add(PackageName, PackageHash);
		AcceptedPackageNames.Add(PackageName);
		OutResult.OutputObjectPaths.Add(ObjectPath);
		++OutResult.Generated;
	}
	ReportAndCleanOrphans(Request, AcceptedPackageNames, OutResult);
	if (OutResult.Generated > 0 || Existing.Num() != Accepted.Num() || !FPaths::FileExists(ManifestPath))
	{
		if (!ProjectMaterialManifest::Save(
			ManifestPath, Accepted, OutResult.ManifestSha256, OutResult.Error))
		{
			return false;
		}
	}
	else
	{
		FString ManifestHashError;
		if (!HashFile(ManifestPath, OutResult.ManifestSha256, ManifestHashError))
		{
			OutResult.Error = ManifestHashError;
			return false;
		}
	}
	UE_LOG(LogProjectMaterialGeneration, Display,
		TEXT("[GenerationService::Regenerate] Accepted - validated=%d generated=%d skipped=%d compiles=%d orphans=%d"),
		OutResult.Validated, OutResult.Generated, OutResult.Skipped,
		OutResult.ShaderCompiles, OutResult.OrphanPackageNames.Num());
	return true;
}
