// Copyright ALIS. All Rights Reserved.

#include "Experience/InitialExperienceLoader.h"
#include "ProjectLoadingLog.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "Experience/ProjectExperienceRegistry.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"

namespace
{
	void AddAssetIds(const TArray<FSoftObjectPath>& From, TArray<FPrimaryAssetId>& To, UAssetManager& AssetManager)
	{
		for (const FSoftObjectPath& Path : From)
		{
			if (!Path.IsNull())
			{
				const FPrimaryAssetId Id = AssetManager.GetPrimaryAssetIdForPath(Path);
				if (Id.IsValid())
				{
					To.Add(Id);
				}
			}
		}
	}
}

FInitialExperienceLoader::FInitialExperienceLoader(UProjectExperienceRegistry* InRegistry)
	: Registry(InRegistry)
{
	UE_LOG(LogProjectLoading, Verbose, TEXT("InitialExperienceLoader: Initialized with registry=%s"),
		Registry ? TEXT("valid") : TEXT("null"));
}

FName FInitialExperienceLoader::ResolveInitialExperienceName() const
{
	FString Value = TEXT("MainMenuWorld");
	GConfig->GetString(TEXT("/Script/Alis.AlisGI"), TEXT("EntryPointExperience"), Value, GGameIni);

	LastResolvedExperience = FName(*Value);

	UE_LOG(LogProjectLoading, Display, TEXT("InitialExperienceLoader: Resolved entry experience: '%s'"),
		*LastResolvedExperience.ToString());

	return LastResolvedExperience;
}

bool FInitialExperienceLoader::BuildLoadRequest(FName ExperienceName, FLoadRequest& OutRequest, FText& OutError)
{
	UE_LOG(LogProjectLoading, Display, TEXT("InitialExperienceLoader: Building load request for experience '%s'"),
		*ExperienceName.ToString());

	// Validate registry
	if (!Registry)
	{
		LastError = TEXT("Experience registry unavailable");
		OutError = FText::FromString(LastError);
		UE_LOG(LogProjectLoading, Error, TEXT("InitialExperienceLoader: %s"), *LastError);
		return false;
	}

	// Find descriptor
	UProjectExperienceDescriptorBase* Descriptor = Registry->FindDescriptor(ExperienceName);
	if (!Descriptor)
	{
		LastError = FString::Printf(TEXT("Descriptor not found for experience '%s'"), *ExperienceName.ToString());
		OutError = FText::FromString(LastError);
		UE_LOG(LogProjectLoading, Error, TEXT("InitialExperienceLoader: %s"), *LastError);

		// Log hint for debugging
		UE_LOG(LogProjectLoading, Warning, TEXT("InitialExperienceLoader: Check registry for available experiences"));

		return false;
	}

	// Ensure asset scans are registered
	EnsureAssetScans(*Descriptor);

	// Build request from descriptor
	OutRequest = FLoadRequest();
	Descriptor->BuildLoadRequest(OutRequest);

	// Resolve map asset ID from soft path if needed
	UAssetManager& AssetManager = UAssetManager::Get();
	if (!OutRequest.MapSoftPath.IsNull() && !OutRequest.MapAssetId.IsValid())
	{
		const FPrimaryAssetId MapId = AssetManager.GetPrimaryAssetIdForPath(OutRequest.MapSoftPath);
		if (MapId.IsValid())
		{
			OutRequest.MapAssetId = MapId;
			UE_LOG(LogProjectLoading, Display, TEXT("InitialExperienceLoader: Resolved MapAssetId=%s from SoftPath"),
				*MapId.ToString());
		}
	}

	// Build asset ID arrays
	OutRequest.CriticalAssetIds.Reset();
	OutRequest.WarmupAssetIds.Reset();
	OutRequest.BackgroundAssetIds.Reset();

	AddAssetIds(Descriptor->LoadAssets.CriticalAssets, OutRequest.CriticalAssetIds, AssetManager);
	AddAssetIds(Descriptor->LoadAssets.WarmupAssets, OutRequest.WarmupAssetIds, AssetManager);
	AddAssetIds(Descriptor->LoadAssets.BackgroundAssets, OutRequest.BackgroundAssetIds, AssetManager);

	// Auto-discover map dependencies if no explicit CriticalAssets defined
	OutRequest.CriticalSoftPaths.Reset();
	if (OutRequest.CriticalAssetIds.Num() == 0 && !OutRequest.MapSoftPath.IsNull())
	{
		UE_LOG(LogProjectLoading, Display, TEXT("InitialExperienceLoader: No explicit CriticalAssets - auto-discovering from map"));

		DiscoverMapDependencies(OutRequest.MapSoftPath, OutRequest.CriticalSoftPaths, 2); // Depth 2 for performance

		UE_LOG(LogProjectLoading, Display, TEXT("InitialExperienceLoader: Auto-discovered %d assets via CriticalSoftPaths"),
			OutRequest.CriticalSoftPaths.Num());
	}

	// Show loading screen for all loads (viewport widget implementation)
	OutRequest.bShowLoadingScreen = true;

	// Validate request
	if (!OutRequest.IsValid())
	{
		LastError = FString::Printf(TEXT("Descriptor '%s' produced invalid request (no map)"), *ExperienceName.ToString());
		OutError = FText::FromString(LastError);
		UE_LOG(LogProjectLoading, Error, TEXT("InitialExperienceLoader: %s"), *LastError);
		return false;
	}

	// Log request details for observability
	LogLoadRequestDetails(OutRequest);

	LastError.Empty();
	return true;
}

