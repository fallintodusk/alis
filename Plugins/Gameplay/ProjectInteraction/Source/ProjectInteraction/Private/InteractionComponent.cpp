// Copyright ALIS. All Rights Reserved.

#include "InteractionComponent.h"
#include "InteractionService.h"
#include "Interfaces/IInteractionService.h"
#include "Interfaces/IInteractableTarget.h"
#include "ProjectServiceLocator.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteraction, Log, All);

namespace
{
	void CollectAttachmentHierarchy(USceneComponent* HitNode, TSet<UPrimitiveComponent*>& OutMeshes)
	{
		if (!HitNode)
		{
			return;
		}

		AActor* Owner = HitNode->GetOwner();
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(HitNode))
		{
			OutMeshes.Add(Prim);
		}

		USceneComponent* Parent = HitNode->GetAttachParent();
		while (Parent && Parent->GetOwner() == Owner)
		{
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Parent))
			{
				OutMeshes.Add(Prim);
			}
			Parent = Parent->GetAttachParent();
		}

		TArray<USceneComponent*> Descendants;
		HitNode->GetChildrenComponents(true, Descendants);
		for (USceneComponent* Child : Descendants)
		{
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child))
			{
				OutMeshes.Add(Prim);
			}
		}
	}

	UPrimitiveComponent* GetCapabilityTargetMesh(UActorComponent* Capability)
	{
		if (!Capability || !Capability->Implements<UInteractableComponentTargetInterface>())
		{
			return nullptr;
		}

		return IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(Capability);
	}

	bool CapabilityMatchesHierarchy(
		UActorComponent* Capability,
		const TSet<UPrimitiveComponent*>& HierarchyMeshes,
		bool& OutHasMeshProperty)
	{
		OutHasMeshProperty = false;
		if (!Capability)
		{
			return false;
		}

		if (UPrimitiveComponent* TargetMesh = GetCapabilityTargetMesh(Capability))
		{
			OutHasMeshProperty = true;
			return HierarchyMeshes.Contains(TargetMesh);
		}
		return false;
	}

	void GatherInteractableComponents(AActor* Target, TArray<UActorComponent*>& OutComponents)
	{
		OutComponents.Reset();
		if (!Target)
		{
			return;
		}

		for (UActorComponent* Comp : Target->GetComponents())
		{
			if (Comp && Comp->Implements<UInteractableComponentTargetInterface>())
			{
				OutComponents.Add(Comp);
			}
		}

		OutComponents.Sort([](const UActorComponent& A, const UActorComponent& B)
		{
			const int32 PriorityA = IInteractableComponentTargetInterface::Execute_GetInteractPriority(&A);
			const int32 PriorityB = IInteractableComponentTargetInterface::Execute_GetInteractPriority(&B);
			return PriorityA > PriorityB;
		});
	}

	UActorComponent* SelectBestInteractableComponent(AActor* Target, UPrimitiveComponent* HitComponent)
	{
		if (!Target)
		{
			return nullptr;
		}

		TArray<UActorComponent*> InteractableComponents;
		GatherInteractableComponents(Target, InteractableComponents);
		if (InteractableComponents.Num() == 0)
		{
			return nullptr;
		}

		// No hit component: use highest-priority interactable component.
		if (!HitComponent)
		{
			return InteractableComponents[0];
		}

		TSet<UPrimitiveComponent*> HierarchyMeshes;
		CollectAttachmentHierarchy(HitComponent, HierarchyMeshes);

		UActorComponent* BestMeshScoped = nullptr;
		int32 BestMeshPriority = MIN_int32;
		UActorComponent* BestActorScoped = nullptr;
		int32 BestActorPriority = MIN_int32;

		for (UActorComponent* Comp : InteractableComponents)
		{
			bool bHasMeshProperty = false;
			const bool bMatchesHierarchy = CapabilityMatchesHierarchy(Comp, HierarchyMeshes, bHasMeshProperty);
			const int32 Priority = IInteractableComponentTargetInterface::Execute_GetInteractPriority(Comp);

			if (bMatchesHierarchy)
			{
				if (!BestMeshScoped || Priority > BestMeshPriority)
				{
					BestMeshScoped = Comp;
					BestMeshPriority = Priority;
				}
			}
			else if (!bHasMeshProperty)
			{
				if (!BestActorScoped || Priority > BestActorPriority)
				{
					BestActorScoped = Comp;
					BestActorPriority = Priority;
				}
			}
		}

		return BestMeshScoped ? BestMeshScoped : BestActorScoped;
	}

	bool ResolveFocusFromComponents(AActor* Target, UPrimitiveComponent* HitComponent, FInteractionFocusInfo& OutFocus)
	{
		if (!Target || !HitComponent)
		{
			return false;
		}

		UActorComponent* Selected = SelectBestInteractableComponent(Target, HitComponent);
		if (!Selected)
		{
			return false;
		}

		OutFocus.Label = IInteractableComponentTargetInterface::Execute_GetInteractionLabel(Selected);
		if (OutFocus.Label.IsEmpty())
		{
			OutFocus.Label = NSLOCTEXT("Interaction", "Interact", "Interact");
		}
		OutFocus.HighlightMesh = GetCapabilityTargetMesh(Selected);
		if (!OutFocus.HighlightMesh)
		{
			OutFocus.HighlightMesh = HitComponent;
		}

		return true;
	}

	bool ResolveInteractionExecutionSpecFromComponents(
		AActor* Target,
		UPrimitiveComponent* HitComponent,
		AActor* Instigator,
		FInteractionExecutionSpec& OutSpec)
	{
		OutSpec = FInteractionExecutionSpec();
		if (!Target)
		{
			return false;
		}

		UActorComponent* Selected = SelectBestInteractableComponent(Target, HitComponent);
		if (!Selected)
		{
			return false;
		}

		OutSpec = IInteractableComponentTargetInterface::Execute_GetInteractionExecutionSpec(Selected, Instigator);
		return true;
	}

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

	bool ExecuteInteractionViaComponents(AActor* Target, AActor* Instigator, UPrimitiveComponent* HitComponent)
	{
		if (!Target || !Instigator)
		{
			return false;
		}

		// Keep execution target selection consistent with focus selection.
		UActorComponent* Selected = SelectBestInteractableComponent(Target, HitComponent);
		if (!Selected)
		{
			return false;
		}

		return IInteractableComponentTargetInterface::Execute_OnComponentInteract(Selected, Instigator);
	}
}

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	OutlineMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/ProjectMaterial/Effect/MI_Outline.MI_Outline"))
	);

	// bDrawDebug = false; // Enable for debug trace visualization

	UE_LOG(LogInteraction, Verbose, TEXT("[%s] Constructor: TraceDistance=%.0f, TraceRadius=%.0f"),
		*GetName(), TraceDistance, TraceRadius);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NULL");
	UE_LOG(LogInteraction, Log, TEXT("[%s] BeginPlay: Owner=%s, TraceDistance=%.0f, TraceRadius=%.0f, Channel=%d, FrameInterval=%d"),
		*GetName(), *OwnerName, TraceDistance, TraceRadius, (int32)TraceChannel.GetValue(), TraceFrameInterval);

	if (bEnableHighlight)
	{
		SetupPostProcess();
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

	// Skip frames for performance (trace every N frames)
	++FrameCounter;
	if (FrameCounter < TraceFrameInterval)
	{
		return;
	}
	FrameCounter = 0;

	UpdateTrace();
}

