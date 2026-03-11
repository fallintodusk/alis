// Composer subsystem that requests ProjectUI to show the main menu widget.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MenuMainComposerSubsystem.generated.h"

class UUserWidget;
class APlayerController;

/** Delegate for game start requests from menu UI */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRequestStartGame, const FString&, MapId, const FString&, GameMode);

UCLASS()
class PROJECTMENUMAIN_API UMenuMainComposerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Shows the main menu via ProjectUI layer host. */
	void ShowMainMenu(APlayerController* PlayerController);

	/** Called by widget to request starting a game. Controller subscribes to this. */
	void RequestStartGame(const FString& MapId, const FString& GameMode);

	/** Delegate broadcast when UI requests starting a game */
	UPROPERTY(BlueprintAssignable, Category = "Menu")
	FOnRequestStartGame OnRequestStartGame;

private:
	TWeakObjectPtr<UUserWidget> ActiveMenuWidget;
};
