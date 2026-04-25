// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Shared pure grid geometry for inventory-like rectangular cell layouts.
 *
 * This lives in ProjectSharedTypes because it is a small cross-plugin primitive
 * used by inventory runtime and inventory UI. It has no asset, world, gameplay,
 * or UI dependencies.
 */
struct PROJECTSHAREDTYPES_API FInventoryGridGeometry
{
	static bool DoRectsOverlap(
		FIntPoint PosA,
		FIntPoint SizeA,
		FIntPoint PosB,
		FIntPoint SizeB);
};
