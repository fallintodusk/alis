// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "UI/InventoryUIVisibilityCoordinator.h"
#include "Engine/LocalPlayer.h"
#include "Components/ProjectInventoryComponent.h"
#include "MVVM/InventoryViewModel.h"
#include "Integration/Fixtures/WorldContainerSessionTestDouble.h"
#include "Support/ProjectInventoryReadOnlyMock.h"
#include "Services/ObjectDefinitionCache.h"
#include "Types/ContainerSessionTypes.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Widgets/W_InventoryPanel.h"
#include "Widgets/W_NearbyContainerPanel.h"
#include "ProjectWidgetHelpers.h"
#include "ProjectGameplayTags.h"
#include "UObject/UnrealType.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"
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

	TSharedPtr<FJsonObject> BuildCellVisualArrayJson(const TArray<FInventoryCellVisualState>& Visuals)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Cells;
		Cells.Reserve(Visuals.Num());

		for (int32 Index = 0; Index < Visuals.Num(); ++Index)
		{
			const FInventoryCellVisualState& Visual = Visuals[Index];
			TSharedPtr<FJsonObject> CellObject = MakeShared<FJsonObject>();
			CellObject->SetNumberField(TEXT("index"), Index);
			CellObject->SetNumberField(TEXT("instance_id"), Visual.InstanceId);
			CellObject->SetStringField(TEXT("primary_text"), Visual.PrimaryText.ToString());
			CellObject->SetStringField(TEXT("quantity_text"), Visual.QuantityText.ToString());
			CellObject->SetBoolField(TEXT("use_icon_font"), Visual.bUseIconFont);
			CellObject->SetBoolField(TEXT("show_quantity"), Visual.bShowQuantity);
			CellObject->SetBoolField(TEXT("is_anchor_cell"), Visual.bIsAnchorCell);
			Cells.Add(MakeShared<FJsonValueObject>(CellObject));
		}

		Result->SetArrayField(TEXT("cells"), Cells);
		return Result;
	}

	FString LexToString(EObjectDefinitionLoadState State)
	{
		switch (State)
		{
		case EObjectDefinitionLoadState::Loaded:
			return TEXT("Loaded");
		case EObjectDefinitionLoadState::Loading:
			return TEXT("Loading");
		case EObjectDefinitionLoadState::Missing:
		default:
			return TEXT("Missing");
		}
	}

	bool FindAnchorVisualForInstance(
		const UInventoryViewModel* ViewModel,
		int32 InstanceId,
		FInventoryCellVisualState& OutVisual,
		FString& OutSurface)
	{
		if (!ViewModel || InstanceId == INDEX_NONE)
		{
			return false;
		}

		auto TryArray = [&OutVisual, &OutSurface, InstanceId](
			const TArray<FInventoryCellVisualState>& Visuals,
			const TCHAR* SurfaceLabel) -> bool
		{
			for (const FInventoryCellVisualState& Visual : Visuals)
			{
				if (Visual.InstanceId == InstanceId && Visual.bIsAnchorCell)
				{
					OutVisual = Visual;
					OutSurface = SurfaceLabel;
					return true;
				}
			}
			return false;
		};

		if (TryArray(ViewModel->GetCellVisuals(), TEXT("primary_grid"))
			|| TryArray(ViewModel->GetSecondaryCellVisuals(), TEXT("secondary_grid"))
			|| TryArray(ViewModel->GetLeftHandCellVisuals(), TEXT("left_hand"))
			|| TryArray(ViewModel->GetRightHandCellVisuals(), TEXT("right_hand")))
		{
			return true;
		}

		for (int32 PocketIndex = 0; PocketIndex < ViewModel->GetPocketContainerCount(); ++PocketIndex)
		{
			const FString PocketSurface = FString::Printf(TEXT("pocket_%d"), PocketIndex);
			if (TryArray(ViewModel->GetPocketCellVisuals(PocketIndex), *PocketSurface))
			{
				return true;
			}
		}

		return false;
	}

	TSharedPtr<FJsonObject> BuildEntryJson(
		const UInventoryViewModel* ViewModel,
		const FInventoryEntryView& Entry,
		const TCHAR* LoadState)
	{
		TSharedPtr<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("item_id"), Entry.ItemId.ToString());
		EntryObject->SetNumberField(TEXT("instance_id"), Entry.InstanceId);
		EntryObject->SetStringField(TEXT("display_name"), Entry.DisplayName.ToString());
		EntryObject->SetStringField(TEXT("icon_code"), Entry.IconCode);
		EntryObject->SetNumberField(TEXT("quantity"), Entry.Quantity);
		EntryObject->SetStringField(TEXT("container_id"), Entry.ContainerId.ToString());
		EntryObject->SetNumberField(TEXT("grid_x"), Entry.GridPos.X);
		EntryObject->SetNumberField(TEXT("grid_y"), Entry.GridPos.Y);
		EntryObject->SetNumberField(TEXT("grid_w"), Entry.GridSize.X);
		EntryObject->SetNumberField(TEXT("grid_h"), Entry.GridSize.Y);
		EntryObject->SetBoolField(TEXT("action_caps_populated"), Entry.bActionCapsPopulated);
		EntryObject->SetBoolField(TEXT("can_use"), Entry.bCanUse);
		EntryObject->SetBoolField(TEXT("can_equip"), Entry.bCanEquip);
		EntryObject->SetBoolField(TEXT("can_drop"), Entry.bCanBeDropped);
		EntryObject->SetStringField(TEXT("load_state"), LoadState);

		FInventoryCellVisualState AnchorVisual;
		FString Surface;
		const bool bHasAnchorVisual = FindAnchorVisualForInstance(ViewModel, Entry.InstanceId, AnchorVisual, Surface);

		TSharedPtr<FJsonObject> DisplayObject = MakeShared<FJsonObject>();
		DisplayObject->SetBoolField(TEXT("has_anchor_visual"), bHasAnchorVisual);
		DisplayObject->SetStringField(TEXT("surface"), Surface);
		DisplayObject->SetStringField(TEXT("primary_text"), bHasAnchorVisual ? AnchorVisual.PrimaryText.ToString() : FString());
		DisplayObject->SetStringField(TEXT("quantity_text"), bHasAnchorVisual ? AnchorVisual.QuantityText.ToString() : FString());
		DisplayObject->SetBoolField(TEXT("use_icon_font"), bHasAnchorVisual && AnchorVisual.bUseIconFont);
		DisplayObject->SetBoolField(TEXT("show_quantity"), bHasAnchorVisual && AnchorVisual.bShowQuantity);
		DisplayObject->SetBoolField(TEXT("is_anchor_cell"), bHasAnchorVisual && AnchorVisual.bIsAnchorCell);
		EntryObject->SetObjectField(TEXT("display_payload"), DisplayObject);

		return EntryObject;
	}

	void AddEntryArrayJson(
		TSharedPtr<FJsonObject> RootObject,
		const TCHAR* FieldName,
		const UInventoryViewModel* ViewModel,
		const TArray<FInventoryEntryView>& Entries,
		const TCHAR* LoadState)
	{
		TArray<TSharedPtr<FJsonValue>> EntryArray;
		EntryArray.Reserve(Entries.Num());

		for (const FInventoryEntryView& Entry : Entries)
		{
			EntryArray.Add(MakeShared<FJsonValueObject>(BuildEntryJson(ViewModel, Entry, LoadState)));
		}

		RootObject->SetArrayField(FieldName, EntryArray);
	}

	void AddDefinitionCacheDiagnosticsJson(TSharedPtr<FJsonObject> RootObject, const UInventoryViewModel* ViewModel)
	{
		TArray<TSharedPtr<FJsonValue>> CacheArray;
		if (const UProjectInventoryComponent* InventoryComponent = Cast<UProjectInventoryComponent>(ViewModel ? ViewModel->GetInventorySourceObjectForDiagnostics() : nullptr))
		{
			if (const UObjectDefinitionCache* Cache = InventoryComponent->GetObjectDefinitionCache())
			{
				TArray<FObjectDefinitionCacheEntryDiagnostic> Diagnostics;
				Cache->GetDiagnostics(Diagnostics);
				CacheArray.Reserve(Diagnostics.Num());

				for (const FObjectDefinitionCacheEntryDiagnostic& Diagnostic : Diagnostics)
				{
					TSharedPtr<FJsonObject> CacheObject = MakeShared<FJsonObject>();
					CacheObject->SetStringField(TEXT("object_id"), Diagnostic.ObjectId.ToString());
					CacheObject->SetStringField(TEXT("state"), LexToString(Diagnostic.State));
					CacheObject->SetBoolField(TEXT("has_resolved_object"), Diagnostic.bHasResolvedObject);
					CacheObject->SetBoolField(TEXT("has_resident_handle"), Diagnostic.bHasResidentHandle);
					CacheObject->SetBoolField(TEXT("has_pending_load"), Diagnostic.bHasPendingLoad);
					CacheArray.Add(MakeShared<FJsonValueObject>(CacheObject));
				}
			}
		}

		RootObject->SetArrayField(TEXT("definition_cache"), CacheArray);
	}

	bool WriteInventoryDiagnosticsDump(const FString& RelativeOutPath, UInventoryViewModel* ViewModel)
	{
		if (!ViewModel)
		{
			return false;
		}

		TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetNumberField(TEXT("grid_width"), ViewModel->GetGridWidth());
		RootObject->SetNumberField(TEXT("grid_height"), ViewModel->GetGridHeight());
		RootObject->SetNumberField(TEXT("secondary_grid_width"), ViewModel->GetSecondaryGridWidth());
		RootObject->SetNumberField(TEXT("secondary_grid_height"), ViewModel->GetSecondaryGridHeight());
		RootObject->SetBoolField(TEXT("has_nearby_container"), ViewModel->GetbHasNearbyContainer());
		RootObject->SetObjectField(TEXT("primary_grid"), BuildCellVisualArrayJson(ViewModel->GetCellVisuals()));
		RootObject->SetObjectField(TEXT("secondary_grid"), BuildCellVisualArrayJson(ViewModel->GetSecondaryCellVisuals()));
		RootObject->SetObjectField(TEXT("left_hand"), BuildCellVisualArrayJson(ViewModel->GetLeftHandCellVisuals()));
		RootObject->SetObjectField(TEXT("right_hand"), BuildCellVisualArrayJson(ViewModel->GetRightHandCellVisuals()));
		AddEntryArrayJson(RootObject, TEXT("inventory_entries"), ViewModel, ViewModel->GetCachedEntriesForDiagnostics(), TEXT("Loaded"));
		AddEntryArrayJson(RootObject, TEXT("nearby_entries"), ViewModel, ViewModel->GetCachedNearbyEntriesForDiagnostics(), TEXT("Loaded"));
		AddDefinitionCacheDiagnosticsJson(RootObject, ViewModel);

		TArray<TSharedPtr<FJsonValue>> PocketArray;
		for (int32 PocketIndex = 0; PocketIndex < ViewModel->GetPocketContainerCount(); ++PocketIndex)
		{
			TSharedPtr<FJsonObject> PocketObject = MakeShared<FJsonObject>();
			PocketObject->SetNumberField(TEXT("pocket_index"), PocketIndex);
			PocketObject->SetStringField(TEXT("container_id"), ViewModel->GetPocketContainerId(PocketIndex).ToString());
			PocketObject->SetObjectField(TEXT("visuals"), BuildCellVisualArrayJson(ViewModel->GetPocketCellVisuals(PocketIndex)));
			PocketArray.Add(MakeShared<FJsonValueObject>(PocketObject));
		}
		RootObject->SetArrayField(TEXT("pockets"), PocketArray);

		FString JsonString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
		{
			return false;
		}

		const FString AbsolutePath = FPaths::ProjectSavedDir() / RelativeOutPath;
		return FFileHelper::SaveStringToFile(JsonString, *AbsolutePath);
	}

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

	UWorld* ResolveInventoryAutomationWorld(FAutomationTestBase* Test)
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (World)
		{
			return World;
		}

		if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
		{
			if (Test)
			{
				Test->AddError(TEXT("Failed to open MainMenu_Persistent for inventory UI automation"));
			}
			return nullptr;
		}

		return AutomationCommon::GetAnyGameWorld();
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

		// Grid visuals.
		const int32 PrimaryCellCount = 6 * 6;
		const int32 SecondaryCellCount = 2 * 2;
		TArray<FInventoryCellVisualState> PrimaryVisuals;
		PrimaryVisuals.SetNum(PrimaryCellCount);
		TArray<FInventoryCellVisualState> SecondaryVisuals;
		SecondaryVisuals.SetNum(SecondaryCellCount);
		ViewModel->SetCellVisuals(PrimaryVisuals);
		ViewModel->SetSecondaryCellVisuals(SecondaryVisuals);

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
				continue;
			}

			// UUserWidget roots its content through the WidgetTree, not
			// through a UPanelWidget / UContentWidget child list. After
			// Slice 17 every inventory grid cell is wrapped in a
			// UW_InventoryCellDropTarget, so walkers must descend into
			// its tree root to reach the hosted UProjectGridCell visuals.
			if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
			{
				if (UserWidget->WidgetTree)
				{
					if (UWidget* Root2 = UserWidget->WidgetTree->RootWidget)
					{
						Stack.Add(Root2);
					}
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

		// Post-Slice 6b/e: NearbySection / NearbyGridHost must not live
		// inside UW_InventoryPanel even in the hands scenario. This prevents
		// regressions where the nearby subtree is reintroduced into the
		// main panel under any VM state.
		Test->TestNull(TEXT("NearbySection must not live inside main panel (hands scenario)"),
			UProjectWidgetHelpers::FindWidgetByNameTyped<UWidget>(LookupRoot, TEXT("NearbySection")));
		Test->TestNull(TEXT("NearbyGridHost must not live inside main panel (hands scenario)"),
			UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(LookupRoot, TEXT("NearbyGridHost")));

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

	if (UW_InventoryPanel* Panel = Cast<UW_InventoryPanel>(Widget))
	{
		if (UInventoryViewModel* VM = Cast<UInventoryViewModel>(Panel->GetViewModel()))
		{
			Test->TestTrue(
				TEXT("Inventory diagnostics dump should succeed"),
				WriteInventoryDiagnosticsDump(TEXT("Dumps/Inventory_state.json"), VM));
		}
	}

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
		int32 NonEmptySecondaryVisuals = 0;
		for (const FInventoryCellVisualState& Visual : ViewModel->GetSecondaryCellVisuals())
		{
			if (!Visual.IsEmpty())
			{
				++NonEmptySecondaryVisuals;
			}
		}
		Test->TestTrue(TEXT("Nearby-loot dump should expose non-empty secondary cell visuals"), NonEmptySecondaryVisuals > 0);

		// Architecture post-decouple: NearbySection / NearbyGridHost live in
		// UW_NearbyContainerPanel (a sibling widget under UI.Layer.Menu), NOT
		// inside UW_InventoryPanel. Assert the absence here to catch
		// regressions where the nearby subtree is reintroduced into the main
		// panel, then locate the nearby widget separately.
		Test->TestNull(TEXT("NearbySection must not live inside the main inventory panel after decouple"),
			UProjectWidgetHelpers::FindWidgetByNameTyped<UWidget>(LookupRoot, TEXT("NearbySection")));
		Test->TestNull(TEXT("NearbyGridHost must not live inside the main inventory panel after decouple"),
			UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(LookupRoot, TEXT("NearbyGridHost")));

		TArray<UUserWidget*> NearbyWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
			Panel->GetWorld(), NearbyWidgets, UW_NearbyContainerPanel::StaticClass(), false);
		Test->TestTrue(TEXT("UW_NearbyContainerPanel must exist in the viewport during a nearby-loot session"),
			NearbyWidgets.Num() > 0);
		if (NearbyWidgets.Num() > 0)
		{
			UW_NearbyContainerPanel* NearbyPanel = Cast<UW_NearbyContainerPanel>(NearbyWidgets[0]);
			UWidget* NearbyRoot = NearbyPanel ? NearbyPanel->GetRootWidget() : nullptr;
			Test->TestNotNull(TEXT("Nearby widget root must be constructed"), NearbyRoot);

			if (NearbyRoot)
			{
				Test->TestNotEqual(TEXT("Nearby widget must be visible during an active nearby session"),
					NearbyPanel->GetVisibility(), ESlateVisibility::Collapsed);

				if (UBorder* NearbyGridHost = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(NearbyRoot, TEXT("NearbyGridHost")))
				{
					UWidget* Content = NearbyGridHost->GetContent();
					Test->TestNotNull(TEXT("NearbyGridHost should contain a populated grid"), Content);
					if (UUniformGridPanel* NearbyGrid = Cast<UUniformGridPanel>(Content))
					{
						const int32 NonEmptyGridTexts = CountNonEmptyTextWidgets(NearbyGrid);
						Test->TestTrue(TEXT("Nearby grid should render non-empty cell text widgets"), NonEmptyGridTexts > 0);
					}
				}
				else
				{
					Test->AddError(TEXT("NearbyGridHost not found inside UW_NearbyContainerPanel"));
				}
			}
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
		Test->TestTrue(
			TEXT("Nearby-loot diagnostics dump should succeed"),
			WriteInventoryDiagnosticsDump(TEXT("Dumps/InventoryNearbyLoot_state.json"), ViewModel));

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
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryDumpTreeTest::RunTest(const FString& Parameters)
{
	// P1: Validate all equip slot tags are registered before proceeding
	if (!ValidateEquipSlotTags(this))
	{
		AddError(TEXT("Equip slot tag validation failed - check GameplayTags.ini"));
		return false;
	}

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		AddError(TEXT("No automation world available for inventory hands dump"));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUINearbyContainerPanelRefreshesOnViewModelChangeTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelRefreshesOnViewModelChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUINearbyContainerPanelRefreshesOnViewModelChangeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Reviewer's watch item: W_NearbyContainerPanel uses a dynamic-delegate
	// binding to UInventoryViewModel::OnPropertyChanged via a dedicated
	// UFUNCTION (HandleInventoryVMPropertyChanged). Prove the binding
	// path actually fires on real VM mutations by driving:
	//   1. no session     -> panel collapsed
	//   2. attach session -> panel visible, non-empty title
	//   3. clear session  -> panel collapsed again
	//   4. re-attach      -> panel visible again
	//
	// No forced SetVisibility, no CreateWidget fallback. Visibility must
	// be derived from VM state by the widget's own refresh path.

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World)) { return false; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance)) { return false; }

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	UW_NearbyContainerPanel* NearbyPanel = CreateWidget<UW_NearbyContainerPanel>(GameInstance, UW_NearbyContainerPanel::StaticClass());
	if (!TestNotNull(TEXT("Nearby container panel should construct"), NearbyPanel)) { return false; }
	NearbyPanel->AddToViewport();

	UProjectInventoryReadOnlyMock* InventorySource = NewObject<UProjectInventoryReadOnlyMock>(GameInstance);
	if (!TestNotNull(TEXT("Mock inventory source should construct"), InventorySource)) { return false; }
	ConfigureHandsPocketsAndBackpackSource(InventorySource);

	AActor* NearbyLootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Nearby loot actor should spawn"), NearbyLootActor)) { return false; }
	UWorldContainerSessionTestDouble* NearbyLootSource = NewObject<UWorldContainerSessionTestDouble>(NearbyLootActor);
	if (!TestNotNull(TEXT("Nearby loot source should construct"), NearbyLootSource))
	{
		NearbyLootActor->Destroy();
		return false;
	}
	NearbyLootActor->AddInstanceComponent(NearbyLootSource);
	NearbyLootSource->RegisterComponent();
	ConfigureNearbyLootSource(NearbyLootSource);

	UInventoryViewModel* InventoryVM = NewObject<UInventoryViewModel>(GameInstance);
	InventoryVM->Initialize(GameInstance);
	InventoryVM->SetInventorySource(InventorySource);
	InventoryVM->ShowPanel();

	NearbyPanel->SetViewModel(InventoryVM);
	NearbyPanel->ForceLayoutPrepass();

	// Step 1: no nearby session yet - panel must be collapsed.
	TestEqual(
		TEXT("Panel is Collapsed before a nearby session is attached"),
		NearbyPanel->GetVisibility(),
		ESlateVisibility::Collapsed);

	// Step 2: attach a nearby session - visibility must flip to non-Collapsed
	// via the VM property-change delegate, with no manual SetVisibility.
	InventoryVM->SetNearbyContainerSource(NearbyLootSource, MakeNearbyLootSessionHandle(NearbyLootSource));
	NearbyPanel->ForceLayoutPrepass();
	TestNotEqual(
		TEXT("Attaching a nearby session must make the panel visible via VM property change"),
		NearbyPanel->GetVisibility(),
		ESlateVisibility::Collapsed);

	// Title should reflect the label carried by the source (or fallback).
	if (UTextBlock* TitleText = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(NearbyPanel->GetRootWidget(), TEXT("NearbyTitleText")))
	{
		const FText CurrentTitle = TitleText->GetText();
		TestTrue(TEXT("Title text must be non-empty while a nearby session is active"), !CurrentTitle.IsEmpty());
	}

	// Step 3: close the session - panel must collapse again.
	InventoryVM->ClearNearbyContainerSource();
	NearbyPanel->ForceLayoutPrepass();
	TestEqual(
		TEXT("Clearing the nearby source must collapse the panel"),
		NearbyPanel->GetVisibility(),
		ESlateVisibility::Collapsed);

	// Step 4: re-open. Visibility must come back; proves the binding isn't
	// a one-shot and survives source teardown + re-attach.
	InventoryVM->SetNearbyContainerSource(NearbyLootSource, MakeNearbyLootSessionHandle(NearbyLootSource));
	NearbyPanel->ForceLayoutPrepass();
	TestNotEqual(
		TEXT("Re-attaching a nearby session must make the panel visible again"),
		NearbyPanel->GetVisibility(),
		ESlateVisibility::Collapsed);

	NearbyLootActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUINearbyPanelIsIndependentlyAnchoredTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelIsIndependentlyAnchored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUINearbyPanelIsIndependentlyAnchoredTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Post-decouple contract: opening / closing a nearby session must NOT
	// reflow the main inventory panel. Before the decouple, the main panel
	// auto-sized itself to include the nearby subtree, causing a visible
	// "jump" when loot sessions opened. After Slice 6b, nearby lives in
	// its own widget under the same layer host and the main panel's width
	// is constant. This test proves that invariant at the widget level:
	// BackgroundWidthSizer->GetWidthOverride() must be stable across
	// nearby-session attach / detach.

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World)) { return false; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance)) { return false; }

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	UW_InventoryPanel* InventoryPanel = CreateWidget<UW_InventoryPanel>(GameInstance, UW_InventoryPanel::StaticClass());
	if (!TestNotNull(TEXT("Main inventory panel should construct"), InventoryPanel)) { return false; }
	InventoryPanel->AddToViewport();

	UProjectInventoryReadOnlyMock* InventorySource = NewObject<UProjectInventoryReadOnlyMock>(GameInstance);
	if (!TestNotNull(TEXT("Mock inventory source should construct"), InventorySource)) { return false; }
	ConfigureHandsPocketsAndBackpackSource(InventorySource);

	AActor* NearbyLootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Nearby loot actor should spawn"), NearbyLootActor)) { return false; }
	UWorldContainerSessionTestDouble* NearbyLootSource = NewObject<UWorldContainerSessionTestDouble>(NearbyLootActor);
	if (!TestNotNull(TEXT("Nearby loot source should construct"), NearbyLootSource))
	{
		NearbyLootActor->Destroy();
		return false;
	}
	NearbyLootActor->AddInstanceComponent(NearbyLootSource);
	NearbyLootSource->RegisterComponent();
	ConfigureNearbyLootSource(NearbyLootSource);

	UInventoryViewModel* InventoryVM = NewObject<UInventoryViewModel>(GameInstance);
	InventoryVM->Initialize(GameInstance);
	InventoryVM->SetInventorySource(InventorySource);
	InventoryVM->ShowPanel();
	InventoryPanel->SetViewModel(InventoryVM);
	InventoryPanel->ForceLayoutPrepass();

	// Measure the main panel desired width. Before Slice 6b the main panel
	// auto-sized around the nearby subtree. Post-decouple the nearby
	// subtree lives outside the main panel, so desired width must be
	// stable across nearby-session toggles regardless of whether a named
	// SizeBox carries the width override.
	UWidget* Root = InventoryPanel->GetRootWidget();
	if (!TestNotNull(TEXT("Main panel must have a root widget"), Root))
	{
		NearbyLootActor->Destroy();
		return false;
	}

	auto MeasureDesiredWidth = [InventoryPanel, Root]()
	{
		InventoryPanel->ForceLayoutPrepass();
		return Root->GetDesiredSize().X;
	};

	const float WidthWithoutNearby = MeasureDesiredWidth();
	TestTrue(TEXT("Main panel should have a non-zero desired width before nearby session"),
		WidthWithoutNearby > 0.f);

	// Attach nearby session; the main panel width must be unchanged.
	InventoryVM->SetNearbyContainerSource(NearbyLootSource, MakeNearbyLootSessionHandle(NearbyLootSource));
	const float WidthWithNearby = MeasureDesiredWidth();
	TestEqual(
		TEXT("Main panel width must not reflow when a nearby session opens"),
		WidthWithNearby,
		WidthWithoutNearby);

	// Close the session; width must still be unchanged.
	InventoryVM->ClearNearbyContainerSource();
	const float WidthAfterClose = MeasureDesiredWidth();
	TestEqual(
		TEXT("Main panel width must not reflow when a nearby session closes"),
		WidthAfterClose,
		WidthWithoutNearby);

	NearbyLootActor->Destroy();
	return true;
}