bool FInitialExperienceLoader::IsAlreadyOnTargetMap(const FLoadRequest& Request, UGameInstance* GameInstance) const
{
	if (!GameInstance)
	{
		return false;
	}

	UWorld* CurrentWorld = GameInstance->GetWorld();
	if (!CurrentWorld)
	{
		return false;
	}

	// Get current map name
	FString CurrentMapName = CurrentWorld->GetMapName();
	CurrentMapName.RemoveFromStart(CurrentWorld->StreamingLevelsPrefix);

	// Get target map name
	FString TargetMapName;
	if (Request.MapAssetId.IsValid())
	{
		TargetMapName = Request.MapAssetId.PrimaryAssetName.ToString();
	}
	else if (!Request.MapSoftPath.IsNull())
	{
		TargetMapName = FPackageName::GetShortName(Request.MapSoftPath.GetAssetName());
	}

	if (TargetMapName.IsEmpty())
	{
		return false;
	}

	const bool bAlreadyOnMap = CurrentMapName.Contains(TargetMapName);

	if (bAlreadyOnMap)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("InitialExperienceLoader: Already on target map '%s' (current='%s', target='%s')"),
			*CurrentMapName, *CurrentMapName, *TargetMapName);
	}
	else
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("InitialExperienceLoader: Not on target map (current='%s', target='%s')"),
			*CurrentMapName, *TargetMapName);
	}

	return bAlreadyOnMap;
}

