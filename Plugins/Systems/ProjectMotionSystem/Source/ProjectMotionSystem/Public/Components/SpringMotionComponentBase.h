// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/IProjectActionReceiver.h"
#include "Sound/SoundBase.h"
#include "SpringMotionComponentBase.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EMotionMode : uint8
{
	Kinematic UMETA(DisplayName = "Kinematic"),
	Chaos UMETA(DisplayName = "Chaos")
};

/**
 * Base class for spring-driven motion components (rotators, sliders).
 *
 * Provides common functionality:
 * - Mesh resolution (auto-find or explicit)
 * - IInteractableComponentTargetInterface implementation
 * - Sound playback helpers
 * - Motion mode selection
 *
 * Subclasses implement motion-specific logic (angular vs linear).
 */
UCLASS(Abstract)
class PROJECTMOTIONSYSTEM_API USpringMotionComponentBase : public UActorComponent, public IInteractableComponentTargetInterface, public IProjectActionReceiver
{
	GENERATED_BODY()

public:
	USpringMotionComponentBase();

	// --- IInteractableComponentTargetInterface ---
	virtual int32 GetInteractPriority_Implementation() const override { return 0; }
	virtual bool OnComponentInteract_Implementation(AActor* Instigator) override;
	virtual FText GetInteractionLabel_Implementation() const override;
	virtual UPrimitiveComponent* GetInteractTargetMesh_Implementation() const override;
	virtual void SetInteractTargetMesh_Implementation(UPrimitiveComponent* TargetMesh) override;

	// --- Common Designer Config ---

	/** Optional explicit mesh reference. If null, auto-finds StaticMeshComponent on owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	/** Sound played when opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Sound")
	TObjectPtr<USoundBase> OpenSound;

	/** Sound played when closing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Sound")
	TObjectPtr<USoundBase> CloseSound;

	/**
	 * Motion mode selection (default: Kinematic).
	 * Kinematic uses collision-aware motion without Chaos constraints.
	 * Chaos uses PhysicsConstraintComponent (experimental).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	EMotionMode MotionMode = EMotionMode::Kinematic;

	/**
	 * Legacy override to force Chaos constraints.
	 * Avoid in new content; use MotionMode instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics", meta = (DisplayName = "Force Chaos Mode (Deprecated)"))
	bool bForceChaosMode = false;

	// --- Runtime State ---

	/** Is currently in open position. */
	UPROPERTY(BlueprintReadOnly, Category = "Motion")
	bool bIsOpen = false;

	/** Is currently animating (kinematic mode). */
	UPROPERTY(BlueprintReadOnly, Category = "Motion")
	bool bIsAnimating = false;

	// --- Public API ---

	/** Open the component. */
	UFUNCTION(BlueprintCallable, Category = "Motion")
	virtual void Open();

	/** Close the component. */
	UFUNCTION(BlueprintCallable, Category = "Motion")
	virtual void Close();

	/** Toggle between open and closed. */
	UFUNCTION(BlueprintCallable, Category = "Motion")
	virtual void Toggle();

	// --- IProjectActionReceiver ---
	virtual void HandleAction(const FString& Context, const FString& Action) override;

	// --- Static Helpers ---

	/**
	 * Parse FVector from string. Supports UE format (X=1 Y=2 Z=3) and tuple format (1,2,3).
	 * @return true if parsing succeeded
	 */
	static bool ParseVector(const FString& Value, FVector& OutVec);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	// --- Resolved References ---

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ResolvedMesh;

	// --- Motion State ---

	bool bMotorActive = false;

	/** Desired open state for toggle reversal during animation/motor active. */
	bool bDesiredOpen = false;

	// --- Common Methods ---

	/** Resolve TargetMesh or auto-find StaticMeshComponent on owner. */
	void ResolveMeshReference();

	/** Play open sound at owner location. */
	void PlayOpenSound();

	/** Play close sound at owner location. */
	void PlayCloseSound();

	// --- Subclass Implementation Points ---

	/** Check if Chaos constraint mode should be used. */
	virtual bool ShouldUseChaosMode() const
	{
		return MotionMode == EMotionMode::Chaos || bForceChaosMode;
	}

	/** Tick active motion. Return true to keep ticking. */
	virtual bool TickMotion(float DeltaTime) PURE_VIRTUAL(USpringMotionComponentBase::TickMotion, return false;);

	/** Called when motion completes (after sleep detection or kinematic settle). */
	virtual void OnMotionComplete(bool bOpened) {}

	/** Perform open action (set targets, enable motor). */
	virtual void DoOpen() PURE_VIRTUAL(USpringMotionComponentBase::DoOpen, return;);

	/** Perform close action (set targets, enable motor). */
	virtual void DoClose() PURE_VIRTUAL(USpringMotionComponentBase::DoClose, return;);

	/** Driver shutdown hook for EndPlay. */
	virtual void ShutdownMotionDriver() PURE_VIRTUAL(USpringMotionComponentBase::ShutdownMotionDriver, return;);
};
