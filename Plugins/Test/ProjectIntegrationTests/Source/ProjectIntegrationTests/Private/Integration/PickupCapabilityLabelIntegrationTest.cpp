// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

#include "Pickup/PickupCapabilityComponent.h"
#include "Template/Interactable/InteractableActor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPickupCapabilityLabel_UsesDefinitionNameFallbackTest,
	"ProjectIntegrationTests.Interaction.Pickup.LabelUsesDefinitionNameFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPickupCapabilityLabel_UsesDefinitionNameFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!World)
	{
		AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"));
		World = AutomationCommon::GetAnyGameWorld();
	}

	TestNotNull(TEXT("Pickup label test world should resolve"), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AInteractableActor* Actor = World->SpawnActor<AInteractableActor>(
		AInteractableActor::StaticClass(),
		FTransform::Identity,
		SpawnParams);
	TestNotNull(TEXT("Interactable actor should spawn"), Actor);
	if (!Actor)
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	};

	Actor->ObjectDefinitionId = FPrimaryAssetId(
		FPrimaryAssetType(TEXT("ObjectDefinition")),
		FName(TEXT("Backpack")));

	UPickupCapabilityComponent* Pickup = NewObject<UPickupCapabilityComponent>(Actor, TEXT("PickupCapability"));
	TestNotNull(TEXT("Pickup capability should create"), Pickup);
	if (!Pickup)
	{
		return false;
	}

	const FText Label = IInteractableComponentTargetInterface::Execute_GetInteractionLabel(Pickup);
	TestEqual(TEXT("Pickup label should include the item identity fallback"), Label.ToString(), FString(TEXT("Pick up Backpack")));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FPickupCapabilityLabel_UsesDefinitionNameFallbackTest,
	"ProjectIntegrationTests.Interaction.Pickup.LabelUsesDefinitionNameFallback",
	"[Fast][Integration][Interaction]")

#endif // WITH_DEV_AUTOMATION_TESTS
