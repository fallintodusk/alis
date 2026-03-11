// Copyright ALIS. All Rights Reserved.

#include "Effects/ProjectFireSparksEffect.h"
#include "Rendering/DrawElements.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectFireSparks, Log, All);

UProjectFireSparksEffect::UProjectFireSparksEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UProjectFireSparksEffect::ResetEffect()
{
	Super::ResetEffect();
	Sparks.Empty();
	SpawnAccumulator = 0.0f;
}

void UProjectFireSparksEffect::ApplyParams(const FProjectEffectParams& InParams)
{
	Super::ApplyParams(InParams);

	// If it's actually a FProjectFireSparksParams, apply sparks-specific settings
	// This allows the base params to work while specialized params add more
}

void UProjectFireSparksEffect::ApplySparksParams(const FProjectFireSparksParams& InParams)
{
	Params = InParams;
	Super::ApplyParams(InParams);
}

void UProjectFireSparksEffect::SetSparkColor(const FLinearColor& InColor)
{
	Params.SparkColor = InColor;
}

void UProjectFireSparksEffect::SetSpawnRate(float InRate)
{
	Params.SpawnRate = FMath::Max(0.0f, InRate);
}

void UProjectFireSparksEffect::OnEffectEnabled()
{
	Super::OnEffectEnabled();
	ResetEffect();
}

void UProjectFireSparksEffect::OnEffectDisabled()
{
	Sparks.Empty();
	Super::OnEffectDisabled();
}

void UProjectFireSparksEffect::UpdateEffect(float DeltaTime, const FGeometry& AllottedGeometry)
{
	SpawnSparks(DeltaTime);
	UpdateSparks(DeltaTime);
}

void UProjectFireSparksEffect::SpawnSparks(float DeltaTime)
{
	if (Params.SpawnRate <= 0.0f)
	{
		return;
	}

	SpawnAccumulator += DeltaTime;

	const float SecondsPerSpawn = 1.0f / Params.SpawnRate;

	while (SpawnAccumulator >= SecondsPerSpawn && Sparks.Num() < Params.MaxSparks)
	{
		SpawnAccumulator -= SecondsPerSpawn;
		Sparks.Add(CreateSpark());
	}

	// Prevent accumulator from growing unbounded when at max sparks
	if (Sparks.Num() >= Params.MaxSparks)
	{
		SpawnAccumulator = FMath::Min(SpawnAccumulator, SecondsPerSpawn);
	}
}

void UProjectFireSparksEffect::UpdateSparks(float DeltaTime)
{
	for (int32 i = Sparks.Num() - 1; i >= 0; --i)
	{
		FProjectSpark& Spark = Sparks[i];

		// Update age
		Spark.Age += DeltaTime;

		// Remove dead sparks
		if (Spark.Age >= Spark.Lifetime)
		{
			Sparks.RemoveAtSwap(i);
			continue;
		}

		// Apply gravity
		Spark.Velocity += Params.Gravity * DeltaTime;

		// Update position
		Spark.Position += Spark.Velocity * DeltaTime;

		// Update rotation
		if (Params.bEnableRotation)
		{
			Spark.Rotation += Spark.RotationSpeed * DeltaTime;
		}

		// Remove sparks that leave the visible area (with some margin)
		if (Spark.Position.X < -0.1f || Spark.Position.X > 1.1f ||
			Spark.Position.Y < -0.1f || Spark.Position.Y > 1.1f)
		{
			Sparks.RemoveAtSwap(i);
			continue;
		}
	}
}

