// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectCinematicSubsystem.h"

#include "CinematicDirector.h"
#include "CinematicHideMetadata.h"            // editor-counterpart soft-ref carrier
#include "Components/PrimitiveComponent.h"
#include "ProjectCinematicEditor.h"   // LogProjectCinematicEditor
#include "Interfaces/IInteractionService.h"   // IInteractionComponentInterface::GetFocusedComponent + IInteractionService
#include "ProjectServiceLocator.h"            // FProjectServiceLocator::Resolve<IInteractionService>
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Recorder/TakeRecorder.h"
#include "Recorder/TakeRecorderSubsystem.h"
#include "MovieSceneObjectBindingID.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "SinglePlayController.h"
#include "TakeRecorderActorSource.h"
#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

// Owning definition for LogProjectCinematicEditor. The forward declaration
// lives in ProjectCinematicEditor.h (the module's public header), which
// every TU in this module includes. Single DECLARE in the codebase = no
// unity-build redeclaration headaches.
DEFINE_LOG_CATEGORY(LogProjectCinematicEditor);

namespace
{
	/** Map (Panel, bVisible) -> parameterless thunk name on UCinematicDirector.
	 *  Thunks bake the panel + bool literals at C++ compile time; the runtime
	 *  evaluator only reads Ptrs.Function (PayloadVariables is editor-only
	 *  and unread by MovieSceneEventSystems.cpp). Extend by adding a new
	 *  thunk to UCinematicDirector + an entry here. */
	UFunction* ResolveThunk(FName Panel, bool bVisible)
	{
		// Static const inside function for lazy init + no global init order issues.
		static const TMap<FName, FName> OpenNames = {
			{ FName(TEXT("Inventory")),   FName(TEXT("Cinematic_OpenInventory")) },
			{ FName(TEXT("Vitals")),      FName(TEXT("Cinematic_OpenVitals")) },
			{ FName(TEXT("MindJournal")), FName(TEXT("Cinematic_OpenMindJournal")) },
		};
		static const TMap<FName, FName> CloseNames = {
			{ FName(TEXT("Inventory")),   FName(TEXT("Cinematic_CloseInventory")) },
			{ FName(TEXT("Vitals")),      FName(TEXT("Cinematic_CloseVitals")) },
			{ FName(TEXT("MindJournal")), FName(TEXT("Cinematic_CloseMindJournal")) },
		};
		const TMap<FName, FName>& Table = bVisible ? OpenNames : CloseNames;
		const FName* Found = Table.Find(Panel);
		if (!Found) { return nullptr; }
		return UCinematicDirector::StaticClass()->FindFunctionByName(*Found);
	}

	/** Find the active recording world (PIE -> editor world fallback).
	 *  Verified by spike 1 that Take Recorder defaults to non-PIE recording. */
	UWorld* ResolveRecordWorld()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			return GEditor->PlayWorld;
		}
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game)
				{
					if (UWorld* W = Ctx.World()) { return W; }
				}
			}
		}
		if (GEditor)
		{
			return GEditor->GetEditorWorldContext().World();
		}
		return nullptr;
	}
}

void UProjectCinematicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UTakeRecorderSubsystem* RecorderSubsys =
		GEngine ? GEngine->GetEngineSubsystem<UTakeRecorderSubsystem>() : nullptr)
	{
		SubsystemStartHandle = RecorderSubsys->GetOnRecordingStartedEvent().AddUObject(
			this, &UProjectCinematicSubsystem::HandleSubsystemRecordingStarted);
		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem] Initialized. Subscribed to UTakeRecorderSubsystem::OnRecordingStartedEvent."));
	}
	else
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem] UTakeRecorderSubsystem unavailable at init. Capture pipeline disabled."));
	}
}

void UProjectCinematicSubsystem::Deinitialize()
{
	if (SubsystemStartHandle.IsValid())
	{
		if (UTakeRecorderSubsystem* RecorderSubsys =
			GEngine ? GEngine->GetEngineSubsystem<UTakeRecorderSubsystem>() : nullptr)
		{
			RecorderSubsys->GetOnRecordingStartedEvent().Remove(SubsystemStartHandle);
		}
		SubsystemStartHandle.Reset();
	}
	UnsubscribeFromController();
	UnsubscribeFromFocusService();
	Super::Deinitialize();
}

void UProjectCinematicSubsystem::HandleSubsystemRecordingStarted(UTakeRecorder* Recorder)
{
	// Fires during CountingDown (state=1). Chain to the recorder's own
	// OnRecordingStarted which fires when state=Started.
	if (!Recorder)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][CountingDown] Subsystem fired event with null Recorder. Aborting chain."));
		return;
	}

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][CountingDown] Recorder='%s' state=%d. Chaining OnRecordingStarted + OnRecordingFinished delegates."),
		*Recorder->GetName(), static_cast<int32>(Recorder->GetState()));

	Recorder->OnRecordingStarted().AddWeakLambda(this,
		[this](UTakeRecorder* StartedRecorder)
		{
			HandleRecorderStateStarted(StartedRecorder);
		});

	Recorder->OnRecordingFinished().AddWeakLambda(this,
		[this](UTakeRecorder* FinishedRecorder)
		{
			HandleRecordingFinished(FinishedRecorder);
		});
}

void UProjectCinematicSubsystem::HandleRecorderStateStarted(UTakeRecorder* Recorder)
{
	if (!Recorder)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Started] Null Recorder; aborting state-Started chain."));
		return;
	}
	ActiveRecorder = Recorder;
	RecordedActors.Reset();
	EditorCounterpartsToHide.Reset();
	BufferedPanelEvents.Reset();
	BufferedFocusEvents.Reset();
	PreviousFocusedComponentName = NAME_None;
	PreviousFocusedActorLabel.Reset();
	bSourcesSettingsApplied = false;

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][Started] Recorder='%s' state=%d. Cleared per-take buffers."),
		*Recorder->GetName(), static_cast<int32>(Recorder->GetState()));

	ULevelSequence* Sequence = Recorder->GetSequence();
	if (!Sequence)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Started] Recorder has no sequence. Capture pipeline cannot proceed for this take."));
		return;
	}
	UTakeRecorderSources* Sources = Sequence->FindMetaData<UTakeRecorderSources>();
	if (!Sources)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Started] No UTakeRecorderSources metadata on sequence '%s'. AddSource will fail."),
			*Sequence->GetPathName());
		return;
	}

	const int32 PreSourceCount = Sources->GetSources().Num();
	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][Started] Sequence='%s' (package='%s') | pre-existing sources count=%d"),
		*Sequence->GetPathName(),
		Sequence->GetOutermost() ? *Sequence->GetOutermost()->GetName() : TEXT("<null>"),
		PreSourceCount);

	FTakeRecorderSourcesSettings S = Sources->GetSettings();
	const bool bPrevSubSeq = S.bRecordSourcesIntoSubSequences;
	if (S.bRecordSourcesIntoSubSequences)
	{
		S.bRecordSourcesIntoSubSequences = false;
		Sources->SetSettings(S);
	}
	bSourcesSettingsApplied = true;
	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][Started] Sources settings | bRecordSourcesIntoSubSequences: was=%d -> set=%d"),
		bPrevSubSeq ? 1 : 0, S.bRecordSourcesIntoSubSequences ? 1 : 0);

	UWorld* RecordWorld = ResolveRecordWorld();
	if (!RecordWorld)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Started] No record world found. Controller signals cannot be subscribed."));
		return;
	}
	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][Started] Record world resolved | name='%s' worldType=%d (0=None,1=Game,2=PIE,3=Editor,...) | netMode=%d"),
		*RecordWorld->GetName(),
		static_cast<int32>(RecordWorld->WorldType),
		static_cast<int32>(RecordWorld->GetNetMode()));
	SubscribeToController(RecordWorld);
	SubscribeToFocusService();

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem] Recording started; subscribed to controller signals + focus service in world '%s'."),
		*RecordWorld->GetName());
}

