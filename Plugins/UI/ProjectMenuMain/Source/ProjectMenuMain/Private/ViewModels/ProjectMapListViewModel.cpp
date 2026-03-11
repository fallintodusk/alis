// Copyright ALIS. All Rights Reserved.

#include "ViewModels/ProjectMapListViewModel.h"
#include "ProjectServiceLocator.h"
#include "Services/ILoadingService.h"
#include "Interfaces/ProjectLoadingHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogMapListViewModel, Log, All);

// Static empty entry for invalid access
FProjectMapEntry UProjectMapListViewModel::EmptyMapEntry;

UProjectMapListViewModel::UProjectMapListViewModel()
{
	UE_LOG(LogMapListViewModel, Log, TEXT("[MapListViewModel] Constructed"));
}

void UProjectMapListViewModel::RefreshMapList()
{
	UE_LOG(LogMapListViewModel, Log, TEXT("=== RefreshMapList BEGIN ==="));

	Maps.Reset();
	SelectedMapIndex = -1;

	// Populate with test data for now
	// TODO: Query ProjectWorld manifests or AssetManager for real map data

	// Test Map 1: City17 with SinglePlayerGameMode
	// Note: These are placeholder paths. Replace with actual map paths when maps exist.
	{
		FProjectMapEntry Entry;
		Entry.DisplayName = TEXT("City17");
		Entry.Description = TEXT("Post-apocalyptic urban environment in Kazan");
		// Full asset path format: /Path/To/Asset.AssetName
		Entry.MapSoftPath = FSoftObjectPath(TEXT("/Game/Maps/City17/City17_Persistent.City17_Persistent"));
		Entry.DefaultGameModeClass = TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode");
		Entry.DefaultModeName = TEXT("Medium");
		Entry.bSupportsSinglePlayer = true;
		Entry.bSupportsMultiplayer = false;
		Maps.Add(Entry);

		UE_LOG(LogMapListViewModel, Log, TEXT("  Added map: %s (GameMode: %s, Mode: %s)"),
			*Entry.DisplayName, *Entry.DefaultGameModeClass, *Entry.DefaultModeName);
	}

	// Test Map 2: TestWorld (simpler test map)
	{
		FProjectMapEntry Entry;
		Entry.DisplayName = TEXT("TestWorld");
		Entry.Description = TEXT("Simple test environment for debugging");
		Entry.MapSoftPath = FSoftObjectPath(TEXT("/Game/Maps/TestWorld/TestWorld_Persistent.TestWorld_Persistent"));
		Entry.DefaultGameModeClass = TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode");
		Entry.DefaultModeName = TEXT("Medium");
		Entry.bSupportsSinglePlayer = true;
		Entry.bSupportsMultiplayer = false;
		Maps.Add(Entry);

		UE_LOG(LogMapListViewModel, Log, TEXT("  Added map: %s (GameMode: %s, Mode: %s)"),
			*Entry.DisplayName, *Entry.DefaultGameModeClass, *Entry.DefaultModeName);
	}

	// Test Map 3: Hardcore mode map
	{
		FProjectMapEntry Entry;
		Entry.DisplayName = TEXT("HardcoreArena");
		Entry.Description = TEXT("Survival arena with permadeath enabled");
		Entry.MapSoftPath = FSoftObjectPath(TEXT("/Game/Maps/HardcoreArena/HardcoreArena_Persistent.HardcoreArena_Persistent"));
		Entry.DefaultGameModeClass = TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode");
		Entry.DefaultModeName = TEXT("Hardcore");
		Entry.bSupportsSinglePlayer = true;
		Entry.bSupportsMultiplayer = false;
		Maps.Add(Entry);

		UE_LOG(LogMapListViewModel, Log, TEXT("  Added map: %s (GameMode: %s, Mode: %s)"),
			*Entry.DisplayName, *Entry.DefaultGameModeClass, *Entry.DefaultModeName);
	}

	UE_LOG(LogMapListViewModel, Log, TEXT("=== RefreshMapList END: %d maps loaded ==="), Maps.Num());

	// Notify listeners
	OnMapListChanged.Broadcast();

	// Auto-select first map if available
	if (Maps.Num() > 0)
	{
		SelectMap(0);
	}
}

