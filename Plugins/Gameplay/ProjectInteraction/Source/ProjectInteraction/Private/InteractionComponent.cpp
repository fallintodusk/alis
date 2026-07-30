// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "InteractionComponent.h"
#include "InteractionCapabilitySelector.h"
#include "InteractionTargetResolver.h"
#include "InteractionService.h"
#include "Interfaces/IInteractionService.h"
#include "Interfaces/IInteractableTarget.h"
#include "ProjectServiceLocator.h"
#include "Camera/CameraComponent.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"

namespace
{
	static TAutoConsoleVariable<int32> CVarInteractionDraw(
		TEXT("alis.Interaction.Draw"),
		0,
		TEXT("Draw interaction targeting debug shapes: 0 off, 1 sphere/winner, 2 include rejected rays."),
		ECVF_Default);

	bool HasHoldTargetChanged(
		const TWeakObjectPtr<AActor>& HoldActor,
		const TWeakObjectPtr<UPrimitiveComponent>& HoldComponent,
		const TWeakObjectPtr<AActor>& CurrentActor,
		const TWeakObjectPtr<UPrimitiveComponent>& CurrentComponent)
	{
		if (!CurrentActor.IsValid() || HoldActor.Get() != CurrentActor.Get())
		{
			return true;
		}

		if (HoldComponent.IsValid() && HoldComponent.Get() != CurrentComponent.Get())
		{
			return true;
		}

		return false;
	}

}

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	OutlineMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/ProjectMaterial/Effect/MI_Outline.MI_Outline"))
	);

	// bDrawDebug = false; // Enable for debug trace visualization

	UE_LOG(
		LogInteraction,
		Verbose,
		TEXT("[%s] Constructor: InteractionRadius=%.0f, MinAimDot=%.2f, FocusSwitchHysteresis=%.2f"),
		*GetName(),
		InteractionRadius,
		MinAimDot,
		FocusSwitchHysteresis);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NULL");
	UE_LOG(
		LogInteraction,
		Log,
		TEXT("[%s] BeginPlay: Owner=%s, InteractionRadius=%.0f, MinAimDot=%.2f, FocusSwitchHysteresis=%.2f, Channel=%d, TraceInterval=%.2fs"),
		*GetName(),
		*OwnerName,
		InteractionRadius,
		MinAimDot,
		FocusSwitchHysteresis,
		(int32)TraceChannel.GetValue(),
		TraceIntervalSeconds);

	if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		PawnOwner->ReceiveRestartedDelegate.AddUniqueDynamic(this, &UInteractionComponent::HandlePawnRestarted);
	}

	ActivateLocalPresentationIfNeeded();
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		PawnOwner->ReceiveRestartedDelegate.RemoveDynamic(this, &UInteractionComponent::HandlePawnRestarted);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PassiveTraceTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UInteractionComponent::HandlePawnRestarted(APawn* Pawn)
{
	if (Pawn == GetOwner())
	{
		ActivateLocalPresentationIfNeeded();
	}
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bHoldInteractionActive)
	{
		const bool bFocusChanged = HasHoldTargetChanged(HoldTargetActor, HoldTargetComponent, FocusedActor, FocusedComponent);

		if (bFocusChanged || !FocusedExecutionSpec.RequiresHold())
		{
			CancelHoldInteraction();
		}
		else if (UWorld* World = GetWorld())
		{
			const float HoldDuration = FMath::Max(FocusedExecutionSpec.DurationSeconds, KINDA_SMALL_NUMBER);
			const float NewProgress = FMath::Clamp(
				(World->GetTimeSeconds() - HoldInteractionStartTime) / HoldDuration,
				0.0f,
				1.0f);

			if (!FMath::IsNearlyEqual(NewProgress, HoldInteractionProgress))
			{
				HoldInteractionProgress = NewProgress;
				BroadcastPromptStateToService();
			}

			if (HoldInteractionProgress >= 1.0f - KINDA_SMALL_NUMBER)
			{
				CompleteHoldInteraction();
			}
		}
	}

	// Dynamic-camera pawns (e.g. ADefinitionCharacter) create their camera
	// AFTER BeginPlay via ApplyViewConfig, so the one-shot SetupPostProcess
	// from BeginPlay can miss it. Retry once per tick until the camera is
	// available, then bPostProcessReady gates this out permanently.
	if (bEnableHighlight && !bPostProcessReady)
	{
		if (AActor* Owner = GetOwner())
		{
			if (Owner->FindComponentByClass<UCameraComponent>())
			{
				SetupPostProcess();
			}
		}
	}

	RefreshComponentTickEnabled();
}

