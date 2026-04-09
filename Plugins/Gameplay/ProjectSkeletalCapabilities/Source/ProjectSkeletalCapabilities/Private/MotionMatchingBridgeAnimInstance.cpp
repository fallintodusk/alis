// Copyright ALIS. All Rights Reserved.

#include "MotionMatchingBridgeAnimInstance.h"
#include "ProjectSkeletalCapabilitiesModule.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "ReflectionWriteHelpers.h"

using namespace SkeletalCapabilities;


// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void UMotionMatchingBridgeAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	USkeletalMeshComponent* SkelComp = GetSkelMeshComponent();
	if (!SkelComp)
	{
		return;
	}

	UAnimInstance* Primary = SkelComp->GetAnimInstance();
	if (Primary && Primary != this)
	{
		CachePropertyPointers(Primary);
	}
}

bool UMotionMatchingBridgeAnimInstance::CachePropertyPointers(UAnimInstance* PrimaryInstance)
{
	bCacheResolved = false;
	CachedPrimaryInstance = PrimaryInstance;
	Fields = FCachedFields();
	CachedCharPropsProp = nullptr;
	CachedThreadSafeProp = nullptr;

	if (!PrimaryInstance)
	{
		return false;
	}

	UClass* PrimaryClass = PrimaryInstance->GetClass();

	// Resolve CharacterProperties struct property
	FProperty* RawProp = PrimaryClass->FindPropertyByName(TEXT("CharacterProperties"));
	CachedCharPropsProp = CastField<FStructProperty>(RawProp);

	if (!CachedCharPropsProp)
	{
		// Log (not Warning): expected when anim mode switches to SingleNode or non-MM ABP
		UE_LOG(LogProjectSkeletalCapabilities, Log,
			TEXT("[MotionMatchingBridge] CharacterProperties not found on '%s' (expected for non-MM AnimInstance)"),
			*PrimaryClass->GetName());
		return false;
	}

	const UScriptStruct* Struct = CachedCharPropsProp->Struct;

	// Cache sub-properties
	Fields.MovementMode = FindPropWithFallback(Struct, TEXT("MovementMode"));
	Fields.Stance = FindPropWithFallback(Struct, TEXT("Stance"));
	Fields.RotationMode = FindPropWithFallback(Struct, TEXT("RotationMode"));
	Fields.Gait = FindPropWithFallback(Struct, TEXT("Gait"));
	Fields.ActorTransform = FindPropWithFallback(Struct, TEXT("ActorTransform"));
	Fields.Velocity = FindPropWithFallback(Struct, TEXT("Velocity"));
	Fields.InputAcceleration = FindPropWithFallback(Struct, TEXT("InputAcceleration"));
	Fields.CurrentMaxAcceleration = FindPropWithFallback(Struct, TEXT("CurrentMaxAcceleration"));
	Fields.CurrentMaxDeceleration = FindPropWithFallback(Struct, TEXT("CurrentMaxDeceleration"));
	Fields.OrientationIntent = FindPropWithFallback(Struct, TEXT("OrientationIntent"));
	Fields.AimingRotation = FindPropWithFallback(Struct, TEXT("AimingRotation"));
	Fields.JustLanded = FindPropWithFallback(Struct, TEXT("JustLanded"));
	Fields.LandVelocity = FindPropWithFallback(Struct, TEXT("LandVelocity"));
	Fields.GroundNormal = FindPropWithFallback(Struct, TEXT("GroundNormal"));
	Fields.InputState = FindPropWithFallback(Struct, TEXT("InputState"));
	if (FStructProperty* InputStateProp = CastField<FStructProperty>(Fields.InputState))
	{
		const UScriptStruct* InputStateStruct = InputStateProp->Struct;
		Fields.WantsToSprint = FindPropWithFallback(InputStateStruct, TEXT("WantsToSprint"));
		Fields.WantsToWalk = FindPropWithFallback(InputStateStruct, TEXT("WantsToWalk"));
		Fields.WantsToStrafe = FindPropWithFallback(InputStateStruct, TEXT("WantsToStrafe"));
		Fields.WantsToAim = FindPropWithFallback(InputStateStruct, TEXT("WantsToAim"));
		Fields.WantsToCrouch = FindPropWithFallback(InputStateStruct, TEXT("WantsToCrouch"));
	}

	// Resolve UseThreadSafeUpdateAnimation (may be suffixed in BP)
	FProperty* ThreadSafeRaw = FindPropWithFallback(PrimaryClass, TEXT("UseThreadSafeUpdateAnimation"));
	CachedThreadSafeProp = CastField<FBoolProperty>(ThreadSafeRaw);

	bCacheResolved = Fields.IsValid();

	int32 ResolvedCount = 0;
	const FProperty* const FieldPtrs[] = {
		Fields.MovementMode, Fields.Stance, Fields.RotationMode, Fields.Gait,
		Fields.ActorTransform, Fields.Velocity, Fields.InputAcceleration,
		Fields.CurrentMaxAcceleration, Fields.CurrentMaxDeceleration,
		Fields.OrientationIntent, Fields.AimingRotation, Fields.JustLanded,
		Fields.LandVelocity, Fields.GroundNormal, Fields.InputState
	};
	for (const FProperty* P : FieldPtrs)
	{
		if (P) ++ResolvedCount;
	}

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[MotionMatchingBridge] Resolved %d/15 CharacterProperties fields on '%s' (ThreadSafe=%s)"),
		ResolvedCount,
		*PrimaryInstance->GetClass()->GetName(),
		CachedThreadSafeProp ? TEXT("found") : TEXT("missing"));

	return bCacheResolved;
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

void UMotionMatchingBridgeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	USkeletalMeshComponent* SkelComp = GetSkelMeshComponent();
	if (!SkelComp)
	{
		return;
	}

	UAnimInstance* Primary = SkelComp->GetAnimInstance();
	if (!Primary || Primary == this)
	{
		return;
	}

	// Re-cache if primary instance was recreated (e.g. after Mutable rebuild)
	if (!bCacheResolved || !CachedPrimaryInstance.IsValid() || CachedPrimaryInstance.Get() != Primary)
	{
		if (!CachePropertyPointers(Primary))
		{
			return;
		}
	}

	APawn* Pawn = TryGetPawnOwner();
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (!CMC)
	{
		return;
	}

	ForceThreadSafeFlag(Primary);
	WriteCharacterProperties(Primary, Character, CMC);

}

// ---------------------------------------------------------------------------
// Property writing
// ---------------------------------------------------------------------------

void UMotionMatchingBridgeAnimInstance::ForceThreadSafeFlag(UAnimInstance* PrimaryInstance)
{
	if (CachedThreadSafeProp)
	{
		CachedThreadSafeProp->SetPropertyValue(
			CachedThreadSafeProp->ContainerPtrToValuePtr<void>(PrimaryInstance), true);
	}
}

void UMotionMatchingBridgeAnimInstance::WriteCharacterProperties(
	UAnimInstance* PrimaryInstance,
	ACharacter* Character,
	UCharacterMovementComponent* CMC)
{
	uint8* StructBase = CachedCharPropsProp->ContainerPtrToValuePtr<uint8>(PrimaryInstance);

	// MovementMode: GASP ABP custom enum (NOT UE's EMovementMode).
	// Legacy BP_Hero ABP dump shows 0=OnGround while moving grounded.
	// Previous mapping (4=Grounded,5=InAir) was WRONG -- caused frozen locomotion.
	{
		const EMovementMode Mode = CMC->MovementMode;
		const uint8 MappedMode = (Mode == MOVE_Falling || Mode == MOVE_Swimming) ? 1 : 0;
		WriteByteField(Fields.MovementMode, StructBase, MappedMode);
	}

	// Stance: Standing=0, Crouching=1
	WriteByteField(Fields.Stance, StructBase, CMC->IsCrouching() ? 1 : 0);

	// Gait: Run=1 for v1 (BP_Hero uses GetDesiredGait with gait curves)
	WriteByteField(Fields.Gait, StructBase, 1);

	// Transform, vectors, rotators
	WriteTransformField(Fields.ActorTransform, StructBase, Character->GetActorTransform());
	WriteVectorField(Fields.Velocity, StructBase, CMC->Velocity);
	WriteVectorField(Fields.InputAcceleration, StructBase, CMC->GetCurrentAcceleration());
	WriteFloatField(Fields.CurrentMaxAcceleration, StructBase, CMC->GetMaxAcceleration());
	WriteFloatField(Fields.CurrentMaxDeceleration, StructBase, CMC->BrakingDecelerationWalking);

	// Current first-person baseline:
	// - Match the legacy BP_Hero / sample CMC contract: Strafe rotation mode.
	// - Actor rotation is the facing target, while full control rotation keeps
	//   the camera/aiming signal for AO and turn gates.
	// - The sample turn-in-place gate also expects aiming/strafe intent, so the
	//   bridged InputState must not stay zeroed.
	{
		const FRotator AimRot = Character->IsLocallyControlled()
			? Character->GetControlRotation()
			: Character->GetBaseAimRotation();

		WriteByteField(Fields.RotationMode, StructBase, 1);
		WriteRotatorField(Fields.OrientationIntent, StructBase, Character->GetActorRotation());
		WriteRotatorField(Fields.AimingRotation, StructBase, AimRot);
	}

	// Landing state: v1 defaults (no gait event tracking)
	WriteBoolField(Fields.JustLanded, StructBase, false);
	WriteVectorField(Fields.LandVelocity, StructBase, FVector::ZeroVector);

	// GroundNormal from current floor hit
	WriteVectorField(Fields.GroundNormal, StructBase,
		CMC->CurrentFloor.HitResult.ImpactNormal);

	// InputState: keep the sample turn/strafe gates alive for the first-person
	// baseline. View/input policy can own these booleans later, but leaving the
	// struct zeroed prevents turn-in-place from ever arming.
	if (Fields.InputState)
	{
		if (FStructProperty* InputStructProp = CastField<FStructProperty>(Fields.InputState))
		{
			void* InputStatePtr = Fields.InputState->ContainerPtrToValuePtr<void>(StructBase);
			InputStructProp->Struct->InitializeStruct(InputStatePtr);
			WriteBoolField(Fields.WantsToStrafe, static_cast<uint8*>(InputStatePtr), true);
			WriteBoolField(Fields.WantsToAim, static_cast<uint8*>(InputStatePtr), true);
			WriteBoolField(Fields.WantsToCrouch, static_cast<uint8*>(InputStatePtr), CMC->IsCrouching());
		}
	}
}
