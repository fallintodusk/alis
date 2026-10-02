// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class AActor;
class APlayerController;
class UWorld;

struct FProjectWorldFixedViewScalarOverride
{
	FString ParameterName;
	float Value = 0.0f;
};

struct FProjectWorldFixedViewConfig
{
	FString OperationId;
	FString ResultPath;
	FString ScreenshotPath;
	FString MapPackage;
	FString SubjectClassPath;
	FString FixtureMeshPath;
	FString FixtureMaterialPath;
	FString ScalarTargetMaterialPath;
	TArray<FProjectWorldFixedViewScalarOverride> ScalarOverrides;
	FVector PlayerLocation = FVector::ZeroVector;
	FVector LookAtLocation = FVector::ZeroVector;
	FVector SubjectLocation = FVector::ZeroVector;
	FVector FixtureStartLocation = FVector::ZeroVector;
	FVector FixtureScale = FVector::OneVector;
	FRotator FixtureStartRotation = FRotator::ZeroRotator;
	FRotator FixtureFinalRotation = FRotator::ZeroRotator;
	float SubjectToleranceCentimeters = 200.0f;
	float ScalarTargetRadiusCentimeters = 20000.0f;
	bool bIsolateBaseColor = false;

	bool HasFixture() const
	{
		return !FixtureMeshPath.IsEmpty() || !FixtureMaterialPath.IsEmpty();
	}

	bool HasScalarOverride() const
	{
		return !ScalarOverrides.IsEmpty();
	}
};

namespace ProjectWorldFixedViewContract
{
	bool ParseVector(const FString& Text, FVector& OutValue);
	bool ParseScalarOverrides(
		const FString& Text,
		TArray<FProjectWorldFixedViewScalarOverride>& OutOverrides,
		FString& OutError);
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
	bool SpawnFixture(FString& OutError);
	bool ApplyFixtureFinalTransform(FString& OutError);
	bool ApplyScalarOverride(FString& OutError);
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
	int32 ScalarOverrideComponentCount = 0;
	int32 ScalarOverrideSlotCount = 0;
	EPhase Phase = EPhase::Finished;
	double GateStartedSeconds = 0.0;
	double PhaseStartedSeconds = 0.0;
	int32 StableStreamingFrames = 0;
	bool bFixtureFinalTransformApplied = false;
};