void UInteractionComponent::StartPassiveTraceTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float IntervalSeconds = FMath::Max(TraceIntervalSeconds, 0.05f);
	World->GetTimerManager().SetTimer(
		PassiveTraceTimerHandle,
		this,
		&UInteractionComponent::UpdateTrace,
		IntervalSeconds,
		true,
		0.0f);
}

void UInteractionComponent::ActivateLocalPresentationIfNeeded()
{
	if (!ShouldRunPassiveFocus())
	{
		return;
	}

	if (bEnableHighlight && !bPostProcessReady)
	{
		SetupPostProcess();
	}

	UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(PassiveTraceTimerHandle))
	{
		StartPassiveTraceTimer();
	}

	RefreshComponentTickEnabled();
}

void UInteractionComponent::RefreshComponentTickEnabled()
{
	const bool bNeedsPostProcessRetry = ShouldRunPassiveFocus() && bEnableHighlight && !bPostProcessReady;
	const bool bNeedsTick = bHoldInteractionActive || bNeedsPostProcessRetry;
	SetComponentTickEnabled(bNeedsTick);
}

bool UInteractionComponent::ShouldRunPassiveFocus() const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	return PawnOwner && PawnOwner->IsLocallyControlled();
}

void UInteractionComponent::UpdateTrace()
{
	if (!ShouldRunPassiveFocus())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn)
	{
		return;
	}

	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewForward = FVector::ZeroVector;
	if (!FInteractionTargetResolver::ResolvePlayerCameraView(Pawn, ViewOrigin, ViewForward))
	{
		return;
	}

	UpdateFocusFromView(ViewOrigin, ViewForward);
}

bool UInteractionComponent::UpdateFocusFromView(const FVector& ViewOrigin, const FVector& ViewForward)
{
	const FInteractionTargetingWeights Weights = FInteractionTargetResolver::BuildWeights(*this);
	const int32 DrawMode = CVarInteractionDraw.GetValueOnGameThread();
	FInteractionTargetDebugSnapshot Debug;
	FInteractionTargetDebugSnapshot* DebugPtr = (DrawMode != 0) ? &Debug : nullptr;
	FInteractionTargetCandidate Winner;
	FInteractionTargetCandidate CurrentFocusCandidate;
	const bool bFoundWinner = FInteractionTargetResolver(
		GetWorld(),
		ViewOrigin,
		ViewForward,
		TraceChannel,
		GetOwner(),
		Weights)
		.Resolve(Winner, DebugPtr, FocusedActor.Get(), &CurrentFocusCandidate);

	FInteractionTargetCandidate FinalWinner = Winner;
	if (bFoundWinner && FInteractionTargetResolver::ShouldKeepCurrentCandidate(CurrentFocusCandidate, Winner, FocusSwitchHysteresis, MinAimDot))
	{
		FinalWinner = CurrentFocusCandidate;
	}

	TraceStart = ViewOrigin;
	TraceEnd = bFoundWinner ? FinalWinner.TargetPoint : (ViewOrigin + (ViewForward * Weights.InteractionRadius));

	if (DrawMode != 0)
	{
		FInteractionTargetResolver::DrawDebug(
			GetWorld(),
			Debug,
			bFoundWinner ? &FinalWinner : nullptr,
			FInteractionTargetResolver::GetDebugLifetime(*this),
			DrawMode > 1);
	}

	SetFocusedActor(bFoundWinner ? FinalWinner.Actor : nullptr, bFoundWinner ? FinalWinner.Component : nullptr);
	return bFoundWinner;
}

