// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Scenario/SinglePlayScenarioPackagedGate.h"

#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "InputKeyEventArgs.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/IInteractionService.h"
#include "Interfaces/IVitalsReadOnly.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProjectGameplayTags.h"
#include "ProjectSinglePlayLog.h"
#include "RHI.h"
#include "Scenario/SinglePlayScenarioRunnerComponent.h"
#include "Scenario/SinglePlayScenarioInventoryInput.h"
#include "Scenario/SinglePlayScenarioSlateInput.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SinglePlayController.h"
#include "SinglePlayerGameMode.h"
#include "UnrealClient.h"
#include "Widgets/W_InventoryCellDropTarget.h"
#include "Widgets/W_InventoryEquipSlotDropTarget.h"
#include "Widgets/W_InventoryPanel.h"
#include "Widgets/ProjectGridCell.h"
#include "MVVM/InventoryViewModel.h"
#include "Components/Button.h"

namespace
{
	constexpr double GateTimeoutSeconds = 300.0;
	constexpr double WorldTimeoutSeconds = 120.0;
	constexpr double TravelTimeoutSeconds = 45.0;
	constexpr double InteractionHoldSeconds = 1.5;
	constexpr double UiTransitionSeconds = 0.35;
	constexpr double ScreenshotTimeoutSeconds = 20.0;
	constexpr float CacheArrivalRadius = 120.0f;
	constexpr float ShelterArrivalRadius = 700.0f;
	constexpr float FlightCruiseHeight = 6000.0f;
	constexpr float FlightDescentStartRadius = 500.0f;

	bool ParseRequiredValue(const TCHAR* Name, FString& OutValue)
	{
		return FParse::Value(FCommandLine::Get(), Name, OutValue) && !OutValue.IsEmpty();
	}

	FVector2D WidgetCenter(const UWidget& Widget)
	{
		return Widget.GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5));
	}

	bool IsScenarioUsableWidget(const UWidget& Widget, const APlayerController& Controller)
	{
		return Widget.GetOwningPlayer() == &Controller && Widget.IsVisible() &&
			Widget.GetCachedGeometry().GetLocalSize().GetMin() > 1.0f;
	}

}

FSinglePlayScenarioPackagedGate::~FSinglePlayScenarioPackagedGate()
{
	ReleaseInputs();
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	if (InteractionHandle.IsValid() && Controller.IsValid())
	{
		Controller->OnInteractionTriggered.Remove(InteractionHandle);
	}
	if (InventoryErrorHandle.IsValid() && InventoryViewModel.IsValid())
	{
		InventoryViewModel->OnInventoryError.Remove(InventoryErrorHandle);
	}
}

void FSinglePlayScenarioPackagedGate::StartIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectSinglePlayScenarioGate")))
	{
		return;
	}
	FString Error;
	if (!ParseConfig(Error))
	{
		FinishRejected(TEXT("scenario_config_invalid"), Error);
		return;
	}
	GateStartedSeconds = FPlatformTime::Seconds();
	SetPhase(EPhase::WaitingForWorld);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FSinglePlayScenarioPackagedGate::Tick));
	UE_LOG(LogProjectSinglePlay, Display,
		TEXT("[FSinglePlayScenarioPackagedGate::StartIfRequested] Started - route=%s result=%s"),
		Route == ERoute::Success ? TEXT("success") : TEXT("failure"), *ResultPath);
}

bool FSinglePlayScenarioPackagedGate::ParseConfig(FString& OutError)
{
	FString RouteValue;
	if (!ParseRequiredValue(TEXT("ProjectSinglePlayScenarioOperation="), OperationId) ||
		!ParseRequiredValue(TEXT("ProjectSinglePlayScenarioResult="), ResultPath) ||
		!ParseRequiredValue(TEXT("ProjectSinglePlayScenarioScreenshot="), ScreenshotPath) ||
		!ParseRequiredValue(TEXT("ProjectSinglePlayScenarioRoute="), RouteValue) ||
		!ParseRequiredValue(TEXT("ProjectWorldRuntimeProfile="), RuntimeProfileId) ||
		!ParseRequiredValue(TEXT("ProjectWorldRuntimeProfileSha256="), RuntimeProfileHash) ||
		!ParseRequiredValue(TEXT("ProjectWorldMachineProfile="), MachineProfileId) ||
		FPaths::IsRelative(ResultPath) || FPaths::IsRelative(ScreenshotPath))
	{
		OutError = TEXT("Scenario gate requires absolute result/screenshot paths and complete operation/profile identity.");
		return false;
	}
	if (RouteValue.Equals(TEXT("success"), ESearchCase::IgnoreCase))
	{
		Route = ERoute::Success;
	}
	else if (RouteValue.Equals(TEXT("failure"), ESearchCase::IgnoreCase))
	{
		Route = ERoute::Failure;
	}
	else
	{
		OutError = TEXT("Scenario route must be Success or Failure.");
		return false;
	}
	return true;
}

