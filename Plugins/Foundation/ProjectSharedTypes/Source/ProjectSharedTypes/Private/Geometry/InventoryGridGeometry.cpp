// Copyright ALIS. All Rights Reserved.

#include "Geometry/InventoryGridGeometry.h"

bool FInventoryGridGeometry::DoRectsOverlap(
	FIntPoint PosA,
	FIntPoint SizeA,
	FIntPoint PosB,
	FIntPoint SizeB)
{
	return PosA.X < (PosB.X + SizeB.X)
		&& (PosA.X + SizeA.X) > PosB.X
		&& PosA.Y < (PosB.Y + SizeB.Y)
		&& (PosA.Y + SizeA.Y) > PosB.Y;
}