// Default fallback label
static const FText DefaultInteractionLabel = NSLOCTEXT("Interaction", "Interact", "Interact");

void UInteractionComponent::SetFocusedActor(AActor* NewFocus, UPrimitiveComponent* HitComponent)
{
	// Query focus info from actor interface first, then fallback to component routing.
	FInteractionFocusInfo FocusInfo;
	FInteractionExecutionSpec NewExecutionSpec;
	bool bResolvedExecutionSpecFromActor = false;
	if (NewFocus && NewFocus->Implements<UInteractableTargetInterface>())
	{
		FocusInfo = IInteractableTargetInterface::Execute_GetFocusInfo(NewFocus, HitComponent);
		if (FocusInfo.IsValid())
		{
			NewExecutionSpec = IInteractableTargetInterface::Execute_GetInteractionExecutionSpec(NewFocus, GetOwner(), HitComponent);
			bResolvedExecutionSpecFromActor = true;
		}
	}
	if (NewFocus && !FocusInfo.IsValid())
	{
		FInteractionCapabilitySelector::ResolveFocus(NewFocus, HitComponent, FocusInfo);
	}
	if (NewFocus && !bResolvedExecutionSpecFromActor)
	{
		FInteractionCapabilitySelector::ResolveExecutionSpec(NewFocus, HitComponent, GetOwner(), NewExecutionSpec);
	}

	// Not interactable if no valid focus info
	if (!FocusInfo.IsValid())
	{
		NewFocus = nullptr;
		HitComponent = nullptr;
		NewExecutionSpec = FInteractionExecutionSpec();
	}

	// Get highlight mesh and label from focus info
	UPrimitiveComponent* HighlightComponent = FocusInfo.HighlightMesh ? FocusInfo.HighlightMesh.Get() : HitComponent;
	FText NewLabel = FocusInfo.Label.IsEmpty() ? DefaultInteractionLabel : FocusInfo.Label;
	const bool bExecutionSpecChanged =
		!FMath::IsNearlyEqual(FocusedExecutionSpec.DurationSeconds, NewExecutionSpec.DurationSeconds)
		|| !FocusedExecutionSpec.ActiveLabel.EqualTo(NewExecutionSpec.ActiveLabel)
		|| FocusedExecutionSpec.bCancelOnRelease != NewExecutionSpec.bCancelOnRelease;

	if (bHoldInteractionActive
		&& HasHoldTargetChanged(
			HoldTargetActor,
			HoldTargetComponent,
			TWeakObjectPtr<AActor>(NewFocus),
			TWeakObjectPtr<UPrimitiveComponent>(HighlightComponent)))
	{
		CancelHoldInteraction();
	}

	// Check if anything changed
	const bool bSameFocus =
		(FocusedActor.Get() == NewFocus) &&
		(FocusedComponent.Get() == HighlightComponent);

	if (bSameFocus)
	{
		FocusedExecutionSpec = NewExecutionSpec;
		// Still same focus - check if label changed (Open -> Close)
		if (!NewLabel.EqualTo(FocusedLabel) || bExecutionSpecChanged)
		{
			FocusedLabel = NewLabel;
			BroadcastFocusChangedToService();
			UE_LOG(LogInteraction, Log, TEXT("[%s] SetFocusedActor: Label changed to '%s'"),
				*GetName(), *FocusedLabel.ToString());
		}
		return;
	}

	// Focus changed - unhighlight old, highlight new
	UPrimitiveComponent* OldComponent = FocusedComponent.Get();
	if (OldComponent)
	{
		SetComponentCustomDepth(OldComponent, false);
		UE_LOG(LogInteraction, Log, TEXT("[%s] SetFocusedActor: Unfocused component '%s' on '%s'"),
			*GetName(), *OldComponent->GetName(),
			FocusedActor.IsValid() ? *FocusedActor->GetActorNameOrLabel() : TEXT("NULL"));
	}

	FocusedActor = NewFocus;
	FocusedComponent = HighlightComponent;
	FocusedLabel = NewLabel;
	FocusedExecutionSpec = NewExecutionSpec;

	if (HighlightComponent && NewFocus)
	{
		SetComponentCustomDepth(HighlightComponent, true);
		UE_LOG(LogInteraction, Log, TEXT("[%s] SetFocusedActor: Focused component '%s' on '%s' (Class=%s) Label='%s' (Hit='%s')"),
			*GetName(), *HighlightComponent->GetName(), *NewFocus->GetActorNameOrLabel(),
			*NewFocus->GetClass()->GetName(), *FocusedLabel.ToString(),
			HitComponent ? *HitComponent->GetName() : TEXT("NULL"));
	}
	else if (OldComponent)
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] SetFocusedActor: Cleared focus"), *GetName());
	}

	// Broadcast focus change through service (SOLID: decoupled from HUD)
	BroadcastFocusChangedToService();
}