void FInitialExperienceLoader::EnsureAssetScans(const UProjectExperienceDescriptorBase& Descriptor)
{
	UE_LOG(LogProjectLoading, Display, TEXT("EnsureAssetScans: Called for '%s'"), *Descriptor.ExperienceName.ToString());
	UE_LOG(LogProjectLoading, Display, TEXT("  Context: GIsEditor=%s, IsRunningGame()=%s, IsRunningCommandlet()=%s"),
		GIsEditor ? TEXT("true") : TEXT("false"),
		IsRunningGame() ? TEXT("true") : TEXT("false"),
		IsRunningCommandlet() ? TEXT("true") : TEXT("false"));

	// In editor, asset types are already registered via DefaultGame.ini/plugin configs.
	// Runtime scanning is only needed for cooked builds where plugin configs may not merge.
	// Scanning in editor can cause parameter mismatch crashes if specs differ from config.
	if (GIsEditor)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("EnsureAssetScans: Skipping in editor (types registered via config)"));
		return;
	}

	// Runtime scan for cooked builds - plugin configs may not merge reliably
	// See: docs/asset_manager_registration.md for architecture details
	TArray<FExperienceAssetScanSpec> Specs;
	Descriptor.GetAssetScanSpecs(Specs);

	UE_LOG(LogProjectLoading, Display, TEXT("EnsureAssetScans: GetAssetScanSpecs returned %d specs"), Specs.Num());

	if (Specs.Num() == 0)
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("EnsureAssetScans: NO scan specs for '%s'"),
			*Descriptor.ExperienceName.ToString());
		UE_LOG(LogProjectLoading, Warning, TEXT("  Override GetAssetScanSpecs() in descriptor to register paths at runtime"));
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	UE_LOG(LogProjectLoading, Display, TEXT("EnsureAssetScans: AssetManager.IsInitialized()=%s"),
		AssetManager.IsInitialized() ? TEXT("true") : TEXT("false"));

	for (const FExperienceAssetScanSpec& Spec : Specs)
	{
		if (Spec.PrimaryAssetType.IsNone() || Spec.Directories.Num() == 0)
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("EnsureAssetScans: SKIPPED invalid spec (Type=%s, Dirs=%d)"),
				*Spec.PrimaryAssetType.ToString(), Spec.Directories.Num());
			continue;
		}

		UE_LOG(LogProjectLoading, Display, TEXT("EnsureAssetScans: Scanning Type='%s', BaseClass='%s'"),
			*Spec.PrimaryAssetType.ToString(),
			Spec.BaseClass ? *Spec.BaseClass->GetName() : TEXT("UObject"));

		for (const FString& Dir : Spec.Directories)
		{
			UE_LOG(LogProjectLoading, Display, TEXT("  Directory: %s"), *Dir);
		}

		const int32 NumFound = AssetManager.ScanPathsForPrimaryAssets(
			FPrimaryAssetType(Spec.PrimaryAssetType),
			Spec.Directories,
			Spec.BaseClass ? Spec.BaseClass.Get() : UObject::StaticClass(),
			Spec.bHasBlueprintClasses,
			Spec.bIsEditorOnly,
			Spec.bForceSynchronousScan);

		UE_LOG(LogProjectLoading, Display, TEXT("EnsureAssetScans: ScanPathsForPrimaryAssets found %d assets of type '%s'"),
			NumFound, *Spec.PrimaryAssetType.ToString());

		// Diagnostic: list discovered assets
		if (NumFound > 0)
		{
			TArray<FPrimaryAssetId> FoundIds;
			AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(Spec.PrimaryAssetType), FoundIds);
			const int32 MaxToLog = FMath::Min(FoundIds.Num(), 10);
			for (int32 i = 0; i < MaxToLog; ++i)
			{
				UE_LOG(LogProjectLoading, Display, TEXT("    [%d] %s"), i, *FoundIds[i].ToString());
			}
			if (FoundIds.Num() > MaxToLog)
			{
				UE_LOG(LogProjectLoading, Display, TEXT("    ... and %d more"), FoundIds.Num() - MaxToLog);
			}
		}
	}
}

FString FInitialExperienceLoader::GetStatusString() const
{
	TStringBuilder<512> Builder;
	Builder.Appendf(TEXT("InitialExperienceLoader Status:\n"));
	Builder.Appendf(TEXT("  Registry: %s\n"), Registry ? TEXT("valid") : TEXT("null"));
	Builder.Appendf(TEXT("  Last Resolved: %s\n"), *LastResolvedExperience.ToString());

	if (!LastError.IsEmpty())
	{
		Builder.Appendf(TEXT("  Last Error: %s\n"), *LastError);
	}

	return Builder.ToString();
}

