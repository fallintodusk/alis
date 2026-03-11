// Copyright ALIS. All Rights Reserved.

#include "Debug/ProjectUIDebugOverlay.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/ProjectUserWidget.h"
#include "MVVM/ProjectViewModel.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"

void SProjectUIDebugOverlay::Construct(const FArguments& InArgs)
{
	// Initialize font for labels
	LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

	// Hit test invisible - overlay should not block input
	SetVisibility(EVisibility::HitTestInvisible);
}

FVector2D SProjectUIDebugOverlay::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// Fill entire viewport
	return FVector2D(10000, 10000);
}

int32 SProjectUIDebugOverlay::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	// Collect all widgets to draw
	TArray<TWeakObjectPtr<UWidget>> Widgets;
	CollectWidgets(Widgets);

	// Draw bounds for each widget
	for (const TWeakObjectPtr<UWidget>& WeakWidget : Widgets)
	{
		UWidget* Widget = WeakWidget.Get();
		if (!Widget)
		{
			continue;
		}

		if (!PassesFilter(Widget))
		{
			continue;
		}

		DrawWidgetBounds(Widget, AllottedGeometry, OutDrawElements, LayerId);
	}

	return LayerId + 1;
}

void SProjectUIDebugOverlay::CollectWidgets(TArray<TWeakObjectPtr<UWidget>>& OutWidgets) const
{
	// Iterate all UserWidgets in viewport
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* UserWidget = *It;
		if (!UserWidget || !UserWidget->IsInViewport())
		{
			continue;
		}

		// Recursively collect all widgets in tree
		TArray<UWidget*> AllWidgets;
		UserWidget->WidgetTree->GetAllWidgets(AllWidgets);

		for (UWidget* Widget : AllWidgets)
		{
			OutWidgets.Add(Widget);
		}

		// Add the root widget itself
		OutWidgets.Add(UserWidget);
	}
}

void SProjectUIDebugOverlay::RefreshWidgetCache()
{
	// Force repaint on next frame
	Invalidate(EInvalidateWidgetReason::Paint);
}

EProjectDebugWidgetState SProjectUIDebugOverlay::GetWidgetState(UWidget* Widget) const
{
	if (!Widget)
	{
		return EProjectDebugWidgetState::Hidden;
	}

	// Check visibility first
	if (!Widget->IsVisible())
	{
		return EProjectDebugWidgetState::Hidden;
	}

	// Check for zero size
	FVector2D ArrangedSize = Widget->GetCachedGeometry().GetLocalSize();
	if (ArrangedSize.IsNearlyZero())
	{
		return EProjectDebugWidgetState::ZeroSize;
	}

	// Check for missing ViewModel on ProjectUserWidget
	if (UProjectUserWidget* ProjectWidget = Cast<UProjectUserWidget>(Widget))
	{
		if (!ProjectWidget->GetViewModel())
		{
			return EProjectDebugWidgetState::NoViewModel;
		}
	}

	// Check if clipped by parent
	TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
	if (SlateWidget.IsValid())
	{
		// Check clipping state
		EWidgetClipping WidgetClipping = Widget->GetClipping();
		if (WidgetClipping == EWidgetClipping::ClipToBounds || WidgetClipping == EWidgetClipping::ClipToBoundsAlways)
		{
			// Widget itself clips - not an issue
		}
		else
		{
			// Check if any parent clips this widget
			UWidget* Parent = Widget->GetParent();
			while (Parent)
			{
				EWidgetClipping ParentClipping = Parent->GetClipping();
				if (ParentClipping == EWidgetClipping::ClipToBounds || ParentClipping == EWidgetClipping::ClipToBoundsAlways)
				{
					FVector2D ParentSize = Parent->GetCachedGeometry().GetLocalSize();
					if (ArrangedSize.X > ParentSize.X || ArrangedSize.Y > ParentSize.Y)
					{
						return EProjectDebugWidgetState::Clipped;
					}
				}
				Parent = Parent->GetParent();
			}
		}
	}

	// Check focus (simple check - may need refinement)
	if (SlateWidget.IsValid() && SlateWidget->HasKeyboardFocus())
	{
		return EProjectDebugWidgetState::Focused;
	}

	return EProjectDebugWidgetState::Normal;
}

FLinearColor SProjectUIDebugOverlay::GetStateColor(EProjectDebugWidgetState State) const
{
	switch (State)
	{
	case EProjectDebugWidgetState::Normal:
		return FLinearColor(0.0f, 0.8f, 0.0f, 0.4f);  // Green
	case EProjectDebugWidgetState::Clipped:
		return FLinearColor(1.0f, 0.8f, 0.0f, 0.5f);  // Yellow
	case EProjectDebugWidgetState::ZeroSize:
		return FLinearColor(1.0f, 0.0f, 0.0f, 0.6f);  // Red
	case EProjectDebugWidgetState::Hidden:
		return FLinearColor(0.5f, 0.0f, 0.0f, 0.3f);  // Dark red
	case EProjectDebugWidgetState::Focused:
		return FLinearColor(0.0f, 0.5f, 1.0f, 0.5f);  // Blue
	case EProjectDebugWidgetState::NoViewModel:
		return FLinearColor(1.0f, 0.5f, 0.0f, 0.5f);  // Orange
	default:
		return FLinearColor(0.5f, 0.5f, 0.5f, 0.3f);  // Gray
	}
}