bool FSinglePlayScenarioPackagedGate::Tick(float DeltaSeconds)
{
	(void)DeltaSeconds;
	const double Now = FPlatformTime::Seconds();
	if (Phase == EPhase::Finished)
	{
		return false;
	}
	if (Now - GateStartedSeconds > GateTimeoutSeconds)
	{
		FinishRejected(TEXT("scenario_timeout"), TEXT("The bounded packaged scenario route timed out."));
		return false;
	}

	FString Error;
	if (Phase == EPhase::WaitingForWorld)
	{
		if (TryAcquireWorld(Error))
		{
			SetPhase(Route == ERoute::Success ? EPhase::TravelToCache : EPhase::TravelToShelter);
		}
		else if (Now - PhaseStartedSeconds > WorldTimeoutSeconds)
		{
			FinishRejected(TEXT("scenario_world_unavailable"), Error);
		}
		return Phase != EPhase::Finished;
	}
	if (Phase == EPhase::WaitingForRestart)
	{
		if (TryAcquireRestartedWorld(Error))
		{
			bRestartObserved = true;
			if (!RequestScreenshot(Error))
			{
				FinishRejected(TEXT("scenario_restart_screenshot_failed"), Error);
			}
			else
			{
				SetPhase(EPhase::WaitingForScreenshot);
			}
		}
		else if (Now - PhaseStartedSeconds > 30.0)
		{
			FinishRejected(TEXT("scenario_restart_missing"), Error);
		}
		return Phase != EPhase::Finished;
	}

	if (!World.IsValid() || !Controller.IsValid() || !Character.IsValid() || !Runner.IsValid())
	{
		FinishRejected(TEXT("scenario_ownership_lost"), TEXT("The product world, controller, character, or scenario runner was lost."));
		return false;
	}

	switch (Phase)
	{
	case EPhase::TravelToCache:
		TickTravel(*CacheActor.Get(), CacheArrivalRadius, EPhase::AimAtCache, Error);
		break;
	case EPhase::AimAtCache:
		if (TickAimAtActor(*CacheActor.Get()))
		{
			SendKeyPress(EKeys::E);
			SetPhase(EPhase::HoldInteraction);
		}
		break;
	case EPhase::HoldInteraction:
		if (Now - PhaseStartedSeconds >= InteractionHoldSeconds)
		{
			ReleaseKey(EKeys::E);
			SetPhase(EPhase::WaitingForInventory);
		}
		else
		{
			HoldKey(EKeys::E);
		}
		break;
	case EPhase::WaitingForInventory:
		if (UInventoryViewModel* ViewModel = FindInventoryViewModel();
			ViewModel != nullptr && ViewModel->GetbPanelVisible() && ViewModel->GetbHasNearbyContainer())
		{
			InventoryViewModel = ViewModel;
			InventoryErrorHandle = ViewModel->OnInventoryError.AddRaw(
				this, &FSinglePlayScenarioPackagedGate::HandleInventoryError);
			bInventoryOpened = true;
			SetPhase(EPhase::MovePouchToBack);
		}
		else if (Now - PhaseStartedSeconds > 10.0)
		{
			FinishRejected(TEXT("scenario_inventory_not_opened"), TEXT("Real E input did not open the emergency cache inventory UI."));
		}
		break;
	case EPhase::MovePouchToBack:
		if (!DragStep && !HasItem(TEXT("EmergencyPouch")) &&
			!BeginItemDrag(TEXT("EmergencyPouch"), ProjectTags::Item_Container_LeftHand, 0, Error))
		{
			FinishRejected(TEXT("scenario_pouch_take_invalid"), Error);
			break;
		}
		else if (!DragStep && !bPouchEquipped &&
			!BeginEquipDrag(TEXT("EmergencyPouch"), ProjectTags::Item_EquipmentSlot_Back, Error))
		{
			FinishRejected(TEXT("scenario_pouch_drag_invalid"), Error);
			break;
		}
		if (DragStep)
		{
			TickDrag();
		}
		if (UInventoryViewModel* ViewModel = FindInventoryViewModel(); !DragStep && HasItem(TEXT("EmergencyPouch")) && ViewModel != nullptr)
		{
			for (int32 Index = 0; Index < ViewModel->GetEquipSlotCount(); ++Index)
			{
				if (ViewModel->GetEquipSlotTag(Index) == ProjectTags::Item_EquipmentSlot_Back &&
					ViewModel->GetEquipSlotInstanceId(Index) != INDEX_NONE)
				{
					bPouchEquipped = true;
					SetPhase(EPhase::MoveWaterToLeftHand);
					break;
				}
			}
		}
		break;
	case EPhase::MoveWaterToLeftHand:
		if (!DragStep && !HasItem(TEXT("EmergencyWater")) &&
			!BeginItemDrag(TEXT("EmergencyWater"), ProjectTags::Item_Container_LeftHand, 0, Error))
		{
			FinishRejected(TEXT("scenario_water_drag_invalid"), Error);
			break;
		}
		if (DragStep) { TickDrag(); }
		if (HasItem(TEXT("EmergencyWater"))) { SetPhase(EPhase::MoveRationToRightHand); }
		break;
	case EPhase::MoveRationToRightHand:
		if (!DragStep && !HasItem(TEXT("EmergencyRation")) &&
			!BeginItemDrag(TEXT("EmergencyRation"), ProjectTags::Item_Container_RightHand, 0, Error))
		{
			FinishRejected(TEXT("scenario_ration_drag_invalid"), Error);
			break;
		}
		if (DragStep) { TickDrag(); }
		if (HasItem(TEXT("EmergencyRation"))) { SetPhase(EPhase::MoveMedkitToBackpack); }
		break;
	case EPhase::MoveMedkitToBackpack:
		if (!DragStep && !HasItem(TEXT("EmergencyMedkit")) &&
			!BeginItemDrag(TEXT("EmergencyMedkit"), ProjectTags::Item_Container_Backpack, 0, Error))
		{
			FinishRejected(TEXT("scenario_medkit_drag_invalid"), Error);
			break;
		}
		if (DragStep) { TickDrag(); }
		if (HasItem(TEXT("EmergencyMedkit"))) { SetPhase(EPhase::RejectPryBarFromBackpack); }
		break;
	case EPhase::RejectPryBarFromBackpack:
		if (!DragStep && !bCapacityRejected &&
			!BeginItemDrag(TEXT("CompactPryBar"), ProjectTags::Item_Container_Backpack, 4, Error))
		{
			FinishRejected(TEXT("scenario_prybar_drag_invalid"), Error);
			break;
		}
		if (DragStep) { TickDrag(); }
		if (bCapacityRejected)
		{
			SetPhase(EPhase::UseWater);
		}
		else if (Now - PhaseStartedSeconds > 10.0)
		{
			FinishRejected(TEXT("scenario_capacity_not_rejected"), TEXT("The 1.4 kg backpack attempt did not emit the expected capacity rejection."));
		}
		break;
	case EPhase::UseWater:
		if (UInventoryViewModel* ViewModel = FindInventoryViewModel(); !bWaterUsed &&
			(ViewModel == nullptr || !FSinglePlayScenarioInventoryInput::TickUseItem(
				TEXT("EmergencyWater"), *ViewModel, *Controller.Get(), bWaterContextOpened,
				bWaterUseRequested, UiPointerEventCount, Error)))
		{
			FinishRejected(TEXT("scenario_water_use_invalid"), Error);
			break;
		}
		if (ReadHydration(HydrationAfter) && HydrationAfter > 0.20)
		{
			bWaterUsed = true;
			SetPhase(EPhase::CloseInventory);
		}
		else if (Now - PhaseStartedSeconds > 10.0)
		{
			FinishRejected(TEXT("scenario_hydration_not_recovered"), TEXT("The real inventory Use action did not cross the hydration threshold."));
		}
		break;
	case EPhase::CloseInventory:
		if (Now - PhaseStartedSeconds < UiTransitionSeconds)
		{
			break;
		}
		SendKeyPress(EKeys::E);
		ReleaseKey(EKeys::E);
		SetPhase(EPhase::TravelToShelter);
		break;
	case EPhase::TravelToShelter:
		TickTravel(*ShelterActor.Get(), ShelterArrivalRadius, EPhase::WaitingForTerminal, Error);
		break;
	case EPhase::WaitingForTerminal:
		if (Route == ERoute::Success && Runner->GetPhase() == ESinglePlayScenarioPhase::Succeeded)
		{
			ShelterArrivalLocation = Character->GetActorLocation();
			if (!RequestScreenshot(Error))
			{
				FinishRejected(TEXT("scenario_screenshot_request_failed"), Error);
			}
			else
			{
				SetPhase(EPhase::WaitingForScreenshot);
			}
		}
		else if (Route == ERoute::Failure && Runner->GetPhase() == ESinglePlayScenarioPhase::Failed)
		{
			bFailureObserved = true;
			SetPhase(EPhase::WaitingForRestart);
		}
		else if (Now - PhaseStartedSeconds > 10.0)
		{
			FinishRejected(TEXT("scenario_terminal_state_missing"), TEXT("Shelter arrival did not produce the selected terminal scenario state."));
		}
		break;
	case EPhase::WaitingForScreenshot:
		if (FPaths::FileExists(ScreenshotPath) && IFileManager::Get().FileSize(*ScreenshotPath) > 0)
		{
			FinishAccepted();
		}
		else if (Now - PhaseStartedSeconds > ScreenshotTimeoutSeconds)
		{
			FinishRejected(TEXT("scenario_screenshot_missing"), TEXT("The requested packaged scenario screenshot was not written."));
		}
		break;
	default:
		break;
	}
	if (!Error.IsEmpty() && Phase != EPhase::Finished)
	{
		FinishRejected(TEXT("scenario_route_invalid"), Error);
	}
	return Phase != EPhase::Finished;
}

