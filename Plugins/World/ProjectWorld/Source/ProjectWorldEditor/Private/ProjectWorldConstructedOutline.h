// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "Curve/GeneralPolygon2.h"

namespace ProjectWorldConstructedOutline
{
	// Exact-vertex outline of bounded nonoverlapping convex pieces; no coordinate repair.
	bool Extract(const TArray<UE::Geometry::FGeneralPolygon2d>& Pieces,
		TArray<UE::Geometry::FGeneralPolygon2d>& Out,FString& Error);
}
