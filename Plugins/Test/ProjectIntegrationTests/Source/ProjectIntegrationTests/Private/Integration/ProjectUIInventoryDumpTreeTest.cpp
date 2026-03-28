// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "MVVM/InventoryViewModel.h"
#include "Integration/Fixtures/WorldContainerSessionTestDouble.h"
#include "Support/ProjectInventoryReadOnlyMock.h"
#include "Types/ContainerSessionTypes.h"
#include "Widgets/W_InventoryPanel.h"
#include "ProjectWidgetHelpers.h"
#include "ProjectGameplayTags.h"
#include "UObject/UnrealType.h"
#include "Components/Border.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Equipment slot count (must match InventoryViewModel expected slots)
	constexpr int32 EquipSlotCount = 8;

	void SetIntProperty(UObject* Obj, const TCHAR* PropertyName, int32 Value)
	{
		if (!Obj)
		{
			UE_LOG(LogTemp, Warning, TEXT("SetIntProperty: Obj is null for %s"), PropertyName);
			return;
		}

		if (FIntProperty* Prop = FindFProperty<FIntProperty>(Obj->GetClass(), PropertyName))
		{
			Prop->SetPropertyValue_InContainer(Obj, Value);
			UE_LOG(LogTemp, Log, TEXT("SetIntProperty: Set %s = %d"), PropertyName, Value);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SetIntProperty: Property '%s' not found on %s"), PropertyName, *Obj->GetClass()->GetName());
		}
	}

	void SetFloatProperty(UObject* Obj, const TCHAR* PropertyName, float Value)
	{
		if (!Obj)
		{
			return;
		}

		if (FFloatProperty* Prop = FindFProperty<FFloatProperty>(Obj->GetClass(), PropertyName))
		{
			Prop->SetPropertyValue_InContainer(Obj, Value);
		}
	}

	template <typename ElementType>
	void SetArrayProperty(UObject* Obj, const TCHAR* PropertyName, const TArray<ElementType>& Values)
	{
		if (!Obj)
		{
			return;
		}

		if (FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Obj->GetClass(), PropertyName))
		{
			FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Obj));
			Helper.Resize(Values.Num());
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				void* Dest = Helper.GetRawPtr(Index);
				ArrayProp->Inner->CopySingleValue(Dest, &Values[Index]);
			}
		}
	}

	// Validates all equip slot tags are properly registered GameplayTags.
	// Prevents silent bugs when tags are missing from the tag table.
	bool ValidateEquipSlotTags(FAutomationTestBase* Test)
	{
		const TArray<FGameplayTag> ExpectedTags = {
			ProjectTags::Item_EquipmentSlot_MainHand,
			ProjectTags::Item_EquipmentSlot_OffHand,
			ProjectTags::Item_EquipmentSlot_Head,
			ProjectTags::Item_EquipmentSlot_Chest,
			ProjectTags::Item_EquipmentSlot_Back,
			ProjectTags::Item_EquipmentSlot_Legs,
			ProjectTags::Item_EquipmentSlot_Feet,
			ProjectTags::Item_EquipmentSlot_Accessory
		};

		const TArray<FString> TagNames = {
			TEXT("MainHand"), TEXT("OffHand"), TEXT("Head"), TEXT("Chest"),
			TEXT("Back"), TEXT("Legs"), TEXT("Feet"), TEXT("Accessory")
		};

		bool bAllValid = true;
		for (int32 i = 0; i < ExpectedTags.Num(); ++i)
		{
			if (!ExpectedTags[i].IsValid())
			{
				Test->AddError(FString::Printf(TEXT("EquipSlot tag '%s' is not valid - check GameplayTags registration"), *TagNames[i]));
				bAllValid = false;
			}
		}

		if (bAllValid)
		{
			Test->AddInfo(FString::Printf(TEXT("All %d equip slot tags validated successfully"), ExpectedTags.Num()));
		}

		return bAllValid;
	}

	// Naked state: only equip slots + hands, zero storage containers/grids.
	void ConfigureNakedViewModel(UInventoryViewModel* ViewModel)
	{
		if (!ViewModel)
		{
			return;
		}

		ViewModel->ShowPanel();

		// No grids - naked character has no storage containers
		ViewModel->SetGridDimensions(0, 0);
		ViewModel->SetSecondaryGridDimensions(0, 0);
		ViewModel->ClearContainerLabels();

		// Equip slots (always present regardless of gear)
		ViewModel->SetEquipSlotLabels({
			FText::FromString(TEXT("Main Hand")),
			FText::FromString(TEXT("Off Hand")),
			FText::FromString(TEXT("Head")),
			FText::FromString(TEXT("Chest")),
			FText::FromString(TEXT("Back")),
			FText::FromString(TEXT("Legs")),
			FText::FromString(TEXT("Feet")),
			FText::FromString(TEXT("Accessory"))
		});

		ViewModel->SetEquipSlotTags({
			ProjectTags::Item_EquipmentSlot_MainHand,
			ProjectTags::Item_EquipmentSlot_OffHand,
			ProjectTags::Item_EquipmentSlot_Head,
			ProjectTags::Item_EquipmentSlot_Chest,
			ProjectTags::Item_EquipmentSlot_Back,
			ProjectTags::Item_EquipmentSlot_Legs,
			ProjectTags::Item_EquipmentSlot_Feet,
			ProjectTags::Item_EquipmentSlot_Accessory
		});

		ViewModel->SetEquipSlotShortLabels({
			FText::FromString(TEXT("MH")),
			FText::FromString(TEXT("OH")),
			FText::FromString(TEXT("HD")),
			FText::FromString(TEXT("CH")),
			FText::FromString(TEXT("BK")),
			FText::FromString(TEXT("LG")),
			FText::FromString(TEXT("FT")),
			FText::FromString(TEXT("AC"))
		});

		const TArray<FText> EquipItemLabels = {
			FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(),
			FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty()
		};
		SetArrayProperty(ViewModel, TEXT("EquipSlotItemLabels"), EquipItemLabels);

		TArray<int32> EquipInstanceIds;
		EquipInstanceIds.Init(0, EquipSlotCount);
		SetArrayProperty(ViewModel, TEXT("EquipSlotInstanceIds"), EquipInstanceIds);
	}

	void ConfigureInventoryViewModelForLayout(UInventoryViewModel* ViewModel)
	{
		if (!ViewModel)
		{
			return;
		}

		ViewModel->ShowPanel();

		// Primary and secondary grids (layout-only, data is synthetic).
		// NOTE: Must use SetGridDimensions() instead of reflection - VIEWMODEL_PROPERTY
		// generates protected UpdateXxx() but no public setter. See docs/testing/agent_ue_inspection.md.
		ViewModel->SetGridDimensions(6, 6);
		ViewModel->SetSecondaryGridDimensions(2, 2);

		// Basic stats - use defaults (0) since no public setters exist for floats.
		// Grid dimensions are the critical properties for layout testing.

		// Keep synthetic grid dimensions stable:
		// SetSelected/SetSecondary rebuild from CachedContainers, which is empty in this harness
		// (no InventorySource), and would reset GridWidth/GridHeight back to 0.
		SetIntProperty(ViewModel, TEXT("SelectedContainerIndex"), 0);
		SetIntProperty(ViewModel, TEXT("SecondaryContainerIndex"), 1);

		// Tabs.
		ViewModel->SetContainerLabels({
			FText::FromString(TEXT("Hands")),
			FText::FromString(TEXT("Backpack"))
		});

		// Grid texts.
		const int32 PrimaryCellCount = 6 * 6;
		const int32 SecondaryCellCount = 2 * 2;
		TArray<FText> PrimaryTexts;
		PrimaryTexts.Init(FText::GetEmpty(), PrimaryCellCount);
		TArray<FText> SecondaryTexts;
		SecondaryTexts.Init(FText::GetEmpty(), SecondaryCellCount);
		ViewModel->SetCellTexts(PrimaryTexts);
		ViewModel->SetSecondaryCellTexts(SecondaryTexts);

		// Equip slots.
		ViewModel->SetEquipSlotLabels({
			FText::FromString(TEXT("Main Hand")),
			FText::FromString(TEXT("Off Hand")),
			FText::FromString(TEXT("Head")),
			FText::FromString(TEXT("Chest")),
			FText::FromString(TEXT("Back")),
			FText::FromString(TEXT("Legs")),
			FText::FromString(TEXT("Feet")),
			FText::FromString(TEXT("Accessory"))
		});

		// Use direct setters instead of reflection (FGameplayTag doesn't work with CopySingleValue)
		ViewModel->SetEquipSlotTags({
			ProjectTags::Item_EquipmentSlot_MainHand,
			ProjectTags::Item_EquipmentSlot_OffHand,
			ProjectTags::Item_EquipmentSlot_Head,
			ProjectTags::Item_EquipmentSlot_Chest,
			ProjectTags::Item_EquipmentSlot_Back,
			ProjectTags::Item_EquipmentSlot_Legs,
			ProjectTags::Item_EquipmentSlot_Feet,
			ProjectTags::Item_EquipmentSlot_Accessory
		});

		ViewModel->SetEquipSlotShortLabels({
			FText::FromString(TEXT("MH")),
			FText::FromString(TEXT("OH")),
			FText::FromString(TEXT("HD")),
			FText::FromString(TEXT("CH")),
			FText::FromString(TEXT("BK")),
			FText::FromString(TEXT("LG")),
			FText::FromString(TEXT("FT")),
			FText::FromString(TEXT("AC"))
		});

		const TArray<FText> EquipItemLabels = {
			FText::GetEmpty(),
			FText::GetEmpty(),
			FText::GetEmpty(),
			FText::GetEmpty(),
			FText::GetEmpty(),
			FText::GetEmpty(),
			FText::GetEmpty(),
			FText::GetEmpty()
		};
		SetArrayProperty(ViewModel, TEXT("EquipSlotItemLabels"), EquipItemLabels);

		TArray<int32> EquipInstanceIds;
		EquipInstanceIds.Init(0, EquipSlotCount);
		SetArrayProperty(ViewModel, TEXT("EquipSlotInstanceIds"), EquipInstanceIds);
	}

	void ConfigureHandsAndSingleStorageSource(UProjectInventoryReadOnlyMock* Source)
	{
		if (!Source)
		{
			return;
		}

		FInventoryContainerView HandsContainer;
		HandsContainer.ContainerId = ProjectTags::Item_Container_Hands;
		HandsContainer.GridSize = FIntPoint(UInventoryViewModel::HandGridSize, UInventoryViewModel::HandGridSize);

		FInventoryContainerView StorageContainer;
		StorageContainer.ContainerId = ProjectTags::Item_Container_Backpack;
		StorageContainer.GridSize = FIntPoint(6, 6);
		StorageContainer.MaxWeight = 50.f;
		StorageContainer.MaxVolume = 100.f;

		TArray<FInventoryContainerView> Containers;
		Containers.Add(HandsContainer);
		Containers.Add(StorageContainer);

		Source->SetContainers(Containers);
		Source->SetEntries({});
		Source->SetTotals(0.f, 50.f, 0.f, 100.f, 0);
	}

	void ConfigureHandsPocketsAndBackpackSource(UProjectInventoryReadOnlyMock* Source)
	{
		if (!Source)
		{
			return;
		}

		FInventoryContainerView HandsContainer;
		HandsContainer.ContainerId = ProjectTags::Item_Container_Hands;
		HandsContainer.GridSize = FIntPoint(UInventoryViewModel::HandGridSize, UInventoryViewModel::HandGridSize);

		FInventoryContainerView Pockets2Container;
		Pockets2Container.ContainerId = ProjectTags::Item_Container_Pockets2;
		Pockets2Container.GridSize = FIntPoint(2, 2);

		FInventoryContainerView Pockets1Container;
		Pockets1Container.ContainerId = ProjectTags::Item_Container_Pockets1;
		Pockets1Container.GridSize = FIntPoint(2, 2);

		FInventoryContainerView BackpackContainer;
		BackpackContainer.ContainerId = ProjectTags::Item_Container_Backpack;
		BackpackContainer.GridSize = FIntPoint(6, 8);
		BackpackContainer.MaxWeight = 50.f;
		BackpackContainer.MaxVolume = 100.f;

		TArray<FInventoryContainerView> Containers;
		Containers.Add(HandsContainer);
		Containers.Add(Pockets2Container); // Intentionally out of order (should normalize to Pockets1, Pockets2)
		Containers.Add(Pockets1Container);
		Containers.Add(BackpackContainer);

		Source->SetContainers(Containers);
		Source->SetEntries({});
		Source->SetTotals(0.f, 50.f, 0.f, 100.f, 0);
	}

	void ConfigureNearbyLootSource(UWorldContainerSessionTestDouble* Source)
	{
		if (!Source)
		{
			return;
		}

		Source->DisplayLabel = FText::FromString(TEXT("Cardboard Box"));
		Source->ContainerKey.InstanceId = FGuid::NewGuid();
		Source->ContainerKey.WorldScopeId = FName(TEXT("Automation"));
		Source->ContainerKey.ContainerSlotId = FName(TEXT("NearbyLoot"));

		Source->ContainerView.ContainerId = ProjectTags::Item_Container_WorldStorage;
		Source->ContainerView.GridSize = FIntPoint(4, 5);
		Source->ContainerView.MaxWeight = 12.f;
		Source->ContainerView.MaxVolume = 18.f;
		Source->ContainerView.MaxCells = 20;
		Source->ContainerView.CurrentWeight = 0.3f;
		Source->ContainerView.CurrentVolume = 0.3f;

		FInventoryEntryView CigaretteEntry;
		CigaretteEntry.InstanceId = 1001;
		CigaretteEntry.ItemId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
		CigaretteEntry.DisplayName = FText::FromString(TEXT("Cigarette"));
		CigaretteEntry.Description = FText::FromString(TEXT("Filtered cigarette."));
		CigaretteEntry.Quantity = 1;
		CigaretteEntry.ContainerId = ProjectTags::Item_Container_WorldStorage;
		CigaretteEntry.GridPos = FIntPoint(0, 0);
		CigaretteEntry.GridSize = FIntPoint(1, 1);
		CigaretteEntry.Weight = 0.1f;
		CigaretteEntry.Volume = 0.1f;
		CigaretteEntry.IconCode = TEXT("C");

		FInventoryEntryView RagEntry;
		RagEntry.InstanceId = 1002;
		RagEntry.ItemId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Rag"));
		RagEntry.DisplayName = FText::FromString(TEXT("Rag"));
		RagEntry.Description = FText::FromString(TEXT("Torn cloth useful for basic crafting."));
		RagEntry.Quantity = 1;
		RagEntry.ContainerId = ProjectTags::Item_Container_WorldStorage;
		RagEntry.GridPos = FIntPoint(1, 0);
		RagEntry.GridSize = FIntPoint(1, 1);
		RagEntry.Weight = 0.2f;
		RagEntry.Volume = 0.2f;
		RagEntry.IconCode = TEXT("R");

		Source->EntryViews = { CigaretteEntry, RagEntry };
	}

	UWidget* ResolveInventoryLookupRoot(UW_InventoryPanel* Panel)
	{
		if (!Panel)
		{
			return nullptr;
		}

		if (UWidget* RootWidget = Panel->GetRootWidget())
		{
			return RootWidget;
		}

		return Panel;
	}

	int32 CountNonEmptyTextWidgets(UWidget* Root)
	{
		if (!Root)
		{
			return 0;
		}

		int32 Result = 0;
		TArray<UWidget*> Stack;
		Stack.Add(Root);

		while (Stack.Num() > 0)
		{
			UWidget* Widget = Stack.Pop(EAllowShrinking::No);
			if (!Widget)
			{
				continue;
			}

			if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
			{
				if (!TextBlock->GetText().IsEmpty())
				{
					++Result;
				}
			}

			if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
			{
				const int32 ChildCount = PanelWidget->GetChildrenCount();
				for (int32 Index = 0; Index < ChildCount; ++Index)
				{
					if (UWidget* Child = PanelWidget->GetChildAt(Index))
					{
						Stack.Add(Child);
					}
				}
				continue;
			}

			if (UContentWidget* ContentWidget = Cast<UContentWidget>(Widget))
			{
				if (UWidget* Child = ContentWidget->GetContent())
				{
					Stack.Add(Child);
				}
			}
		}

		return Result;
	}

	FContainerSessionHandle MakeNearbyLootSessionHandle(const UWorldContainerSessionTestDouble* Source)
	{
		FContainerSessionHandle Handle;
		if (!Source)
		{
			return Handle;
		}

		Handle.SessionId = FGuid::NewGuid();
		Handle.ContainerKey = Source->ContainerKey;
		Handle.Mode = EContainerSessionMode::FullOpen;
		return Handle;
	}

	void ConfigureVerboseUILayoutLogs(UWorld* World)
	{
		if (!World || !GEngine)
		{
			return;
		}

		GEngine->Exec(World, TEXT("log LogInventoryPanel Verbose"));
		GEngine->Exec(World, TEXT("log LogInventoryVM Verbose"));
		GEngine->Exec(World, TEXT("log LogProjectUserWidget Verbose"));
	}
}

