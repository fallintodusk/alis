// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldScreenshotValidation.h"

#include "ImageCore.h"
#include "ImageUtils.h"

bool ProjectWorldScreenshotValidation::ValidatePixels(
	int32 Width,
	int32 Height,
	TArrayView64<const FColor> Pixels,
	FString& OutError)
{
	const int64 ExpectedPixels = static_cast<int64>(Width) * Height;
	if (Width < 64 || Height < 64 || Pixels.Num() != ExpectedPixels)
	{
		OutError = TEXT("The screenshot dimensions or pixel count are invalid.");
		return false;
	}
	uint8 Minimum = MAX_uint8;
	uint8 Maximum = 0;
	int64 NearWhite = 0;
	int64 NearBlack = 0;
	const int64 Stride = FMath::Max<int64>(1, ExpectedPixels / 65536);
	int64 SampleCount = 0;
	for (int64 Index = 0; Index < ExpectedPixels; Index += Stride)
	{
		const FColor& Color = Pixels[Index];
		Minimum = FMath::Min(Minimum, FMath::Min3(Color.R, Color.G, Color.B));
		Maximum = FMath::Max(Maximum, FMath::Max3(Color.R, Color.G, Color.B));
		NearWhite += Color.R >= 248 && Color.G >= 248 && Color.B >= 248 ? 1 : 0;
		NearBlack += Color.R <= 4 && Color.G <= 4 && Color.B <= 4 ? 1 : 0;
		++SampleCount;
	}
	const bool bHasRange = static_cast<int32>(Maximum) - Minimum >= 16;
	const bool bNotBlank = NearWhite * 100 < SampleCount * 99 && NearBlack * 100 < SampleCount * 99;
	if (!bHasRange || !bNotBlank)
	{
		OutError = FString::Printf(
			TEXT("The screenshot is visually blank (range=%d, white=%lld/%lld, black=%lld/%lld)."),
			static_cast<int32>(Maximum) - Minimum,
			NearWhite,
			SampleCount,
			NearBlack,
			SampleCount);
		return false;
	}
	return true;
}

bool ProjectWorldScreenshotValidation::ValidateFile(const FString& Path, FString& OutError)
{
	FImage Image;
	if (!FImageUtils::LoadImage(*Path, Image))
	{
		OutError = TEXT("The screenshot could not be decoded.");
		return false;
	}
	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	return ValidatePixels(Image.SizeX, Image.SizeY, Image.AsBGRA8(), OutError);
}
