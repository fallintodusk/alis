#include "SinglePlayerGameMode.h"
#include "SinglePlayModeRegistry.h"
#include "FeatureRegistry.h"
#include "FeatureInitContext.h"
#include "ProjectSinglePlayLog.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Engine/AssetManager.h"
#include "Modules/ModuleManager.h"
#include "Spawning/ObjectSpawnUtility.h"
#include "Data/ObjectDefinition.h"
#include "ProjectServiceLocator.h"
#include "Services/ILoadingService.h"
#include "Types/ProjectLoadRequest.h"
#include "Interfaces/IVitalsEventsSource.h"
#include "Camera/PlayerCameraManager.h"
#include "Scenario/SinglePlayScenarioPolicy.h"
#include "Scenario/SinglePlayScenarioRunnerComponent.h"

namespace SinglePlayCharacterRuntime
{
	static constexpr TCHAR CharacterDefinitionOptionName[] = TEXT("CharacterDefinition");
	static constexpr TCHAR DefaultCharacterDefinition[] = TEXT("ObjectDefinition:Hero");

	static FPrimaryAssetId GetDefaultCharacterDefinitionId()
	{
		return FPrimaryAssetId::FromString(DefaultCharacterDefinition);
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
}

ASinglePlayerGameMode::ASinglePlayerGameMode()
{
	// Pawn spawning is definition-only; ModeConfig may override the controller.
	PlayerControllerClass = APlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	ScenarioRunner = CreateDefaultSubobject<USinglePlayScenarioRunnerComponent>(TEXT("ScenarioRunner"));
}

void ASinglePlayerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Parse Mode parameter from URL (e.g., "?Mode=Single")
	const FString ModeParam = UGameplayStatics::ParseOption(Options, TEXT("Mode"));
	const FString CharacterDefinitionParam = UGameplayStatics::ParseOption(Options, SinglePlayCharacterRuntime::CharacterDefinitionOptionName);
	const FString TraversalParam = UGameplayStatics::ParseOption(
		Options,
		ProjectSinglePlayTraversal::OptionName());
	const FSinglePlayTraversalSelection TraversalSelection =
		ProjectSinglePlayTraversal::Resolve(TraversalParam);
	TraversalMode = TraversalSelection.Mode;
	if (TraversalSelection.ParseResult == ESinglePlayTraversalParseResult::Unknown)
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("[ASinglePlayerGameMode::InitGame] Unknown traversal - value=%s fallback=Default"),
			*TraversalParam);
	}

	const FSinglePlayScenarioSelection ScenarioSelection =
		FSinglePlayScenarioPolicy::Resolve(Options);
	if (ScenarioSelection.Result == ESinglePlayScenarioParseResult::Unknown)
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("[ASinglePlayerGameMode::InitGame] Unknown scenario - fallback=Disabled"));
	}
	else if (ScenarioSelection.IsEnabled())
	{
		FString ScenarioError;
		if (!ScenarioRunner->Configure(ScenarioSelection.ScenarioId, ScenarioError))
		{
			ErrorMessage = ScenarioError;
			UE_LOG(LogProjectSinglePlay, Error,
				TEXT("[ASinglePlayerGameMode::InitGame] Scenario configuration failed - id=%s error=%s"),
				*ScenarioSelection.ScenarioId.ToString(), *ScenarioError);
		}
	}

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("InitGame: Map=%s, ModeParam=%s, CharacterDefinition=%s, Traversal=%s"),
		*MapName,
		ModeParam.IsEmpty() ? TEXT("(default)") : *ModeParam,
		CharacterDefinitionParam.IsEmpty() ? TEXT("(default)") : *CharacterDefinitionParam,
		TraversalParam.IsEmpty() ? TEXT("Default") : *TraversalParam);

	// Load mode configuration
	ModeConfig = LoadModeConfig(ModeParam);
	LoadCharacterSelection(CharacterDefinitionParam);

	// Experience name for death/reload is resolved from ILoadingService at reload time.
	// No need to capture here -- authoritative source is the loading subsystem.

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("Loaded ModeConfig: ModeName=%s, RequiredPlugins=%d, FeatureNames=%d, CharacterDefinition=%s"),
		*ModeConfig.ModeName.ToString(),
		ModeConfig.RequiredFeaturePlugins.Num(),
		ModeConfig.FeatureNames.Num(),
		ActiveCharacterDefinitionId.IsValid() ? *ActiveCharacterDefinitionId.ToString() : TEXT("(none)"));

	// Ensure required feature plugins are loaded before proceeding
	EnsureFeaturePluginsLoaded();

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
	// Character definition selection is independent from gameplay mode/difficulty.
	if (!ActiveCharacterDefinitionId.IsValid())
	{
		UE_LOG(LogProjectSinglePlay, Error,
			TEXT("SpawnDefaultPawn: No valid CharacterDefinition; refusing non-definition fallback"));
		return nullptr;
	}

	UE_LOG(LogProjectSinglePlay, Log, TEXT("SpawnDefaultPawn: CharacterDefinition=%s"),
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

	if (SpawnedActor)
	{
		UE_LOG(LogProjectSinglePlay, Error,
			TEXT("SpawnDefaultPawn: Definition %s spawned non-pawn class %s; destroying the stray actor"),
			*ActiveCharacterDefinitionId.ToString(),
			*SpawnedActor->GetClass()->GetPathName());
		SpawnedActor->Destroy();
		return nullptr;
	}

	UE_LOG(LogProjectSinglePlay, Error,
		TEXT("SpawnDefaultPawn: Definition spawn FAILED: %s. Fix the definition JSON."),
		*SpawnError.ToString());
	return nullptr;
}

