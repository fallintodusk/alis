// Based on UE5 Third Person Template Character
// Adapted for first-person camera with visible body

#include "ProjectCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "AbilitySystemComponent.h"
#include "Attributes/HealthAttributeSet.h"
#include "Attributes/SurvivalAttributeSet.h"
#include "Attributes/StaminaAttributeSet.h"
#include "Attributes/StatusAttributeSet.h"
#include "Abilities/ProjectAbilitySet.h"
#include "ProjectVitalsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "GroomComponent.h"
#include "LocalBodyAnimInstance.h"  // CopyPose + spine lock AnimInstance for LocalBody

DEFINE_LOG_CATEGORY(LogProjectCharacter);

//////////////////////////////////////////////////////////////////////////
// AProjectCharacter

AProjectCharacter::AProjectCharacter()
{
	// Tick enabled for FP debug visualization (disable after debugging)
	PrimaryActorTick.bCanEverTick = false;

	// -------------------------------------------------------------------------
	// GAS Setup
	// -------------------------------------------------------------------------
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Player character gets all vital AttributeSets (see ProjectGAS README)
	HealthAttributes = CreateDefaultSubobject<UHealthAttributeSet>(TEXT("HealthAttributes"));
	SurvivalAttributes = CreateDefaultSubobject<USurvivalAttributeSet>(TEXT("SurvivalAttributes"));
	StaminaAttributes = CreateDefaultSubobject<UStaminaAttributeSet>(TEXT("StaminaAttributes"));
	StatusAttributes = CreateDefaultSubobject<UStatusAttributeSet>(TEXT("StatusAttributes"));

	// Vitals tick component (server-only: metabolism, thresholds, condition regen/drain)
	// See ProjectVitals README for design
	VitalsComponent = CreateDefaultSubobject<UProjectVitalsComponent>(TEXT("VitalsComponent"));

	// -------------------------------------------------------------------------
	// Collision
	// -------------------------------------------------------------------------
	// Medium human proportions: ~175cm tall, ~46cm body width
	// Radius 23 = half of 46cm torso width (not shoulders, arms move freely)
	// HalfHeight 88 = half of 176cm total height
	GetCapsuleComponent()->InitCapsuleSize(23.f, 88.0f);

	// FIRST-PERSON: Character rotates with controller view
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	// FIRST-PERSON: Don't rotate to movement direction (character faces where looking)
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// -------------------------------------------------------------------------
	// Movement Speeds - Realistic Human Locomotion
	// -------------------------------------------------------------------------
	// Reference: average adult male biomechanics
	// - Standing vertical jump: ~40-50cm (physics: v = sqrt(2*g*h) -> ~280-315 cm/s)
	// - We use 420 cm/s for ~90cm jump (athletic but not superhuman)
	// - Air control 0.35 allows minor course correction mid-air
	// -------------------------------------------------------------------------
	GetCharacterMovement()->JumpZVelocity = 420.f;
	GetCharacterMovement()->AirControl = 0.35f;

	// Default speed is WalkSpeed (brisk walk ~6.5 km/h = 180 cm/s)
	// Sprint key (Shift) temporarily increases to RunSpeed
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// -------------------------------------------------------------------------
	// Physics Interaction - Door/Drawer Blocking Behavior
	// -------------------------------------------------------------------------
	// Character can push light physics objects but resists being pushed.
	// This allows doors to block against player instead of pushing through.
	// -------------------------------------------------------------------------
	GetCharacterMovement()->bEnablePhysicsInteraction = true;
	GetCharacterMovement()->bPushForceScaledToMass = true;     // Push force scales with object mass
	GetCharacterMovement()->PushForceFactor = 50000.0f;        // Character can push physics objects
	GetCharacterMovement()->InitialPushForceFactor = 500.0f;   // Initial impact force
	GetCharacterMovement()->TouchForceFactor = 0.0f;           // Physics objects cannot push character
	GetCharacterMovement()->RepulsionForce = 2.0f;             // Resist overlap penetration

	// -------------------------------------------------------------------------
	// Crouch Configuration
	// -------------------------------------------------------------------------
	// Crouched capsule: ~120cm total height (half-height 60cm)
	// Speed reduction: stealth movement at ~4 km/h (110 cm/s)
	// -------------------------------------------------------------------------
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(60.0f);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;

	// Driver mesh: capsule bottom, face forward
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// FIRST-PERSON: Camera directly attached at eye level (no spring arm)
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(23.f, 0.f, 62.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->FieldOfView = 90.0f;

	// -------------------------------------------------------------------------
	// Layer 2a — world body: other players see full character + shadow
	// -------------------------------------------------------------------------
	WorldBodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WorldBodyMesh"));
	WorldBodyMesh->SetupAttachment(GetMesh());
	// Visual-only layer, physics handled by capsule
	WorldBodyMesh->SetCollisionProfileName(FName("NoCollision"));
	// CopyPose source for LocalBody — bones must be fresh every frame even when OwnerNoSee hides it
	WorldBodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// -------------------------------------------------------------------------
	// Layer 2b: Headless local body — owner sees this (no head geo to clip)
	// -------------------------------------------------------------------------
	// Shares animation from Layer 2a (CharacterMesh0) via LeaderPose.
	// Head geometry removed in ApplyFirstPersonVisibility().
	// Shadow comes from Layer 2a (world body), not this mesh.
	// -------------------------------------------------------------------------
	LocalBodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LocalBodyMesh"));
	LocalBodyMesh->SetupAttachment(GetMesh());
	LocalBodyMesh->SetLeaderPoseComponent(GetMesh());
	LocalBodyMesh->SetOnlyOwnerSee(true);
	LocalBodyMesh->SetCastShadow(false);
	LocalBodyMesh->SetCollisionProfileName(FName("NoCollision"));
	// HideBoneByName and CopyPose require fresh bone transforms every frame — AlwaysTickPose alone is insufficient
	LocalBodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character)
	// are set in the derived blueprint asset (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// GAS Initialization

void AProjectCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server: Initialize ASC actor info (Owner=this, Avatar=this for pawn-owned ASC)
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Bind to movement speed attribute now that ASC is ready
		BindMovementSpeedAttribute();

		// Grant startup abilities on server only
		if (HasAuthority())
		{
			GiveStartupAbilitySets();

			// Restart vitals on re-possession (after UnPossessed->Stop).
			// On first spawn, ASC::InitializeComponent hasn't run yet
			// (PossessedBy fires from PreInitializeComponents), so
			// SpawnedAttributes is empty. PostInitializeComponents handles
			// the first start after ASC discovers attribute sets.
			if (VitalsComponent && AbilitySystemComponent->HasBeenInitialized())
			{
				VitalsComponent->Start();
			}
		}
	}

	UE_LOG(LogProjectCharacter, Log, TEXT("PossessedBy: ASC initialized (Server)"));
}