// Latent command that waits for Slate to paint the nearby grid and then
// exercises the two controller APIs at a screen position inside an
// occupied cell. This is the real-geometry regression guard for:
//   UW_NearbyContainerPanel::NativeOnDragDetected must use the source-
//   hit API, not the drop-validation API, so an occupied cell (which
//   every drag source is) can still start a drag.
class FTestNearbyDragStartOnOccupiedCell : public IAutomationLatentCommand
{
public:
	FTestNearbyDragStartOnOccupiedCell(
		FAutomationTestBase* InTest,
		UW_NearbyContainerPanel* InPanel,
		AActor* InLootActor,
		int32 InFramesRemaining = 5)
		: Test(InTest)
		, Panel(InPanel)
		, LootActor(InLootActor)
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

		AActor* LootActorPtr = LootActor.Get();
		ON_SCOPE_EXIT
		{
			if (LootActorPtr)
			{
				LootActorPtr->Destroy();
			}
		};

		if (!Panel.IsValid())
		{
			Test->AddError(TEXT("Nearby panel disappeared before drag-start test could run"));
			return true;
		}

		APlayerController* PC = Panel->GetOwningPlayer();
		if (!Test->TestNotNull(TEXT("Nearby panel should have an owning player"), PC))
		{
			return true;
		}
		ULocalPlayer* LP = PC->GetLocalPlayer();
		if (!Test->TestNotNull(TEXT("Owning player should be local"), LP))
		{
			return true;
		}
		UInventoryUIDragHostSubsystem* Subsystem = LP->GetSubsystem<UInventoryUIDragHostSubsystem>();
		if (!Test->TestNotNull(TEXT("Drag-host subsystem should be resolvable from local player"), Subsystem))
		{
			return true;
		}

