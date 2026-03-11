// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Theme/ProjectUIThemeData.h"
#include "Styling/SlateBrush.h"
#include "ProjectGradientHelpers.generated.h"

/**
 * Gradient Utility Helpers
 *
 * Utilities for creating and applying gradients to UI widgets.
 * Converts FProjectUIGradient data to Slate brushes for rendering.
 */
UCLASS()
class PROJECTUI_API UProjectGradientHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create a gradient brush from gradient data
	 *
	 * @param Gradient - Gradient definition
	 * @param Size - Brush size (affects gradient rendering)
	 * @return Slate brush configured with gradient
	 *
	 * Usage:
	 * @code
	 * FProjectUIGradient MyGradient;
	 * FSlateBrush Brush = UProjectGradientHelpers::CreateGradientBrush(MyGradient, FVector2D(200, 100));
	 * MyImage->SetBrush(Brush);
	 * @endcode
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Gradient")
	static FSlateBrush CreateGradientBrush(const FProjectUIGradient& Gradient, FVector2D Size = FVector2D(256, 256));

	/**
	 * Apply gradient to widget image brush
	 *
	 * @param Widget - Widget with SetBrush method (UImage, UBorder, etc.)
	 * @param Gradient - Gradient definition
	 * @param Size - Brush size
	 *
	 * Note: Widget must have SetBrush method. This is a convenience wrapper.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Gradient", meta = (DefaultToSelf = "Widget"))
	static void ApplyGradientToWidget(UWidget* Widget, const FProjectUIGradient& Gradient, FVector2D Size = FVector2D(256, 256));

	/**
	 * Interpolate color along gradient at position
	 *
	 * @param Gradient - Gradient definition
	 * @param Position - Position along gradient (0.0 = start, 1.0 = end)
	 * @return Interpolated color at position
	 *
	 * Usage:
	 * @code
	 * FLinearColor MidColor = UProjectGradientHelpers::SampleGradient(MyGradient, 0.5f);
	 * @endcode
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Gradient")
	static FLinearColor SampleGradient(const FProjectUIGradient& Gradient, float Position);

	/**
	 * Create gradient texture (runtime generated)
	 *
	 * @param Gradient - Gradient definition
	 * @param Width - Texture width
	 * @param Height - Texture height
	 * @return Generated texture with gradient
	 *
	 * Note: Creates texture in memory. Use sparingly for performance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Gradient")
	static UTexture2D* CreateGradientTexture(const FProjectUIGradient& Gradient, int32 Width = 256, int32 Height = 256);

private:
	/**
	 * Convert angle to direction vector
	 */
	static FVector2D AngleToDirection(float AngleDegrees);

	/**
	 * Calculate gradient position for pixel coordinates
	 */
	static float CalculateGradientPosition(FVector2D PixelCoord, FVector2D Size, float AngleDegrees);
};
