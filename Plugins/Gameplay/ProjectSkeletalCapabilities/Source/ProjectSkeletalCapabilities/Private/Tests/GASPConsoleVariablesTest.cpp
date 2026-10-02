// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMotionMatchingSampleConsoleVariablesTest,
	"Project.Gameplay.SkeletalCapabilities.MotionMatching.SampleConsoleVariablesRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectMotionMatchingSampleConsoleVariablesTest::RunTest(const FString& Parameters)
{
	// This plugin's Config/Engine.ini registers these in every build, so a public
	// build without the sample still catches a lost entry.
	static const TCHAR* const DataDrivenNames[] = {
		TEXT("DDCvar.MMDatabaseLOD"),
		TEXT("DDCvar.OffsetRootBone.TranslationRadius"),
		TEXT("DDCVar.ThreadSafeAnimationUpdate.Enable"),
		TEXT("DDCVar.ExperimentalStateMachine.Enable"),
		TEXT("DDCVar.LocomotionSetupCMC"),
		TEXT("DDCvar.DrawCharacterDebugShapes"),
		TEXT("DDCvar.DrawCharacterDebugStates"),
		TEXT("DDCvar.DrawCharacterDebugGraphs"),
	};

	for (const TCHAR* Name : DataDrivenNames)
	{
		TestNotNull(FString::Printf(TEXT("%s is registered"), Name),
			IConsoleManager::Get().FindConsoleVariable(Name, false));
	}

	// Only the sample reads the engine-owned name, so it is required only when the
	// sample is mounted.
	if (!FPackageName::MountPointExists(TEXT("/MotionMatching/")))
	{
		AddInfo(TEXT("Motion Matching sample content is not mounted; the engine-owned name is not required."));
		return true;
	}

	TestNotNull(TEXT("a.animnode.offsetrootbone.enable is registered"),
		IConsoleManager::Get().FindConsoleVariable(TEXT("a.animnode.offsetrootbone.enable"), false));
	return true;
}

#endif