void AProjectCharacter::GiveStartupAbilitySets()
{
	// Guard against duplicate grants (PossessedBy can run multiple times)
	if (!AbilitySystemComponent || bStartupSetsGranted)
	{
		return;
	}
	bStartupSetsGranted = true;

	for (UProjectAbilitySet* AbilitySet : StartupAbilitySets)
	{
		if (!AbilitySet)
		{
			continue;
		}

		TSharedPtr<FProjectAbilitySetHandles> Handles = MakeShared<FProjectAbilitySetHandles>();
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, Handles.Get());
		StartupAbilitySetHandles.Add(Handles);

		UE_LOG(LogProjectCharacter, Log, TEXT("Granted startup AbilitySet: %s"), *AbilitySet->GetName());
	}
}

void AProjectCharacter::RevokeStartupAbilitySets()
{
	// Only server grants abilities, so only server revokes
	if (!HasAuthority() || !AbilitySystemComponent || !bStartupSetsGranted)
	{
		return;
	}

	for (TSharedPtr<FProjectAbilitySetHandles>& Handles : StartupAbilitySetHandles)
	{
		if (Handles.IsValid())
		{
			UProjectAbilitySet::TakeFromAbilitySystem(AbilitySystemComponent, Handles.Get());
		}
	}
	StartupAbilitySetHandles.Empty();
	bStartupSetsGranted = false;

	UE_LOG(LogProjectCharacter, Log, TEXT("Revoked all startup AbilitySets"));
}