void UProjectCinematicSubsystem::SubscribeToController(UWorld* RecordWorld)
{
	UnsubscribeFromController();
	if (!RecordWorld)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Subscribe] Null world; subscription skipped."));
		return;
	}

	int32 PCSeen = 0;
	for (TActorIterator<ASinglePlayController> It(RecordWorld); It; ++It)
	{
		ASinglePlayController* PC = *It;
		++PCSeen;
		if (!PC) { continue; }
		InteractionHandle = PC->OnInteractionTriggered.AddUObject(
			this, &UProjectCinematicSubsystem::HandleInteractionTriggered);
		PanelHandle = PC->OnPanelVisibilityChanged.AddUObject(
			this, &UProjectCinematicSubsystem::HandlePanelVisibilityChanged);
		SubscribedController = PC;
		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][Subscribe] Subscribed to PC '%s' (path='%s') | interactionHandleValid=%d panelHandleValid=%d"),
			*PC->GetName(), *PC->GetPathName(),
			InteractionHandle.IsValid() ? 1 : 0, PanelHandle.IsValid() ? 1 : 0);
		return;
	}
	UE_LOG(LogProjectCinematicEditor, Warning,
		TEXT("[CinematicSubsystem][Subscribe] No ASinglePlayController found in world '%s' (iterated %d). Interaction + panel signals will not be captured this take."),
		*RecordWorld->GetName(), PCSeen);
}

void UProjectCinematicSubsystem::UnsubscribeFromController()
{
	const bool bHadPC = SubscribedController.IsValid();
	if (ASinglePlayController* PC = SubscribedController.Get())
	{
		if (InteractionHandle.IsValid()) { PC->OnInteractionTriggered.Remove(InteractionHandle); }
		if (PanelHandle.IsValid())       { PC->OnPanelVisibilityChanged.Remove(PanelHandle); }
	}
	const bool bHadInteractionHandle = InteractionHandle.IsValid();
	const bool bHadPanelHandle = PanelHandle.IsValid();
	InteractionHandle.Reset();
	PanelHandle.Reset();
	SubscribedController.Reset();
	if (bHadPC || bHadInteractionHandle || bHadPanelHandle)
	{
		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][Unsubscribe] Cleared | hadPC=%d hadInteractionHandle=%d hadPanelHandle=%d"),
			bHadPC ? 1 : 0, bHadInteractionHandle ? 1 : 0, bHadPanelHandle ? 1 : 0);
	}
}

void UProjectCinematicSubsystem::SubscribeToFocusService()
{
	// Resolve IInteractionService via the project's service locator
	// (ProjectCore). The service is registered by ProjectInteraction at
	// module startup; if unregistered, focus mirroring is disabled for this
	// take with a clear warning.
	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (!Service.IsValid())
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][FocusSub] IInteractionService not registered with ProjectServiceLocator. Cinematic highlight will NOT be recorded for this take."));
		return;
	}

	FocusChangedHandle = Service->OnFocusChanged().AddUObject(
		this, &UProjectCinematicSubsystem::HandleFocusChanged);

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][FocusSub] Subscribed to IInteractionService::OnFocusChanged | handleValid=%d"),
		FocusChangedHandle.IsValid() ? 1 : 0);
}

void UProjectCinematicSubsystem::UnsubscribeFromFocusService()
{
	if (!FocusChangedHandle.IsValid())
	{
		return;
	}
	if (TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>())
	{
		Service->OnFocusChanged().Remove(FocusChangedHandle);
	}
	const bool bWasValid = FocusChangedHandle.IsValid();
	FocusChangedHandle.Reset();
	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][FocusUnsub] Cleared | wasValid=%d"),
		bWasValid ? 1 : 0);
}