#if WITH_DEV_AUTOMATION_TESTS
bool UInteractionComponent::TestOnly_ExecuteInteraction(AActor* Target, UPrimitiveComponent* HitComponent, AActor* OverrideInstigator)
{
	AActor* Instigator = OverrideInstigator ? OverrideInstigator : GetOwner();
	if (!Target || !Instigator)
	{
		return false;
	}

	if (Target->Implements<UInteractableTargetInterface>())
	{
		return IInteractableTargetInterface::Execute_OnInteract(Target, Instigator, HitComponent);
	}

	return FInteractionCapabilitySelector::ExecuteInteraction(Target, Instigator, HitComponent);
}

bool UInteractionComponent::TestOnly_ResolveBestInteractionTarget(
	const FVector& ViewOrigin,
	const FVector& ViewForward,
	AActor*& OutActor,
	UPrimitiveComponent*& OutHitComponent) const
{
	OutActor = nullptr;
	OutHitComponent = nullptr;

	FInteractionTargetCandidate Winner;
	const bool bFoundWinner = FInteractionTargetResolver(
		GetWorld(),
		ViewOrigin,
		ViewForward,
		TraceChannel,
		GetOwner(),
		FInteractionTargetResolver::BuildWeights(*this))
		.Resolve(Winner);

	if (bFoundWinner)
	{
		OutActor = Winner.Actor;
		OutHitComponent = Winner.Component;
	}

	return bFoundWinner;
}

bool UInteractionComponent::TestOnly_UpdateFocusFromView(const FVector& ViewOrigin, const FVector& ViewForward)
{
	return UpdateFocusFromView(ViewOrigin, ViewForward);
}
#endif

void UInteractionComponent::BroadcastFocusChangedToService()
{
	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (!Service.IsValid())
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	Service->BroadcastFocusChanged(OwnerPawn, FocusedActor.Get(), FocusedComponent.Get(), FocusedLabel);
	Service->BroadcastPromptState(OwnerPawn, BuildPromptState());
}

