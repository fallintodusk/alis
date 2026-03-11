#include "SinglePlayerPlayerController.h"
#include "ProjectSinglePlayLog.h"
#include "GameFramework/Character.h"
#include "Engine/GameViewportClient.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "MVVM/VitalsViewModel.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Subsystems/ProjectUIFactorySubsystem.h"
#include "HAL/PlatformTLS.h"
#include "Interfaces/IInteractionService.h"
#include "Interfaces/IInventoryCommands.h"
#include "Interfaces/IMindRuntimeControl.h"
#include "ProjectServiceLocator.h"
#if !UE_SERVER
#include "MVVM/InventoryViewModel.h"
#include "MVVM/MindJournalViewModel.h"
#endif

// Thread-safe logging macro for init diagnostics
#define LOG_INIT(Format, ...) UE_LOG(LogProjectSinglePlay, Log, TEXT("[Thread:%u] " Format), FPlatformTLS::GetCurrentThreadId(), ##__VA_ARGS__)

ASinglePlayerPlayerController::ASinglePlayerPlayerController()
{
	// Defaults - will be overridden in OnPossess based on pawn type
	bShowMouseCursor = false;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	VitalsUIBindAttempts = 0;
}

void ASinglePlayerPlayerController::BeginPlay()
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

void ASinglePlayerPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LOG_INIT("EndPlay START - Reason=%d", static_cast<int32>(EndPlayReason));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VitalsUIRetryHandle);
		LOG_INIT("EndPlay: Cleared VitalsUIRetryHandle timer");
	}

	if (VitalsViewModel)
	{
		VitalsViewModel->OnPropertyChanged.RemoveDynamic(this, &ASinglePlayerPlayerController::HandleVitalsViewModelPropertyChanged);
		LOG_INIT("EndPlay: Unbound from VitalsViewModel.OnPropertyChanged");
	}

#if !UE_SERVER
	if (UInventoryViewModel* InventoryVM = Cast<UInventoryViewModel>(InventoryViewModel))
	{
		InventoryVM->OnPropertyChanged.RemoveDynamic(this, &ASinglePlayerPlayerController::HandleInventoryViewModelPropertyChanged);
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

void ASinglePlayerPlayerController::OnPossess(APawn* InPawn)
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

void ASinglePlayerPlayerController::SetupInputComponent()
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
				&ASinglePlayerPlayerController::HandleToggleVitalsAction);
			LOG_INIT("SetupInputComponent: Bound ToggleVitalsAction");
		}

#if !UE_SERVER
		if (ToggleInventoryAction)
		{
			EnhancedInputComponent->BindAction(
				ToggleInventoryAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayerPlayerController::HandleToggleInventoryAction);
			LOG_INIT("SetupInputComponent: Bound ToggleInventoryAction");
		}

		if (ToggleMindJournalAction)
		{
			EnhancedInputComponent->BindAction(
				ToggleMindJournalAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayerPlayerController::HandleToggleMindJournalAction);
			LOG_INIT("SetupInputComponent: Bound ToggleMindJournalAction");
		}
#endif

		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(
				InteractAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayerPlayerController::HandleInteractAction);
			LOG_INIT("SetupInputComponent: Bound InteractAction");
		}

		if (SwapHandsAction)
		{
			EnhancedInputComponent->BindAction(
				SwapHandsAction,
				ETriggerEvent::Started,
				this,
				&ASinglePlayerPlayerController::HandleSwapHandsAction);
			LOG_INIT("SetupInputComponent: Bound SwapHandsAction");
		}
	}
	else
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("[Thread:%u] SetupInputComponent: No EnhancedInputComponent"), FPlatformTLS::GetCurrentThreadId());
	}
	LOG_INIT("SetupInputComponent END");
}

bool ASinglePlayerPlayerController::InputKey(const FInputKeyEventArgs& Params)
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

void ASinglePlayerPlayerController::SetFirstPersonInputMode()
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

void ASinglePlayerPlayerController::SetUIInputMode()
{
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ASinglePlayerPlayerController::InitializeVitalsUI(APawn* InPawn)
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

void ASinglePlayerPlayerController::InitializeInventoryUI(APawn* InPawn)
{
#if !UE_SERVER
	// Inventory UI is demand-loaded: ViewModel created on first toggle, not on possess.
	// TryBindInventoryViewModel() is called from HandleToggleInventoryAction() when user opens inventory.
	LOG_INIT("InitializeInventoryUI: Pawn=%s (binding deferred to first toggle)", InPawn ? *InPawn->GetName() : TEXT("null"));
#else
	(void)InPawn;
#endif
}

void ASinglePlayerPlayerController::TryBindVitalsViewModel()
{
	LOG_INIT("TryBindVitalsViewModel START - Attempt=%d", VitalsUIBindAttempts + 1);

	if (!IsLocalController() || VitalsViewModel || !GetWorld())
	{
		LOG_INIT("TryBindVitalsViewModel: Early return - IsLocal=%d, HasVM=%d, HasWorld=%d",
			IsLocalController(), VitalsViewModel != nullptr, GetWorld() != nullptr);
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
			VitalsViewModel->OnPropertyChanged.RemoveDynamic(this, &ASinglePlayerPlayerController::HandleVitalsViewModelPropertyChanged);
			LOG_INIT("TryBindVitalsViewModel: Unbound previous ViewModel");
		}

		VitalsViewModel = FoundVM;
		VitalsViewModel->OnPropertyChanged.AddUniqueDynamic(this, &ASinglePlayerPlayerController::HandleVitalsViewModelPropertyChanged);
		LOG_INIT("TryBindVitalsViewModel: Bound to VitalsViewModel.OnPropertyChanged delegate");

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
			&ASinglePlayerPlayerController::TryBindVitalsViewModel,
			0.2f,
			false);
	}
	else
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("[Thread:%u] TryBindVitalsViewModel FAILED - No ViewModel after %d retries"),
			FPlatformTLS::GetCurrentThreadId(), VitalsUIBindAttempts);
	}
}