void UProjectCinematicSubsystem::HandleFocusChanged(
	APawn* Instigator, AActor* FocusedActor, UPrimitiveComponent* FocusedComponent, FText Label)
{
	// State-mirror entry point. Every gameplay focus change broadcast by
	// the InteractionComponent through IInteractionService lands here. We
	// translate the broadcast into a (component, on/off, frame) buffered
	// event. StampFocusHighlights walks the buffer at recording-finished
	// and writes one MovieSceneBoolTrack per unique component on the
	// take's Spawnable, with keys at the exact frame each focus transition
	// happened in gameplay.
	//
	// Semantics:
	//   FocusedActor != null + FocusedComponent != null  -> focus ON for that component
	//   FocusedActor == null OR FocusedComponent == null -> focus OFF (clear previous)
	UTakeRecorder* Recorder = ActiveRecorder.Get();
	if (!Recorder || !bSourcesSettingsApplied)
	{
		// Not actively recording -- the service still broadcasts (HUD uses
		// it) but we ignore. Verbose log so it doesn't flood the active log.
		UE_LOG(LogProjectCinematicEditor, Verbose,
			TEXT("[CinematicSubsystem][Focus] Ignored (no active recording) | actor=%s component=%s"),
			FocusedActor ? *FocusedActor->GetName() : TEXT("<null>"),
			FocusedComponent ? *FocusedComponent->GetName() : TEXT("<null>"));
		return;
	}

	ULevelSequence* Sequence = Recorder->GetSequence();
	if (!Sequence)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Focus] Active recorder has no sequence. Focus event dropped."));
		return;
	}
	UTakeRecorderSources* Sources = Sequence->FindMetaData<UTakeRecorderSources>();
	if (!Sources)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Focus] No Sources metadata; focus event dropped."));
		return;
	}

	const FQualifiedFrameTime Now = Sources->GetCachedFrameTime();
	const FName NewComponentName = FocusedComponent ? FocusedComponent->GetFName() : NAME_None;
	const bool  bNewIsFocused    = (FocusedActor != nullptr && FocusedComponent != nullptr);
	const FString NewActorLabel  = (bNewIsFocused && FocusedActor)
		? FocusedActor->GetActorLabel()
		: FString();

	// `IInteractionService::OnFocusChanged` only broadcasts the NEW focus.
	// When focus moves A -> B we don't get an explicit "focus off A" event
	// from the broadcast -- we infer it: any (actor, component) that WAS
	// focused before this broadcast and isn't the new focus gets a synthetic
	// OFF at the same frame the new ON fires. Without this, the bool track
	// for the previous mesh would have an ON key with no matching OFF key
	// and stay outlined forever in the render.
	//
	// Identity comparison MUST include the actor label, not just the
	// component name: component names like "StaticMeshComponent_1" are
	// shared across spawnables, so going from
	// Wardrobe.StaticMeshComponent_1 -> Dresser.StaticMeshComponent_1
	// would compare equal on name alone, skip the synthetic OFF, and leave
	// the wardrobe outline stuck on while the dresser also lights up.
	const bool bSameAsPrevious =
		(PreviousFocusedComponentName == NewComponentName) &&
		PreviousFocusedActorLabel.Equals(NewActorLabel, ESearchCase::CaseSensitive);

	if (PreviousFocusedComponentName != NAME_None && !bSameAsPrevious)
	{
		// Deterministic stamper requires the previous actor's label so it can
		// resolve which spawnable owns this OFF transition. Without it, an
		// OFF on "StaticMeshComponent_1" could be attributed to whichever
		// actor's StaticMeshComponent_1 the stamper found first -- wrong
		// outline turns off, right one stays lit.
		FBufferedFocusEvent SyntheticOff;
		SyntheticOff.ActorLabel    = PreviousFocusedActorLabel;
		SyntheticOff.ComponentName = PreviousFocusedComponentName;
		SyntheticOff.bFocused      = false;
		SyntheticOff.Frame         = Now.Time.FrameNumber;
		SyntheticOff.FrameRate     = Now.Rate;
		BufferedFocusEvents.Add(SyntheticOff);

		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][Focus] OFF (synthetic) | actor='%s' component='%s' frame=%d | bufferedTotal=%d"),
			*PreviousFocusedActorLabel,
			*PreviousFocusedComponentName.ToString(),
			SyntheticOff.Frame.Value,
			BufferedFocusEvents.Num());
	}

	// Now buffer the broadcast event itself. If the broadcast had no
	// component (focus lost), it's a real OFF transition for the previously
	// focused component, which the synthetic-OFF block above already wrote
	// -- we still log the broadcast for observability but only buffer if
	// there's something to key.
	if (bNewIsFocused)
	{
		// Primary capture path: any actor the player LOOKS at becomes a
		// Take Recorder source (deduped). Required for StampFocusHighlights
		// to resolve possessables by component name -- without this, focus
		// events on actors that the player never E-pressed get stamped
		// against a missing possessable and silently dropped (verified in
		// Alis.log 2026-05-20 T2 line 11226-11232: Wardrobe_Classic focus
		// events buffered but recordedActors=1, hideTargets=1 -- wardrobe
		// outline never appeared in render). E-press is no longer the
		// gatekeeper for capture; HandleInteractionTriggered remains as a
		// dedup-friendly safety net for edge cases (instant pickups, etc).
		const bool bAddedViaFocus = AddRecorderSourceForActor(FocusedActor);
		if (bAddedViaFocus)
		{
			UE_LOG(LogProjectCinematicEditor, Display,
				TEXT("[CinematicSubsystem][Focus] Recorded via look (AddSource) | actor='%s' component='%s'"),
				*FocusedActor->GetActorLabel(),
				*NewComponentName.ToString());
		}

		FBufferedFocusEvent Ev;
		Ev.ActorLabel    = FocusedActor->GetActorLabel();
		Ev.ComponentName = NewComponentName;
		Ev.bFocused      = true;
		Ev.Frame         = Now.Time.FrameNumber;
		Ev.FrameRate     = Now.Rate;
		BufferedFocusEvents.Add(Ev);

		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][Focus] ON  | actor='%s' component='%s' label='%s' frame=%d | bufferedTotal=%d"),
			*Ev.ActorLabel,
			*Ev.ComponentName.ToString(),
			*Label.ToString(),
			Ev.Frame.Value,
			BufferedFocusEvents.Num());
	}
	else
	{
		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][Focus] OFF (broadcast) | previous='%s' frame=%d | bufferedTotal=%d"),
			*PreviousFocusedComponentName.ToString(),
			Now.Time.FrameNumber.Value,
			BufferedFocusEvents.Num());
	}

	PreviousFocusedComponentName = NewComponentName;
	PreviousFocusedActorLabel    = NewActorLabel;
}

