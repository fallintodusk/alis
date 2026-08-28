// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SinglePlayTraversalPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayTraversalPolicyResolutionTest,
	"ProjectIntegrationTests.SinglePlay.Traversal.PolicyResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSinglePlayTraversalPolicyResolutionTest::RunTest(const FString& Parameters)
{
	const FSinglePlayTraversalSelection Absent = ProjectSinglePlayTraversal::Resolve(TEXT(""));
	TestEqual(TEXT("An absent option preserves default traversal."),
		Absent.Mode, ESinglePlayTraversalMode::Default);
	TestEqual(TEXT("Absence is an expected silent result."),
		Absent.ParseResult, ESinglePlayTraversalParseResult::Absent);

	const FSinglePlayTraversalSelection PreviewFlight =
		ProjectSinglePlayTraversal::Resolve(TEXT("PreviewFlight"));
	TestEqual(TEXT("The supported value selects preview flight."),
		PreviewFlight.Mode, ESinglePlayTraversalMode::PreviewFlight);
	TestEqual(TEXT("The supported value is recognized."),
		PreviewFlight.ParseResult, ESinglePlayTraversalParseResult::Supported);

	const FSinglePlayTraversalSelection Unknown =
		ProjectSinglePlayTraversal::Resolve(TEXT("previewflight"));
	TestEqual(TEXT("Unknown values fail closed."), Unknown.Mode, ESinglePlayTraversalMode::Default);
	TestEqual(TEXT("Unknown values remain distinguishable for owner-scoped warning."),
		Unknown.ParseResult, ESinglePlayTraversalParseResult::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayTraversalPolicyLifecycleTest,
	"ProjectIntegrationTests.SinglePlay.Traversal.SpawnLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSinglePlayTraversalPolicyLifecycleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Default traversal is a no-op for a missing pawn."),
		ProjectSinglePlayTraversal::Apply(nullptr, ESinglePlayTraversalMode::Default));
	TestFalse(TEXT("Preview flight fails closed for a missing pawn."),
		ProjectSinglePlayTraversal::Apply(nullptr, ESinglePlayTraversalMode::PreviewFlight));

	FString Source;
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Private/SinglePlayerGameMode.cpp"));
	TestTrue(TEXT("The SinglePlay lifecycle owner is readable."),
		FFileHelper::LoadFileToString(Source, *SourcePath));
	TestTrue(TEXT("Spawn and respawn initialization reapplies the generic traversal policy."),
		Source.Contains(TEXT("ProjectSinglePlayTraversal::Apply(Pawn, TraversalMode)")));
	return true;
}

#endif
