// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectSkeletalAssemblyModule.h"
#include "CharacterDebugCaptureComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogSkeletalAssembly);

#define LOCTEXT_NAMESPACE "FProjectSkeletalAssemblyModule"

namespace
{
	// Find or create DebugCapture on the current pawn.
	// Auto-creates for legacy characters that don't have it in their definition.
	UCharacterDebugCaptureComponent* FindOrCreateDebugCapture()
	{
		UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
		if (!World) return nullptr;

		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC || !PC->GetPawn()) return nullptr;

		APawn* Pawn = PC->GetPawn();
		UCharacterDebugCaptureComponent* Comp = Pawn->FindComponentByClass<UCharacterDebugCaptureComponent>();

		if (!Comp)
		{
			Comp = NewObject<UCharacterDebugCaptureComponent>(Pawn);
			Comp->RegisterComponent();
			UE_LOG(LogSkeletalAssembly, Log,
				TEXT("Auto-created DebugCapture component on %s"), *Pawn->GetName());
		}

		return Comp;
	}

	FAutoConsoleCommand CmdDebug(
		TEXT("project.character.debug"),
		TEXT("Toggle character debug overlay. Usage: project.character.debug 0|1"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UCharacterDebugCaptureComponent* Comp = FindOrCreateDebugCapture();
			if (!Comp)
			{
				UE_LOG(LogSkeletalAssembly, Warning, TEXT("project.character.debug: no pawn available"));
				return;
			}

			bool bEnable = true;
			if (Args.Num() > 0)
			{
				bEnable = Args[0] != TEXT("0");
			}
			else
			{
				bEnable = !Comp->IsOverlayEnabled();
			}
			Comp->SetOverlayEnabled(bEnable);
		}));

	FAutoConsoleCommand CmdCapture(
		TEXT("project.character.capture"),
		TEXT("Capture character debug snapshot. Usage: project.character.capture [label]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UCharacterDebugCaptureComponent* Comp = FindOrCreateDebugCapture();
			if (!Comp)
			{
				UE_LOG(LogSkeletalAssembly, Warning, TEXT("project.character.capture: no pawn available"));
				return;
			}

			FString Label = Args.Num() > 0 ? Args[0] : TEXT("");
			Comp->CaptureSnapshot(Label);
		}));
}

void FProjectSkeletalAssemblyModule::StartupModule()
{
	UE_LOG(LogSkeletalAssembly, Log, TEXT("ProjectSkeletalAssembly module started."));
}

void FProjectSkeletalAssemblyModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSkeletalAssemblyModule, ProjectSkeletalAssembly)