bool UProjectCinematicSubsystem::AddRecorderSourceForActor(AActor* TargetActor)
{
	// Shared AddSource path used by both E-press (HandleInteractionTriggered)
	// and focus-driven capture (HandleFocusChanged). Dedup via RecordedActors
	// makes both callers idempotent -- the first one to hit a given actor
	// wins, subsequent calls are no-ops.
	//
	// State preconditions: there must be an active recorder in the Started
	// state (bSourcesSettingsApplied is the marker), and the actor must be
	// valid and not mid-destruction. If any precondition fails, return false
	// without logging at Display level -- callers log their own context.
	UTakeRecorder* Recorder = ActiveRecorder.Get();
	if (!Recorder || !bSourcesSettingsApplied || !TargetActor)
	{
		return false;
	}

	if (!IsValid(TargetActor) || TargetActor->IsActorBeingDestroyed())
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][AddSource] Skipped invalid actor | valid=%d beingDestroyed=%d"),
			IsValid(TargetActor) ? 1 : 0,
			TargetActor->IsActorBeingDestroyed() ? 1 : 0);
		return false;
	}

	if (RecordedActors.Contains(TargetActor))
	{
		// Deduped -- caller-side log will say which path triggered the dedup.
		return false;
	}

	// Do NOT mark RecordedActors yet. We only commit to the set AFTER
	// StartRecordingSource succeeds. If Sequence / Sources / AddSource fails,
	// the next focus broadcast (or next E-press) for the same actor will
	// retry. Marking too early permanently locks out retries within the take.

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][AddSource] Begin | actor='%s' label='%s' class='%s' world='%s' worldType=%d | path='%s'"),
		*TargetActor->GetName(), *TargetActor->GetActorLabel(),
		*TargetActor->GetClass()->GetName(),
		TargetActor->GetWorld() ? *TargetActor->GetWorld()->GetName() : TEXT("<null>"),
		TargetActor->GetWorld() ? static_cast<int32>(TargetActor->GetWorld()->WorldType) : -1,
		*TargetActor->GetPathName());

	ULevelSequence* Sequence = Recorder->GetSequence();
	if (!Sequence)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][AddSource] Recorder lost its Sequence; aborting AddSource for '%s' (will retry on next focus/E-press)."),
			*TargetActor->GetName());
		return false;
	}
	UTakeRecorderSources* Sources = Sequence->FindMetaData<UTakeRecorderSources>();
	if (!Sources)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][AddSource] No UTakeRecorderSources metadata on sequence; aborting AddSource for '%s' (will retry on next focus/E-press)."),
			*TargetActor->GetName());
		return false;
	}

	UTakeRecorderActorSource* Src = Sources->AddSource<UTakeRecorderActorSource>();
	if (!Src)
	{
		UE_LOG(LogProjectCinematicEditor, Error,
			TEXT("[CinematicSubsystem][AddSource] Sources->AddSource<UTakeRecorderActorSource>() returned null. AddSource failed for '%s' (will retry on next focus/E-press)."),
			*TargetActor->GetName());
		return false;
	}
	Src->Target = TargetActor;
	Src->bRecordParentHierarchy = false;
	Src->RecordType = ETakeRecorderActorRecordType::Spawnable;

	// RebuildRecordedPropertyMap is protected on UTakeRecorderActorSource;
	// the public path is PostEditChangeProperty on Target (engine handler at
	// TakeRecorderActorSource.cpp:1105-1118 rebuilds internally).
	FProperty* TargetProp = FindFProperty<FProperty>(
		UTakeRecorderActorSource::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UTakeRecorderActorSource, Target));
	if (TargetProp)
	{
		FPropertyChangedEvent Evt(TargetProp, EPropertyChangeType::ValueSet);
		Src->PostEditChangeProperty(Evt);
	}
	else
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][AddSource] FProperty 'Target' not found on UTakeRecorderActorSource. Property map will not rebuild; component sub-tracks may be missing."));
	}

	const FQualifiedFrameTime StartTime = Sources->GetCachedFrameTime();
	Sources->StartRecordingSource({ Src }, StartTime);

	// Commit AFTER success. Past this point, the actor is genuinely being
	// recorded by Take Recorder and dedup is correct: subsequent focus/E
	// events for this actor will short-circuit at the RecordedActors.Contains
	// check at the top of this function.
	RecordedActors.Add(TargetActor);

	// Editor-counterpart capture happens AFTER successful AddSource. If
	// AddSource had failed, capturing the counterpart would leak an entry
	// into EditorCounterpartsToHide that StampHideOriginals would later
	// stamp -- producing a hide track for an actor that was never
	// recorded (would hide the placed editor copy at render even though
	// no take-Spawnable exists to take its place, leaving an empty slot).
	//
	// Remap PIE actor -> editor-world counterpart so Take Recorder binds the
	// Possessable to a stable identity that survives PIE exit. This mirrors
	// the standard UI flow (designer picks from editor outliner). Runtime
	// delegates give us the PIE actor, so we remap explicitly.
	//
	// If no counterpart exists (player pawn, runtime-spawned AI), BindTarget
	// stays as the PIE actor. Take Recorder's check at
	// TakeRecorderActorSource.cpp:261-266 then auto-converts to Spawnable so
	// the binding still resolves outside PIE. Net effect:
	//   - placed interactable with editor counterpart -> Possessable
	//   - runtime-only actor (no counterpart)          -> Spawnable (auto)
	// ALIS interactables are data-composed: AInteractableActor adds
	// StaticMeshComponent + SpringSliderComponent + capability components
	// from JSON at runtime. These have CreationMethod != SCS/Native, so
	// Take Recorder's Possessable check at TakeRecorderActorSource.cpp:996
	// rejects them ("had dynamically added component at runtime ... cannot
	// be saved because we are recording to a possessable, component binding
	// will be broken"). Spawnable duplicates the runtime component tree
	// into the take's object template -> drawer motion records correctly.
	//
	// Trade-off: a Spawnable take spawns its own dresser at playback time.
	// The placed editor actor is still in the world -> two dressers visible
	// at MRQ render. We capture the editor counterpart here and the
	// finish-time stamper emits a Possessable + Visibility track that hides
	// the placed actor over the sequence range. Net result: one dresser
	// visible (the take's), drawers move.
	if (GEditor && TargetActor->GetWorld() &&
		TargetActor->GetWorld()->WorldType != EWorldType::Editor)
	{
		if (AActor* EditorCounterpart =
				EditorUtilities::GetEditorWorldCounterpartActor(TargetActor))
		{
			if (EditorCounterpart != TargetActor)
			{
				EditorCounterpartsToHide.Add(EditorCounterpart);
				UE_LOG(LogProjectCinematicEditor, Display,
					TEXT("[CinematicSubsystem][AddSource] Captured editor counterpart for hide-track: '%s' (path='%s')."),
					*EditorCounterpart->GetName(), *EditorCounterpart->GetPathName());
			}
		}
		else
		{
			// PIE-only actor (player pawn, runtime spawn) -- nothing to hide.
			UE_LOG(LogProjectCinematicEditor, Display,
				TEXT("[CinematicSubsystem][AddSource] No editor counterpart for '%s' (PIE-only) -- no hide track needed."),
				*TargetActor->GetName());
		}
	}

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][AddSource] OK | actor='%s' RecordType=Spawnable bRecordParentHierarchy=%d startTime=%d (rate=%s) | post-add source count=%d"),
		*TargetActor->GetActorLabel(),
		Src->bRecordParentHierarchy ? 1 : 0,
		StartTime.Time.FrameNumber.Value,
		*StartTime.Rate.ToPrettyText().ToString(),
		Sources->GetSources().Num());

	return true;
}

void UProjectCinematicSubsystem::HandleInteractionTriggered(AActor* TargetActor, UActorComponent* RespondingComponent)
{
	// E-press path. AddRecorderSourceForActor handles state checks, validity,
	// dedup, editor-counterpart capture, and the actual AddSource. We just
	// log the caller-side context (which path triggered the add or skip).
	//
	// In normal flow the focus broadcast (HandleFocusChanged) has already
	// AddSourced this actor by the time E-press fires, so we expect this
	// callsite to mostly hit the deduped branch (return false). Kept anyway
	// to cover the edge case of E-press without a preceding focus event
	// (instant-range pickups, AI-initiated interactions, etc).
	const bool bAdded = AddRecorderSourceForActor(TargetActor);
	if (bAdded)
	{
		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][Interaction] Recorded via E-press | actor='%s'"),
			TargetActor ? *TargetActor->GetActorLabel() : TEXT("<null>"));
	}
	else if (TargetActor && RecordedActors.Contains(TargetActor))
	{
		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][Interaction] Deduped (focus path already recorded) | actor='%s'"),
			*TargetActor->GetName());
	}
	else
	{
		UE_LOG(LogProjectCinematicEditor, Verbose,
			TEXT("[CinematicSubsystem][Interaction] Filtered (no recorder or invalid) | hasActiveRecorder=%d settingsApplied=%d targetValid=%d"),
			ActiveRecorder.IsValid() ? 1 : 0, bSourcesSettingsApplied ? 1 : 0, TargetActor ? 1 : 0);
	}

	// Focus highlight is the sole source of bRenderCustomDepth bool-track
	// keys. RespondingComponent is intentionally unused here -- focus path
	// owns highlight timing via the state-mirror model.
	(void)RespondingComponent;
}