void UInteractionComponent::DrawInteractionDebugTraceOnInput()
{
#if WITH_EDITOR
	if (!bDrawDebug)
	{
		return;
	}

	AActor* Owner = GetOwner();
	APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn)
	{
		UE_LOG(LogInteraction, Verbose, TEXT("[%s] DrawInteractionDebugTraceOnInput: Skipped - owner is not a pawn"), *GetName());
		return;
	}

	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewForward = FVector::ZeroVector;
	if (!FInteractionTargetResolver::ResolvePlayerCameraView(Pawn, ViewOrigin, ViewForward))
	{
		UE_LOG(LogInteraction, Verbose, TEXT("[%s] DrawInteractionDebugTraceOnInput: Skipped - no PlayerController or CameraManager"), *GetName());
		return;
	}

	FInteractionTargetDebugSnapshot Debug;
	FInteractionTargetCandidate Winner;
	FInteractionTargetCandidate CurrentFocusCandidate;
	const bool bFoundWinner = FInteractionTargetResolver(
		GetWorld(),
		ViewOrigin,
		ViewForward,
		TraceChannel,
		Owner,
		FInteractionTargetResolver::BuildWeights(*this))
		.Resolve(Winner, &Debug, FocusedActor.Get(), &CurrentFocusCandidate);

	FInteractionTargetCandidate FinalWinner = Winner;
	if (bFoundWinner && FInteractionTargetResolver::ShouldKeepCurrentCandidate(CurrentFocusCandidate, Winner, FocusSwitchHysteresis, MinAimDot))
	{
		FinalWinner = CurrentFocusCandidate;
	}

	FInteractionTargetResolver::DrawDebug(
		GetWorld(),
		Debug,
		bFoundWinner ? &FinalWinner : nullptr,
		5.0f,
		CVarInteractionDraw.GetValueOnGameThread() > 1);

	UE_LOG(
		LogInteraction,
		Log,
		TEXT("[%s] DrawInteractionDebugTraceOnInput: Drawn %s resolver debug for '%s'"),
		*GetName(),
		bFoundWinner ? TEXT("winner") : TEXT("miss"),
		bFoundWinner && FinalWinner.Actor ? *FinalWinner.Actor->GetActorNameOrLabel() : TEXT("None"));
#endif
}

bool UInteractionComponent::TryInteract_Implementation()
{
	UE_LOG(LogInteraction, Verbose, TEXT("[%s] TryInteract: Called"), *GetName());

	// The client's focused (Actor, Component) is the single source of truth -
	// highlight and interaction must converge on the same primitive. The server
	// will validate the target on its side but NOT re-resolve from a different
	// view (which is what produced the dresser regression: client highlighted
	// drawer slot A, server resolved a sibling slot from its eye view).
	return DispatchInteract(FocusedActor.Get(), FocusedComponent.Get());
}

bool UInteractionComponent::TryInteractWithActor_Implementation(AActor* Target)
{
	// Bypass focus / highlight. Used by Sequencer-driven cinematic replay
	// to interact with the exact actor recorded during PIE. Server still
	// validates plausibility (interactable + range); no re-resolve.
	if (!Target)
	{
		UE_LOG(LogInteraction, Warning,
			TEXT("[%s] TryInteractWithActor: null target"),
			*GetName());
		return false;
	}
	return DispatchInteract(Target, nullptr);
}

bool UInteractionComponent::DispatchInteract(AActor* Target, UPrimitiveComponent* HitComponent)
{
	if (!Target)
	{
		UE_LOG(LogInteraction, Verbose, TEXT("[%s] DispatchInteract: No focused target"), *GetName());
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogInteraction, Warning, TEXT("[%s] DispatchInteract: FAILED - No owner"), *GetName());
		return false;
	}

	if (Owner->HasAuthority())
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] DispatchInteract: Has authority, executing locally on '%s'"),
			*GetName(), *Target->GetActorNameOrLabel());
		ExecuteInteraction_ServerAuth(Target, HitComponent);
	}
	else
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] DispatchInteract: No authority, sending Server RPC for '%s'"),
			*GetName(), *Target->GetActorNameOrLabel());
		Server_TryInteract(Target, HitComponent);
	}

	return true;
}

bool UInteractionComponent::BeginInteractInput_Implementation()
{
	UpdateTrace();
	DrawInteractionDebugTraceOnInput();

	if (!FocusedActor.IsValid())
	{
		return false;
	}

	if (!FocusedExecutionSpec.RequiresHold())
	{
		return TryInteract_Implementation();
	}

	if (bHoldInteractionActive)
	{
		return true;
	}

	UWorld* World = GetWorld();
	HoldInteractionStartTime = World ? World->GetTimeSeconds() : 0.0f;
	HoldInteractionProgress = 0.0f;
	bHoldInteractionActive = true;
	HoldTargetActor = FocusedActor;
	HoldTargetComponent = FocusedComponent;
	RefreshComponentTickEnabled();

	UE_LOG(LogInteraction, Log, TEXT("[%s] BeginInteractInput: Started timed interaction '%s' (Duration=%.2fs)"),
		*GetName(),
		*FocusedLabel.ToString(),
		FocusedExecutionSpec.DurationSeconds);

	BroadcastPromptStateToService();
	return true;
}

