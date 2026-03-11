// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemComponent.h"
#include "Attributes/HealthAttributeSet.h"
#include "Attributes/StaminaAttributeSet.h"
#include "Attributes/SurvivalAttributeSet.h"
#include "Attributes/StatusAttributeSet.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "MVVM/VitalsViewModel.h"
#include "Widgets/W_VitalsHUD.h"
#include "Widgets/W_VitalsPanel.h"
#include "ProjectGameplayTags.h"
#include "ProjectVitalsComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool LoadJsonArrayFromFile(const FString& FilePath, TArray<TSharedPtr<FJsonValue>>& OutArray)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
		{
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		return FJsonSerializer::Deserialize(Reader, OutArray);
	}

	bool LoadJsonObjectFromFile(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
		{
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		return FJsonSerializer::Deserialize(Reader, OutObject);
	}

	TSharedPtr<FJsonObject> FindDefinitionById(const TArray<TSharedPtr<FJsonValue>>& Definitions, const FString& Id)
	{
		for (const TSharedPtr<FJsonValue>& Value : Definitions)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}

			FString CurrentId;
			if (Object->TryGetStringField(TEXT("id"), CurrentId) && CurrentId == Id)
			{
				return Object;
			}
		}

		return nullptr;
	}

	void CollectWidgetNames(const TSharedPtr<FJsonObject>& WidgetNode, TSet<FString>& OutNames)
	{
		if (!WidgetNode.IsValid())
		{
			return;
		}

		FString NodeName;
		if (WidgetNode->TryGetStringField(TEXT("name"), NodeName) && !NodeName.IsEmpty())
		{
			OutNames.Add(NodeName);
		}

		const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
		if (!WidgetNode->TryGetArrayField(TEXT("children"), Children) || !Children)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Child : *Children)
		{
			const TSharedPtr<FJsonObject> ChildObject = Child.IsValid() ? Child->AsObject() : nullptr;
			if (ChildObject.IsValid())
			{
				CollectWidgetNames(ChildObject, OutNames);
			}
		}
	}

	UWorld* ResolveVitalsTestWorld()
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (World)
		{
			return World;
		}

		if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
		{
			return nullptr;
		}

		return AutomationCommon::GetAnyGameWorld();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsUIDefinitionsContractTest,
	"ProjectIntegrationTests.UI.Vitals.DefinitionsContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsUIDefinitionsContractTest::RunTest(const FString& Parameters)
{
	const FString DefinitionsPath =
		UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectVitalsUI"), TEXT("ui_definitions.json"));

	if (!TestFalse(TEXT("Vitals ui_definitions path should not be empty"), DefinitionsPath.IsEmpty()))
	{
		return false;
	}

	if (!TestTrue(TEXT("Vitals ui_definitions.json should exist"), FPaths::FileExists(DefinitionsPath)))
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Definitions;
	if (!LoadJsonArrayFromFile(DefinitionsPath, Definitions))
	{
		AddError(FString::Printf(TEXT("Failed to parse %s"), *DefinitionsPath));
		return false;
	}

	const TSharedPtr<FJsonObject> HUDDef = FindDefinitionById(Definitions, TEXT("ProjectVitalsUI.VitalsHUD"));
	if (!TestTrue(TEXT("VitalsHUD definition should exist"), HUDDef.IsValid()))
	{
		return false;
	}

	const TSharedPtr<FJsonObject> PanelDef = FindDefinitionById(Definitions, TEXT("ProjectVitalsUI.VitalsPanel"));
	if (!TestTrue(TEXT("VitalsPanel definition should exist"), PanelDef.IsValid()))
	{
		return false;
	}

	FString Value;

	TestTrue(TEXT("VitalsHUD should have slot field"), HUDDef->TryGetStringField(TEXT("slot"), Value));
	TestEqual(TEXT("VitalsHUD slot"), Value, FString(TEXT("HUD.Slot.VitalsMini")));

	TestTrue(TEXT("VitalsHUD layer field"), HUDDef->TryGetStringField(TEXT("layer"), Value));
	TestEqual(TEXT("VitalsHUD layer"), Value, FString(TEXT("UI.Layer.Game")));

	TestTrue(TEXT("VitalsHUD widget_class field"), HUDDef->TryGetStringField(TEXT("widget_class"), Value));
	TestEqual(TEXT("VitalsHUD widget_class"), Value, FString(TEXT("/Script/ProjectVitalsUI.W_VitalsHUD")));

	TestTrue(TEXT("VitalsHUD viewmodel_class field"), HUDDef->TryGetStringField(TEXT("viewmodel_class"), Value));
	TestEqual(TEXT("VitalsHUD viewmodel_class"), Value, FString(TEXT("/Script/ProjectVitalsUI.VitalsViewModel")));

	TestTrue(TEXT("VitalsHUD layout_json field"), HUDDef->TryGetStringField(TEXT("layout_json"), Value));
	TestEqual(TEXT("VitalsHUD layout_json"), Value, FString(TEXT("VitalsHUD.json")));

	TestTrue(TEXT("VitalsHUD vm_creation field"), HUDDef->TryGetStringField(TEXT("vm_creation"), Value));
	TestEqual(TEXT("VitalsHUD vm_creation"), Value, FString(TEXT("Global")));

	TestTrue(TEXT("VitalsHUD load field"), HUDDef->TryGetStringField(TEXT("load"), Value));
	TestEqual(TEXT("VitalsHUD load mode"), Value, FString(TEXT("Preload")));

	TestTrue(TEXT("VitalsHUD input field"), HUDDef->TryGetStringField(TEXT("input"), Value));
	TestEqual(TEXT("VitalsHUD input"), Value, FString(TEXT("Game")));

	TestFalse(TEXT("VitalsPanel should not define a slot field"), PanelDef->HasField(TEXT("slot")));

	TestTrue(TEXT("VitalsPanel layer field"), PanelDef->TryGetStringField(TEXT("layer"), Value));
	TestEqual(TEXT("VitalsPanel layer"), Value, FString(TEXT("UI.Layer.Menu")));

	TestTrue(TEXT("VitalsPanel layout_json field"), PanelDef->TryGetStringField(TEXT("layout_json"), Value));
	TestEqual(TEXT("VitalsPanel layout_json"), Value, FString(TEXT("VitalsPanel.json")));

	TestTrue(TEXT("VitalsPanel vm_creation field"), PanelDef->TryGetStringField(TEXT("vm_creation"), Value));
	TestEqual(TEXT("VitalsPanel vm_creation"), Value, FString(TEXT("Global")));

	TestTrue(TEXT("VitalsPanel load field"), PanelDef->TryGetStringField(TEXT("load"), Value));
	TestEqual(TEXT("VitalsPanel load mode"), Value, FString(TEXT("OnDemand")));

	TestTrue(TEXT("VitalsPanel input field"), PanelDef->TryGetStringField(TEXT("input"), Value));
	TestEqual(TEXT("VitalsPanel input"), Value, FString(TEXT("Menu")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsUILayoutContractTest,
	"ProjectIntegrationTests.UI.Vitals.LayoutContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsUILayoutContractTest::RunTest(const FString& Parameters)
{
	const FString HUDLayoutPath =
		UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectVitalsUI"), TEXT("VitalsHUD.json"));
	const FString PanelLayoutPath =
		UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectVitalsUI"), TEXT("VitalsPanel.json"));

	if (!TestTrue(TEXT("VitalsHUD.json should exist"), FPaths::FileExists(HUDLayoutPath)))
	{
		return false;
	}

	if (!TestTrue(TEXT("VitalsPanel.json should exist"), FPaths::FileExists(PanelLayoutPath)))
	{
		return false;
	}

	TSharedPtr<FJsonObject> HUDLayout;
	if (!LoadJsonObjectFromFile(HUDLayoutPath, HUDLayout))
	{
		AddError(FString::Printf(TEXT("Failed to parse %s"), *HUDLayoutPath));
		return false;
	}

	TSharedPtr<FJsonObject> HUDRoot;
	if (!HUDLayout->HasTypedField<EJson::Object>(TEXT("root")))
	{
		AddError(TEXT("VitalsHUD layout missing root object"));
		return false;
	}
	HUDRoot = HUDLayout->GetObjectField(TEXT("root"));

	FString RootType;
	TestTrue(TEXT("VitalsHUD root type field"), HUDRoot->TryGetStringField(TEXT("type"), RootType));
	TestEqual(TEXT("VitalsHUD root type"), RootType, FString(TEXT("CanvasPanel")));

	TSet<FString> HUDNames;
	CollectWidgetNames(HUDRoot, HUDNames);
	TestTrue(TEXT("VitalsHUD should include ConditionBar"), HUDNames.Contains(TEXT("ConditionBar")));
	TestTrue(TEXT("VitalsHUD should include WarningIcons"), HUDNames.Contains(TEXT("WarningIcons")));
	TestTrue(TEXT("VitalsHUD should include ExpandButton"), HUDNames.Contains(TEXT("ExpandButton")));

	TSharedPtr<FJsonObject> PanelLayout;
	if (!LoadJsonObjectFromFile(PanelLayoutPath, PanelLayout))
	{
		AddError(FString::Printf(TEXT("Failed to parse %s"), *PanelLayoutPath));
		return false;
	}

	TSharedPtr<FJsonObject> PanelRoot;
	if (!PanelLayout->HasTypedField<EJson::Object>(TEXT("root")))
	{
		AddError(TEXT("VitalsPanel layout missing root object"));
		return false;
	}
	PanelRoot = PanelLayout->GetObjectField(TEXT("root"));

	TestTrue(TEXT("VitalsPanel root type field"), PanelRoot->TryGetStringField(TEXT("type"), RootType));
	TestEqual(TEXT("VitalsPanel root type"), RootType, FString(TEXT("CanvasPanel")));

	TSet<FString> PanelNames;
	CollectWidgetNames(PanelRoot, PanelNames);
	TestTrue(TEXT("VitalsPanel should include ConditionBar"), PanelNames.Contains(TEXT("ConditionBar")));
	TestTrue(TEXT("VitalsPanel should include StaminaBar"), PanelNames.Contains(TEXT("StaminaBar")));
	TestTrue(TEXT("VitalsPanel should include CaloriesBar"), PanelNames.Contains(TEXT("CaloriesBar")));
	TestTrue(TEXT("VitalsPanel should include HydrationBar"), PanelNames.Contains(TEXT("HydrationBar")));
	TestTrue(TEXT("VitalsPanel should include FatigueBar"), PanelNames.Contains(TEXT("FatigueBar")));
	TestTrue(TEXT("VitalsPanel should include StatusEffects"), PanelNames.Contains(TEXT("StatusEffects")));
	TestTrue(TEXT("VitalsPanel should include CloseButton"), PanelNames.Contains(TEXT("CloseButton")));
	TestTrue(TEXT("VitalsPanel should include OverlayButton"), PanelNames.Contains(TEXT("OverlayButton")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsTagContractTest,
	"ProjectIntegrationTests.Gameplay.Vitals.TagContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsTagContractTest::RunTest(const FString& Parameters)
{
	const FGameplayTag State = ProjectTags::State.GetTag();
	const FGameplayTag StateCondition = ProjectTags::State_Condition.GetTag();
	const FGameplayTag StateConditionOK = ProjectTags::State_Condition_OK.GetTag();
	const FGameplayTag StateCaloriesCritical = ProjectTags::State_Calories_Critical.GetTag();
	const FGameplayTag StateHydrationCritical = ProjectTags::State_Hydration_Critical.GetTag();
	const FGameplayTag StateHydrationEmpty = ProjectTags::State_Hydration_Empty.GetTag();
	const FGameplayTag StateFatigueRested = ProjectTags::State_Fatigue_Rested.GetTag();
	const FGameplayTag StateFatigueCritical = ProjectTags::State_Fatigue_Critical.GetTag();
	const FGameplayTag StateWeightHeavy = ProjectTags::State_Weight_Heavy.GetTag();
	const FGameplayTag StateWeightOverweight = ProjectTags::State_Weight_Overweight.GetTag();
	const FGameplayTag HUDSlotVitalsMini = ProjectTags::HUD_Slot_VitalsMini.GetTag();

	TestTrue(TEXT("State root tag should be valid"), State.IsValid());
	TestTrue(TEXT("State.Condition tag should be valid"), StateCondition.IsValid());
	TestTrue(TEXT("State.Condition.OK tag should be valid"), StateConditionOK.IsValid());
	TestTrue(TEXT("State.Fatigue.Critical tag should be valid"), StateFatigueCritical.IsValid());

	TestEqual(TEXT("State.Condition.OK literal"), StateConditionOK.ToString(), FString(TEXT("State.Condition.OK")));
	TestEqual(TEXT("State.Calories.Critical literal"), StateCaloriesCritical.ToString(), FString(TEXT("State.Calories.Critical")));
	TestEqual(TEXT("State.Hydration.Empty literal"), StateHydrationEmpty.ToString(), FString(TEXT("State.Hydration.Empty")));
	TestEqual(TEXT("State.Fatigue.Rested literal"), StateFatigueRested.ToString(), FString(TEXT("State.Fatigue.Rested")));

	TestTrue(TEXT("State.Condition.OK should match State.Condition"), StateConditionOK.MatchesTag(StateCondition));
	TestTrue(TEXT("State.Hydration.Critical should match State.Hydration"),
		StateHydrationCritical.MatchesTag(ProjectTags::State_Hydration.GetTag()));

	TestTrue(TEXT("State.Weight.Heavy tag should be valid"), StateWeightHeavy.IsValid());
	TestTrue(TEXT("State.Weight.Overweight tag should be valid"), StateWeightOverweight.IsValid());

	TestEqual(TEXT("HUD slot tag literal"), HUDSlotVitalsMini.ToString(), FString(TEXT("HUD.Slot.VitalsMini")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsHysteresisTransitionsTest,
	"ProjectIntegrationTests.Gameplay.Vitals.HysteresisTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsHysteresisTransitionsTest::RunTest(const FString& Parameters)
{
	UProjectVitalsComponent* Component = NewObject<UProjectVitalsComponent>(GetTransientPackage(), UProjectVitalsComponent::StaticClass());
	if (!TestNotNull(TEXT("Vitals component should be created"), Component))
	{
		return false;
	}

	// Vital hysteresis (normal direction): 70/40/20 with 2% exit buffer.
	TestEqual(TEXT("OK should hold above 68% exit threshold"),
		Component->TestComputeVitalStateWithHysteresis(0.69f, EVitalState::OK), EVitalState::OK);
	TestEqual(TEXT("OK should drop to Low below 68%"),
		Component->TestComputeVitalStateWithHysteresis(0.67f, EVitalState::OK), EVitalState::Low);

	// Large jumps should settle within a single evaluation (iterative transitions).
	TestEqual(TEXT("Large drop should settle from OK to Empty"),
		Component->TestComputeVitalStateWithHysteresis(0.10f, EVitalState::OK), EVitalState::Empty);
	TestEqual(TEXT("Large rise should settle from Empty to OK"),
		Component->TestComputeVitalStateWithHysteresis(0.80f, EVitalState::Empty), EVitalState::OK);

	// Fatigue hysteresis (inverted thresholds): 30/60/85 with 2% exit buffer.
	TestEqual(TEXT("Rested should hold below 32% exit threshold"),
		Component->TestComputeFatigueStateWithHysteresis(0.31f, EFatigueState::Rested), EFatigueState::Rested);
	TestEqual(TEXT("Rested should advance to Tired above 32%"),
		Component->TestComputeFatigueStateWithHysteresis(0.33f, EFatigueState::Rested), EFatigueState::Tired);

	// Large jumps should also settle in inverted direction.
	TestEqual(TEXT("Large rise should settle from Rested to Critical"),
		Component->TestComputeFatigueStateWithHysteresis(0.90f, EFatigueState::Rested), EFatigueState::Critical);
	TestEqual(TEXT("Large recovery should settle from Critical to Rested"),
		Component->TestComputeFatigueStateWithHysteresis(0.10f, EFatigueState::Critical), EFatigueState::Rested);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsDebuffHandleCleanupTest,
	"ProjectIntegrationTests.Gameplay.Vitals.DebuffHandleCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsDebuffHandleCleanupTest::RunTest(const FString& Parameters)
{
	UProjectVitalsComponent* Component = NewObject<UProjectVitalsComponent>(GetTransientPackage(), UProjectVitalsComponent::StaticClass());
	if (!TestNotNull(TEXT("Vitals component should be created"), Component))
	{
		return false;
	}

	// KISS coverage: verify cleanup path invalidates all handles even when ASC is gone.
	Component->TestSeedDebuffHandles();
	Component->TestClearDebuffsWithoutASC();
	TestTrue(TEXT("All debuff handles should be invalidated"), Component->TestAreAllDebuffHandlesInvalid());

	// Idempotent safety check.
	Component->TestClearDebuffsWithoutASC();
	TestTrue(TEXT("Cleanup remains stable on repeated calls"), Component->TestAreAllDebuffHandlesInvalid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsDebuffLifecycleASCTest,
	"ProjectIntegrationTests.Gameplay.Vitals.DebuffLifecycleASC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsDebuffLifecycleASCTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveVitalsTestWorld();
	if (!TestNotNull(TEXT("Automation world should exist"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	AActor* TestActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Test actor should spawn"), TestActor))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(TestActor, TEXT("VitalsTestASC"));
	TestActor->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	if (!TestNotNull(TEXT("ASC should be created"), ASC))
	{
		TestActor->Destroy();
		return false;
	}

	UProjectVitalsComponent* Vitals = NewObject<UProjectVitalsComponent>(TestActor, TEXT("VitalsTestComponent"));
	TestActor->AddInstanceComponent(Vitals);
	Vitals->RegisterComponent();
	if (!TestNotNull(TEXT("Vitals component should be created"), Vitals))
	{
		TestActor->Destroy();
		return false;
	}

	UHealthAttributeSet* HealthSet = NewObject<UHealthAttributeSet>(TestActor, TEXT("VitalsTestHealthSet"));
	UStaminaAttributeSet* StaminaSet = NewObject<UStaminaAttributeSet>(TestActor, TEXT("VitalsTestStaminaSet"));
	USurvivalAttributeSet* SurvivalSet = NewObject<USurvivalAttributeSet>(TestActor, TEXT("VitalsTestSurvivalSet"));
	UStatusAttributeSet* StatusSet = NewObject<UStatusAttributeSet>(TestActor, TEXT("VitalsTestStatusSet"));
	ASC->AddAttributeSetSubobject(HealthSet);
	ASC->AddAttributeSetSubobject(StaminaSet);
	ASC->AddAttributeSetSubobject(SurvivalSet);
	ASC->AddAttributeSetSubobject(StatusSet);

	ASC->InitAbilityActorInfo(TestActor, TestActor);

	// Stage 1: Force calories to empty/critical zone -> critical debuff should block condition regen.
	ASC->SetNumericAttributeBase(UHealthAttributeSet::GetConditionRegenRateAttribute(), 1.0f);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetMaxCaloriesAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetMaxHydrationAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetMaxFatigueAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetCaloriesAttribute(), 10.0f);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetHydrationAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetFatigueAttribute(), 0.0f);

	Vitals->TestTickVitalsOnce();

	const UHealthAttributeSet* HealthAfterApply = ASC->GetSet<UHealthAttributeSet>();
	const bool bCaloriesCriticalOrEmpty =
		ASC->HasMatchingGameplayTag(ProjectTags::State_Calories_Critical.GetTag()) ||
		ASC->HasMatchingGameplayTag(ProjectTags::State_Calories_Empty.GetTag());

	TestTrue(TEXT("Calories should be in critical/empty state after low value"), bCaloriesCriticalOrEmpty);
	TestTrue(TEXT("Condition regen rate should be blocked by critical calories debuff"),
		HealthAfterApply && FMath::IsNearlyEqual(HealthAfterApply->GetConditionRegenRate(), 0.0f, KINDA_SMALL_NUMBER));

	// Stage 2: Recover calories -> debuff should be removed and regen rate restored.
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetCaloriesAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetHydrationAttribute(), 100.0f);
	Vitals->TestTickVitalsOnce();

	const UHealthAttributeSet* HealthAfterRecover = ASC->GetSet<UHealthAttributeSet>();
	TestTrue(TEXT("Calories should recover to OK state"),
		ASC->HasMatchingGameplayTag(ProjectTags::State_Calories_OK.GetTag()));
	TestTrue(TEXT("Condition regen rate should restore after debuff removal"),
		HealthAfterRecover && FMath::IsNearlyEqual(HealthAfterRecover->GetConditionRegenRate(), 1.0f, KINDA_SMALL_NUMBER));

	TestActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsViewModelTagPriorityTest,
	"ProjectIntegrationTests.UI.Vitals.ViewModelTagPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsViewModelTagPriorityTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveVitalsTestWorld();
	if (!TestNotNull(TEXT("Automation world should exist"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	AActor* TestActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Test actor should spawn"), TestActor))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(TestActor, TEXT("VitalsUITestASC"));
	TestActor->AddInstanceComponent(ASC);
	ASC->RegisterComponent();

	ASC->AddAttributeSetSubobject(NewObject<UHealthAttributeSet>(TestActor, TEXT("VitalsUITestHealthSet")));
	ASC->AddAttributeSetSubobject(NewObject<UStaminaAttributeSet>(TestActor, TEXT("VitalsUITestStaminaSet")));
	ASC->AddAttributeSetSubobject(NewObject<USurvivalAttributeSet>(TestActor, TEXT("VitalsUITestSurvivalSet")));
	ASC->AddAttributeSetSubobject(NewObject<UStatusAttributeSet>(TestActor, TEXT("VitalsUITestStatusSet")));
	ASC->InitAbilityActorInfo(TestActor, TestActor);

	// Keep calories numerically in OK range, then force a conflicting tag state.
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetMaxCaloriesAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USurvivalAttributeSet::GetCaloriesAttribute(), 90.0f);

	UVitalsViewModel* VM = NewObject<UVitalsViewModel>(GetTransientPackage(), UVitalsViewModel::StaticClass());
	if (!TestNotNull(TEXT("VitalsViewModel should be created"), VM))
	{
		TestActor->Destroy();
		return false;
	}

	VM->Initialize(ASC);
	VM->RefreshFromASC();
	TestEqual(TEXT("Without tags, ViewModel should use percent fallback (90% -> OK)"), VM->GetCaloriesState(), EVitalState::OK);

	ASC->AddLooseGameplayTag(ProjectTags::State_Calories_Critical.GetTag());
	VM->RefreshFromASC();
	TestEqual(TEXT("State tag should override percent fallback (tag priority)"), VM->GetCaloriesState(), EVitalState::Critical);

	TestActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectVitalsSharedViewModelContractTest,
	"ProjectIntegrationTests.UI.Vitals.SharedViewModelContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectVitalsSharedViewModelContractTest::RunTest(const FString& Parameters)
{
	UVitalsViewModel* SharedVM = NewObject<UVitalsViewModel>(GetTransientPackage(), UVitalsViewModel::StaticClass());
	UAbilitySystemComponent* SharedASCContext = NewObject<UAbilitySystemComponent>(GetTransientPackage(), TEXT("SharedVMContextASC"));
	UW_VitalsHUD* HUD = NewObject<UW_VitalsHUD>(GetTransientPackage(), UW_VitalsHUD::StaticClass());
	UW_VitalsPanel* Panel = NewObject<UW_VitalsPanel>(GetTransientPackage(), UW_VitalsPanel::StaticClass());

	if (!TestNotNull(TEXT("Shared VitalsViewModel should be created"), SharedVM) ||
		!TestNotNull(TEXT("Shared VM context ASC should be created"), SharedASCContext) ||
		!TestNotNull(TEXT("Vitals HUD should be created"), HUD) ||
		!TestNotNull(TEXT("Vitals Panel should be created"), Panel))
	{
		return false;
	}

	SharedVM->Initialize(SharedASCContext);
	HUD->SetVitalsViewModel(SharedVM);
	Panel->SetVitalsViewModel(SharedVM);

	TestTrue(TEXT("HUD should hold shared ViewModel instance"), HUD->GetVitalsViewModel() == SharedVM);
	TestTrue(TEXT("Panel should hold shared ViewModel instance"), Panel->GetVitalsViewModel() == SharedVM);

	SharedVM->HidePanel();
	TestEqual(TEXT("Panel should collapse when shared ViewModel hides panel"), Panel->GetVisibility(), ESlateVisibility::Collapsed);

	SharedVM->ShowPanel();
	TestEqual(TEXT("Panel should become visible when shared ViewModel shows panel"), Panel->GetVisibility(), ESlateVisibility::Visible);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