/**
 * Latent command that waits for Slate to complete layout then dumps the widget tree.
 * Arranged sizes are only valid after Slate tick, not just ForceLayoutPrepass.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(
	FDumpInventoryTreeAfterLayout,
	FAutomationTestBase*, Test,
	UProjectUIDebugSubsystem*, DebugSub,
	UUserWidget*, Widget,
	int32, FramesRemaining
);

bool FDumpInventoryTreeAfterLayout::Update()
{
	if (FramesRemaining > 0)
	{
		--FramesRemaining;
		return false;
	}

	if (!Widget || !DebugSub)
	{
		Test->AddError(TEXT("Widget or DebugSubsystem became invalid"));
		return true;
	}

	// Verify grids were populated before dump (catches ViewModel -> UI binding bugs)
	if (UW_InventoryPanel* Panel = Cast<UW_InventoryPanel>(Widget))
	{
		UWidget* LookupRoot = ResolveInventoryLookupRoot(Panel);

		// Check ViewModel state
		if (UInventoryViewModel* VM = Cast<UInventoryViewModel>(Panel->GetViewModel()))
		{
			const int32 GridW = VM->GetGridWidth();
			const int32 GridH = VM->GetGridHeight();
			Test->AddInfo(FString::Printf(TEXT("ViewModel grid dimensions: %dx%d"), GridW, GridH));
			if (GridW <= 0 || GridH <= 0)
			{
				Test->AddError(TEXT("ViewModel grid dimensions are zero - ConfigureInventoryViewModelForLayout failed"));
			}
		}
		else
		{
			Test->AddError(TEXT("ViewModel not set or wrong type"));
		}

		// Check grid host content (UBorder is single-content, use GetContent not GetChildrenCount)
		if (UBorder* GridHost = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(LookupRoot, TEXT("GridHostPrimary")))
		{
			UWidget* Content = GridHost->GetContent();
			if (!Content)
			{
				Test->AddError(TEXT("GridHostPrimary has no content - grid was not populated. Check RebuildGrids() flow."));
			}
			else if (!Cast<UUniformGridPanel>(Content))
			{
				Test->AddError(FString::Printf(TEXT("GridHostPrimary content is %s, expected UUniformGridPanel"), *Content->GetClass()->GetName()));
			}
			else
			{
				Test->AddInfo(TEXT("GridHostPrimary has valid grid panel content"));
			}
		}
		else
		{
			Test->AddWarning(TEXT("GridHostPrimary not found - layout definition may be incorrect"));
		}
	}

	const FString OutPath = TEXT("Dumps/Inventory.json");
	const bool bDumpOk = DebugSub->DumpWidgetTreeEx(OutPath, TEXT("json"), TEXT("Inventory"));
	Test->TestTrue(TEXT("DumpWidgetTreeEx should succeed"), bDumpOk);

	// P2: Capture screenshot alongside JSON dump for agent visual debugging
	const FString ScreenshotPath = TEXT("Dumps/Inventory_screenshot.png");
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
	Test->AddInfo(FString::Printf(TEXT("Screenshot requested: %s"), *ScreenshotPath));

	return true;
}

class FDumpNearbyLootInventoryTree : public IAutomationLatentCommand
{
public:
	FDumpNearbyLootInventoryTree(
		FAutomationTestBase* InTest,
		UProjectUIDebugSubsystem* InDebugSub,
		UW_InventoryPanel* InPanel,
		int32 InFramesRemaining = 2)
		: Test(InTest)
		, DebugSub(InDebugSub)
		, Panel(InPanel)
		, FramesRemaining(InFramesRemaining)
	{
	}

	virtual bool Update() override
	{
		if (FramesRemaining > 0)
		{
			--FramesRemaining;
			return false;
		}

		if (!Panel || !DebugSub)
		{
			Test->AddError(TEXT("Panel or DebugSubsystem became invalid"));
			return true;
		}

		UInventoryViewModel* ViewModel = Cast<UInventoryViewModel>(Panel->GetViewModel());
		if (!Test->TestNotNull(TEXT("Nearby-loot dump should have an inventory view model"), ViewModel))
		{
			return true;
		}

		UWidget* LookupRoot = ResolveInventoryLookupRoot(Panel);

		Test->TestTrue(TEXT("Nearby-loot dump should expose nearby container state"), ViewModel->GetbHasNearbyContainer());
		Test->TestEqual(TEXT("Nearby-loot dump should expose nearby grid width"), ViewModel->GetSecondaryGridWidth(), 4);
		Test->TestEqual(TEXT("Nearby-loot dump should expose nearby grid height"), ViewModel->GetSecondaryGridHeight(), 5);
		int32 NonEmptySecondaryTexts = 0;
		for (const FText& Text : ViewModel->GetSecondaryCellTexts())
		{
			if (!Text.IsEmpty())
			{
				++NonEmptySecondaryTexts;
			}
		}
		Test->TestTrue(TEXT("Nearby-loot dump should expose non-empty secondary cell texts"), NonEmptySecondaryTexts > 0);

		if (UWidget* NearbySection = UProjectWidgetHelpers::FindWidgetByNameTyped<UWidget>(LookupRoot, TEXT("NearbySection")))
		{
			Test->TestTrue(TEXT("NearbySection should be visible"), NearbySection->GetVisibility() != ESlateVisibility::Collapsed);
		}

		if (UBorder* NearbyGridHost = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(LookupRoot, TEXT("NearbyGridHost")))
		{
			UWidget* Content = NearbyGridHost->GetContent();
			Test->TestNotNull(TEXT("NearbyGridHost should contain a populated grid"), Content);
			if (Content)
			{
				UUniformGridPanel* NearbyGrid = Cast<UUniformGridPanel>(Content);
				Test->TestNotNull(TEXT("NearbyGridHost content should be a uniform grid"), NearbyGrid);
				if (NearbyGrid)
				{
					const int32 NonEmptyGridTexts = CountNonEmptyTextWidgets(NearbyGrid);
					Test->TestTrue(TEXT("Nearby grid should render non-empty cell text widgets"), NonEmptyGridTexts > 0);
				}
			}
		}
		else
		{
			Test->AddError(TEXT("NearbyGridHost not found in inventory panel"));
		}

		if (UBorder* LeftHandGridHost = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(LookupRoot, TEXT("LeftHandGridHost")))
		{
			Test->TestNotNull(TEXT("LeftHandGridHost should contain a populated hand grid"), LeftHandGridHost->GetContent());
		}
		else
		{
			Test->AddError(TEXT("LeftHandGridHost not found in inventory panel"));
		}

		if (UVerticalBox* EquipSlotsHost = UProjectWidgetHelpers::FindWidgetByNameTyped<UVerticalBox>(LookupRoot, TEXT("EquipSlotsHost")))
		{
			Test->TestTrue(TEXT("EquipSlotsHost should contain rebuilt slot rows"), EquipSlotsHost->GetChildrenCount() > 0);
		}
		else
		{
			Test->AddError(TEXT("EquipSlotsHost not found in inventory panel"));
		}

		const bool bDumpOk = DebugSub->DumpWidgetTreeEx(TEXT("Dumps/InventoryNearbyLoot.json"), TEXT("json"), TEXT("Inventory"));
		Test->TestTrue(TEXT("Nearby-loot dump should succeed"), bDumpOk);

		FScreenshotRequest::RequestScreenshot(TEXT("Dumps/InventoryNearbyLoot_screenshot.png"), false, false);
		Test->AddInfo(TEXT("Screenshot requested: Dumps/InventoryNearbyLoot_screenshot.png"));
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	UProjectUIDebugSubsystem* DebugSub = nullptr;
	UW_InventoryPanel* Panel = nullptr;
	int32 FramesRemaining = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryDumpTreeTest,
	"ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryDumpTreeTest::RunTest(const FString& Parameters)
{
	// P1: Validate all equip slot tags are registered before proceeding
	if (!ValidateEquipSlotTags(this))
	{
		AddError(TEXT("Equip slot tag validation failed - check GameplayTags.ini"));
		return false;
	}

	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		AddError(TEXT("No game world available - run with -game flag and map specified"));
		return false;
	}

	const FString WorldPackageOriginal = World->GetOutermost()->GetName();
	const FString WorldPackageStripped = UWorld::RemovePIEPrefix(WorldPackageOriginal);
	FString ExpectedMap;
	GConfig->GetString(TEXT("ProjectIntegrationTests"), TEXT("InventoryDumpMap"), ExpectedMap, GGameIni);
	ExpectedMap = ExpectedMap.TrimStartAndEnd();
	if (ExpectedMap.Contains(TEXT(".")))
	{
		ExpectedMap = FPackageName::ObjectPathToPackageName(ExpectedMap);
	}
	if (ExpectedMap.IsEmpty())
	{
		ExpectedMap = TEXT("/MainMenuWorld/Maps/MainMenu_Persistent");
	}

	if (WorldPackageStripped != ExpectedMap)
	{
		AddWarning(FString::Printf(TEXT("Unexpected map: %s (expected %s from ProjectIntegrationTests.InventoryDumpMap)."),
			*WorldPackageStripped, *ExpectedMap));
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		AddError(TEXT("GameInstance is null"));
		return false;
	}

	UProjectUILayerHostSubsystem* LayerHost = GameInstance->GetSubsystem<UProjectUILayerHostSubsystem>();
	if (!TestNotNull(TEXT("ProjectUILayerHostSubsystem should exist"), LayerHost))
	{
		AddError(TEXT("ProjectUILayerHostSubsystem not found"));
		return false;
	}

	UProjectUIDebugSubsystem* DebugSub = GameInstance->GetSubsystem<UProjectUIDebugSubsystem>();
	if (!TestNotNull(TEXT("ProjectUIDebugSubsystem should exist"), DebugSub))
	{
		AddError(TEXT("ProjectUIDebugSubsystem not found"));
		return false;
	}

	ConfigureVerboseUILayoutLogs(World);

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		LayerHost->InitializeForPlayer(PlayerController, true);
	}

	UUserWidget* InventoryWidget = LayerHost->ShowDefinition(TEXT("ProjectInventoryUI.InventoryPanel"));
	if (!InventoryWidget)
	{
		UW_InventoryPanel* InventoryPanel = CreateWidget<UW_InventoryPanel>(GameInstance, UW_InventoryPanel::StaticClass());
		if (!InventoryPanel)
		{
			AddError(TEXT("Inventory panel could not be created from definition or class"));
			return false;
		}
		InventoryPanel->AddToViewport();
		InventoryWidget = InventoryPanel;
	}
	if (!TestNotNull(TEXT("Inventory panel should be created"), InventoryWidget))
	{
		AddError(TEXT("Inventory panel definition not found or failed to create"));
		return false;
	}

	UW_InventoryPanel* InventoryPanel = Cast<UW_InventoryPanel>(InventoryWidget);
	if (!TestNotNull(TEXT("Inventory panel class should be W_InventoryPanel"), InventoryPanel))
	{
		AddError(TEXT("Inventory widget is not W_InventoryPanel"));
		return false;
	}

	UInventoryViewModel* InventoryVM = NewObject<UInventoryViewModel>(GameInstance);
	InventoryVM->Initialize(GameInstance);
	ConfigureInventoryViewModelForLayout(InventoryVM);
	InventoryPanel->SetViewModel(InventoryVM);

	InventoryWidget->SetVisibility(ESlateVisibility::Visible);
	InventoryWidget->ForceLayoutPrepass();

	// Wait 2 frames for Slate to compute arranged sizes, then dump.
	// Frame 1: Slate processes invalidation
	// Frame 2: Layout is finalized
	ADD_LATENT_AUTOMATION_COMMAND(FDumpInventoryTreeAfterLayout(this, DebugSub, InventoryWidget, 2));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryNearbyLootPreboundViewModelTest,
	"ProjectIntegrationTests.UI.Layout.InventoryNearbyLoot.PreboundViewModel",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryNearbyLootPreboundViewModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		AddError(TEXT("No game world available - run with -game flag and map specified"));
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		AddError(TEXT("GameInstance is null"));
		return false;
	}

	UProjectUIDebugSubsystem* DebugSub = GameInstance->GetSubsystem<UProjectUIDebugSubsystem>();
	if (!TestNotNull(TEXT("ProjectUIDebugSubsystem should exist"), DebugSub))
	{
		return false;
	}

	ConfigureVerboseUILayoutLogs(World);
	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	UW_InventoryPanel* InventoryPanel = CreateWidget<UW_InventoryPanel>(GameInstance, UW_InventoryPanel::StaticClass());
	if (!TestNotNull(TEXT("Inventory panel should be created"), InventoryPanel))
	{
		return false;
	}

	UProjectInventoryReadOnlyMock* InventorySource = NewObject<UProjectInventoryReadOnlyMock>(GameInstance);
	if (!TestNotNull(TEXT("Mock inventory source should be created"), InventorySource))
	{
		return false;
	}

	ConfigureHandsPocketsAndBackpackSource(InventorySource);

	AActor* NearbyLootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Nearby loot actor should be created"), NearbyLootActor))
	{
		return false;
	}

	UWorldContainerSessionTestDouble* NearbyLootSource = NewObject<UWorldContainerSessionTestDouble>(NearbyLootActor);
	if (!TestNotNull(TEXT("Nearby loot session source should be created"), NearbyLootSource))
	{
		NearbyLootActor->Destroy();
		return false;
	}

	NearbyLootActor->AddInstanceComponent(NearbyLootSource);
	NearbyLootSource->RegisterComponent();
	ConfigureNearbyLootSource(NearbyLootSource);

	UInventoryViewModel* InventoryVM = NewObject<UInventoryViewModel>(GameInstance);
	if (!TestNotNull(TEXT("Inventory view model should be created"), InventoryVM))
	{
		NearbyLootActor->Destroy();
		return false;
	}

	InventoryVM->Initialize(GameInstance);
	InventoryVM->SetInventorySource(InventorySource);
	InventoryVM->SetNearbyContainerSource(NearbyLootSource, MakeNearbyLootSessionHandle(NearbyLootSource));
	InventoryVM->ShowPanel();

	// Reproduce the live runtime order: the shared ViewModel already exists and is
	// populated before the inventory panel is constructed and added to the viewport.
	InventoryPanel->SetViewModel(InventoryVM);
	InventoryPanel->AddToViewport();
	InventoryPanel->SetVisibility(ESlateVisibility::Visible);
	InventoryPanel->ForceLayoutPrepass();

	ADD_LATENT_AUTOMATION_COMMAND(FDumpNearbyLootInventoryTree(this, DebugSub, InventoryPanel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryNearbyLootDumpTreeTest,
	"ProjectIntegrationTests.UI.Layout.InventoryNearbyLoot.DumpTree",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryNearbyLootDumpTreeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		AddError(TEXT("No game world available - run with -game flag and map specified"));
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		AddError(TEXT("GameInstance is null"));
		return false;
	}

	UProjectUILayerHostSubsystem* LayerHost = GameInstance->GetSubsystem<UProjectUILayerHostSubsystem>();
	UProjectUIDebugSubsystem* DebugSub = GameInstance->GetSubsystem<UProjectUIDebugSubsystem>();
	if (!TestNotNull(TEXT("ProjectUILayerHostSubsystem should exist"), LayerHost)
		|| !TestNotNull(TEXT("ProjectUIDebugSubsystem should exist"), DebugSub))
	{
		return false;
	}

	ConfigureVerboseUILayoutLogs(World);
	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		LayerHost->InitializeForPlayer(PlayerController, true);
	}

	UUserWidget* InventoryWidget = LayerHost->ShowDefinition(TEXT("ProjectInventoryUI.InventoryPanel"));
	if (!InventoryWidget)
	{
		UW_InventoryPanel* InventoryPanel = CreateWidget<UW_InventoryPanel>(GameInstance, UW_InventoryPanel::StaticClass());
		if (!InventoryPanel)
		{
			AddError(TEXT("Inventory panel could not be created from definition or class"));
			return false;
		}
		InventoryPanel->AddToViewport();
		InventoryWidget = InventoryPanel;
	}

	UW_InventoryPanel* InventoryPanel = Cast<UW_InventoryPanel>(InventoryWidget);
	if (!TestNotNull(TEXT("Inventory widget should be W_InventoryPanel"), InventoryPanel))
	{
		return false;
	}

	UProjectInventoryReadOnlyMock* InventorySource = NewObject<UProjectInventoryReadOnlyMock>(GameInstance);
	if (!TestNotNull(TEXT("Mock inventory source should be created"), InventorySource))
	{
		return false;
	}

	ConfigureHandsPocketsAndBackpackSource(InventorySource);

	AActor* NearbyLootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Nearby loot actor should be created"), NearbyLootActor))
	{
		return false;
	}

	UWorldContainerSessionTestDouble* NearbyLootSource = NewObject<UWorldContainerSessionTestDouble>(NearbyLootActor);
	if (!TestNotNull(TEXT("Nearby loot session source should be created"), NearbyLootSource))
	{
		NearbyLootActor->Destroy();
		return false;
	}

	NearbyLootActor->AddInstanceComponent(NearbyLootSource);
	NearbyLootSource->RegisterComponent();
	ConfigureNearbyLootSource(NearbyLootSource);

	UInventoryViewModel* InventoryVM = NewObject<UInventoryViewModel>(GameInstance);
	if (!TestNotNull(TEXT("Inventory view model should be created"), InventoryVM))
	{
		NearbyLootActor->Destroy();
		return false;
	}

	InventoryVM->Initialize(GameInstance);
	InventoryVM->SetInventorySource(InventorySource);
	InventoryVM->SetNearbyContainerSource(NearbyLootSource, MakeNearbyLootSessionHandle(NearbyLootSource));
	InventoryVM->ShowPanel();
	InventoryPanel->SetViewModel(InventoryVM);

	InventoryWidget->SetVisibility(ESlateVisibility::Visible);
	InventoryWidget->ForceLayoutPrepass();

	ADD_LATENT_AUTOMATION_COMMAND(FDumpNearbyLootInventoryTree(this, DebugSub, InventoryPanel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryHandsSingleStorageIndexingTest,
	"ProjectIntegrationTests.UI.Layout.InventoryHands.SingleStorageIndexing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryHandsSingleStorageIndexingTest::RunTest(const FString& Parameters)
{
	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage(), UInventoryViewModel::StaticClass());
	if (!TestNotNull(TEXT("ViewModel should be created"), ViewModel))
	{
		return false;
	}

	UProjectInventoryReadOnlyMock* Source = NewObject<UProjectInventoryReadOnlyMock>(GetTransientPackage(), UProjectInventoryReadOnlyMock::StaticClass());
	if (!TestNotNull(TEXT("Mock inventory source should be created"), Source))
	{
		return false;
	}

	ConfigureHandsAndSingleStorageSource(Source);
	ViewModel->SetInventorySource(Source);

	TestEqual(TEXT("Only storage containers should appear as tabs"), ViewModel->GetContainerLabels().Num(), 1);
	TestEqual(TEXT("Primary storage index should normalize to 0"), ViewModel->GetSelectedContainerIndex(), 0);
	TestEqual(TEXT("Secondary storage index should normalize to INDEX_NONE with one storage container"), ViewModel->GetSecondaryContainerIndex(), INDEX_NONE);
	TestEqual(TEXT("Primary grid width should come from storage container"), ViewModel->GetGridWidth(), 6);
	TestEqual(TEXT("Primary grid height should come from storage container"), ViewModel->GetGridHeight(), 6);
	TestEqual(TEXT("Secondary grid width should be 0 when no second storage container"), ViewModel->GetSecondaryGridWidth(), 0);
	TestEqual(TEXT("Secondary grid height should be 0 when no second storage container"), ViewModel->GetSecondaryGridHeight(), 0);
	TestTrue(TEXT("Selected container id should be backpack"), ViewModel->GetSelectedContainerId().MatchesTagExact(ProjectTags::Item_Container_Backpack));
	TestFalse(TEXT("Secondary container id should be invalid"), ViewModel->GetSecondaryContainerId().IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryHandsPocketOrderingTest,
	"ProjectIntegrationTests.UI.Layout.InventoryHands.PocketOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryHandsPocketOrderingTest::RunTest(const FString& Parameters)
{
	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage(), UInventoryViewModel::StaticClass());
	if (!TestNotNull(TEXT("ViewModel should be created"), ViewModel))
	{
		return false;
	}

	UProjectInventoryReadOnlyMock* Source = NewObject<UProjectInventoryReadOnlyMock>(GetTransientPackage(), UProjectInventoryReadOnlyMock::StaticClass());
	if (!TestNotNull(TEXT("Mock inventory source should be created"), Source))
	{
		return false;
	}

	ConfigureHandsPocketsAndBackpackSource(Source);
	ViewModel->SetInventorySource(Source);

	TestEqual(TEXT("Pocket container count should be 2"), ViewModel->GetPocketContainerCount(), 2);
	TestTrue(TEXT("Pocket 0 should normalize to Pockets1"), ViewModel->GetPocketContainerId(0).MatchesTagExact(ProjectTags::Item_Container_Pockets1));
	TestTrue(TEXT("Pocket 1 should normalize to Pockets2"), ViewModel->GetPocketContainerId(1).MatchesTagExact(ProjectTags::Item_Container_Pockets2));

	const FString PocketLabel0 = ViewModel->GetPocketContainerLabel(0).ToString();
	const FString PocketLabel1 = ViewModel->GetPocketContainerLabel(1).ToString();
	TestEqual(TEXT("Pocket label 0"), PocketLabel0, FString(TEXT("Pockets1")));
	TestEqual(TEXT("Pocket label 1"), PocketLabel1, FString(TEXT("Pockets2")));

	TestEqual(TEXT("Lower storage tabs should only include backpack"), ViewModel->GetContainerLabels().Num(), 1);
	TestTrue(TEXT("Primary large container should be backpack"), ViewModel->GetSelectedContainerId().MatchesTagExact(ProjectTags::Item_Container_Backpack));
	TestEqual(TEXT("Backpack grid width should be selected in lower grid"), ViewModel->GetGridWidth(), 6);
	TestEqual(TEXT("Backpack grid height should be selected in lower grid"), ViewModel->GetGridHeight(), 8);

	return true;
}

/**
 * Tier 1 Test: Multi-resolution layout dump.
 * Runs the inventory dump at multiple resolutions to catch resolution-specific bugs.
 * Agent can compare issues across resolutions to find responsive layout problems.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryMultiResTest,
	"ProjectIntegrationTests.UI.Layout.InventoryHands.MultiResolution",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

/**
 * Latent command to change resolution, wait, and dump.
 * Uses instance state (not static) to handle sequential execution correctly.
 */
