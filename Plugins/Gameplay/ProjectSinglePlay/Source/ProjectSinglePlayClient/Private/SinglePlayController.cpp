#include "SinglePlayController.h"
#include "ProjectSinglePlayLog.h"
#include "Template/Interactable/InteractableActor.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "Components/ActorComponent.h"
#include "Engine/GameViewportClient.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "MVVM/VitalsViewModel.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Subsystems/ProjectUIFactorySubsystem.h"
#include "Subsystems/ProjectUIRegistrySubsystem.h"
#include "UI/InventoryUIVisibilityCoordinator.h"
#include "HAL/PlatformTLS.h"
#include "Interfaces/IInteractionService.h"
#include "Interfaces/IInventoryCommands.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Interfaces/IMindRuntimeControl.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "ProjectServiceLocator.h"
#include "Scenario/SinglePlayScenarioPresentationComponent.h"
#if !UE_SERVER
#include "MVVM/InventoryViewModel.h"
#include "MVVM/MindJournalViewModel.h"
#endif

// Thread-safe logging macro for init diagnostics
#define LOG_INIT(Format, ...) UE_LOG(LogProjectSinglePlay, Log, TEXT("[Thread:%u] " Format), FPlatformTLS::GetCurrentThreadId(), ##__VA_ARGS__)

namespace
{
UObject* ResolveWorldContainerSessionSource(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return nullptr;
	}

	auto ResolveSource = [](UObject* Candidate) -> UObject*
	{
		return Candidate && Candidate->GetClass()->ImplementsInterface(UWorldContainerSessionSource::StaticClass())
			? Candidate
			: nullptr;
	};

	if (UObject* SourceObject = ResolveSource(TargetActor))
	{
		return SourceObject;
	}

	TInlineComponentArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (UObject* SourceObject = ResolveSource(Component))
		{
			return SourceObject;
		}
	}

	return nullptr;
}
}

ASinglePlayController::ASinglePlayController()
{
	// Defaults - will be overridden in OnPossess based on pawn type
	bShowMouseCursor = false;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	VitalsUIBindAttempts = 0;
	ScenarioPresentation = CreateDefaultSubobject<USinglePlayScenarioPresentationComponent>(
		TEXT("ScenarioPresentation"));
}

void ASinglePlayController::BeginPlay()
{
	LOG_INIT("BeginPlay START - Controller=%s", *GetName());
	Super::BeginPlay();

	if (IsLocalController())
	{
		LOG_INIT("BeginPlay: IsLocalController=true, creating UI input assets");
		CreateUIInputAssets();

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (UIInputMappingContext)
			{
				Subsystem->AddMappingContext(UIInputMappingContext, 1);
				LOG_INIT("BeginPlay: Added UIInputMappingContext to subsystem");
			}
		}
		else
		{
			UE_LOG(LogProjectSinglePlay, Warning, TEXT("[Thread:%u] BeginPlay: No EnhancedInputLocalPlayerSubsystem"), FPlatformTLS::GetCurrentThreadId());
		}
	}
	LOG_INIT("BeginPlay END");
}

void ASinglePlayController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LOG_INIT("EndPlay START - Reason=%d", static_cast<int32>(EndPlayReason));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VitalsUIRetryHandle);
		LOG_INIT("EndPlay: Cleared VitalsUIRetryHandle timer");
	}

	if (VitalsViewModel)
	{
		VitalsViewModel->OnPropertyChanged.RemoveDynamic(this, &ASinglePlayController::HandleVitalsViewModelPropertyChanged);
		LOG_INIT("EndPlay: Unbound from VitalsViewModel.OnPropertyChanged");
	}

#if !UE_SERVER
	if (UInventoryViewModel* InventoryVM = Cast<UInventoryViewModel>(InventoryViewModel))
	{
		InventoryVM->OnPropertyChanged.RemoveDynamic(this, &ASinglePlayController::HandleInventoryViewModelPropertyChanged);
		LOG_INIT("EndPlay: Unbound from InventoryViewModel.OnPropertyChanged");
	}