void UProjectCinematicSubsystem::HandlePanelVisibilityChanged(FName PanelName, bool bVisible)
{
	UTakeRecorder* Recorder = ActiveRecorder.Get();
	if (!Recorder)
	{
		UE_LOG(LogProjectCinematicEditor, Verbose,
			TEXT("[CinematicSubsystem][Panel] Ignored (no active recorder) | panel=%s visible=%d"),
			*PanelName.ToString(), bVisible ? 1 : 0);
		return;
	}
	ULevelSequence* Sequence = Recorder->GetSequence();
	if (!Sequence)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Panel] Recorder has no sequence; panel event dropped | panel=%s visible=%d"),
			*PanelName.ToString(), bVisible ? 1 : 0);
		return;
	}
	UTakeRecorderSources* Sources = Sequence->FindMetaData<UTakeRecorderSources>();
	if (!Sources)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Panel] No Sources metadata on sequence; panel event dropped | panel=%s visible=%d"),
			*PanelName.ToString(), bVisible ? 1 : 0);
		return;
	}

	const FQualifiedFrameTime Cached = Sources->GetCachedFrameTime();
	FBufferedPanelEvent E;
	E.Panel = PanelName;
	E.bVisible = bVisible;
	E.FrameNumber = Cached.Time.FrameNumber;
	BufferedPanelEvents.Add(E);
	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][Panel] Buffered | panel='%s' visible=%d frame=%d rate=%s | bufferedTotal=%d"),
		*PanelName.ToString(), bVisible ? 1 : 0,
		E.FrameNumber.Value,
		*Cached.Rate.ToPrettyText().ToString(),
		BufferedPanelEvents.Num());
}

void UProjectCinematicSubsystem::HandleRecordingFinished(UTakeRecorder* Recorder)
{
	UnsubscribeFromController();
	UnsubscribeFromFocusService();

	if (!Recorder)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Finished] Null Recorder on finish callback. Nothing to stamp/save."));
		ActiveRecorder.Reset();
		return;
	}
	ULevelSequence* Sequence = Recorder->GetSequence();
	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][Finished] Recorder='%s' state=%d sequence='%s' | recordedActors=%d hideTargets=%d panelEvents=%d"),
		*Recorder->GetName(), static_cast<int32>(Recorder->GetState()),
		Sequence ? *Sequence->GetPathName() : TEXT("<null>"),
		RecordedActors.Num(), EditorCounterpartsToHide.Num(), BufferedPanelEvents.Num());

	// CameraCutTrack must run for EVERY take with a valid sequence, regardless
	// of whether any focus/panel/hide work was buffered. The take always has
	// a recorded camera (Take Recorder's auto-added Player source) and MRQ
	// render needs a CameraCutTrack binding to that camera -- without it MRQ
	// falls back to the gameplay view target (hidden phantom pawn at
	// PlayerStart) and renders the wrong POV. This must therefore live
	// OUTSIDE the bHasStampWork gate.
	const bool bHasStampWork = Sequence &&
		(BufferedPanelEvents.Num() > 0
			|| EditorCounterpartsToHide.Num() > 0
			|| BufferedFocusEvents.Num() > 0);

	if (Sequence)
	{
		if (bHasStampWork)
		{
			// Director class assignment is required whenever ANY Event Track key
			// will be stamped -- panel events and hide-originals both use Director
			// thunks. Run unconditionally on the first stamp pass so we never
			// emit Event Track keys against a sequence with no DirectorClass set
			// (which causes silent no-evaluation at render).
			AssignDirectorClass(Sequence);

			if (BufferedPanelEvents.Num() > 0)
			{
				StampPanelEvents(Sequence);
			}
			if (EditorCounterpartsToHide.Num() > 0)
			{
				StampHideOriginals(Sequence);
			}
			if (BufferedFocusEvents.Num() > 0)
			{
				StampFocusHighlights(Sequence);
			}
		}
		else
		{
			UE_LOG(LogProjectCinematicEditor, Display,
				TEXT("[CinematicSubsystem][Finished] No focus/panel/hide stamp work to do; still running CameraCutTrack pass."));
		}

		// Always run -- see comment block above.
		EnsureCameraCutTrack(Sequence);

		// Save the produced asset so any stamped track (including the
		// CameraCutTrack we may have just added) lands on disk.
		if (UPackage* Pkg = Sequence->GetOutermost())
		{
			Pkg->MarkPackageDirty();
			const FString PackageFilename = FPackageName::LongPackageNameToFilename(
				Pkg->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			const bool bSaveOk = UPackage::SavePackage(Pkg, Sequence, *PackageFilename, SaveArgs);
			UE_LOG(LogProjectCinematicEditor, Display,
				TEXT("[CinematicSubsystem][Save] package='%s' file='%s' result=%s"),
				*Pkg->GetName(), *PackageFilename,
				bSaveOk ? TEXT("Success") : TEXT("FAILED"));
		}
		else
		{
			UE_LOG(LogProjectCinematicEditor, Warning,
				TEXT("[CinematicSubsystem][Save] Sequence has no outermost package; cannot persist stamped tracks."));
		}
	}
	else
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Finished] Null sequence on finish callback -- nothing to stamp or save."));
	}

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][Finished] Done | stamped panelEvents=%d hideRefs=%d focusEvents=%d capturedActors=%d. Resetting buffers."),
		BufferedPanelEvents.Num(), EditorCounterpartsToHide.Num(), BufferedFocusEvents.Num(), RecordedActors.Num());

	ActiveRecorder.Reset();
	RecordedActors.Reset();
	EditorCounterpartsToHide.Reset();
	BufferedPanelEvents.Reset();
	BufferedFocusEvents.Reset();
	bSourcesSettingsApplied = false;
}

void UProjectCinematicSubsystem::AssignDirectorClass(ULevelSequence* Sequence) const
{
	if (!Sequence)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][Director] Null sequence; cannot assign DirectorClass."));
		return;
	}
	// ULevelSequence::DirectorClass is protected. Use UPROPERTY reflection
	// (same path Python's unreal.set_editor_property uses internally;
	// validated by spike 2 + save-load roundtrip).
	FObjectProperty* Prop = FindFProperty<FObjectProperty>(
		ULevelSequence::StaticClass(), TEXT("DirectorClass"));
	if (!Prop)
	{
		UE_LOG(LogProjectCinematicEditor, Error,
			TEXT("[CinematicSubsystem][Director] FObjectProperty 'DirectorClass' not found on ULevelSequence. Engine layout changed or wrong UClass."));
		return;
	}
	void* Container = Prop->ContainerPtrToValuePtr<void>(Sequence);
	UObject* PrevValue = Prop->GetObjectPropertyValue(Container);
	Prop->SetPropertyValue(Container, UCinematicDirector::StaticClass());
	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][Director] Assigned DirectorClass | sequence='%s' | was='%s' -> set='%s'"),
		*Sequence->GetPathName(),
		PrevValue ? *PrevValue->GetName() : TEXT("<null>"),
		*UCinematicDirector::StaticClass()->GetName());
}

