// Copyright ALIS. All Rights Reserved.

#include "Effects/ProjectEffectWidget.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectEffect, Log, All);

UProjectEffectWidget::UProjectEffectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Ticking is enabled by default for UserWidgets when they have NativeTick override
}

void UProjectEffectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bEffectEnabled)
	{
		OnEffectEnabled();
	}
}

void UProjectEffectWidget::NativeDestruct()
{
	if (bEffectEnabled)
	{
		OnEffectDisabled();
	}

	Super::NativeDestruct();
}

void UProjectEffectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bEffectEnabled)
	{
		UpdateEffect(InDeltaTime, MyGeometry);
	}
}

int32 UProjectEffectWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	if (bEffectEnabled && EffectOpacity > 0.0f)
	{
		RenderEffect(AllottedGeometry, OutDrawElements, LayerId, EffectOpacity);
	}

	return LayerId + 1;
}

void UProjectEffectWidget::SetEffectEnabled(bool bInEnabled)
{
	if (bEffectEnabled == bInEnabled)
	{
		return;
	}

	bEffectEnabled = bInEnabled;

	if (bEffectEnabled)
	{
		OnEffectEnabled();
	}
	else
	{
		OnEffectDisabled();
	}
}

void UProjectEffectWidget::SetEffectOpacity(float InOpacity)
{
	EffectOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
}

void UProjectEffectWidget::ResetEffect()
{
	// Base implementation - derived classes override for specific reset behavior
}

void UProjectEffectWidget::ApplyParams(const FProjectEffectParams& InParams)
{
	SetEffectEnabled(InParams.bEnabled);
	SetEffectOpacity(InParams.Opacity);
}

void UProjectEffectWidget::UpdateEffect(float DeltaTime, const FGeometry& AllottedGeometry)
{
	// Base implementation - derived classes override
}

void UProjectEffectWidget::RenderEffect(const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, float InOpacity) const
{
	// Base implementation - derived classes override
}

void UProjectEffectWidget::OnEffectEnabled()
{
	UE_LOG(LogProjectEffect, Verbose, TEXT("Effect enabled: %s"), *GetEffectTypeName());
}

void UProjectEffectWidget::OnEffectDisabled()
{
	UE_LOG(LogProjectEffect, Verbose, TEXT("Effect disabled: %s"), *GetEffectTypeName());
}

const FSlateBrush* UProjectEffectWidget::GetWhiteBrush()
{
	return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
}

void UProjectEffectWidget::DrawBox(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& AllottedGeometry, const FVector2D& Position, const FVector2D& Size,
	const FLinearColor& Color)
{
	const FSlateBrush* Brush = GetWhiteBrush();
	if (!Brush)
	{
		return;
	}

	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry(
		FVector2f(Size),
		FSlateLayoutTransform(FVector2f(Position))
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		Geometry,
		Brush,
		ESlateDrawEffect::None,
		Color
	);
}

void UProjectEffectWidget::DrawRotatedBox(FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FGeometry& AllottedGeometry, const FVector2D& Position, const FVector2D& Size,
	float RotationDegrees, const FLinearColor& Color)
{
	const FSlateBrush* Brush = GetWhiteBrush();
	if (!Brush)
	{
		return;
	}

	// Create transform with rotation around center
	const FVector2D HalfSize = Size * 0.5f;
	const FVector2D Center = Position + HalfSize;
	const FSlateRenderTransform RenderTransform =
		Concatenate(
			FSlateRenderTransform(FVector2D(-HalfSize.X, -HalfSize.Y)),
			FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(RotationDegrees))),
			FSlateRenderTransform(Center)
		);

	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry(
		Size,
		FSlateLayoutTransform(),
		RenderTransform
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		Geometry,
		Brush,
		ESlateDrawEffect::None,
		Color
	);
}