class FDumpAtResolution : public IAutomationLatentCommand
{
public:
	FDumpAtResolution(
		FAutomationTestBase* InTest,
		UProjectUIDebugSubsystem* InDebugSub,
		UUserWidget* InWidget,
		FIntPoint InResolution,
		const FString& InOutputFileName)
		: Test(InTest)
		, DebugSub(InDebugSub)
		, Widget(InWidget)
		, Resolution(InResolution)
		, OutputFileName(InOutputFileName)
		, FrameCount(0)
		, bResolutionSet(false)
	{
	}

	virtual bool Update() override
	{
		if (!bResolutionSet)
		{
			// Set viewport resolution via console command (windowed to avoid affecting display)
			if (GEngine)
			{
				FString Cmd = FString::Printf(TEXT("r.SetRes %dx%dw"), Resolution.X, Resolution.Y);
				GEngine->Exec(nullptr, *Cmd);
			}
			bResolutionSet = true;
			FrameCount = 0;
			return false;
		}

		// Wait 3 frames for layout to stabilize at new resolution
		if (FrameCount < 3)
		{
			++FrameCount;
			return false;
		}

		if (!Widget || !DebugSub)
		{
			Test->AddError(TEXT("Widget or DebugSubsystem became invalid"));
			return true;
		}

		Widget->ForceLayoutPrepass();

		const bool bDumpOk = DebugSub->DumpWidgetTreeEx(OutputFileName, TEXT("json"), TEXT("Inventory"));
		Test->TestTrue(FString::Printf(TEXT("DumpWidgetTreeEx at %dx%d should succeed"), Resolution.X, Resolution.Y), bDumpOk);

		// P2: Capture screenshot at each resolution for agent visual debugging
		const FString ScreenshotName = OutputFileName.Replace(TEXT(".json"), TEXT("_screenshot.png"));
		FScreenshotRequest::RequestScreenshot(ScreenshotName, false, false);
		Test->AddInfo(FString::Printf(TEXT("Screenshot requested at %dx%d: %s"), Resolution.X, Resolution.Y, *ScreenshotName));

		return true;
	}

private:
	FAutomationTestBase* Test;
	UProjectUIDebugSubsystem* DebugSub;
	UUserWidget* Widget;
	FIntPoint Resolution;
	FString OutputFileName;
	int32 FrameCount;
	bool bResolutionSet;
};

