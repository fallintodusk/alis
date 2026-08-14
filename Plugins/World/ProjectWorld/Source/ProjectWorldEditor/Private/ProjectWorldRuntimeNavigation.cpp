// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimeNavigation.h"

#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"

namespace ProjectWorldRuntimeNavigation
{
	bool EnsureInternalData(
		UWorld* World,
		UNavigationSystemV1* Navigation,
		ARecastNavMesh*& OutRecast,
		FString& OutError)
	{
		OutRecast = Cast<ARecastNavMesh>(
			Navigation->GetDefaultNavDataInstance(FNavigationSystem::Create));
		if (OutRecast == nullptr)
		{
			OutError = TEXT("Accepted route requires stock Recast navigation data.");
			return false;
		}
		if (OutRecast->IsPackageExternal())
		{
			OutRecast->SetPackageExternal(false);
		}
		return true;
	}
}
