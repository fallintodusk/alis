#include "SinglePlayerGameMode.h"
#include "SinglePlayModeRegistry.h"
#include "FeatureRegistry.h"
#include "FeatureInitContext.h"
#include "ProjectSinglePlayLog.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"
#include "Engine/GameInstance.h"
#include "Engine/AssetManager.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Spawning/ObjectSpawnUtility.h"
#include "Data/ObjectDefinition.h"

namespace SinglePlayCharacterRuntime
{
	static constexpr TCHAR CharacterSystemOptionName[] = TEXT("CharacterSystem");
	static constexpr TCHAR CharacterDefinitionOptionName[] = TEXT("CharacterDefinition");
	static constexpr TCHAR LegacySystemToken[] = TEXT("legacy");
	static constexpr TCHAR ModularSystemToken[] = TEXT("modular");
	static constexpr TCHAR DefaultModularDefinition[] = TEXT("ObjectDefinition:Hero");

	static FPrimaryAssetId GetDefaultModularDefinitionId()
	{
		return FPrimaryAssetId::FromString(DefaultModularDefinition);
	}

	static bool TryParseCharacterSystem(const FString& Value, ESinglePlayCharacterSystem& OutSystem)
	{
		if (Value.Equals(LegacySystemToken, ESearchCase::IgnoreCase))
		{
			OutSystem = ESinglePlayCharacterSystem::Legacy;
			return true;
		}

		if (Value.Equals(ModularSystemToken, ESearchCase::IgnoreCase))
		{
			OutSystem = ESinglePlayCharacterSystem::Modular;
			return true;
		}

		return false;
	}

	static FPrimaryAssetId ParseCharacterDefinitionId(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return FPrimaryAssetId();
		}

		if (Value.Contains(TEXT(":")))
		{
			return FPrimaryAssetId::FromString(Value);
		}

		return FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(*Value));
	}

	static const TCHAR* ToToken(ESinglePlayCharacterSystem System)
	{
		return System == ESinglePlayCharacterSystem::Modular ? ModularSystemToken : LegacySystemToken;
	}

#if !UE_BUILD_SHIPPING
	static void HandleCharacterSwitchCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("project.character.switch: No world context available"));
			return;
		}

		ASinglePlayerGameMode* GameMode = World->GetAuthGameMode<ASinglePlayerGameMode>();
		if (!GameMode)
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("project.character.switch: No ASinglePlayerGameMode active in world '%s'"),
				*World->GetName());
			return;
		}

		if (Args.Num() < 1 || Args.Num() > 2)
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("Usage: project.character.switch <legacy|modular> [ObjectDefinitionIdOrName]"));
			return;
		}

		ESinglePlayCharacterSystem RequestedSystem = ESinglePlayCharacterSystem::Legacy;
		if (!TryParseCharacterSystem(Args[0], RequestedSystem))
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("project.character.switch: Unknown character system '%s' (expected legacy or modular)"),
				*Args[0]);
			return;
		}

		const FPrimaryAssetId RequestedDefinitionId =
			Args.Num() > 1 ? ParseCharacterDefinitionId(Args[1]) : FPrimaryAssetId();

		if (!GameMode->SwitchCharacterSystemRuntime(RequestedSystem, RequestedDefinitionId, true))
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("project.character.switch: Respawn reported problems for system '%s'"),
				ToToken(RequestedSystem));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CharacterSwitchCommand(
		TEXT("project.character.switch"),
		TEXT("Respawn current single-player players with character system legacy or modular. Optional second arg overrides modular ObjectDefinition."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleCharacterSwitchCommand));
#endif
}

ASinglePlayerGameMode::ASinglePlayerGameMode()
{
	// Set default classes - can be overridden by ModeConfig in InitGame
	PlayerControllerClass = APlayerController::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
}

void ASinglePlayerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Parse Mode parameter from URL (e.g., "?Mode=Single")
	const FString ModeParam = UGameplayStatics::ParseOption(Options, TEXT("Mode"));
	const FString CharacterSystemParam = UGameplayStatics::ParseOption(Options, SinglePlayCharacterRuntime::CharacterSystemOptionName);
	const FString CharacterDefinitionParam = UGameplayStatics::ParseOption(Options, SinglePlayCharacterRuntime::CharacterDefinitionOptionName);

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("InitGame: Map=%s, ModeParam=%s, CharacterSystem=%s, CharacterDefinition=%s"),
		*MapName,
		ModeParam.IsEmpty() ? TEXT("(default)") : *ModeParam,
		CharacterSystemParam.IsEmpty() ? TEXT("(default)") : *CharacterSystemParam,
		CharacterDefinitionParam.IsEmpty() ? TEXT("(default)") : *CharacterDefinitionParam);

	// Load mode configuration
	ModeConfig = LoadModeConfig(ModeParam);
	LoadCharacterSelection(CharacterSystemParam, CharacterDefinitionParam);

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("Loaded ModeConfig: ModeName=%s, RequiredPlugins=%d, FeatureNames=%d, CharacterSystem=%s, CharacterDefinition=%s"),
		*ModeConfig.ModeName.ToString(),
		ModeConfig.RequiredFeaturePlugins.Num(),
		ModeConfig.FeatureNames.Num(),
		*GetActiveCharacterSystemName().ToString(),
		ActiveCharacterDefinitionId.IsValid() ? *ActiveCharacterDefinitionId.ToString() : TEXT("(none)"));

	// Ensure required feature plugins are loaded before proceeding
	EnsureFeaturePluginsLoaded();

	// Apply UE layer classes from config if specified
	if (!ModeConfig.DefaultPawnClass.IsNull())
	{
		UClass* PawnClass = ModeConfig.DefaultPawnClass.LoadSynchronous();
		if (PawnClass)
		{
			DefaultPawnClass = PawnClass;
			UE_LOG(LogProjectSinglePlay, Log, TEXT("Set DefaultPawnClass: %s"), *PawnClass->GetName());
		}
		else
		{
			UE_LOG(LogProjectSinglePlay, Warning, TEXT("Failed to load DefaultPawnClass from config"));
		}
	}

	if (!ModeConfig.PlayerControllerClass.IsNull())
	{
		if (IsRunningDedicatedServer())
		{
			UE_LOG(LogProjectSinglePlay, Verbose,
				TEXT("Skipping PlayerControllerClass load on dedicated server: %s"),
				*ModeConfig.PlayerControllerClass.ToString());
		}
		else
		{
			UClass* PCClass = ModeConfig.PlayerControllerClass.LoadSynchronous();
			if (PCClass)
			{
				PlayerControllerClass = PCClass;
				UE_LOG(LogProjectSinglePlay, Log, TEXT("Set PlayerControllerClass: %s"), *PCClass->GetName());
			}
			else
			{
				UE_LOG(LogProjectSinglePlay, Warning, TEXT("Failed to load PlayerControllerClass from config"));
			}
		}
	}
}

APawn* ASinglePlayerGameMode::SpawnDefaultPawnAtTransform_Implementation(
	AController* NewPlayer, const FTransform& SpawnTransform)
{
	// Character system selection is independent from gameplay mode/difficulty.
	if (ActiveCharacterSystem == ESinglePlayCharacterSystem::Modular)
	{
		if (!ActiveCharacterDefinitionId.IsValid())
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("SpawnDefaultPawn: CharacterSystem=Modular but no definition is selected; falling back to DefaultPawnClass"));
			return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
		}

		UE_LOG(LogProjectSinglePlay, Log, TEXT("SpawnDefaultPawn: CharacterSystem=Modular, Definition=%s"),
			*ActiveCharacterDefinitionId.ToString());

		FText SpawnError;
		AActor* SpawnedActor = ProjectObjectSpawn::SpawnFromDefinition(
			GetWorld(),
			ActiveCharacterDefinitionId,
			SpawnTransform,
			FActorSpawnParameters(),
			&SpawnError);

		if (APawn* SpawnedPawn = Cast<APawn>(SpawnedActor))
		{
			UE_LOG(LogProjectSinglePlay, Log,
				TEXT("SpawnDefaultPawn: Spawned %s from definition"),
				*SpawnedPawn->GetClass()->GetName());
			return SpawnedPawn;
		}

		UE_LOG(LogProjectSinglePlay, Error,
			TEXT("SpawnDefaultPawn: Modular definition spawn FAILED: %s. Fix the definition JSON. No silent fallback to legacy."),
			*SpawnError.ToString());
		return nullptr;
	}

	// Legacy path: only reached when CharacterSystem=Legacy
	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
}

void ASinglePlayerGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("PostLogin: NewPlayer is null"));
		return;
	}

	// Note: Pawn typically doesn't exist yet in PostLogin.
	// Features are initialized in HandleStartingNewPlayer after pawn spawn.
	UE_LOG(LogProjectSinglePlay, Verbose, TEXT("PostLogin: Player %s logged in, awaiting pawn spawn"),
		*NewPlayer->GetName());
}

void ASinglePlayerGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("BeginPlay: Single-player mode '%s' active with CharacterSystem=%s, CharacterDefinition=%s"),
		*ModeConfig.ModeName.ToString(),
		*GetActiveCharacterSystemName().ToString(),
		ActiveCharacterDefinitionId.IsValid() ? *ActiveCharacterDefinitionId.ToString() : TEXT("(none)"));

	// Run verification (idempotent)
	VerifyFeatures();
}

void ASinglePlayerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (!IsValid(NewPlayer))
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("HandleStartingNewPlayer: NewPlayer is invalid, skipping setup"));
		return;
	}

	// Note: Input mode is handled by PlayerController::OnPossess (SOC)
	InitializePlayerPawn(NewPlayer);
}

FName ASinglePlayerGameMode::GetActiveCharacterSystemName() const
{
	return ActiveCharacterSystem == ESinglePlayCharacterSystem::Modular
		? FName(TEXT("Modular"))
		: FName(TEXT("Legacy"));
}

bool ASinglePlayerGameMode::SwitchCharacterSystemRuntime(
	ESinglePlayCharacterSystem NewSystem,
	const FPrimaryAssetId& RequestedDefinitionId,
	bool bRespawnExistingPlayers)
{
	const FPrimaryAssetId ResolvedDefinitionId = ResolveCharacterDefinitionId(NewSystem, RequestedDefinitionId);
	const bool bSelectionChanged =
		ActiveCharacterSystem != NewSystem || ActiveCharacterDefinitionId != ResolvedDefinitionId;

	if (!bSelectionChanged)
	{
		UE_LOG(LogProjectSinglePlay, Log,
			TEXT("SwitchCharacterSystemRuntime: CharacterSystem=%s already active (Definition=%s)"),
			*GetActiveCharacterSystemName().ToString(),
			ResolvedDefinitionId.IsValid() ? *ResolvedDefinitionId.ToString() : TEXT("(none)"));
		return true;
	}

	ActiveCharacterSystem = NewSystem;
	ActiveCharacterDefinitionId = ResolvedDefinitionId;

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("SwitchCharacterSystemRuntime: Switched to CharacterSystem=%s, CharacterDefinition=%s"),
		*GetActiveCharacterSystemName().ToString(),
		ActiveCharacterDefinitionId.IsValid() ? *ActiveCharacterDefinitionId.ToString() : TEXT("(none)"));

	if (!bRespawnExistingPlayers)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("SwitchCharacterSystemRuntime: No world available for respawn"));
		return false;
	}

	int32 PlayerCount = 0;
	int32 RespawnedCount = 0;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!IsValid(PlayerController))
		{
			continue;
		}

		PlayerCount++;
		if (RespawnPlayerForCurrentCharacterSelection(PlayerController))
		{
			RespawnedCount++;
		}
	}

	if (PlayerCount == 0)
	{
		UE_LOG(LogProjectSinglePlay, Log,
			TEXT("SwitchCharacterSystemRuntime: No active players yet; selection will apply on next spawn"));
		return true;
	}

	if (RespawnedCount != PlayerCount)
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("SwitchCharacterSystemRuntime: Respawned %d/%d players after character-system switch"),
			RespawnedCount, PlayerCount);
		return false;
	}

	return true;
}