bool FSinglePlayScenarioPackagedGate::TryAcquireWorld(FString& OutError)
{
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		OutError = TEXT("The packaged game viewport is not ready.");
		return false;
	}
	UWorld* CandidateWorld = GEngine->GameViewport->GetWorld();
	ASinglePlayController* CandidateController = CandidateWorld == nullptr
		? nullptr
		: Cast<ASinglePlayController>(CandidateWorld->GetFirstPlayerController());
	ACharacter* CandidateCharacter = CandidateController == nullptr
		? nullptr
		: Cast<ACharacter>(CandidateController->GetPawn());
	ASinglePlayerGameMode* GameMode = CandidateWorld == nullptr
		? nullptr
		: CandidateWorld->GetAuthGameMode<ASinglePlayerGameMode>();
	USinglePlayScenarioRunnerComponent* CandidateRunner = GameMode == nullptr
		? nullptr
		: GameMode->GetScenarioRunner();
	if (CandidateWorld == nullptr || CandidateController == nullptr || CandidateCharacter == nullptr ||
		CandidateRunner == nullptr || CandidateRunner->GetScenarioId() != TEXT("UrbanSurvivalProofV1") ||
		CandidateWorld->URL.GetOption(TEXT("ProjectLoadingRoute="), TEXT("")) != FString(TEXT("1")))
	{
		OutError = TEXT("Waiting for the menu-to-ProjectLoading Kazan scenario route and possessed production character.");
		return false;
	}
	AActor* CandidateCache = nullptr;
	AActor* CandidateShelter = nullptr;
	for (TActorIterator<AActor> It(CandidateWorld); It; ++It)
	{
		if (It->ActorHasTag(TEXT("ProjectScenario.Anchor.Cache")) &&
			It->GetClass()->ImplementsInterface(UInteractableTargetInterface::StaticClass())) { CandidateCache = *It; }
		if (It->ActorHasTag(TEXT("ProjectScenario.Anchor.Shelter"))) { CandidateShelter = *It; }
	}
	if (CandidateCache == nullptr || CandidateShelter == nullptr)
	{
		OutError = TEXT("Waiting for the protected cache and shelter authored overlays.");
		return false;
	}

	World = CandidateWorld;
	Controller = CandidateController;
	Character = CandidateCharacter;
	Runner = CandidateRunner;
	CacheActor = CandidateCache;
	ShelterActor = CandidateShelter;
	StartLocation = CandidateCharacter->GetActorLocation();
	ReadHydration(HydrationBefore);
	InteractionHandle = CandidateController->OnInteractionTriggered.AddRaw(
		this, &FSinglePlayScenarioPackagedGate::HandleInteraction);
	return true;
}

