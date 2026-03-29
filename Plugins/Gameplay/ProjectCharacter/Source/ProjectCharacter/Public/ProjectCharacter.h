// Based on UE5 Third Person Template Character
// Adapted for first-person camera with visible body

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Logging/LogMacros.h"
#include "ProjectCharacter.generated.h"

class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
struct FOnAttributeChangeData;
class UAbilitySystemComponent;
class UHealthAttributeSet;
class USurvivalAttributeSet;
class UStaminaAttributeSet;
class UStatusAttributeSet;
class UProjectAbilitySet;
struct FProjectAbilitySetHandles;
class UProjectVitalsComponent;
class UCustomizableObjectInstance;

DECLARE_LOG_CATEGORY_EXTERN(LogProjectCharacter, Log, All);

UCLASS(config=Game)
class PROJECTCHARACTER_API AProjectCharacter : public ACharacter, public IAbilitySystemInterface
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

	/** Ability sets granted on possession (configured in Blueprint or defaults) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UProjectAbilitySet>> StartupAbilitySets;

	/** Handles for granted startup abilities (for cleanup) */
	TArray<TSharedPtr<FProjectAbilitySetHandles>> StartupAbilitySetHandles;

	/** Guard against duplicate startup set grants (PossessedBy can run multiple times) */
	UPROPERTY(Transient)
	bool bStartupSetsGranted = false;

	// -------------------------------------------------------------------------
	// Vitals (metabolism, threshold states, condition regen/drain)
	// -------------------------------------------------------------------------

	/** Server-side vitals tick: metabolism, State.* tags, threshold debuffs, condition regen/drain */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (AllowPrivateAccess = "true"))
	UProjectVitalsComponent* VitalsComponent;

	// -------------------------------------------------------------------------
	// Camera
	// -------------------------------------------------------------------------

	/** First-person camera at eye level */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FirstPerson", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* WorldBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FirstPerson", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* LocalBodyMesh;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	/** Sprint Input Action (Left Shift) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;

	/** Crouch Input Action (Left Ctrl) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

	// -------------------------------------------------------------------------
	// Movement Speed Configuration
	// -------------------------------------------------------------------------
	// Human locomotion reference values (source: biomechanics research):
	// - Comfortable walk: 4.5-5.5 km/h (125-153 cm/s)
	// - Brisk walk: 6-7 km/h (167-194 cm/s)
	// - Jog: 8-10 km/h (222-278 cm/s)
	// - Run: 12-15 km/h (333-417 cm/s)
	// - Sprint (untrained): 20-25 km/h (556-694 cm/s)
	// - Sprint (trained): 30+ km/h (833+ cm/s)
	// -------------------------------------------------------------------------

	/** Walking speed - default movement without sprint (brisk walk ~6.5 km/h) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Speed", meta = (AllowPrivateAccess = "true"))
	float WalkSpeed = 180.0f;

	/** Running speed - when sprint key held (~15 km/h jogging pace) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Speed", meta = (AllowPrivateAccess = "true"))
	float RunSpeed = 420.0f;

	/** Crouched movement speed - slower for stealth (~4 km/h) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Speed", meta = (AllowPrivateAccess = "true"))
	float CrouchSpeed = 110.0f;

	/** Current sprint state (used for speed calculations) */
	bool bIsSprinting = false;

	/** Guard against double-binding to attribute delegate */
	bool bMovementSpeedBound = false;

	// Deferred re-apply visibility after Mutable update — Groom components appear asynchronously
	FTimerHandle GroomRetryTimerHandle;

public:
	AProjectCharacter();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	/** Called for movement input (Enhanced Input) */
	void Move(const FInputActionValue& Value);

	/** Called for looking input (Enhanced Input) */
	void Look(const FInputActionValue& Value);

	/** Legacy input - move forward/backward */
	void MoveForward(float Value);

	/** Legacy input - move right/left */
	void MoveRight(float Value);

	/** Called when sprint key pressed - switches to RunSpeed */
	void StartSprint();

	/** Called when sprint key released - returns to WalkSpeed */
	void StopSprint();

	/** Refresh movement speed based on current state (walk/sprint) and attribute modifiers */
	void RefreshMovementSpeed();

	/** Get movement speed multiplier from StatusAttributeSet */
	float GetMovementSpeedMultiplier() const;

	/** Bind to MovementSpeedMultiplier attribute changes */
	void BindMovementSpeedAttribute();

	/** Unbind from attribute changes */
	void UnbindMovementSpeedAttribute();

	/** Called when MovementSpeedMultiplier attribute changes */
	void OnMovementSpeedMultiplierChanged(const FOnAttributeChangeData& Data);

	/** Called when crouch key pressed */
	void StartCrouch();

	/** Called when crouch key released */
	void StopCrouch();

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	/** Creates default Enhanced Input assets if not configured */
	void CreateDefaultInputAssets();

	// Re-applies visibility flags (Mutable resets them on mesh regeneration)
	void ApplyFirstPersonVisibility();

	/** Mutable callback — re-apply visibility after mesh rebuild */
	void OnMutableMeshUpdated(UCustomizableObjectInstance* Instance);

	/** Grant all startup ability sets to ASC (called on server in PossessedBy) */
	void GiveStartupAbilitySets();

	/** Revoke all granted startup ability sets (called on unpossess/endplay) */
	void RevokeStartupAbilitySets();

public:
	/** Returns FirstPersonCamera subobject */
	FORCEINLINE class UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	/** WHY exposed: LocalBodyAnimInstance needs WorldBodyMesh as CopyPose source */
	FORCEINLINE class USkeletalMeshComponent* GetWorldBodyMesh() const { return WorldBodyMesh; }
};
