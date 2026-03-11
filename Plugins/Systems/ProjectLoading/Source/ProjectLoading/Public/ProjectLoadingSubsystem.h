// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Services/ILoadingService.h"

#include "Interfaces/ProjectLoadingHandle.h"
#include "Types/ProjectLoadPhaseState.h"
#include "Types/ProjectLoadRequest.h"

#include "ProjectLoadingSubsystem.generated.h"

// Forward declarations for SOLID components
class FLoadRequestValidator;
class FLoadEventBroadcaster;
class FLoadCancellationManager;
class FLoadProgressTracker;
class FPhaseRetryStrategy;
class FLoadPipelineOrchestrator;
class FInitialExperienceLoader;
class FProjectLoadPhaseExecutor;

namespace ProjectLoading
{
	class FProjectLoadingHandle;
}

/**
 * Telemetry data structure for load operations.
 */
USTRUCT(BlueprintType)
struct PROJECTLOADING_API FProjectLoadTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Loading")
	double LoadStartTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Loading")
	double LoadEndTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Loading")
	float TotalProgress = 0.0f;
};

// Blueprint delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProjectLoadStartedSignature, const FLoadRequest&, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProjectLoadPhaseChangedSignature, const FLoadPhaseState&, PhaseState, float, NormalizedOverallProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProjectLoadProgressSignature, float, NormalizedOverallProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProjectLoadCompletedSignature, const FLoadRequest&, Request, const FProjectLoadTelemetry&, Telemetry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FProjectLoadFailedSignature, const FLoadRequest&, Request, const FText&, ErrorMessage, int32, ErrorCode);

/**
 * UProjectLoadingSubsystem
 *
 * GameInstance subsystem responsible for orchestrating phased load requests.
 * Implements ILoadingService interface for DIP compliance.
 *
 * Architecture (SOLID):
 * - This class acts as a FACADE delegating to focused components
 * - Each component has single responsibility (SRP)
 * - Components depend on abstractions (DIP)
 * - Components are open for extension, closed for modification (OCP)
 *
 * Components:
 * - FLoadRequestValidator: Validate requests
 * - FLoadEventBroadcaster: Broadcast events (thread-safe)
 * - FLoadCancellationManager: Handle cancellation
 * - FLoadProgressTracker: Track progress/telemetry
 * - FPhaseRetryStrategy: Retry logic with backoff
 * - FLoadPipelineOrchestrator: Execute pipeline
 * - FInitialExperienceLoader: Load entry experience
 *
 * Features access this via ILoadingService interface from ServiceLocator.
 */
UCLASS()
class PROJECTLOADING_API UProjectLoadingSubsystem : public UGameInstanceSubsystem, public ILoadingService
{
	GENERATED_BODY()

public:
	friend class ProjectLoading::FProjectLoadingHandle;

	UProjectLoadingSubsystem();

	//~UGameInstanceSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ILoadingService interface
	virtual TSharedPtr<ILoadingHandle> StartLoad(const FLoadRequest& Request) override;
	virtual bool CancelActiveLoad(bool bForce = false) override;
	virtual bool IsLoadInProgress() const override { return ActiveHandle.IsValid() && ActiveHandle->IsInProgress(); }
	virtual TSharedPtr<ILoadingHandle> GetActiveLoadHandle() const override { return ActiveHandle; }
	virtual bool BuildLoadRequestForExperience(FName ExperienceName, FLoadRequest& OutRequest, FText& OutError) override;
	//~End ILoadingService interface

	/** Reads initial entry experience name from config/state and triggers the first load. */
	void StartInitialExperience();

	/** Access telemetry snapshot for the current or most recent load. */
	const FProjectLoadTelemetry& GetTelemetry() const { return Telemetry; }

	/** Handle cancellation request from loading handle */
	void HandleCancellationRequest(bool bForce);

	/**
	 * Set the frame pump callback (called by UI layer).
	 * This delegate is invoked during blocking waits to allow UI updates.
	 * Keeps Systems layer decoupled from Slate - UI layer provides the pump.
	 *
	 * Example (in LoadingScreenSubsystem or loading widget):
	 *   LoadingSubsystem->SetPumpFrameCallback([]() {
	 *       if (FSlateApplication::IsInitialized()) {
	 *           FSlateApplication::Get().PumpMessages();
	 *           FSlateApplication::Get().Tick();
	 *       }
	 *   });
	 */
	void SetPumpFrameCallback(TFunction<void()> InCallback) { PumpFrameCallback = MoveTemp(InCallback); }

	/** Get the frame pump callback (used by orchestrator to pass to context) */
	TFunction<void()> GetPumpFrameCallback() const { return PumpFrameCallback; }

public:
	// Blueprint-assignable delegates
	UPROPERTY(BlueprintAssignable, Category = "Project Loading")
	FProjectLoadStartedSignature OnLoadStarted;

	UPROPERTY(BlueprintAssignable, Category = "Project Loading")
	FProjectLoadPhaseChangedSignature OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Project Loading")
	FProjectLoadProgressSignature OnProgress;

	UPROPERTY(BlueprintAssignable, Category = "Project Loading")
	FProjectLoadCompletedSignature OnCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Project Loading")
	FProjectLoadFailedSignature OnFailed;

protected:
	/** Complete the active load with final state */
	void CompleteActiveLoad(ELoadingState FinalState, const FText& ErrorMessage = FText(), int32 ErrorCode = 0);

	/** Execute the loading pipeline asynchronously */
	void ExecutePipeline();

private:
	/** Initialize SOLID components */
	void InitializeComponents();

	/** Shutdown SOLID components */
	void ShutdownComponents();

	/** Initialize phase executors */
	void InitializePhaseExecutors();

	/** Shutdown phase executors */
	void ShutdownPhaseExecutors();

	/** Schedule initial experience load on next tick */
	void ScheduleInitialExperienceLoad();

private:
	// SOLID Components (each with single responsibility)
	TSharedPtr<FLoadCancellationManager> CancellationManager;
	TSharedPtr<FLoadProgressTracker> ProgressTracker;
	TSharedPtr<FPhaseRetryStrategy> RetryStrategy;
	TSharedPtr<FLoadPipelineOrchestrator> PipelineOrchestrator;
	TSharedPtr<FInitialExperienceLoader> ExperienceLoader;
	TSharedPtr<FLoadEventBroadcaster> EventBroadcaster;

	// Phase executors (reused for all loads)
	TArray<TSharedPtr<FProjectLoadPhaseExecutor>> PhaseExecutors;

	// Active load state
	TSharedPtr<ILoadingHandle> ActiveHandle;
	FLoadRequest ActiveRequest;
	FProjectLoadTelemetry Telemetry;
	double LoadStartTimeSeconds = 0.0;

	// Async task handle for pipeline execution
	FGraphEventRef PipelineTask;

	// Ensures initial experience load is scheduled only once
	bool bInitialLoadScheduled = false;

	// Frame pump callback (injected by UI layer, used during blocking waits)
	TFunction<void()> PumpFrameCallback;
};
