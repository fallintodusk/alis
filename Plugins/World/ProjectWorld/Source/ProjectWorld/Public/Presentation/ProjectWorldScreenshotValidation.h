// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

namespace ProjectWorldScreenshotValidation
{
	PROJECTWORLD_API bool ValidatePixels(
		int32 Width,
		int32 Height,
		TArrayView64<const FColor> Pixels,
		FString& OutError);

	PROJECTWORLD_API bool ValidateFile(const FString& Path, FString& OutError);
}
