// Copyright ALIS. All Rights Reserved.

#include "Overlay/ProjectUIHoverTooltipPresenter.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectUIHoverTooltipPresenter, Log, All);

void FProjectUIHoverTooltipPresenter::Initialize(
    UCanvasPanel* InCanvas,
    UWidget* InOuterWidget,
    TSubclassOf<UUserWidget> InTooltipClass,
    int32 InTooltipZOrder)
{
    Canvas = InCanvas;
    if (!Canvas || !InOuterWidget || !*InTooltipClass)
    {
        return;
    }

    TooltipWidget = CreateWidget<UUserWidget>(InOuterWidget, InTooltipClass);
    if (!TooltipWidget)
    {
        return;
    }

    Canvas->AddChild(TooltipWidget);
    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(TooltipWidget->Slot))
    {
        Slot->SetAutoSize(true);
        Slot->SetZOrder(InTooltipZOrder);
    }

    TooltipWidget->SetVisibility(ESlateVisibility::Collapsed);
    UE_LOG(LogProjectUIHoverTooltipPresenter, Verbose, TEXT("Hover tooltip presenter initialized"));
}

void FProjectUIHoverTooltipPresenter::Hide()
{
    if (TooltipWidget)
    {
        TooltipWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool FProjectUIHoverTooltipPresenter::IsVisible() const
{
    return TooltipWidget && TooltipWidget->GetVisibility() != ESlateVisibility::Collapsed;
}

void FProjectUIHoverTooltipPresenter::PositionNearCursor(
    const FVector2D& ViewportPos,
    const FVector2D& CursorOffset,
    float MinMargin)
{
    if (!TooltipWidget)
    {
        return;
    }

    FVector2D ViewportSize(1920.f, 1080.f);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }

    TooltipWidget->ForceLayoutPrepass();
    FVector2D TooltipSize = TooltipWidget->GetDesiredSize();
    if (TooltipSize.IsNearlyZero())
    {
        TooltipSize = FVector2D(220.f, 120.f);
    }

    FVector2D FinalPos = ViewportPos + CursorOffset;

    if (FinalPos.X + TooltipSize.X > ViewportSize.X)
    {
        FinalPos.X = ViewportPos.X - TooltipSize.X - 8.f;
    }
    if (FinalPos.Y + TooltipSize.Y > ViewportSize.Y)
    {
        FinalPos.Y = ViewportPos.Y - TooltipSize.Y - 8.f;
    }

    const float MaxX = FMath::Max(MinMargin, ViewportSize.X - TooltipSize.X - MinMargin);
    const float MaxY = FMath::Max(MinMargin, ViewportSize.Y - TooltipSize.Y - MinMargin);
    FinalPos.X = FMath::Clamp(FinalPos.X, MinMargin, MaxX);
    FinalPos.Y = FMath::Clamp(FinalPos.Y, MinMargin, MaxY);

    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(TooltipWidget->Slot))
    {
        Slot->SetPosition(FinalPos);
    }
}