void UInteractionComponent::EndInteractInput_Implementation()
{
	if (bHoldInteractionActive && FocusedExecutionSpec.bCancelOnRelease)
	{
		CancelHoldInteraction();
	}
}

FInteractionPromptState UInteractionComponent::GetInteractionPromptState_Implementation() const
{
	return BuildPromptState();
}

void UInteractionComponent::Server_TryInteract_Implementation(AActor* TargetActor, UPrimitiveComponent* TargetComponent)
{
	UE_LOG(LogInteraction, Log, TEXT("[%s] Server_TryInteract: RPC received target='%s'"),
		*GetName(),
		TargetActor ? *TargetActor->GetActorNameOrLabel() : TEXT("NULL"));
	ExecuteInteraction_ServerAuth(TargetActor, TargetComponent);
}

void UInteractionComponent::ExecuteInteraction_ServerAuth(AActor* Target, UPrimitiveComponent* HitComponent)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogInteraction, Warning, TEXT("[%s] ExecuteInteraction_ServerAuth: No owner"), *GetName());
		return;
	}

	APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn)
	{
		UE_LOG(LogInteraction, Warning, TEXT("[%s] ExecuteInteraction_ServerAuth: Owner is not a pawn"), *GetName());
		return;
	}

	// SOT validation: client passed the highlighted target; server confirms it
	// is non-null, interactable, and within a reasonable range from the pawn.
	// The server does NOT re-resolve from a different view - that would split
	// highlight from interaction (dresser regression).
	if (!Target)
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] ExecuteInteraction_ServerAuth: No target supplied"), *GetName());
		return;
	}

	// Anti-cheat range gate. The client picked this target under aim cone + LOS;
	// here we only need a generous bound so a malicious or stale client can't
	// trigger interactions across the map. Use 1.5x InteractionRadius to absorb
	// timing and pawn-vs-actor-origin offsets.
	const float MaxInteractRange = FMath::Max(InteractionRadius, 1.0f) * 1.5f;
	const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation());
	if (DistSq > MaxInteractRange * MaxInteractRange)
	{
		UE_LOG(LogInteraction, Warning,
			TEXT("[%s] ExecuteInteraction_ServerAuth: Target '%s' out of range (%.1f > %.1f cm)"),
			*GetName(),
			*Target->GetActorNameOrLabel(),
			FMath::Sqrt(DistSq),
			MaxInteractRange);
		return;
	}

	// Anti-cheat interactability gate. The Target must either implement the
	// actor-level interface or carry at least one capability the selector
	// recognises - the same gates the client resolver applied.
	bool bIsInteractable = Target->Implements<UInteractableTargetInterface>();
	if (!bIsInteractable)
	{
		TArray<UActorComponent*> InteractableComponents;
		FInteractionCapabilitySelector::GatherComponents(Target, InteractableComponents);
		bIsInteractable = InteractableComponents.Num() > 0;
	}
	if (!bIsInteractable)
	{
		UE_LOG(LogInteraction, Warning,
			TEXT("[%s] ExecuteInteraction_ServerAuth: Target '%s' is not interactable"),
			*GetName(),
			*Target->GetActorNameOrLabel());
		return;
	}

	// Broadcast interaction event (server-side, features subscribe to this)
	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (!Service.IsValid())
	{
		UE_LOG(LogInteraction, Error, TEXT("[%s] ExecuteInteraction_ServerAuth: IInteractionService not registered"),
			*GetName());
		return;
	}

	TSharedPtr<FInteractionService> InteractionService = StaticCastSharedPtr<FInteractionService>(Service);
	if (!InteractionService.IsValid())
	{
		UE_LOG(LogInteraction, Error, TEXT("[%s] ExecuteInteraction_ServerAuth: Could not cast to FInteractionService"),
			*GetName());
		return;
	}

	// Execute interaction through actor interface first, fallback to component routing.
	bool bHandled = false;
	if (Target->Implements<UInteractableTargetInterface>())
	{
		bHandled = IInteractableTargetInterface::Execute_OnInteract(Target, Owner, HitComponent);
	}
	else
	{
		bHandled = FInteractionCapabilitySelector::ExecuteInteraction(Target, Owner, HitComponent);
	}

	if (bHandled)
	{
		// Then broadcast to features (inventory handles pickups/loot, etc.)
		InteractionService->BroadcastInteraction(Target, Owner);
	}
}

