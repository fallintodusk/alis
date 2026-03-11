// Copyright ALIS. All Rights Reserved.

#include "Components/ProjectMindComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Interfaces/IMindRuntimeControl.h"
#include "ProjectServiceLocator.h"

UProjectMindComponent::UProjectMindComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProjectMindComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (!PawnOwner || !PawnOwner->IsPlayerControlled())
	{
		return;
	}

	const AController* Controller = PawnOwner->GetController();
	if (Controller && !Controller->IsLocalController())
	{
		return;
	}

	const TSharedPtr<IMindRuntimeControl> MindRuntimeControl = FProjectServiceLocator::Resolve<IMindRuntimeControl>();
	if (MindRuntimeControl.IsValid())
	{
		MindRuntimeControl->SetLocalPlayerPawn(PawnOwner);
	}
}

void UProjectMindComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	const TSharedPtr<IMindRuntimeControl> MindRuntimeControl = FProjectServiceLocator::Resolve<IMindRuntimeControl>();
	if (MindRuntimeControl.IsValid() && PawnOwner)
	{
		MindRuntimeControl->ClearLocalPlayerPawn(PawnOwner);
	}

	Super::EndPlay(EndPlayReason);
}