void UProjectCinematicSubsystem::StampPanelEvents(ULevelSequence* Sequence)
{
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][StampPanel] No MovieScene on sequence; cannot stamp %d event(s)."),
			BufferedPanelEvents.Num());
		return;
	}

	int32 EventIdx = 0;
	int32 Stamped = 0;
	int32 Skipped = 0;
	for (const FBufferedPanelEvent& Ev : BufferedPanelEvents)
	{
		++EventIdx;
		UFunction* Fn = ResolveThunk(Ev.Panel, Ev.bVisible);
		if (!Fn)
		{
			UE_LOG(LogProjectCinematicEditor, Warning,
				TEXT("[CinematicSubsystem][StampPanel] [%d/%d] No thunk found for panel='%s' visible=%d -- skipping."),
				EventIdx, BufferedPanelEvents.Num(),
				*Ev.Panel.ToString(), Ev.bVisible ? 1 : 0);
			++Skipped;
			continue;
		}

		UMovieSceneEventTrack* MasterTrack = Cast<UMovieSceneEventTrack>(
			MovieScene->AddTrack(UMovieSceneEventTrack::StaticClass()));
		if (!MasterTrack)
		{
			UE_LOG(LogProjectCinematicEditor, Error,
				TEXT("[CinematicSubsystem][StampPanel] [%d/%d] MovieScene->AddTrack(UMovieSceneEventTrack) returned null; skipping panel='%s' visible=%d."),
				EventIdx, BufferedPanelEvents.Num(),
				*Ev.Panel.ToString(), Ev.bVisible ? 1 : 0);
			++Skipped;
			continue;
		}

		UMovieSceneEventTriggerSection* Section = CastChecked<UMovieSceneEventTriggerSection>(
			MasterTrack->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>::All());
		MasterTrack->AddSection(*Section);

		FMovieSceneEvent EventKey;
		EventKey.Ptrs.Function = Fn;
		Section->EventChannel.GetData().AddKey(Ev.FrameNumber, EventKey);
		++Stamped;

		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][StampPanel] [%d/%d] OK | panel='%s' visible=%d frame=%d thunk='%s'"),
			EventIdx, BufferedPanelEvents.Num(),
			*Ev.Panel.ToString(), Ev.bVisible ? 1 : 0,
			Ev.FrameNumber.Value, *Fn->GetName());
	}

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][StampPanel] Done | total=%d stamped=%d skipped=%d"),
		BufferedPanelEvents.Num(), Stamped, Skipped);
}

void UProjectCinematicSubsystem::StampHideOriginals(ULevelSequence* Sequence)
{
	// Write the editor-world placement actors (counterparts of the PIE actors
	// the player interacted with) as soft refs into UCinematicHideMetadata
	// attached to the produced ULevelSequence. ACinematicGameMode reads this
	// at Render-mode BeginPlay and calls SetActorHiddenInGame(true) on each.
	//
	// Why not Sequencer Possessable + bHidden bool track:
	//   - Spawnable + Possessable binding hierarchy works in editor preview
	//     (binding resolves through the current editor-world context).
	//   - But at MRQ render the editor placement is a WP external actor
	//     loaded into the PIE-duplicated world, and the Possessable binding
	//     does not reliably resolve through that path -- the track gets
	//     stamped fine but evaluates against a null binding, leaving the
	//     placement visible alongside the take's Spawnable duplicate
	//     ("double drawer" regression).
	//
	// Direct game-time SetActorHiddenInGame avoids all binding-resolution
	// gymnastics and works regardless of WP / placed / runtime spawn.
	if (!Sequence)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][StampHide] Null sequence; cannot persist %d hide target(s)."),
			EditorCounterpartsToHide.Num());
		return;
	}

	UCinematicHideMetadata* Metadata = Sequence->FindOrAddMetaData<UCinematicHideMetadata>();
	if (!Metadata)
	{
		UE_LOG(LogProjectCinematicEditor, Error,
			TEXT("[CinematicSubsystem][StampHide] FindOrAddMetaData<UCinematicHideMetadata> returned null on sequence '%s'."),
			*Sequence->GetPathName());
		return;
	}

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][StampHide] Begin | sequence='%s' targets=%d preExistingHideRefs=%d"),
		*Sequence->GetPathName(), EditorCounterpartsToHide.Num(),
		Metadata->ActorsToHide.Num());

	int32 Stamped = 0;
	int32 Skipped = 0;
	for (const TWeakObjectPtr<AActor>& WeakActor : EditorCounterpartsToHide)
	{
		AActor* EditorActor = WeakActor.Get();
		if (!EditorActor)
		{
			UE_LOG(LogProjectCinematicEditor, Warning,
				TEXT("[CinematicSubsystem][StampHide] Skipping null/stale TWeakObjectPtr entry."));
			++Skipped;
			continue;
		}

		const FSoftObjectPath Path(EditorActor);
		Metadata->ActorsToHide.AddUnique(Path);
		++Stamped;

		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][StampHide] Persisted | label='%s' path='%s' world='%s' worldType=%d"),
			*EditorActor->GetActorLabel(), *Path.ToString(),
			EditorActor->GetWorld() ? *EditorActor->GetWorld()->GetName() : TEXT("<null>"),
			EditorActor->GetWorld() ? static_cast<int32>(EditorActor->GetWorld()->WorldType) : -1);
	}

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][StampHide] Persisted refs | total=%d stamped=%d skipped=%d (sequence now carries %d hide ref(s))"),
		EditorCounterpartsToHide.Num(), Stamped, Skipped, Metadata->ActorsToHide.Num());

	// Soft refs are persisted. Now emit a single Event Track key at sequence
	// playback start that fires Cinematic_HideRecordedOriginals -- the
	// Director-side reader of UCinematicHideMetadata. Same Sequencer Event
	// Track pattern as panel events (parameterless thunk, Ptrs.Function
	// direct assignment).
	if (Stamped == 0)
	{
		return; // Nothing to fire on; skip emitting the Event Track.
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][StampHide] No MovieScene -- cannot stamp hide-event."));
		return;
	}

	UFunction* HideFn = UCinematicDirector::StaticClass()->FindFunctionByName(
		FName(TEXT("Cinematic_HideRecordedOriginals")));
	if (!HideFn)
	{
		UE_LOG(LogProjectCinematicEditor, Error,
			TEXT("[CinematicSubsystem][StampHide] UFunction 'Cinematic_HideRecordedOriginals' not found on UCinematicDirector."));
		return;
	}

	UMovieSceneEventTrack* MasterTrack = Cast<UMovieSceneEventTrack>(
		MovieScene->AddTrack(UMovieSceneEventTrack::StaticClass()));
	if (!MasterTrack)
	{
		UE_LOG(LogProjectCinematicEditor, Error,
			TEXT("[CinematicSubsystem][StampHide] AddTrack(UMovieSceneEventTrack) returned null -- cannot stamp hide-event."));
		return;
	}

	UMovieSceneEventTriggerSection* Section = CastChecked<UMovieSceneEventTriggerSection>(
		MasterTrack->CreateNewSection());
	Section->SetRange(TRange<FFrameNumber>::All());
	MasterTrack->AddSection(*Section);

	const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
	const FFrameNumber StartFrame = Range.HasLowerBound()
		? Range.GetLowerBoundValue()
		: FFrameNumber(0);

	FMovieSceneEvent EventKey;
	EventKey.Ptrs.Function = HideFn;
	Section->EventChannel.GetData().AddKey(StartFrame, EventKey);

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][StampHide] Hide-event stamped | thunk='%s' frame=%d"),
		*HideFn->GetName(), StartFrame.Value);
}