void UInteractionComponent::BroadcastPromptStateToService() const
{
	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (!Service.IsValid())
	{
		return;
	}

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		Service->BroadcastPromptState(OwnerPawn, BuildPromptState());
	}
}

FInteractionPromptState UInteractionComponent::BuildPromptState() const
{
	FInteractionPromptState State;
	State.bHasFocus = FocusedActor.IsValid() && !FocusedLabel.IsEmpty();
	if (!State.bHasFocus)
	{
		return State;
	}

	State.bRequiresHold = FocusedExecutionSpec.RequiresHold();
	State.bIsInProgress = bHoldInteractionActive;
	State.Progress = bHoldInteractionActive ? FMath::Clamp(HoldInteractionProgress, 0.0f, 1.0f) : 0.0f;
	State.Label =
		(bHoldInteractionActive && !FocusedExecutionSpec.ActiveLabel.IsEmpty())
			? FocusedExecutionSpec.ActiveLabel
			: FocusedLabel;
	return State;
}

void UInteractionComponent::CancelHoldInteraction()
{
	if (!bHoldInteractionActive)
	{
		return;
	}

	bHoldInteractionActive = false;
	HoldInteractionProgress = 0.0f;
	HoldInteractionStartTime = 0.0f;
	HoldTargetActor.Reset();
	HoldTargetComponent.Reset();
	RefreshComponentTickEnabled();

	UE_LOG(LogInteraction, Verbose, TEXT("[%s] CancelHoldInteraction: Timed interaction cancelled"), *GetName());
	BroadcastPromptStateToService();
}

void UInteractionComponent::CompleteHoldInteraction()
{
	if (!bHoldInteractionActive)
	{
		return;
	}

	// Capture hold target BEFORE clearing - the SOT for this interaction is the
	// (Actor, Component) the player started the hold on, not whatever happens
	// to be focused at the instant the hold completes (focus may have drifted
	// within hysteresis tolerance during the hold).
	AActor* HoldTarget = HoldTargetActor.Get();
	UPrimitiveComponent* HoldComp = HoldTargetComponent.Get();

	bHoldInteractionActive = false;
	HoldInteractionProgress = 0.0f;
	HoldInteractionStartTime = 0.0f;
	HoldTargetActor.Reset();
	HoldTargetComponent.Reset();
	RefreshComponentTickEnabled();
	BroadcastPromptStateToService();

	UE_LOG(LogInteraction, Log, TEXT("[%s] CompleteHoldInteraction: Timed interaction completed"), *GetName());
	DispatchInteract(HoldTarget, HoldComp);
}