bool SProjectUIDebugOverlay::PassesFilter(UWidget* Widget) const
{
	if (!Widget)
	{
		return false;
	}

	// Name filter
	if (!Settings.NameFilter.IsEmpty())
	{
		FString WidgetName = Widget->GetName();
		if (!WidgetName.Contains(Settings.NameFilter, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	// Problems filter - only show problematic widgets
	if (Settings.bFilterProblems)
	{
		EProjectDebugWidgetState State = GetWidgetState(Widget);
		if (State == EProjectDebugWidgetState::Normal || State == EProjectDebugWidgetState::Focused)
		{
			return false;
		}
	}

	return true;
}

void SProjectUIDebugOverlay::DrawWidgetBounds(
	UWidget* Widget,
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	if (!Widget)
	{
		return;
	}

	const FGeometry& WidgetGeometry = Widget->GetCachedGeometry();
	FVector2D LocalSize = WidgetGeometry.GetLocalSize();

	// Skip if no meaningful size (unless filtering problems and this is zero size)
	if (LocalSize.IsNearlyZero() && !Settings.bFilterProblems)
	{
		return;
	}

	// Get absolute position in screen space
	FVector2D AbsolutePosition = WidgetGeometry.GetAbsolutePosition();
	FVector2D AbsoluteSize = WidgetGeometry.GetAbsoluteSize();

	// Convert to local coordinates of overlay
	FVector2D LocalPosition = AllottedGeometry.AbsoluteToLocal(AbsolutePosition);

	// Build rectangle
	FSlateRect BoundsRect(
		LocalPosition.X,
		LocalPosition.Y,
		LocalPosition.X + AbsoluteSize.X / AllottedGeometry.Scale,
		LocalPosition.Y + AbsoluteSize.Y / AllottedGeometry.Scale
	);

	// Get color based on state
	EProjectDebugWidgetState State = GetWidgetState(Widget);
	FLinearColor BoundsColor = GetStateColor(State);

	// Draw filled rectangle with low opacity
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(BoundsRect.Right - BoundsRect.Left, BoundsRect.Bottom - BoundsRect.Top),
			FSlateLayoutTransform(FVector2D(BoundsRect.Left, BoundsRect.Top))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		BoundsColor * 0.3f
	);

	// Draw border with higher opacity
	FLinearColor BorderColor = BoundsColor;
	BorderColor.A = 0.8f;

	// Top border
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(BoundsRect.Right - BoundsRect.Left, 1.0f),
			FSlateLayoutTransform(FVector2D(BoundsRect.Left, BoundsRect.Top))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		BorderColor
	);

	// Bottom border
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(BoundsRect.Right - BoundsRect.Left, 1.0f),
			FSlateLayoutTransform(FVector2D(BoundsRect.Left, BoundsRect.Bottom - 1.0f))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		BorderColor
	);

	// Left border
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(1.0f, BoundsRect.Bottom - BoundsRect.Top),
			FSlateLayoutTransform(FVector2D(BoundsRect.Left, BoundsRect.Top))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		BorderColor
	);

	// Right border
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(1.0f, BoundsRect.Bottom - BoundsRect.Top),
			FSlateLayoutTransform(FVector2D(BoundsRect.Right - 1.0f, BoundsRect.Top))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		BorderColor
	);

	// Draw label if enabled
	if (Settings.bShowNames || Settings.bShowSizes || Settings.bShowZOrder)
	{
		DrawWidgetLabel(Widget, WidgetGeometry, AllottedGeometry, OutDrawElements, LayerId + 2);
	}
}

void SProjectUIDebugOverlay::DrawWidgetLabel(
	UWidget* Widget,
	const FGeometry& WidgetGeometry,
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId) const
{
	if (!Widget)
	{
		return;
	}

	// Build label text
	FString LabelText;

	if (Settings.bShowNames)
	{
		LabelText = Widget->GetName();
	}

	if (Settings.bShowSizes)
	{
		FVector2D Size = WidgetGeometry.GetLocalSize();
		FString SizeStr = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
		if (!LabelText.IsEmpty())
		{
			LabelText += TEXT(" ");
		}
		LabelText += SizeStr;
	}

	if (Settings.bShowZOrder)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			FString ZStr = FString::Printf(TEXT("Z:%d"), CanvasSlot->GetZOrder());
			if (!LabelText.IsEmpty())
			{
				LabelText += TEXT(" ");
			}
			LabelText += ZStr;
		}
	}

	if (LabelText.IsEmpty())
	{
		return;
	}

	// Measure text
	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FVector2D TextSize = FontMeasure->Measure(LabelText, LabelFont);

	// Position label at top-left of widget
	FVector2D AbsolutePosition = WidgetGeometry.GetAbsolutePosition();
	FVector2D LocalPosition = AllottedGeometry.AbsoluteToLocal(AbsolutePosition);

	// Draw background for text
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(TextSize.X + 4, TextSize.Y + 2),
			FSlateLayoutTransform(FVector2D(LocalPosition.X, LocalPosition.Y - TextSize.Y - 2))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.7f)
	);

	// Draw text
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(
			TextSize,
			FSlateLayoutTransform(FVector2D(LocalPosition.X + 2, LocalPosition.Y - TextSize.Y - 1))
		),
		LabelText,
		LabelFont,
		ESlateDrawEffect::None,
		FLinearColor::White
	);
}
