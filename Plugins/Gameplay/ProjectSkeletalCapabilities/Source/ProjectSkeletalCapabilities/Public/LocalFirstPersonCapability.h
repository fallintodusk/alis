// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "Interfaces/AssemblyTypes.h"
#include "LocalFirstPersonCapability.generated.h"

class USkeletalMeshComponent;
class ULocalBodyAnimInstance;

/**
 * First-person body visibility capability for the skeletal assembly framework.
 *
 * Handles owner-only visibility and animation policy for the fixed skeletal
 * assembly:
 *
 * - Hide head/neck bones on the LocalBody mesh (owner sees headless body)
 * - Hide Head mesh and Groom components from owner (with CastHiddenShadow)
 * - Set Head mesh LeaderPoseComponent to the active visual body source
 *   (prefer WorldBody, fall back to DriverBody only during early init)
 * - Install the owner-visible CopyPose animation instance on LocalBody
 *
 * Capability ID: "LocalFirstPerson"
 * Scope: per-mesh (attached to LocalBody mesh component)
 *
 * ## Timing
 *
 * Self-managed via assembly state delegate. On assembly Ready:
 * - Discovers LocalBody, WorldBody, Head, and Groom components via role tags
 * - Applies visibility policy
 *
 * ## Properties (set from JSON)
 *
 * - HiddenBones: comma-separated bone names to hide (e.g. "head,neck_01")
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTSKELETALCAPABILITIES_API ULocalFirstPersonCapability : public UActorComponent
{
	GENERATED_BODY()

public:
	ULocalFirstPersonCapability();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	// Discover meshes by role tags
	void DiscoverMeshes();

	// Apply full first-person visibility policy
	void ApplyVisibility();

	// Assembly state callback
	void OnAssemblyStateChanged(EAssemblyState NewState);

	// Comma-separated bone names to hide on LocalBody (e.g. "head,neck_01")
	UPROPERTY(EditAnywhere, Category = "FirstPerson")
	FString HiddenBones;

	// Discovered mesh references
	TWeakObjectPtr<USkeletalMeshComponent> LocalBodyMesh;
	TWeakObjectPtr<USkeletalMeshComponent> WorldBodyMesh;
	TWeakObjectPtr<USkeletalMeshComponent> HeadMesh;
	TWeakObjectPtr<USkeletalMeshComponent> DriverBodyMesh;

	// Assembly lifecycle binding
	TWeakObjectPtr<UActorComponent> CachedAssemblyComponent;
	FDelegateHandle AssemblyStateHandle;

	// Timers
	FTimerHandle LocalControlRetryHandle;

	// Retry tracking
	int32 LocalControlRetryCount = 0;
	static constexpr int32 MaxLocalControlRetries = 30;

	bool bInitialized = false;
	bool bVisibilityApplied = false;
};