#endif

	if (IsLocalController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (UIInputMappingContext)
			{
				Subsystem->RemoveMappingContext(UIInputMappingContext);
				LOG_INIT("EndPlay: Removed UIInputMappingContext");
			}
		}
	}

	Super::EndPlay(EndPlayReason);
	LOG_INIT("EndPlay END");
}

void ASinglePlayController::OnPossess(APawn* InPawn)
{
	LOG_INIT("OnPossess START - Pawn=%s", InPawn ? *InPawn->GetName() : TEXT("null"));
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		LOG_INIT("OnPossess: Pawn is null, returning early");
		return;
	}

	// Character pawn = first-person gameplay mode
	if (InPawn->IsA<ACharacter>())
	{
		SetFirstPersonInputMode();
		LOG_INIT("OnPossess: Character '%s' - first-person input mode, initializing VitalsUI", *InPawn->GetName());
		InitializeVitalsUI(InPawn);
		InitializeInventoryUI(InPawn);
	}
	else
	{
		// Spectator or other pawn = UI mode
		SetUIInputMode();
		LOG_INIT("OnPossess: Pawn '%s' - UI input mode (non-character)", *InPawn->GetName());
	}
	LOG_INIT("OnPossess END");
}

void ASinglePlayController::SetupInputComponent()
{
	LOG_INIT("SetupInputComponent START");
	Super::SetupInputComponent();

	CreateUIInputAssets();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ToggleVitalsAction)
		{
			EnhancedInputComponent->BindAction(
				ToggleVitalsAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayController::HandleToggleVitalsAction);
			LOG_INIT("SetupInputComponent: Bound ToggleVitalsAction");
		}

#if !UE_SERVER
		if (ToggleGameMenuAction)
		{
			EnhancedInputComponent->BindAction(
				ToggleGameMenuAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayController::HandleToggleGameMenuAction);
		}

		if (ToggleInventoryAction)
		{
			EnhancedInputComponent->BindAction(
				ToggleInventoryAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayController::HandleToggleInventoryAction);
			LOG_INIT("SetupInputComponent: Bound ToggleInventoryAction");
		}

		if (ToggleMindJournalAction)
		{
			EnhancedInputComponent->BindAction(
				ToggleMindJournalAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayController::HandleToggleMindJournalAction);
			LOG_INIT("SetupInputComponent: Bound ToggleMindJournalAction");
		}
#endif

		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(
				InteractAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayController::HandleInteractAction);
			EnhancedInputComponent->BindAction(
				InteractAction,
				ETriggerEvent::Completed,
				this,
				&ASinglePlayController::HandleInteractReleasedAction);
			EnhancedInputComponent->BindAction(
				InteractAction,
				ETriggerEvent::Canceled,
				this,
				&ASinglePlayController::HandleInteractReleasedAction);
			LOG_INIT("SetupInputComponent: Bound InteractAction");
		}

		if (SwapHandsAction)
		{
			EnhancedInputComponent->BindAction(
				SwapHandsAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayController::HandleSwapHandsAction);
			LOG_INIT("SetupInputComponent: Bound SwapHandsAction");
		}
	}
	else
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("[Thread:%u] SetupInputComponent: No EnhancedInputComponent"), FPlatformTLS::GetCurrentThreadId());
	}
	LOG_INIT("SetupInputComponent END");
}

bool ASinglePlayController::InputKey(const FInputKeyEventArgs& Params)
{
	const bool bHandled = Super::InputKey(Params);

	switch (Params.Event)
	{
	case IE_Axis:
	case IE_Pressed:
	case IE_Repeat:
	case IE_DoubleClick:
		NotifyMindInputActivity();
		break;
	default:
		break;
	}

	return bHandled;
}

