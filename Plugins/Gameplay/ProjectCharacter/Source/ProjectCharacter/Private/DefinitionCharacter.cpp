// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "DefinitionCharacter.h"
#include "Interfaces/ILookInputModifier.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
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
#include "Interfaces/IAssemblyCapability.h"
#include "Interfaces/IAssemblyViewConfigSource.h"
#include "Interfaces/AssemblyTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY(LogDefinitionCharacter);

ADefinitionCharacter::ADefinitionCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// -------------------------------------------------------------------------
	// GAS
	// -------------------------------------------------------------------------
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthAttributes = CreateDefaultSubobject<UHealthAttributeSet>(TEXT("HealthAttributes"));
	SurvivalAttributes = CreateDefaultSubobject<USurvivalAttributeSet>(TEXT("SurvivalAttributes"));
	StaminaAttributes = CreateDefaultSubobject<UStaminaAttributeSet>(TEXT("StaminaAttributes"));
	StatusAttributes = CreateDefaultSubobject<UStatusAttributeSet>(TEXT("StatusAttributes"));

	// NOTE (DIP exception - intentional composition ownership):
	// CreateDefaultSubobject<T> requires a concrete type at compile time. The
	// ProjectVitals dependency is therefore composition, not consumption.
	// Consumer plugins that only observe vitals events go through
	// IVitalsEventsSource in ProjectCore. See docs/architecture/plugin_rules.md
	// (Composition Ownership vs Consumption).
	VitalsComponent = CreateDefaultSubobject<UProjectVitalsComponent>(TEXT("VitalsComponent"));

	// -------------------------------------------------------------------------
	// Capsule -- match the historical AProjectCharacter baseline (23, 88).
	// CDO shows 30/86 but runtime uses 23/88 for tight corridor navigation
	// -------------------------------------------------------------------------
	GetCapsuleComponent()->InitCapsuleSize(23.f, 88.0f);
	// Pawn profile ignores Visibility by default. Override so interaction
	// traces (ECC_Visibility) can hit NPC characters for dialogue/pickup.
	// Player's InteractionComponent ignores self via AddIgnoredActor.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// First-person rotation: GASP MM ABP owns body yaw via RootYawOffset
	// and turn-in-place. Capsule must NOT snap to camera yaw instantly.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// -------------------------------------------------------------------------
	// Movement defaults seeded from the historical Blueprint runtime baseline.
	// The former Blueprint path set these dynamically from gait/strafe.
	// The definition-driven runtime uses the Run gait steady-state values directly.
	// -------------------------------------------------------------------------
	// First-person controller-yaw: treat the actor as the target rotation and
	// let the movement component rotate it toward the controller, matching the
	// historical Blueprint UpdateRotation_PreCMC contract.
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	// UE sentinel: negative Yaw = instant rotation (no interpolation).
	// Overridden dynamically in UpdateRotationPolicy for air vs ground.
	GetCharacterMovement()->RotationRate = FRotator(0.0f, -1.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 420.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->GroundFriction = 5.f;

	// Physics interaction (door blocking)
	GetCharacterMovement()->bEnablePhysicsInteraction = true;
	GetCharacterMovement()->bPushForceScaledToMass = true;
	GetCharacterMovement()->PushForceFactor = 50000.0f;
	GetCharacterMovement()->InitialPushForceFactor = 500.0f;
	GetCharacterMovement()->TouchForceFactor = 0.0f;
	GetCharacterMovement()->RepulsionForce = 2.0f;

	// Crouch
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(60.0f);
	// Historical Blueprint runtime: CrouchSpeeds[1]=200 * strafe multiplier ~= 225.
	GetCharacterMovement()->MaxWalkSpeedCrouched = 225.f;

	// Driver mesh: capsule bottom, face forward (same as AProjectCharacter)
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// Camera is created dynamically in ApplyViewConfig when the definition
	// contains a view section. NPCs (no view) get no camera component.
	FirstPersonCamera = nullptr;

	// NO WorldBodyMesh/LocalBodyMesh -- definition meshes come from ObjectSpawnUtility
}

//////////////////////////////////////////////////////////////////////////
// IAbilitySystemInterface

UAbilitySystemComponent* ADefinitionCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

//////////////////////////////////////////////////////////////////////////
// IGameplayTagAssetInterface

void ADefinitionCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetOwnedGameplayTags(TagContainer);
	}
}

//////////////////////////////////////////////////////////////////////////
// Lifecycle

void ADefinitionCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateRotationPolicy();

	// Client ASC init (idempotent)
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	CreateInputAssets();
	BindMovementSpeedAttribute();

	// Resolve assembly interfaces independently -- lifecycle and data
	// may live on different components in future implementations.
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (!CachedAssemblyComponent.IsValid())
		{
			if (IAssemblyCapability* Assembly = Cast<IAssemblyCapability>(Comp))
			{
				CachedAssemblyComponent = Comp;
				AssemblyStateHandle = Assembly->AddAssemblyStateChanged(
					FOnAssemblyStateChangedNative::FDelegate::CreateUObject(
						this, &ADefinitionCharacter::OnAssemblyStateChanged));
			}
		}
		if (!CachedDataSourceComponent.IsValid())
		{
			if (Cast<IAssemblyViewConfigSource>(Comp))
			{
				CachedDataSourceComponent = Comp;
			}
		}
	}

	// If assembly already reached Ready before BeginPlay (unlikely but safe)
	if (CachedAssemblyComponent.IsValid())
	{
		if (IAssemblyCapability* Assembly = Cast<IAssemblyCapability>(CachedAssemblyComponent.Get()))
		{
			if (Assembly->GetCurrentAssemblyState() == EAssemblyState::Ready)
			{
				ApplyViewConfig();
			}
		}
	}
}

void ADefinitionCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// First vitals start. Re-possession path is in PossessedBy.
	// VitalsComponent::Start() is idempotent (bIsRunning guard).
	if (HasAuthority() && VitalsComponent)
	{
		VitalsComponent->Start();
	}
}

void ADefinitionCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	UpdateRotationPolicy();
}

void ADefinitionCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		BindMovementSpeedAttribute();

		if (HasAuthority())
		{
			GiveStartupAbilitySets();

			// Re-possession restart (after UnPossessed->Stop).
			if (VitalsComponent && AbilitySystemComponent->HasBeenInitialized())
			{
				VitalsComponent->Start();
			}
		}
	}

	UpdateRotationPolicy();

	UE_LOG(LogDefinitionCharacter, Log, TEXT("PossessedBy: ASC initialized"));
}

void ADefinitionCharacter::UnPossessed()
{
	if (VitalsComponent)
	{
		VitalsComponent->Stop();
	}

	RemoveDefaultInputMappingContext();
	RevokeStartupAbilitySets();
	Super::UnPossessed();
}

void ADefinitionCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind assembly state delegate
	if (CachedAssemblyComponent.IsValid() && AssemblyStateHandle.IsValid())
	{
		if (IAssemblyCapability* Assembly = Cast<IAssemblyCapability>(CachedAssemblyComponent.Get()))
		{
			Assembly->RemoveAssemblyStateChanged(AssemblyStateHandle);
		}
		AssemblyStateHandle.Reset();
	}

	UnbindMovementSpeedAttribute();

	if (VitalsComponent)
	{
		VitalsComponent->Stop();
	}

	RemoveDefaultInputMappingContext();
	RevokeStartupAbilitySets();
	Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////////
// GAS Ability Sets

void ADefinitionCharacter::GiveStartupAbilitySets()
{
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

		UE_LOG(LogDefinitionCharacter, Log, TEXT("Granted startup AbilitySet: %s"), *AbilitySet->GetName());
	}
}

void ADefinitionCharacter::RevokeStartupAbilitySets()
{
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

	UE_LOG(LogDefinitionCharacter, Log, TEXT("Revoked all startup AbilitySets"));
}

//////////////////////////////////////////////////////////////////////////
// Assembly Lifecycle -> View Config (via IAssemblyCapability contract)

void ADefinitionCharacter::OnAssemblyStateChanged(EAssemblyState NewState)
{
	UE_LOG(LogDefinitionCharacter, Log, TEXT("Assembly state -> %d"),
		static_cast<uint8>(NewState));

	if (NewState == EAssemblyState::Ready)
	{
		ApplyViewConfig();
	}
}