		const FGameplayTag& WorldStorageTag = ProjectTags::Item_Container_WorldStorage;

		// Nearby session is live, so the panel must have registered its
		// WorldStorage surface with the subsystem.
		Test->TestTrue(TEXT("WorldStorage surface should be registered during a live nearby session"),
			Subsystem->HasSurface(WorldStorageTag));

		UWidget* Root = Panel->GetRootWidget();
		if (!Test->TestNotNull(TEXT("Nearby panel root should exist"), Root))
		{
			return true;
		}
		UUniformGridPanel* NearbyGrid = nullptr;
		if (UBorder* GridHost = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(Root, TEXT("NearbyGridHost")))
		{
			NearbyGrid = Cast<UUniformGridPanel>(GridHost->GetContent());
		}
		if (!Test->TestNotNull(TEXT("Nearby grid panel should be content of NearbyGridHost"), NearbyGrid))
		{
			return true;
		}

		const FGeometry Geometry = NearbyGrid->GetCachedGeometry();
		const FVector2D GridLocalSize = Geometry.GetLocalSize();
		if (GridLocalSize.X <= 0.f || GridLocalSize.Y <= 0.f)
		{
			Test->AddWarning(TEXT("Nearby grid has no cached geometry; Slate likely did not paint in the test harness. Skipping real-geometry assertions."));
			return true;
		}