void AProjectCharacter::UnPossessed()
{
	// Stop vitals tick before revoking abilities
	if (VitalsComponent)
	{
		VitalsComponent->Stop();
	}

	RevokeStartupAbilitySets();
	Super::UnPossessed();
}

void AProjectCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind movement speed attribute
	UnbindMovementSpeedAttribute();

	// Stop vitals tick before cleanup
	if (VitalsComponent)
	{
		VitalsComponent->Stop();
	}

	RevokeStartupAbilitySets();
	Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////////
// PostInitializeComponents
// ASC::InitializeComponent() runs during Super call, populating SpawnedAttributes.
// This is the earliest safe point to write GAS attribute values.
// PossessedBy fires from PreInitializeComponents (before SpawnedAttributes exist).

void AProjectCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Start vitals on first spawn (server-only, bIsRunning guard prevents double-start)
	if (HasAuthority() && VitalsComponent)
	{
		VitalsComponent->Start();
	}
}

//////////////////////////////////////////////////////////////////////////
// BeginPlay

void AProjectCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Client: Initialize ASC actor info (idempotent - safe to call multiple times)
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	// Create default input assets if not configured via Blueprint/Editor
	CreateDefaultInputAssets();

	// Bind to MovementSpeedMultiplier attribute for movement speed changes
	BindMovementSpeedAttribute();

	// -------------------------------------------------------------------------
	// First-Person Visibility — Layer 2 split
	// -------------------------------------------------------------------------
	// Bind Mutable delegate — reapply visibility after mesh rebuild
	TArray<UCustomizableSkeletalComponent*> MutableComps;
	GetComponents<UCustomizableSkeletalComponent>(MutableComps);
	for (UCustomizableSkeletalComponent* Comp : MutableComps)
	{
		UCustomizableObjectInstance* Instance = Comp ? Comp->GetCustomizableObjectInstance() : nullptr;
		if (Instance)
		{
			Instance->UpdatedNativeDelegate.AddUObject(this, &AProjectCharacter::OnMutableMeshUpdated);
		}
	}

	// Apply immediately (in case Mutable already finished before BeginPlay)
	ApplyFirstPersonVisibility();
}

//////////////////////////////////////////////////////////////////////////

// Proxy is a member field — no heap allocation
FAnimInstanceProxy* ULocalBodyAnimInstance::CreateAnimInstanceProxy()
{
	return &LocalBodyProxy;
}

void ULocalBodyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) {}

void AProjectCharacter::CreateDefaultInputAssets()
{
	// Only create if not already set
	if (!DefaultMappingContext)
	{
		DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("DefaultMappingContext"));
	}

	if (!MoveAction)
	{
		MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
		MoveAction->ValueType = EInputActionValueType::Axis2D;

		// Add to mapping context: WASD
		FEnhancedActionKeyMapping& MoveForwardMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
		MoveForwardMapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));

		FEnhancedActionKeyMapping& MoveBackMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
		MoveBackMapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		MoveBackMapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));

		FEnhancedActionKeyMapping& MoveRightMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::D);
		FEnhancedActionKeyMapping& MoveLeftMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
		MoveLeftMapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}

	if (!LookAction)
	{
		LookAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
		LookAction->ValueType = EInputActionValueType::Axis2D;

		// Add to mapping context: Mouse XY
		FEnhancedActionKeyMapping& LookMapping = DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
		// Negate Y for proper pitch (look up when mouse moves up)
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(this);
		NegateY->bX = false;
		NegateY->bY = true;
		NegateY->bZ = false;
		LookMapping.Modifiers.Add(NegateY);
	}

	if (!JumpAction)
	{
		JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
		JumpAction->ValueType = EInputActionValueType::Boolean;

		// Add to mapping context: Space
		DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	}

	// Sprint: Left Shift - hold to run faster
	if (!SprintAction)
	{
		SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint"));
		SprintAction->ValueType = EInputActionValueType::Boolean;

		DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	}

	// Crouch: Left Ctrl - hold to crouch (lower profile, slower movement)
	if (!CrouchAction)
	{
		CrouchAction = NewObject<UInputAction>(this, TEXT("IA_Crouch"));
		CrouchAction->ValueType = EInputActionValueType::Boolean;

		DefaultMappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	}

	UE_LOG(LogProjectCharacter, Log, TEXT("Created default Enhanced Input assets"));
}

