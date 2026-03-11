// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UCanvasPanel;
class UUserWidget;
class UWidget;

/**
 * Generic hover tooltip presenter for canvas overlays.
 * Handles tooltip widget creation, visibility and viewport-clamped positioning.
 */
struct PROJECTUI_API FProjectUIHoverTooltipPresenter
{
public:
    /**
     * Create tooltip widget under target canvas.
     *
     * @param InCanvas - target canvas overlay.
     * @param InOuterWidget - widget outer for CreateWidget.
     * @param InTooltipClass - tooltip widget class.
     * @param InTooltipZOrder - tooltip z-order.
     */
    void Initialize(
        UCanvasPanel* InCanvas,
        UWidget* InOuterWidget,
        TSubclassOf<UUserWidget> InTooltipClass,
        int32 InTooltipZOrder = 50);

    /** Hide tooltip widget. */
    void Hide();

    /** True when tooltip visibility is not collapsed. */
    bool IsVisible() const;

    /**
     * Position tooltip near cursor with viewport clamp.
     * Tooltip size is measured from desired size after layout prepass.
     */
    void PositionNearCursor(
        const FVector2D& ViewportPos,
        const FVector2D& CursorOffset = FVector2D(16.f, 16.f),
        float MinMargin = 4.f);

    /** Access tooltip widget as typed class. */
    template<typename TWidget>
    TWidget* GetTooltipWidget() const
    {
        return Cast<TWidget>(TooltipWidget);
    }

private:
    UUserWidget* TooltipWidget = nullptr;
    UCanvasPanel* Canvas = nullptr;
};