		// Cigarette occupies cell (0,0) in the fixture (InstanceId=1001).
		// Compute an absolute screen position at the CENTER of cell (0,0).
		const FVector2D CellStride(
			GridLocalSize.X / 4.0f,  // ContainerView.GridSize.X = 4
			GridLocalSize.Y / 5.0f); // ContainerView.GridSize.Y = 5
		const FVector2D OccupiedLocalCenter(CellStride.X * 0.5f, CellStride.Y * 0.5f);
		const FVector2D OccupiedAbsolutePos = Geometry.LocalToAbsolute(OccupiedLocalCenter);

		// Source hit test: must succeed even though cell is occupied.
		// This is the behavior NativeOnDragDetected now depends on.
		FGameplayTag SourceTag;
		int32 SourceCol = INDEX_NONE;
		int32 SourceRow = INDEX_NONE;
		const bool bSourceHit = Subsystem->GetController().ResolveSurfaceCellAtScreenPos(
			OccupiedAbsolutePos, SourceTag, SourceCol, SourceRow);
		Test->TestTrue(TEXT("Source hit-test must succeed on an occupied nearby cell"), bSourceHit);
		Test->TestEqual(TEXT("Source hit tag is WorldStorage"), SourceTag, WorldStorageTag);
		Test->TestEqual(TEXT("Source hit resolves to cell (0,0)"), SourceCol, 0);
		Test->TestEqual(TEXT("Source hit resolves to cell (0,0) row"), SourceRow, 0);