void ASinglePlayerPlayerController::TryBindInventoryViewModel()
{
#if !UE_SERVER
	// Called from HandleToggleInventoryAction after ShowDefinition creates the ViewModel.
	// No retry loop needed - ViewModel exists by the time this is called.

	if (!IsLocalController() || InventoryViewModel)
	{
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
		InventoryViewModel = FoundVM;
		FoundVM->OnPropertyChanged.AddUniqueDynamic(this, &ASinglePlayerPlayerController::HandleInventoryViewModelPropertyChanged);
		LOG_INIT("TryBindInventoryViewModel: Bound to InventoryViewModel");
	}
#endif
}

void ASinglePlayerPlayerController::TryBindMindJournalViewModel()
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

void ASinglePlayerPlayerController::CreateUIInputAssets()
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

void ASinglePlayerPlayerController::HandleVitalsViewModelPropertyChanged(FName PropertyName)
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

void ASinglePlayerPlayerController::HandleInventoryViewModelPropertyChanged(FName PropertyName)
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
				LOG_INIT("HandleInventoryViewModelPropertyChanged - bPanelVisible=%d, calling %s",
					bVisible, bVisible ? TEXT("ShowDefinition") : TEXT("HideDefinition"));
				if (bVisible)
				{
					LayerHost->ShowDefinition(TEXT("ProjectInventoryUI.InventoryPanel"));
				}
				else
				{
					LayerHost->HideDefinition(TEXT("ProjectInventoryUI.InventoryPanel"));
				}
				LOG_INIT("HandleInventoryViewModelPropertyChanged - Done, IgnoreMoveInput=%d, IgnoreLookInput=%d",
					IsMoveInputIgnored(), IsLookInputIgnored());
			}
		}
	}
#else
	(void)PropertyName;
#endif
}

void ASinglePlayerPlayerController::HandleToggleVitalsAction(const FInputActionValue& Value)
{
	if (!Value.Get<bool>())
	{
		return;
	}

	if (!VitalsViewModel)
	{
		TryBindVitalsViewModel();
	}

	if (VitalsViewModel)
	{
		VitalsViewModel->TogglePanel();
	}
}

void ASinglePlayerPlayerController::HandleToggleInventoryAction(const FInputActionValue& Value)
{
#if !UE_SERVER
	if (!Value.Get<bool>())
	{
		return;
	}

	LOG_INIT("HandleToggleInventoryAction START - HasViewModel=%d", InventoryViewModel != nullptr);
	LOG_INIT("HandleToggleInventoryAction - BEFORE: IgnoreMoveInput=%d, IgnoreLookInput=%d",
		IsMoveInputIgnored(), IsLookInputIgnored());

	// Bootstrap: If ViewModel not bound yet, show the panel to create it, then bind
	if (!InventoryViewModel)
	{
		LOG_INIT("HandleToggleInventoryAction - Bootstrap: showing definition to create ViewModel");
		if (UProjectUILayerHostSubsystem* LayerHost = GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>())
		{
			// Ensure the InventoryPanel definition is created so the shared ViewModel exists.
			LayerHost->ShowDefinition(TEXT("ProjectInventoryUI.InventoryPanel"));
		}
		TryBindInventoryViewModel();
	}

	// Toggle panel - the delegate HandleInventoryViewModelPropertyChanged handles Show/Hide
	if (UInventoryViewModel* InventoryVM = Cast<UInventoryViewModel>(InventoryViewModel))
	{
		const bool bWasVisible = InventoryVM->GetbPanelVisible();
		LOG_INIT("HandleToggleInventoryAction - Calling TogglePanel, wasVisible=%d", bWasVisible);
		InventoryVM->TogglePanel();
		LOG_INIT("HandleToggleInventoryAction - After TogglePanel, isVisible=%d", InventoryVM->GetbPanelVisible());
	}

	LOG_INIT("HandleToggleInventoryAction END - AFTER: IgnoreMoveInput=%d, IgnoreLookInput=%d",
		IsMoveInputIgnored(), IsLookInputIgnored());
#else
	(void)Value;
#endif
}

void ASinglePlayerPlayerController::HandleToggleMindJournalAction(const FInputActionValue& Value)
{
#if !UE_SERVER
	if (!Value.Get<bool>())
	{
		return;
	}

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
	}
#else
	(void)Value;
#endif
}

void ASinglePlayerPlayerController::HandleInteractAction(const FInputActionValue& Value)
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

	// Find interaction component on pawn (attached by Interaction feature)
	for (UActorComponent* Comp : ControlledPawn->GetComponents())
	{
		if (Comp && Comp->Implements<UInteractionComponentInterface>())
		{
			IInteractionComponentInterface::Execute_TryInteract(Comp);
			return;
		}
	}
}

void ASinglePlayerPlayerController::HandleSwapHandsAction(const FInputActionValue& Value)
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

void ASinglePlayerPlayerController::NotifyMindInputActivity()
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
