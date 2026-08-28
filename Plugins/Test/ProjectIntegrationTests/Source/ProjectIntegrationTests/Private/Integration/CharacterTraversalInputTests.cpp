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
				Movement->SetMovementMode(MOVE_Flying);
				Controller->SetControlRotation(FRotator(0.0, 90.0, 0.0));
				Start = Character->GetActorLocation();
				Send(EKeys::SpaceBar, IE_Pressed);
				Send(EKeys::W, IE_Pressed);
				Stage = 1;
				StageFrame = 0;
				return false;
			}

			++StageFrame;
			if (Stage == 1)
			{
				Send(EKeys::SpaceBar, IE_Repeat);
				Send(EKeys::W, IE_Repeat);
				if (StageFrame < 45)
				{
					return false;
				}
				Send(EKeys::SpaceBar, IE_Released);
				Send(EKeys::W, IE_Released);
				const FVector Delta = Character->GetActorLocation() - Start;
				Test.TestTrue(TEXT("Held Space ascends through mapped input."), Delta.Z > 10.0);
				Test.TestTrue(TEXT("W remains controller-yaw relative while flying."), Delta.Y > FMath::Abs(Delta.X));
				HighPoint = Character->GetActorLocation();
				Send(EKeys::LeftControl, IE_Pressed);
				Stage = 2;
				StageFrame = 0;
				return false;
			}

			if (Stage == 2)
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
				Stage = 3;
				StageFrame = 0;
				return false;
			}

			if (Stage == 3)
			{
				if (StageFrame < 10)
				{
					return false;
				}
				Test.TestTrue(TEXT("Simulated Mouse2D rotates through the mapped look action."),
					FMath::Abs(FMath::FindDeltaAngleDegrees(
						LookStartYaw, Controller->GetControlRotation().Yaw)) > 0.1);
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