bool FSinglePlayScenarioPackagedGate::TryAcquireRestartedWorld(FString& OutError)
{
	UWorld* CandidateWorld = GEngine != nullptr && GEngine->GameViewport != nullptr
		? GEngine->GameViewport->GetWorld()
		: nullptr;
	ASinglePlayController* CandidateController = CandidateWorld == nullptr
		? nullptr
		: Cast<ASinglePlayController>(CandidateWorld->GetFirstPlayerController());
	ACharacter* CandidateCharacter = CandidateController == nullptr
		? nullptr
		: Cast<ACharacter>(CandidateController->GetPawn());
	ASinglePlayerGameMode* GameMode = CandidateWorld == nullptr
		? nullptr
		: CandidateWorld->GetAuthGameMode<ASinglePlayerGameMode>();
	USinglePlayScenarioRunnerComponent* CandidateRunner = GameMode == nullptr
		? nullptr
		: GameMode->GetScenarioRunner();
	if (CandidateWorld == nullptr || CandidateController == nullptr || CandidateCharacter == nullptr ||
		CandidateRunner == nullptr || CandidateRunner == Runner.Get() ||
		CandidateRunner->GetScenarioId() != TEXT("UrbanSurvivalProofV1") ||
		CandidateRunner->GetPhase() != ESinglePlayScenarioPhase::SearchCache ||
		CandidateWorld->URL.GetOption(TEXT("ProjectLoadingRoute="), TEXT("")) != FString(TEXT("1")))
	{
		OutError = TEXT("Failure did not return through ProjectLoading to a fresh scenario runner.");
		return false;
	}
	World = CandidateWorld;
	Controller = CandidateController;
	Character = CandidateCharacter;
	Runner = CandidateRunner;
	return true;
}

bool FSinglePlayScenarioPackagedGate::TickTravel(
	AActor& Target,
	float ArrivalRadius,
	EPhase NextPhase,
	FString& OutError)
{
	const FVector Location = Character->GetActorLocation();
	const FVector Delta = Target.GetActorLocation() - Location;
	const double Distance = Delta.Size();
	const double Distance2D = FVector2D(Delta.X, Delta.Y).Size();
	const FVector Velocity = Character->GetVelocity();
	const double Speed2D = FVector2D(Velocity.X, Velocity.Y).Size();
	const bool bIsShelter = &Target == ShelterActor.Get();
	const double AcceptanceDistance = bIsShelter ? Distance : Distance2D;
	if (AcceptanceDistance <= ArrivalRadius && (bIsShelter || Speed2D <= 100.0))
	{
		ReleaseInputs();
		if (&Target == CacheActor.Get()) { CacheArrivalLocation = Location; }
		if (&Target == ShelterActor.Get()) { ShelterArrivalLocation = Location; }
		SetPhase(NextPhase);
		return true;
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > TravelTimeoutSeconds)
	{
		OutError = FString::Printf(
			TEXT("Real Space/W/Ctrl/look input did not reach %s within the bounded route timeout "
				"(remaining_cm=%.1f horizontal_cm=%.1f vertical_cm=%.1f)."),
			*Target.GetName(), Distance, Distance2D, Delta.Z);
		return false;
	}
	const double DescentRadius = FMath::Max(static_cast<double>(ArrivalRadius),
		static_cast<double>(FlightDescentStartRadius));
	if (bIsShelter && Distance2D <= DescentRadius)
	{
		ReleaseKey(EKeys::W);
		const double PitchError = FMath::FindDeltaAngleDegrees(Controller->GetControlRotation().Pitch, 0.0);
		SendLookInput(0.0, PitchError);
		if (Delta.Z < 0.0)
		{
			ReleaseKey(EKeys::SpaceBar);
			HoldKey(EKeys::LeftControl);
		}
		else
		{
			ReleaseKey(EKeys::LeftControl);
			HoldKey(EKeys::SpaceBar);
		}
		return false;
	}
	if (bIsShelter && Location.Z < Target.GetActorLocation().Z + FlightCruiseHeight)
	{
		ReleaseKey(EKeys::W);
		ReleaseKey(EKeys::LeftControl);
		HoldKey(EKeys::SpaceBar);
		return false;
	}
	ReleaseKey(EKeys::SpaceBar);
	ReleaseKey(EKeys::LeftControl);
	const double TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	const double YawError = FMath::FindDeltaAngleDegrees(Controller->GetControlRotation().Yaw, TargetYaw);
	const double PitchError = bIsShelter
		? FMath::FindDeltaAngleDegrees(Controller->GetControlRotation().Pitch, 0.0)
		: 0.0;
	SendLookInput(YawError, PitchError);
	const FVector2D Direction = FVector2D(Delta.X, Delta.Y).GetSafeNormal();
	const double ApproachSpeed = FMath::Max(0.0, FVector2D::DotProduct(FVector2D(Velocity.X, Velocity.Y), Direction));
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const double Braking = Movement == nullptr ? 1.0 : FMath::Max(1.0, static_cast<double>(Movement->BrakingDecelerationFlying));
	const double StoppingDistance = FMath::Square(ApproachSpeed) / (2.0 * Braking);
	if (FMath::Abs(YawError) < 20.0 && Distance2D - ArrivalRadius > StoppingDistance)
	{
		HoldKey(EKeys::W);
	}
	else
	{
		ReleaseKey(EKeys::W);
	}
	return false;
}

