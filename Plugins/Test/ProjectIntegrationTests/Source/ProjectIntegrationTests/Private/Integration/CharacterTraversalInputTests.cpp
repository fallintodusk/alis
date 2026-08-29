// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "DefinitionCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FCharacterTraversalInputCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FCharacterTraversalInputCommand(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (GFrameCounter == LastFrame)
			{
				return false;
			}
			LastFrame = GFrameCounter;
			++Frame;

			if (!AcquireCharacter())
			{
				if (Frame > 1800)
				{
					Test.AddError(TEXT("Timed out waiting for the possessed DefinitionCharacter."));
					return true;
				}
				return false;
			}

			if (Stage == 0)
			{
				GroundMaxWalkSpeed = Movement->MaxWalkSpeed;
				BaseMaxFlySpeed = Movement->MaxFlySpeed;
				BaseMaxAcceleration = Movement->MaxAcceleration;
				BaseBrakingDecelerationFlying = Movement->BrakingDecelerationFlying;
				Send(EKeys::LeftShift, IE_Pressed);
				Stage = 1;
				StageFrame = 0;
				return false;
			}

			++StageFrame;
			if (Stage == 1)
			{
				if (StageFrame < 10)
				{
					return false;
				}
				Test.TestTrue(TEXT("Grounded Shift retains sprint behavior."),
					Movement->MaxWalkSpeed > GroundMaxWalkSpeed);
				Test.TestEqual(TEXT("Grounded Shift does not alter flight speed."),
					Movement->MaxFlySpeed, BaseMaxFlySpeed);
				Send(EKeys::LeftShift, IE_Released);
				Stage = 2;
				StageFrame = 0;
				return false;
			}

			if (Stage == 2)
			{
				if (StageFrame < 10)
				{
					return false;
				}
				Test.TestTrue(TEXT("Grounded Shift release restores prior speed."),
					FMath::IsNearlyEqual(Movement->MaxWalkSpeed, GroundMaxWalkSpeed));
				Movement->SetMovementMode(MOVE_Flying);
				Controller->SetControlRotation(FRotator(0.0, 90.0, 0.0));
				Start = Character->GetActorLocation();
				Send(EKeys::SpaceBar, IE_Pressed);
				Send(EKeys::W, IE_Pressed);
				Stage = 3;
				StageFrame = 0;
				return false;
			}

			if (Stage == 3)
			{
				Send(EKeys::SpaceBar, IE_Repeat);
				Send(EKeys::W, IE_Repeat);
				if (StageFrame < 120)
				{
					return false;
				}
				BaseMeasuredVelocity = Movement->Velocity.Size();
				const FVector Delta = Character->GetActorLocation() - Start;
				Test.TestTrue(TEXT("Held Space ascends through mapped input."), Delta.Z > 10.0);
				Test.TestTrue(TEXT("W remains controller-yaw relative while flying."), Delta.Y > FMath::Abs(Delta.X));
				Send(EKeys::LeftShift, IE_Pressed);
				Stage = 4;
				StageFrame = 0;
				return false;
			}

			if (Stage == 4)
			{
				Send(EKeys::SpaceBar, IE_Repeat);
				Send(EKeys::W, IE_Repeat);
				if (StageFrame < 120)
				{
					return false;
				}
				Test.TestTrue(TEXT("Flying Shift selects the five-times speed candidate."),
					FMath::IsNearlyEqual(Movement->MaxFlySpeed, BaseMaxFlySpeed * 5.0f));
				Test.TestTrue(TEXT("Flying Shift scales only the active flight acceleration."),
					FMath::IsNearlyEqual(Movement->MaxAcceleration, BaseMaxAcceleration * 5.0f));
				Test.TestTrue(TEXT("Flying Shift scales flight braking for bounded stopping."),
					FMath::IsNearlyEqual(Movement->BrakingDecelerationFlying,
						BaseBrakingDecelerationFlying * 5.0f));
				Test.TestTrue(TEXT("Flying Shift produces higher measured velocity."),
					Movement->Velocity.Size() > BaseMeasuredVelocity + 100.0f);
				Test.AddInfo(FString::Printf(
					TEXT("PreviewFlight measured velocity: base=%.3f cm/s boost=%.3f cm/s ")
					TEXT("base_max=%.3f cm/s boost_max=%.3f cm/s"),
					BaseMeasuredVelocity, Movement->Velocity.Size(),
					BaseMaxFlySpeed, Movement->MaxFlySpeed));
				Send(EKeys::SpaceBar, IE_Released);
				Send(EKeys::W, IE_Released);
				Send(EKeys::LeftShift, IE_Released);
				Stage = 5;
				StageFrame = 0;
				return false;
			}

			if (Stage == 5)
			{
				if (StageFrame < 10)
				{
					return false;
				}
				Test.TestTrue(TEXT("Shift release restores base flight speed."),
					FMath::IsNearlyEqual(Movement->MaxFlySpeed, BaseMaxFlySpeed));
				Test.TestTrue(TEXT("Shift release restores base acceleration."),
					FMath::IsNearlyEqual(Movement->MaxAcceleration, BaseMaxAcceleration));
				Test.TestTrue(TEXT("Shift release restores base flight braking."),
					FMath::IsNearlyEqual(Movement->BrakingDecelerationFlying,
						BaseBrakingDecelerationFlying));
				Send(EKeys::LeftShift, IE_Pressed);
				Stage = 6;
				StageFrame = 0;
				return false;
			}

			if (Stage == 6)
			{
				if (StageFrame < 10)
				{
					return false;
				}
				Movement->SetMovementMode(MOVE_Falling);
				Test.TestTrue(TEXT("Leaving flight restores base speed even while Shift is held."),
					FMath::IsNearlyEqual(Movement->MaxFlySpeed, BaseMaxFlySpeed));
				Test.TestTrue(TEXT("Leaving flight restores base acceleration."),
					FMath::IsNearlyEqual(Movement->MaxAcceleration, BaseMaxAcceleration));
				Test.TestTrue(TEXT("Leaving flight restores base flight braking."),
					FMath::IsNearlyEqual(Movement->BrakingDecelerationFlying,
						BaseBrakingDecelerationFlying));
				Send(EKeys::LeftShift, IE_Released);
				Movement->SetMovementMode(MOVE_Flying);
				HighPoint = Character->GetActorLocation();
				Send(EKeys::LeftControl, IE_Pressed);
				Stage = 7;
				StageFrame = 0;
				return false;
			}

			if (Stage == 7)
			{
				Send(EKeys::LeftControl, IE_Repeat);
				if (StageFrame < 240)
				{
					return false;
				}
				Send(EKeys::LeftControl, IE_Released);
				Test.TestTrue(TEXT("Held Ctrl descends through mapped input."),
					Character->GetActorLocation().Z < HighPoint.Z - 5.0);
				Test.TestEqual(TEXT("Flight keeps swept CharacterMovement active."),
					Movement->MovementMode, MOVE_Flying);
				Test.TestTrue(TEXT("The production capsule remains collision-enabled."),
					Character->GetCapsuleComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
				LookStartYaw = Controller->GetControlRotation().Yaw;
				SendLook(FVector2D(5.0, 0.0));
				Stage = 8;
				StageFrame = 0;
				return false;
			}

			if (Stage == 8)
			{
				if (StageFrame < 10)
				{
					return false;
				}
				Test.TestTrue(TEXT("Simulated Mouse2D rotates through the mapped look action."),
					FMath::Abs(FMath::FindDeltaAngleDegrees(
						LookStartYaw, Controller->GetControlRotation().Yaw)) > 0.1);
				Send(EKeys::LeftShift, IE_Pressed);
				Stage = 9;
				StageFrame = 0;
				return false;
			}

			if (Stage == 9)
			{
				if (StageFrame < 10)
				{
					return false;
				}
				Test.TestTrue(TEXT("Flying Shift is active before unpossession."),
					FMath::IsNearlyEqual(Movement->MaxFlySpeed, BaseMaxFlySpeed * 5.0f));
				Controller->UnPossess();
				Test.TestNull(TEXT("Controller releases the pawn during unpossession."),
					Controller->GetPawn());
				Test.TestTrue(TEXT("Unpossession restores base flight speed."),
					FMath::IsNearlyEqual(Movement->MaxFlySpeed, BaseMaxFlySpeed));
				Test.TestTrue(TEXT("Unpossession restores base acceleration."),
					FMath::IsNearlyEqual(Movement->MaxAcceleration, BaseMaxAcceleration));
				Test.TestTrue(TEXT("Unpossession restores base flight braking."),
					FMath::IsNearlyEqual(Movement->BrakingDecelerationFlying,
						BaseBrakingDecelerationFlying));
				return true;
			}

			Test.AddError(TEXT("Traversal input test entered an unknown stage."));
			return true;
		}

	private:
		bool AcquireCharacter()
		{
			if (Character.IsValid() && Controller.IsValid() && Movement.IsValid())
			{
				return true;
			}
			if (GEngine == nullptr)
			{
				return false;
			}
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				APlayerController* Candidate = World == nullptr ? nullptr :
					UGameplayStatics::GetPlayerController(World, 0);
				ADefinitionCharacter* Pawn = Candidate == nullptr ? nullptr :
					Cast<ADefinitionCharacter>(Candidate->GetPawn());
				if (Pawn != nullptr)
				{
					Controller = Candidate;
					Character = Pawn;
					Movement = Pawn->GetCharacterMovement();
					return Movement.IsValid();
				}
			}
			return false;
		}

		void Send(const FKey& Key, EInputEvent Event) const
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, 1.0f));
		}

		void SendLook(const FVector2D& Value) const
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(
				EKeys::MouseX, IE_Axis, Value.X));
		}

		FAutomationTestBase& Test;
		TWeakObjectPtr<APlayerController> Controller;
		TWeakObjectPtr<ADefinitionCharacter> Character;
		TWeakObjectPtr<UCharacterMovementComponent> Movement;
		FVector Start = FVector::ZeroVector;
		FVector HighPoint = FVector::ZeroVector;
		float GroundMaxWalkSpeed = 0.0f;
		float BaseMaxFlySpeed = 0.0f;
		float BaseMaxAcceleration = 0.0f;
		float BaseBrakingDecelerationFlying = 0.0f;
		float BaseMeasuredVelocity = 0.0f;
		double LookStartYaw = 0.0;
		uint64 LastFrame = MAX_uint64;
		int32 Frame = 0;
		int32 StageFrame = 0;
		int32 Stage = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCharacterTraversalInputTest,
	"ProjectIntegrationTests.Character.Traversal.ModeAwareInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
		EAutomationTestFlags::ProductFilter)

bool FCharacterTraversalInputTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/Project/Maps/Test/ClipMatrix_CleanMap"));
	ADD_LATENT_AUTOMATION_COMMAND(FCharacterTraversalInputCommand(*this));
	return true;
}

#endif
