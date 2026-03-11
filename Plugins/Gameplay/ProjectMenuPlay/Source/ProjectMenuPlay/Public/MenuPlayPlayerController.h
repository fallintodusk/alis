// Minimal player controller for menu world: shows mouse cursor and enables UI input.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MenuPlayPlayerController.generated.h"

UCLASS()
class PROJECTMENUPLAY_API AMenuPlayPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMenuPlayPlayerController();

protected:
	virtual void BeginPlay() override;

	/** Callback when menu UI requests starting a game */
	UFUNCTION()
	void OnRequestStartGame(const FString& MapId, const FString& GameMode);

private:
	/** Start the loading pipeline for a map */
	void StartGameLoad(const FString& MapId, const FString& GameMode);
};
