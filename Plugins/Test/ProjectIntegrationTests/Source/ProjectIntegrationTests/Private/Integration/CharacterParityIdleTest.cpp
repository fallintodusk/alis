// Copyright ALIS. All Rights Reserved.
// Idle parity snapshot: capture legacy and modular hero at rest.
// Run with -ProjectSkipFrontEnd to bypass menu travel.
// Output: Saved/Validation/CharacterDebug/ (JSON sidecars with unique RunId)

#include "Misc/AutomationTest.h"
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
		, RunId(FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")))
		, LegacyLabel(FString::Printf(TEXT("idle_legacy_%s"), *RunId))
		, ModularLabel(FString::Printf(TEXT("idle_modular_%s"), *RunId))
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
			Test->AddInfo(FString::Printf(TEXT("Pawn: %s RunId: %s"),
				*PC->GetPawn()->GetClass()->GetName(), *RunId));
			Stage = 1; Tick = 0;
			return false;
		}

		case 1: // Settle
		{
			if (Tick < 120) return false;
			GEngine->Exec(World, TEXT("project.character.debug 1"));
			GEngine->Exec(World, *FString::Printf(TEXT("project.character.capture %s"), *LegacyLabel));
			Test->AddInfo(FString::Printf(TEXT("Legacy capture: %s"), *LegacyLabel));
			Stage = 2; Tick = 0;
			return false;
		}

		case 2: // Poll legacy file
		{
			if (FindFile(LegacyLabel)) { Stage = 3; Tick = 0; return false; }
			if (Tick > 120) { Test->AddError(TEXT("Legacy JSON not created")); return true; }
			return false;
		}

		case 3: // Switch to modular
		{
			GEngine->Exec(World, TEXT("project.character.switch modular"));
			Test->AddInfo(TEXT("Switched to modular"));
			Stage = 4; Tick = 0;
			return false;
		}

		case 4: // Wait for DefinitionCharacter
		{
			APlayerController* PC = FindPC();
			if (PC && PC->GetPawn() &&
				PC->GetPawn()->GetClass()->GetName().Contains(TEXT("DefinitionCharacter")))
			{
				World = PC->GetWorld();
				Stage = 5; Tick = 0;
				return false;
			}
			if (Tick > 1800) { Test->AddError(TEXT("Timed out waiting for DefinitionCharacter")); return true; }
			return false;
		}

		case 5: // Wait for Mutable rebuild
		{
			if (Tick < 180) return false;
			GEngine->Exec(World, *FString::Printf(TEXT("project.character.capture %s"), *ModularLabel));
			Test->AddInfo(FString::Printf(TEXT("Modular capture: %s"), *ModularLabel));
			Stage = 6; Tick = 0;
			return false;
		}

		case 6: // Poll modular file
		{
			if (FindFile(ModularLabel))
			{
				Test->AddInfo(TEXT("Both idle captures verified"));
				return true;
			}
			if (Tick > 120) { Test->AddError(TEXT("Modular JSON not created")); return true; }
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
	const FString LegacyLabel;
	const FString ModularLabel;
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