const FProjectMapEntry& UProjectMapListViewModel::GetMapEntry(int32 Index) const
{
	if (Index >= 0 && Index < Maps.Num())
	{
		return Maps[Index];
	}
	UE_LOG(LogMapListViewModel, Warning, TEXT("GetMapEntry: Invalid index %d (MapCount=%d)"), Index, Maps.Num());
	return EmptyMapEntry;
}

void UProjectMapListViewModel::SelectMap(int32 Index)
{
	if (Index < -1 || Index >= Maps.Num())
	{
		UE_LOG(LogMapListViewModel, Warning, TEXT("SelectMap: Invalid index %d (MapCount=%d), ignoring"), Index, Maps.Num());
		return;
	}

	const int32 OldIndex = SelectedMapIndex;
	SelectedMapIndex = Index;

	if (OldIndex != SelectedMapIndex)
	{
		if (SelectedMapIndex >= 0)
		{
			const FProjectMapEntry& Selected = Maps[SelectedMapIndex];
			UE_LOG(LogMapListViewModel, Log, TEXT("SelectMap: Selected map '%s' (index=%d)"),
				*Selected.DisplayName, SelectedMapIndex);
		}
		else
		{
			UE_LOG(LogMapListViewModel, Log, TEXT("SelectMap: Selection cleared"));
		}

		OnSelectedMapChanged.Broadcast(SelectedMapIndex);
	}
}

const FProjectMapEntry& UProjectMapListViewModel::GetSelectedMap() const
{
	if (HasSelection())
	{
		return Maps[SelectedMapIndex];
	}
	return EmptyMapEntry;
}

