#include "MenuPlayGameMode.h"

#include "MenuPlayPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogMenuPlayGameMode, Log, All);

AMenuPlayGameMode::AMenuPlayGameMode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Minimal pawn that costs nothing and has no visible mesh.
    DefaultPawnClass = ASpectatorPawn::StaticClass();

    // Players start as spectators by design in the menu.
    bStartPlayersAsSpectators = true;

    // Use custom menu player controller for UI logic.
    PlayerControllerClass = AMenuPlayPlayerController::StaticClass();

    MenuStartSpot = nullptr;
}

void AMenuPlayGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    UE_LOG(LogMenuPlayGameMode, Log,
        TEXT("MenuPlayGameMode InitGame (map=%s)"),
        *MapName);
}

void AMenuPlayGameMode::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogMenuPlayGameMode, Log,
        TEXT("MenuPlayGameMode BeginPlay"));
}

void AMenuPlayGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // Let base class do whatever bookkeeping it needs.
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    if (!IsValid(NewPlayer))
    {
        UE_LOG(LogMenuPlayGameMode, Warning,
            TEXT("HandleStartingNewPlayer_Implementation: NewPlayer is invalid, skipping setup"));
        return;
    }

    // Configure controller for UI-only input.
    if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
    {
        PC->bShowMouseCursor = true;

        FInputModeUIOnly InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);

        UE_LOG(LogMenuPlayGameMode, Log,
            TEXT("HandleStartingNewPlayer_Implementation: configured UI-only input for menu"));
    }
}

AActor* AMenuPlayGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
    // If we already spawned one, just reuse it.
    if (MenuStartSpot && IsValid(MenuStartSpot))
    {
        UE_LOG(LogMenuPlayGameMode, Verbose,
            TEXT("FindPlayerStart_Implementation (menu): reusing cached start %s"),
            *MenuStartSpot->GetName());
        return MenuStartSpot;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogMenuPlayGameMode, Warning,
            TEXT("FindPlayerStart_Implementation (menu): World is null, falling back to Super"));
        return Super::FindPlayerStart_Implementation(Player, IncomingName);
    }

    // Spawn a transient PlayerStart actor in code so:
    // - Engine never logs PATHS NOT DEFINED / NO PLAYERSTART
    // - No map/uasset needs to be changed
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;  // do not save to map
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    const FVector SpawnLocation = FVector::ZeroVector;
    const FRotator SpawnRotation = FRotator::ZeroRotator;

    APlayerStart* SpawnedStart = World->SpawnActor<APlayerStart>(
        APlayerStart::StaticClass(),
        SpawnLocation,
        SpawnRotation,
        Params
    );

    if (!SpawnedStart)
    {
        UE_LOG(LogMenuPlayGameMode, Warning,
            TEXT("FindPlayerStart_Implementation (menu): failed to spawn PlayerStart, falling back to Super"));
        return Super::FindPlayerStart_Implementation(Player, IncomingName);
    }

    MenuStartSpot = SpawnedStart;

    UE_LOG(LogMenuPlayGameMode, Log,
        TEXT("FindPlayerStart_Implementation (menu): spawned menu PlayerStart %s at %s"),
        *SpawnedStart->GetName(),
        *SpawnedStart->GetActorLocation().ToString());

    return MenuStartSpot;
}

AActor* AMenuPlayGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    // Route to our custom FindPlayerStart_Implementation so engine
    // never hits its own "no PlayerStart" path.
    return FindPlayerStart_Implementation(Player, FString());
}
