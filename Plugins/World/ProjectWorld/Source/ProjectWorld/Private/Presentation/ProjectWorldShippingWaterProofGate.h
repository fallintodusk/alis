// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Presentation/ProjectWorldPlayableTourDriver.h"
#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"

class ACharacter;
class APlayerController;
class UWorld;

namespace ProjectWorldShippingWaterProofContract
{
	constexpr double MaximumTargetXYErrorCentimeters = 25000.0;

	bool ValidateTargetRelativeTravel(
		double RequestedTargetDistanceCentimeters,
		double TargetXYErrorCentimeters,
		double HorizontalDisplacementCentimeters,
		FString& OutError);
}

struct FProjectWorldShippingWaterProofConfig
{
	FString OperationId;
	FString ResultPath;
	FString ReferencePath;
	FString RepeatPath;
	FString ProductPath;
	FString MapPackage;
	FString RuntimeProfileId;
	FString RuntimeProfileHash;
	FString MachineProfileId;
	FVector TargetLocation = FVector::ZeroVector;
};

class FProjectWorldShippingWaterProofGate
{
public:
	~FProjectWorldShippingWaterProofGate();

	void StartIfRequested();

private:
	enum class EPhase : uint8
	{
		WaitingForWorld,
		Touring,
		SettlingAtWater,
		WaitingForRepeat,
		Finished
	};

	bool ParseConfig(FString& OutError);
	bool Tick(float DeltaSeconds);
	bool TryAcquireProductWorld(FString& OutError);
	bool IsStreamingComplete() const;
	bool InitializeWaterCapture(FString& OutError);
	bool CaptureWater(const FString& Path, FString& OutError);
	bool CaptureFinalColorWater(FString& OutError);
	void FinishAccepted();
	void FinishRejected(const FString& Code, const FString& Message);
	void WriteResult(const FString& Status, const FString& ErrorCode, const FString& ErrorMessage) const;
	void SetPhase(EPhase NewPhase);

	FProjectWorldShippingWaterProofConfig Config;
	FProjectWorldPlayableTourDriver Driver;
	TWeakObjectPtr<UWorld> ProductWorld;
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<ACharacter> PlayerCharacter;
	ProjectWorldRuntimeScreenshotCapture::FCaptureSession WaterCaptureSession;
	ProjectWorldRuntimeScreenshotCapture::FCaptureSession FinalColorCaptureSession;
	FTSTicker::FDelegateHandle TickerHandle;
	FVector StartPlayerLocation = FVector::ZeroVector;
	FVector CapturePlayerLocation = FVector::ZeroVector;
	FVector CaptureCameraLocation = FVector::ZeroVector;
	FVector FinalColorCameraLocation = FVector::ZeroVector;
	FRotator FinalColorCameraRotation = FRotator::ZeroRotator;
	FString StartupError;
	FString ReferenceCaptureSessionId;
	FString RepeatCaptureSessionId;
	double RequestedTargetDistanceCentimeters = 0.0;
	EPhase Phase = EPhase::Finished;
	double GateStartedSeconds = 0.0;
	double PhaseStartedSeconds = 0.0;
	int32 StableFrames = 0;
	bool bReferenceCaptured = false;
	bool bRepeatCaptured = false;
	bool bFinalColorCaptured = false;
};
