// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/ProjectLoadRequest.h"
#include "ProjectMapListViewModel.generated.h"

/**
 * Map entry data for display in map browser
 */
USTRUCT(BlueprintType)
struct PROJECTMENUMAIN_API FProjectMapEntry
{
	GENERATED_BODY()

	/** Display name for the map */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FString DisplayName;

	/** Description of the map */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FString Description;

	/** Soft path to the map asset */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FSoftObjectPath MapSoftPath;

	/** Primary Asset ID (if registered with AssetManager) */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FPrimaryAssetId MapAssetId;

	/** Default game mode class path (optional - can be overridden by GameMode URL param) */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FString DefaultGameModeClass;

	/** Default single-play mode name (e.g., "Beginner", "Medium", "Hardcore") */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FString DefaultModeName;

	/** Whether this map supports single-player */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bSupportsSinglePlayer = true;

	/** Whether this map supports multiplayer */
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bSupportsMultiplayer = false;

	FProjectMapEntry() = default;
};

/**
 * ViewModel for map list browser
 * Manages available maps data for W_MapBrowser widget
 *
 * Provides:
 * - List of available maps with metadata
 * - Selection state
 * - Loading initiation via ILoadingService
 * - Comprehensive debug logging
 */
UCLASS(BlueprintType)
class PROJECTMENUMAIN_API UProjectMapListViewModel : public UObject
{
	GENERATED_BODY()

public:
	UProjectMapListViewModel();

	/**
	 * Refresh the list of available maps.
	 * Currently populates with hardcoded test data.
	 * Future: Query ProjectWorld manifests or AssetManager.
	 */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	void RefreshMapList();

	/** Get total number of maps in the list */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	int32 GetMapCount() const { return Maps.Num(); }

	/** Get map entry by index */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	const FProjectMapEntry& GetMapEntry(int32 Index) const;

	/** Get selected map index (-1 if none selected) */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	int32 GetSelectedIndex() const { return SelectedMapIndex; }

	/** Select a map by index */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	void SelectMap(int32 Index);

	/** Check if a map is currently selected */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	bool HasSelection() const { return SelectedMapIndex >= 0 && SelectedMapIndex < Maps.Num(); }

	/** Get the currently selected map entry (returns empty if none selected) */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	const FProjectMapEntry& GetSelectedMap() const;

	/**
	 * Start loading the selected map via ILoadingService.
	 * Builds FLoadRequest with proper CustomOptions for GameMode binding.
	 *
	 * @param GameModeClass Optional override for GameMode (uses map default if empty)
	 * @param ModeName Optional override for Mode parameter (uses map default if empty)
	 * @return true if loading was initiated, false if failed (no selection, no service, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	bool StartLoadingSelectedMap(const FString& GameModeClass = TEXT(""), const FString& ModeName = TEXT(""));

	/**
	 * Set search filter for map list
	 */
	UFUNCTION(BlueprintCallable, Category = "MapList")
	void SetSearchFilter(const FString& Filter);

	// Delegate for when map list changes
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMapListChanged);
	UPROPERTY(BlueprintAssignable, Category = "MapList")
	FOnMapListChanged OnMapListChanged;

	// Delegate for when selection changes
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedMapChanged, int32, NewIndex);
	UPROPERTY(BlueprintAssignable, Category = "MapList")
	FOnSelectedMapChanged OnSelectedMapChanged;

	// Delegate for when loading starts
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadingStarted, const FString&, MapName);
	UPROPERTY(BlueprintAssignable, Category = "MapList")
	FOnLoadingStarted OnLoadingStarted;

protected:
	/** All available maps (unfiltered) */
	UPROPERTY(BlueprintReadOnly, Category = "MapList")
	TArray<FProjectMapEntry> Maps;

	/** Currently selected map index (-1 = none) */
	UPROPERTY(BlueprintReadOnly, Category = "MapList")
	int32 SelectedMapIndex = -1;

	/** Current search filter */
	UPROPERTY(BlueprintReadOnly, Category = "MapList")
	FString SearchFilter;

	/** Empty entry for invalid access */
	static FProjectMapEntry EmptyMapEntry;
};
