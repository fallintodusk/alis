// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameRate.h"
#include "ProjectCinematicSubsystem.generated.h"

class ASinglePlayController;
class UTakeRecorder;

/**
 * Editor-only subsystem orchestrating ALIS cinematic capture.
 *
 * Two responsibilities under one UCLASS (KISS - the data flow recording
 * -> stamping is tight enough that splitting is over-engineered at this
 * scope):
 *
 *  Recording side:
 *    - Subscribes to UTakeRecorderSubsystem::OnRecordingStartedEvent
 *      (fires during CountingDown - we chain to the recorder's own
 *      OnRecordingStarted() which fires when state=Started, verified by
 *      spike against engine source TakeRecorder.cpp:1010/1090)
 *    - At state=Started: snaps bRecordSourcesIntoSubSequences=false, hooks
 *      ASinglePlayController's observability delegates AND
 *      IInteractionService::OnFocusChanged.
 *    - On IInteractionService::OnFocusChanged (focus ON): calls
 *      UTakeRecorderSources::AddSource + StartRecordingSource on the
 *      focused actor (deduplicated via RecordedActors). This is the
 *      primary capture path -- the player just LOOKS at something and it
 *      becomes a Spawnable in the take. Required for StampFocusHighlights
 *      to find component possessables when stamping bRenderCustomDepth
 *      tracks.
 *    - On OnInteractionTriggered (E-press): same AddSource path (deduped).
 *      Covers the case where the player E-presses something that the
 *      focus broadcast hasn't already captured (e.g. interaction at point-
 *      blank range where focus + E fire on the same tick).
 *    - On OnPanelVisibilityChanged: buffers (Panel, bVisible, FrameNumber)
 *      for the stamping pass.
 *
 *  Stamping side:
 *    - On UTakeRecorder::OnRecordingFinished: assigns the produced
 *      LevelSequence's DirectorClass to UCinematicDirector::StaticClass()
 *      via FObjectProperty reflection (protected field on ULevelSequence),
 *      then walks the buffered panel events and emits one master Event
 *      Track + Trigger Section + FMovieSceneEvent per event, with
 *      Ptrs.Function pointing at the matching parameterless thunk on
 *      UCinematicDirector. No Director Blueprint asset, no
 *      FMovieSceneEventUtils, no BP compile pass -- pure C++ dispatch
 *      verified end-to-end by spike 2 (Sequencer Play + MRQ render both
 *      fire the function).
 */
