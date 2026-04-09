// Copyright ALIS. All Rights Reserved.
// Locomotion parity timeline: scripted 15-phase movement matrix for legacy vs modular.
// Produces JSONL timeline + phase summary JSON for machine-readable comparison.
// Run with -ProjectSkipFrontEnd to bypass menu travel.
// Output: Saved/Validation/CharacterDebug/ (JSONL + summary JSON per character system)

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

// ---------------------------------------------------------------------------
// Phase definition
// ---------------------------------------------------------------------------

struct FLocomotionPhase
{
	FString Name;
	float DurationSec;
	FVector2D MoveInput; // X=forward/back, Y=right/left (character-relative)
	float YawRateDegPerSec;
	bool bCrouch;
	bool bJump;
};

static const FLocomotionPhase GPhases[] = {
	{ TEXT("IdleSettle"),       2.0f, {0, 0},    0.f,   false, false },
	{ TEXT("ForwardAccel"),     1.0f, {1, 0},    0.f,   false, false },
	{ TEXT("ForwardSteady"),    2.0f, {1, 0},    0.f,   false, false },
	{ TEXT("ReleaseStop"),      1.5f, {0, 0},    0.f,   false, false },
	{ TEXT("Backward"),         2.0f, {-1, 0},   0.f,   false, false },
	{ TEXT("StrafeLeft"),       2.0f, {0, -1},   0.f,   false, false },
	{ TEXT("StrafeRight"),      2.0f, {0, 1},    0.f,   false, false },
	{ TEXT("DiagForwardLeft"),  2.0f, {1, -1},   0.f,   false, false },
	{ TEXT("DiagForwardRight"), 2.0f, {1, 1},    0.f,   false, false },
	{ TEXT("CrouchIdle"),       1.0f, {0, 0},    0.f,   true,  false },
	{ TEXT("CrouchForward"),    2.0f, {1, 0},    0.f,   true,  false },
	{ TEXT("Uncrouch"),         1.0f, {0, 0},    0.f,   false, false },
	{ TEXT("TurnLeft"),         1.5f, {0, 0},    -90.f, false, false },
	{ TEXT("TurnRight"),        1.5f, {0, 0},    90.f,  false, false },
	{ TEXT("JumpFallLand"),     2.0f, {0, 0},    0.f,   false, true  },
};
static constexpr int32 GNumPhases = UE_ARRAY_COUNT(GPhases);

// ---------------------------------------------------------------------------
// Phase summary accumulator
// ---------------------------------------------------------------------------

struct FPhaseSummary
{
	FString PhaseName;
	int32 SampleCount = 0;
	float PeakSpeed2D = 0.f;
	float MinFootZ = TNumericLimits<float>::Max();
	float MaxFootZ = TNumericLimits<float>::Lowest();
	bool bEnteredCrouch = false;
	bool bEnteredAir = false;
};

// ---------------------------------------------------------------------------
// Reflection helper (matches MotionMatchingCapability pattern)
// ---------------------------------------------------------------------------

namespace LocomotionHelpers
{

FProperty* FindProp(const UStruct* Owner, const TCHAR* Name)
{
	if (FProperty* P = Owner->FindPropertyByName(FName(Name))) return P;
	const FString Prefix = FString(Name) + TEXT("_");
	for (TFieldIterator<FProperty> It(Owner); It; ++It)
	{
		if (It->GetName().StartsWith(Prefix)) return *It;
	}
	return nullptr;
}

void ReadFloat(TSharedPtr<FJsonObject>& J, UClass* C, UAnimInstance* I, const TCHAR* Name)
{
	if (FProperty* P = FindProp(C, Name))
	{
		void* Ptr = P->ContainerPtrToValuePtr<void>(I);
		double V = CastField<FDoubleProperty>(P) ? *static_cast<double*>(Ptr)
			: CastField<FFloatProperty>(P) ? static_cast<double>(*static_cast<float*>(Ptr)) : 0.0;
		J->SetNumberField(Name, V);
	}
}

void ReadBool(TSharedPtr<FJsonObject>& J, UClass* C, UAnimInstance* I, const TCHAR* Name)
{
	if (FBoolProperty* P = CastField<FBoolProperty>(FindProp(C, Name)))
	{
		J->SetBoolField(Name, P->GetPropertyValue(P->ContainerPtrToValuePtr<void>(I)));
	}
}

void ReadByte(TSharedPtr<FJsonObject>& J, UClass* C, UAnimInstance* I, const TCHAR* Name)
{
	if (FProperty* P = FindProp(C, Name))
	{
		J->SetNumberField(Name, *static_cast<uint8*>(P->ContainerPtrToValuePtr<void>(I)));
	}
}

FString Vec3Str(const FVector& V) { return FString::Printf(TEXT("%.2f,%.2f,%.2f"), V.X, V.Y, V.Z); }
FString RotStr(const FRotator& R) { return FString::Printf(TEXT("%.2f,%.2f,%.2f"), R.Pitch, R.Yaw, R.Roll); }

} // namespace LocomotionHelpers