void ASinglePlayController::SetFirstPersonInputMode()
{
	// Disable move/look ignore (in case coming from menu)
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);

	// Hide cursor and set game-only input with mouse lock
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	// Lock mouse to viewport center for first-person gameplay
	if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
	{
		ViewportClient->SetMouseLockMode(EMouseLockMode::LockAlways);
	}
}

void ASinglePlayController::SetUIInputMode()
{
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ASinglePlayController::InitializeVitalsUI(APawn* InPawn)
{
	LOG_INIT("InitializeVitalsUI START - Pawn=%s", InPawn ? *InPawn->GetName() : TEXT("null"));

	if (!IsLocalController() || !InPawn)
	{
		LOG_INIT("InitializeVitalsUI: Early return - IsLocalController=%d, InPawn=%d", IsLocalController(), InPawn != nullptr);
		return;
	}

	if (UProjectUILayerHostSubsystem* LayerHost = GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
	{
		LayerHost->InitializeForPlayer(this);
		LOG_INIT("InitializeVitalsUI: ProjectUI layer host initialized");
	}
	else
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("[Thread:%u] InitializeVitalsUI: ProjectUILayerHostSubsystem not found"), FPlatformTLS::GetCurrentThreadId());
	}

	VitalsUIBindAttempts = 0;
	TryBindVitalsViewModel();
	LOG_INIT("InitializeVitalsUI END");
}

void ASinglePlayController::InitializeInventoryUI(APawn* InPawn)
{
#if !UE_SERVER
	// Inventory UI is demand-loaded: ViewModel created on first toggle, not on possess.
	// TryBindInventoryViewModel() is called from HandleToggleInventoryAction() when user opens inventory.
	LOG_INIT("InitializeInventoryUI: Pawn=%s (binding deferred to first toggle)", InPawn ? *InPawn->GetName() : TEXT("null"));
	SyncInventoryViewModelSourceFromPawn();
#else
	(void)InPawn;
#endif
}

void ASinglePlayController::TryBindVitalsViewModel()
{
	LOG_INIT("TryBindVitalsViewModel START - Attempt=%d", VitalsUIBindAttempts + 1);

	if (!IsLocalController() || !GetWorld())
	{
		LOG_INIT("TryBindVitalsViewModel: Early return - IsLocal=%d, HasVM=%d, HasWorld=%d",
			IsLocalController(), VitalsViewModel != nullptr, GetWorld() != nullptr);
		return;
	}

	if (VitalsViewModel)
	{
		SyncVitalsViewModelSourceFromPawn();
		LOG_INIT("TryBindVitalsViewModel: Reused shared ViewModel and refreshed ASC source");
		return;
	}

	++VitalsUIBindAttempts;

	UProjectUIFactorySubsystem* Factory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProjectUIFactorySubsystem>() : nullptr;
	if (!Factory)
	{
		LOG_INIT("TryBindVitalsViewModel: ProjectUIFactorySubsystem not found");
		return;
	}

	if (UVitalsViewModel* FoundVM = Cast<UVitalsViewModel>(
		Factory->GetSharedViewModel(UVitalsViewModel::StaticClass())))
	{
		if (VitalsViewModel)
		{
			VitalsViewModel->OnPropertyChanged.RemoveDynamic(this, &ASinglePlayController::HandleVitalsViewModelPropertyChanged);
			LOG_INIT("TryBindVitalsViewModel: Unbound previous ViewModel");
		}

		VitalsViewModel = FoundVM;
		VitalsViewModel->OnPropertyChanged.AddUniqueDynamic(this, &ASinglePlayController::HandleVitalsViewModelPropertyChanged);
		LOG_INIT("TryBindVitalsViewModel: Bound to VitalsViewModel.OnPropertyChanged delegate");
		SyncVitalsViewModelSourceFromPawn();

		if (VitalsViewModel->GetbPanelVisible())
		{
			if (UProjectUILayerHostSubsystem* LayerHost = GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
			{
				LayerHost->ShowDefinition(TEXT("ProjectVitalsUI.VitalsPanel"));
			}
		}

		LOG_INIT("TryBindVitalsViewModel SUCCESS - ViewModel bound from ProjectUI factory");
		return;
	}

	if (VitalsUIBindAttempts < 5)
	{
		LOG_INIT("TryBindVitalsViewModel: ViewModel not found, scheduling retry %d/5 in 0.2s", VitalsUIBindAttempts);
		GetWorld()->GetTimerManager().SetTimer(
			VitalsUIRetryHandle,
			this,
			&ASinglePlayController::TryBindVitalsViewModel,
			0.2f,
			false);
	}
	else
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("[Thread:%u] TryBindVitalsViewModel FAILED - No ViewModel after %d retries"),
			FPlatformTLS::GetCurrentThreadId(), VitalsUIBindAttempts);
	}
}

