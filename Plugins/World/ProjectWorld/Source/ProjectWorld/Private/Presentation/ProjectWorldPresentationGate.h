// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class ACameraActor;
class APlayerController;
class UWorld;

struct FProjectWorldPresentationGateConfig
{
	FString OperationId;
	FString ResultPath;
	FString MapPackage;
	FString WorldDataPlugin;
	FString MachineProfileId;
	FString PresentationProfileId;
	FString PresentationProfileHash;
	FString RuntimeProfileId;
	FString RuntimeProfileHash;
	TArray<FString> CameraRoles;
	int32 ResolutionX = 0;
	int32 ResolutionY = 0;
	int32 ScalabilityLevel = 0;
	int32 WarmupFrames = 0;
	int32 SampleFrames = 0;
	double FrameTimeBudgetMs = 0.0;
};

struct FProjectWorldPresentationViewpointEvidence
{
	FString CameraRole;
	FString ScreenshotPath;
	int32 SampleCount = 0;
	double P95FrameTimeMs = 0.0;
};

class FProjectWorldPresentationGate
{
public:
	~FProjectWorldPresentationGate();

	void StartIfRequested();

private:
	enum class EPhase : uint8
	{
		WaitingForWorld,
		Warmup,
		Sampling,
		WaitingForScreenshot,
		Finished
	};

	bool ParseConfig(FString& OutError);
	bool Tick(float DeltaSeconds);
	bool TryPrepareWorld();
	bool ValidateWorldIdentity(UWorld& World, FString& OutError) const;
	bool FindOwnedCameras(UWorld& World, FString& OutError);
	bool InspectRuntimeRoles(UWorld& World, FString& OutError);
	void BeginViewpoint();
	void RequestViewpointScreenshot();
	void CompleteViewpoint();
	void FinishAccepted();
	void FinishRejected(const FString& Code, const FString& Message);
	void WriteResult(const FString& Status, const FString& ErrorCode, const FString& ErrorMessage);
	double P95(const TArray<double>& Values) const;

	FProjectWorldPresentationGateConfig Config;
	TArray<TWeakObjectPtr<ACameraActor>> Cameras;
	TArray<FProjectWorldPresentationViewpointEvidence> ViewpointEvidence;
	TArray<double> FrameTimesMs;
	// Required roles observed so far across the fixed viewpoint sequence.
	// World Partition may stream route actors in and out as viewpoints
	// advance; the contract requires the complete set by the final viewpoint,
	// not simultaneity at any single camera.
	TSet<FString> ObservedRuntimeRoles;
	TWeakObjectPtr<APlayerController> PlayerController;
	FTSTicker::FDelegateHandle TickerHandle;
	FString LastReadinessError;
	EPhase Phase = EPhase::Finished;
	double PhaseStartedSeconds = 0.0;
	int32 CurrentViewpoint = 0;
	int32 RemainingWarmupFrames = 0;
	bool bMapTravelRequested = false;
};