		// Drop validation with the OLD probe-payload approach (InstanceId=INDEX_NONE,
		// ItemSize=(1,1)) must now REJECT the occupied cell. This is exactly
		// why the source-hit API exists: if drag-start used this path, picking
		// up the cigarette would fail every time. Locking that rejection in
		// catches regressions where someone rewires NativeOnDragDetected
		// back onto the drop API.
		FProjectUIGridDragPayload Probe;
		Probe.InstanceId = INDEX_NONE;
		Probe.ItemSize = FIntPoint(1, 1);
		FGameplayTag DropTag;
		int32 DropCol = INDEX_NONE;
		int32 DropRow = INDEX_NONE;
		const bool bDropHitProbe = Subsystem->GetController().ResolveDropTargetOverSurfaces(
			OccupiedAbsolutePos, Probe, DropTag, DropCol, DropRow);
		Test->TestFalse(
			TEXT("Drop validation with a probe payload must REJECT an occupied cell. "
			     "If this becomes true, the two APIs have been conflated again - drag-start would start rejecting real users."),
			bDropHitProbe);

		// Drop validation with the real dragged instance id (simulating the
		// same item dropped back on its source cell) should succeed, proving
		// the drop API does work when the payload matches the occupant.
		Probe.InstanceId = 1001;
		const bool bDropHitSelf = Subsystem->GetController().ResolveDropTargetOverSurfaces(
			OccupiedAbsolutePos, Probe, DropTag, DropCol, DropRow);
		Test->TestTrue(
			TEXT("Drop validation should accept the cell when payload is the cell's own occupant (self-drop)"),
			bDropHitSelf);

		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	TWeakObjectPtr<UW_NearbyContainerPanel> Panel;
	TWeakObjectPtr<AActor> LootActor;
	int32 FramesRemaining = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUINearbyDragStartWorksOnOccupiedCellTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.NearbyDragStartWorksOnOccupiedCell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUINearbyDragStartWorksOnOccupiedCellTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Real-geometry regression guard for the source-hit vs drop-validation
	// contract. A 4x5 nearby grid is populated with an item at cell (0,0)
	// via the shared fixture (Cigarette, InstanceId=1001). The latent
	// command below waits for Slate to paint and then runs both controller
	// APIs at the center of cell (0,0). See the latent command body for
	// the specific assertions.

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World)) { return false; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance)) { return false; }

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	UW_NearbyContainerPanel* NearbyPanel = CreateWidget<UW_NearbyContainerPanel>(
		GameInstance, UW_NearbyContainerPanel::StaticClass());
	if (!TestNotNull(TEXT("Nearby container panel should construct"), NearbyPanel)) { return false; }
	NearbyPanel->AddToViewport();

	UProjectInventoryReadOnlyMock* InventorySource = NewObject<UProjectInventoryReadOnlyMock>(GameInstance);
	if (!TestNotNull(TEXT("Mock inventory source should construct"), InventorySource)) { return false; }
	ConfigureHandsPocketsAndBackpackSource(InventorySource);

	AActor* NearbyLootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Nearby loot actor should spawn"), NearbyLootActor)) { return false; }
	UWorldContainerSessionTestDouble* NearbyLootSource = NewObject<UWorldContainerSessionTestDouble>(NearbyLootActor);
	if (!TestNotNull(TEXT("Nearby loot source should construct"), NearbyLootSource))
	{
		NearbyLootActor->Destroy();
		return false;
	}
	NearbyLootActor->AddInstanceComponent(NearbyLootSource);
	NearbyLootSource->RegisterComponent();
	ConfigureNearbyLootSource(NearbyLootSource);

	UInventoryViewModel* InventoryVM = NewObject<UInventoryViewModel>(GameInstance);
	InventoryVM->Initialize(GameInstance);
	InventoryVM->SetInventorySource(InventorySource);
	InventoryVM->SetNearbyContainerSource(NearbyLootSource, MakeNearbyLootSessionHandle(NearbyLootSource));
	InventoryVM->ShowPanel();
	NearbyPanel->SetViewModel(InventoryVM);
	NearbyPanel->ForceLayoutPrepass();

	// Five-frame wait lets Slate paint so the grid's CachedGeometry is
	// non-zero. Shorter waits were flaky in earlier dump-tree tests.
	ADD_LATENT_AUTOMATION_COMMAND(FTestNearbyDragStartOnOccupiedCell(this, NearbyPanel, NearbyLootActor, 5));
	return true;
}

// Latent command for the equip-backpack regression. Asserts that with a
// real W_InventoryPanel + populated subsystem surface, dropping on an
// EMPTY primary backpack cell with a real payload SUCCEEDS. Pre-fix this
// failed because the OccupantAllowedChecker reached to GetDragDroppingContent
// and rejected every empty cell when that returned null.
class FTestBackpackEmptyCellDropAccepts : public IAutomationLatentCommand
{
public:
	FTestBackpackEmptyCellDropAccepts(
		FAutomationTestBase* InTest,
		UW_InventoryPanel* InPanel,
		int32 InFramesRemaining = 5)
		: Test(InTest)
		, Panel(InPanel)
		, FramesRemaining(InFramesRemaining)
	{
	}

	virtual bool Update() override
	{
		if (FramesRemaining > 0) { --FramesRemaining; return false; }
		if (!Panel.IsValid())
		{
			Test->AddError(TEXT("Inventory panel disappeared before backpack drop test could run"));
			return true;
		}

		APlayerController* PC = Panel->GetOwningPlayer();
		if (!Test->TestNotNull(TEXT("Inventory panel must have an owning player"), PC)) { return true; }
		ULocalPlayer* LP = PC->GetLocalPlayer();
		if (!Test->TestNotNull(TEXT("Owning player must be local"), LP)) { return true; }
		UInventoryUIDragHostSubsystem* Subsystem = LP->GetSubsystem<UInventoryUIDragHostSubsystem>();
		if (!Test->TestNotNull(TEXT("Drag host subsystem must be reachable"), Subsystem)) { return true; }

		const FGameplayTag& BackpackTag = ProjectTags::Item_Container_Backpack;
		Test->TestTrue(TEXT("Backpack surface must be registered after equip"),
			Subsystem->HasSurface(BackpackTag));

		UWidget* Root = Panel->GetRootWidget();
		if (!Test->TestNotNull(TEXT("Inventory panel root must exist"), Root)) { return true; }
		UBorder* GridHost = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(Root, TEXT("GridHostPrimary"));
		if (!Test->TestNotNull(TEXT("GridHostPrimary must exist"), GridHost)) { return true; }
		UUniformGridPanel* PrimaryGrid = Cast<UUniformGridPanel>(GridHost->GetContent());
		if (!Test->TestNotNull(TEXT("Primary grid must be present"), PrimaryGrid)) { return true; }

		const FGeometry Geometry = PrimaryGrid->GetCachedGeometry();
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f)
		{
			Test->AddWarning(TEXT("Primary backpack grid has no cached geometry; Slate did not paint. Skipping real-geometry assertions."));
			return true;
		}

