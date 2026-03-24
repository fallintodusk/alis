// Copyright ALIS. All Rights Reserved.

#include "Widgets/InventoryPanelTextUpdater.h"
#include "MVVM/InventoryViewModel.h"
#include "Widgets/InventoryPanelState.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Layout/ProjectWidgetLayoutLoader.h"

void FInventoryPanelTextUpdater::Initialize(const FWidgetRefs& InRefs)
{
    Refs = InRefs;
}

void FInventoryPanelTextUpdater::UpdateStatsText(UInventoryViewModel* VM)
{
    if (!VM)
    {
        return;
    }

    const float ContainerMaxWeight = VM->GetContainerMaxWeight();
    const float ContainerMaxVolume = VM->GetContainerMaxVolume();

    if (Refs.WeightText)
    {
        FText TotalWeight = FormatWeight(VM->GetCurrentWeight(), VM->GetMaxWeight());
        if (ContainerMaxWeight > 0.f)
        {
            const int32 ContainerPercent = FMath::RoundToInt(VM->GetContainerWeightRatio() * 100.f);
            Refs.WeightText->SetText(FText::Format(
                NSLOCTEXT("Inventory", "WeightWithContainer", "{0} [Tab: {1}%]"),
                TotalWeight,
                FText::AsNumber(ContainerPercent)));
        }
        else
        {
            Refs.WeightText->SetText(TotalWeight);
        }
    }

    if (Refs.VolumeText)
    {
        FText TotalVolume = FormatVolume(VM->GetCurrentVolume(), VM->GetMaxVolume());
        if (ContainerMaxVolume > 0.f)
        {
            const int32 ContainerPercent = FMath::RoundToInt(VM->GetContainerVolumeRatio() * 100.f);
            Refs.VolumeText->SetText(FText::Format(
                NSLOCTEXT("Inventory", "VolumeWithContainer", "{0} [Tab: {1}%]"),
                TotalVolume,
                FText::AsNumber(ContainerPercent)));
        }
        else
        {
            Refs.VolumeText->SetText(TotalVolume);
        }
    }

    if (Refs.ItemCountText)
    {
        Refs.ItemCountText->SetText(FText::AsNumber(VM->GetItemCount()));
    }
}

void FInventoryPanelTextUpdater::UpdateNearbyContainerInfo(UInventoryViewModel* VM)
{
    if (Refs.NearbyTitleText)
    {
        FText Title = NSLOCTEXT("Inventory", "NearbyLootFallbackTitle", "Nearby Loot");
        if (VM && VM->GetbHasNearbyContainer() && !VM->GetNearbyContainerLabel().IsEmpty())
        {
            Title = VM->GetNearbyContainerLabel();
        }
        Refs.NearbyTitleText->SetText(Title);
    }

    if (Refs.NearbyStatsText)
    {
        if (VM && VM->GetbHasNearbyContainer())
        {
            FString StatsText = FText::Format(
                NSLOCTEXT("Inventory", "NearbyLootStats", "{0}   {1}"),
                FormatWeight(VM->GetNearbyContainerCurrentWeight(), VM->GetNearbyContainerMaxWeight()),
                FormatVolume(VM->GetNearbyContainerCurrentVolume(), VM->GetNearbyContainerMaxVolume())).ToString();

            if (VM->GetNearbyContainerCellDepthUnits() > 1)
            {
                StatsText += FString::Printf(TEXT("   Depth: %d/cell"), VM->GetNearbyContainerCellDepthUnits());
            }

            Refs.NearbyStatsText->SetText(FText::FromString(StatsText));
        }
        else
        {
            Refs.NearbyStatsText->SetText(FText::GetEmpty());
        }
    }
}

