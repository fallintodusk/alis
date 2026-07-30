// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

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

FVector2D FProjectUIHoverTooltipPresenter::ResolveViewportSize() const
{
    FVector2D ViewportSize(1920.f, 1080.f);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }
    return ViewportSize;
}

FVector2D FProjectUIHoverTooltipPresenter::ResolveTooltipSize() const
{
    if (!TooltipWidget)
    {
        return FVector2D(220.f, 120.f);
    }

    TooltipWidget->ForceLayoutPrepass();
    FVector2D TooltipSize = TooltipWidget->GetDesiredSize();
    if (TooltipSize.IsNearlyZero())
    {
        TooltipSize = FVector2D(220.f, 120.f);
    }
    return TooltipSize;
}

FVector2D FProjectUIHoverTooltipPresenter::ClampTooltipPosition(
    const FVector2D& Position,
    const FVector2D& TooltipSize,
    const FVector2D& ViewportSize,
    float MinMargin)
{
    const float MaxX = FMath::Max(MinMargin, ViewportSize.X - TooltipSize.X - MinMargin);
    const float MaxY = FMath::Max(MinMargin, ViewportSize.Y - TooltipSize.Y - MinMargin);
    return FVector2D(
        FMath::Clamp(Position.X, MinMargin, MaxX),
        FMath::Clamp(Position.Y, MinMargin, MaxY));
}

void FProjectUIHoverTooltipPresenter::ApplyPosition(const FVector2D& Position)
{
    if (TooltipWidget)
    {
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(TooltipWidget->Slot))
        {
            Slot->SetPosition(Position);
        }
    }
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

    const FVector2D ViewportSize = ResolveViewportSize();
    const FVector2D TooltipSize = ResolveTooltipSize();

    FVector2D FinalPos = ViewportPos + CursorOffset;

    if (FinalPos.X + TooltipSize.X > ViewportSize.X)
    {
        FinalPos.X = ViewportPos.X - TooltipSize.X - 8.f;
    }
    if (FinalPos.Y + TooltipSize.Y > ViewportSize.Y)
    {
        FinalPos.Y = ViewportPos.Y - TooltipSize.Y - 8.f;
    }

    ApplyPosition(ClampTooltipPosition(FinalPos, TooltipSize, ViewportSize, MinMargin));
}

void FProjectUIHoverTooltipPresenter::PositionAtAnchor(
    const FVector2D& AnchorViewportPos,
    const FVector2D& Pivot,
    const FVector2D& AnchorOffset,
    float MinMargin)
{
    if (!TooltipWidget)
    {
        return;
    }

    const FVector2D TooltipSize = ResolveTooltipSize();
    const FVector2D ViewportSize = ResolveViewportSize();
    const FVector2D PivotOffset(TooltipSize.X * Pivot.X, TooltipSize.Y * Pivot.Y);
    const FVector2D FinalPos = AnchorViewportPos + AnchorOffset - PivotOffset;
    ApplyPosition(ClampTooltipPosition(FinalPos, TooltipSize, ViewportSize, MinMargin));
}