bool FSinglePlayScenarioPackagedGate::TickAimAtActor(AActor& Target)
{
	const FVector Origin = Controller->PlayerCameraManager != nullptr
		? Controller->PlayerCameraManager->GetCameraLocation()
		: Character->GetPawnViewLocation();
	const FRotator Desired = (Target.GetActorLocation() - Origin).Rotation();
	const FRotator Current = Controller->GetControlRotation();
	const double YawError = FMath::FindDeltaAngleDegrees(Current.Yaw, Desired.Yaw);
	const double PitchError = FMath::FindDeltaAngleDegrees(Current.Pitch, Desired.Pitch);
	SendLookInput(YawError, PitchError);
	AActor* FocusedActor = nullptr;
	for (UActorComponent* Component : Character->GetComponents())
	{
		if (Cast<IInteractionComponentInterface>(Component) != nullptr)
		{
			FocusedActor = IInteractionComponentInterface::Execute_GetFocusedActor(Component);
			break;
		}
	}
	if (FocusedActor != &Target)
	{
		HoldKey(EKeys::LeftControl);
		return false;
	}
	ReleaseKey(EKeys::LeftControl);
	ReleaseKey(EKeys::W);
	return FMath::Abs(YawError) < 2.0 && FMath::Abs(PitchError) < 2.0;
}

void FSinglePlayScenarioPackagedGate::HoldKey(const FKey& Key)
{
	if (!Controller.IsValid()) { return; }
	const EInputEvent Event = HeldKeys.Contains(Key) ? IE_Repeat : IE_Pressed;
	static_cast<APlayerController*>(Controller.Get())->InputKey(
		FInputKeyEventArgs::CreateSimulated(Key, Event, 1.0f));
	HeldKeys.Add(Key);
	++InputEventCount;
}