UCLASS()
class UProjectCinematicSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// --- Take Recorder lifecycle ---
	void HandleSubsystemRecordingStarted(UTakeRecorder* Recorder);
	void HandleRecorderStateStarted(UTakeRecorder* Recorder);
	void HandleRecordingFinished(UTakeRecorder* Recorder);

	// --- Controller signal handlers ---
	void HandleInteractionTriggered(AActor* TargetActor, UActorComponent* RespondingComponent);
	void HandlePanelVisibilityChanged(FName PanelName, bool bVisible);

	// Add a Take Recorder ActorSource for the given actor (deduped via
	// RecordedActors). Returns true if a new source was added; false if
	// skipped (no active recorder, invalid actor, or already recorded).
	// Shared by E-press (HandleInteractionTriggered) and focus-driven
	// capture (HandleFocusChanged) so everything the player LOOKS AT or
	// interacts with becomes a possessable. StampFocusHighlights matches
	// focus events to possessables by component name, so the actor must
	// be a Take Recorder source BEFORE its components show up in the
	// MovieScene -- otherwise focus-only actors get their bool tracks
	// silently dropped with "No component possessable matched".
	bool AddRecorderSourceForActor(AActor* TargetActor);

	void SubscribeToController(UWorld* RecordWorld);
	void UnsubscribeFromController();

	// --- Stamper helpers ---
	void StampPanelEvents(class ULevelSequence* Sequence);
	void StampHideOriginals(class ULevelSequence* Sequence);
	void StampFocusHighlights(class ULevelSequence* Sequence);
	void AssignDirectorClass(ULevelSequence* Sequence) const;

	// Ensure the take has a CameraCutTrack pointing at its recorded camera
	// spawnable. Without one, MRQ render falls back to the gameplay view
	// target (the hidden phantom pawn at PlayerStart) instead of the camera
	// the player actually moved during the take. Idempotent: bails if a
	// CameraCutTrack already exists, or if no camera spawnable was found in
	// the take (Take Recorder didn't auto-add a Player source, etc).
	void EnsureCameraCutTrack(class ULevelSequence* Sequence);

	// --- Focus-state mirror (cinematic highlight source) ---
	void HandleFocusChanged(APawn* Instigator, AActor* FocusedActor, UPrimitiveComponent* FocusedComponent, FText Label);
	void SubscribeToFocusService();
	void UnsubscribeFromFocusService();
	FDelegateHandle FocusChangedHandle;

	// `IInteractionService::OnFocusChanged` only broadcasts the new focus
	// target -- it does NOT emit an explicit "focus off" for the previously-
	// focused component when focus moves from A to B. To get correct bool
	// track timing (drawer A's outline turns OFF at the same frame drawer B's
	// outline turns ON), we synthesise the OFF event ourselves by tracking
	// what was focused last. Cleared at recording start, updated on every
	// focus transition.
	//
	// We track BOTH the previous component name AND the previous actor label
	// because component names (e.g. "StaticMeshComponent_1") are NOT globally
	// unique across spawnables -- multiple actors in the same take can each
	// have a StaticMeshComponent_1. StampFocusHighlights must match on
	// (actor binding, component name) tuple, so we need both pieces of
	// identity here to emit a deterministic synthetic OFF.
	FName   PreviousFocusedComponentName = NAME_None;
	FString PreviousFocusedActorLabel;

	struct FBufferedPanelEvent
	{
		FName Panel = NAME_None;
		bool  bVisible = false;
		FFrameNumber FrameNumber;
	};

	/** Captured at each IInteractionService::OnFocusChanged broadcast during
	 *  the take. The stamper walks these in HandleRecordingFinished and
	 *  emits bRenderCustomDepth bool keys on the matching spawnable
	 *  component bindings. State-mirror model: every focus ON / focus OFF
	 *  transition becomes a key, so the render outline appears for exactly
	 *  the same frames the player saw the outline in gameplay -- no
	 *  arbitrary "0.6s window" timing. */
	struct FBufferedFocusEvent
	{
		FString      ActorLabel;            // owning actor's GetActorLabel(); empty if FocusedActor was null
		FName        ComponentName = NAME_None; // visual mesh FName; NAME_None if FocusedComponent was null
		bool         bFocused = false;      // true = focus ON for this component, false = focus OFF (cleared)
		FFrameNumber Frame;                 // frame of the transition in the active sequence
		FFrameRate   FrameRate;             // sequence tick rate (debug)
	};

	// Subscriber state
	FDelegateHandle SubsystemStartHandle;
	TWeakObjectPtr<ASinglePlayController> SubscribedController;
	FDelegateHandle InteractionHandle;
	FDelegateHandle PanelHandle;

	// Active take state
	TWeakObjectPtr<UTakeRecorder> ActiveRecorder;
	bool bSourcesSettingsApplied = false;
	TSet<TWeakObjectPtr<AActor>> RecordedActors;
	// Editor-world counterparts of PIE actors we recorded. Stamped as
	// Possessable bindings + Visibility track (hidden=true) over the
	// sequence range so the take's spawned duplicate is the only dresser
	// visible at MRQ render time. Population uses
	// EditorUtilities::GetEditorWorldCounterpartActor at AddSource time.
	TSet<TWeakObjectPtr<AActor>> EditorCounterpartsToHide;
	TArray<FBufferedPanelEvent> BufferedPanelEvents;
	TArray<FBufferedFocusEvent> BufferedFocusEvents;
};
