// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UWorld;

struct FProjectWorldPerformanceEnvelope
{
	bool EstablishAndValidate(UWorld& World, FString& OutError);
	bool Validate(FString& OutError) const;
	void AppendReceiptFields(FJsonObject& Root) const;

	int32 VSync = INDEX_NONE;
	float MaxFps = -1.0f;
	bool bSmoothFrameRate = true;
	bool bUseFixedFrameRate = true;
	float FixedFrameRate = -1.0f;
	bool bUseFixedTimeStep = true;
	double FixedDeltaTimeSeconds = -1.0;
	bool bBenchmarking = true;
	int32 DynamicResolutionOperationMode = INDEX_NONE;
	FString DynamicResolutionStatus;
	bool bDynamicResolutionEnabled = true;
};
