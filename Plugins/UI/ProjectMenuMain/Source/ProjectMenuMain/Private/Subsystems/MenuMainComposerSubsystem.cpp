// Copyright ALIS.

#include "Subsystems/MenuMainComposerSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Logging/LogMacros.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Blueprint/UserWidget.h"

// FAnchors is included transitively via UMG -> Blueprint/UserWidget.h

DEFINE_LOG_CATEGORY_STATIC(LogMenuMainComposer, Log, All);

void UMenuMainComposerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMenuMainComposerSubsystem::Deinitialize()
{
	ActiveMenuWidget.Reset();
	Super::Deinitialize();
}

void UMenuMainComposerSubsystem::ShowMainMenu(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (ActiveMenuWidget.IsValid())
	{
		return; // Already shown
	}

	if (UProjectUILayerHostSubsystem* LayerHost = GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
	{
		// Main menu only needs layer stack, not HUD layout
		LayerHost->InitializeForPlayer(PlayerController, /*bInitializeHUD=*/false);
		if (UUserWidget* Menu = LayerHost->ShowDefinition(TEXT("ProjectMenuMain.MainMenu")))
		{
			ActiveMenuWidget = Menu;
			UE_LOG(LogMenuMainComposer, Log, TEXT("Main menu shown via ProjectUI layer host"));
		}
	}
}

void UMenuMainComposerSubsystem::RequestStartGame(const FString& MapId, const FString& GameMode)
{
	UE_LOG(LogMenuMainComposer, Display, TEXT("RequestStartGame: MapId=%s, GameMode=%s"), *MapId, *GameMode);
	OnRequestStartGame.Broadcast(MapId, GameMode);
}
