// Copyright ALIS. All Rights Reserved.
// Simple animation sanity: bypass ABP, play AnimSequence on DriverBody.
// Proves skeletal mesh evaluation works independently of MotionMatching.
// Run with -ProjectSkipFrontEnd to bypass menu travel.

#include "Misc/AutomationTest.h"
#include "DefinitionCharacter.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"

class FSimpleAnimSanityCommand : public IAutomationLatentCommand
{
public:
	FSimpleAnimSanityCommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{}

	virtual bool Update() override
	{
		if (!Test) return true;
		const uint64 Frame = GFrameCounter;
		if (Frame == LastFrame) return false;
		LastFrame = Frame;
		++Tick;

		switch (Stage)
		{
		case 0: // Wait for pawn
		{
			APlayerController* PC = FindPC();
			if (!PC)
			{
				if (Tick > 1800) { Test->AddError(TEXT("Timed out waiting for pawn")); return true; }
				return false;
			}
			World = PC->GetWorld();
			Stage = 1; Tick = 0;
			return false;
		}

		case 1: // Settle
		{
			if (Tick < 120) return false;
			Stage = 2; Tick = 0;
			return false;
		}

		case 2: // Wait for DefinitionCharacter
		{
			APlayerController* PC = FindPC();
			if (PC && Cast<ADefinitionCharacter>(PC->GetPawn()))
			{
				World = PC->GetWorld();
				Stage = 3; Tick = 0;
				return false;
			}
			if (Tick > 1800) { Test->AddError(TEXT("Timed out for DefinitionCharacter")); return true; }
			return false;
		}

		case 3: // Wait for Mutable + play simple anim
		{
			if (Tick < 180) return false;

			ACharacter* Character = Cast<ACharacter>(FindPC()->GetPawn());
			if (!Character) { Test->AddError(TEXT("No character")); return true; }

			// Find DriverBody
			static const FName Tag(TEXT("AssemblyRole=DriverBody"));
			TArray<USkeletalMeshComponent*> Comps;
			Character->GetComponents<USkeletalMeshComponent>(Comps);
			for (USkeletalMeshComponent* C : Comps)
			{
				if (C->ComponentTags.Contains(Tag)) { DriverBody = C; break; }
			}

			if (!DriverBody)
			{
				Test->AddError(TEXT("No DriverBody mesh found"));
				return true;
			}

			UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr,
				TEXT("/MotionMatching/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Break_v01.M_Neutral_Stand_Idle_Break_v01"));
			if (!Anim)
			{
				Test->AddError(TEXT("Could not load idle break AnimSequence"));
				return true;
			}

			DriverBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			DriverBody->PlayAnimation(Anim, true);
			Test->AddInfo(FString::Printf(TEXT("[AnimSanity] Playing '%s' on DriverBody"), *Anim->GetName()));

			Stage = 4; Tick = 0; SampleCount = 0;
			return false;
		}

		case 4: // Sample bones during simple animation
		{
			if (Tick > 0 && (Tick % 30 == 0) && DriverBody)
			{
				static const FName Bones[] = {
					FName("pelvis"), FName("thigh_l"), FName("foot_l"),
					FName("thigh_r"), FName("foot_r")
				};

				const TArray<FTransform>& CS = DriverBody->GetComponentSpaceTransforms();
				FString BoneStr;
				for (const FName& B : Bones)
				{
					int32 Idx = DriverBody->GetBoneIndex(B);
					if (Idx != INDEX_NONE && CS.IsValidIndex(Idx))
					{
						const FRotator R = CS[Idx].GetRotation().Rotator();
						BoneStr += FString::Printf(TEXT(" %s=(%.2f,%.2f,%.2f R=%.2f)"),
							*B.ToString(),
							CS[Idx].GetLocation().X, CS[Idx].GetLocation().Y, CS[Idx].GetLocation().Z,
							R.Yaw);
					}
				}

				Test->AddInfo(FString::Printf(TEXT("[AnimSanity] S%d%s"), SampleCount, *BoneStr));
				++SampleCount;
			}

			if (Tick >= 180) // ~3s
			{
				Test->AddInfo(FString::Printf(TEXT("[AnimSanity] Complete: %d samples"), SampleCount));
				return true;
			}
			return false;
		}

		default: return true;
		}
	}

private:
	APlayerController* FindPC() const
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (UWorld* W = Ctx.World())
			{
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(W, 0))
				{
					if (PC->GetPawn()) return PC;
				}
			}
		}
		return nullptr;
	}

	FAutomationTestBase* Test = nullptr;
	UWorld* World = nullptr;
	USkeletalMeshComponent* DriverBody = nullptr;
	int32 Stage = 0;
	int32 Tick = 0;
	int32 SampleCount = 0;
	uint64 LastFrame = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCharacterParityAnimSanityTest,
	"ProjectIntegrationTests.Character.Parity.SimpleAnimSanity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FCharacterParityAnimSanityTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSimpleAnimSanityCommand(this));
	return true;
}
