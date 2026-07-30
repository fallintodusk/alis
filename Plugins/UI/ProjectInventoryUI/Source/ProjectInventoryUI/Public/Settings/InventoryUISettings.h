// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Single source of truth for inventory UI layout constants shared across
 * every inventory-facing widget (main panel, nearby container panel, any
 * future side panel). Lives in Data/InventoryUISettings.json.
 *
 * Consumers read via FInventoryUISettings::Get(); loaded lazily once per
 * process. If the JSON is missing or malformed the struct's in-code
 * defaults are used and a warning is logged at first access.
 */
struct PROJECTINVENTORYUI_API FInventoryUISettings
{
	/** Logical cell size in pixels. Every grid renders cells at this size. */
	float CellSize = 64.f;

	/** UniformGridPanel slot padding (one side) that renders the grid line between cells. */
	float GridSlotLineWidth = 1.f;

	/** Cell content padding inside UProjectGridCell (one side). */
	float CellInnerPadding = 4.f;

	/** Border padding around a grid host (one side). */
	float HostOuterPadding = 4.f;

	/**
	 * Lazy-loaded shared instance.
	 *
	 * First call performs file I/O through the layout loader - call it
	 * on the game thread during widget construction. The underlying
	 * storage is a `static const` initialised under C++11 thread-safe
	 * statics, so subsequent reads from any thread after the first call
	 * has returned are safe (the data is const).
	 */
	static const FInventoryUISettings& Get();
};
