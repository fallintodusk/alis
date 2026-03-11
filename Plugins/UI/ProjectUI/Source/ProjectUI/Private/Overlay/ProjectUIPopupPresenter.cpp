// Copyright ALIS. All Rights Reserved.

#include "Overlay/ProjectUIPopupPresenter.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectUIPopupPresenter, Log, All);

void FProjectUIPopupPresenter::Initialize(
    UCanvasPanel* InCanvas,
    UWidget* InOuterWidget,
    TSubclassOf<UUserWidget> InPopupClass,
    int32 InPopupZOrder,
    int32 InClickCatcherZOrder)
{
    Canvas = InCanvas;
    if (!Canvas || !InOuterWidget || !*InPopupClass)
    {
        return;
    }

    ClickCatcher = NewObject<UButton>(InOuterWidget, UButton::StaticClass(), TEXT("ProjectUIPopupClickCatcher"));
    if (ClickCatcher)
    {
        FButtonStyle InvisibleStyle;
        InvisibleStyle.Normal.TintColor = FSlateColor(FLinearColor::Transparent);
        InvisibleStyle.Hovered.TintColor = FSlateColor(FLinearColor::Transparent);
        InvisibleStyle.Pressed.TintColor = FSlateColor(FLinearColor::Transparent);
        ClickCatcher->SetStyle(InvisibleStyle);
        ClickCatcher->SetBackgroundColor(FLinearColor::Transparent);

        Canvas->AddChild(ClickCatcher);
        if (UCanvasPanelSlot* CatcherSlot = Cast<UCanvasPanelSlot>(ClickCatcher->Slot))
        {
            CatcherSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
            CatcherSlot->SetOffsets(FMargin(0.f));
            CatcherSlot->SetZOrder(InClickCatcherZOrder);
        }

        ClickCatcher->SetVisibility(ESlateVisibility::Collapsed);
    }

    PopupWidget = CreateWidget<UUserWidget>(InOuterWidget, InPopupClass);
    if (PopupWidget)
    {
        Canvas->AddChild(PopupWidget);
        if (UCanvasPanelSlot* PopupSlot = Cast<UCanvasPanelSlot>(PopupWidget->Slot))
        {
            PopupSlot->SetAutoSize(true);
            PopupSlot->SetZOrder(InPopupZOrder);
        }

        PopupWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    UE_LOG(LogProjectUIPopupPresenter, Verbose, TEXT("Popup presenter initialized"));
}

void FProjectUIPopupPresenter::Hide()
{
    if (PopupWidget)
    {
        PopupWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
    ShowClickCatcher(false);
}

void FProjectUIPopupPresenter::ShowClickCatcher(bool bShow)
{
    if (ClickCatcher)
    {
        ClickCatcher->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

bool FProjectUIPopupPresenter::IsVisible() const
{
    return PopupWidget && PopupWidget->GetVisibility() != ESlateVisibility::Collapsed;
}

