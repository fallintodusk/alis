// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IContentBrowserSingleton.h"
#include "UObject/StrongObjectPtr.h"
#include "ProjectObjectActorFactory.h"

/**
 * Object browser widget with folder tree and asset tiles.
 *
 * Layout: PathPicker (left) | AssetPicker (right)
 * - PathPicker: Folder tree starting at /ProjectObject
 * - AssetPicker: Tile view of UObjectDefinition assets
 *
 * Interaction:
 * - Double-click: Opens asset editor
 * - Drag-drop to viewport: Spawns AInteractableActor via UProjectObjectActorFactory
 *
 * Single Responsibility: Object browsing via DataAssets.
 */
class SProjectObjectBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProjectObjectBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refresh asset view (call after generator runs). */
	void RefreshAssets();

private:
	// Path navigation
	void OnPathSelected(const FString& Path);

	// Asset interaction
	void OnAssetDoubleClicked(const FAssetData& AssetData);
	void SpawnFromAsset(const FAssetData& AssetData);
	TSharedPtr<SWidget> GetAssetContextMenu(const TArray<FAssetData>& SelectedAssets);

	/** Filter callback - return true to HIDE the asset. */
	bool ShouldFilterAsset(const FAssetData& AssetData) const;

	// Capability filtering
	void RefreshCapabilityFilters();
	TSharedRef<SWidget> BuildCapabilityFilterBar();
	TSharedRef<SWidget> BuildPickupRoleFilterBar();
	FReply OnCapabilityFilterChanged(FName NewFilter);
	FReply OnPickupRoleFilterChanged(FName NewRole);

	// AssetRegistry callback (fires when scan completes, during broadcast)
	void OnAssetRegistryScanComplete();

	// Periodic check for tags (runs every 1 second for up to 30 seconds)
	void CheckForTagsPeriodically();

	// Run a few delayed refresh passes when panel appears to warm thumbnails
	// without requiring manual hover over every tile.
	void StartThumbnailWarmupPasses();
	void RunThumbnailWarmupPass();

	FTransform GetSpawnTransform() const;

	/** Root path for objects. */
	static constexpr const TCHAR* ObjectsRootPath = TEXT("/ProjectObject");

	/** Current selected path. */
	FString CurrentPath;

	/** Factory for spawning object actors (prevents GC). */
	TStrongObjectPtr<UProjectObjectActorFactory> SpawnFactory;

	/** Capability filter state (NAME_None = All). */
	FName CurrentCapabilityFilter = NAME_None;
	FName CurrentPickupRoleFilter = NAME_None;

	/** Cached tag name for Item.Type filter (avoids string build per asset). */
	FName CachedItemTypeTagName = NAME_None;

	/** Cached filter lists (refreshed on construct). */
	TArray<FName> AvailableCapabilities;
	TArray<FName> AvailablePickupRoles;

	/** Filter bar containers (rebuilt dynamically on refresh). */
	TSharedPtr<SHorizontalBox> CapabilityFilterBarContainer;
	TSharedPtr<SHorizontalBox> PickupRoleFilterBarContainer;

	/** Delegate to refresh asset view. */
	FRefreshAssetViewDelegate RefreshDelegate;

	/** Remaining attempts for tag readiness check (used by CheckForTagsNextTick). */
	int32 TagCheckAttemptsRemaining = 0;

	/** Remaining one-shot refresh passes to warm newly visible thumbnails. */
	int32 ThumbnailWarmupPassesRemaining = 0;

	/** Timer handle for delayed warmup passes (allows cancel/restart). */
	FTimerHandle ThumbnailWarmupTimerHandle;
};
