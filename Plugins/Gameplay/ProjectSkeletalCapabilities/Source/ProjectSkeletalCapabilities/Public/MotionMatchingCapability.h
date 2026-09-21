// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "Interfaces/AssemblyTypes.h"
#include "MotionMatchingCapability.generated.h"

class IAssemblyCapability;
class UAnimInstance;
class USkeletalMeshComponent;

/**
 * Motion Matching adapter for the skeletal assembly framework.
 *
 * Installs the PostProcess AnimBP bridge on DriverBody that injects CMC data
 *    into the primary ABP's CharacterProperties via UE reflection.
 *    The bridge is a BP AnimBP (ABP_MotionMatchingBridge) with a pass-through
 *    AnimGraph that preserves the primary ABP's pose unchanged.
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
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void OnAssemblyStateChanged(EAssemblyState NewState);
	void InitializeMotionMatching();

	// PostProcess bridge lifecycle
	bool TryInstallPostProcessBridge();
	void RetryBridgeInstall();

	USkeletalMeshComponent* FindMeshByRole(const TCHAR* RoleName) const;

	// PostProcess bridge AnimBP class (set from Hero.json properties)
	UPROPERTY(EditAnywhere, Category = "MotionMatching")
	TSoftClassPtr<UAnimInstance> BridgeAnimBPPath;

	TWeakObjectPtr<UActorComponent> CachedAssemblyComponent;

	FDelegateHandle AssemblyStateHandle;
	FTimerHandle BridgeRetryHandle;

	static constexpr int32 MaxRetries = 10;
	int32 BridgeRetryCount = 0;

	bool bBridgeInstalled = false;
};