void FInitialExperienceLoader::LogLoadRequestDetails(const FLoadRequest& Request) const
{
	UE_LOG(LogProjectLoading, Display, TEXT("InitialExperienceLoader: Load request details:"));

	if (Request.MapAssetId.IsValid())
	{
		UE_LOG(LogProjectLoading, Display, TEXT("  MapAssetId: %s"), *Request.MapAssetId.ToString());
	}
	else
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("  MapAssetId: INVALID (using soft path fallback)"));
	}

	if (!Request.MapSoftPath.IsNull())
	{
		UE_LOG(LogProjectLoading, Display, TEXT("  MapSoftPath: %s"), *Request.MapSoftPath.ToString());
	}

	UE_LOG(LogProjectLoading, Display, TEXT("  ExperienceName: %s"), *Request.ExperienceName.ToString());
	UE_LOG(LogProjectLoading, Display, TEXT("  LoadMode: %d"), static_cast<int32>(Request.LoadMode));
	UE_LOG(LogProjectLoading, Display, TEXT("  CriticalAssets: %d"), Request.CriticalAssetIds.Num());
	UE_LOG(LogProjectLoading, Display, TEXT("  WarmupAssets: %d"), Request.WarmupAssetIds.Num());
	UE_LOG(LogProjectLoading, Display, TEXT("  BackgroundAssets: %d"), Request.BackgroundAssetIds.Num());
	UE_LOG(LogProjectLoading, Display, TEXT("  Features: %d"), Request.FeaturesToActivate.Num());
	UE_LOG(LogProjectLoading, Display, TEXT("  ShowLoadingScreen: %s"), Request.bShowLoadingScreen ? TEXT("true") : TEXT("false"));

	// Log asset IDs for debugging
	if (Request.CriticalAssetIds.Num() > 0)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("  Critical Assets:"));
		for (const FPrimaryAssetId& Id : Request.CriticalAssetIds)
		{
			UE_LOG(LogProjectLoading, Verbose, TEXT("    - %s"), *Id.ToString());
		}
	}
}