		// Backpack fixture is 6x8, all cells empty. Drop on cell (1,1) which
		// is empty - this must succeed even with a degenerate payload (mirroring
		// the lifecycle-edge case where DragOp is null in older code paths).
		const FVector2D CellStride(LocalSize.X / 6.0f, LocalSize.Y / 8.0f);
		const FVector2D LocalCenter(CellStride.X * 1.5f, CellStride.Y * 1.5f);
		const FVector2D AbsolutePos = Geometry.LocalToAbsolute(LocalCenter);

		// Build a payload mirroring what NativeOnDrop populates today.
		FProjectUIGridDragPayload Payload;
		Payload.InstanceId = 9999;            // synthetic source instance
		Payload.ItemSize = FIntPoint(1, 1);
		Payload.Quantity = 1;
		Payload.SourceSurfaceTag = ProjectTags::Item_Container_Pockets1;

		FGameplayTag OutTag;
		int32 OutCol = INDEX_NONE;
		int32 OutRow = INDEX_NONE;
		const bool bResolved = Subsystem->GetController().ResolveDropTargetOverSurfaces(
			AbsolutePos, Payload, OutTag, OutCol, OutRow);

		Test->TestTrue(
			TEXT("Drop on EMPTY backpack cell (1,1) must succeed with full payload. "
			     "If false, OccupantAllowedChecker is rejecting empty cells - "
			     "the equip-backpack 'unavailable cell' regression is back."),
			bResolved);
		Test->TestEqual(TEXT("Resolved tag is Backpack"), OutTag, BackpackTag);
		Test->TestEqual(TEXT("Resolved col"), OutCol, 1);
		Test->TestEqual(TEXT("Resolved row"), OutRow, 1);

