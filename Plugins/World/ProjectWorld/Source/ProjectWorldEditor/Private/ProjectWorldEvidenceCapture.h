// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UWorld;

struct FProjectWorldCaptureVantage
{
	FString Name;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	double FieldOfViewDegrees = 90.0;
	FString Note;
};

struct FProjectWorldCaptureView
{
	FString Name;
	FString File;
	int32 RequestedWidth = 0;
	int32 RequestedHeight = 0;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	double FieldOfViewDegrees = 0.0;
	FString ImageSha256;
};

struct FProjectWorldCaptureResult
{
	FString MapPackage;
	FString VantagePlanSha256;
	FString ScreenshotDirectory;
	int32 Width = 0;
	int32 Height = 0;
	TArray<FProjectWorldCaptureView> Views;
	// Same pose captured twice. Identical bytes prove the route is deterministic, which is what
	// makes the pairwise-distinct check below evidence of pose response rather than noise.
	FString ControlName;
	FString ControlSha256;
	bool bControlMatches = false;
	bool bViewsPairwiseDistinct = false;
	FString Status;
	FString Message;
};

namespace ProjectWorldEvidenceCapture
{
	bool LoadVantagePlan(
		const FString& PlanPath,
		int32& OutWidth,
		int32& OutHeight,
		TArray<FProjectWorldCaptureVantage>& OutVantages,
		FString& OutPlanSha256,
		FString& OutError);

	bool CaptureVantages(
		UWorld* World,
		const TArray<FProjectWorldCaptureVantage>& Vantages,
		int32 Width,
		int32 Height,
		const FString& OutputDirectory,
		FProjectWorldCaptureResult& OutResult,
		FString& OutError);

	bool WriteReceipt(const FProjectWorldCaptureResult& Result, const FString& ReceiptPath, FString& OutError);
}