//////////////////////////////////////////////////////////////////////////
// Input

void AProjectCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Client: Re-init ASC on re-possess (BeginPlay only runs once, this handles respawn/travel)
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Rebind movement speed attribute (BeginPlay doesn't re-run on respawn/travel)
		BindMovementSpeedAttribute();
	}

	if (!IsLocallyControlled())
	{
		return;
	}

	// Create default input assets if not configured
	CreateDefaultInputAssets();

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC || !PC->GetLocalPlayer())
	{
		UE_LOG(LogProjectCharacter, Warning, TEXT("PawnClientRestart: No local player yet"));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogProjectCharacter, Warning, TEXT("PawnClientRestart: No EnhancedInputLocalPlayerSubsystem"));
		return;
	}

	Subsystem->AddMappingContext(DefaultMappingContext, 0);
	UE_LOG(LogProjectCharacter, Log, TEXT("Added DefaultMappingContext (PawnClientRestart)"));
}

void AProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Create default input assets if not configured (called here because SetupPlayerInputComponent runs before BeginPlay)
	CreateDefaultInputAssets();

	// Use Enhanced Input
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		// Moving
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProjectCharacter::Move);
		}

		// Looking
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProjectCharacter::Look);
		}

		// Sprinting (Shift held = run, released = walk)
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AProjectCharacter::StartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AProjectCharacter::StopSprint);
		}

		// Crouching (Ctrl toggle)
		if (CrouchAction)
		{
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AProjectCharacter::StartCrouch);
		}

		UE_LOG(LogProjectCharacter, Log, TEXT("Enhanced Input bindings configured (Move=%s, Look=%s, Jump=%s, Sprint=%s, Crouch=%s)"),
			MoveAction ? TEXT("YES") : TEXT("NO"),
			LookAction ? TEXT("YES") : TEXT("NO"),
			JumpAction ? TEXT("YES") : TEXT("NO"),
			SprintAction ? TEXT("YES") : TEXT("NO"),
			CrouchAction ? TEXT("YES") : TEXT("NO"));
	}
	else
	{
		UE_LOG(LogProjectCharacter, Error, TEXT("Failed to find Enhanced Input component!"));
	}
}

void AProjectCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AProjectCharacter::MoveForward(float Value)
{
	if (Controller != nullptr && Value != 0.0f)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AProjectCharacter::MoveRight(float Value)
{
	if (Controller != nullptr && Value != 0.0f)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

//////////////////////////////////////////////////////////////////////////
// Sprint & Crouch

void AProjectCharacter::StartSprint()
{
	bIsSprinting = true;
	RefreshMovementSpeed();
}

void AProjectCharacter::StopSprint()
{
	bIsSprinting = false;
	RefreshMovementSpeed();
}

void AProjectCharacter::RefreshMovementSpeed()
{
	const float BaseSpeed = bIsSprinting ? RunSpeed : WalkSpeed;
	const float Multiplier = GetMovementSpeedMultiplier();
	const float FinalSpeed = BaseSpeed * Multiplier;

	GetCharacterMovement()->MaxWalkSpeed = FinalSpeed;

	// Also apply to crouch speed
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed * Multiplier;

	UE_LOG(LogProjectCharacter, Verbose, TEXT("Movement speed refreshed: Base=%.0f, Mult=%.2f, Final=%.0f"),
		BaseSpeed, Multiplier, FinalSpeed);
}

float AProjectCharacter::GetMovementSpeedMultiplier() const
{
	// Read from GAS attribute (modified by weight GEs, status effects, buffs, etc.)
	if (StatusAttributes)
	{
		return StatusAttributes->GetMovementSpeedMultiplier();
	}
	return 1.0f;
}

void AProjectCharacter::BindMovementSpeedAttribute()
{
	// Guard against double-binding
	if (bMovementSpeedBound)
	{
		return;
	}

	if (!AbilitySystemComponent || !StatusAttributes)
	{
		return;
	}

	// Bind to MovementSpeedMultiplier attribute changes
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UStatusAttributeSet::GetMovementSpeedMultiplierAttribute()
	).AddUObject(this, &AProjectCharacter::OnMovementSpeedMultiplierChanged);

	bMovementSpeedBound = true;

	// Initial refresh
	RefreshMovementSpeed();
}

void AProjectCharacter::UnbindMovementSpeedAttribute()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UStatusAttributeSet::GetMovementSpeedMultiplierAttribute()
	).RemoveAll(this);

	bMovementSpeedBound = false;
}