void FSinglePlayScenarioPackagedGate::ReleaseKey(const FKey& Key)
{
	if (!HeldKeys.Remove(Key)) { return; }
	if (Controller.IsValid())
	{
		static_cast<APlayerController*>(Controller.Get())->InputKey(
			FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
		++InputEventCount;
	}
}

void FSinglePlayScenarioPackagedGate::ReleaseInputs()
{
	for (const FKey& Key : HeldKeys.Array()) { ReleaseKey(Key); }
}

void FSinglePlayScenarioPackagedGate::SendLookInput(double YawError, double PitchError)
{
	if (!Controller.IsValid()) { return; }
	static_cast<APlayerController*>(Controller.Get())->InputKey(FInputKeyEventArgs::CreateSimulated(
		EKeys::MouseX, IE_Axis, FMath::Clamp(YawError * 0.2, -10.0, 10.0)));
	static_cast<APlayerController*>(Controller.Get())->InputKey(FInputKeyEventArgs::CreateSimulated(
		EKeys::MouseY, IE_Axis, FMath::Clamp(PitchError * 0.2, -10.0, 10.0)));
	InputEventCount += 2;
}

void FSinglePlayScenarioPackagedGate::SendKeyPress(const FKey& Key)
{
	if (!Controller.IsValid()) { return; }
	static_cast<APlayerController*>(Controller.Get())->InputKey(
		FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
	HeldKeys.Add(Key);
	++InputEventCount;
}

UInventoryViewModel* FSinglePlayScenarioPackagedGate::FindInventoryViewModel() const
{
	for (TObjectIterator<UInventoryViewModel> It; It; ++It)
	{
		if (It->GetWorld() == World.Get() && It->GetbPanelVisible())
		{
			return *It;
		}
	}
	return nullptr;
}

bool FSinglePlayScenarioPackagedGate::BeginItemDrag(
	FName ItemName,
	const FGameplayTag& TargetSurface,
	int32 TargetCell,
	FString& OutError)
{
	UInventoryViewModel* ViewModel = FindInventoryViewModel();
	if (ViewModel == nullptr)
	{
		OutError = TEXT("The live inventory ViewModel is unavailable.");
		return false;
	}
	const FInventoryEntryView* Entry = ViewModel->GetCachedNearbyEntriesForDiagnostics().FindByPredicate(
		[ItemName](const FInventoryEntryView& Candidate)
		{
			return Candidate.ItemId.PrimaryAssetName == ItemName;
		});
	if (Entry == nullptr)
	{
		OutError = FString::Printf(TEXT("Nearby cache item '%s' is unavailable."), *ItemName.ToString());
		return false;
	}
	const int32 SourceCell = Entry->GridPos.Y * ViewModel->GetSecondaryGridWidth() + Entry->GridPos.X;
	UW_InventoryCellDropTarget* Source = nullptr;
	UW_InventoryCellDropTarget* Target = nullptr;
	for (TObjectIterator<UW_InventoryCellDropTarget> It; It; ++It)
	{
		if (!Controller.IsValid() || !IsScenarioUsableWidget(**It, *Controller.Get())) { continue; }
		if (It->GetSurfaceTag() == ProjectTags::Item_Container_WorldStorage && It->GetCellIndex() == SourceCell)
		{
			Source = *It;
		}
		if (It->GetSurfaceTag() == TargetSurface && It->GetCellIndex() == TargetCell)
		{
			Target = *It;
		}
	}
	if (Source == nullptr || Source->GetHostedCell() == nullptr || Target == nullptr)
	{
		OutError = FString::Printf(
			TEXT("Real UI drag endpoints are unavailable for '%s' (%s:%d)."),
			*ItemName.ToString(), *TargetSurface.ToString(), TargetCell);
		return false;
	}
	DragStep = MakeUnique<FDragStep>();
	DragStep->Source = Source->GetHostedCell();
	DragStep->Target = Target;
	DragStep->SourcePosition = WidgetCenter(*Source->GetHostedCell());
	DragStep->TargetPosition = WidgetCenter(*Target);
	return true;
}

bool FSinglePlayScenarioPackagedGate::BeginEquipDrag(
	FName ItemName,
	const FGameplayTag& EquipSlot,
	FString& OutError)
{
	UInventoryViewModel* ViewModel = FindInventoryViewModel();
	if (ViewModel == nullptr)
	{
		OutError = TEXT("The live inventory ViewModel is unavailable.");
		return false;
	}
	const FInventoryEntryView* Entry = ViewModel->GetCachedEntriesForDiagnostics().FindByPredicate(
		[ItemName](const FInventoryEntryView& Candidate)
		{
			return Candidate.ItemId.PrimaryAssetName == ItemName;
		});
	if (Entry == nullptr)
	{
		OutError = FString::Printf(TEXT("Player inventory item '%s' is unavailable."), *ItemName.ToString());
		return false;
	}
	const int32 SourceCell = Entry->GridPos.Y * UInventoryViewModel::HandGridSize + Entry->GridPos.X;
	UW_InventoryCellDropTarget* Source = nullptr;
	UW_InventoryEquipSlotDropTarget* Target = nullptr;
	for (TObjectIterator<UW_InventoryCellDropTarget> It; It; ++It)
	{
		if (Controller.IsValid() && IsScenarioUsableWidget(**It, *Controller.Get()) &&
			It->GetSurfaceTag() == Entry->ContainerId &&
			It->GetCellIndex() == SourceCell)
		{
			Source = *It;
			break;
		}
	}
	for (TObjectIterator<UW_InventoryEquipSlotDropTarget> It; It; ++It)
	{
		if (Controller.IsValid() && IsScenarioUsableWidget(**It, *Controller.Get()) && It->GetSlotTag() == EquipSlot)
		{
			Target = *It;
			break;
		}
	}
	if (Source == nullptr || Source->GetHostedCell() == nullptr || Target == nullptr)
	{
		OutError = FString::Printf(TEXT("Real UI equip endpoints are unavailable for '%s'."), *ItemName.ToString());
		return false;
	}
	DragStep = MakeUnique<FDragStep>();
	DragStep->Source = Source->GetHostedCell();
	DragStep->Target = Target;
	DragStep->SourcePosition = WidgetCenter(*Source->GetHostedCell());
	DragStep->TargetPosition = WidgetCenter(*Target);
	return true;
}

bool FSinglePlayScenarioPackagedGate::TickDrag()
{
	if (!DragStep || !DragStep->Source.IsValid() || !DragStep->Target.IsValid())
	{
		DragStep.Reset();
		return false;
	}
	bool bRouted = false;
	switch (DragStep->Step++)
	{
	case 0:
		bRouted = FSinglePlayScenarioSlateInput::RoutePointerDown(*DragStep->Source, DragStep->SourcePosition);
		break;
	case 1:
	{
		const FVector2D DetectPosition = DragStep->SourcePosition + FVector2D(24.0, 0.0);
		bRouted = FSinglePlayScenarioSlateInput::RoutePointerMove(
			*DragStep->Source, DetectPosition, DragStep->SourcePosition);
		break;
	}
	case 2:
		bRouted = FSinglePlayScenarioSlateInput::RoutePointerMove(
			*DragStep->Target, DragStep->TargetPosition, DragStep->SourcePosition);
		break;
	default:
		bRouted = FSinglePlayScenarioSlateInput::RoutePointerUp(*DragStep->Target, DragStep->TargetPosition);
		DragStep.Reset();
		UiPointerEventCount += bRouted ? 1 : 0;
		return true;
	}
	UiPointerEventCount += bRouted ? 1 : 0;
	return false;
}

bool FSinglePlayScenarioPackagedGate::ClickWidget(UWidget& Widget)
{
	if (!Controller.IsValid() || !Widget.IsVisible() || !Widget.GetIsEnabled() ||
		Widget.GetCachedGeometry().GetLocalSize().GetMin() <= 1.0f)
	{
		return false;
	}
	const FVector2D Position = WidgetCenter(Widget);
	const bool bPressed = FSinglePlayScenarioSlateInput::RoutePointerDown(Widget, Position);
	const bool bReleased = FSinglePlayScenarioSlateInput::RoutePointerUp(Widget, Position);
	UiPointerEventCount += static_cast<int32>(bPressed) + static_cast<int32>(bReleased);
	return bPressed && bReleased;
}

void FSinglePlayScenarioPackagedGate::HandleInventoryError(const FText& ErrorMessage)
{
	CapacityRejection = ErrorMessage.ToString();
	bCapacityRejected = CapacityRejection.Equals(
		TEXT("Target inventory container would exceed its weight limit."),
		ESearchCase::CaseSensitive);
	UE_LOG(LogProjectSinglePlay, Display,
		TEXT("[FSinglePlayScenarioPackagedGate::HandleInventoryError] Observed - message=%s expected=%d"),
		*CapacityRejection, bCapacityRejected ? 1 : 0);
}

void FSinglePlayScenarioPackagedGate::HandleInteraction(
	AActor* TargetActor,
	UActorComponent* RespondingComponent)
{
	(void)RespondingComponent;
	if (TargetActor == CacheActor.Get())
	{
		bInteractionObserved = true;
	}
}

bool FSinglePlayScenarioPackagedGate::HasItem(FName ItemName) const
{
	if (!Character.IsValid()) { return false; }
	const FPrimaryAssetId ItemId(FPrimaryAssetType(TEXT("ObjectDefinition")), ItemName);
	for (UActorComponent* Component : Character->GetComponents())
	{
		if (const IInventoryReadOnly* Inventory = Cast<IInventoryReadOnly>(Component))
		{
			return Inventory->ContainsItem(ItemId);
		}
	}
	return false;
}

bool FSinglePlayScenarioPackagedGate::ReadHydration(double& OutFraction) const
{
	if (!Character.IsValid()) { return false; }
	for (UActorComponent* Component : Character->GetComponents())
	{
		if (const IVitalsReadOnly* Vitals = Cast<IVitalsReadOnly>(Component))
		{
			FVitalsReadOnlySnapshot Snapshot;
			if (Vitals->GetVitalsSnapshot(Snapshot))
			{
				OutFraction = Snapshot.GetHydrationFraction();
				return true;
			}
		}
	}
	return false;
}

AActor* FSinglePlayScenarioPackagedGate::FindTaggedActor(FName ActorTag) const
{
	if (!World.IsValid()) { return nullptr; }
	for (TActorIterator<AActor> It(World.Get()); It; ++It)
	{
		if (It->ActorHasTag(ActorTag)) { return *It; }
	}
	return nullptr;
}

bool FSinglePlayScenarioPackagedGate::RequestScreenshot(FString& OutError)
{
	if (!World.IsValid() || !Controller.IsValid() || ScreenshotPath.IsEmpty())
	{
		OutError = TEXT("Screenshot ownership is unavailable.");
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
	IFileManager::Get().Delete(*ScreenshotPath, false, true, true);
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	bScreenshotRequested = true;
	return true;
}

void FSinglePlayScenarioPackagedGate::FinishAccepted()
{
	ReleaseInputs();
	const bool bAccepted = bScreenshotRequested && FPaths::FileExists(ScreenshotPath) &&
		(Route == ERoute::Success
			? bInteractionObserved && bInventoryOpened && bPouchEquipped && bCapacityRejected &&
				bWaterUsed && HydrationAfter > 0.20 && HasItem(TEXT("EmergencyRation")) &&
				Runner.IsValid() && Runner->GetPhase() == ESinglePlayScenarioPhase::Succeeded
			: bFailureObserved && bRestartObserved);
	if (!bAccepted)
	{
		FinishRejected(TEXT("scenario_evidence_incomplete"), TEXT("The packaged scenario route lacks one or more required observations."));
		return;
	}
	WriteResult(TEXT("accepted"), FString(), FString());
	SetPhase(EPhase::Finished);
	UE_LOG(LogProjectSinglePlay, Display,
		TEXT("[FSinglePlayScenarioPackagedGate::FinishAccepted] Accepted - route=%s"),
		Route == ERoute::Success ? TEXT("success") : TEXT("failure"));
	if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldProductPerformanceGate")))
	{
		RequestExit(0, TEXT("ProjectSinglePlayScenarioPackagedGate.Accepted"));
	}
}

void FSinglePlayScenarioPackagedGate::FinishRejected(const FString& Code, const FString& Message)
{
	ReleaseInputs();
	WriteResult(TEXT("rejected"), Code, Message);
	SetPhase(EPhase::Finished);
	UE_LOG(LogProjectSinglePlay, Error,
		TEXT("[FSinglePlayScenarioPackagedGate::FinishRejected] Rejected - code=%s message=%s"),
		*Code, *Message);
	RequestExit(11, TEXT("ProjectSinglePlayScenarioPackagedGate.Rejected"));
}

void FSinglePlayScenarioPackagedGate::WriteResult(
	const FString& Status,
	const FString& ErrorCode,
	const FString& ErrorMessage)
{
	if (ResultPath.IsEmpty()) { return; }
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"), TEXT("https://alis.world/schemas/single-play-scenario/scenario-result-v1.json"));
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("correctness_contract"), TEXT("single-play-scenario-v1"));
	Root->SetStringField(TEXT("operation_id"), OperationId);
	Root->SetStringField(TEXT("status"), Status);
	Root->SetStringField(TEXT("route"), Route == ERoute::Success ? TEXT("success") : TEXT("failure"));
	Root->SetStringField(TEXT("runtime_profile"), RuntimeProfileId);
	Root->SetStringField(TEXT("runtime_profile_sha256"), RuntimeProfileHash);
	Root->SetStringField(TEXT("machine_profile_id"), MachineProfileId);
	Root->SetStringField(TEXT("map_package"), World.IsValid() ? World->GetPackage()->GetName() : FString());
	Root->SetStringField(TEXT("executable"), FPlatformProcess::ExecutablePath());
	Root->SetStringField(TEXT("build_configuration"), LexToString(FApp::GetBuildConfiguration()));
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("gpu_adapter"), GRHIAdapterName);
	const FGPUDriverInfo Driver = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
	Root->SetStringField(TEXT("gpu_driver"), Driver.UserDriverVersion);
	Root->SetStringField(TEXT("rhi"), GDynamicRHI == nullptr ? TEXT("unavailable") : GDynamicRHI->GetName());
	Root->SetStringField(TEXT("game_mode"), TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode"));
	Root->SetStringField(TEXT("pawn_class"), Character.IsValid() ? Character->GetClass()->GetPathName() : FString());
	Root->SetStringField(TEXT("input_method"), TEXT("APlayerController::InputKey + Slate pointer events"));
	Root->SetNumberField(TEXT("input_event_count"), InputEventCount);
	Root->SetNumberField(TEXT("ui_pointer_event_count"), UiPointerEventCount);
	Root->SetBoolField(TEXT("project_loading_provenance"), World.IsValid() && World->URL.GetOption(TEXT("ProjectLoadingRoute="), TEXT("")) == FString(TEXT("1")));
	Root->SetBoolField(TEXT("possessed_player"), Controller.IsValid() && Controller->GetPawn() != nullptr);
	Root->SetBoolField(TEXT("gameplay_interaction"), bInteractionObserved);
	Root->SetBoolField(TEXT("inventory_opened"), bInventoryOpened);
	Root->SetBoolField(TEXT("pouch_equipped"), bPouchEquipped);
	Root->SetBoolField(TEXT("capacity_rejected"), bCapacityRejected);
	Root->SetStringField(TEXT("capacity_rejection"), CapacityRejection);
	Root->SetBoolField(TEXT("water_used"), bWaterUsed);
	Root->SetNumberField(TEXT("hydration_before"), HydrationBefore);
	Root->SetNumberField(TEXT("hydration_after"), HydrationAfter);
	Root->SetBoolField(TEXT("ration_carried"), HasItem(TEXT("EmergencyRation")));
	Root->SetBoolField(TEXT("failure_observed"), bFailureObserved);
	Root->SetBoolField(TEXT("restart_observed"), bRestartObserved);
	Root->SetStringField(TEXT("cache_anchor"), CacheActor.IsValid() ? CacheActor->GetPathName() : FString());
	Root->SetStringField(TEXT("shelter_anchor"), ShelterActor.IsValid() ? ShelterActor->GetPathName() : FString());
	Root->SetStringField(TEXT("screenshot"), ScreenshotPath);
	Root->SetBoolField(TEXT("terrain_collision"), false);
	Root->SetBoolField(TEXT("road_collision"), false);
	Root->SetBoolField(TEXT("building_collision"), false);
	Root->SetStringField(TEXT("error_code"), ErrorCode);
	Root->SetStringField(TEXT("error_message"), ErrorMessage);
	FString Payload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	if (!FJsonSerializer::Serialize(Root, Writer)) { return; }
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true);
	const FString Staging = ResultPath + TEXT(".tmp");
	if (FFileHelper::SaveStringToFile(Payload + TEXT("\n"), *Staging, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		IFileManager::Get().Move(*ResultPath, *Staging, true, true);
	}
}

void FSinglePlayScenarioPackagedGate::SetPhase(EPhase NewPhase)
{
	UE_LOG(LogProjectSinglePlay, Display, TEXT("[FSinglePlayScenarioPackagedGate::SetPhase] Changed - from=%d to=%d"), static_cast<int32>(Phase), static_cast<int32>(NewPhase));
	Phase = NewPhase;
	PhaseStartedSeconds = FPlatformTime::Seconds();
}

void FSinglePlayScenarioPackagedGate::RequestExit(int32 Status, const TCHAR* Reason) const
{
	FPlatformMisc::RequestExitWithStatus(false, Status, Reason);
}