void UProjectCinematicSubsystem::StampFocusHighlights(ULevelSequence* Sequence)
{
	// State-mirror stamper. Walks BufferedFocusEvents -- every gameplay
	// focus transition that IInteractionService broadcast during the take
	// -- and writes Sequencer property tracks on bRenderCustomDepth that
	// reproduce that exact focus history on the take's Spawnable.
	//
	// Model:
	//   gameplay focus ON  drawer_2 at frame F1  -> bool true  key at F1
	//   gameplay focus OFF drawer_2 at frame F2  -> bool false key at F2
	//   gameplay focus ON  drawer_3 at frame F2  -> bool true  key at F2 on drawer_3
	//   ...
	// Each unique mesh component name gets ONE bool track + ONE section
	// spanning the entire playback range, with a key per transition that
	// affects that component. Section default=false so anything outside
	// recorded focus is dark.
	//
	// This produces an outline that turns on/off in render at EXACTLY the
	// same frames the player saw it during gameplay -- no arbitrary
	// 0.6s window, no fake timing patch.

	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][StampHL] No MovieScene; cannot stamp %d focus event(s)."),
			BufferedFocusEvents.Num());
		return;
	}

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][StampHL] Begin (state-mirror) | sequence='%s' focusEvents=%d spawnables=%d possessables=%d"),
		*Sequence->GetPathName(), BufferedFocusEvents.Num(),
		MovieScene->GetSpawnableCount(), MovieScene->GetPossessableCount());

	// Group events by (ActorLabel, ComponentName) tuple -- one bool track per
	// unique (actor, mesh) pair. Component names like "StaticMeshComponent_1"
	// are NOT globally unique across spawnables (every actor in the take can
	// have one), so grouping by component name alone produces a non-
	// deterministic stamp that picks the first matching possessable and
	// silently mis-attributes outlines to the wrong actor.
	//
	// Use TTuple<FString, FName> as the map key rather than a custom struct
	// to get GetTypeHash + operator== from UE's built-in TTuple
	// specialisation -- defining a local struct inside this member function
	// puts the friend-declared GetTypeHash out of ADL reach for TMap's
	// hashing instantiation in UE 5.7 (verified by build error at
	// SparseSet.h.inl:69 'KeyFuncs::KeyInitType').
	using FActorComponentKey = TTuple<FString, FName>;
	TMap<FActorComponentKey, TArray<const FBufferedFocusEvent*>> EventsByActorComponent;
	int32 EventsDroppedNoLabel = 0;
	for (const FBufferedFocusEvent& Ev : BufferedFocusEvents)
	{
		if (Ev.ComponentName == NAME_None) { continue; }
		if (Ev.ActorLabel.IsEmpty())
		{
			// Should not happen with the synthetic-OFF fix in HandleFocusChanged,
			// but defend against future regressions: an OFF with no actor label
			// can't be deterministically attributed to a spawnable, so drop it
			// with a warning rather than guess.
			++EventsDroppedNoLabel;
			continue;
		}
		EventsByActorComponent.FindOrAdd(FActorComponentKey(Ev.ActorLabel, Ev.ComponentName)).Add(&Ev);
	}
	if (EventsDroppedNoLabel > 0)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][StampHL] Dropped %d focus event(s) with empty ActorLabel -- cannot deterministically attribute to a spawnable."),
			EventsDroppedNoLabel);
	}

	if (EventsByActorComponent.Num() == 0)
	{
		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][StampHL] No focused (actor, component) pairs in buffer (only OFF-to-null transitions or label-less events). Nothing to stamp."));
		return;
	}

	// Helper: resolve an actor label to its spawnable (or actor-level
	// possessable) GUID in the MovieScene. Take Recorder names the spawnable
	// after Actor->GetActorLabel() at AddSource time -- the buffered focus
	// events carry the same label, so case-sensitive equals matches reliably.
	// Cache results because multiple (label, component) pairs from the same
	// actor are common.
	TMap<FString, FGuid> LabelToActorGuid;
	auto ResolveActorGuid = [&](const FString& Label) -> FGuid
	{
		if (const FGuid* Cached = LabelToActorGuid.Find(Label))
		{
			return *Cached;
		}
		FGuid Resolved;
		const int32 NumSpawnables = MovieScene->GetSpawnableCount();
		for (int32 i = 0; i < NumSpawnables; ++i)
		{
			const FMovieSceneSpawnable& S = MovieScene->GetSpawnable(i);
			if (S.GetName().Equals(Label, ESearchCase::CaseSensitive))
			{
				Resolved = S.GetGuid();
				break;
			}
		}
		// Take Recorder may auto-convert to Possessable when an actor doesn't
		// satisfy the spawnable check (placed actor, etc). Such actor-level
		// possessables have no Parent GUID -- check those too as a fallback.
		if (!Resolved.IsValid())
		{
			const int32 NumPossessables = MovieScene->GetPossessableCount();
			for (int32 i = 0; i < NumPossessables; ++i)
			{
				const FMovieScenePossessable& P = MovieScene->GetPossessable(i);
				if (!P.GetParent().IsValid() &&
					P.GetName().Equals(Label, ESearchCase::CaseSensitive))
				{
					Resolved = P.GetGuid();
					break;
				}
			}
		}
		LabelToActorGuid.Add(Label, Resolved);
		return Resolved;
	};

	int32 TracksStamped  = 0;
	int32 TracksSkipped  = 0;
	int32 KeysWritten    = 0;
	for (const TPair<FActorComponentKey, TArray<const FBufferedFocusEvent*>>& Pair : EventsByActorComponent)
	{
		// Pair.Key is TTuple<FString, FName>: .Key = ActorLabel, .Value = ComponentName.
		const FString& ActorLabel = Pair.Key.Key;
		const FName ComponentName = Pair.Key.Value;
		const TArray<const FBufferedFocusEvent*>& Events = Pair.Value;
		const FString TargetName = ComponentName.ToString();

		const FGuid ActorGuid = ResolveActorGuid(ActorLabel);
		if (!ActorGuid.IsValid())
		{
			UE_LOG(LogProjectCinematicEditor, Warning,
				TEXT("[CinematicSubsystem][StampHL] No spawnable/possessable matched actor label '%s' -- skipping (%d focus transitions on '%s')."),
				*ActorLabel, Events.Num(), *TargetName);
			++TracksSkipped;
			continue;
		}

		// Find the component possessable whose Parent GUID is THIS actor's
		// binding. Filtering by parent removes the cross-actor collision risk
		// when multiple spawnables share a child component name.
		FGuid ComponentGuid;
		const int32 NumPossessables = MovieScene->GetPossessableCount();
		for (int32 i = 0; i < NumPossessables; ++i)
		{
			const FMovieScenePossessable& Poss = MovieScene->GetPossessable(i);
			if (Poss.GetParent() == ActorGuid &&
				Poss.GetName().Equals(TargetName, ESearchCase::CaseSensitive))
			{
				ComponentGuid = Poss.GetGuid();
				break;
			}
		}
		if (!ComponentGuid.IsValid())
		{
			UE_LOG(LogProjectCinematicEditor, Warning,
				TEXT("[CinematicSubsystem][StampHL] No child possessable matched actor='%s' (guid=%s) component='%s' -- skipping (%d focus transitions)."),
				*ActorLabel, *ActorGuid.ToString(), *TargetName, Events.Num());
			++TracksSkipped;
			continue;
		}

		// One bool track per unique component for bRenderCustomDepth.
		UMovieSceneBoolTrack* Track = Cast<UMovieSceneBoolTrack>(
			MovieScene->AddTrack(UMovieSceneBoolTrack::StaticClass(), ComponentGuid));
		if (!Track)
		{
			UE_LOG(LogProjectCinematicEditor, Error,
				TEXT("[CinematicSubsystem][StampHL] AddTrack(UMovieSceneBoolTrack) returned null for component='%s' GUID=%s."),
				*TargetName, *ComponentGuid.ToString());
			++TracksSkipped;
			continue;
		}
		Track->SetPropertyNameAndPath(
			FName(TEXT("bRenderCustomDepth")),
			TEXT("bRenderCustomDepth"));

		// Section spans the entire playback range; channel default=false.
		// Each buffered focus transition for this component becomes one key
		// (true for focus-on, false for focus-off). The mesh's
		// bRenderCustomDepth flips at the exact frame the gameplay focus
		// changed -- outline matches gameplay timing exactly.
		UMovieSceneBoolSection* Section = CastChecked<UMovieSceneBoolSection>(
			Track->CreateNewSection());
		Section->SetRange(MovieScene->GetPlaybackRange());
		Section->GetChannel().SetDefault(false);

		int32 OnKeysThisTrack  = 0;
		int32 OffKeysThisTrack = 0;
		for (const FBufferedFocusEvent* Ev : Events)
		{
			Section->GetChannel().GetData().AddKey(Ev->Frame, Ev->bFocused);
			if (Ev->bFocused) { ++OnKeysThisTrack; } else { ++OffKeysThisTrack; }
			++KeysWritten;
			UE_LOG(LogProjectCinematicEditor, Verbose,
				TEXT("[CinematicSubsystem][StampHL]   key | component='%s' frame=%d value=%s"),
				*TargetName, Ev->Frame.Value, Ev->bFocused ? TEXT("true") : TEXT("false"));
		}
		Track->AddSection(*Section);
		++TracksStamped;

		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][StampHL] Track stamped | actor='%s' (actorGuid=%s) component='%s' (compGuid=%s) | keys=%d (on=%d off=%d)"),
			*ActorLabel, *ActorGuid.ToString(),
			*TargetName, *ComponentGuid.ToString(),
			Events.Num(), OnKeysThisTrack, OffKeysThisTrack);
	}

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][StampHL] Done (state-mirror) | uniqueActorComponents=%d tracksStamped=%d tracksSkipped=%d totalKeys=%d eventsDroppedNoLabel=%d"),
		EventsByActorComponent.Num(), TracksStamped, TracksSkipped, KeysWritten, EventsDroppedNoLabel);
}