namespace LocomotionHelpers
{

// ---------------------------------------------------------------------------
// Latent command
// ---------------------------------------------------------------------------

class FLocomotionTimelineCommand : public IAutomationLatentCommand
{
public:
	FLocomotionTimelineCommand(FAutomationTestBase* InTest)
		: Test(InTest)
		, RunId(FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")))
		, OutputDir(FPaths::ProjectSavedDir() / TEXT("Validation/CharacterDebug"))
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
		case 0: return WaitForPawn();
		case 1: return Settle();
		case 2: return RunPhases(TEXT("Legacy"));
		case 3: return WriteSummary(TEXT("Legacy"));
		case 4: return SwitchToModular();
		case 5: return WaitForModular();
		case 6: return MutableSettle();
		case 7: return RunPhases(TEXT("Modular"));
		case 8: return WriteSummary(TEXT("Modular"));
		case 9: Test->AddInfo(TEXT("Locomotion timeline complete")); return true;
		default: return true;
		}
	}

private:
	// ---- Stage handlers ----

	bool WaitForPawn()
	{
		if (APlayerController* PC = FindPC())
		{
			World = PC->GetWorld();
			Test->AddInfo(FString::Printf(TEXT("Pawn: %s RunId: %s"),
				*PC->GetPawn()->GetClass()->GetName(), *RunId));
			NextStage(); return false;
		}
		if (Tick > 1800) { Test->AddError(TEXT("Timed out waiting for pawn")); return true; }
		return false;
	}

	bool Settle()
	{
		if (Tick < 120) return false;
		GEngine->Exec(World, TEXT("project.character.debug 1"));
		Test->AddInfo(TEXT("Settled. Starting legacy phases..."));
		ResetPhaseRunner(); NextStage(); return false;
	}

	bool SwitchToModular()
	{
		GEngine->Exec(World, TEXT("project.character.switch modular"));
		Test->AddInfo(TEXT("Switched to modular"));
		NextStage(); return false;
	}

	bool WaitForModular()
	{
		if (APlayerController* PC = FindPC())
		{
			if (PC->GetPawn() && PC->GetPawn()->GetClass()->GetName().Contains(TEXT("DefinitionCharacter")))
			{
				World = PC->GetWorld();
				NextStage(); return false;
			}
		}
		if (Tick > 1800) { Test->AddError(TEXT("Timed out for DefinitionCharacter")); return true; }
		return false;
	}

	bool MutableSettle()
	{
		if (Tick < 180) return false;
		Test->AddInfo(TEXT("Mutable settled. Starting modular phases..."));
		ResetPhaseRunner(); NextStage(); return false;
	}

	// ---- Phase runner (used by stages 2 and 7) ----