bool UProjectMapListViewModel::StartLoadingSelectedMap(const FString& GameModeClass, const FString& ModeName)
{
	UE_LOG(LogMapListViewModel, Log, TEXT(""));
	UE_LOG(LogMapListViewModel, Log, TEXT("========================================"));
	UE_LOG(LogMapListViewModel, Log, TEXT("=== StartLoadingSelectedMap BEGIN ==="));
	UE_LOG(LogMapListViewModel, Log, TEXT("========================================"));
	UE_LOG(LogMapListViewModel, Log, TEXT("  GameModeClass override: %s"), GameModeClass.IsEmpty() ? TEXT("(none - use map default)") : *GameModeClass);
	UE_LOG(LogMapListViewModel, Log, TEXT("  ModeName override: %s"), ModeName.IsEmpty() ? TEXT("(none - use map default)") : *ModeName);

	// Validate selection
	if (!HasSelection())
	{
		UE_LOG(LogMapListViewModel, Error, TEXT("  FAILED: No map selected (SelectedMapIndex=%d, MapCount=%d)"),
			SelectedMapIndex, Maps.Num());
		return false;
	}

	const FProjectMapEntry& SelectedMap = Maps[SelectedMapIndex];
	UE_LOG(LogMapListViewModel, Log, TEXT("  Selected map: '%s'"), *SelectedMap.DisplayName);
	UE_LOG(LogMapListViewModel, Log, TEXT("  MapSoftPath: %s"), *SelectedMap.MapSoftPath.ToString());
	UE_LOG(LogMapListViewModel, Log, TEXT("  MapAssetId: %s"), SelectedMap.MapAssetId.IsValid() ? *SelectedMap.MapAssetId.ToString() : TEXT("(not set)"));

	// Resolve ILoadingService
	UE_LOG(LogMapListViewModel, Log, TEXT("  Resolving ILoadingService from ServiceLocator..."));
	TSharedPtr<ILoadingService> LoadingService = FProjectServiceLocator::Resolve<ILoadingService>();

	if (!LoadingService.IsValid())
	{
		UE_LOG(LogMapListViewModel, Error, TEXT("  FAILED: ILoadingService not registered in ServiceLocator!"));
		UE_LOG(LogMapListViewModel, Error, TEXT("  Check that UProjectLoadingSubsystem is initialized and registered the service."));
		return false;
	}
	UE_LOG(LogMapListViewModel, Log, TEXT("  ILoadingService resolved successfully"));

	// Check if loading is already in progress
	if (LoadingService->IsLoadInProgress())
	{
		UE_LOG(LogMapListViewModel, Warning, TEXT("  WARNING: Load already in progress, cancelling previous load"));
		LoadingService->CancelActiveLoad(false);
	}

	// Build FLoadRequest
	UE_LOG(LogMapListViewModel, Log, TEXT("  Building FLoadRequest..."));
	FLoadRequest Request;

	// Set map path (prefer AssetId if available, fallback to SoftPath)
	if (SelectedMap.MapAssetId.IsValid())
	{
		Request.MapAssetId = SelectedMap.MapAssetId;
		UE_LOG(LogMapListViewModel, Log, TEXT("    MapAssetId: %s"), *Request.MapAssetId.ToString());
	}
	else
	{
		Request.MapSoftPath = SelectedMap.MapSoftPath;
		UE_LOG(LogMapListViewModel, Log, TEXT("    MapSoftPath: %s"), *Request.MapSoftPath.ToString());
	}

	// Set load mode
	Request.LoadMode = ELoadMode::SinglePlayer;
	UE_LOG(LogMapListViewModel, Log, TEXT("    LoadMode: SinglePlayer"));

	// Determine GameMode class (override > map default)
	const FString FinalGameModeClass = GameModeClass.IsEmpty() ? SelectedMap.DefaultGameModeClass : GameModeClass;
	if (!FinalGameModeClass.IsEmpty())
	{
		Request.CustomOptions.Add(TEXT("game"), FinalGameModeClass);
		UE_LOG(LogMapListViewModel, Log, TEXT("    CustomOptions[game]: %s"), *FinalGameModeClass);
	}

	// Determine Mode name (override > map default)
	const FString FinalModeName = ModeName.IsEmpty() ? SelectedMap.DefaultModeName : ModeName;
	if (!FinalModeName.IsEmpty())
	{
		Request.CustomOptions.Add(TEXT("Mode"), FinalModeName);
		UE_LOG(LogMapListViewModel, Log, TEXT("    CustomOptions[Mode]: %s"), *FinalModeName);
	}

	// Log the full CustomOptions map
	UE_LOG(LogMapListViewModel, Log, TEXT("    Full CustomOptions (%d entries):"), Request.CustomOptions.Num());
	for (const auto& Pair : Request.CustomOptions)
	{
		UE_LOG(LogMapListViewModel, Log, TEXT("      [%s] = %s"), *Pair.Key, *Pair.Value);
	}

	// Validate request
	TArray<FText> ValidationErrors;
	if (!Request.Validate(ValidationErrors))
	{
		UE_LOG(LogMapListViewModel, Error, TEXT("  FAILED: FLoadRequest validation failed:"));
		for (const FText& Error : ValidationErrors)
		{
			UE_LOG(LogMapListViewModel, Error, TEXT("    - %s"), *Error.ToString());
		}
		return false;
	}
	UE_LOG(LogMapListViewModel, Log, TEXT("  FLoadRequest validation passed"));

	// Start loading
	UE_LOG(LogMapListViewModel, Log, TEXT("  Calling ILoadingService->StartLoad()..."));
	TSharedPtr<ILoadingHandle> Handle = LoadingService->StartLoad(Request);

	if (!Handle.IsValid())
	{
		UE_LOG(LogMapListViewModel, Error, TEXT("  FAILED: StartLoad returned null handle"));
		return false;
	}

	UE_LOG(LogMapListViewModel, Log, TEXT("  SUCCESS: Loading initiated"));
	UE_LOG(LogMapListViewModel, Log, TEXT("========================================"));
	UE_LOG(LogMapListViewModel, Log, TEXT("=== StartLoadingSelectedMap END ==="));
	UE_LOG(LogMapListViewModel, Log, TEXT("========================================"));
	UE_LOG(LogMapListViewModel, Log, TEXT(""));

	// Notify listeners
	OnLoadingStarted.Broadcast(SelectedMap.DisplayName);

	return true;
}

void UProjectMapListViewModel::SetSearchFilter(const FString& Filter)
{
	if (SearchFilter != Filter)
	{
		SearchFilter = Filter;
		UE_LOG(LogMapListViewModel, Log, TEXT("SetSearchFilter: '%s'"), *SearchFilter);

		// TODO: Apply filter to Maps array (create filtered view)
		// For now, filter is stored but not applied

		OnMapListChanged.Broadcast();
	}
}
