// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UToolMenu;
struct FAssetData;

/**
 * Menu extension for "Replace Selected Actors with Definition".
 * Adds submenu to actor context menu showing all ObjectDefinitions organized by capability.
 */
class FReplaceWithDefinitionMenuExtension
{
public:
	/** Register the menu extension with UToolMenus. */
	static void Register();

	/** Unregister the menu extension. */
	static void Unregister();

private:
	/** Extend the actor context menu with our submenu. */
	static void ExtendActorContextMenu(UToolMenu* Menu);

	/** Build capability submenu (Pickup, Hinged, etc.). */
	static void BuildCapabilitySubmenu(UToolMenu* Menu, FName CapabilityName);

	/** Execute replacement of selected actors with the given definition. */
	static void ExecuteReplace(const FAssetData& DefinitionAsset);

	/** Get all unique capabilities from ObjectDefinitions. */
	static TArray<FName> GetAvailableCapabilities();

	/** Get definitions filtered by capability. */
	static TArray<FAssetData> GetDefinitionsByCapability(FName CapabilityName);

	/** Check if any actors are selected in the level. */
	static bool HasSelectedActors();
};