FProjectSpark UProjectFireSparksEffect::CreateSpark() const
{
	FProjectSpark Spark;

	// Random position within spawn area
	Spark.Position.X = FMath::FRandRange(Params.SpawnAreaMin.X, Params.SpawnAreaMax.X);
	Spark.Position.Y = FMath::FRandRange(Params.SpawnAreaMin.Y, Params.SpawnAreaMax.Y);

	// Random velocity
	Spark.Velocity.X = FMath::FRandRange(Params.MinVelocity.X, Params.MaxVelocity.X);
	Spark.Velocity.Y = FMath::FRandRange(Params.MinVelocity.Y, Params.MaxVelocity.Y);

	// Random lifetime
	Spark.Lifetime = FMath::FRandRange(Params.MinLifetime, Params.MaxLifetime);
	Spark.Age = 0.0f;

	// Random rotation
	if (Params.bEnableRotation)
	{
		Spark.Rotation = FMath::FRandRange(0.0f, 360.0f);
		Spark.RotationSpeed = FMath::FRandRange(-Params.MaxRotationSpeed, Params.MaxRotationSpeed);
	}

	// Random size variation
	if (Params.bEnableSizeVariation)
	{
		Spark.SizeMultiplier = FMath::FRandRange(1.0f - Params.SizeVariation, 1.0f + Params.SizeVariation);
	}
	else
	{
		Spark.SizeMultiplier = 1.0f;
	}

	return Spark;
}

void UProjectFireSparksEffect::RenderEffect(const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, float InOpacity) const
{
	if (Sparks.Num() == 0)
	{
		return;
	}

	const FSlateBrush* WhiteBrush = GetWhiteBrush();
	if (!WhiteBrush)
	{
		return;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();

	for (const FProjectSpark& Spark : Sparks)
	{
		const FLinearColor Color = GetSparkColor(Spark, InOpacity);
		const FVector2D Size = GetSparkSize(Spark);
		const FVector2D HalfSize = Size * 0.5f;

		// Convert normalized position to pixel position
		const FVector2D CenterPx(
			Spark.Position.X * LocalSize.X,
			Spark.Position.Y * LocalSize.Y
		);
		const FVector2D TopLeft = CenterPx - HalfSize;

		if (Params.bEnableRotation && FMath::Abs(Spark.Rotation) > KINDA_SMALL_NUMBER)
		{
			// Rotated rendering
			const FSlateRenderTransform SparkRenderTransform =
				Concatenate(
					FSlateRenderTransform(FVector2D(-HalfSize.X, -HalfSize.Y)),
					FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Spark.Rotation))),
					FSlateRenderTransform(CenterPx)
				);

			const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry(
				FVector2f(Size),
				FSlateLayoutTransform(),
				SparkRenderTransform
			);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				Geometry,
				WhiteBrush,
				ESlateDrawEffect::None,
				Color
			);
		}
		else
		{
			// Non-rotated rendering (faster)
			const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry(
				FVector2f(Size),
				FSlateLayoutTransform(FVector2f(TopLeft))
			);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				Geometry,
				WhiteBrush,
				ESlateDrawEffect::None,
				Color
			);
		}
	}
}

FLinearColor UProjectFireSparksEffect::GetSparkColor(const FProjectSpark& Spark, float GlobalOpacity) const
{
	// Life progress from 0 (new) to 1 (dead)
	const float LifeProgress = FMath::Clamp(Spark.Age / Spark.Lifetime, 0.0f, 1.0f);

	// Lerp between start and end color
	FLinearColor Color = FMath::Lerp(Params.SparkColor, Params.EndColor, LifeProgress);

	// Apply global opacity
	Color.A *= GlobalOpacity;

	// Fade out towards end of life
	const float FadeStart = 0.7f;
	if (LifeProgress > FadeStart)
	{
		const float FadeProgress = (LifeProgress - FadeStart) / (1.0f - FadeStart);
		Color.A *= (1.0f - FadeProgress);
	}

	return Color;
}

FVector2D UProjectFireSparksEffect::GetSparkSize(const FProjectSpark& Spark) const
{
	// Life progress from 0 (new) to 1 (dead)
	const float LifeProgress = FMath::Clamp(Spark.Age / Spark.Lifetime, 0.0f, 1.0f);

	// Shrink towards end of life
	const float ShrinkFactor = 1.0f - (LifeProgress * 0.5f);

	return Params.PixelSize * Spark.SizeMultiplier * ShrinkFactor;
}
