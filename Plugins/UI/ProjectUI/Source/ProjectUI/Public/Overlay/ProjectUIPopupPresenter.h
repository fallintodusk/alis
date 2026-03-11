// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UButton;
class UCanvasPanel;
class UUserWidget;
class UWidget;

/**
 * Generic popup presenter for canvas overlays.
 * Owns popup widget creation and click-catcher lifecycle.
 *
 * Domain-specific behavior (popup content/actions) stays in feature widgets.
 */
struct PROJECTUI_API FProjectUIPopupPresenter
{
public:
    /**
     * Create popup widget and click catcher under the provided canvas.
     *
     * @param InCanvas - target canvas overlay.
     * @param InOuterWidget - widget outer for CreateWidget.
     * @param InPopupClass - popup widget class.
     * @param InPopupZOrder - popup z-order.
     * @param InClickCatcherZOrder - click-catcher z-order.
     */
    void Initialize(
        UCanvasPanel* InCanvas,
        UWidget* InOuterWidget,
        TSubclassOf<UUserWidget> InPopupClass,
        int32 InPopupZOrder = 100,
        int32 InClickCatcherZOrder = 99);

    /** Hide popup and click catcher. */
    void Hide();

    /** Show/hide click catcher behind popup. */
    void ShowClickCatcher(bool bShow);

    /** True when popup widget visibility is not collapsed. */
    bool IsVisible() const;

    /** Access popup widget as typed class. */
    template<typename TWidget>
    TWidget* GetPopupWidget() const
    {
        return Cast<TWidget>(PopupWidget);
    }

    /** Access click catcher for binding outside-click handler. */
    UButton* GetClickCatcher() const { return ClickCatcher; }

private:
    UUserWidget* PopupWidget = nullptr;
    UButton* ClickCatcher = nullptr;
    UCanvasPanel* Canvas = nullptr;
};

