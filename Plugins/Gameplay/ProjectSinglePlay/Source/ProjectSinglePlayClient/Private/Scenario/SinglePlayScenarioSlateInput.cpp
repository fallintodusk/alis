// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Scenario/SinglePlayScenarioSlateInput.h"

#include "Components/Widget.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWidget.h"

namespace
{
	bool ResolveWidgetPath(UWidget& Widget, FWidgetPath& OutPath)
	{
		if (!FSlateApplication::IsInitialized()) { return false; }
		const TSharedPtr<SWidget> CachedWidget = Widget.GetCachedWidget();
		FWidgetPath ArrangedPath;
		if (!CachedWidget.IsValid() ||
			!FSlateApplication::Get().GeneratePathToWidgetUnchecked(CachedWidget.ToSharedRef(), ArrangedPath))
		{
			return false;
		}

		TArray<FWidgetAndPointer> WidgetsAndPointers;
		WidgetsAndPointers.Reserve(ArrangedPath.Widgets.Num());
		for (int32 WidgetIndex = 0; WidgetIndex < ArrangedPath.Widgets.Num(); ++WidgetIndex)
		{
			WidgetsAndPointers.Emplace(ArrangedPath.Widgets[WidgetIndex]);
		}
		OutPath = FWidgetPath(WidgetsAndPointers);
		return OutPath.IsValid();
	}

	FPointerEvent PointerEvent(
		const FVector2D& Position,
		const FVector2D& PreviousPosition,
		const TSet<FKey>& PressedButtons,
		const FKey& EffectingButton)
	{
		return FPointerEvent(
			0,
			Position,
			PreviousPosition,
			PressedButtons,
			EffectingButton,
			0.0f,
			FModifierKeysState());
	}
}

bool FSinglePlayScenarioSlateInput::RoutePointerDown(UWidget& Widget, const FVector2D& Position)

{
	return RoutePointerDown(Widget, Position, EKeys::LeftMouseButton);
}

bool FSinglePlayScenarioSlateInput::RoutePointerDown(
	UWidget& Widget,
	const FVector2D& Position,
	const FKey& Button)
{
	FWidgetPath Path;
	if (!ResolveWidgetPath(Widget, Path)) { return false; }
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSet<FKey> Pressed = {Button};
	Slate.SetCursorPos(Position);
	Slate.RoutePointerDownEvent(Path, PointerEvent(Position, Position, Pressed, Button));
	return true;
}

bool FSinglePlayScenarioSlateInput::RoutePointerMove(
	UWidget& Widget,
	const FVector2D& Position,
	const FVector2D& PreviousPosition)
{
	FWidgetPath Path;
	if (!ResolveWidgetPath(Widget, Path)) { return false; }
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSet<FKey> Pressed = {EKeys::LeftMouseButton};
	Slate.SetCursorPos(Position);
	Slate.RoutePointerMoveEvent(Path, PointerEvent(Position, PreviousPosition, Pressed, EKeys::Invalid), false);
	return true;
}

bool FSinglePlayScenarioSlateInput::RoutePointerUp(UWidget& Widget, const FVector2D& Position)

{
	return RoutePointerUp(Widget, Position, EKeys::LeftMouseButton);
}

bool FSinglePlayScenarioSlateInput::RoutePointerUp(
	UWidget& Widget,
	const FVector2D& Position,
	const FKey& Button)
{
	FWidgetPath Path;
	if (!ResolveWidgetPath(Widget, Path)) { return false; }
	FSlateApplication& Slate = FSlateApplication::Get();
	Slate.SetCursorPos(Position);
	Slate.RoutePointerUpEvent(Path, PointerEvent(Position, Position, {}, Button));
	return true;
}

bool FSinglePlayScenarioSlateInput::RouteClick(
	UWidget& Widget,
	const FVector2D& Position,
	const FKey& Button)
{
	FWidgetPath Path;
	const TSharedPtr<SWidget> CachedWidget = Widget.GetCachedWidget();
	if (!CachedWidget.IsValid() || !ResolveWidgetPath(Widget, Path)) { return false; }
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSet<FKey> Pressed = {Button};
	Slate.SetCursorPos(Position);
	const FPointerEvent HoverEvent = PointerEvent(Position, Position, {}, EKeys::Invalid);
	CachedWidget->OnMouseEnter(Widget.GetCachedGeometry(), HoverEvent);
	Slate.RoutePointerDownEvent(Path, PointerEvent(Position, Position, Pressed, Button));
	Slate.RoutePointerUpEvent(Path, PointerEvent(Position, Position, {}, Button));
	CachedWidget->OnMouseLeave(HoverEvent);
	return true;
}