	bool RunPhases(const TCHAR* SystemLabel)
	{
		if (PhaseIdx >= GNumPhases) { NextStage(); return false; }

		if (!World) return true;
		const float DT = World->GetDeltaSeconds();
		if (DT <= 0.f) return false;

		APlayerController* PC = FindPC();
		if (!PC || !PC->GetPawn()) { Test->AddError(TEXT("Pawn lost")); return true; }
		ACharacter* Char = Cast<ACharacter>(PC->GetPawn());
		if (!Char) return true;

		const FLocomotionPhase& Phase = GPhases[PhaseIdx];

		// Phase start: one-time actions
		if (!bPhaseStarted)
		{
			bPhaseStarted = true;
			PhaseElapsed = 0.f;
			SampleAccum = 0.f;
			PhaseSampleIdx = 0;

			// Crouch/uncrouch
			if (Phase.bCrouch && !Char->bIsCrouched) Char->Crouch();
			if (!Phase.bCrouch && Char->bIsCrouched) Char->UnCrouch();
			if (Phase.bJump) Char->Jump();

			// Init summary
			FPhaseSummary S;
			S.PhaseName = Phase.Name;
			CurrentSummaries.Add(S);

			// Phase-start sample
			CaptureSample(Char, SystemLabel, Phase, PhaseSampleIdx);
			++PhaseSampleIdx;
		}

		// Apply movement input
		if (!Phase.MoveInput.IsNearlyZero())
		{
			const FRotator YawRot(0, Char->GetControlRotation().Yaw, 0);
			const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			FVector Dir = Fwd * Phase.MoveInput.X + Right * Phase.MoveInput.Y;
			Dir = Dir.GetSafeNormal();
			Char->AddMovementInput(Dir, 1.0f);
		}

		// Apply yaw input
		if (FMath::Abs(Phase.YawRateDegPerSec) > 0.f)
		{
			PC->AddYawInput(Phase.YawRateDegPerSec * DT);
		}

		// Sample every 0.25s
		SampleAccum += DT;
		if (SampleAccum >= 0.25f)
		{
			SampleAccum -= 0.25f;
			CaptureSample(Char, SystemLabel, Phase, PhaseSampleIdx);
			++PhaseSampleIdx;
		}

		// Phase end
		PhaseElapsed += DT;
		if (PhaseElapsed >= Phase.DurationSec)
		{
			// Phase-end sample
			CaptureSample(Char, SystemLabel, Phase, PhaseSampleIdx);

			Test->AddInfo(FString::Printf(TEXT("[%s] Phase %d/%d '%s' done (%d samples)"),
				SystemLabel, PhaseIdx + 1, GNumPhases, *Phase.Name, PhaseSampleIdx + 1));

			++PhaseIdx;
			bPhaseStarted = false;
		}

		return false;
	}

