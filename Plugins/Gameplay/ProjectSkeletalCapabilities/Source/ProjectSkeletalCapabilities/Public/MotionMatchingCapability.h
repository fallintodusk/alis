// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "Interfaces/AssemblyTypes.h"
#include "MotionMatchingCapability.generated.h"

class IAssemblyCapability;
class UCustomizableObjectInstance;
class USkeletalMeshComponent;

/**
 * Motion Matching adapter for the skeletal assembly framework.
 *
 * Two responsibilities:
 * 1. Install PostProcess AnimBP bridge on DriverBody that injects CMC data
 *    into the primary ABP's CharacterProperties via UE reflection.
 *    The bridge is a BP AnimBP (ABP_MotionMatchingBridge) with a pass-through
 *    AnimGraph that preserves the primary ABP's pose unchanged.
 * 2. Wire LeaderPose from DriverBody to Mutable CSK output meshes
 *    (BodyCustomization, HeadCustomization) after each Mutable rebuild.
 *
 * Capability ID: "MotionMatching"
 * Scope: per-mesh (targets DriverBody)
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTSKELETALCAPABILITIES_API UMotionMatchingCapability : public UActorComponent
{
	GENERATED_BODY()

public:
	UMotionMatchingCapability();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void OnAssemblyStateChanged(EAssemblyState NewState);
	void InitializeMotionMatching();

	// PostProcess bridge lifecycle
	bool TryInstallPostProcessBridge();
	void RetryBridgeInstall();

	// LeaderPose wiring after Mutable rebuild
	void WireLeaderPoseChain();
	void OnMutableInstanceUpdated(UCustomizableObjectInstance* Instance);
	bool TryBindMutableDelegate();
	void RetryMutableBind();

	USkeletalMeshComponent* FindMeshByRole(const TCHAR* RoleName) const;

	// PostProcess bridge AnimBP class path (set from Hero.json properties)
	UPROPERTY(EditAnywhere, Category = "MotionMatching")
	FString BridgeAnimBPPath;

	TWeakObjectPtr<UActorComponent> CachedAssemblyComponent;
	TWeakObjectPtr<USkeletalMeshComponent> CachedDriverBody;

	UPROPERTY()
	TObjectPtr<UCustomizableObjectInstance> BoundMutableInstance;

	FDelegateHandle AssemblyStateHandle;
	FTimerHandle BridgeRetryHandle;
	FTimerHandle MutableBindRetryHandle;

	static constexpr int32 MaxRetries = 10;
	int32 BridgeRetryCount = 0;
	int32 MutableBindRetryCount = 0;

	bool bBridgeInstalled = false;
	bool bMutableBound = false;
};
