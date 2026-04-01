// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkeletalAssemblyComponent.generated.h"

/**
 * Assembly lifecycle states.
 *
 * Linear progression: Idle -> Assembling -> Ready.
 * TearingDown is entered from any state when the owning actor is being destroyed
 * or when the assembly is explicitly torn down for a rebuild.
 */
UENUM(BlueprintType)
enum class ESkeletalAssemblyState : uint8
{
	/** No definition assigned. Waiting for configuration. */
	Idle,

	/** Definition assigned, component graph being created. */
	Assembling,

	/** Assembly complete, all features activated. */
	Ready,

	/** Shutting down features and releasing component graph. */
	TearingDown
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAssemblyStateChanged,
	ESkeletalAssemblyState, OldState,
	ESkeletalAssemblyState, NewState);

/**
 * Main runtime host for skeletal actor assembly.
 *
 * Owns the assembly lifecycle state machine, creates the runtime component
 * graph from a definition, and manages registered skeletal features.
 *
 * ## What this component owns (core behavior)
 *
 * - Assembly state machine (Idle -> Assembling -> Ready -> TearingDown)
 * - Component graph creation from definition
 * - Attachment policy
 * - Visibility policy
 * - Feature activation / deactivation ordering
 *
 * ## What this component does NOT own (feature behavior)
 *
 * - Motion Matching (registered as feature)
 * - Mutable customization (registered as feature)
 * - Local first-person body (registered as feature)
 * - Debug capture (registered as feature)
 *
 * ## Usage
 *
 * Add to any skeletal actor (character pawn, NPC, creature).
 * Call RequestAssembly() to start the lifecycle state machine.
 * Definition-driven assembly will be added in Phase 2.
 */
UCLASS(ClassGroup=(Project), meta=(BlueprintSpawnableComponent))
class PROJECTSKELETALASSEMBLY_API USkeletalAssemblyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkeletalAssemblyComponent();

	/** Current assembly state. */
	UFUNCTION(BlueprintPure, Category = "SkeletalAssembly")
	ESkeletalAssemblyState GetAssemblyState() const { return AssemblyState; }

	/** Fired when the assembly state changes. */
	UPROPERTY(BlueprintAssignable, Category = "SkeletalAssembly")
	FOnAssemblyStateChanged OnAssemblyStateChanged;

	/** Request transition to Assembling state. Returns false if transition is invalid. */
	UFUNCTION(BlueprintCallable, Category = "SkeletalAssembly")
	bool RequestAssembly();

	/** Signal that assembly is complete. Transitions to Ready. Returns false if not Assembling. */
	UFUNCTION(BlueprintCallable, Category = "SkeletalAssembly")
	bool CompleteAssembly();

	/** Request teardown. Transitions to TearingDown, then back to Idle. Returns false if already tearing down. */
	UFUNCTION(BlueprintCallable, Category = "SkeletalAssembly")
	bool RequestTeardown();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Attempt a state transition. Validates and logs. */
	bool TransitionTo(ESkeletalAssemblyState NewState);

	/** Check if a transition from current state to target is valid. */
	bool IsValidTransition(ESkeletalAssemblyState From, ESkeletalAssemblyState To) const;

	UPROPERTY()
	ESkeletalAssemblyState AssemblyState = ESkeletalAssemblyState::Idle;
};