	bool WriteSummary(const TCHAR* SystemLabel)
	{
		// Write JSONL timeline
		FString TimelinePath = OutputDir / FString::Printf(TEXT("%s_timeline_%s.jsonl"),
			*FString(SystemLabel).ToLower(), *RunId);
		FFileHelper::SaveStringToFile(
			FString::Join(TimelineLines, TEXT("\n")),
			*TimelinePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test->AddInfo(FString::Printf(TEXT("[%s] Timeline: %d lines -> %s"),
			SystemLabel, TimelineLines.Num(), *TimelinePath));
		TimelineLines.Reset();

		// Write phase summaries
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("RunId"), RunId);
		Root->SetStringField(TEXT("System"), SystemLabel);

		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FPhaseSummary& S : CurrentSummaries)
		{
			TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("Phase"), S.PhaseName);
			P->SetNumberField(TEXT("Samples"), S.SampleCount);
			P->SetNumberField(TEXT("PeakSpeed2D"), S.PeakSpeed2D);
			P->SetNumberField(TEXT("FootVerticalRange"),
				(S.MaxFootZ > S.MinFootZ) ? (S.MaxFootZ - S.MinFootZ) : 0.f);
			P->SetBoolField(TEXT("EnteredCrouch"), S.bEnteredCrouch);
			P->SetBoolField(TEXT("EnteredAir"), S.bEnteredAir);
			Arr.Add(MakeShared<FJsonValueObject>(P));
		}
		Root->SetArrayField(TEXT("Phases"), Arr);

		FString SummaryStr;
		auto Writer = TJsonWriterFactory<>::Create(&SummaryStr);
		FJsonSerializer::Serialize(Root, Writer);

		FString SummaryPath = OutputDir / FString::Printf(TEXT("%s_summary_%s.json"),
			*FString(SystemLabel).ToLower(), *RunId);
		FFileHelper::SaveStringToFile(SummaryStr, *SummaryPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test->AddInfo(FString::Printf(TEXT("[%s] Summary: %d phases -> %s"),
			SystemLabel, CurrentSummaries.Num(), *SummaryPath));

		CurrentSummaries.Reset();
		NextStage();
		return false;
	}

	// ---- Sampling ----

	void CaptureSample(ACharacter* Char, const TCHAR* System,
		const FLocomotionPhase& Phase, int32 SampleIdx)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		UCharacterMovementComponent* CMC = Char->GetCharacterMovement();

		// Header
		Row->SetStringField(TEXT("RunId"), RunId);
		Row->SetStringField(TEXT("System"), System);
		Row->SetStringField(TEXT("Phase"), Phase.Name);
		Row->SetNumberField(TEXT("Sample"), SampleIdx);
		Row->SetNumberField(TEXT("Frame"), static_cast<double>(GFrameCounter));
		Row->SetNumberField(TEXT("WorldTime"), World->GetTimeSeconds());
		Row->SetNumberField(TEXT("PhaseTime"), PhaseElapsed);
		Row->SetNumberField(TEXT("DeltaTime"), World->GetDeltaSeconds());

		// Layer 1: Movement
		TSharedPtr<FJsonObject> L1 = MakeShared<FJsonObject>();
		L1->SetStringField(TEXT("Loc"), Vec3Str(Char->GetActorLocation()));
		L1->SetStringField(TEXT("Rot"), RotStr(Char->GetActorRotation()));
		L1->SetStringField(TEXT("CtrlRot"), RotStr(Char->GetControlRotation()));
		L1->SetStringField(TEXT("Vel"), Vec3Str(CMC->Velocity));
		L1->SetStringField(TEXT("Accel"), Vec3Str(CMC->GetCurrentAcceleration()));
		L1->SetNumberField(TEXT("MoveMode"), static_cast<int32>(CMC->MovementMode.GetValue()));
		L1->SetBoolField(TEXT("Crouch"), Char->bIsCrouched);
		L1->SetNumberField(TEXT("MaxSpeed"), CMC->MaxWalkSpeed);
		Row->SetObjectField(TEXT("Mvt"), L1);

		// Layer 2: ABP
		TSharedPtr<FJsonObject> L2 = MakeShared<FJsonObject>();
		UAnimInstance* ABP = FindABP(Char);
		if (ABP)
		{
			UClass* PC = ABP->GetClass();
			ReadFloat(L2, PC, ABP, TEXT("Speed2D"));
			ReadBool(L2, PC, ABP, TEXT("HasVelocity"));
			ReadBool(L2, PC, ABP, TEXT("HasAcceleration"));
			ReadFloat(L2, PC, ABP, TEXT("AccelerationAmount"));
			ReadByte(L2, PC, ABP, TEXT("MovementState"));
			ReadByte(L2, PC, ABP, TEXT("MovementMode"));
			ReadByte(L2, PC, ABP, TEXT("Gait"));
			ReadByte(L2, PC, ABP, TEXT("Stance"));
			ReadByte(L2, PC, ABP, TEXT("RotationMode"));
			ReadByte(L2, PC, ABP, TEXT("MovementDirection"));
			ReadBool(L2, PC, ABP, TEXT("NoValidAnim"));
		}
		Row->SetObjectField(TEXT("ABP"), L2);

		// Layer 3: Pose
		TSharedPtr<FJsonObject> L3 = MakeShared<FJsonObject>();
		USkeletalMeshComponent* Mesh = FindMesh(Char);
		float FootLZ = 0.f, FootRZ = 0.f;
		if (Mesh && Mesh->GetSkeletalMeshAsset())
		{
			static const FName Bones[] = {
				FName("pelvis"), FName("thigh_l"), FName("thigh_r"),
				FName("calf_l"), FName("calf_r"), FName("foot_l"), FName("foot_r"),
				FName("ball_l"), FName("ball_r")
			};
			const TArray<FTransform>& CS = Mesh->GetComponentSpaceTransforms();
			for (const FName& B : Bones)
			{
				int32 Idx = Mesh->GetBoneIndex(B);
				if (Idx != INDEX_NONE && CS.IsValidIndex(Idx))
				{
					const FVector& Loc = CS[Idx].GetLocation();
					const FRotator Rot = CS[Idx].GetRotation().Rotator();
					L3->SetStringField(B.ToString(),
						FString::Printf(TEXT("%.2f,%.2f,%.2f|%.2f,%.2f,%.2f"),
							Loc.X, Loc.Y, Loc.Z, Rot.Pitch, Rot.Yaw, Rot.Roll));

					if (B == FName("foot_l")) FootLZ = Loc.Z;
					if (B == FName("foot_r")) FootRZ = Loc.Z;
				}
			}
		}
		Row->SetObjectField(TEXT("Pose"), L3);

		// Serialize to JSONL line
		FString Line;
		auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
		FJsonSerializer::Serialize(Row.ToSharedRef(), Writer);
		TimelineLines.Add(Line);

		// Update phase summary
		if (CurrentSummaries.Num() > 0)
		{
			FPhaseSummary& S = CurrentSummaries.Last();
			++S.SampleCount;
			S.PeakSpeed2D = FMath::Max(S.PeakSpeed2D, CMC->Velocity.Size2D());
			S.MinFootZ = FMath::Min(S.MinFootZ, FMath::Min(FootLZ, FootRZ));
			S.MaxFootZ = FMath::Max(S.MaxFootZ, FMath::Max(FootLZ, FootRZ));
			S.bEnteredCrouch |= Char->bIsCrouched;
			S.bEnteredAir |= CMC->IsFalling();
		}
	}

	// ---- Helpers ----

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

	UAnimInstance* FindABP(ACharacter* Char) const
	{
		static const FName Tag(TEXT("AssemblyRole=DriverBody"));
		TArray<USkeletalMeshComponent*> Comps;
		Char->GetComponents<USkeletalMeshComponent>(Comps);
		for (USkeletalMeshComponent* C : Comps)
		{
			if (C->ComponentTags.Contains(Tag))
			{
				if (UAnimInstance* AI = C->GetAnimInstance()) return AI;
			}
		}
		if (USkeletalMeshComponent* M = Char->GetMesh()) return M->GetAnimInstance();
		return nullptr;
	}

	USkeletalMeshComponent* FindMesh(ACharacter* Char) const
	{
		static const FName Tag(TEXT("AssemblyRole=DriverBody"));
		TArray<USkeletalMeshComponent*> Comps;
		Char->GetComponents<USkeletalMeshComponent>(Comps);
		for (USkeletalMeshComponent* C : Comps)
		{
			if (C->ComponentTags.Contains(Tag)) return C;
		}
		return Char->GetMesh();
	}

	void NextStage() { ++Stage; Tick = 0; }

	void ResetPhaseRunner()
	{
		PhaseIdx = 0;
		bPhaseStarted = false;
		PhaseElapsed = 0.f;
		SampleAccum = 0.f;
		PhaseSampleIdx = 0;
	}

	// ---- Members ----

	FAutomationTestBase* Test = nullptr;
	UWorld* World = nullptr;
	const FString RunId;
	const FString OutputDir;

	int32 Stage = 0;
	int32 Tick = 0;
	uint64 LastFrame = 0;

	// Phase runner state
	int32 PhaseIdx = 0;
	bool bPhaseStarted = false;
	float PhaseElapsed = 0.f;
	float SampleAccum = 0.f;
	int32 PhaseSampleIdx = 0;

	// Output buffers
	TArray<FString> TimelineLines;
	TArray<FPhaseSummary> CurrentSummaries;
};

} // namespace LocomotionHelpers

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCharacterParityLocomotionTest,
	"ProjectIntegrationTests.Character.Parity.LocomotionTimeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FCharacterParityLocomotionTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(LocomotionHelpers::FLocomotionTimelineCommand(this));
	return true;
}
