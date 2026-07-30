// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
// Idle regression snapshot: capture the definition-driven hero at rest.
// Run with -ProjectSkipFrontEnd to bypass menu travel.
// Output: Saved/Validation/CharacterDebug/ (JSON sidecars with unique RunId)

#include "Misc/AutomationTest.h"
#include "DefinitionCharacter.h"
#include "Support/CharacterTestRunContext.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"

class FIdleSnapshotCommand : public IAutomationLatentCommand
{
public:
	FIdleSnapshotCommand(FAutomationTestBase* InTest)
		: Test(InTest)
		, RunId(ProjectCharacterTest::ResolveCaptureRunId())
		, DefinitionLabel(FString::Printf(TEXT("idle_definition_%s"), *RunId))
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
		case 0: // Wait for possessed pawn
		{
			APlayerController* PC = FindPC();
			if (!PC)
			{
				if (Tick > 1800) { Test->AddError(TEXT("Timed out waiting for pawn")); return true; }
				return false;
			}
			World = PC->GetWorld();
			if (!Cast<ADefinitionCharacter>(PC->GetPawn()))
			{
				Test->AddError(FString::Printf(TEXT("Expected DefinitionCharacter, got %s"),
					*PC->GetPawn()->GetClass()->GetPathName()));
				return true;
			}
			Test->AddInfo(FString::Printf(TEXT("Pawn: %s RunId: %s"),
				*PC->GetPawn()->GetClass()->GetName(), *RunId));
			Stage = 1; Tick = 0;
			return false;
		}

		case 1: // Wait for Mutable rebuild and capture
		{
			if (Tick < 180) return false;
			GEngine->Exec(World, TEXT("project.character.debug 1"));
			GEngine->Exec(World, *FString::Printf(TEXT("project.character.capture %s"), *DefinitionLabel));
			Test->AddInfo(FString::Printf(TEXT("Definition capture: %s"), *DefinitionLabel));
			Stage = 2; Tick = 0;
			return false;
		}

		case 2: // Poll capture file
		{
			if (FindFile(DefinitionLabel))
			{
				Test->AddInfo(TEXT("Definition-driven idle capture verified"));
				return true;
			}
			if (Tick > 120) { Test->AddError(TEXT("Definition capture JSON not created")); return true; }
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

	bool FindFile(const FString& Label) const
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files,
			*(OutputDir / FString::Printf(TEXT("*%s*.json"), *Label)), true, false);
		return Files.Num() > 0;
	}

	FAutomationTestBase* Test = nullptr;
	UWorld* World = nullptr;
	const FString RunId;
	const FString DefinitionLabel;
	const FString OutputDir;
	int32 Stage = 0;
	int32 Tick = 0;
	uint64 LastFrame = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCharacterParityIdleTest,
	"ProjectIntegrationTests.Character.Parity.IdleSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FCharacterParityIdleTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FIdleSnapshotCommand(this));
	return true;
}