UClass* ASinglePlayerGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// AGameModeBase checks this hook for a non-null value before it dispatches
	// SpawnDefaultPawnAtTransform. The returned APawn class is only a dispatch
	// sentinel: this GameMode never asks the engine to instantiate it, and the
	// definition remains the sole source of the concrete pawn class.
	return ActiveCharacterDefinitionId.IsValid() ? APawn::StaticClass() : nullptr;
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
		TEXT("BeginPlay: Single-player mode '%s' active with CharacterDefinition=%s"),
		*ModeConfig.ModeName.ToString(),
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
		UE_LOG(LogProjectSinglePlay, Log, TEXT("LoadModeConfig: Fallback found 'Medium' with %d features"),
			FoundConfig->FeatureNames.Num());
		return *FoundConfig;
	}

	// Ultimate fallback - return static default
	UE_LOG(LogProjectSinglePlay, Warning, TEXT("LoadModeConfig: 'Medium' not found in registry, using GetDefault()"));
	return FSinglePlayModeConfig::GetDefault();
}

void ASinglePlayerGameMode::LoadCharacterSelection(const FString& CharacterDefinitionParam)
{
	const FPrimaryAssetId RequestedDefinitionId =
		SinglePlayCharacterRuntime::ParseCharacterDefinitionId(CharacterDefinitionParam);

	ActiveCharacterDefinitionId = ResolveCharacterDefinitionId(RequestedDefinitionId);
}

FPrimaryAssetId ASinglePlayerGameMode::ResolveCharacterDefinitionId(
	const FPrimaryAssetId& RequestedDefinitionId) const
{
	if (RequestedDefinitionId.IsValid())
	{
		return RequestedDefinitionId;
	}

	return SinglePlayCharacterRuntime::GetDefaultCharacterDefinitionId();
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

		// Bind death response after features are initialized (VitalsComponent must be ready)
		if (APlayerController* PC = Cast<APlayerController>(PlayerController))
		{
			BindVitalsResponse(PC);
		}

		if (!ProjectSinglePlayTraversal::Apply(Pawn, TraversalMode))
		{
			UE_LOG(LogProjectSinglePlay, Warning,
				TEXT("[ASinglePlayerGameMode::InitializePlayerPawn] Traversal application failed - pawn=%s"),
				*Pawn->GetClass()->GetPathName());
		}

		ScenarioRunner->Start(Pawn);
	}
	else if (!Pawn)
	{
		UE_LOG(LogProjectSinglePlay, Warning,
			TEXT("InitializePlayerPawn: Pawn still null after spawn for %s"),
			*PlayerController->GetName());
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

// -------------------------------------------------------------------------
// Death Response (single-player: fade + reload)
// -------------------------------------------------------------------------

void ASinglePlayerGameMode::BindVitalsResponse(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}

	// Discover vitals source via interface (DIP: no hard dep on ProjectVitals).
	// The provider is an ActorComponent implementing IVitalsEventsSource.
	UActorComponent* VitalsSourceComp = nullptr;
	for (UActorComponent* Comp : Pawn->GetComponents())
	{
		if (Comp && Comp->GetClass()->ImplementsInterface(UVitalsEventsSource::StaticClass()))
		{
			VitalsSourceComp = Comp;
			break;
		}
	}

	if (!VitalsSourceComp)
	{
		UE_LOG(LogProjectSinglePlay, Verbose,
			TEXT("[Death] No IVitalsEventsSource component on pawn '%s', skipping death response binding"),
			*Pawn->GetName());
		return;
	}

	IVitalsEventsSource* EventsSource = Cast<IVitalsEventsSource>(VitalsSourceComp);
	if (!EventsSource)
	{
		return;
	}

	// Unbind previous if any (prevents duplicates on re-init).
	FOnVitalsDamageTaken& DamageDelegate = EventsSource->GetOnDamageTakenDelegate();
	DamageDelegate.RemoveDynamic(this, &ThisClass::HandleDamageTaken);
	DamageDelegate.AddDynamic(this, &ThisClass::HandleDamageTaken);

	FOnVitalsConditionDepleted& DepletedDelegate = EventsSource->GetOnConditionDepletedDelegate();
	DepletedDelegate.RemoveDynamic(this, &ThisClass::HandleConditionDepleted);
	DepletedDelegate.AddDynamic(this, &ThisClass::HandleConditionDepleted);

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("[Vitals] Bound damage + death delegates for '%s'"),
		*PC->GetName());
}

