// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectLoadPhaseState.generated.h"

/**
 * Loading phase enum
 * Defines the sequential phases of the loading pipeline
 */
UENUM(BlueprintType)
enum class ELoadPhase : uint8
{
	None UMETA(DisplayName = "None"),
	ResolveAssets UMETA(DisplayName = "Resolve Assets"),
	MountContent UMETA(DisplayName = "Mount Content"),
	PreloadCriticalAssets UMETA(DisplayName = "Preload Critical Assets"),
	ActivateFeatures UMETA(DisplayName = "Activate Features"),
	Travel UMETA(DisplayName = "Travel"),
	Warmup UMETA(DisplayName = "Warmup"),
	Complete UMETA(DisplayName = "Complete")
};

/**
 * Phase execution state
 */
UENUM(BlueprintType)
enum class EPhaseState : uint8
{
	Pending UMETA(DisplayName = "Pending"),
	InProgress UMETA(DisplayName = "In Progress"),
	Completed UMETA(DisplayName = "Completed"),
	Failed UMETA(DisplayName = "Failed"),
	Skipped UMETA(DisplayName = "Skipped")
};

/**
 * FLoadPhaseState
 *
 * State information for a single phase of the loading pipeline.
 * Used for progress tracking, telemetry, and UI display.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FLoadPhaseState
{
	GENERATED_BODY()

public:
	/** Which phase this state represents */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	ELoadPhase Phase = ELoadPhase::None;

	/** Current execution state of this phase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	EPhaseState State = EPhaseState::Pending;

	/** Progress within this phase (0.0 to 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	float Progress = 0.0f;

	/** Human-readable status message for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	FText StatusMessage;

	/** Time when this phase started (in seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	double StartTime = 0.0;

	/** Time when this phase ended (in seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	double EndTime = 0.0;

	/** Error message if failed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	FText ErrorMessage;

	/** Error code for telemetry (0 = no error) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	int32 ErrorCode = 0;

public:
	/** Default constructor */
	FLoadPhaseState() = default;

	/** Constructor with phase */
	explicit FLoadPhaseState(ELoadPhase InPhase)
		: Phase(InPhase)
	{
	}

	/** Get duration of this phase in seconds */
	double GetDuration() const
	{
		if (EndTime > 0.0 && StartTime > 0.0)
		{
			return EndTime - StartTime;
		}
		return 0.0;
	}

	/** Check if this phase is complete (success or skip) */
	bool IsComplete() const
	{
		return State == EPhaseState::Completed || State == EPhaseState::Skipped;
	}

	/** Check if this phase failed */
	bool IsFailed() const
	{
		return State == EPhaseState::Failed;
	}

	/** Check if this phase is in progress */
	bool IsInProgress() const
	{
		return State == EPhaseState::InProgress;
	}

	/** Get phase name as string */
	FString GetPhaseName() const
	{
		return StaticEnum<ELoadPhase>()->GetNameStringByValue(static_cast<int64>(Phase));
	}

	/** Get debug string */
	FString ToString() const
	{
		return FString::Printf(TEXT("%s: %s (%.1f%%)"),
			*GetPhaseName(),
			*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(State)),
			Progress * 100.0f);
	}

	/**
	 * Validates the phase state for correctness and logical consistency.
	 * Checks progress bounds, timing, error state, and state transitions.
	 *
	 * @param OutErrors Array to populate with detailed error messages
	 * @return True if valid, false if any validation errors found
	 */
	bool Validate(TArray<FText>& OutErrors) const
	{
		bool bIsValid = true;

		// 1. Validate progress is in valid range [0.0, 1.0]
		if (Progress < 0.0f || Progress > 1.0f)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Progress out of range: %.2f (must be 0.0-1.0)"), Progress)));
			bIsValid = false;
		}

		// 2. Validate progress matches state expectations
		if (State == EPhaseState::Completed || State == EPhaseState::Skipped)
		{
			// Completed/Skipped phases should have progress = 1.0
			if (!FMath::IsNearlyEqual(Progress, 1.0f, 0.01f))
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Phase %s is %s but progress is %.2f (expected 1.0)"),
					*GetPhaseName(),
					State == EPhaseState::Completed ? TEXT("Completed") : TEXT("Skipped"),
					Progress)));
				// Warning only - don't mark invalid
			}
		}
		else if (State == EPhaseState::InProgress)
		{
			// InProgress phases should have progress > 0.0
			if (Progress <= 0.0f)
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Phase %s is InProgress but progress is %.2f (expected > 0.0)"),
					*GetPhaseName(), Progress)));
				// Warning only - valid to just start
			}
		}
		else if (State == EPhaseState::Pending)
		{
			// Pending phases should have progress = 0.0
			if (Progress > 0.0f)
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Phase %s is Pending but progress is %.2f (expected 0.0)"),
					*GetPhaseName(), Progress)));
				// Warning only
			}
		}

		// 3. Validate timing consistency
		if (StartTime < 0.0)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("StartTime is negative: %.2f"), StartTime)));
			bIsValid = false;
		}

		if (EndTime < 0.0)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("EndTime is negative: %.2f"), EndTime)));
			bIsValid = false;
		}

		if (State != EPhaseState::Pending && StartTime <= 0.0)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Phase %s is %s but StartTime not set (%.2f)"),
				*GetPhaseName(),
				*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(State)),
				StartTime)));
			// Warning only - may not be set yet
		}

		if ((State == EPhaseState::Completed || State == EPhaseState::Failed || State == EPhaseState::Skipped) && EndTime <= 0.0)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Phase %s is %s but EndTime not set (%.2f)"),
				*GetPhaseName(),
				*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(State)),
				EndTime)));
			// Warning only
		}

		if (EndTime > 0.0 && StartTime > 0.0 && EndTime < StartTime)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("EndTime (%.2f) is before StartTime (%.2f) - Duration: %.2f seconds"),
				EndTime, StartTime, EndTime - StartTime)));
			bIsValid = false;
		}

		// 4. Validate error state consistency
		if (State == EPhaseState::Failed)
		{
			if (ErrorMessage.IsEmpty())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Phase %s is Failed but ErrorMessage is empty"),
					*GetPhaseName())));
				bIsValid = false;
			}

			if (ErrorCode == 0)
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Phase %s is Failed but ErrorCode is 0 (expected non-zero)"),
					*GetPhaseName())));
				bIsValid = false;
			}
		}
		else
		{
			// Non-failed states should not have error messages or codes
			if (!ErrorMessage.IsEmpty())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Phase %s is %s but has ErrorMessage: '%s'"),
					*GetPhaseName(),
					*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(State)),
					*ErrorMessage.ToString())));
				// Warning only - may be residual
			}

			if (ErrorCode != 0)
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Phase %s is %s but has ErrorCode: %d"),
					*GetPhaseName(),
					*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(State)),
					ErrorCode)));
				// Warning only
			}
		}

		// 5. Validate phase is not None (except default construction)
		if (Phase == ELoadPhase::None && State != EPhaseState::Pending)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Phase is None but State is %s (invalid configuration)"),
				*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(State)))));
			bIsValid = false;
		}

		return bIsValid;
	}
};
