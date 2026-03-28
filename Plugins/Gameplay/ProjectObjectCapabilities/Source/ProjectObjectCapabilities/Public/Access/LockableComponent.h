// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/IProjectActionReceiver.h"
#include "Sound/SoundBase.h"
#include "LockableComponent.generated.h"

/**
 * Reusable lock/unlock capability for world objects (doors, chests, gates).
 *
 * Uses FGameplayTag for key matching. When interacted:
 * - If locked, blocks interaction and broadcasts OnAccessDenied (UI feedback)
 * - If unlocked (or no lock), allows interaction to continue
 *
 * Priority: 100 (runs before action components like SpringRotator)
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API ULockableComponent : public UActorComponent, public IInteractableComponentTargetInterface, public IProjectActionReceiver
{
	GENERATED_BODY()

public:
	ULockableComponent();

	// --- Stable ID for Capability Registry ---
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// --- IInteractableComponentTargetInterface ---
	virtual int32 GetInteractPriority_Implementation() const override { return 100; }
	virtual bool OnComponentInteract_Implementation(AActor* Instigator) override;
	virtual FInteractionPrompt GetInteractionPrompt_Implementation(AActor* Instigator) const override;
	virtual FText GetInteractionLabel_Implementation() const override;
	virtual FInteractionExecutionSpec GetInteractionExecutionSpec_Implementation(AActor* Instigator) const override;
	virtual UPrimitiveComponent* GetInteractTargetMesh_Implementation() const override;
	virtual void SetInteractTargetMesh_Implementation(UPrimitiveComponent* TargetMesh) override;

	// --- Designer Config ---

	/** Required tag to unlock. Can be item tag, ability tag, etc. Empty = unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Access")
	FGameplayTag LockTag;

	/** Whether key is consumed when used to unlock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Access")
	bool bConsumeKeyOnUnlock = false;

	// --- Sound ---

	/** Sound played when unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Access|Sound")
	TObjectPtr<USoundBase> UnlockSound;

	/** Sound played when access denied (locked, no key). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Access|Sound")
	TObjectPtr<USoundBase> AccessDeniedSound;

	// --- Runtime State ---

	/** Has been unlocked (persists even if LockTag is set). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Access")
	bool bIsUnlocked = false;

	// --- Public API ---

	/** Check if currently locked (has LockTag and not yet unlocked). */
	UFUNCTION(BlueprintPure, Category = "Access")
	bool IsLocked() const;

	/** Get the required key tag. */
	UFUNCTION(BlueprintPure, Category = "Access")
	FGameplayTag GetLockTag() const { return LockTag; }

	/** Check if key should be consumed on unlock. */
	UFUNCTION(BlueprintPure, Category = "Access")
	bool ShouldConsumeKey() const { return bConsumeKeyOnUnlock; }

	/** Unlock this object. Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Access")
	void Unlock();

	/** Lock this object (reset to locked state). Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Access")
	void Lock();

	// --- IProjectActionReceiver ---
	virtual void HandleAction(const FString& Context, const FString& Action) override;

	// --- Delegates ---

	/** Called when access is denied (locked, no key). UI can subscribe for feedback. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAccessDenied, FGameplayTag, RequiredKeyTag, AActor*, Instigator);

	UPROPERTY(BlueprintAssignable, Category = "Access")
	FOnAccessDenied OnAccessDenied;

	/** Called when unlocked. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUnlocked);

	UPROPERTY(BlueprintAssignable, Category = "Access")
	FOnUnlocked OnUnlocked;

	/** Called when locked. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLocked);

	UPROPERTY(BlueprintAssignable, Category = "Access")
	FOnLocked OnLocked;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/**
	 * Optional mesh scope binding for strict interaction selection.
	 * Must persist across editor world -> PIE duplication; do not mark Transient.
	 */
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> InteractTargetMesh = nullptr;
};
