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
#include "Modules/ModuleManager.h"

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

	UE_LOG(LogProjectSinglePlay, Log, TEXT("InitGame: Map=%s, ModeParam=%s"),
		*MapName, ModeParam.IsEmpty() ? TEXT("(default)") : *ModeParam);

	// Load mode configuration
	ModeConfig = LoadModeConfig(ModeParam);

	UE_LOG(LogProjectSinglePlay, Log, TEXT("Loaded ModeConfig: ModeName=%s, RequiredPlugins=%d, FeatureNames=%d"),
		*ModeConfig.ModeName.ToString(), ModeConfig.RequiredFeaturePlugins.Num(), ModeConfig.FeatureNames.Num());

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

	UE_LOG(LogProjectSinglePlay, Log, TEXT("BeginPlay: Single-player mode '%s' active"),
		*ModeConfig.ModeName.ToString());

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

	// Initialize features for the spawned pawn
	// This is the correct place - pawn now exists after Super::HandleStartingNewPlayer
	APawn* Pawn = NewPlayer->GetPawn();
	if (Pawn && Pawn->HasAuthority())
	{
		UE_LOG(LogProjectSinglePlay, Log,
			TEXT("HandleStartingNewPlayer: Initializing features for pawn %s"),
			*Pawn->GetName());
		InitializeFeatures(Pawn);
	}
	else if (!Pawn)
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("HandleStartingNewPlayer: Pawn still null after RestartPlayer for %s"),
			*NewPlayer->GetName());
	}
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
