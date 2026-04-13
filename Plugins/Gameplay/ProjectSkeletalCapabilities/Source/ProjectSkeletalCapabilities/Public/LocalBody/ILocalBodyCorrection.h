// Interface for LocalBody upper-chain correction strategies.
// Each implementation owns its own anim nodes, settings, and evaluation logic.
// The anim instance selects the active strategy and delegates all correction to it.

#pragma once

#include "CoreMinimal.h"
#include "ILocalBodyCorrection.generated.h"

UENUM()
enum class ELocalBodyUpperChainMode : uint8
{
	Disabled,
	TransitionGuard,
	AngleClamp,
	ChainIK,
};

// Snapshot of one frame's inputs, built on the game thread.
struct FLocalBodyFrameContext
{
	FTransform ComponentTransform;
	FTransform CameraTransform;
	FVector CameraWorldPos = FVector::ZeroVector;
	FVector NeckTargetWorld = FVector::ZeroVector;
	float ControlPitch = 0.f;
	float DeltaSeconds = 0.f;

	FVector SourceHeadWorld = FVector::ZeroVector;
	FVector SourceNeckWorld = FVector::ZeroVector;
	FVector SourceSpine05World = FVector::ZeroVector;
	bool bHasSourceHead = false;
	bool bHasSourceNeck = false;
	bool bHasSourceSpine05 = false;

	bool bIsFalling = false;
	bool bIsCrouching = false;
	float HorizontalSpeed = 0.f;
};

struct FSpineLockData;

// Debug state for visualization (shared across modes).
struct FLocalBodyFilterState
{
	float PreviousHorizontalSpeed = 0.f;
	float StopGuardTimeRemaining = 0.f;
	float LandingGuardTimeRemaining = 0.f;
	bool bWasFallingLastUpdate = false;
	bool bWasCorrectingLastFrame = false;
	float StopGuardAlpha = 0.f;
	float LandingGuardAlpha = 0.f;
};

// Strategy interface for upper-chain correction.
class ILocalBodyCorrection
{
public:
	virtual ~ILocalBodyCorrection() = default;

	// Configure and wire owned anim nodes. InputNode is the last base node (Spine02).
	virtual void InitializeNodes(UAnimInstance* AnimInstance, FAnimNode_Base* InputNode) = 0;

	// Return the tail node of this correction's chain (for wiring to CSToLocal).
	virtual FAnimNode_Base* GetOutputNode() = 0;

	// Append all owned nodes for proxy GetCustomNodes.
	virtual void GetNodes(TArray<FAnimNode_Base*>& OutNodes) = 0;

	// Apply correction to owned nodes each frame (anim thread, from PreUpdate).
	virtual void Update(const FSpineLockData& Data) = 0;

	// Evaluate correction on game thread. Writes results into OutData.
	virtual void EvaluateGameThread(
		const FLocalBodyFrameContext& Context,
		FSpineLockData& OutData) = 0;
};