void FInitialExperienceLoader::DiscoverMapDependencies(const FSoftObjectPath& MapSoftPath, TArray<FSoftObjectPath>& OutDependencies, int32 MaxDepth) const
{
	if (MapSoftPath.IsNull() || MaxDepth <= 0)
	{
		return;
	}

	UE_LOG(LogProjectLoading, Display, TEXT("DiscoverMapDependencies: Scanning '%s' (depth=%d)"), *MapSoftPath.ToString(), MaxDepth);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// Get package name from soft path
	const FName PackageName = FName(*MapSoftPath.GetLongPackageName());

	// Get dependencies recursively
	TArray<FAssetIdentifier> Dependencies;
	AssetRegistry.GetDependencies(FAssetIdentifier(PackageName), Dependencies, UE::AssetRegistry::EDependencyCategory::Package);

	UE_LOG(LogProjectLoading, Display, TEXT("DiscoverMapDependencies: Found %d direct dependencies"), Dependencies.Num());

	// Filter and collect heavy asset types
	// CRITICAL: Track visited PACKAGES (not assets) to avoid duplicate GetDependencies calls
	TSet<FName> VisitedPackages;
	TSet<FSoftObjectPath> VisitedAssets;
	TArray<FAssetIdentifier> ToProcess = Dependencies;

	while (ToProcess.Num() > 0 && MaxDepth > 0)
	{
		TArray<FAssetIdentifier> NextLevel;

		for (const FAssetIdentifier& DepId : ToProcess)
		{
			const FName DepPackageName = DepId.PackageName;
			if (DepPackageName.IsNone())
			{
				continue;
			}

			// Skip already-visited packages (prevents N*M duplicate processing)
			if (VisitedPackages.Contains(DepPackageName))
			{
				continue;
			}
			VisitedPackages.Add(DepPackageName);

			const FString DepPath = DepPackageName.ToString();

			// Skip engine/plugin content (only game content)
			if (DepPath.StartsWith(TEXT("/Engine/")) || DepPath.StartsWith(TEXT("/Script/")))
			{
				continue;
			}

			// Get asset data to check type
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByPackageName(DepPackageName, Assets, true);

			for (const FAssetData& Asset : Assets)
			{
				const FSoftObjectPath AssetPath = Asset.GetSoftObjectPath();
				if (VisitedAssets.Contains(AssetPath))
				{
					continue;
				}
				VisitedAssets.Add(AssetPath);

				// Check if this is a heavy asset type we want to preload
				const FTopLevelAssetPath AssetClass = Asset.AssetClassPath;
				const bool bIsStaticMesh = AssetClass == UStaticMesh::StaticClass()->GetClassPathName();
				const bool bIsSkeletalMesh = AssetClass == USkeletalMesh::StaticClass()->GetClassPathName();
				const bool bIsTexture = AssetClass.GetAssetName().ToString().StartsWith(TEXT("Texture"));
				const bool bIsMaterial = AssetClass.GetAssetName().ToString().Contains(TEXT("Material"));

				if (bIsStaticMesh || bIsSkeletalMesh || bIsTexture || bIsMaterial)
				{
					OutDependencies.Add(AssetPath);
				}
			}

			// Recurse ONCE per package (outside asset loop to avoid N duplicates)
			TArray<FAssetIdentifier> SubDeps;
			AssetRegistry.GetDependencies(FAssetIdentifier(DepPackageName), SubDeps, UE::AssetRegistry::EDependencyCategory::Package);
			NextLevel.Append(SubDeps);
		}

		ToProcess = NextLevel;
		MaxDepth--;
	}

	UE_LOG(LogProjectLoading, Display, TEXT("DiscoverMapDependencies: Discovered %d heavy assets (before filtering)"), OutDependencies.Num());

	// WORLD PARTITION SPECIAL HANDLING:
	// For WP maps, don't preload streaming cell assets - WP manages its own streaming.
	// Only preload SHARED assets (materials/textures used by multiple cells) - these benefit from preload.
	// Strategy: Count references to each asset; only preload if referenced multiple times.

	// Count how many times each asset is referenced
	TMap<FSoftObjectPath, int32> AssetRefCounts;
	TMap<FSoftObjectPath, FTopLevelAssetPath> AssetClasses;

	for (const FSoftObjectPath& AssetPath : OutDependencies)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(FName(*AssetPath.GetLongPackageName()), Assets, true);

		for (const FAssetData& Asset : Assets)
		{
			const FSoftObjectPath Path = Asset.GetSoftObjectPath();
			AssetRefCounts.FindOrAdd(Path, 0)++;
			AssetClasses.Add(Path, Asset.AssetClassPath);
		}
	}

	// Type-based priority filtering (NOT RefCount-based - WP breaks RefCount heuristic)
	// Materials first (biggest win - shader compilation), then meshes, then textures
	TArray<FSoftObjectPath> Materials, Meshes, Textures;

	for (const auto& Pair : AssetRefCounts)
	{
		const FSoftObjectPath& AssetPath = Pair.Key;
		const FTopLevelAssetPath* AssetClassPtr = AssetClasses.Find(AssetPath);

		if (!AssetClassPtr)
		{
			continue;
		}

		const FString ClassName = AssetClassPtr->GetAssetName().ToString();

		if (ClassName.Contains(TEXT("Material")))
		{
			Materials.Add(AssetPath);
		}
		else if (ClassName == TEXT("StaticMesh") || ClassName == TEXT("SkeletalMesh"))
		{
			Meshes.Add(AssetPath);
		}
		else if (ClassName.StartsWith(TEXT("Texture")))
		{
			Textures.Add(AssetPath);
		}
	}

	// Cap at 20 total - prevents "big world = huge memory overhead"
	const int32 Cap = 20;
	OutDependencies.Reset();

	// Materials first (shader work is the biggest win)
	for (int32 i = 0; i < FMath::Min(Cap, Materials.Num()); ++i)
	{
		OutDependencies.Add(Materials[i]);
	}

	// Fill remaining slots with meshes
	int32 Remaining = Cap - OutDependencies.Num();
	for (int32 i = 0; i < FMath::Min(Remaining, Meshes.Num()); ++i)
	{
		OutDependencies.Add(Meshes[i]);
	}

	// Fill remaining slots with textures
	Remaining = Cap - OutDependencies.Num();
	for (int32 i = 0; i < FMath::Min(Remaining, Textures.Num()); ++i)
	{
		OutDependencies.Add(Textures[i]);
	}

	UE_LOG(LogProjectLoading, Display, TEXT("DiscoverMapDependencies: Type-based filtering - Materials=%d Meshes=%d Textures=%d -> Preloading %d (cap=%d)"),
		Materials.Num(), Meshes.Num(), Textures.Num(), OutDependencies.Num(), Cap);
}
