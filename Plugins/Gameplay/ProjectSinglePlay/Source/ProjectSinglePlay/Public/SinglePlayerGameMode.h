#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"
#include "SinglePlayModeConfig.h"
#include "SinglePlayerGameMode.generated.h"

UENUM()
enum class ESinglePlayCharacterSystem : uint8
{
	Legacy,
	Modular
};

/**
 * GameMode for single-player gameplay in ALIS.
 *
 * Acts as a thin orchestrator that:
 * - Parses URL options to determine mode configuration
 * - Loads UE layer classes (Pawn, PlayerController) from config
 * - Initializes features via FFeatureRegistry (features attach own components)
 * - Configures gameplay input mode for players
 * - Listens for death (via VitalsComponent delegate) and reloads map
 *
 * URL Game Mode Syntax:
 *   Full class path WITHOUT 'A' prefix (UClass name != C++ class name):
 *   ?game=/Script/ProjectSinglePlay.SinglePlayerGameMode
 *
 *   Optional alias (if configured in DefaultEngine.ini GameModeClassAliases):
 *   ?game=SinglePlayer
 *   NOTE: Alias is NOT configured by default - use full path for modular/decoupled approach.
 *
 *   Example: ServerTravel("MapName?Mode=Medium?CharacterSystem=Modular?CharacterDefinition=Hero?game=/Script/ProjectSinglePlay.SinglePlayerGameMode")
 *
 * Why no 'A' prefix?
 *   C++ naming: ASinglePlayerGameMode (with A prefix for Actors)
 *   UClass naming: SinglePlayerGameMode (reflection name, NO prefix)
 *   LoadClass<> expects the UClass name, not the C++ class name.
 *
 * URL Rules:
 *   - Use ? separator for ALL options (not & like web URLs)
 *   - game= option should come LAST (UE parser reads value to end-of-string)
 *   - Mode selects gameplay/difficulty configuration
 *   - CharacterSystem selects legacy vs modular pawn implementation independently
 *
 * UE Engine References:
 *   - UGameInstance::CreateGameModeForURL - Engine/Source/Runtime/Engine/Private/GameInstance.cpp:1490
 *   - UGameMapsSettings::GetGameModeForName - Engine/Source/Runtime/EngineSettings/Private/EngineSettingsModule.cpp:117
 *   - LoadClass<AGameModeBase> call at GameInstance.cpp:1521
 *
 * This class is world-agnostic - it does not reference specific maps or world plugins.
 * Features self-register on module startup; GameMode just calls InitializeFeature for each.
 */
UCLASS()
class PROJECTSINGLEPLAY_API ASinglePlayerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASinglePlayerGameMode();

	//~ Begin AGameModeBase Interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	//~ End AGameModeBase Interface

	// Get the current mode configuration
	UFUNCTION(BlueprintPure, Category = "Single Play")
	const FSinglePlayModeConfig& GetModeConfig() const { return ModeConfig; }

	ESinglePlayCharacterSystem GetActiveCharacterSystem() const { return ActiveCharacterSystem; }
	FName GetActiveCharacterSystemName() const;
	const FPrimaryAssetId& GetActiveCharacterDefinitionId() const { return ActiveCharacterDefinitionId; }

	// Switch the active character system and optionally respawn existing players through GameMode.
	bool SwitchCharacterSystemRuntime(
		ESinglePlayCharacterSystem NewSystem,
		const FPrimaryAssetId& RequestedDefinitionId = FPrimaryAssetId(),
		bool bRespawnExistingPlayers = true);

protected:
	// Load mode configuration based on URL parameter
	// Returns default config if ModeParam is empty or unknown
	virtual FSinglePlayModeConfig LoadModeConfig(const FString& ModeParam);

	// Parse non-mode character selection options from URL.
	virtual void LoadCharacterSelection(const FString& CharacterSystemParam, const FString& CharacterDefinitionParam);

	// Resolve the effective definition ID for the active character system.
	virtual FPrimaryAssetId ResolveCharacterDefinitionId(
		ESinglePlayCharacterSystem NewSystem,
		const FPrimaryAssetId& RequestedDefinitionId) const;

	// Ensure required feature plugins are loaded before proceeding
	// Called during InitGame after ModeConfig is loaded
	// TODO: Integrate with Orchestrator's on-demand loading API
	virtual void EnsureFeaturePluginsLoaded();

	// Shared post-spawn initialization for new pawns.
	virtual void InitializePlayerPawn(AController* PlayerController);

	// Destroy the current pawn and respawn through the active character selection.
	virtual bool RespawnPlayerForCurrentCharacterSelection(AController* PlayerController);

	// Initialize features via FFeatureRegistry
	// Called during HandleStartingNewPlayer after pawn spawn
	// Features self-register on module startup and attach their own components
	virtual void InitializeFeatures(APawn* Pawn);

	// Verify that configured features are registered in FFeatureRegistry
	// Called during BeginPlay as a safety check (does not inspect pawn components)
	virtual void VerifyFeatures();

protected:
	// Current mode configuration loaded during InitGame
	UPROPERTY(BlueprintReadOnly, Category = "Single Play")
	FSinglePlayModeConfig ModeConfig;

	// Character implementation selection is independent from gameplay mode/difficulty.
	ESinglePlayCharacterSystem ActiveCharacterSystem = ESinglePlayCharacterSystem::Modular;
	FPrimaryAssetId ActiveCharacterDefinitionId;

private:
	// Track whether we've already verified features (for idempotency)
	bool bHasVerifiedFeatures = false;

	// -------------------------------------------------------------------------
	// Death Response (single-player specific: fade + reload)
	// -------------------------------------------------------------------------

	// Bind to VitalsComponent delegates on the pawn
	void BindVitalsResponse(APlayerController* PC);

	// Damage taken: brief red camera flash
	UFUNCTION()
	void HandleDamageTaken(float Amount);

	// VitalsComponent fires this when Condition reaches zero
	UFUNCTION()
	void HandleConditionDepleted();

	// Reload the current experience/map via ILoadingService
	void ReloadCurrentExperience();

	// Timer for delayed reload after death
	FTimerHandle DeathReloadTimerHandle;

	// Guard against multiple death triggers
	bool bDeathSequenceStarted = false;

	// Damage flash cooldown (prevents constant red from bleeding tick)
	double LastDamageFlashTimeSec = 0.0;
};