void ASinglePlayController::SyncVitalsViewModelSourceFromPawn()
{
	if (!VitalsViewModel)
	{
		return;
	}

	UAbilitySystemComponent* ASC = nullptr;
	if (APawn* ControlledPawn = GetPawn())
	{
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn);
	}

	VitalsViewModel->SetAbilitySystemComponent(ASC);
	LOG_INIT("SyncVitalsViewModelSourceFromPawn: Pawn=%s, ASC=%s",
		GetPawn() ? *GetPawn()->GetName() : TEXT("null"),
		ASC ? *GetNameSafe(ASC->GetOwner()) : TEXT("null"));
}

void ASinglePlayController::TryBindInventoryViewModel()
{
#if !UE_SERVER
	// Global InventoryViewModel can now be created without showing the panel widget.
	// This keeps timed loot-search interactions from opening an empty panel before
	// the world-container session actually exists.

	if (!IsLocalController() || InventoryViewModel)
	{
		SyncInventoryViewModelSourceFromPawn();
		return;
	}

	UProjectUIFactorySubsystem* Factory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProjectUIFactorySubsystem>() : nullptr;
	if (!Factory)
	{
		return;
	}

	if (UInventoryViewModel* FoundVM = Cast<UInventoryViewModel>(
		Factory->GetSharedViewModel(UInventoryViewModel::StaticClass())))
	{
		if (UInventoryViewModel* ExistingVM = Cast<UInventoryViewModel>(InventoryViewModel))
		{
			ExistingVM->OnPropertyChanged.RemoveDynamic(this, &ASinglePlayController::HandleInventoryViewModelPropertyChanged);
		}

		InventoryViewModel = FoundVM;
		FoundVM->OnPropertyChanged.AddUniqueDynamic(this, &ASinglePlayController::HandleInventoryViewModelPropertyChanged);
		SyncInventoryViewModelSourceFromPawn();
		LOG_INIT("TryBindInventoryViewModel: Bound to InventoryViewModel");
	}
#endif
}

bool ASinglePlayController::EnsureInventoryViewModelReady()
{
#if !UE_SERVER
	if (!IsLocalController())
	{
		return false;
	}

	if (InventoryViewModel)
	{
		SyncInventoryViewModelSourceFromPawn();
		return true;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	UProjectUIFactorySubsystem* Factory = GameInstance->GetSubsystem<UProjectUIFactorySubsystem>();
	UProjectUIRegistrySubsystem* Registry = GameInstance->GetSubsystem<UProjectUIRegistrySubsystem>();
	if (!Factory || !Registry)
	{
		return false;
	}

	const FProjectUIDefinition* Definition = Registry->FindDefinition(TEXT("ProjectInventoryUI.InventoryPanel"));
	if (!Definition)
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("[Thread:%u] EnsureInventoryViewModelReady: Missing UI definition for ProjectInventoryUI.InventoryPanel"),
			FPlatformTLS::GetCurrentThreadId());
		return false;
	}

	Factory->EnsureGlobalViewModel(*Definition);
	TryBindInventoryViewModel();
	SyncInventoryViewModelSourceFromPawn();
	return InventoryViewModel != nullptr;
#else
	return false;
#endif
}