void UInteractionComponent::UpdateTrace()
{
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

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	TraceStart = PC->PlayerCameraManager->GetCameraLocation();
	FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	TraceEnd = TraceStart + (CameraForward * TraceDistance);

	// Line trace only (same as interaction trace for consistency)
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		TraceChannel,
		QueryParams
	);

	AActor* HitActor = bHit ? HitResult.GetActor() : nullptr;
	UPrimitiveComponent* HitComponent = bHit ? HitResult.GetComponent() : nullptr;
	SetFocusedActor(HitActor, HitComponent);
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
		ResolveFocusFromComponents(NewFocus, HitComponent, FocusInfo);
	}
	if (NewFocus && !bResolvedExecutionSpecFromActor)
	{
		ResolveInteractionExecutionSpecFromComponents(NewFocus, HitComponent, GetOwner(), NewExecutionSpec);
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

	return ExecuteInteractionViaComponents(Target, Instigator, HitComponent);
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
	if (!bDrawDebug && !GIsEditor)
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

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		UE_LOG(LogInteraction, Verbose, TEXT("[%s] DrawInteractionDebugTraceOnInput: Skipped - no PlayerController or CameraManager"), *GetName());
		return;
	}

	const FVector DebugTraceStart = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector DebugTraceEnd = DebugTraceStart + (CameraForward * TraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		DebugTraceStart,
		DebugTraceEnd,
		TraceChannel,
		QueryParams
	);

	const FColor LineColor = bHit ? FColor::Green : FColor::Red;
	const FVector EndPoint = bHit ? HitResult.ImpactPoint : DebugTraceEnd;
	UKismetSystemLibrary::DrawDebugLine(this, DebugTraceStart, EndPoint, LineColor, 5.0f, 0.3f);

	UE_LOG(
		LogInteraction,
		Log,
		TEXT("[%s] DrawInteractionDebugTraceOnInput: Drawn %s line to '%s'"),
		*GetName(),
		bHit ? TEXT("hit") : TEXT("miss"),
		bHit && HitResult.GetActor() ? *HitResult.GetActor()->GetActorNameOrLabel() : TEXT("None"));
