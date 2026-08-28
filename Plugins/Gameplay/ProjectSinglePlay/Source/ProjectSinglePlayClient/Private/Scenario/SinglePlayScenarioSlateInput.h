// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UWidget;

class FSinglePlayScenarioSlateInput final
{
public:
	static bool RoutePointerDown(UWidget& Widget, const FVector2D& Position);
	static bool RoutePointerDown(UWidget& Widget, const FVector2D& Position, const FKey& Button);
	static bool RoutePointerMove(UWidget& Widget, const FVector2D& Position, const FVector2D& PreviousPosition);
	static bool RoutePointerUp(UWidget& Widget, const FVector2D& Position);
	static bool RoutePointerUp(UWidget& Widget, const FVector2D& Position, const FKey& Button);
	static bool RouteClick(UWidget& Widget, const FVector2D& Position, const FKey& Button);
};
