// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldRuntimePartitionPolicy.h"

#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldTerritoryRuntimeProfileContractTest,
	"Project.World.Realization.Runtime.TerritoryProfileContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTerritoryRuntimeProfileContractTest::RunTest(const FString& Parameters)
{
	struct FCandidate
	{
		const TCHAR* FileName;
		int32 CellSizeMeters;
		int32 LoadingRangeMeters;
	};
	const FCandidate Candidates[] = {
		{TEXT("kazan_territory_128_768_v1.json"), 128, 768},
		{TEXT("kazan_territory_256_768_v1.json"), 256, 768},
		{TEXT("kazan_territory_512_1536_v1.json"), 512, 1536}};
	for (const FCandidate& Candidate : Candidates)
	{
		const FString ProfilePath = FPaths::Combine(
			FPaths::ProjectPluginsDir(), TEXT("World/ProjectWorldData/Data/Runtime"), Candidate.FileName);
		FProjectWorldRuntimeProfile Profile;
		FString ErrorCode;
		FString Error;
		TestTrue(
			FString::Printf(TEXT("Candidate %s is executable."), Candidate.FileName),
			ProjectWorldRuntimeProfile::Load(ProfilePath, Profile, ErrorCode, Error));
		TestEqual(TEXT("Candidate owns product territory behavior."), Profile.ProfileKind, FString(TEXT("territory_product")));
		TestEqual(TEXT("Candidate uses native LHGrid."), Profile.RuntimePartitionClass, FString(TEXT("lhgrid_2d")));
		TestEqual(TEXT("Candidate cell size is pinned."), Profile.RuntimeCellSizeMeters, Candidate.CellSizeMeters);
		TestEqual(TEXT("Candidate loading range is pinned."), Profile.RuntimeLoadingRangeMeters, Candidate.LoadingRangeMeters);
		TestTrue(TEXT("Candidate blocks readiness on slow streaming."), Profile.bBlockOnSlowStreaming);
		TestEqual(TEXT("Candidate starts at the Spasskaya-derived engine origin."),
			Profile.ProductSpawnAnchor, FString(TEXT("engine_georeference_origin")));
		TestEqual(TEXT("Candidate starts above the landmark massing."),
			Profile.ProductSpawnHeightAboveTerrainMeters, 180.0);
		TestEqual(TEXT("Candidate starts with the selected city-facing yaw."),
			Profile.ProductSpawnYawDegrees, 45.0);
		TestEqual(TEXT("Candidate starts with a downward overview pitch."),
			Profile.ProductSpawnPitchDegrees, -20.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldTerritoryRuntimeHashSetReadbackTest,
	"Project.World.Realization.Runtime.TerritoryHashSetReadback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTerritoryRuntimeHashSetReadbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor->NewMap(true);
	FProjectWorldRuntimePartitionSettings Expected;
	Expected.PartitionCount = 1;
	Expected.CellSizeCentimeters = 25600;
	Expected.LoadingRangeCentimeters = 76800;
	Expected.bIs2D = true;
	Expected.bBlockOnSlowStreaming = true;
	bool bChanged = false;
	FString Error;
	if (!TestTrue(
		TEXT("The profile applies through UE 5.8 Runtime Hash Set."),
		ProjectWorldRuntimePartitionPolicy::Apply(World, Expected, bChanged, Error)))
	{
		AddError(Error);
		return false;
	}
	FProjectWorldRuntimePartitionSettings Actual;
	TestTrue(TEXT("Applied settings are readable."), ProjectWorldRuntimePartitionPolicy::Read(World, Actual, Error));
	TestEqual(TEXT("Exactly one runtime partition remains."), Actual.PartitionCount, 1);
	TestEqual(TEXT("Cell size round-trips."), Actual.CellSizeCentimeters, 25600);
	TestEqual(TEXT("Loading range round-trips."), Actual.LoadingRangeCentimeters, 76800);
	TestTrue(TEXT("The runtime grid is 2D."), Actual.bIs2D);
	TestTrue(TEXT("Readiness blocks on slow streaming."), Actual.bBlockOnSlowStreaming);
	TestTrue(TEXT("First application changes the engine defaults."), bChanged);
	TestTrue(TEXT("An unchanged application is accepted."), ProjectWorldRuntimePartitionPolicy::Apply(World, Expected, bChanged, Error));
	TestFalse(TEXT("An unchanged application has zero partition churn."), bChanged);
	return true;
}

#endif
