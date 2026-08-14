// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimeNavigation.h"

#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRuntimeNavigationDataOwnershipTest,
	"Project.World.Realization.Runtime.NavigationDataOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeNavigationDataOwnershipTest::RunTest(const FString& Parameters)
{
	FString Error;

	UWorld* MigratedWorld = GEditor->NewMap(false);
	UNavigationSystemV1* MigratedNavigation = Cast<UNavigationSystemV1>(
		MigratedWorld->GetNavigationSystem());
	TestNotNull(TEXT("Migration world has NavigationSystemV1."), MigratedNavigation);
	if (MigratedNavigation == nullptr)
	{
		return false;
	}
	ARecastNavMesh* OwnedRecast = nullptr;
	TestTrue(
		TEXT("The runtime owner resolves map-owned navigation data."),
		ProjectWorldRuntimeNavigation::EnsureInternalData(
			MigratedWorld, MigratedNavigation, OwnedRecast, Error));
	TestNotNull(TEXT("Owned navigation data exists."), OwnedRecast);
	if (OwnedRecast == nullptr)
	{
		return false;
	}
	TestFalse(TEXT("Navigation data stays inside the map package."), OwnedRecast->IsPackageExternal());

	ARecastNavMesh* ReusedRecast = nullptr;
	TestTrue(
		TEXT("An unchanged Apply reuses map-owned navigation data."),
		ProjectWorldRuntimeNavigation::EnsureInternalData(
			MigratedWorld, MigratedNavigation, ReusedRecast, Error));
	TestEqual(TEXT("Unchanged Apply keeps the same actor."), ReusedRecast, OwnedRecast);
	return true;
}

#endif