void ADefinitionCharacter::ApplyViewConfig()
{
	if (!CachedDataSourceComponent.IsValid())
	{
		UE_LOG(LogDefinitionCharacter, Verbose,
			TEXT("ApplyViewConfig: No data source -- no camera"));
		return;
	}

	IAssemblyViewConfigSource* DataSource = Cast<IAssemblyViewConfigSource>(CachedDataSourceComponent.Get());
	if (!DataSource)
	{
		return;
	}

	FAssemblyViewConfig ViewConfig;
	if (!DataSource->GetViewConfig(ViewConfig))
	{
		UE_LOG(LogDefinitionCharacter, Verbose,
			TEXT("ApplyViewConfig: No view config -- no camera"));
		return;
	}

	// Create camera on demand (only for characters with a view section)
	if (!FirstPersonCamera)
	{
		FirstPersonCamera = NewObject<UCameraComponent>(this, TEXT("FirstPersonCamera"));
		FirstPersonCamera->SetupAttachment(RootComponent);
		FirstPersonCamera->bUsePawnControlRotation = true;
		FirstPersonCamera->FieldOfView = 90.0f;
		FirstPersonCamera->RegisterComponent();

		UE_LOG(LogDefinitionCharacter, Log,
			TEXT("ApplyViewConfig: Camera created (definition has view section)"));

		// Limit downward pitch to prevent seeing the neck/upper torso
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->ViewPitchMin = -79.f;  // limit looking down
				PC->PlayerCameraManager->ViewPitchMax = 80.f;   // limit looking up
			}
		}
	}

	if (!ViewConfig.RelativeOffset.IsNearlyZero())
	{
		FirstPersonCamera->SetRelativeLocation(ViewConfig.RelativeOffset);
		UE_LOG(LogDefinitionCharacter, Log,
			TEXT("ApplyViewConfig: Camera offset set to %s"),
			*ViewConfig.RelativeOffset.ToString());
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ADefinitionCharacter::CreateInputAssets()
{
	if (DefaultMappingContext)
	{
		return;
	}

	DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("DefaultMappingContext"));

	// Move (WASD -> Axis2D)
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	FEnhancedActionKeyMapping& MoveForward = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
	MoveForward.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));

	FEnhancedActionKeyMapping& MoveBack = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
	MoveBack.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	MoveBack.Modifiers.Add(NewObject<UInputModifierNegate>(this));

	DefaultMappingContext->MapKey(MoveAction, EKeys::D);
	FEnhancedActionKeyMapping& MoveLeft = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
	MoveLeft.Modifiers.Add(NewObject<UInputModifierNegate>(this));

	// Look (Mouse -> Axis2D)
	LookAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;

	FEnhancedActionKeyMapping& LookMapping = DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
	UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(this);
	NegateY->bX = false;
	NegateY->bY = true;
	NegateY->bZ = false;
	LookMapping.Modifiers.Add(NegateY);

	// Jump (Space)
	JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
	JumpAction->ValueType = EInputActionValueType::Boolean;
	DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);

	// Sprint (Left Shift)
	SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint"));
	SprintAction->ValueType = EInputActionValueType::Boolean;
	DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);

	// Crouch (Left Ctrl)
	CrouchAction = NewObject<UInputAction>(this, TEXT("IA_Crouch"));
	CrouchAction->ValueType = EInputActionValueType::Boolean;
	DefaultMappingContext->MapKey(CrouchAction, EKeys::LeftControl);

	// Walk (CapsLock toggle)
	WalkAction = NewObject<UInputAction>(this, TEXT("IA_Walk"));
	WalkAction->ValueType = EInputActionValueType::Boolean;
	DefaultMappingContext->MapKey(WalkAction, EKeys::CapsLock);

	UE_LOG(LogDefinitionCharacter, Log, TEXT("Created Enhanced Input assets"));
}

void ADefinitionCharacter::RemoveDefaultInputMappingContext()
{
	if (!DefaultMappingContext || !IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC || !PC->GetLocalPlayer())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(DefaultMappingContext);
		UE_LOG(LogDefinitionCharacter, Log, TEXT("Removed DefaultMappingContext"));
	}
}

void ADefinitionCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		BindMovementSpeedAttribute();
	}

	if (!IsLocallyControlled())
	{
		return;
	}

	CreateInputAssets();

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC || !PC->GetLocalPlayer())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->RemoveMappingContext(DefaultMappingContext);
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ADefinitionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	CreateInputAssets();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogDefinitionCharacter, Error, TEXT("No EnhancedInputComponent"));
		return;
	}

	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADefinitionCharacter::Move);
	}
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADefinitionCharacter::Look);
	}
	if (SprintAction)
	{
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ADefinitionCharacter::StartSprint);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADefinitionCharacter::StopSprint);
	}
	if (CrouchAction)
	{
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ADefinitionCharacter::StartCrouch);
	}
	if (WalkAction)
	{
		EIC->BindAction(WalkAction, ETriggerEvent::Started, this, &ADefinitionCharacter::ToggleWalk);
	}

	UE_LOG(LogDefinitionCharacter, Log, TEXT("Input bindings configured"));
}

void ADefinitionCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator YawRotation(0, Controller->GetControlRotation().Yaw, 0);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementVector.Y);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementVector.X);
	}
}

void ADefinitionCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxis = Value.Get<FVector2D>();

	// If some authority in the current world implements ILookInputModifier
	// (e.g. a recording-aware game mode wanting smoother captured camera),
	// give it a chance to transform the input before we accumulate it on
	// the controller. The character has no opinion about who or why; it
	// only knows the interface. Default-implemented to identity, so the
	// no-op case is free.
	if (AGameModeBase* GM = UGameplayStatics::GetGameMode(this))
	{
		if (GM->Implements<ULookInputModifier>())
		{
			if (const ILookInputModifier* Mod = Cast<ILookInputModifier>(GM))
			{
				LookAxis = Mod->ModifyLook(LookAxis);
			}
		}
	}

	if (Controller)
	{
		AddControllerYawInput(LookAxis.X);
		AddControllerPitchInput(LookAxis.Y);
	}
}

//////////////////////////////////////////////////////////////////////////
// Movement Speed

void ADefinitionCharacter::StartSprint()
{
	bIsSprinting = true;
	RefreshMovementSpeed();
}

void ADefinitionCharacter::StopSprint()
{
	bIsSprinting = false;
	RefreshMovementSpeed();
}

void ADefinitionCharacter::ToggleWalk()
{
	double Now = FPlatformTime::Seconds();
	if (Now - LastWalkToggleTime < 0.3) return;
	LastWalkToggleTime = Now;

	bIsWalking = !bIsWalking;
	RefreshMovementSpeed();
}

void ADefinitionCharacter::StartCrouch()
{
	if (bIsCrouched) { UnCrouch(); return; }
	Crouch();
}

void ADefinitionCharacter::RefreshMovementSpeed()
{
	const float BaseSpeed = bIsSprinting ? SprintSpeed
	                      : bIsWalking   ? WalkSpeed
	                      :                RunSpeed;
	const float Multiplier = GetMovementSpeedMultiplier();

	GetCharacterMovement()->MaxWalkSpeed = BaseSpeed * Multiplier;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed * Multiplier;
}

float ADefinitionCharacter::GetMovementSpeedMultiplier() const
{
	if (StatusAttributes)
	{
		return StatusAttributes->GetMovementSpeedMultiplier();
	}
	return 1.0f;
}

void ADefinitionCharacter::BindMovementSpeedAttribute()
{
	if (bMovementSpeedBound || !AbilitySystemComponent || !StatusAttributes)
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UStatusAttributeSet::GetMovementSpeedMultiplierAttribute()
	).AddUObject(this, &ADefinitionCharacter::OnMovementSpeedMultiplierChanged);

	bMovementSpeedBound = true;
	RefreshMovementSpeed();
}

void ADefinitionCharacter::UnbindMovementSpeedAttribute()
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

void ADefinitionCharacter::OnMovementSpeedMultiplierChanged(const FOnAttributeChangeData& Data)
{
	RefreshMovementSpeed();
}

void ADefinitionCharacter::UpdateRotationPolicy()
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!CMC)
	{
		return;
	}

	// UE sentinel: negative RotationRate.Yaw = instant rotation (no interpolation)
	static constexpr float InstantRotationRate = -1.0f;
	static constexpr float FallingRotationRate = 200.0f;

	CMC->bUseControllerDesiredRotation = true;
	CMC->bOrientRotationToMovement = false;
	CMC->RotationRate = CMC->IsFalling()
		? FRotator(0.0f, FallingRotationRate, 0.0f)
		: FRotator(0.0f, InstantRotationRate, 0.0f);
}