void ASinglePlayerGameMode::HandleDamageTaken(float Amount)
{
	if (bDeathSequenceStarted)
	{
		return;
	}

	// Skip tiny damage (bleeding drain ~0.05/tick)
	if (Amount < 1.0f)
	{
		return;
	}

	// Cooldown: one flash per second max
	const double NowSec = FPlatformTime::Seconds();
	if (NowSec - LastDamageFlashTimeSec < 1.0)
	{
		return;
	}
	LastDamageFlashTimeSec = NowSec;

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
		{
			const float Alpha = FMath::Clamp(Amount / 50.0f, 0.15f, 0.5f);
			CamMgr->StartCameraFade(Alpha, 0.0f, 0.4f, FLinearColor::Red, false, false);
		}
	}
}

void ASinglePlayerGameMode::HandleConditionDepleted()
{
	if (bDeathSequenceStarted)
	{
		return;
	}
	bDeathSequenceStarted = true;

	UE_LOG(LogProjectSinglePlay, Log, TEXT("[Death] Condition depleted, starting death sequence"));

	// Local presentation: fade + disable input + death message
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);

		if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
		{
			CamMgr->StartCameraFade(0.0f, 1.0f, 1.0f, FLinearColor::Black, false, true);
		}

	}

	// Authority: reload map after delay (1.0s fade + 0.5s hold on black)
	GetWorldTimerManager().SetTimer(
		DeathReloadTimerHandle,
		this,
		&ThisClass::ReloadCurrentExperience,
		1.5f,
		false);
}

void ASinglePlayerGameMode::ReloadCurrentExperience()
{
	TSharedPtr<ILoadingService> LoadingService =
		FProjectServiceLocator::Resolve<ILoadingService>();
	if (!LoadingService)
	{
		UE_LOG(LogProjectSinglePlay, Error,
			TEXT("[Death] Cannot reload: ILoadingService not available"));
		return;
	}

	// Authoritative experience name from loading subsystem (set on last successful load)
	const FName ExperienceName = LoadingService->GetLastLoadedExperienceName();
	if (ExperienceName.IsNone())
	{
		UE_LOG(LogProjectSinglePlay, Error,
			TEXT("[Death] Cannot reload: no last loaded experience name available"));
		return;
	}

	FLoadRequest Request;
	FText Error;

	if (!LoadingService->BuildLoadRequestForExperience(ExperienceName, Request, Error))
	{
		UE_LOG(LogProjectSinglePlay, Error,
			TEXT("[Death] Failed to build reload request for '%s': %s"),
			*ExperienceName.ToString(), *Error.ToString());
		return;
	}

	Request.LoadMode = ELoadMode::SinglePlayer;
	Request.CustomOptions.Add(TEXT("game"), TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode"));
	Request.CustomOptions.Add(TEXT("Mode"), ModeConfig.ModeName.ToString());
	if (ActiveCharacterDefinitionId.IsValid())
	{
		// The option contract accepts the ObjectDefinition name shorthand. A full
		// primary-asset ID contains ':' and is not a valid ServerTravel URL value.
		Request.CustomOptions.Add(
			TEXT("CharacterDefinition"), ActiveCharacterDefinitionId.PrimaryAssetName.ToString());
	}
	if (TraversalMode == ESinglePlayTraversalMode::PreviewFlight)
	{
		Request.CustomOptions.Add(
			ProjectSinglePlayTraversal::OptionName(),
			ProjectSinglePlayTraversal::PreviewFlightValue());
	}
	if (ScenarioRunner != nullptr && !ScenarioRunner->GetScenarioId().IsNone())
	{
		Request.CustomOptions.Add(
			FSinglePlayScenarioPolicy::OptionName(),
			ScenarioRunner->GetScenarioId().ToString());
	}

	UE_LOG(LogProjectSinglePlay, Log,
		TEXT("[Death] Reloading experience '%s'"),
		*ExperienceName.ToString());

	LoadingService->StartLoad(Request);
}

void ASinglePlayerGameMode::RequestScenarioRestart()
{
	UE_LOG(LogProjectSinglePlay, Display,
		TEXT("[ASinglePlayerGameMode::RequestScenarioRestart] Restarting current experience"));
	ReloadCurrentExperience();
}
