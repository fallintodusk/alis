// Copyright ALIS. All Rights Reserved.

#include "Widgets/ProjectRadialProgress.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

class SProjectRadialProgress final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SProjectRadialProgress)
		: _Percent(0.0f)
		, _Thickness(3.0f)
		, _FillColor(FLinearColor(0.2f, 0.6f, 0.9f, 1.0f))
		, _TrackColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.18f))
		, _StartAngleDegrees(-90.0f)
		, _bClockwise(true)
		, _bShowTrack(true)
	{
	}
		SLATE_ARGUMENT(float, Percent)
		SLATE_ARGUMENT(float, Thickness)
		SLATE_ARGUMENT(FLinearColor, FillColor)
		SLATE_ARGUMENT(FLinearColor, TrackColor)
		SLATE_ARGUMENT(float, StartAngleDegrees)
		SLATE_ARGUMENT(bool, bClockwise)
		SLATE_ARGUMENT(bool, bShowTrack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Percent = FMath::Clamp(InArgs._Percent, 0.0f, 1.0f);
		Thickness = FMath::Max(1.0f, InArgs._Thickness);
		FillColor = InArgs._FillColor;
		TrackColor = InArgs._TrackColor;
		StartAngleDegrees = InArgs._StartAngleDegrees;
		bClockwise = InArgs._bClockwise;
		bShowTrack = InArgs._bShowTrack;
	}

	void SetPercent(float InPercent)
	{
		Percent = FMath::Clamp(InPercent, 0.0f, 1.0f);
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetThickness(float InThickness)
	{
		Thickness = FMath::Max(1.0f, InThickness);
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetFillColor(FLinearColor InFillColor)
	{
		FillColor = InFillColor;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetTrackColor(FLinearColor InTrackColor)
	{
		TrackColor = InTrackColor;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetStartAngleDegrees(float InStartAngleDegrees)
	{
		StartAngleDegrees = InStartAngleDegrees;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetClockwise(bool bInClockwise)
	{
		bClockwise = bInClockwise;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void SetShowTrack(bool bInShowTrack)
	{
		bShowTrack = bInShowTrack;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D(20.0f, 20.0f);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		const float Diameter = FMath::Min(LocalSize.X, LocalSize.Y);
		if (Diameter <= Thickness)
		{
			return LayerId;
		}

		const FVector2D Center(LocalSize.X * 0.5f, LocalSize.Y * 0.5f);
		const float Radius = (Diameter * 0.5f) - (Thickness * 0.5f) - 1.0f;
		if (Radius <= 0.0f)
		{
			return LayerId;
		}

		if (bShowTrack)
		{
			TArray<FVector2D> TrackPoints;
			BuildArcPoints(TrackPoints, Center, Radius, StartAngleDegrees, bClockwise ? 360.0f : -360.0f, true);
			if (TrackPoints.Num() > 1)
			{
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					TrackPoints,
					ESlateDrawEffect::None,
					TrackColor * InWidgetStyle.GetColorAndOpacityTint(),
					true,
					Thickness);
			}
		}

		if (Percent > KINDA_SMALL_NUMBER)
		{
			TArray<FVector2D> ProgressPoints;
			const float SweepDegrees = (bClockwise ? 360.0f : -360.0f) * Percent;
			BuildArcPoints(ProgressPoints, Center, Radius, StartAngleDegrees, SweepDegrees, Percent >= 1.0f - KINDA_SMALL_NUMBER);
			if (ProgressPoints.Num() > 1)
			{
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId + 1,
					AllottedGeometry.ToPaintGeometry(),
					ProgressPoints,
					ESlateDrawEffect::None,
					FillColor * InWidgetStyle.GetColorAndOpacityTint(),
					true,
					Thickness);
			}
		}

		return LayerId + 1;
	}

private:
	static void BuildArcPoints(
		TArray<FVector2D>& OutPoints,
		const FVector2D& Center,
		float Radius,
		float StartAngleDegrees,
		float SweepDegrees,
		bool bCloseLoop)
	{
		const float AbsSweep = FMath::Abs(SweepDegrees);
		const int32 SegmentCount = FMath::Clamp(FMath::CeilToInt(AbsSweep / 10.0f), 2, 72);
		OutPoints.Reset();
		OutPoints.Reserve(SegmentCount + 2);

		const float StartRadians = FMath::DegreesToRadians(StartAngleDegrees);
		const float SweepRadians = FMath::DegreesToRadians(SweepDegrees);
		for (int32 SegmentIndex = 0; SegmentIndex <= SegmentCount; ++SegmentIndex)
		{
			const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
			const float Angle = StartRadians + (SweepRadians * Alpha);
			OutPoints.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}

		if (bCloseLoop && OutPoints.Num() > 0)
		{
			const FVector2D FirstPoint = OutPoints[0];
			OutPoints.Add(FirstPoint);
		}
	}

	float Percent = 0.0f;
	float Thickness = 3.0f;
	FLinearColor FillColor = FLinearColor::White;
	FLinearColor TrackColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.18f);
	float StartAngleDegrees = -90.0f;
	bool bClockwise = true;
	bool bShowTrack = true;
};

UProjectRadialProgress::UProjectRadialProgress(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UProjectRadialProgress::SetPercent(float InPercent)
{
	Percent = FMath::Clamp(InPercent, 0.0f, 1.0f);
	ApplyToSlate();
}

void UProjectRadialProgress::SetThickness(float InThickness)
{
	Thickness = FMath::Max(1.0f, InThickness);
	ApplyToSlate();
}

void UProjectRadialProgress::SetFillColor(FLinearColor InFillColor)
{
	FillColor = InFillColor;
	ApplyToSlate();
}

void UProjectRadialProgress::SetTrackColor(FLinearColor InTrackColor)
{
	TrackColor = InTrackColor;
	ApplyToSlate();
}

void UProjectRadialProgress::SetStartAngleDegrees(float InStartAngleDegrees)
{
	StartAngleDegrees = InStartAngleDegrees;
	ApplyToSlate();
}

void UProjectRadialProgress::SetClockwise(bool bInClockwise)
{
	bClockwise = bInClockwise;
	ApplyToSlate();
}

void UProjectRadialProgress::SetShowTrack(bool bInShowTrack)
{
	bShowTrack = bInShowTrack;
	ApplyToSlate();
}

TSharedRef<SWidget> UProjectRadialProgress::RebuildWidget()
{
	MyRadialProgress = SNew(SProjectRadialProgress)
		.Percent(Percent)
		.Thickness(Thickness)
		.FillColor(FillColor)
		.TrackColor(TrackColor)
		.StartAngleDegrees(StartAngleDegrees)
		.bClockwise(bClockwise)
		.bShowTrack(bShowTrack);

	return MyRadialProgress.ToSharedRef();
}

void UProjectRadialProgress::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyToSlate();
}

void UProjectRadialProgress::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyRadialProgress.Reset();
}

void UProjectRadialProgress::ApplyToSlate()
{
	if (!MyRadialProgress.IsValid())
	{
		return;
	}

	MyRadialProgress->SetPercent(Percent);
	MyRadialProgress->SetThickness(Thickness);
	MyRadialProgress->SetFillColor(FillColor);
	MyRadialProgress->SetTrackColor(TrackColor);
	MyRadialProgress->SetStartAngleDegrees(StartAngleDegrees);
	MyRadialProgress->SetClockwise(bClockwise);
	MyRadialProgress->SetShowTrack(bShowTrack);
}