void UProjectCinematicSubsystem::EnsureCameraCutTrack(ULevelSequence* Sequence)
{
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][CameraCut] No MovieScene; skipping."));
		return;
	}

	// Idempotent: Take Recorder's Player source variants may have already
	// added a CameraCutTrack with their own binding. Don't double-stamp.
	// UMovieScene::GetCameraCutTrack returns UMovieSceneTrack* (base) in
	// UE 5.7 -- cast to the typed pointer explicitly.
	if (UMovieSceneCameraCutTrack* Existing =
			Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()))
	{
		UE_LOG(LogProjectCinematicEditor, Display,
			TEXT("[CinematicSubsystem][CameraCut] Already present (sections=%d) -- skipping auto-add."),
			Existing->GetAllSections().Num());
		return;
	}

	// Resolve the camera spawnable by class-name match. Class-name string
	// avoids a hard module dep on CinematicCamera -- Take Recorder produces
	// either a CineCameraActor (default Player source) or a CameraActor
	// (older configurations) for the recorded POV. Both class names match
	// the substring "Camera". This is a deliberate string-based resolution
	// because the Build.cs already pulls in MovieScene + MovieSceneTracks
	// (where we need them for the cut track itself); pulling in
	// CinematicCamera would expand the editor module's surface for no
	// behavioural gain.
	FGuid CameraGuid;
	const int32 NumSpawnables = MovieScene->GetSpawnableCount();
	for (int32 i = 0; i < NumSpawnables; ++i)
	{
		const FMovieSceneSpawnable& S = MovieScene->GetSpawnable(i);
		const UObject* Template = S.GetObjectTemplate();
		const UClass* TemplateClass = Template ? Template->GetClass() : nullptr;
		if (!TemplateClass) { continue; }
		const FString ClassName = TemplateClass->GetName();
		// Match CineCameraActor, CameraActor, and any future *Camera*Actor
		// derivative Take Recorder might emit. Skip generic Actor matches.
		if (ClassName.Contains(TEXT("CameraActor")) ||
			ClassName.Contains(TEXT("CineCamera")))
		{
			CameraGuid = S.GetGuid();
			UE_LOG(LogProjectCinematicEditor, Display,
				TEXT("[CinematicSubsystem][CameraCut] Resolved camera spawnable | name='%s' class='%s' guid=%s"),
				*S.GetName(), *ClassName, *CameraGuid.ToString());
			break;
		}
	}

	if (!CameraGuid.IsValid())
	{
		UE_LOG(LogProjectCinematicEditor, Warning,
			TEXT("[CinematicSubsystem][CameraCut] No camera spawnable found (Take Recorder Player source absent or atypical class). MRQ render will fall back to gameplay view target -- recorded camera motion will NOT be visible. Verify Take Recorder added a Player source for the take."));
		return;
	}

	UMovieSceneCameraCutTrack* Track = Cast<UMovieSceneCameraCutTrack>(
		MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
	if (!Track)
	{
		UE_LOG(LogProjectCinematicEditor, Error,
			TEXT("[CinematicSubsystem][CameraCut] MovieScene->AddCameraCutTrack returned null."));
		return;
	}
	UMovieSceneCameraCutSection* Section = CastChecked<UMovieSceneCameraCutSection>(
		Track->CreateNewSection());
	const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
	Section->SetRange(PlaybackRange);
	// FRelativeObjectBindingID resolves at evaluation time relative to the
	// sequence playing the section -- correct for our root-level take.
	Section->SetCameraBindingID(UE::MovieScene::FRelativeObjectBindingID(CameraGuid));
	Track->AddSection(*Section);

	UE_LOG(LogProjectCinematicEditor, Display,
		TEXT("[CinematicSubsystem][CameraCut] Stamped | cameraGuid=%s rangeLo=%d rangeHi=%d"),
		*CameraGuid.ToString(),
		PlaybackRange.HasLowerBound() ? PlaybackRange.GetLowerBoundValue().Value : 0,
		PlaybackRange.HasUpperBound() ? PlaybackRange.GetUpperBoundValue().Value : 0);
}
