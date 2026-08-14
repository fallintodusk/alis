// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

class ARecastNavMesh;
class UNavigationSystemV1;
class UWorld;

namespace ProjectWorldRuntimeNavigation
{
	bool EnsureInternalData(
		UWorld* World,
		UNavigationSystemV1* Navigation,
		ARecastNavMesh*& OutRecast,
		FString& OutError);
}
