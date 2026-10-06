// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldTerritoryRuntimeAcceptance.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldTerritoryRuntimeInstancingPolicyTest,
	"Project.World.Realization.Runtime.TerritoryInstancingPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTerritoryRuntimeInstancingPolicyTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		bool bSelected;
		bool bPresent;
		bool bValidHism;
		bool bExpected;
	};
	const FCase Cases[] = {
		{false, false, false, true},
		{true, false, false, false},
		{false, true, false, false},
		{false, true, true, true},
		{true, true, true, true},
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
	{
		const FCase& Case = Cases[Index];
		TestEqual(
			*FString::Printf(TEXT("Instancing policy case %d"), Index),
			ProjectWorldTerritoryRuntimeAcceptance::IsInstancingPolicySatisfied(
				Case.bSelected, Case.bPresent, Case.bValidHism),
			Case.bExpected);
	}
	TestFalse(TEXT("A vegetation actor without mesh components cannot prove instancing."),
		ProjectWorldTerritoryRuntimeAcceptance::HasValidVegetationInstancing(0, true));
	TestFalse(TEXT("A vegetation actor with a non-HISM or empty HISM cannot prove instancing."),
		ProjectWorldTerritoryRuntimeAcceptance::HasValidVegetationInstancing(1, false));
	TestTrue(TEXT("A vegetation actor with valid HISM components proves instancing."),
		ProjectWorldTerritoryRuntimeAcceptance::HasValidVegetationInstancing(1, true));
	return true;
}

#endif