void UInteractionComponent::SetupPostProcess()
{
	UE_LOG(LogInteraction, Log, TEXT("[%s] SetupPostProcess: Starting setup, bEnableHighlight=%s"),
		*GetName(), bEnableHighlight ? TEXT("true") : TEXT("false"));

	if (OutlineMaterial.IsNull())
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] SetupPostProcess: No OutlineMaterial configured, highlight disabled"),
			*GetName());
		return;
	}

	// Reuse a previously-loaded material across tick-retries.
	if (!LoadedOutlineMaterial)
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] SetupPostProcess: Loading material from path '%s'"),
			*GetName(), *OutlineMaterial.ToString());

		LoadedOutlineMaterial = OutlineMaterial.LoadSynchronous();
		if (!LoadedOutlineMaterial)
		{
			UE_LOG(LogInteraction, Warning, TEXT("[%s] SetupPostProcess: FAILED to load OutlineMaterial '%s'"),
				*GetName(), *OutlineMaterial.ToString());
			return;
		}

		UE_LOG(LogInteraction, Log, TEXT("[%s] SetupPostProcess: Loaded material '%s' (Class=%s)"),
			*GetName(), *LoadedOutlineMaterial->GetName(), *LoadedOutlineMaterial->GetClass()->GetName());
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogInteraction, Warning, TEXT("[%s] SetupPostProcess: No owner actor"), *GetName());
		return;
	}

	UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
	if (!Camera)
	{
		UE_LOG(LogInteraction, Warning, TEXT("[%s] SetupPostProcess: No CameraComponent found on owner '%s'"),
			*GetName(), *Owner->GetName());
		return;
	}

	UE_LOG(LogInteraction, Log, TEXT("[%s] SetupPostProcess: Found camera '%s' on owner '%s'"),
		*GetName(), *Camera->GetName(), *Owner->GetName());

	CachedCamera = Camera;

	// Ensure camera post-process is enabled
	Camera->PostProcessBlendWeight = 1.0f;

	// Add PP material to camera's post-process settings
	FWeightedBlendable Blendable;
	Blendable.Object = LoadedOutlineMaterial;
	Blendable.Weight = 1.0f;
	Camera->PostProcessSettings.WeightedBlendables.Array.Add(Blendable);

	bPostProcessReady = true;

	UE_LOG(LogInteraction, Log, TEXT("[%s] SetupPostProcess: SUCCESS - PP material added (BlendablesCount=%d)"),
		*GetName(), Camera->PostProcessSettings.WeightedBlendables.Array.Num());
}

void UInteractionComponent::SetComponentCustomDepth(UPrimitiveComponent* Component, bool bEnable)
{
	if (!Component || !bPostProcessReady)
	{
		return;
	}

	Component->SetRenderCustomDepth(bEnable);

	UE_LOG(LogInteraction, Log, TEXT("[%s] SetComponentCustomDepth: Component='%s' Enable=%s"),
		*GetName(), *Component->GetName(), bEnable ? TEXT("true") : TEXT("false"));
}

void UInteractionComponent::SuppressInteractionVisuals()
{
	// IInteractionVisualSuppressor implementation. Called from cinematic code
	// (currently ACinematicGameMode Render mode) via interface query; this
	// gameplay-side component does not know about cinematics -- it only
	// satisfies the contract.
	//
	// 1) Drop focus through the existing path. SetFocusedActor(nullptr, nullptr)
	//    runs the standard unfocus branch which calls SetComponentCustomDepth(
	//    OldComponent, false) on the previously-focused mesh, broadcasts focus
	//    cleared to IInteractionService (which is how the HUD "[E] Open" prompt
	//    learns to hide), and resets FocusedActor/FocusedComponent.
	SetFocusedActor(nullptr, nullptr);

	// 2) Cancel any in-flight hold interaction so the hold timer / progress bar
	//    doesn't keep ticking + broadcasting prompt updates.
	if (bHoldInteractionActive)
	{
		CancelHoldInteraction();
	}

	// 3) Stop the passive trace timer so no new focus is produced. Without this,
	//    the next passive tick would re-resolve a target under the (now hidden)
	//    phantom pawn and re-light its stencil within a few frames.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PassiveTraceTimerHandle);
	}

	// 4) Disable highlight production for the rest of this session. Belt-and-
	//    suspenders: even if some future tick path calls into
	//    SetComponentCustomDepth, the bPostProcessReady guard combined with
	//    bEnableHighlight=false means no new stencil will be lit.
	bEnableHighlight = false;

	// 5) Drop component tick. RefreshComponentTickEnabled() may re-enable it
	//    if a hold interaction is in progress; we just cancelled the hold so
	//    that path is closed.
	SetComponentTickEnabled(false);

	UE_LOG(LogInteraction, Log,
		TEXT("[%s] SuppressInteractionVisuals: focus cleared, highlight disabled, passive trace stopped, tick disabled."),
		*GetName());
}
