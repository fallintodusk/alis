// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectMindRuntimeBootstrapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IMindRuntimeControl.h"
#include "ProjectServiceLocator.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMindBootstrap, Log, All);

bool UProjectMindRuntimeBootstrapSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	return !IsRunningDedicatedServer();
}

void UProjectMindRuntimeBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetOnPawnControllerChanged().AddUniqueDynamic(
			this, &UProjectMindRuntimeBootstrapSubsystem::HandlePawnControllerChanged);
	}

	ApplyResolvedPawn(ResolvePrimaryLocalPawn());
	UE_LOG(LogProjectMindBootstrap, Log, TEXT("ProjectMind bootstrap subsystem initialized"));
}

void UProjectMindRuntimeBootstrapSubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetOnPawnControllerChanged().RemoveDynamic(
			this, &UProjectMindRuntimeBootstrapSubsystem::HandlePawnControllerChanged);
	}

	const TSharedPtr<IMindRuntimeControl> MindRuntimeControl = FProjectServiceLocator::Resolve<IMindRuntimeControl>();
	if (MindRuntimeControl.IsValid())
	{
		if (const APawn* LastPawn = LastAppliedLocalPawn.Get())
		{
			MindRuntimeControl->ClearLocalPlayerPawn(LastPawn);
		}
	}

	LastAppliedLocalPawn.Reset();
	UE_LOG(LogProjectMindBootstrap, Log, TEXT("ProjectMind bootstrap subsystem deinitialized"));

	Super::Deinitialize();
}

void UProjectMindRuntimeBootstrapSubsystem::HandlePawnControllerChanged(APawn* Pawn, AController* Controller)
{
	(void)Pawn;
	(void)Controller;
	ApplyResolvedPawn(ResolvePrimaryLocalPawn());
}

void UProjectMindRuntimeBootstrapSubsystem::ApplyResolvedPawn(APawn* ResolvedPawn)
{
	const APawn* LastPawn = LastAppliedLocalPawn.Get();
	if (LastPawn == ResolvedPawn)
	{
		return;
	}

	const TSharedPtr<IMindRuntimeControl> MindRuntimeControl = FProjectServiceLocator::Resolve<IMindRuntimeControl>();
	if (!MindRuntimeControl.IsValid())
	{
		return;
	}

	if (LastPawn)
	{
		MindRuntimeControl->ClearLocalPlayerPawn(LastPawn);
	}

	if (ResolvedPawn)
	{
		MindRuntimeControl->SetLocalPlayerPawn(ResolvedPawn);
	}

	LastAppliedLocalPawn = ResolvedPawn;
}

APawn* UProjectMindRuntimeBootstrapSubsystem::ResolvePrimaryLocalPawn() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		if (!LocalPlayer)
		{
			continue;
		}

		APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld());
		if (!PlayerController || !PlayerController->IsLocalController())
		{
			continue;
		}

		APawn* PlayerPawn = PlayerController->GetPawn();
		if (PlayerPawn && PlayerPawn->IsPlayerControlled())
		{
			return PlayerPawn;
		}
	}

	return nullptr;
}