void FInventoryPanelTextUpdater::UpdateSelectionInfo(UInventoryViewModel* VM, FInventoryPanelState& PanelState)
{
    FInventoryEntryView Entry;
    if (!PanelState.TryGetSelectedEntry(VM, Entry))
    {
        // No selection: show placeholder text, collapse icon and stats
        if (Refs.SelectionText) Refs.SelectionText->SetText(NSLOCTEXT("Inventory", "NoSelection", "No item selected"));
        if (Refs.SelectionStatsText) Refs.SelectionStatsText->SetVisibility(ESlateVisibility::Collapsed);
        if (Refs.ItemDetailsText) Refs.ItemDetailsText->SetVisibility(ESlateVisibility::Collapsed);
        if (Refs.ItemIcon) Refs.ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // Item selected: show info and restore visibility
    if (Refs.SelectionText)
    {
        Refs.SelectionText->SetText(Entry.DisplayName);
    }

    if (Refs.SelectionStatsText)
    {
        FString StatsText = FString::Printf(TEXT("Qty: %d  W: %.1f  V: %.1f"), Entry.Quantity, Entry.Weight, Entry.Volume);
        if (Entry.bUsesDepthStacking && Entry.MaxDepthUnits > 0)
        {
            StatsText += FString::Printf(TEXT("  D: %d/%d"), Entry.DepthUnitsUsed, Entry.MaxDepthUnits);
        }
        Refs.SelectionStatsText->SetText(FText::FromString(StatsText));
        Refs.SelectionStatsText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    if (Refs.ItemDetailsText)
    {
        Refs.ItemDetailsText->SetText(Entry.Description);
        Refs.ItemDetailsText->SetVisibility(Entry.Description.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::SelfHitTestInvisible);
    }

    if (Refs.ItemIcon)
    {
        if (!Entry.IconCode.IsEmpty())
        {
            Refs.ItemIcon->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("GameIcon"), nullptr));
            Refs.ItemIcon->SetText(FText::FromString(Entry.IconCode));
            Refs.ItemIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            Refs.ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    PanelState.SelectedMaxQuantity = Entry.Quantity;
    PanelState.ClampQuantity();
}

void FInventoryPanelTextUpdater::UpdateCommandButtons(UInventoryViewModel* VM, const FInventoryPanelState& PanelState)
{
    TArray<FProjectUIActionDescriptor> Actions;

    FInventoryEntryView Entry;
    if (VM && PanelState.TryGetSelectedEntry(VM, Entry))
    {
        VM->BuildActionDescriptors(Entry, Actions);
    }

    auto ApplyActionState = [&Actions](UButton* Button, FName ActionId)
    {
        if (!Button)
        {
            return;
        }

        const FProjectUIActionDescriptor* Action = UInventoryViewModel::FindActionDescriptor(Actions, ActionId);
        const bool bVisible = Action && Action->bVisible;
        const bool bEnabled = Action && Action->bVisible && Action->bEnabled;
        Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button->SetIsEnabled(bEnabled);
    };

    if (Refs.UseButton)
    {
        ApplyActionState(Refs.UseButton, UInventoryViewModel::GetActionIdUse());
    }

    if (Refs.DropButton)
    {
        ApplyActionState(Refs.DropButton, UInventoryViewModel::GetActionIdDrop());
    }

    if (Refs.EquipButton)
    {
        ApplyActionState(Refs.EquipButton, UInventoryViewModel::GetActionIdEquip());
    }

    if (Refs.TakeAllButton)
    {
        const bool bShowTakeAll = VM && VM->GetbHasNearbyContainer();
        const bool bEnableTakeAll = bShowTakeAll && VM->HasNearbyEntries();
        Refs.TakeAllButton->SetVisibility(bShowTakeAll ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Refs.TakeAllButton->SetIsEnabled(bEnableTakeAll);
    }
}

void FInventoryPanelTextUpdater::UpdateQuantityControls(const FInventoryPanelState& PanelState)
{
    if (Refs.QtyValueText)
    {
        Refs.QtyValueText->SetText(FText::AsNumber(PanelState.SelectedQuantity));
    }
    if (Refs.QtyDownButton)
    {
        Refs.QtyDownButton->SetIsEnabled(PanelState.SelectedQuantity > 1);
    }
    if (Refs.QtyUpButton)
    {
        Refs.QtyUpButton->SetIsEnabled(PanelState.SelectedMaxQuantity > 0 && PanelState.SelectedQuantity < PanelState.SelectedMaxQuantity);
    }
}

void FInventoryPanelTextUpdater::UpdateRotateState(bool bRotateOn)
{
    if (Refs.RotateStateText)
    {
        Refs.RotateStateText->SetText(FText::FromString(bRotateOn ? TEXT("On") : TEXT("Off")));
    }
}

FText FInventoryPanelTextUpdater::FormatWeight(float Current, float Max)
{
    return FText::FromString(FString::Printf(TEXT("Weight: %.1f / %.1f kg"), Current, Max));
}

FText FInventoryPanelTextUpdater::FormatVolume(float Current, float Max)
{
    return FText::FromString(FString::Printf(TEXT("Volume: %.1f / %.1f L"), Current, Max));
}