void AProjectCharacter::OnMovementSpeedMultiplierChanged(const FOnAttributeChangeData& Data)
{
	// Attribute changed - refresh movement speed
	RefreshMovementSpeed();
}

void AProjectCharacter::StartCrouch()
{
	if (bIsCrouched) { UnCrouch(); return; }
	Crouch();
}

void AProjectCharacter::StopCrouch()
{
	// Built-in UE uncrouch handles standing back up
	// Will fail gracefully if blocked by geometry above
	UnCrouch();
}

//////////////////////////////////////////////////////////////////////////
// IAbilitySystemInterface

UAbilitySystemComponent* AProjectCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

//////////////////////////////////////////////////////////////////////////
// First-Person Visibility (Layer 2 Split)

void AProjectCharacter::ApplyFirstPersonVisibility()
{
	if (!IsLocallyControlled()) return;

	USkeletalMeshComponent* Body = GetMesh();
	if (!Body) return;

	// === Layer 1: Driver — invisible to all; exists only for Motion Matching + Root Motion ===
	Body->SetHiddenInGame(true);   // No one sees the driver mesh
	Body->SetCastShadow(false);    // Shadow comes from WorldBodyMesh instead

	// === Layer 2a: World Body (full, others see, casts shadow) ===
	Body->SetOwnerNoSee(true);
	Body->SetCastHiddenShadow(true);

	// === WorldBodyMesh: other players see this body + it casts the FP shadow ===
	if (WorldBodyMesh)
	{
		// Sync mesh from Driver in case Mutable generated it after constructor
		if (Body->GetSkeletalMeshAsset() && WorldBodyMesh->GetSkeletalMeshAsset() != Body->GetSkeletalMeshAsset())
		{
			WorldBodyMesh->SetSkeletalMeshAsset(Body->GetSkeletalMeshAsset());
		}

		WorldBodyMesh->SetOwnerNoSee(true);       // Owner sees LocalBody instead
		WorldBodyMesh->SetCastHiddenShadow(true);  // Shadow visible even when mesh is OwnerNoSee
		WorldBodyMesh->SetHiddenInGame(false);      // Other players see this body

		// Camera stays on capsule — SpineLock pulls bone TO camera, not camera to bone

		// Head must follow WorldBody (not Driver) — otherwise neck-body seam gap appears
		TArray<USkeletalMeshComponent*> SkelComps;
		GetComponents<USkeletalMeshComponent>(SkelComps);
		for (USkeletalMeshComponent* Comp : SkelComps)
		{
			if (Comp == Body || Comp == WorldBodyMesh || Comp == LocalBodyMesh) continue;
			if (Comp->GetFName() == FName("Head"))
			{
				Comp->SetLeaderPoseComponent(WorldBodyMesh);
				break;
			}
		}
	}

	// === Layer 2b: Local Body (headless, owner sees) ===
	// Local_Body_CSK (Blueprint) should generate mesh via Mutable.
	// Fallback: copy from Layer 2a if CSK hasn't populated it yet.
	if (LocalBodyMesh)
	{
		LocalBodyMesh->SetOnlyOwnerSee(true);
		LocalBodyMesh->SetCastShadow(false);       // Original (kept for zero-diff with git HEAD)
		LocalBodyMesh->SetCastShadow(true);        // Override: owner needs to see own body shadow
		LocalBodyMesh->SetHiddenInGame(false);

		// Fallback: sync mesh from Layer 2a if Local_Body_CSK hasn't generated it
		if (!LocalBodyMesh->GetSkeletalMeshAsset() && Body->GetSkeletalMeshAsset())
		{
			LocalBodyMesh->SetSkeletalMeshAsset(Body->GetSkeletalMeshAsset());
		}

		// Second fallback: WorldBody may have mesh if Driver was empty (Mutable workflow)
		if (!LocalBodyMesh->GetSkeletalMeshAsset() && WorldBodyMesh)
		{
			LocalBodyMesh->SetSkeletalMeshAsset(WorldBodyMesh->GetSkeletalMeshAsset());
		}

		// Setup CopyPose + ModifyBone(root) AnimInstance for spine lock
		// LeaderPose bypasses AnimGraph so per-bone offset is impossible with it
		if (LocalBodyMesh->GetSkeletalMeshAsset() && !LocalBodyMesh->GetAnimInstance())
		{
			LocalBodyMesh->SetAnimInstanceClass(ULocalBodyAnimInstance::StaticClass());
			LocalBodyMesh->InitAnim(true);
		}

		// Clear LeaderPose (set in constructor) — LocalBody uses CopyPose via AnimGraph instead
		if (LocalBodyMesh->LeaderPoseComponent.IsValid())
		{
			LocalBodyMesh->SetLeaderPoseComponent(nullptr);
		}

		// Remove head geometry on local body only (scales head bone to zero)
		// Layer 2a retains full head for other players and shadow
		LocalBodyMesh->HideBoneByName(TEXT("head"), PBO_None);
	}

	// === Head, Groom, and other non-body primitives: hide from owner, keep shadow ===
	// GetComponents only returns Actor-owned components; Grooms may be children of Head mesh
	TArray<UPrimitiveComponent*> PrimComps;
	GetComponents<UPrimitiveComponent>(PrimComps);

	// Also gather children of Head mesh (Groom bindings are attached there)
	TArray<USceneComponent*> HeadChildren;
	for (UPrimitiveComponent* PC : PrimComps)
	{
		if (PC->GetFName().ToString().Contains(TEXT("Head")))
		{
			PC->GetChildrenComponents(true, HeadChildren);
			break;
		}
	}
	for (USceneComponent* Child : HeadChildren)
	{
		if (UPrimitiveComponent* ChildPrim = Cast<UPrimitiveComponent>(Child))
		{
			PrimComps.AddUnique(ChildPrim);
		}
	}

	for (UPrimitiveComponent* PrimComp : PrimComps)
	{
		if (PrimComp == Body || PrimComp == WorldBodyMesh || PrimComp == LocalBodyMesh) continue;
		if (PrimComp == GetCapsuleComponent()) continue;

		const FString CompName = PrimComp->GetFName().ToString();
		const FString ClassName = PrimComp->GetClass()->GetName();

		bool bIsHead = CompName.Contains(TEXT("Head"));
		bool bIsGroom = CompName.Contains(TEXT("Groom"))
			|| ClassName.Contains(TEXT("Groom"))
			|| CompName.Contains(TEXT("Hair"))
			|| CompName.Contains(TEXT("Beard"))
			|| CompName.Contains(TEXT("Eyebrow"))
			|| CompName.Contains(TEXT("Eyelash"))
			|| CompName.Contains(TEXT("Mustache"));

		if (bIsHead || bIsGroom)
		{
			PrimComp->SetOwnerNoSee(true);
			PrimComp->SetCastHiddenShadow(true);
		}
	}
}

void AProjectCharacter::OnMutableMeshUpdated(UCustomizableObjectInstance* Instance)
{
	ApplyFirstPersonVisibility();

	// Groom components may be re-attached asynchronously after Mutable finishes — retry after short delay
	GetWorld()->GetTimerManager().SetTimer(
		GroomRetryTimerHandle,
		[this]()
		{
			ApplyFirstPersonVisibility();
		},
		0.5f,
		false
	);
}
