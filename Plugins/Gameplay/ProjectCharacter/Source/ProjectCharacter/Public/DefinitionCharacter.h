// Copyright ALIS. All Rights Reserved.
// Data-driven character spawned from UObjectDefinition via ObjectSpawnUtility.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "DefinitionCharacter.generated.h"

class UCameraComponent;
class UAbilitySystemComponent;
class UHealthAttributeSet;
class USurvivalAttributeSet;
class UStaminaAttributeSet;
class UStatusAttributeSet;
class UProjectAbilitySet;
struct FProjectAbilitySetHandles;
class UProjectVitalsComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
struct FOnAttributeChangeData;
enum class EAssemblyState : uint8;

DECLARE_LOG_CATEGORY_EXTERN(LogDefinitionCharacter, Log, All);

/**
 * Generic data-driven character class.
 *
 * Spawned by ObjectSpawnUtility::SpawnFromDefinition() from a UObjectDefinition.
 * NO hardcoded mesh subobjects -- meshes, capabilities, and assembly lifecycle
 * come from the definition. Character owns only the player pawn contract:
 * GAS, camera, input, movement, vitals.
 *
 * View section (camera offset) is applied when the assembly reaches Ready state
 * via OnAssemblyStateChanged delegate.
 */
UCLASS(config = Game)
class PROJECTCHARACTER_API ADefinitionCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	// -------------------------------------------------------------------------
	// GAS (Gameplay Ability System)
	// -------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY()
	UHealthAttributeSet* HealthAttributes;

	UPROPERTY()
	USurvivalAttributeSet* SurvivalAttributes;

	UPROPERTY()
	UStaminaAttributeSet* StaminaAttributes;

	UPROPERTY()
	UStatusAttributeSet* StatusAttributes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UProjectAbilitySet>> StartupAbilitySets;

	TArray<TSharedPtr<FProjectAbilitySetHandles>> StartupAbilitySetHandles;

	UPROPERTY(Transient)
	bool bStartupSetsGranted = false;

	// -------------------------------------------------------------------------
	// Vitals
	// -------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (AllowPrivateAccess = "true"))
	UProjectVitalsComponent* VitalsComponent;

	// -------------------------------------------------------------------------
	// Camera
	// -------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCamera;

	// -------------------------------------------------------------------------
	// Input (created programmatically -- no Blueprint required)
	// -------------------------------------------------------------------------

	UInputMappingContext* DefaultMappingContext;
	UInputAction* JumpAction;
	UInputAction* MoveAction;
	UInputAction* LookAction;
	UInputAction* SprintAction;
	UInputAction* CrouchAction;
	UInputAction* WalkAction;

	// -------------------------------------------------------------------------
	// Movement
	// -------------------------------------------------------------------------

	// Match legacy BP_Hero runtime values (Run gait forward = RunSpeeds[0]=500)
	float WalkSpeed = 200.0f;
	float RunSpeed = 500.0f;
	float SprintSpeed = 700.0f;
	float CrouchSpeed = 225.0f;

	bool bIsSprinting = false;
	bool bIsWalking = false;
	double LastWalkToggleTime = 0.0;
	bool bMovementSpeedBound = false;

public:
	ADefinitionCharacter();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	FORCEINLINE UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

private:
	// Input
	void CreateInputAssets();
	void RemoveDefaultInputMappingContext();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void ToggleWalk();
	void StartCrouch();
	void RefreshMovementSpeed();
	float GetMovementSpeedMultiplier() const;
	void BindMovementSpeedAttribute();
	void UnbindMovementSpeedAttribute();
	void OnMovementSpeedMultiplierChanged(const FOnAttributeChangeData& Data);
	void UpdateRotationPolicy();

	// GAS
	void GiveStartupAbilitySets();
	void RevokeStartupAbilitySets();

	// Assembly lifecycle (via ProjectCore interfaces)
	void OnAssemblyStateChanged(EAssemblyState NewState);
	void ApplyViewConfig();

	/** Cached lifecycle provider for delegate unbinding. */
	TWeakObjectPtr<UActorComponent> CachedAssemblyComponent;
	FDelegateHandle AssemblyStateHandle;

	/** Cached data source (may be separate from lifecycle provider). */
	TWeakObjectPtr<UActorComponent> CachedDataSourceComponent;
};
