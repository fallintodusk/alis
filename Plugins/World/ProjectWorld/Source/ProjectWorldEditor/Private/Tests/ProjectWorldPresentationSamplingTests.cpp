// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

// Focused coverage for the packaged Presentation Gate decision policy so the
// sampling and runtime-role contract is proven without the expensive packaged
// rendered run: invalid frame timing rejects rather than extending the sample
// window, roles streaming in across inspections accumulate to the required
// set, and stale or duplicated ownership invalidates the loaded state at any
// inspection point.

#include "Presentation/ProjectWorldPresentationSampling.h"

#include "Editor.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldPresentationSamplingTests
{
	ATargetPoint* SpawnRoleActor(
		UWorld& World,
		const FString& Role,
		const FString& ProfileId,
		const FString& ProfileHash)
	{
		ATargetPoint* Actor = World.SpawnActor<ATargetPoint>();
		Actor->Tags.Add(FName(*(TEXT("ProjectWorld.RuntimeRole=") + Role)));
		if (!ProfileId.IsEmpty())
		{
			Actor->Tags.Add(FName(*(TEXT("ProjectWorld.Runtime=") + ProfileId)));
		}
		if (!ProfileHash.IsEmpty())
		{
			Actor->Tags.Add(FName(*(TEXT("ProjectWorld.RuntimeHash=") + ProfileHash)));
		}
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPresentationSampleFrameValidityTest,
	"Project.World.Realization.Presentation.SampleFrameValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPresentationSampleFrameValidityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldPresentation;
	TestTrue(TEXT("A normal frame duration is a valid sample."), IsValidSampleFrameMs(16.7));
	TestTrue(
		TEXT("A catastrophic stall above one second is a valid sample and is never filtered."),
		IsValidSampleFrameMs(5000.0));
	TestFalse(TEXT("A zero duration is not a rendered frame."), IsValidSampleFrameMs(0.0));
	TestFalse(TEXT("A negative duration is not a rendered frame."), IsValidSampleFrameMs(-5.0));
	TestFalse(
		TEXT("A NaN duration rejects instead of extending the window."),
		IsValidSampleFrameMs(std::numeric_limits<double>::quiet_NaN()));
	TestFalse(
		TEXT("An infinite duration rejects instead of extending the window."),
		IsValidSampleFrameMs(std::numeric_limits<double>::infinity()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPresentationRuntimeRoleScanTest,
	"Project.World.Realization.Presentation.RuntimeRoleScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPresentationRuntimeRoleScanTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldPresentation;
	using namespace ProjectWorldPresentationSamplingTests;
	const FString ProfileId = TEXT("runtime_v1");
	const FString ProfileHash = FString::ChrN(64, TEXT('a'));
	UWorld* World = GEditor->NewMap(false);

	// Roles that stream in between inspections accumulate to the full set.
	TSet<FString> Observed;
	SpawnRoleActor(*World, TEXT("RouteStart"), ProfileId, ProfileHash);
	const FRuntimeRoleScan First = ScanRuntimeRoles(*World, ProfileId, ProfileHash);
	TestTrue(TEXT("A valid loaded state scans clean."), First.bValid);
	Observed.Append(First.LoadedRoles);
	TestEqual(
		TEXT("Two required roles are still missing after the first inspection."),
		MissingRequiredRoles(Observed).Num(),
		2);

	SpawnRoleActor(*World, TEXT("RouteEnd"), ProfileId, ProfileHash);
	SpawnRoleActor(*World, TEXT("RouteNavigation"), ProfileId, ProfileHash);
	const FRuntimeRoleScan Second = ScanRuntimeRoles(*World, ProfileId, ProfileHash);
	TestTrue(TEXT("Roles streamed in after the first inspection scan clean."), Second.bValid);
	Observed.Append(Second.LoadedRoles);
	TestEqual(
		TEXT("Accumulated observations complete the required route set."),
		MissingRequiredRoles(Observed).Num(),
		0);

	// A role duplicated in one loaded state invalidates the scan even when
	// the duplicate appears only mid-sequence.
	ATargetPoint* Duplicate = SpawnRoleActor(*World, TEXT("RouteStart"), ProfileId, ProfileHash);
	const FRuntimeRoleScan DuplicateScan = ScanRuntimeRoles(*World, ProfileId, ProfileHash);
	TestFalse(TEXT("A duplicated runtime role invalidates the loaded state."), DuplicateScan.bValid);
	TestTrue(
		TEXT("The duplicate rejection names the ambiguous role."),
		DuplicateScan.Error.Contains(TEXT("RouteStart")));
	Duplicate->Destroy();

	// Stale hash ownership invalidates the scan even when it appears late.
	ATargetPoint* StaleHash =
		SpawnRoleActor(*World, TEXT("RouteExtra"), ProfileId, FString::ChrN(64, TEXT('b')));
	const FRuntimeRoleScan StaleHashScan = ScanRuntimeRoles(*World, ProfileId, ProfileHash);
	TestFalse(TEXT("Stale hash ownership invalidates the loaded state."), StaleHashScan.bValid);
	StaleHash->Destroy();

	// A role tag with no profile ownership at all is equally stale.
	ATargetPoint* Unowned = SpawnRoleActor(*World, TEXT("RouteExtra"), FString(), FString());
	const FRuntimeRoleScan UnownedScan = ScanRuntimeRoles(*World, ProfileId, ProfileHash);
	TestFalse(TEXT("A role without profile ownership invalidates the loaded state."), UnownedScan.bValid);
	Unowned->Destroy();

	// After removing the invalid actors the accumulated state remains usable.
	const FRuntimeRoleScan Final = ScanRuntimeRoles(*World, ProfileId, ProfileHash);
	TestTrue(TEXT("The cleaned loaded state scans clean again."), Final.bValid);
	return true;
}

#endif