bool FProjectUIInventoryMultiResTest::RunTest(const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		AddError(TEXT("No game world available - run with -game flag and map specified"));
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		return false;
	}

	UProjectUILayerHostSubsystem* LayerHost = GameInstance->GetSubsystem<UProjectUILayerHostSubsystem>();
	UProjectUIDebugSubsystem* DebugSub = GameInstance->GetSubsystem<UProjectUIDebugSubsystem>();
	if (!LayerHost || !DebugSub)
	{
		AddError(TEXT("Required subsystems not found"));
		return false;
	}

	ConfigureVerboseUILayoutLogs(World);

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		LayerHost->InitializeForPlayer(PlayerController, true);
	}

	UUserWidget* InventoryWidget = LayerHost->ShowDefinition(TEXT("ProjectInventoryUI.InventoryPanel"));
	if (!InventoryWidget)
	{
		UW_InventoryPanel* InventoryPanel = CreateWidget<UW_InventoryPanel>(GameInstance, UW_InventoryPanel::StaticClass());
		if (!InventoryPanel)
		{
			AddError(TEXT("Inventory panel could not be created"));
			return false;
		}
		InventoryPanel->AddToViewport();
		InventoryWidget = InventoryPanel;
	}

	UW_InventoryPanel* InventoryPanel = Cast<UW_InventoryPanel>(InventoryWidget);
	if (!InventoryPanel)
	{
		AddError(TEXT("Inventory widget is not W_InventoryPanel"));
		return false;
	}

	UInventoryViewModel* InventoryVM = NewObject<UInventoryViewModel>(GameInstance);
	InventoryVM->Initialize(GameInstance);
	ConfigureInventoryViewModelForLayout(InventoryVM);
	InventoryPanel->SetViewModel(InventoryVM);
	InventoryWidget->SetVisibility(ESlateVisibility::Visible);

	// Tier 1 resolutions: common displays + ultrawide
	struct FResolutionConfig
	{
		FIntPoint Size;
		FString Name;
	};
	const TArray<FResolutionConfig> Resolutions = {
		{ FIntPoint(1280, 720),  TEXT("720p") },
		{ FIntPoint(1920, 1080), TEXT("1080p") },
		{ FIntPoint(2560, 1440), TEXT("1440p") },
		{ FIntPoint(3440, 1440), TEXT("Ultrawide") }
	};

	for (const FResolutionConfig& Res : Resolutions)
	{
		FString OutputFile = FString::Printf(TEXT("Dumps/Inventory_%s.json"), *Res.Name);
		FAutomationTestFramework::Get().EnqueueLatentCommand(
			MakeShared<FDumpAtResolution>(this, DebugSub, InventoryWidget, Res.Size, OutputFile));
	}

	// Restore default resolution after all dumps (prevents messing up display)
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<FDumpAtResolution>(this, DebugSub, InventoryWidget,
			FIntPoint(1920, 1080), TEXT("Dumps/Inventory_Restore.json")));

	return true;
}

