// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTextBlock;
class UButton;
class UInventoryViewModel;
class FInventoryPanelState;
struct FInventoryEntryView;

/**
 * SOLID helper: Updates text widgets and command buttons based on ViewModel state.
 * Single responsibility - text/stats display and button enablement.
 */
struct PROJECTINVENTORYUI_API FInventoryPanelTextUpdater
{
public:
    // Widget references (set via Initialize)
    struct FWidgetRefs
    {
        UTextBlock* WeightText = nullptr;
        UTextBlock* VolumeText = nullptr;
        UTextBlock* ItemCountText = nullptr;
        UTextBlock* SelectionText = nullptr;
        UTextBlock* SelectionStatsText = nullptr;
        UTextBlock* ItemDetailsText = nullptr;
        UTextBlock* QtyValueText = nullptr;
        UTextBlock* RotateStateText = nullptr;
        UTextBlock* ItemIcon = nullptr;
        UButton* QtyDownButton = nullptr;
        UButton* QtyUpButton = nullptr;
        UButton* UseButton = nullptr;
        UButton* DropButton = nullptr;
        UButton* EquipButton = nullptr;
    };

    void Initialize(const FWidgetRefs& InRefs);

    // Update methods - each has single responsibility
    void UpdateStatsText(UInventoryViewModel* VM);
    void UpdateSelectionInfo(UInventoryViewModel* VM, FInventoryPanelState& PanelState);
    void UpdateCommandButtons(UInventoryViewModel* VM, const FInventoryPanelState& PanelState);
    void UpdateQuantityControls(const FInventoryPanelState& PanelState);
    void UpdateRotateState(bool bRotateOn);

    // Utility
    static FText FormatWeight(float Current, float Max);
    static FText FormatVolume(float Current, float Max);

private:
    FWidgetRefs Refs;
};