void ASinglePlayController::SyncInventoryViewModelSourceFromPawn()
{
#if !UE_SERVER
	UInventoryViewModel* InventoryVM = Cast<UInventoryViewModel>(InventoryViewModel);
	if (!InventoryVM)
	{
		return;
	}

	UObject* InventorySource = nullptr;
	if (APawn* ControlledPawn = GetPawn())
	{
		TInlineComponentArray<UActorComponent*> Components;
		ControlledPawn->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (Component && Component->GetClass()->ImplementsInterface(UInventoryReadOnly::StaticClass()))
			{
				InventorySource = Component;
				break;
			}
		}
	}

	InventoryVM->SetInventorySource(InventorySource);
#endif
}

void ASinglePlayController::TryBindMindJournalViewModel()
{
#if !UE_SERVER
	if (!IsLocalController() || MindJournalViewModel)
	{
		return;
	}

	UProjectUIFactorySubsystem* Factory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProjectUIFactorySubsystem>() : nullptr;
	if (!Factory)
	{
		return;
	}

	MindJournalViewModel = Cast<UMindJournalViewModel>(Factory->GetSharedViewModel(UMindJournalViewModel::StaticClass()));
#endif
}

void ASinglePlayController::CreateUIInputAssets()
{
	if (!UIInputMappingContext)
	{
		UIInputMappingContext = NewObject<UInputMappingContext>(this, TEXT("UIInputMappingContext"));
	}

	if (!ToggleVitalsAction)
	{
		ToggleVitalsAction = NewObject<UInputAction>(this, TEXT("IA_ToggleVitalsPanel"));
		ToggleVitalsAction->ValueType = EInputActionValueType::Boolean;
		UIInputMappingContext->MapKey(ToggleVitalsAction, EKeys::H);
	}

#if !UE_SERVER
	if (!ToggleGameMenuAction)
	{
		ToggleGameMenuAction = NewObject<UInputAction>(this, TEXT("IA_ToggleGameMenu"));
		ToggleGameMenuAction->ValueType = EInputActionValueType::Boolean;
		ToggleGameMenuAction->bTriggerWhenPaused = true;
		UIInputMappingContext->MapKey(ToggleGameMenuAction, EKeys::Escape);
	}

	if (!ToggleInventoryAction)
	{
		ToggleInventoryAction = NewObject<UInputAction>(this, TEXT("IA_ToggleInventoryPanel"));
		ToggleInventoryAction->ValueType = EInputActionValueType::Boolean;
		UIInputMappingContext->MapKey(ToggleInventoryAction, EKeys::I);
	}

	if (!ToggleMindJournalAction)
	{
		ToggleMindJournalAction = NewObject<UInputAction>(this, TEXT("IA_ToggleMindJournalPanel"));
		ToggleMindJournalAction->ValueType = EInputActionValueType::Boolean;
		UIInputMappingContext->MapKey(ToggleMindJournalAction, EKeys::J);
	}
#endif

	if (!InteractAction)
	{
		InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact"));
		InteractAction->ValueType = EInputActionValueType::Boolean;
		UIInputMappingContext->MapKey(InteractAction, EKeys::E);
	}

	if (!SwapHandsAction)
	{
		SwapHandsAction = NewObject<UInputAction>(this, TEXT("IA_SwapHands"));
		SwapHandsAction->ValueType = EInputActionValueType::Boolean;
		UIInputMappingContext->MapKey(SwapHandsAction, EKeys::X);
	}
}