FSinglePlayModeConfig ASinglePlayerGameMode::LoadModeConfig(const FString& ModeParam)
{
	// Determine which mode name to look up (default: Medium)
	const FName ModeName = ModeParam.IsEmpty() ? FName(TEXT("Medium")) : FName(*ModeParam);

	// Try to find config from C++ registry (defined in SinglePlayModeDefaults.cpp)
	const FSinglePlayModeConfig* FoundConfig = FSinglePlayModeRegistry::FindMode(ModeName);
	if (FoundConfig)
	{
		UE_LOG(LogProjectSinglePlay, Log, TEXT("LoadModeConfig: Found config for '%s' in registry"),
			*ModeName.ToString());
		return *FoundConfig;
	}

	// Fallback to default if mode not found
	UE_LOG(LogProjectSinglePlay, Warning, TEXT("LoadModeConfig: Mode '%s' not found, trying 'Medium' fallback"),
		*ModeName.ToString());

	// Try to get "Medium" as fallback
	FoundConfig = FSinglePlayModeRegistry::FindMode(FName(TEXT("Medium")));
	if (FoundConfig)
	{
		UE_LOG(LogProjectSinglePlay, Log, TEXT("LoadModeConfig: Fallback found 'Medium' with %d features, PawnClass=%s"),
			FoundConfig->FeatureNames.Num(),
			FoundConfig->DefaultPawnClass.IsNull() ? TEXT("(null)") : *FoundConfig->DefaultPawnClass.GetAssetName());
		return *FoundConfig;
	}

	// Ultimate fallback - return static default
	UE_LOG(LogProjectSinglePlay, Warning, TEXT("LoadModeConfig: 'Medium' not found in registry, using GetDefault()"));
	return FSinglePlayModeConfig::GetDefault();
}

void ASinglePlayerGameMode::LoadCharacterSelection(
	const FString& CharacterSystemParam,
	const FString& CharacterDefinitionParam)
{
	ESinglePlayCharacterSystem RequestedSystem = ESinglePlayCharacterSystem::Modular;
	if (!CharacterSystemParam.IsEmpty() &&
		!SinglePlayCharacterRuntime::TryParseCharacterSystem(CharacterSystemParam, RequestedSystem))
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("LoadCharacterSelection: Unknown CharacterSystem '%s', defaulting to Modular"),
			*CharacterSystemParam);
		RequestedSystem = ESinglePlayCharacterSystem::Modular;
	}

	const FPrimaryAssetId RequestedDefinitionId =
		SinglePlayCharacterRuntime::ParseCharacterDefinitionId(CharacterDefinitionParam);

	ActiveCharacterSystem = RequestedSystem;
	ActiveCharacterDefinitionId = ResolveCharacterDefinitionId(RequestedSystem, RequestedDefinitionId);
}

FPrimaryAssetId ASinglePlayerGameMode::ResolveCharacterDefinitionId(
	ESinglePlayCharacterSystem NewSystem,
	const FPrimaryAssetId& RequestedDefinitionId) const
{
	if (NewSystem != ESinglePlayCharacterSystem::Modular)
	{
		return FPrimaryAssetId();
	}

	if (RequestedDefinitionId.IsValid())
	{
		return RequestedDefinitionId;
	}

	return SinglePlayCharacterRuntime::GetDefaultModularDefinitionId();
}

void ASinglePlayerGameMode::EnsureFeaturePluginsLoaded()
{
	if (ModeConfig.RequiredFeaturePlugins.Num() == 0)
	{
		UE_LOG(LogProjectSinglePlay, Verbose, TEXT("No required feature plugins specified"));
		return;
	}

	UE_LOG(LogProjectSinglePlay, Log, TEXT("Ensuring %d required feature plugins are loaded..."),
		ModeConfig.RequiredFeaturePlugins.Num());

	// TODO: Integrate with Orchestrator's on-demand loading API when available
	// For now, we assume Orchestrator has already loaded feature plugins via manifest
	// This is a verification step - if plugins are missing, features will fail to initialize

	for (const FName& PluginName : ModeConfig.RequiredFeaturePlugins)
	{
		// Check if module is loaded (simple availability check)
		const bool bModuleLoaded = FModuleManager::Get().IsModuleLoaded(PluginName);

		if (bModuleLoaded)
		{
			UE_LOG(LogProjectSinglePlay, Log, TEXT("  [OK] Plugin '%s' is loaded"), *PluginName.ToString());
		}
		else
		{
			// TODO: Call Orchestrator->EnsurePluginLoaded(PluginName) when API available
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("  [MISSING] Plugin '%s' not loaded - feature may fail to initialize"),
				*PluginName.ToString());
		}
	}
}

