// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "DefinitionCharacter.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/ILookInputModifier.h"
#include "Kismet/GameplayStatics.h"

void ADefinitionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	CreateInputAssets();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (Input == nullptr)
	{
		UE_LOG(LogDefinitionCharacter, Error,
			TEXT("[ADefinitionCharacter::SetupPlayerInputComponent] Enhanced input component missing"));
		return;
	}

	if (JumpAction != nullptr)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this,
			&ADefinitionCharacter::HandleJumpStarted);
		Input->BindAction(JumpAction, ETriggerEvent::Triggered, this,
			&ADefinitionCharacter::HandleJumpTriggered);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this,
			&ADefinitionCharacter::HandleJumpCompleted);
	}
	if (MoveAction != nullptr)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADefinitionCharacter::Move);
	}
	if (LookAction != nullptr)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADefinitionCharacter::Look);
	}
	if (SprintAction != nullptr)
	{
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ADefinitionCharacter::StartSprint);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADefinitionCharacter::StopSprint);
	}
	if (CrouchAction != nullptr)
	{
		Input->BindAction(CrouchAction, ETriggerEvent::Started, this,
			&ADefinitionCharacter::HandleCrouchStarted);
		Input->BindAction(CrouchAction, ETriggerEvent::Triggered, this,
			&ADefinitionCharacter::HandleCrouchTriggered);
	}
	if (WalkAction != nullptr)
	{
		Input->BindAction(WalkAction, ETriggerEvent::Started, this, &ADefinitionCharacter::ToggleWalk);
	}

	UE_LOG(LogDefinitionCharacter, Log,
		TEXT("[ADefinitionCharacter::SetupPlayerInputComponent] Input bindings configured"));
}

void ADefinitionCharacter::HandleJumpStarted()
{
	if (!GetCharacterMovement()->IsFlying())
	{
		Jump();
	}
}

void ADefinitionCharacter::HandleJumpTriggered()
{
	if (GetCharacterMovement()->IsFlying())
	{
		AddMovementInput(FVector::UpVector);
	}
}

void ADefinitionCharacter::HandleJumpCompleted()
{
	StopJumping();
}

void ADefinitionCharacter::HandleCrouchStarted()
{
	if (!GetCharacterMovement()->IsFlying())
	{
		StartCrouch();
	}
}

void ADefinitionCharacter::HandleCrouchTriggered()
{
	if (GetCharacterMovement()->IsFlying())
	{
		AddMovementInput(FVector::DownVector);
	}
}

void ADefinitionCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller != nullptr)
	{
		const FRotator YawRotation(0, Controller->GetControlRotation().Yaw, 0);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementVector.Y);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementVector.X);
	}
}

void ADefinitionCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxis = Value.Get<FVector2D>();
	if (AGameModeBase* GameMode = UGameplayStatics::GetGameMode(this))
	{
		if (GameMode->Implements<ULookInputModifier>())
		{
			if (const ILookInputModifier* Modifier = Cast<ILookInputModifier>(GameMode))
			{
				LookAxis = Modifier->ModifyLook(LookAxis);
			}
		}
	}

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxis.X);
		AddControllerPitchInput(LookAxis.Y);
	}
}
