// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Scalability.h"

class ACharacter;
class APlayerController;
class FProjectWorldPerformanceCollector;
class UWorld;

struct FProjectWorldPerformanceRoute
{
	FString Name;
	TArray<FVector> Points;
	double DurationSeconds = 0.0;
	double ReadyWaitSeconds = 0.0;
	double LoadedCellSeconds = 0.0;
	double ActivatedCellSeconds = 0.0;
	int32 PeakLoadedCells = 0;
	int32 PeakActivatedCells = 0;
	int32 ActivationTransitions = 0;
	int32 StreamingFailures = 0;
};

class FProjectWorldProductPerformanceGate
{
public:
	~FProjectWorldProductPerformanceGate();

	void StartIfRequested();

private:
	enum class EPhase : uint8
	{
		WaitingForCorrectness,
		Warmup,
		Traversing,
		Settling,
		WaitingForCsv,
		Finished
	};

	bool ParseConfig(FString& OutError);
	bool Tick(float DeltaSeconds);
	bool TryReadCorrectnessResult(FString& OutError);
	bool TryAcquireProductWorld(FString& OutError);
	bool ConfigureCapture(FString& OutError);
	bool BuildRoutes(FString& OutError);
	bool TickWarmup();
	void BeginRoute();
	void TickRoute(float DeltaSeconds);
	void TickSettlement();
	void SampleResidency(float DeltaSeconds);
	FVector RoutePosition(const FProjectWorldPerformanceRoute& Route, double Alpha) const;
	bool IsStreamingCompleted() const;
	void EndCapture();
	void Finish(const FString& Status, const FString& ErrorCode, const FString& ErrorMessage);
	void WriteResult(const FString& Status, const FString& ErrorCode, const FString& ErrorMessage);
	void SetPhase(EPhase NewPhase);

	FString ResultPath;
	FString CorrectnessResultPath;
	FString RequestedCsvPath;
	FString WrittenCsvPath;
	FString OperationId;
	FString RuntimeProfileId;
	FString RuntimeProfileHash;
	FString MachineProfileId;
	FString MapPackage;
	FString CorrectnessStatus;
	FString CorrectnessExecutable;
	FString CorrectnessEngineVersion;
	FString CorrectnessGpuAdapter;
	FString CorrectnessGpuDriver;
	FString CorrectnessRhi;
	FString AcceptanceReason;
	FString PendingStatus;
	FString PendingErrorCode;
	FString PendingErrorMessage;
	TWeakObjectPtr<UWorld> ProductWorld;
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<ACharacter> PlayerCharacter;
	TSharedPtr<FProjectWorldPerformanceCollector> Collector;
	TArray<FProjectWorldPerformanceRoute> Routes;
	TMap<FGuid, uint8> PriorCellStates;
	FTSTicker::FDelegateHandle TickerHandle;
	TSharedFuture<FString> CsvWriteFuture;
	FVector CenterLocation = FVector::ZeroVector;
	FIntPoint CapturedResolution = FIntPoint::ZeroValue;
	Scalability::FQualityLevels PriorQualityLevels;
	EPhase Phase = EPhase::Finished;
	double GateStartedSeconds = 0.0;
	double PhaseStartedSeconds = 0.0;
	double CaptureReadySeconds = 0.0;
	double RouteStartedSeconds = 0.0;
	double SettlementStartedSeconds = 0.0;
	double LastMemorySampleSeconds = 0.0;
	double LastResidencySampleSeconds = 0.0;
	uint64 PeakProcessPhysicalBytes = 0;
	uint64 PeakGpuLocalBytes = 0;
	int32 CurrentRouteIndex = INDEX_NONE;
	int32 TotalStreamingFailures = 0;
	int32 TotalActivationTransitions = 0;
	int32 HighQualityLevel = INDEX_NONE;
	int32 StableReadyFrames = 0;
	bool bCsvCaptureStarted = false;
	bool bQualityOverrideApplied = false;
};
