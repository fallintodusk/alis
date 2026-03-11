// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogProjectPlacementEditor, Log, All);

class SProjectAssetPicker;

/**
 * Editor module for the Project Asset Picker panel.
 *
 * Provides a dockable panel (Window > Project Placement) for browsing
 * and spawning DataAssets with:
 * - Large thumbnails in tile view
 * - Folder hierarchy for categories
 * - Double-click to spawn at viewport center
 * - Drag-drop to spawn at cursor
 *
 * Auto-refreshes when assets are added/removed via Asset Registry listeners.
 */
class FProjectPlacementEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the asset picker widget (for external refresh calls). */
	TSharedPtr<SProjectAssetPicker> GetAssetPicker() const { return AssetPickerWidget; }

private:
	void RegisterMenus();
	void UnregisterMenus();

	TSharedRef<SDockTab> SpawnAssetPickerTab(const FSpawnTabArgs& Args);

	// Asset registry listeners for auto-refresh
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);

	/** Tab identifier. */
	static const FName AssetPickerTabId;

	/** Cached picker widget. */
	TSharedPtr<SProjectAssetPicker> AssetPickerWidget;
};
