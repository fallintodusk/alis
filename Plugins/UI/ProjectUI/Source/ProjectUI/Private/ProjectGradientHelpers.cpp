// Copyright ALIS. All Rights Reserved.

#include "ProjectGradientHelpers.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetMathLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectGradientHelpers, Log, All);

FSlateBrush UProjectGradientHelpers::CreateGradientBrush(const FProjectUIGradient& Gradient, FVector2D Size)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = Size;

	// Create runtime texture with gradient
	UTexture2D* GradientTexture = CreateGradientTexture(Gradient, static_cast<int32>(Size.X), static_cast<int32>(Size.Y));

	if (GradientTexture)
	{
		Brush.SetResourceObject(GradientTexture);
	}
	else
	{
		// Fallback to solid color if texture creation fails
		if (Gradient.ColorStops.Num() > 0)
		{
			Brush.TintColor = FSlateColor(Gradient.ColorStops[0]);
		}
	}

	return Brush;
}

void UProjectGradientHelpers::ApplyGradientToWidget(UWidget* Widget, const FProjectUIGradient& Gradient, FVector2D Size)
{
	if (!Widget)
	{
		UE_LOG(LogProjectGradientHelpers, Warning, TEXT("ApplyGradientToWidget: Widget is null"));
		return;
	}

	FSlateBrush GradientBrush = CreateGradientBrush(Gradient, Size);

	// Try casting to known widget types
	if (UImage* Image = Cast<UImage>(Widget))
	{
		Image->SetBrush(GradientBrush);
	}
	else if (UBorder* Border = Cast<UBorder>(Widget))
	{
		Border->SetBrush(GradientBrush);
	}
	else
	{
		UE_LOG(LogProjectGradientHelpers, Warning, TEXT("ApplyGradientToWidget: Widget type not supported for gradient"));
	}
}

FLinearColor UProjectGradientHelpers::SampleGradient(const FProjectUIGradient& Gradient, float Position)
{
	Position = FMath::Clamp(Position, 0.0f, 1.0f);

	if (Gradient.ColorStops.Num() == 0)
	{
		return FLinearColor::White;
	}

	if (Gradient.ColorStops.Num() == 1)
	{
		return Gradient.ColorStops[0];
	}

	// Calculate which color stops to interpolate between
	const int32 NumStops = Gradient.ColorStops.Num();
	const float StopSpacing = 1.0f / static_cast<float>(NumStops - 1);

	int32 StopIndex = FMath::FloorToInt(Position / StopSpacing);
	StopIndex = FMath::Clamp(StopIndex, 0, NumStops - 2);

	const float LocalPosition = (Position - (StopIndex * StopSpacing)) / StopSpacing;

	return FMath::Lerp(Gradient.ColorStops[StopIndex], Gradient.ColorStops[StopIndex + 1], LocalPosition);
}

UTexture2D* UProjectGradientHelpers::CreateGradientTexture(const FProjectUIGradient& Gradient, int32 Width, int32 Height)
{
	if (Gradient.ColorStops.Num() < 2)
	{
		UE_LOG(LogProjectGradientHelpers, Warning, TEXT("CreateGradientTexture: Gradient needs at least 2 color stops"));
		return nullptr;
	}

	// Create texture
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogProjectGradientHelpers, Error, TEXT("CreateGradientTexture: Failed to create texture"));
		return nullptr;
	}

	// Lock texture for writing
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Pixels = static_cast<uint8*>(TextureData);

	// Fill pixels with gradient
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			FVector2D PixelCoord(static_cast<float>(X), static_cast<float>(Y));
			FVector2D Size(static_cast<float>(Width), static_cast<float>(Height));

			float GradientPos = CalculateGradientPosition(PixelCoord, Size, Gradient.AngleDegrees);
			FLinearColor Color = SampleGradient(Gradient, GradientPos);

			int32 PixelIndex = (Y * Width + X) * 4;
			Pixels[PixelIndex + 0] = static_cast<uint8>(Color.B * 255.0f);
			Pixels[PixelIndex + 1] = static_cast<uint8>(Color.G * 255.0f);
			Pixels[PixelIndex + 2] = static_cast<uint8>(Color.R * 255.0f);
			Pixels[PixelIndex + 3] = static_cast<uint8>(Color.A * 255.0f);
		}
	}

	// Unlock and update texture
	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	return Texture;
}

FVector2D UProjectGradientHelpers::AngleToDirection(float AngleDegrees)
{
	float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	return FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians));
}

float UProjectGradientHelpers::CalculateGradientPosition(FVector2D PixelCoord, FVector2D Size, float AngleDegrees)
{
	// Normalize pixel coordinates to 0-1 range
	FVector2D NormalizedCoord = PixelCoord / Size;

	// Calculate gradient direction
	FVector2D Direction = AngleToDirection(AngleDegrees);

	// Project pixel coordinate onto gradient direction
	float Position = FVector2D::DotProduct(NormalizedCoord, Direction);

	// Normalize to 0-1 range accounting for gradient length
	float GradientLength = FMath::Abs(Direction.X) + FMath::Abs(Direction.Y);
	Position = (Position + 0.5f * GradientLength) / GradientLength;

	return FMath::Clamp(Position, 0.0f, 1.0f);
}
