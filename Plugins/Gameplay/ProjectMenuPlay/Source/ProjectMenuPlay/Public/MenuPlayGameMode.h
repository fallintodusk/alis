#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MenuPlayGameMode.generated.h"

/**
 * GameMode for the main menu world.
 * - Spawns a minimal spectator pawn at a code-created PlayerStart
 * - No map PlayerStart/uasset changes needed
 * - Uses a custom PlayerController for UI only
 */
UCLASS()
class PROJECTMENUPLAY_API AMenuPlayGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMenuPlayGameMode(const FObjectInitializer& ObjectInitializer);

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;

    /** Called after a new player logs in. We still spawn normally, then configure UI input. */
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

    /** Provide our own PlayerStart so engine never logs or crashes. */
    virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

    /** Route ChoosePlayerStart to our FindPlayerStart implementation. */
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
    /** Cached PlayerStart used for the menu world (spawned in code, not from map). */
    UPROPERTY()
    AActor* MenuStartSpot = nullptr;
};
