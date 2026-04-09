// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "Interfaces/AssemblyTypes.h"
#include "LocalFirstPersonCapability.generated.h"

class USkeletalMeshComponent;
class UCustomizableObjectInstance;
class ULocalBodyAnimInstance;

/**
 * First-person body visibility capability for the skeletal assembly framework.
 *
 * Handles owner-only visibility concerns that must be re-applied after Mutable
 * rebuilds and cannot be set once at spawn time:
 *
 * - Hide head/neck bones on the LocalBody mesh (owner sees headless body)
 * - Hide Head mesh and Groom components from owner (with CastHiddenShadow)
 * - Set Head mesh LeaderPoseComponent to the active visual body source
 *   (prefer WorldBody, fall back to DriverBody only during early init)
 * - Re-apply all of the above after every Mutable COI rebuild
 *
 * Capability ID: "LocalFirstPerson"
 * Scope: per-mesh (attached to LocalBody mesh component)
 *
 * ## Timing
 *
 * Self-managed via assembly state delegate. On assembly Ready:
 * - Discovers LocalBody, WorldBody, Head, and Groom components via role tags
 * - Applies visibility policy
 * - Attempts to bind to Mutable COI for rebuild re-application
 * - If MutableCustomization hasn't created CSKs yet (delegate ordering),
 *   retries COI binding on next frame
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

	// Try to bind to Mutable COI. Returns true if bound, false if no CSKs found.
	bool TryBindMutableCOI();

	// Deferred retry: if COI binding fails at init, try again next frame
	void RetryMutableBinding();

	// Apply full first-person visibility policy
	void ApplyVisibility();

	// Assembly state callback
	void OnAssemblyStateChanged(EAssemblyState NewState);

	// Mutable rebuild callback -- re-apply visibility after mesh regeneration
	void OnMutableInstanceUpdated(UCustomizableObjectInstance* Instance);

	// Retry visibility after short delay (groom components attach asynchronously)
	void RetryVisibility();

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

	// Mutable rebuild binding
	TWeakObjectPtr<UCustomizableObjectInstance> BoundMutableInstance;

	// Timers
	FTimerHandle GroomRetryTimerHandle;
	FTimerHandle MutableBindRetryHandle;
	FTimerHandle LocalControlRetryHandle;

	// Retry tracking
	int32 MutableBindRetryCount = 0;
	static constexpr int32 MaxMutableBindRetries = 10;
	int32 LocalControlRetryCount = 0;
	static constexpr int32 MaxLocalControlRetries = 30;

	bool bInitialized = false;
	bool bVisibilityApplied = false;
};