		// Also exercise the degenerate payload path - if a frame ever arrives
		// without full source context, empty cells must STILL accept.
		FProjectUIGridDragPayload DegenPayload;
		DegenPayload.ItemSize = FIntPoint(1, 1);
		FGameplayTag DegenTag;
		int32 DegenCol = INDEX_NONE;
		int32 DegenRow = INDEX_NONE;
		const bool bDegenResolved = Subsystem->GetController().ResolveDropTargetOverSurfaces(
			AbsolutePos, DegenPayload, DegenTag, DegenCol, DegenRow);
		Test->TestTrue(TEXT("Degenerate payload still resolves on empty cell (defensive)"), bDegenResolved);
		Test->TestEqual(TEXT("Degenerate tag is Backpack"), DegenTag, BackpackTag);
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	TWeakObjectPtr<UW_InventoryPanel> Panel;
	int32 FramesRemaining = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIBackpackEmptyCellDropAcceptsTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.BackpackEmptyCellDropAccepts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIBackpackEmptyCellDropAcceptsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Reproduces the equip-backpack regression at integration level: a live
	// W_InventoryPanel with the Backpack registered as a tabbed primary
	// surface, then exercises the controller's drop-resolution path at an
	// EMPTY cell. The empty cell must accept the drop. Pre-fix the lambda
	// reached to UWidgetBlueprintLibrary::GetDragDroppingContent() and a
	// null return from there made every empty cell appear "unavailable".

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World)) { return false; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance)) { return false; }

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	UW_InventoryPanel* InventoryPanel = CreateWidget<UW_InventoryPanel>(
		GameInstance, UW_InventoryPanel::StaticClass());
	if (!TestNotNull(TEXT("Inventory panel should construct"), InventoryPanel)) { return false; }
	InventoryPanel->AddToViewport();

	UProjectInventoryReadOnlyMock* InventorySource = NewObject<UProjectInventoryReadOnlyMock>(GameInstance);
	if (!TestNotNull(TEXT("Mock inventory source"), InventorySource)) { return false; }
	ConfigureHandsPocketsAndBackpackSource(InventorySource);

	UInventoryViewModel* InventoryVM = NewObject<UInventoryViewModel>(GameInstance);
	InventoryVM->Initialize(GameInstance);
	InventoryVM->SetInventorySource(InventorySource);
	InventoryVM->ShowPanel();
	InventoryPanel->SetViewModel(InventoryVM);
	InventoryPanel->ForceLayoutPrepass();

	ADD_LATENT_AUTOMATION_COMMAND(FTestBackpackEmptyCellDropAccepts(this, InventoryPanel, 5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUINearbyCellsHaveMouseDownHandlerTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.NearbyCellsHaveMouseDownHandler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUINearbyCellsHaveMouseDownHandlerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Regression: with the user-widget root SelfHitTestInvisible (so empty
	// canvas area passes events through to the sibling W_InventoryPanel),
	// the ONLY pathway for cell clicks to reach the widget is the per-cell
	// mouse-down handler bound via FInventoryPanelGridBuilder. If
	// RebuildGrid forgets to bind it, clicking a nearby cell does NOTHING
	// (no logs, no drag) - exactly the symptom that broke drag-from-nearby
	// after the SelfHitTestInvisible flip.
	//
	// This test attaches a real nearby session, forces layout, and asserts
	// every built cell has its GridMouseDownHandler bound. If you ever
	// remove the SetCellMouseDownHandler call from
	// W_NearbyContainerPanel::NativeConstruct OR forget to set
	// SetIsGridCell on the cells, this test fires.

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance"), GameInstance)) { return false; }

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));
	UW_NearbyContainerPanel* Panel = CreateWidget<UW_NearbyContainerPanel>(
		GameInstance, UW_NearbyContainerPanel::StaticClass());
	if (!TestNotNull(TEXT("Nearby panel"), Panel)) { return false; }
	Panel->AddToViewport();

	UProjectInventoryReadOnlyMock* InventorySource = NewObject<UProjectInventoryReadOnlyMock>(GameInstance);
	if (!TestNotNull(TEXT("Mock inventory"), InventorySource)) { return false; }
	ConfigureHandsPocketsAndBackpackSource(InventorySource);

	AActor* NearbyLootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Loot actor"), NearbyLootActor)) { return false; }
	UWorldContainerSessionTestDouble* NearbyLootSource = NewObject<UWorldContainerSessionTestDouble>(NearbyLootActor);
	if (!TestNotNull(TEXT("Loot source"), NearbyLootSource))
	{
		NearbyLootActor->Destroy();
		return false;
	}
	NearbyLootActor->AddInstanceComponent(NearbyLootSource);
	NearbyLootSource->RegisterComponent();
	ConfigureNearbyLootSource(NearbyLootSource);

	UInventoryViewModel* VM = NewObject<UInventoryViewModel>(GameInstance);
	VM->Initialize(GameInstance);
	VM->SetInventorySource(InventorySource);
	VM->SetNearbyContainerSource(NearbyLootSource, MakeNearbyLootSessionHandle(NearbyLootSource));
	VM->ShowPanel();
	Panel->SetViewModel(VM);
	Panel->ForceLayoutPrepass();

	UWidget* Root = Panel->GetRootWidget();
	if (!TestNotNull(TEXT("Panel root"), Root)) { NearbyLootActor->Destroy(); return false; }
	UBorder* GridHost = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(Root, TEXT("NearbyGridHost"));
	if (!TestNotNull(TEXT("NearbyGridHost"), GridHost)) { NearbyLootActor->Destroy(); return false; }
	UUniformGridPanel* Grid = Cast<UUniformGridPanel>(GridHost->GetContent());
	if (!TestNotNull(TEXT("Nearby grid panel"), Grid)) { NearbyLootActor->Destroy(); return false; }

	const int32 SlotCount = Grid->GetChildrenCount();
	if (!TestTrue(TEXT("Nearby grid must have at least one cell-slot after RebuildGrid"), SlotCount > 0))
	{
		NearbyLootActor->Destroy();
		return false;
	}

	// Slice 17: GridBuilder wraps each ProjectGridCell in a
	// UW_InventoryCellDropTarget (a UUserWidget), which itself sits
	// inside a SizeBox. The traversal chain is:
	//   UniformGridSlot -> SizeBox -> UW_InventoryCellDropTarget -> UProjectGridCell
	// (via the wrapper's WidgetTree->RootWidget). Older code expected
	// UProjectGridCell directly under SizeBox; after Slice 17 we descend
	// one more level through the wrapper when present.
	int32 ResolvedCells = 0;
	int32 BoundCells = 0;
	int32 GridCells = 0;
	for (int32 i = 0; i < SlotCount; ++i)
	{
		USizeBox* SlotBox = Cast<USizeBox>(Grid->GetChildAt(i));
		if (!SlotBox) { continue; }
		UProjectGridCell* Cell = Cast<UProjectGridCell>(SlotBox->GetContent());
		if (!Cell)
		{
			if (UUserWidget* CellHost = Cast<UUserWidget>(SlotBox->GetContent()))
			{
				if (CellHost->WidgetTree)
				{
					Cell = Cast<UProjectGridCell>(CellHost->WidgetTree->RootWidget);
				}
			}
		}
		if (!Cell) { continue; }
		++ResolvedCells;
		if (Cell->IsGridCell()) { ++GridCells; }
		if (Cell->IsGridMouseDownHandlerBound()) { ++BoundCells; }
	}

	TestTrue(TEXT("Cell traversal should resolve at least one ProjectGridCell"), ResolvedCells > 0);
	TestEqual(TEXT("All resolved cells should be marked as grid cells (SetIsGridCell)"),
		GridCells, ResolvedCells);
	TestEqual(
		TEXT("EVERY nearby grid cell MUST have a bound GridMouseDownHandler. "
		     "If 0 (or fewer than total), clicks on cells silently do nothing - "
		     "exactly the symptom of forgetting SetCellMouseDownHandler in NativeConstruct."),
		BoundCells, ResolvedCells);

	NearbyLootActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUINearbyPanelRootIsHitTransparentTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelRootIsHitTransparent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUINearbyPanelRootIsHitTransparentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Regression: NearbyContainerPanel.json declares RootCanvas with
	// anchor=Fill (full viewport). When the panel is Visible, it
	// intercepts every drag/drop event before they reach the sibling
	// W_InventoryPanel - even though the actual nearby content is just a
	// small CenterRight border. Net effect: opening a loot container
	// breaks all drag/drop in the main inventory panel because the nearby
	// widget's NativeOnDragOver/Drop run first (return true or return
	// false-but-Slate-doesn't-bubble-cross-sibling) and the main panel
	// never sees the event.
	//
	// Fix: the user-widget root MUST be SelfHitTestInvisible when shown.
	// Children (NearbyGridHost cells, TakeAllButton) stay interactive;
	// empty canvas area passes events through to the sibling.

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World)) { return false; }
	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance"), GameInstance)) { return false; }

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));
	UW_NearbyContainerPanel* Panel = CreateWidget<UW_NearbyContainerPanel>(
		GameInstance, UW_NearbyContainerPanel::StaticClass());
	if (!TestNotNull(TEXT("Nearby panel"), Panel)) { return false; }
	Panel->AddToViewport();

	UProjectInventoryReadOnlyMock* InventorySource = NewObject<UProjectInventoryReadOnlyMock>(GameInstance);
	if (!TestNotNull(TEXT("Mock inventory"), InventorySource)) { return false; }
	ConfigureHandsPocketsAndBackpackSource(InventorySource);

	AActor* NearbyLootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Loot actor"), NearbyLootActor)) { return false; }
	UWorldContainerSessionTestDouble* NearbyLootSource = NewObject<UWorldContainerSessionTestDouble>(NearbyLootActor);
	if (!TestNotNull(TEXT("Loot source"), NearbyLootSource))
	{
		NearbyLootActor->Destroy();
		return false;
	}
	NearbyLootActor->AddInstanceComponent(NearbyLootSource);
	NearbyLootSource->RegisterComponent();
	ConfigureNearbyLootSource(NearbyLootSource);

	UInventoryViewModel* VM = NewObject<UInventoryViewModel>(GameInstance);
	VM->Initialize(GameInstance);
	VM->SetInventorySource(InventorySource);
	VM->SetNearbyContainerSource(NearbyLootSource, MakeNearbyLootSessionHandle(NearbyLootSource));
	VM->ShowPanel();
	Panel->SetViewModel(VM);
	Panel->ForceLayoutPrepass();

	TestEqual(
		TEXT("Nearby panel root MUST be SelfHitTestInvisible when shown - "
		     "Visible would intercept sibling main-panel drag/drop events because "
		     "RootCanvas anchors to Fill."),
		Panel->GetVisibility(),
		ESlateVisibility::SelfHitTestInvisible);

	// Hide path still collapses (no event interception possible when
	// Collapsed; this is the close-loot-session state).
	VM->ClearNearbyContainerSource();
	Panel->ForceLayoutPrepass();
	TestEqual(
		TEXT("Nearby panel collapses when nearby session ends"),
		Panel->GetVisibility(),
		ESlateVisibility::Collapsed);

	NearbyLootActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUINearbyPanelAnchoredCenterRightTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelAnchoredCenterRight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUINearbyPanelAnchoredCenterRightTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Regression: NearbyContainerPanel.json used "RightCenter" as the
	// anchor preset name, but the registry exposes "CenterRight". The
	// silent fallback dropped the panel at TopLeft (0,0) where the editor
	// title bar covered it, so the user opened a loot box and saw nothing.
	// This test pins the anchor at the data layer: load the layout, find
	// NearbyBackground, assert its CanvasPanelSlot anchors land at the
	// right edge / vertical middle. A future typo ("MiddleRight",
	// "RightMiddle", whatever) will trip this test instead of shipping
	// silently.

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World)) { return false; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance)) { return false; }

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	UW_NearbyContainerPanel* Panel = CreateWidget<UW_NearbyContainerPanel>(
		GameInstance, UW_NearbyContainerPanel::StaticClass());
	if (!TestNotNull(TEXT("NearbyContainerPanel should construct"), Panel)) { return false; }
	Panel->AddToViewport();
	Panel->ForceLayoutPrepass();

	UWidget* Root = Panel->GetRootWidget();
	if (!TestNotNull(TEXT("Panel root widget exists"), Root)) { return false; }

	UBorder* NearbyBackground = UProjectWidgetHelpers::FindWidgetByNameTyped<UBorder>(Root, TEXT("NearbyBackground"));
	if (!TestNotNull(TEXT("NearbyBackground should exist after layout load"), NearbyBackground)) { return false; }

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(NearbyBackground->Slot);
	if (!TestNotNull(TEXT("NearbyBackground must be parented to a CanvasPanel"), CanvasSlot)) { return false; }

	const FAnchors Anchors = CanvasSlot->GetAnchors();
	TestEqual(TEXT("NearbyBackground anchor X (right edge)"), static_cast<float>(Anchors.Minimum.X), 1.0f);
	TestEqual(TEXT("NearbyBackground anchor Y (vertical center)"), static_cast<float>(Anchors.Minimum.Y), 0.5f);
	TestEqual(TEXT("NearbyBackground anchor max X matches min (point anchor)"), static_cast<float>(Anchors.Maximum.X), 1.0f);
	TestEqual(TEXT("NearbyBackground anchor max Y matches min (point anchor)"), static_cast<float>(Anchors.Maximum.Y), 0.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryUIVisibilityCoordinatorSpawnsBothPanelsTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.VisibilityCoordinatorSpawnsBothPanels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryUIVisibilityCoordinatorSpawnsBothPanelsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Regression: after the nearby-decouple landed, only the main inventory
	// panel was wired into SinglePlayController's bPanelVisible handler.
	// The nearby loot panel was registered in ui_definitions.json but
	// nothing ever called ShowDefinition for it during a real loot
	// session, so the user opened a cardboard box and saw nothing on the
	// right side. The dump-tree test "passed" because it manually called
	// ShowDefinition for both widgets - test theater.
	//
	// This test pins down the coordinator contract: a single call to
	// `InventoryUIVisibilityCoordinator::SetInventoryUIVisible(LayerHost, true)`
	// MUST spawn BOTH widgets via the layer host. This is the entry point
	// every production trigger MUST go through.
	//
	// If anyone reroutes the spawn through a direct
	// `LayerHost->ShowDefinition("...InventoryPanel")` call, this test
	// keeps firing because that direct call only spawns one widget.

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World)) { return false; }

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance)) { return false; }

	UProjectUILayerHostSubsystem* LayerHost = GameInstance->GetSubsystem<UProjectUILayerHostSubsystem>();
	if (!TestNotNull(TEXT("LayerHost subsystem should exist"), LayerHost)) { return false; }

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		LayerHost->InitializeForPlayer(PC, true);
	}

	FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

	// Sanity: the helper exposes both ids so callers can't silently drift
	// off-spec. Test by definition id, not by class, so a typo in the
	// constants surfaces here.
	const FString MainId = FString(InventoryUIVisibilityCoordinator::GetMainPanelDefinitionId());
	const FString NearbyId = FString(InventoryUIVisibilityCoordinator::GetNearbyPanelDefinitionId());
	TestEqual(TEXT("Main panel id wired"), MainId, FString(TEXT("ProjectInventoryUI.InventoryPanel")));
	TestEqual(TEXT("Nearby panel id wired"), NearbyId, FString(TEXT("ProjectInventoryUI.NearbyContainerPanel")));

	// Ensure clean baseline.
	InventoryUIVisibilityCoordinator::SetInventoryUIVisible(LayerHost, false);

	// Production-shaped trigger: ONE call, both widgets must come up.
	InventoryUIVisibilityCoordinator::SetInventoryUIVisible(LayerHost, true);

	auto AnyWidgetOfClass = [World](UClass* Class) -> UUserWidget*
	{
		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Found, Class, false);
		return Found.Num() > 0 ? Found[0] : nullptr;
	};

	UUserWidget* MainPanel = AnyWidgetOfClass(UW_InventoryPanel::StaticClass());
	UUserWidget* NearbyPanel = AnyWidgetOfClass(UW_NearbyContainerPanel::StaticClass());

	TestNotNull(
		TEXT("After SetInventoryUIVisible(true), main inventory panel must exist in viewport"),
		MainPanel);
	TestNotNull(
		TEXT("After SetInventoryUIVisible(true), nearby container panel MUST also exist - "
		     "this is the regression that hid loot containers in production. If null, the "
		     "coordinator is no longer spawning the sibling and any production trigger "
		     "calling SetInventoryUIVisible expects both."),
		NearbyPanel);

	// Cleanup: hide both. We don't assert hide post-state because the
	// LayerHost's HideDefinition leaves the persistent widget instance in
	// the viewport graph; the layer-host bookkeeping toggles its visibility
	// via the input-mode pipeline, not via ESlateVisibility on the widget
	// itself. The contract worth locking down is the SHOW pair (above);
	// any future hide-side regression should add its own focused test.
	InventoryUIVisibilityCoordinator::SetInventoryUIVisible(LayerHost, false);
	return true;
}

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
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryNearbyLootDumpTreeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		AddError(TEXT("No automation world available for nearby-loot dump"));
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

	// Nearby-loot panel is a separate widget since the decouple. This call
	// MUST succeed via layer-host -> ui_definitions.json lookup; we do NOT
	// fall back to CreateWidget here, because the whole point of this slice
	// is to guarantee that `ProjectInventoryUI.NearbyContainerPanel` is
	// wired into `ui_definitions.json`. Falling back would let the test
	// pass with a broken registration.
	UUserWidget* NearbyWidget = LayerHost->ShowDefinition(TEXT("ProjectInventoryUI.NearbyContainerPanel"));
	if (!TestNotNull(
		TEXT("LayerHost must resolve ProjectInventoryUI.NearbyContainerPanel via ui_definitions.json"),
		NearbyWidget))
	{
		return false;
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

	// The nearby widget binds the same VM. Visibility is NOT forced here -
	// the widget must derive it from `VM.bPanelVisible && VM.bHasNearbyContainer`
	// via its own RefreshFromViewModel. If this test starts failing because
	// visibility stays collapsed, the VM binding / property notification
	// path is genuinely broken - don't paper over it with SetVisibility.
	UW_NearbyContainerPanel* NearbyPanel = Cast<UW_NearbyContainerPanel>(NearbyWidget);
	TestNotNull(TEXT("NearbyContainerPanel definition must resolve to UW_NearbyContainerPanel"), NearbyPanel);
	if (NearbyPanel)
	{
		NearbyPanel->SetViewModel(InventoryVM);
		NearbyPanel->ForceLayoutPrepass();
		TestNotEqual(
			TEXT("NearbyContainerPanel must become visible when bPanelVisible && bHasNearbyContainer both hold"),
			NearbyPanel->GetVisibility(),
			ESlateVisibility::Collapsed);
	}

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
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryNakedDumpTest::RunTest(const FString& Parameters)
{
	if (!ValidateEquipSlotTags(this))
	{
		AddError(TEXT("Equip slot tag validation failed"));
		return false;
	}

	UWorld* World = ResolveInventoryAutomationWorld(this);
	if (!TestNotNull(TEXT("World"), World))
	{
		AddError(TEXT("No automation world available for naked inventory dump"));
		return false;
	}

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
