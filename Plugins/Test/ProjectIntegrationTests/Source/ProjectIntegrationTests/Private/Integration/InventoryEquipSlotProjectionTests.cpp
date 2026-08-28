// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MVVM/InventoryViewModelEquipSlotBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryEquipSlotsUseCanonicalEmptySentinelTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.EquipSlots.EmptySlotsUseCanonicalSentinel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FInventoryEquipSlotsUseCanonicalEmptySentinelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FInventoryViewModelEquipSlotBuilder::FResult Result;
	FInventoryViewModelEquipSlotBuilder::Build({}, Result);

	TestEqual(TEXT("Every standard equipment slot is projected"), Result.InstanceIds.Num(), 8);
	for (int32 SlotIndex = 0; SlotIndex < Result.InstanceIds.Num(); ++SlotIndex)
	{
		TestEqual(
			FString::Printf(TEXT("Empty equipment slot %d uses INDEX_NONE"), SlotIndex),
			Result.InstanceIds[SlotIndex],
			INDEX_NONE);
	}
	return true;
}

#endif
