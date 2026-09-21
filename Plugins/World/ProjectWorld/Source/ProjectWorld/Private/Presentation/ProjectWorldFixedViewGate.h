// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class AActor;
class APlayerController;
class UWorld;

struct FProjectWorldFixedViewConfig
{
	FString OperationId;
	FString ResultPath;
	FString ScreenshotPath;
	FString MapPackage;
	FString SubjectClassPath;
	FVector PlayerLocation = FVector::ZeroVector;
	FVector LookAtLocation = FVector::ZeroVector;
	FVector SubjectLocation = FVector::ZeroVector;
	float SubjectToleranceCentimeters = 200.0f;
};

namespace ProjectWorldFixedViewContract
{
	bool ParseVector(const FString& Text, FVector& OutValue);
	bool ValidateConfig(const FProjectWorldFixedViewConfig& Config, FString& OutError);
}

class FProjectWorldFixedViewGate
{
public:
	~FProjectWorldFixedViewGate();

	void StartIfRequested();

private:
	enum class EPhase : uint8
	{
		WaitingForWorld,
		WaitingForStreaming,
		Settling,
		Finished
	};

	bool ParseConfig(FString& OutError);
	bool Tick(float DeltaSeconds);
	bool TryAcquireWorld(FString& OutError);
	bool IsStreamingComplete() const;
	AActor* FindSubject() const;
	bool Capture(FString& OutError);
	void FinishAccepted();
	void FinishRejected(const FString& Code, const FString& Message);
	void WriteResult(const FString& Status, const FString& ErrorCode, const FString& ErrorMessage) const;
	void SetPhase(EPhase NewPhase);

	FProjectWorldFixedViewConfig Config;
	TWeakObjectPtr<UWorld> ProductWorld;
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<AActor> SubjectActor;
	FTSTicker::FDelegateHandle TickerHandle;
	FVector ActualCameraLocation = FVector::ZeroVector;
	FRotator ActualCameraRotation = FRotator::ZeroRotator;
	EPhase Phase = EPhase::Finished;
	double GateStartedSeconds = 0.0;
	double PhaseStartedSeconds = 0.0;
	int32 StableStreamingFrames = 0;
};