#endif
}

bool UInteractionComponent::TryInteract_Implementation()
{
	UE_LOG(LogInteraction, Verbose, TEXT("[%s] TryInteract: Called"), *GetName());

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogInteraction, Warning, TEXT("[%s] TryInteract: FAILED - No owner"), *GetName());
		return false;
	}

	// Server-authoritative: if we have authority, execute directly; otherwise send RPC
	if (Owner->HasAuthority())
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] TryInteract: Has authority, executing locally"), *GetName());
		ExecuteInteraction_ServerAuth();
	}
	else
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] TryInteract: No authority, sending Server RPC"), *GetName());
		Server_TryInteract();
	}

	return true;
}

bool UInteractionComponent::BeginInteractInput_Implementation()
{
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

void UInteractionComponent::Server_TryInteract_Implementation()
{
	UE_LOG(LogInteraction, Log, TEXT("[%s] Server_TryInteract: RPC received"), *GetName());
	ExecuteInteraction_ServerAuth();
}

void UInteractionComponent::ExecuteInteraction_ServerAuth()
{
	// Re-trace on server (don't trust client's cached FocusedActor)
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

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		UE_LOG(LogInteraction, Warning, TEXT("[%s] ExecuteInteraction_ServerAuth: No PlayerController or CameraManager"), *GetName());
		return;
	}

	// Server-side trace (authoritative)
	FVector ServerTraceStart = PC->PlayerCameraManager->GetCameraLocation();
	FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	FVector ServerTraceEnd = ServerTraceStart + (CameraForward * TraceDistance);

	// Setup trace parameters
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Owner);

	const ETraceTypeQuery TraceType = UEngineTypes::ConvertToTraceType(TraceChannel);

	// Line trace only (no sphere trace for cleaner debug visualization)
	FHitResult HitResult;
	const bool bHit = UKismetSystemLibrary::LineTraceSingle(
		this,
		ServerTraceStart,
		ServerTraceEnd,
		TraceType,
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		true
	);

	AActor* Target = bHit ? HitResult.GetActor() : nullptr;
	UPrimitiveComponent* HitComponent = bHit ? HitResult.GetComponent() : nullptr;

	if (!Target)
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] ExecuteInteraction_ServerAuth: No target found (server trace)"), *GetName());
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
		bHandled = ExecuteInteractionViaComponents(Target, Owner, HitComponent);
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

	UE_LOG(LogInteraction, Verbose, TEXT("[%s] CancelHoldInteraction: Timed interaction cancelled"), *GetName());
	BroadcastPromptStateToService();
}

void UInteractionComponent::CompleteHoldInteraction()
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
	BroadcastPromptStateToService();

	UE_LOG(LogInteraction, Log, TEXT("[%s] CompleteHoldInteraction: Timed interaction completed"), *GetName());
	TryInteract_Implementation();
}


void UInteractionComponent::SetupPostProcess()
{
	UE_LOG(LogInteraction, Log, TEXT("[%s] SetupPostProcess: Starting setup, bEnableHighlight=%s"),
		*GetName(), bEnableHighlight ? TEXT("true") : TEXT("false"));

	if (!OutlineMaterial.IsNull())
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
	else
	{
		UE_LOG(LogInteraction, Log, TEXT("[%s] SetupPostProcess: No OutlineMaterial configured, highlight disabled"),
			*GetName());
		return;
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
