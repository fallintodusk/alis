// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProjectEffectWidget.generated.h"

/**
 * Base parameters for all UI effects.
 * Derived effects add their own specific parameters.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectEffectParams
{
	GENERATED_BODY()

	/** Whether the effect is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	bool bEnabled = true;

	/** Effect opacity multiplier (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 1.0f;

	/** Z-order for layering effects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	int32 ZOrder = 0;
};

/**
 * Base class for all procedural UI effects.
 *
 * Effects are pure C++ widgets that render using NativeOnPaint.
 * No project .uasset files required - uses engine's built-in brushes.
 *
 * SOLID Architecture:
 * - Single Responsibility: Each effect handles one visual behavior
 * - Open/Closed: New effects extend this base, don't modify it
 * - Liskov Substitution: All effects can be used interchangeably
 * - Interface Segregation: Minimal required interface
 * - Dependency Inversion: Effects depend on abstractions (this base)
 *
 * Usage:
 * 1. Derive from UProjectEffectWidget
 * 2. Override UpdateEffect() for simulation logic
 * 3. Override NativeOnPaint() for rendering
 * 4. Register in LayoutEffectRegistry
 */
UCLASS(Abstract, Blueprintable)
class PROJECTUI_API UProjectEffectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UProjectEffectWidget(const FObjectInitializer& ObjectInitializer);

	// UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// ==========================================================================
	// Effect Control
	// ==========================================================================

	/** Enable or disable the effect */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Effects")
	void SetEffectEnabled(bool bInEnabled);

	/** Check if effect is enabled */
	UFUNCTION(BlueprintPure, Category = "Project UI|Effects")
	bool IsEffectEnabled() const { return bEffectEnabled; }

	/** Set effect opacity (0-1) */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Effects")
	void SetEffectOpacity(float InOpacity);

	/** Get current effect opacity */
	UFUNCTION(BlueprintPure, Category = "Project UI|Effects")
	float GetEffectOpacity() const { return EffectOpacity; }

	/** Reset the effect to initial state */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Effects")
	virtual void ResetEffect();

	/** Apply parameters from JSON/data-driven config */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Effects")
	virtual void ApplyParams(const FProjectEffectParams& InParams);

	/** Get the effect type name for registry lookup */
	UFUNCTION(BlueprintPure, Category = "Project UI|Effects")
	virtual FString GetEffectTypeName() const { return TEXT("Base"); }

protected:
	// ==========================================================================
	// Override Points for Derived Effects
	// ==========================================================================

	/** Called every tick to update effect simulation. Override in derived classes. */
	virtual void UpdateEffect(float DeltaTime, const FGeometry& AllottedGeometry);

	/** Render the effect. Called from NativePaint. Override in derived classes. */
	virtual void RenderEffect(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, float InOpacity) const;

	/** Called when effect is enabled. Override for initialization. */
	virtual void OnEffectEnabled();

	/** Called when effect is disabled. Override for cleanup. */
	virtual void OnEffectDisabled();

	// ==========================================================================
	// Common Rendering Helpers
	// ==========================================================================

	/** Get engine's built-in white brush (no project assets needed) */
	static const FSlateBrush* GetWhiteBrush();

	/** Draw a colored box at the specified position */
	static void DrawBox(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& AllottedGeometry, const FVector2D& Position, const FVector2D& Size,
		const FLinearColor& Color);

	/** Draw a colored box with rotation */
	static void DrawRotatedBox(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FGeometry& AllottedGeometry, const FVector2D& Position, const FVector2D& Size,
		float RotationDegrees, const FLinearColor& Color);

private:
	UPROPERTY()
	bool bEffectEnabled = true;

	UPROPERTY()
	float EffectOpacity = 1.0f;
};
