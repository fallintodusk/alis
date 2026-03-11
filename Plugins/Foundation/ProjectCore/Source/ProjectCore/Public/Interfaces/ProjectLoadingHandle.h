// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ProjectLoadingHandle.generated.h"

/**
 * Loading handle state enum
 */
UENUM(BlueprintType)
enum class ELoadingState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	InProgress UMETA(DisplayName = "In Progress"),
	Completed UMETA(DisplayName = "Completed"),
	Failed UMETA(DisplayName = "Failed"),
	Cancelled UMETA(DisplayName = "Cancelled")
};

/**
 * ILoadingHandle
 *
 * Interface for tracking and controlling an async loading operation.
 * Returned by UProjectLoadingSubsystem::StartLoad() to allow callers to:
 * - Query current state and progress
 * - Cancel the operation
 * - Subscribe to completion events
 *
 * This is a non-UObject interface for C++ only.
 * For Blueprint exposure, use UProjectLoadingHandleWrapper.
 */
class PROJECTCORE_API ILoadingHandle
{
public:
	virtual ~ILoadingHandle() = default;

	/**
	 * Get the current state of the loading operation
	 * @return Current loading state
	 */
	virtual ELoadingState GetState() const = 0;

	/**
	 * Get the current progress percentage (0.0 to 1.0)
	 * @return Progress value between 0 and 1
	 */
	virtual float GetProgress() const = 0;

	/**
	 * Get the name of the current loading phase
	 * @return Phase name (e.g., "ResolveAssets", "Travel")
	 */
	virtual FName GetCurrentPhase() const = 0;

	/**
	 * Get human-readable status message for UI display
	 * @return Localized status text
	 */
	virtual FText GetStatusMessage() const = 0;

	/**
	 * Check if the loading operation is still in progress
	 * @return True if loading is active
	 */
	virtual bool IsInProgress() const = 0;

	/**
	 * Check if the loading operation completed successfully
	 * @return True if completed without errors
	 */
	virtual bool IsCompleted() const = 0;

	/**
	 * Check if the loading operation failed
	 * @return True if failed with error
	 */
	virtual bool IsFailed() const = 0;

	/**
	 * Check if the loading operation was cancelled
	 * @return True if cancelled by user or system
	 */
	virtual bool IsCancelled() const = 0;

	/**
	 * Cancel the loading operation
	 * @param bForce If true, cancel immediately without cleanup
	 * @return True if cancellation was initiated
	 */
	virtual bool Cancel(bool bForce = false) = 0;

	/**
	 * Get the error message if failed
	 * @return Error text, empty if not failed
	 */
	virtual FText GetErrorMessage() const = 0;

	/**
	 * Get the error code if failed
	 * @return Error code for telemetry, 0 if not failed
	 */
	virtual int32 GetErrorCode() const = 0;
};