void ASinglePlayerGameMode::InitializePlayerPawn(AController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("InitializePlayerPawn: Controller is invalid, skipping setup"));
		return;
	}

	APawn* Pawn = PlayerController->GetPawn();
	if (Pawn && Pawn->HasAuthority())
	{
		UE_LOG(LogProjectSinglePlay, Log,
			TEXT("InitializePlayerPawn: Initializing features for pawn %s"),
			*Pawn->GetName());
		InitializeFeatures(Pawn);
	}
	else if (!Pawn)
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("InitializePlayerPawn: Pawn still null after spawn for %s"),
			*PlayerController->GetName());
	}
}

bool ASinglePlayerGameMode::RespawnPlayerForCurrentCharacterSelection(AController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return false;
	}

	if (APawn* ExistingPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		ExistingPawn->Destroy();
	}

	RestartPlayer(PlayerController);
	InitializePlayerPawn(PlayerController);

	APawn* NewPawn = PlayerController->GetPawn();
	if (!NewPawn)
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("RespawnPlayerForCurrentCharacterSelection: Failed to respawn %s"),
			*PlayerController->GetName());
		return false;
	}

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("RespawnPlayerForCurrentCharacterSelection: %s now possesses %s"),
		*PlayerController->GetName(),
		*NewPawn->GetClass()->GetName());
	return true;
}

void ASinglePlayerGameMode::InitializeFeatures(APawn* Pawn)
{
	// ORCHESTRATION: GameMode controls ORDER, TIMING, and SELECTION of feature init
	// Features registered lambdas on module startup; now we call them in order with pawn
	// See: Plugins/Gameplay/ProjectFeature/README.md for design rationale

	if (!Pawn)
	{
		return;
	}

	if (ModeConfig.FeatureNames.Num() == 0)
	{
		UE_LOG(LogProjectSinglePlay, Verbose, TEXT("No features configured for this mode"));
		return;
	}

	int32 InitializedCount = 0;
	int32 FailedCount = 0;

	UE_LOG(LogProjectSinglePlay, Log, TEXT("Initializing %d features in order..."),
		ModeConfig.FeatureNames.Num());

	// GameMode controls ORDER via FeatureNames array (Combat before Inventory, etc.)
	for (const FName& FeatureName : ModeConfig.FeatureNames)
	{
		// Build context - feature will grab Pawn and attach its components
		FFeatureInitContext Context(
			GetWorld(),
			this,
			Pawn,  // Feature attaches components here
			ModeConfig.ModeName,
			ModeConfig.FeatureConfigs.FindRef(FeatureName)  // Feature-specific config
		);

		// GameMode knows NOTHING about what feature does internally
		// Just calls registry; feature decides what to attach to pawn
		if (FFeatureRegistry::InitializeFeature(FeatureName, Context))
		{
			InitializedCount++;
		}
		else
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("  Feature '%s' not registered - skipping"),
				*FeatureName.ToString());
			FailedCount++;
		}
	}

	UE_LOG(LogProjectSinglePlay, Log, TEXT("Features: %d initialized, %d failed"),
		InitializedCount, FailedCount);
}

void ASinglePlayerGameMode::VerifyFeatures()
{
	// Verify that configured features are registered in FFeatureRegistry
	// This only checks registration, not pawn components (features own their init)

	// Idempotency check
	if (bHasVerifiedFeatures)
	{
		return;
	}
	bHasVerifiedFeatures = true;

	if (ModeConfig.FeatureNames.Num() == 0)
	{
		UE_LOG(LogProjectSinglePlay, Verbose, TEXT("No features configured for verification"));
		return;
	}

	int32 RegisteredCount = 0;
	int32 MissingCount = 0;

	for (const FName& FeatureName : ModeConfig.FeatureNames)
	{
		if (FFeatureRegistry::IsFeatureRegistered(FeatureName))
		{
			RegisteredCount++;
		}
		else
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("Verification: Feature '%s' not registered in FFeatureRegistry"),
				*FeatureName.ToString());
			MissingCount++;
		}
	}

	if (MissingCount > 0)
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("Feature verification: %d registered, %d missing"),
			RegisteredCount, MissingCount);
	}
	else
	{
		UE_LOG(LogProjectSinglePlay, Log, TEXT("Feature verification: All %d features registered"),
			RegisteredCount);
	}
}
