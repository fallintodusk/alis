// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

namespace ProjectWorldRuntimeScreenshotCapture
{
	bool CapturePlayerContext(
		UWorld& World,
		APlayerController& PlayerController,
		const FString& Path,
		FString& OutError);
}
