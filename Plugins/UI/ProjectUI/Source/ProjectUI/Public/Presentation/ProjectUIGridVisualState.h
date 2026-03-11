// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

class UProjectUIThemeData;

/**
 * Cell visual state flags for determining appearance.
 */
struct PROJECTUI_API FProjectUICellState
{
    bool bEnabled = true;
    bool bSelected = false;
    bool bHovered = false;
    bool bDragPreview = false;
    bool bDragPreviewValid = false;
};

/**
 * Cached colors resolved from theme for grid cell states.
 */
struct PROJECTUI_API FProjectUIGridColors
{
    FLinearColor Base;
    FLinearColor Disabled;
    FLinearColor Selected;
    FLinearColor Hovered;
    FLinearColor PreviewValid;
    FLinearColor PreviewInvalid;

    FProjectUIGridColors();

    void UpdateFromTheme(const UProjectUIThemeData* Theme);
    FLinearColor GetColorForState(const FProjectUICellState& State, float PulseAlpha = 1.0f) const;
};

/**
 * Generic visual-state applier for grid/tabs.
 * Widget-specific apply behavior is provided by caller callbacks.
 */
class PROJECTUI_API FProjectUIGridVisualState
{
public:
    FProjectUIGridVisualState() = default;

    void UpdateColors(const UProjectUIThemeData* Theme);
    const FProjectUIGridColors& GetColors() const { return Colors; }

    template<typename TWidgetType, typename TApplyVisualFunc, typename TEnabledCheckerFunc>
    void ApplyToGrid(
        const TArray<TObjectPtr<TWidgetType>>& Cells,
        int32 SelectedIndex,
        int32 HoveredIndex,
        const TSet<int32>& DragPreviewCells,
        bool bDragPreviewValid,
        TApplyVisualFunc&& ApplyVisual,
        TEnabledCheckerFunc&& EnabledChecker) const
    {
        const float PulseAlpha = bDragPreviewValid ? 1.0f
            : 0.5f + 0.5f * FMath::Abs(FMath::Sin(FPlatformTime::Seconds() * PI * 2.0));

        for (int32 Index = 0; Index < Cells.Num(); ++Index)
        {
            TWidgetType* Cell = Cells[Index];
            if (!Cell)
            {
                continue;
            }

            FProjectUICellState State;
            State.bEnabled = EnabledChecker(Index);
            State.bSelected = (Index == SelectedIndex);
            State.bHovered = (Index == HoveredIndex);
            State.bDragPreview = DragPreviewCells.Contains(Index);
            State.bDragPreviewValid = bDragPreviewValid;

            ApplyVisual(Cell, Colors.GetColorForState(State, PulseAlpha), State.bEnabled);
        }
    }

    template<typename TWidgetType, typename TApplyVisualFunc>
    void ApplyToTabs(
        const TArray<TObjectPtr<TWidgetType>>& TabCells,
        int32 SelectedTabIndex,
        TApplyVisualFunc&& ApplyVisual) const
    {
        for (int32 Index = 0; Index < TabCells.Num(); ++Index)
        {
            TWidgetType* TabCell = TabCells[Index];
            if (!TabCell)
            {
                continue;
            }

            ApplyVisual(TabCell, Index == SelectedTabIndex ? Colors.Selected : Colors.Base);
        }
    }

private:
    FProjectUIGridColors Colors;
};