/**
 * Naked-state dump test: ViewModel with 0 containers, 0 grids.
 * Validates that GridRow/ContainerTabs collapse and EmptyStoragePlaceholder shows.
 */
class FDumpNakedInventoryTree : public IAutomationLatentCommand
{
public:
	FDumpNakedInventoryTree(
		FAutomationTestBase* InTest,
		UProjectUIDebugSubsystem* InDebugSub,
		UW_InventoryPanel* InPanel,
		int32 InFrames = 2)
		: Test(InTest), DebugSub(InDebugSub), Panel(InPanel), FramesRemaining(InFrames) {}

	virtual bool Update() override
	{
		if (FramesRemaining > 0) { --FramesRemaining; return false; }
		if (!Panel || !DebugSub)
		{
			Test->AddError(TEXT("Panel or DebugSub became invalid"));
			return true;
		}

		// Validate naked state: grids should have zero dimensions
		if (UInventoryViewModel* VM = Cast<UInventoryViewModel>(Panel->GetViewModel()))
		{
			Test->TestEqual(TEXT("GridWidth should be 0"), VM->GetGridWidth(), 0);
			Test->TestEqual(TEXT("GridHeight should be 0"), VM->GetGridHeight(), 0);
			Test->TestEqual(TEXT("SecondaryGridWidth should be 0"), VM->GetSecondaryGridWidth(), 0);
		}

		// Validate widget visibility
		if (UWidget* GridRow = UProjectWidgetHelpers::FindWidgetByNameTyped<UWidget>(Panel, TEXT("GridRow")))
		{
			Test->TestEqual(TEXT("GridRow should be Collapsed"), GridRow->GetVisibility(), ESlateVisibility::Collapsed);
		}
		if (UWidget* Placeholder = UProjectWidgetHelpers::FindWidgetByNameTyped<UWidget>(Panel, TEXT("EmptyStoragePlaceholder")))
		{
			Test->TestTrue(TEXT("EmptyStoragePlaceholder should be visible"),
				Placeholder->GetVisibility() != ESlateVisibility::Collapsed);
		}

		const bool bOk = DebugSub->DumpWidgetTreeEx(TEXT("Dumps/InventoryNaked.json"), TEXT("json"), TEXT("Inventory"));
		Test->TestTrue(TEXT("Naked dump should succeed"), bOk);
		return true;
	}

private:
	FAutomationTestBase* Test;
	UProjectUIDebugSubsystem* DebugSub;
	UW_InventoryPanel* Panel;
	int32 FramesRemaining;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryNakedDumpTest,
	"ProjectIntegrationTests.UI.Layout.InventoryNaked.DumpTree",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryNakedDumpTest::RunTest(const FString& Parameters)
{
	if (!ValidateEquipSlotTags(this))
	{
		AddError(TEXT("Equip slot tag validation failed"));
		return false;
	}

	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!TestNotNull(TEXT("World"), World)) { return false; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance"), GameInstance)) { return false; }

	UProjectUILayerHostSubsystem* LayerHost = GameInstance->GetSubsystem<UProjectUILayerHostSubsystem>();
	UProjectUIDebugSubsystem* DebugSub = GameInstance->GetSubsystem<UProjectUIDebugSubsystem>();
	if (!LayerHost || !DebugSub)
	{
		AddError(TEXT("Required subsystems not found"));
		return false;
	}

	ConfigureVerboseUILayoutLogs(World);

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		LayerHost->InitializeForPlayer(PC, true);
	}

	UUserWidget* Widget = LayerHost->ShowDefinition(TEXT("ProjectInventoryUI.InventoryPanel"));
	if (!Widget)
	{
		UW_InventoryPanel* Panel = CreateWidget<UW_InventoryPanel>(GameInstance, UW_InventoryPanel::StaticClass());
		if (!Panel) { AddError(TEXT("Could not create inventory panel")); return false; }
		Panel->AddToViewport();
		Widget = Panel;
	}

	UW_InventoryPanel* Panel = Cast<UW_InventoryPanel>(Widget);
	if (!TestNotNull(TEXT("Panel cast"), Panel)) { return false; }

	// Naked state: no storage, only equip slots
	UInventoryViewModel* VM = NewObject<UInventoryViewModel>(GameInstance);
	VM->Initialize(GameInstance);
	ConfigureNakedViewModel(VM);
	Panel->SetViewModel(VM);

	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->ForceLayoutPrepass();

	ADD_LATENT_AUTOMATION_COMMAND(FDumpNakedInventoryTree(this, DebugSub, Panel));
	return true;
}

#endif