void ASinglePlayController::HandleVitalsViewModelPropertyChanged(FName PropertyName)
{
	if (PropertyName == TEXT("bPanelVisible"))
	{
		if (UProjectUILayerHostSubsystem* LayerHost = GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
		{
			if (VitalsViewModel && VitalsViewModel->GetbPanelVisible())
			{
				LayerHost->ShowDefinition(TEXT("ProjectVitalsUI.VitalsPanel"));
			}
			else
			{
				LayerHost->HideDefinition(TEXT("ProjectVitalsUI.VitalsPanel"));
			}
		}
	}
}

void ASinglePlayController::HandleInventoryViewModelPropertyChanged(FName PropertyName)
{
#if !UE_SERVER
	LOG_INIT("HandleInventoryViewModelPropertyChanged - PropertyName=%s", *PropertyName.ToString());
	if (PropertyName == TEXT("bPanelVisible"))
	{
		if (UProjectUILayerHostSubsystem* LayerHost = GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
		{
			if (UInventoryViewModel* InventoryVM = Cast<UInventoryViewModel>(InventoryViewModel))
			{
				const bool bVisible = InventoryVM->GetbPanelVisible();
				LOG_INIT("HandleInventoryViewModelPropertyChanged - bPanelVisible=%d, toggling inventory UI",
					bVisible);
				// Both inventory widgets toggle as a single unit via the
				// coordinator; calling ShowDefinition for the main panel
				// alone leaves the nearby loot sibling unspawned and
				// breaks loot sessions silently.
				InventoryUIVisibilityCoordinator::SetInventoryUIVisible(LayerHost, bVisible);
				LOG_INIT("HandleInventoryViewModelPropertyChanged - Done, IgnoreMoveInput=%d, IgnoreLookInput=%d",
					IsMoveInputIgnored(), IsLookInputIgnored());
			}
		}
	}
#else
	(void)PropertyName;
#endif
}

void ASinglePlayController::HandleToggleVitalsAction(const FInputActionValue& Value)
{
	if (!Value.Get<bool>())
	{
		return;
	}
	TogglePanel_Vitals();
}

void ASinglePlayController::HandleToggleInventoryAction(const FInputActionValue& Value)
{
#if !UE_SERVER
	if (!Value.Get<bool>())
	{
		return;
	}
	TogglePanel_Inventory();
#else
	(void)Value;
#endif
}

void ASinglePlayController::HandleToggleMindJournalAction(const FInputActionValue& Value)
{
#if !UE_SERVER
	if (!Value.Get<bool>())
	{
		return;
	}
	TogglePanel_MindJournal();
#else
	(void)Value;
#endif
}

void ASinglePlayController::TogglePanel_Vitals()
{
	if (!IsLocalController()) return;

	if (!VitalsViewModel)
	{
		TryBindVitalsViewModel();
	}

	if (VitalsViewModel)
	{
		VitalsViewModel->TogglePanel();
		OnPanelVisibilityChanged.Broadcast(TEXT("Vitals"), VitalsViewModel->GetbPanelVisible());
	}
}

void ASinglePlayController::TogglePanel_Inventory()
{
#if !UE_SERVER
	if (!IsLocalController()) return;

	LOG_INIT("TogglePanel_Inventory START - HasViewModel=%d", InventoryViewModel != nullptr);
	LOG_INIT("TogglePanel_Inventory - BEFORE: IgnoreMoveInput=%d, IgnoreLookInput=%d",
		IsMoveInputIgnored(), IsLookInputIgnored());

	if (!EnsureInventoryViewModelReady())
	{
		return;
	}

	// Toggle panel - the delegate HandleInventoryViewModelPropertyChanged handles Show/Hide
	if (UInventoryViewModel* InventoryVM = Cast<UInventoryViewModel>(InventoryViewModel))
	{
		const bool bWasVisible = InventoryVM->GetbPanelVisible();
		LOG_INIT("TogglePanel_Inventory - Calling TogglePanel, wasVisible=%d", bWasVisible);
		InventoryVM->TogglePanel();
		LOG_INIT("TogglePanel_Inventory - After TogglePanel, isVisible=%d", InventoryVM->GetbPanelVisible());
		OnPanelVisibilityChanged.Broadcast(TEXT("Inventory"), InventoryVM->GetbPanelVisible());
	}

	LOG_INIT("TogglePanel_Inventory END - AFTER: IgnoreMoveInput=%d, IgnoreLookInput=%d",
		IsMoveInputIgnored(), IsLookInputIgnored());
#endif
}

void ASinglePlayController::TogglePanel_MindJournal()
{
#if !UE_SERVER
	if (!IsLocalController()) return;

	if (!MindJournalViewModel)
	{
		if (UProjectUILayerHostSubsystem* LayerHost = GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
		{
			LayerHost->ShowDefinition(TEXT("ProjectMindUI.MindJournalPanel"));
		}
		TryBindMindJournalViewModel();
	}

	if (MindJournalViewModel)
	{
		MindJournalViewModel->TogglePanel();
		OnPanelVisibilityChanged.Broadcast(TEXT("MindJournal"), MindJournalViewModel->GetbPanelVisible());
	}
#endif
}

// -----------------------------------------------------------------------------
// Idempotent setters (for Sequencer Event Tracks / scripted gameplay).
// Each reads current ViewModel state and forwards to Toggle only when the
// target state differs - so re-firing the same key under MRQ warmup or from
// a duplicated Director BP key is a no-op instead of a state flip.
// -----------------------------------------------------------------------------

void ASinglePlayController::SetPanel_VitalsVisible(bool bVisible)
{
	if (!IsLocalController()) return;
	if (!VitalsViewModel) TryBindVitalsViewModel();
	if (VitalsViewModel && VitalsViewModel->GetbPanelVisible() != bVisible)
	{
		VitalsViewModel->TogglePanel();
	}
	OnPanelVisibilityChanged.Broadcast(TEXT("Vitals"), bVisible);
}

void ASinglePlayController::SetPanel_InventoryVisible(bool bVisible)
{
#if !UE_SERVER
	if (!IsLocalController()) return;
	if (!EnsureInventoryViewModelReady()) return;
	if (UInventoryViewModel* VM = Cast<UInventoryViewModel>(InventoryViewModel))
	{
		if (VM->GetbPanelVisible() != bVisible)
		{
			VM->TogglePanel();
		}
	}
	OnPanelVisibilityChanged.Broadcast(TEXT("Inventory"), bVisible);
#else
	(void)bVisible;
#endif
}

void ASinglePlayController::SetPanel_MindJournalVisible(bool bVisible)
{
#if !UE_SERVER
	if (!IsLocalController()) return;
	if (!MindJournalViewModel)
	{
		if (UProjectUILayerHostSubsystem* LayerHost = GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
		{
			LayerHost->ShowDefinition(TEXT("ProjectMindUI.MindJournalPanel"));
		}
		TryBindMindJournalViewModel();
	}
	if (MindJournalViewModel && MindJournalViewModel->GetbPanelVisible() != bVisible)
	{
		MindJournalViewModel->TogglePanel();
	}
	OnPanelVisibilityChanged.Broadcast(TEXT("MindJournal"), bVisible);
#else
	(void)bVisible;
#endif
}

void ASinglePlayController::HandleInteractAction(const FInputActionValue& Value)
{
	if (!Value.Get<bool>())
	{
		return;
	}
	TriggerFocusedInteraction();
}

void ASinglePlayController::TriggerFocusedInteraction()
{
	if (!IsLocalController()) return;

#if !UE_SERVER
	if (UInventoryViewModel* InventoryVM = Cast<UInventoryViewModel>(InventoryViewModel))
	{
		if (InventoryVM->GetbPanelVisible() && InventoryVM->GetbHasNearbyContainer())
		{
			InventoryVM->HidePanel();
			return;
		}
	}
#endif

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	// Find interaction component on pawn (attached by Interaction feature)
	for (UActorComponent* Comp : ControlledPawn->GetComponents())
	{
		if (Comp && Comp->Implements<UInteractionComponentInterface>())
		{
			// Snapshot focus BEFORE BeginInteractInput so the world-container
			// session resolution path runs against the focus the player saw
			// at the moment of E-press (the player aimed at this dresser/etc
			// and pressed; downstream inventory state setup depends on that
			// pre-press snapshot).
			AActor* PrePressFocus = IInteractionComponentInterface::Execute_GetFocusedActor(Comp);
#if !UE_SERVER
			if (ResolveWorldContainerSessionSource(PrePressFocus))
			{
				EnsureInventoryViewModelReady();
			}
#endif
			// BeginInteractInput re-traces internally and may shift focus
			// between the snapshot above and the actual interaction.
			// Capture its return so we only broadcast when input was
			// accepted by SOME interactable -- for hold interactions this
			// means "the hold has started," not "the interaction has
			// completed execution," so observers must not assume completion
			// from this broadcast alone. The point of the gate is to
			// suppress spurious broadcasts when the press lands on nothing
			// (no focus post-trace, range fail, all capabilities rejected),
			// not to guarantee completion: that lower bar already prevents
			// the cinematic recorder from AddSource'ing actors the player
			// didn't actually engage with.
			const bool bInteractionAccepted =
				IInteractionComponentInterface::Execute_BeginInteractInput(Comp);
			if (!bInteractionAccepted)
			{
				return;
			}
			// Re-read focus POST-update so the broadcast reflects what was
			// actually interacted with (not the stale pre-press snapshot).
			AActor* InteractedActor =
				IInteractionComponentInterface::Execute_GetFocusedActor(Comp);
			// Resolve the capability component that responded (set by
			// AInteractableActor::OnInteract_Implementation before its
			// component-side execution returned). Pass nullptr when the
			// focused actor isn't an AInteractableActor or has no responding
			// component cached -- observers handle both shapes.
			UActorComponent* Responder = nullptr;
			if (AInteractableActor* IA = Cast<AInteractableActor>(InteractedActor))
			{
				Responder = IA->GetLastRespondingComponent();
			}
			OnInteractionTriggered.Broadcast(InteractedActor, Responder);
			return;
		}
	}
}

void ASinglePlayController::TriggerInteractionWithActor(AActor* TargetActor)
{
	if (!IsLocalController() || !TargetActor) return;

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	for (UActorComponent* Comp : ControlledPawn->GetComponents())
	{
		if (Comp && Comp->Implements<UInteractionComponentInterface>())
		{
			const bool bOk = IInteractionComponentInterface::Execute_TryInteractWithActor(Comp, TargetActor);
			if (bOk)
			{
				// Broadcast the targeted-interaction event so any external
				// observer (e.g. analytics, recorder during a non-Take session)
				// sees a symmetric signal to OnInteractionTriggered.
				UActorComponent* Responder = nullptr;
				if (AInteractableActor* IA = Cast<AInteractableActor>(TargetActor))
				{
					Responder = IA->GetLastRespondingComponent();
				}
				OnInteractionTriggered.Broadcast(TargetActor, Responder);
			}
			return;
		}
	}
}

void ASinglePlayController::HandleInteractReleasedAction(const FInputActionValue& Value)
{
	(void)Value;

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	for (UActorComponent* Comp : ControlledPawn->GetComponents())
	{
		if (Comp && Comp->Implements<UInteractionComponentInterface>())
		{
			IInteractionComponentInterface::Execute_EndInteractInput(Comp);
			return;
		}
	}
}

void ASinglePlayController::HandleSwapHandsAction(const FInputActionValue& Value)
{
	if (!Value.Get<bool>())
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	// Find inventory component on pawn and call RequestSwapHands
	for (UActorComponent* Comp : ControlledPawn->GetComponents())
	{
		if (IInventoryCommands* InventoryCommands = Cast<IInventoryCommands>(Comp))
		{
			InventoryCommands->RequestSwapHands();
			return;
		}
	}
}

void ASinglePlayController::NotifyMindInputActivity()
{
	if (!IsLocalController())
	{
		return;
	}

	const TSharedPtr<IMindRuntimeControl> MindRuntimeControl = FProjectServiceLocator::Resolve<IMindRuntimeControl>();
	if (MindRuntimeControl.IsValid())
	{
		MindRuntimeControl->NotifyPlayerInputActivity();
	}
}
