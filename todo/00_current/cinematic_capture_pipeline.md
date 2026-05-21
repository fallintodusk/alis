# Cinematic Capture Pipeline (Take Recorder runtime AddSource)

Status: implementation under review (latest log recheck 2026-05-21).
Architecture pivoted four times before landing on this one; each pivot
was the user pushing toward simpler. Final design uses an
engine-internal pattern (`UTakeRecorderSources::AddSource` mid-recording,
the same mechanism `UTakeRecorderNearbySpawnedActorSource` uses
internally) to make Take Recorder capture interacted-actor transforms
natively. No transform baking. No DirBP for visible motion. Only a
tiny event-track stamper for UI panels. ACinematicGameMode owns WP
streaming so designers don't manage cinematic-actor layer flags.

Goal: PIE-record a take with Take Recorder. Without opening Sequencer
or doing any per-take manual work, MRQ render produces trailer footage
where every recorded interaction visibly happens at the correct frame
and every UI panel toggle visibly happens at the correct frame.

## 2026-05-21 latest log recheck

Source log: `Saved/Logs/Alis.log`, take recorded at
`2026.05.20-20.46.40` through `20.46.57`, MRQ render at
`20.47.12` through `20.49.50`.

What the log proves:
- [OK] Focus-driven `AddSource` works for looked-at actors. Four actors
  were recorded by the focus path: `Dresser_Classic`,
  `Wardrobe_Classic`, `WardrobeSlider3`, and `Loot_CardboardBox`.
- [OK] Deterministic highlight stamping works for this take. The
  stamper wrote 29 keys across 7 `(actor, component)` pairs with
  `tracksSkipped=0` and `eventsDroppedNoLabel=0`.
- [OK] Component-name collisions are separated by actor binding in the
  stamped result. Example from this take:
  `Dresser_Classic.StaticMeshComponent_2` and
  `Wardrobe_Classic.StaticMeshComponent_2` resolved to different
  component GUIDs.
- [OK] Camera ownership did not hit the fallback path. The take already
  had a Camera Cut Track: `[CameraCut] Already present (sections=1)`.

What the log does NOT prove:
- [!] It does not prove every focused actor was actually interacted
  with. `DispatchInteract` happened only for `Dresser_Classic` (4
  times) and `WardrobeSlider3` (2 times). There is no
  `DispatchInteract` for `Wardrobe_Classic` or `Loot_CardboardBox` in
  the recording window. Those two actors were focus/AddSource/highlight
  only in this take.
- [!] It does not prove render-side evaluation of the stamped
  `bRenderCustomDepth` tracks. It proves the tracks were authored and
  saved, not that MRQ visually evaluated them as intended.
- [!] It does not prove live focus is harmless during MRQ. Render-mode
  logs show the live `InteractionComponent` still writing
  `SetComponentCustomDepth` repeatedly from `20.47.39` through
  `20.49.26`, mostly on `Dresser_Classic.StaticMeshComponent_2`, then
  `Wardrobe_Classic`, then `WardrobeSlider3`. That means Sequencer and
  live gameplay can still both write the same stencil state during
  render.
- [!] It does not prove whether render live focus is hitting the take
  spawnables or the hidden editor counterparts. The current log prints
  labels and component names, not object paths, hidden state, or binding
  GUIDs.

Immediate interpretation:
- Do not treat "focus/AddSource happened" as "opening happened".
  Motion comes from gameplay state that actually changed during
  recording. Focus-only actors can have source bindings and highlight
  tracks while still having no open/search action.
- The other-agent verdict "architecture is correct end-to-end" is too
  strong. The focus stamper path is much healthier, but interaction
  coverage and render-side highlight ownership are still open.
- The likely next fault line is ownership: Record mode can let gameplay
  own live highlight; Render mode should probably let Sequencer own
  `bRenderCustomDepth`. If the prompt must remain visible in MRQ, split
  prompt broadcast from stencil writes instead of keeping all live
  interaction visuals active.

Comparison tests required before declaring done:
1. **Interaction matrix test.** Record one controlled take and press E
   on `Dresser_Classic`, `Wardrobe_Classic`, `WardrobeSlider3`, and
   `Loot_CardboardBox`. For each pressed actor, verify:
   `Focus ON`, `AddSource OK`, `DispatchInteract`, capability/actor
   response log, and expected transform or UI result in Sequencer.
2. **Focus-only negative test.** Look at an actor without pressing E.
   Expected: `AddSource OK` and highlight keys may exist, but there
   must be no claim that gameplay motion happened unless a gameplay
   response log or transform change exists.
3. **Same-component collision test.** Force
   `Wardrobe_Classic.StaticMeshComponent_1 ->
   Dresser_Classic.StaticMeshComponent_1` and verify the synthetic OFF
   includes the previous actor label and stamps distinct component GUIDs.
4. **Render stencil ownership test.** Render the same take twice:
   first with current live interaction visuals, then with render-mode
   live stencil writes suppressed but prompt/HUD left alone if needed.
   Compare whether highlights still appear inside the wrong actors or
   stick to repeated live focus targets.
5. **Render diagnostic log.** Add a temporary render-only audit that
   logs focused actor object path, `bHiddenInGame`, component path, and
   `bRenderCustomDepth` for the target component. The current label-only
   log cannot prove whether live focus is touching a hidden original or
   a visible take spawnable.
6. **Sequencer-vs-MRQ visual proof.** Capture frames around every
   stamped ON/OFF key and compare Sequencer preview against MRQ output.
   A stamp log alone is not enough for visual correctness.

Open implementation debt from this round:
- `ProjectCinematicSubsystem.cpp` has grown past the repo mega-file
  guardrail. Before finalizing, split focused-source capture,
  highlight stamping, and camera-cut stamping into SRP-consistent helper
  files or get explicit exception approval.
- The doc below still describes the original interaction-triggered
  AddSource architecture in places. Current code is focus-driven first,
  E-press is a deduped safety net. Keep this section as the latest SOT
  until the long-form design text is fully reconciled.

## KISS vector (the direction)

Every decision in this doc was made against this priority order:

1. **Sequencer is the literal SOT.** The produced LevelSequence asset
   must self-describe the entire cinematic. Open it, scrub it, see
   everything, render is just consumption. If render is needed to
   verify, the architecture is wrong.
2. **Use UE primitives. Do not reinvent.** Take Recorder is built to
   capture transforms; let it. Event Tracks are built to fire UI
   actions; let them. No custom Tick proxies, no clock-drift fixes, no
   transform-baking math.
3. **Zero per-take manual work.** Once-per-project setup is acceptable
   (scaffold the plugin, define the C++ Director class). Per-recording
   designer steps are not. Press Record, play, press Stop -> done.
4. **Zero cinematic-only UFUNCTIONs on gameplay code.** The gameplay
   path during recording is the real gameplay path. Take Recorder
   samples its visible result. No `CinematicToggle`, no
   `CinematicInteract`, no anti-cheat bypass UFUNCTIONs. Gameplay
   plugins do not know cinematics exist.
5. **Code-side over designer-side when feasible.** WP streaming for
   cinematic actors is owned by `ACinematicGameMode`, not by per-actor
   Always-Loaded flags. Self-contained pipeline.

History of pivots (each forced a simplification, each was correct):
- Round 1: dropped runtime Tick proxy -> Event-Track-only.
- Round 2: dropped DirBP-for-everything -> hybrid (transforms + UI events).
- Round 3: dropped transform-baking math -> runtime AddSource (Take
  Recorder captures real motion natively).
- Round 4: dropped designer Always-Loaded WP layer -> gamemode-owned
  streaming source.
- Round 5 (this): dropped Director Blueprint asset -> pure C++
  `UCinematicDirector` class. No .uasset committed, no BP compile path,
  no signature validator (compile errors catch drift), no
  `FMovieSceneEventUtils` indirection.

Net result: small recorder + small UI-only stamper + one C++ class
(`UCinematicDirector`, ~30 LOC). Gameplay-side touches: widen one
delegate, fix one NAME_None to deterministic, add CallInEditor metadata
on three existing UFUNCTIONs. That is the entire surface delta.

## The single proof shot

> In City17 cinematic sublevel, walk to `Dresser_Classic_Inst7`, press
> E on `drawer_mid`, press I (inventory open), wait 1s, press I (close),
> stop Take Recorder. Open the produced LevelSequence in Sequencer.
> See: Possessable for the dresser, child Possessable for the drawer
> mesh component, transform track on the drawer mesh with sampled keys
> matching the real spring motion the player saw, master Event Track
> at sequence root with two trigger keys for the panel toggles. Scrub
> the timeline -> drawer slides at recorded frames. Press Play in
> Sequencer -> UI panel toggles fire. MRQ render -> output MP4 matches
> the Sequencer playback frame-for-frame.

The Sequencer asset is the SOT. Render is consumption.

## How it works in practice (plain English)

### Once-per-project setup (4 things, done once, then forgotten)

1. Plugin `Plugins/Editor/ProjectCinematic` is enabled in the editor.
   It defines a C++ `UCinematicDirector` class (one UFUNCTION,
   ~30 LOC) and the recorder/stamper subsystems. No content assets.
2. `ACinematicGameMode` is set as the GameMode override on the
   cinematic sublevel's `WorldSettings` (or on the MRQ preset).
3. The cinematic sublevel placed actors have deterministic names
   (one tiny code change in `AInteractableActor` made this automatic;
   no per-actor designer work needed).
4. ALIS-default Take Recorder settings + project settings already
   include `RelativeLocation` in the recorded properties; one-time
   verification.

### Recording a take (designer workflow, fully automatic)

1. Designer opens the cinematic sublevel, opens Take Recorder, presses
   Start. Take Recorder begins capturing the player's pawn + camera as
   usual.
2. Designer plays through the scene: walks up to the dresser, presses
   E on a drawer (the drawer slides open in real gameplay - this is
   the actual `USpringSliderComponent` spring sim), presses I to open
   inventory, etc.
3. Behind the scenes (no designer action needed): each focus ON
   broadcast is the primary editor-recorder trigger. The recorder calls
   `Sources->AddSource(FocusedActor) + StartRecordingSource()` ONE TIME
   per actor. E-press uses the same path as a deduped safety net. From
   the very next engine tick, Take Recorder samples that actor's
   transform every frame INCLUDING every child component (the drawer
   mesh that the spring is animating). If gameplay later opens/closes
   that actor, the real motion - the spring's actual settle curve, with
   whatever physics and damping the gameplay produced - is captured
   per-frame as transform keys.
4. UI panel toggles (which have no spatial transform to record) are
   buffered separately with their app-time offset.
5. Designer presses Stop. Take Recorder finalizes a LevelSequence
   asset with: pawn + camera tracks (default), dresser Possessable +
   all 5 drawer child Possessables + transform tracks with REAL
   sampled motion. Immediately after, our stamper runs once: it
   assigns `LevelSequence->DirectorClass = UCinematicDirector::StaticClass()`,
   adds master Event Track keys at the recorded panel frames with each
   `FMovieSceneEvent::Ptrs.Function` pointing directly at
   `UCinematicDirector::CinematicSetPanelVisible` and payload variables
   set to `{Panel, bVisible}`, saves. No BP compile step.

### What designer sees after Stop

Open the produced LevelSequence in Sequencer. The outliner shows:
- Camera Cuts track (from Take Recorder)
- Player pawn binding + transform/anim/audio tracks
- For each actor the player interacted with: a Possessable binding,
  with child Possessable bindings for each of its components, each
  with a transform track containing the real sampled motion
- A master Event Track at the sequence root with one trigger key per
  UI panel toggle, each pointing at
  `UCinematicDirector::CinematicSetPanelVisible` with payload
  `(Panel, bVisible)`

Designer drags the playhead (scrub): the drawer slides in the
viewport at the recorded frame. The drawer's spring overshoot/settle
that the player saw is faithfully captured. UI events do not fire on
scrub (engine limitation - by design); designer presses Play in
Sequencer to verify UI events.

### What designer can edit

Anything. The transform tracks are real keys: change timing, modify
positions, retime, key-reduce, copy/paste between takes. Delete a
binding the designer doesn't want. The asset is self-describing; the
designer is not locked into a particular interpretation.

### What happens at MRQ render time

MRQ loads the LevelSequence, plays it through. `ACinematicGameMode`
is active, which spawns the WP streaming source so all needed cells
stay loaded. Transform tracks evaluate -> drawer slides. Event Tracks
fire -> Director endpoint calls `SetPanel_InventoryVisible(true)` on
the live PC -> inventory panel becomes visible. MRQ composites UMG into
the rendered frame.

Current caveat: render-mode live `InteractionComponent` focus can still
write `bRenderCustomDepth` while Sequencer is also evaluating stamped
highlight tracks. That makes the old rule "if render is wrong, the
Sequencer asset is wrong first" incomplete for highlights. For stencil
highlights, MRQ correctness requires a single owner: either Sequencer
owns custom depth during render, or live focus must be proven to touch
only invisible/non-rendered counterparts.

## Architecture

```
PIE recording, Take Recorder armed and recording:

Player looks at drawer_mid of Dresser_Classic_Inst7
  -> IInteractionService broadcasts
     OnFocusChanged(PlayerPawn, Dresser_Classic_Inst7, drawer_mid_Mesh, "Open")
  -> UProjectCinematicSubsystem (editor subsystem in ProjectCinematic) receives signal:
       UTakeRecorder* Rec = UTakeRecorder::GetActiveRecorder();
       UTakeRecorderSources* Sources = ...;
       Sources->Settings.bRecordSourcesIntoSubSequences = false; // root only, no sub-assets
       UTakeRecorderActorSource* Src = Sources->AddSource<UTakeRecorderActorSource>();
       Src->Target = FocusedActor;
       Src->bRecordParentHierarchy = false;
       Src->PostEditChangeProperty(FPropertyChangedEvent(Target, ValueSet)); // rebuild prop map
       Sources->StartRecordingSource({Src}, Sources->GetCachedFrameTime());
  -> next engine tick: Take Recorder samples the dresser + every
     USceneComponent child (RelativeLocation on the drawer mesh that
     USpringSliderComponent is currently animating)
  -> if the player later presses E and gameplay opens/closes this actor,
     the already-added source captures that real motion
  -> as the spring sim runs frame-by-frame in gameplay, Take Recorder
     writes one transform key per sample -> the REAL spring motion
     ends up in the LevelSequence

Player presses E on drawer_mid of Dresser_Classic_Inst7
  -> ASinglePlayController broadcasts
     OnInteractionTriggered(Dresser_Classic_Inst7, responding component)
  -> UProjectCinematicSubsystem calls the same AddSource helper
  -> usually dedupes because focus already recorded the actor

Player presses I (inventory open)
  -> OnPanelVisibilityChanged(Inventory, true) broadcast
  -> UProjectCinematicSubsystem buffers (Panel=Inventory, bVisible=true, FrameNumber=F)
  -> (later, on RecordingFinished) stamper writes a master Event Track
     trigger at this frame pointing at
     UCinematicDirector::CinematicSetPanelVisible

Stop recording:
  - Take Recorder finalizes:
      - Player source: pawn + camera + audio (default)
      - Each dynamically-added actor source: actor Possessable + child
        component Possessables + transform tracks with real sampled motion
  - Stamper post-pass (UProjectCinematicSubsystem, editor subsystem):
      - Assign LevelSequence->DirectorClass = UCinematicDirector::StaticClass()
      - For each buffered UI panel event:
          add binding-less master UMovieSceneEventTrack to MovieScene
          add UMovieSceneEventTriggerSection
          create FMovieSceneEvent with Ptrs.Function pointing at
              UCinematicDirector::CinematicSetPanelVisible
          set PayloadVariables {Panel, bVisible}
          AddKey at the recorded frame
      - Save sequence (no BP compile pass needed)
```

## Why this is the right primitive (verified UE 5.7 source)

- `UTakeRecorderSources::AddSource` has zero guards against mid-record
  invocation. Just appends to `TArray<UTakeRecorderSource*>`. Source:
  `TakeRecorderSources.cpp:60-78`.
- Tick reads `Sources.Num()` each frame; new entries pick up next tick.
  Source: `TakeRecorderSources.cpp:405-411`.
- `UTakeRecorderActorSource` class comment: *"Records the properties
  of the actor and the components on the actor and safely handles new
  components being spawned at runtime."* Source:
  `TakeRecorderActorSource.h:33-35`.
- Epic's own `UTakeRecorderNearbySpawnedActorSource::HandleActorSpawned`
  uses this exact two-call mid-record pattern. Source:
  `TakeRecorderNearbySpawnedActorSource.cpp:202`.
- Child component transform capture is automatic via
  `RebuildRecordedPropertyMapRecursive` -> `GetSceneComponents` walks
  every USceneComponent in the actor's tree. Source:
  `TakeRecorderActorSource.cpp:1287-1297, 1483-1492`.
- Property selection comes from `UTakeRecorderProjectSettings`;
  `RelativeLocation` is in the default record set for `USceneComponent`.

## Non-negotiable invariants

- Gameplay plugins MUST NOT depend on the cinematic plugin. One-way.
- Cinematic plugin is editor-only (`Type=Editor, LoadingPhase=PostEngineInit`).
- No `Alis*` prefix in reusable code.
- File-size guardrail: each new `.cpp` stays under 700 LOC.
- ZERO cinematic-only UFUNCTIONs on gameplay code. The runtime
  AddSource path means no `CinematicToggle`, no `CinematicInteract` -
  Take Recorder captures the existing gameplay-driven motion verbatim.

## Plugin layout

```
Plugins/Editor/ProjectCinematic/
  ProjectCinematic.uplugin                 Type=Editor, LoadingPhase=PostEngineInit
  Source/
    ProjectCinematic/
      Public/
        CinematicDirector.h                ULevelSequenceDirector C++ subclass, ONE UFUNCTION
        ProjectCinematicModule.h
      Private/
        CinematicDirector.cpp              ~30 LOC, body of CinematicSetPanelVisible
      ProjectCinematic.Build.cs
    ProjectCinematicEditor/
      Public/
        ProjectCinematicSubsystem.h        UEditorSubsystem, recorder + stamper orchestration
      Private/
        ProjectCinematicSubsystem.cpp      focus-driven AddSource, panel/hide/highlight/camera stamping
        CinematicDirector.cpp              director thunk implementation
      ProjectCinematicEditor.Build.cs
```

No `Content/` folder. No .uasset committed. No BP authoring step.

Build.cs private dependencies (editor-only):
- `Core`, `CoreUObject`, `Engine`, `Slate`, `EditorSubsystem`, `UnrealEd`
- `LevelSequence` (`ULevelSequenceDirector` base class, `ULevelSequence` type)
- `MovieScene`, `MovieSceneTracks` (`UMovieSceneEventTrack`,
  `UMovieSceneEventTriggerSection`, `FMovieSceneEvent`)
- `TakeRecorder`, `TakesCore` (`UTakeRecorder`, `UTakeRecorderSources`)
- `TakeTrackRecorders` (`UTakeRecorderActorSource`)
- `ProjectSinglePlay` (`ACinematicGameMode`, controller signals)
- `ProjectObject` (`AInteractableActor` UCLASS reference)

NOTE: `MovieSceneTools`, `BlueprintGraph`, `KismetCompiler`, `Kismet`
are NO LONGER needed (we don't use BP compile path; no
`FMovieSceneEventUtils`, no `FKismetEditorUtilities::CompileBlueprint`).

## Director (pure C++ with parameterless thunks)

LOAD-BEARING CORRECTION (round-6 research, 2026-05-20):
`FMovieSceneEvent::PayloadVariables` is `#if WITH_EDITORONLY_DATA`
(`MovieSceneEvent.h:80-84`). It is stripped from cooked builds and is
NEVER read by the runtime evaluator. The BP compiler consumes it at
compile time to bake literal values into a generated function body. A
direct-payload stamp at runtime would fire the function with a memzero'd
parameter buffer (Panel=NAME_None, bVisible=false) -- silent skip, no
crash, no log warning.

The fix: bake payload at C++ compile time as parameterless thunks per
(Panel, bVisible) combination. Stamper picks by name. No payload
variables needed at runtime. Direct `Ptrs.Function` assignment works.

```cpp
// Public/CinematicDirector.h
#pragma once
#include "LevelSequenceDirector.h"
#include "CinematicDirector.generated.h"

UCLASS()
class UCinematicDirector : public ULevelSequenceDirector
{
    GENERATED_BODY()
public:
    // Parameterless thunks (one per Panel x bVisible combination).
    // Sequencer Event Track trigger keys call these by UFunction name.
    // Each thunk forwards to the shared C++ helper below with baked literals.
    UFUNCTION(BlueprintCallable, CallInEditor) void Cinematic_OpenInventory();
    UFUNCTION(BlueprintCallable, CallInEditor) void Cinematic_CloseInventory();
    UFUNCTION(BlueprintCallable, CallInEditor) void Cinematic_OpenVitals();
    UFUNCTION(BlueprintCallable, CallInEditor) void Cinematic_CloseVitals();
    UFUNCTION(BlueprintCallable, CallInEditor) void Cinematic_OpenMindJournal();
    UFUNCTION(BlueprintCallable, CallInEditor) void Cinematic_CloseMindJournal();

private:
    // Shared logic. Resolves the live PC from the director's world.
    void SetPanelVisible(FName Panel, bool bVisible);
};
```

```cpp
// Private/CinematicDirector.cpp
void UCinematicDirector::SetPanelVisible(FName Panel, bool bVisible)
{
    UWorld* World = GetWorld();
    if (!World) return;
    auto* PC = Cast<ASinglePlayController>(
        UGameplayStatics::GetPlayerController(World, 0));
    if (!PC) return;
    if      (Panel == TEXT("Inventory"))   PC->SetPanel_InventoryVisible(bVisible);
    else if (Panel == TEXT("Vitals"))      PC->SetPanel_VitalsVisible(bVisible);
    else if (Panel == TEXT("MindJournal")) PC->SetPanel_MindJournalVisible(bVisible);
}

void UCinematicDirector::Cinematic_OpenInventory()    { SetPanelVisible(TEXT("Inventory"),   true);  }
void UCinematicDirector::Cinematic_CloseInventory()   { SetPanelVisible(TEXT("Inventory"),   false); }
void UCinematicDirector::Cinematic_OpenVitals()       { SetPanelVisible(TEXT("Vitals"),      true);  }
void UCinematicDirector::Cinematic_CloseVitals()      { SetPanelVisible(TEXT("Vitals"),      false); }
void UCinematicDirector::Cinematic_OpenMindJournal()  { SetPanelVisible(TEXT("MindJournal"), true);  }
void UCinematicDirector::Cinematic_CloseMindJournal() { SetPanelVisible(TEXT("MindJournal"), false); }
```

Six thunks, ~25 LOC total. Adding a panel: add one private method on
the controller (already exists today) + two new thunks + one if branch.

Stamper picks the right UFunction name by (Panel, bVisible) lookup:

```cpp
static UFunction* ResolveThunk(FName Panel, bool bVisible)
{
    static const TMap<TTuple<FName, bool>, FName> ThunkMap = {
        { {TEXT("Inventory"),   true},  TEXT("Cinematic_OpenInventory")    },
        { {TEXT("Inventory"),   false}, TEXT("Cinematic_CloseInventory")   },
        { {TEXT("Vitals"),      true},  TEXT("Cinematic_OpenVitals")       },
        { {TEXT("Vitals"),      false}, TEXT("Cinematic_CloseVitals")      },
        { {TEXT("MindJournal"), true},  TEXT("Cinematic_OpenMindJournal")  },
        { {TEXT("MindJournal"), false}, TEXT("Cinematic_CloseMindJournal") },
    };
    const FName* Found = ThunkMap.Find({Panel, bVisible});
    if (!Found) return nullptr;
    return UCinematicDirector::StaticClass()->FindFunctionByName(*Found);
}
```

No validator. No README. No signature drift class. If a thunk is ever
removed, `FindFunctionByName` returns nullptr at stamp time -- log
loudly and skip the key.

### Watch for: `PostDuplicate` wipes DirectorClass

`LevelSequence.cpp:319-336` resets `DirectorClass = nullptr` when an
asset is duplicated with no `DirectorBlueprint`. Doesn't affect us for
freshly-stamped takes (we don't duplicate). If any future flow
duplicates the asset (e.g. "Save as new take"), the duplicator must
re-stamp `DirectorClass`.

## Gameplay-side additions (minimal)

Three changes only. No cinematic-only UFUNCTIONs. No bake providers.

### 1. Widen `FOnSinglePlayInteractionTriggered`

`Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlayClient/Public/SinglePlayController.h`:
```cpp
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FOnSinglePlayInteractionTriggered,
    AActor*           /* TargetActor */,
    UActorComponent*  /* RespondingComponent (capability that responded) */);
```
Thread `Selected` (from `AInteractableActor::OnInteract_Implementation:226`)
into the broadcast. The cinematic recorder reads the actor for AddSource;
the component pointer is mostly for diagnostics (Take Recorder captures
the whole actor + all children regardless).

### 2. Deterministic capability component names

`Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp:578`:
replace `NAME_None` with `FName(*FString::Printf(TEXT("Cap_%s_%s"), *CapEntry.Type.ToString(), *ScopeStr))`.

Effect: drawer slider becomes `Cap_Sliding_drawer_mid`. Take Recorder
Possessable bindings use component path; deterministic names = stable
bindings across PIE recording and MRQ render. Same reasoning still
applies even though Take Recorder authors the bindings (it uses the
runtime component name).

### 3. `CallInEditor` on existing `SetPanel_*Visible` UFUNCTIONs

Three existing UFUNCTIONs in `SinglePlayController.h` gain
`meta=(CallInEditor="true")`. PIE-in-Sequencer preview needs this;
MRQ render is game-world (does not strictly need it) but the metadata
is free.

## Runtime AddSource flow (canonical C++)

In `UProjectCinematicSubsystem::AddRecorderSourceForActor`:

```cpp
UTakeRecorder* Recorder = ActiveRecorder.Get();
if (!Recorder || !bSourcesSettingsApplied || !TargetActor) { return false; }
if (RecordedActors.Contains(TargetActor)) { return false; }

ULevelSequence* Sequence = Recorder->GetSequence();
UTakeRecorderSources* Sources = Sequence->FindMetaData<UTakeRecorderSources>();
if (!Sequence || !Sources) { return false; }

UTakeRecorderActorSource* Src = Sources->AddSource<UTakeRecorderActorSource>();
if (!Src) { return false; }
Src->Target = TargetActor;
Src->bRecordParentHierarchy = false;
Src->RecordType = ETakeRecorderActorRecordType::Spawnable;
FPropertyChangedEvent Evt(
    UTakeRecorderActorSource::StaticClass()->FindPropertyByName(
        GET_MEMBER_NAME_CHECKED(UTakeRecorderActorSource, Target)),
    EPropertyChangeType::ValueSet);
Src->PostEditChangeProperty(Evt);  // rebuilds RecordedProperties map incl. all child components

Sources->StartRecordingSource({ Src }, Sources->GetCachedFrameTime());
RecordedActors.Add(TargetActor);   // commit only after StartRecordingSource
```

This helper is called from focus ON first and from E-press second. The
E-press call should usually dedupe; `DispatchInteract` remains the proof
that gameplay action actually fired.

## UI panel stamper (minimal, pure C++)

In `UProjectCinematicSubsystem::HandleRecordingFinished`:

```cpp
// Assign the C++ Director class to the sequence. No BP, no compile pass.
LevelSequence->DirectorClass = UCinematicDirector::StaticClass();

UMovieScene* MS = LevelSequence->GetMovieScene();

for (const FBufferedPanelEvent& Ev : Recorder->GetPanelEvents())
{
    UFunction* Fn = ResolveThunk(Ev.Panel, Ev.bVisible);  // see Director section
    if (!Fn) { UE_LOG(LogProjectCinematic, Warning, ...); continue; }

    UMovieSceneEventTrack* MasterTrack = Cast<UMovieSceneEventTrack>(
        MS->AddTrack(UMovieSceneEventTrack::StaticClass()));  // no GUID = master/root
    UMovieSceneEventTriggerSection* Section = CastChecked<UMovieSceneEventTriggerSection>(
        MasterTrack->CreateNewSection());
    Section->SetRange(TRange<FFrameNumber>::All());
    MasterTrack->AddSection(*Section);

    FMovieSceneEvent Key;
    Key.Ptrs.Function = Fn;
    // Ptrs.BoundObjectProperty stays nullptr - UI events have no bound object.
    // NO PayloadVariables: thunk function is parameterless; literal Panel +
    // bVisible are baked into the C++ thunk body at compile time. This
    // bypasses the editor-only PayloadVariables -> Parameters marshaling
    // path that does not exist at runtime (PayloadVariables is
    // #if WITH_EDITORONLY_DATA, never read by MovieSceneEventSystems.cpp).

    Section->EventChannel.GetData().AddKey(Ev.FrameNum, Key);
}

SaveSequence(LevelSequence);
```

Compared to the BP path this skips: duplicate-into-outer, compile, find
entry node, `BindEventSectionToBlueprint`, `SetEndpoint`, second
compile. ~6 API calls -> ~2. At runtime, the Sequencer evaluator reads
`Ptrs.Function` (set directly by us) and dispatches via
`DirectorInstance->ProcessEvent(Fn, memzero_parameters)`. The thunk has
no parameters, so the memzero'd buffer is irrelevant; the function
body uses baked literals.

That is the entire stamper. No transform tracks, no per-actor adapters,
no capability-specific code.

## Files removed in this slice

| File | Why |
|---|---|
| `ProjectSinglePlayClient/Public/Cinematic/CinematicProxy.h` + .cpp | Tick proxy replaced |
| `ProjectSinglePlayClient/Public/Cinematic/CinematicEventRecorder.h` + .cpp | Moved + rewritten in ProjectCinematic |
| `ProjectSinglePlayClient/Public/Cinematic/CinematicTakeStamper.h` + .cpp | Moved + rewritten (now UI-only) |
| `FCinematicEvent` struct | Replaced by `FBufferedPanelEvent` |

`ProjectSinglePlayClient.Build.cs` loses: `LevelSequence`, `MovieScene`,
`EditorSubsystem`, `Json`, `JsonUtilities`, `LevelSequenceEditor`,
`MovieSceneTracks`, `TakeRecorder`, `UnrealEd`.

## What stays unchanged

- `ACinematicGameMode` (gameplay mode flip, input blocking, phantom-pawn hide)
- `ASinglePlayController` observability delegates (widening only)
- `ILookInputModifier` + `LookSensitivityScale`
- MRQ preset scripts
- All gameplay code paths during recording: `USpringSliderComponent` ticks
  normally and produces real spring motion, which Take Recorder samples
  per-frame. Nothing in gameplay code needs to know about cinematics.

## World Partition mitigation (code-side, in ACinematicGameMode)

Verified UE 5.7 (round 4, 2026-05-20): the only runtime-capable
mechanism is `UWorldPartitionStreamingSourceComponent`. `PinActors` is
editor-only. `DisableStreamingIn` only blocks new cells, doesn't
force-load. MRQ does NOT pre-stream the camera trajectory (engine:
`MoviePipelineRendering.cpp:635` blocks per frame on already-active
streaming sources). So our gamemode-side streaming source is the
authoritative mechanism for both PIE-recording AND MRQ-render.

`ACinematicGameMode::BeginPlay` (Render and Record profiles both):

```cpp
UWorld* World = GetWorld();
if (!World || !World->IsGameWorld()) return;

FActorSpawnParameters P;
P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
CinematicStreamingHost = World->SpawnActor<AActor>(
    AActor::StaticClass(), CinematicTrajectoryCenter, FRotator::ZeroRotator, P);
CinematicStreamingHost->SetActorHiddenInGame(true);

UWorldPartitionStreamingSourceComponent* Src =
    NewObject<UWorldPartitionStreamingSourceComponent>(CinematicStreamingHost);
FStreamingSourceShape Shape;
Shape.bUseGridLoadingRange = false;
Shape.Radius = CinematicStreamingRadiusCm;   // UPROPERTY default 100000.f (1km)
Shape.bIsSector = false;
Src->Shapes.Add(Shape);
Src->Priority = EStreamingSourcePriority::Highest;
Src->RegisterComponent();                    // auto-registers with subsystem
```

Cleanup on `EndPlay` destroys `CinematicStreamingHost`. Same code path
for PIE-recording and MRQ-render. No designer-side per-actor flag
needed; the gamemode owns it.

UPROPERTIES on `ACinematicGameMode`:
- `FVector CinematicTrajectoryCenter = FVector::ZeroVector` (designer-set per cinematic sublevel)
- `float CinematicStreamingRadiusCm = 100000.f` (1km default; tune per shot)

`TargetState` on `UWorldPartitionStreamingSourceComponent` defaults to
`Activated` -- correct for Possessable binding resolution at MRQ
render. (The field is private; default is correct, no override needed
in v1.)

Trade-offs documented:
- Wider radius = more memory. 1km default acceptable for indoor
  trailer scenes; bump for outdoor wide shots.
- HLODs may stream in via the same source (empty `TargetGrids` =
  affects all grids). v2 can populate `TargetGrids` to exclude HLODs.
- Per-shot autotuning (read camera trajectory bbox from LevelSequence
  to set Center+Radius) is v2; v1 uses the static UPROPERTY values.

## Spike gates (run BEFORE full implementation)

Two architectural gates need empirical proof in ALIS context before
the full pipeline is written. Each spike lives in a deletable
`Source/.../Private/Spikes/` folder. Both: under 1 hour each.

### Spike 2 first (Gate 2: pure C++ Director, ~30 min)

Smallest possible test of "native ULevelSequenceDirector subclass +
direct Ptrs.Function assignment fires at the recorded frame, in
Sequencer Play AND MRQ render."

Files: `SpikeDirector.h/.cpp` (tiny native subclass with one
parameterless `UFUNCTION SpikeFire()`), `SpikeDirectorAuthor.cpp`
(editor subsystem with console command `Spike.Director.AuthorSequence`
that creates a LevelSequence asset, sets DirectorClass to the spike
subclass, adds one master Event Track + Trigger Section + key pointing
at SpikeFire, saves).

Procedure: run console command -> open the produced asset in Sequencer
-> Press Play -> see log line. Then open MRQ -> add the asset -> render
-> see same log line in render output.

PASS: log line appears in BOTH Sequencer Play AND MRQ render.

FAIL: log line missing in either, OR inspector shows "Function not
bound" warning, OR `Ptrs.Function` does not survive SavePackage.

FALLBACK if fail: set `Key.CompiledFunctionName = TEXT("SpikeFire")`
in addition to `Ptrs.Function`. If still failing, revert to
hand-authored DirBP_ProjectCinematic.uasset with native parent +
`FMovieSceneEventUtils::BindNewUserFacingEvent`. Add back the
`MovieSceneTools`, `BlueprintGraph`, `KismetCompiler`, `Kismet` build
deps. Document as "Round 5b: BP wrapper fallback".

### Spike 1 second (Gate 1: mid-record AddSource, ~60 min)

Smallest possible test of "calling AddSource on a live UTakeRecorder
mid-recording produces a working Possessable + transform track in the
output LevelSequence."

Files: `SpikeAddSourceSubsystem.cpp` (editor subsystem listening for
console command `Spike.AddSource.Trigger`). Spike map: blank PIE map
with one placed `AStaticMeshActor` named `SpikeCube_A`.

Procedure: open map, Take Recorder, press Record, drag the cube
manually for 2s (to prove pre-AddSource motion isn't captured), run
console command (AddSource fires), drag cube further for 3s, press
Stop. Open produced asset.

PASS: Sequencer shows Possessable for `SpikeCube_A` + child component
+ transform track with multiple keys spanning post-AddSource motion.
Scrubbing visibly moves the cube.

FAIL: missing binding / missing transform track / single key only at
AddSource frame / sub-sequence sub-asset created despite
bRecordSourcesIntoSubSequences=false / crash inside StartRecording.

FALLBACK if fail: try `Src->StartRecording(...)` directly instead of
the public `Sources->StartRecordingSource({Src}, ...)` wrapper.
If still fails: revert to Round 4 architecture (pre-stamper that adds
known cinematic targets as Take Recorder sources at the START of
recording from a level scan). Documented as fallback in
`cinematic_capture_pipeline.md` history.

Both spikes write deletable code. After both pass:
`git rm -r Plugins/Editor/ProjectCinematic/Source/ProjectCinematic/Private/Spikes/`
and proceed with full implementation.

## Verification gates (after spikes pass)

1. Compile clean.
2. Open `UTakeRecorderProjectSettings`; verify `USceneComponent::RelativeLocation`
   is in the default record list (one-time project-settings check).
3. PIE-record the single proof shot (top of doc).
4. Open the produced LevelSequence in Sequencer. SEE the dresser
   Possessable + drawer mesh child Possessable + transform track with
   sampled keys + master Event Track for UI events. Inspector confirms
   trigger keys resolve to `UCinematicDirector::CinematicSetPanelVisible`
   (function name visible in the trigger key details).
5. SCRUB the timeline -> drawer slides at recorded frames in viewport.
6. Press Play in Sequencer -> UI panel toggles fire.
7. MRQ render -> output MP4 matches Sequencer playback frame-for-frame.
8. Re-record same actors -> second take stamps cleanly; first take's
   bindings do not leak into the second take's sequence.
9. UE 5.4 subsequence regression -> verify Event Tracks on Take
   Recorder subsequences fire in UE 5.7 MRQ.
10. Interaction/focus comparison gate:
    - every actor the player presses E on has `DispatchInteract`
    - every actor expected to move has a gameplay response log and
      transform-key evidence
    - every actor only looked at is marked focus-only and must not be
      counted as an opened/searched actor
11. Highlight comparison gate:
    - every gameplay focus ON/OFF has a matching stamped key
    - every stamped key is checked visually in Sequencer preview
    - the same frames are checked in MRQ output
    - render live focus does not write competing custom-depth state on
      visible actors during those frames
12. Camera gate:
    - either Take Recorder authored a Camera Cut Track, or
      `EnsureCameraCutTrack` stamped one
    - no `[CameraCut] No camera spawnable found` warning appears
    - MRQ POV matches the recorded player camera movement

If gate 4 passes but 5 does not, Take Recorder did not capture child
component transforms. Re-check project settings + verify `bRecordParentHierarchy`
+ `PostEditChangeProperty` are in the AddSource call.

## Risks / open

- **`EnsureObjectTemplateHasComponent` collision bug** (forum-reported):
  affects some dynamically-spawned components, notably Niagara FX and
  spline-spawned actors. Our drawer mesh is created during
  `ApplyDefinition` at BeginPlay (before recording starts) so the
  drawer exists when AddSource fires - likely safe but flag for gate 4
  verification.
- **Sub-sequence sub-asset spam**: default `bRecordSourcesIntoSubSequences=true`
  would create one sub-asset per AddSource. Recorder MUST set this
  to `false` on the first interaction (one-time per recording).
- **Multi-toggle takes**: Take Recorder samples at engine tick rate
  (~60Hz). Multiple opens/closes of same drawer within one take are
  captured natively; no special handling.
- **UE 5.4 MRQ subsequence regression**: forum-reported. Validate
  against 5.7 explicitly on first MRQ render attempt.
- **Idempotent re-record**: each Take Recorder run produces a new
  LevelSequence asset. No cross-take binding overlap by construction.
  `RecordedActors` set inside the recorder prevents double-AddSource
  for the same actor within ONE take.
- **Focus is not interaction**: focus-driven AddSource is correct for
  early capture and highlight attribution, but it does not mean the
  actor opened/searched. `DispatchInteract` plus capability response
  logs are the proof of actual gameplay action.
- **Render live-stencil contention**: latest MRQ log shows live
  `InteractionComponent` writes `SetComponentCustomDepth` during render.
  This can fight Sequencer bool tracks. Preferred next design is to
  suppress live stencil writes in Render mode while keeping prompt/HUD
  behavior separate if the shot needs it.
- **Unbalanced final focus**: if recording stops while focus is still
  on an actor, the final bool track can remain true to the end of the
  playback range. This mirrors gameplay state, but MRQ handle frames
  may expose it. Decide whether to stamp a terminal OFF at sequence end
  or keep strict state-mirror behavior.
- **Label-only diagnostics**: current logs cannot distinguish a hidden
  editor original from a visible take spawnable during render. Add
  object path, hidden state, and world/type to render focus logs before
  making another conclusion about highlight leakage.

## Why the rewrite (architecture history)

The first attempt stored events in a runtime `TArray<FCinematicEvent>`
on `ACinematicProxy` and fired them from `Tick()` reading
`ULevelSequencePlayer::GetCurrentTime()`. Cinematic truth was invisible
in Sequencer.

The second attempt (Event Track + DirBP wrapping gameplay UFUNCTIONs):
Sequencer asset would contain event triggers but no spatial state.
Visible motion would be computed at playback time by gameplay code.
Scrub-preview did not work. The Sequencer asset was not truly SOT.

The third attempt (stamper bakes transform tracks from spring config):
Sequencer would have transform keys but the spring motion would be a
2-key approximation, not the real motion. Stamper would carry
capability-specific code; abstraction would need bake-provider
interfaces; anim-duration math would need closed-form spring solving.
All to approximate motion that Take Recorder is already capable of
sampling literally.

This (round 3) architecture: Take Recorder is already designed to
capture transforms over time. The only missing piece was "add the
interacted actor as a source at the moment of interaction" - which
the engine already supports via the same code path
`UTakeRecorderNearbySpawnedActorSource` uses internally for
spawn-driven additions. No baking. No approximation. No
capability-specific code. Real motion captured per-frame. Sequencer
is the literal SOT.

## References

- Engine source (UE 5.7 at `<ue-path>/`):
  - `Engine/Plugins/VirtualProduction/Takes/Source/TakesCore/Private/TakeRecorderSources.cpp:60-78, 301-312, 405-411`
    (AddSource has no mid-record guard; StartRecordingSource exists; tick iterates Sources)
  - `Engine/Plugins/VirtualProduction/Takes/Source/TakeTrackRecorders/Private/TrackRecorders/MovieSceneActorTrackRecorder.cpp`
    (component-recursion)
  - `Engine/Plugins/VirtualProduction/Takes/Source/TakeMovieScene/Private/Sources/TakeRecorderActorSource.cpp:33-35, 1287-1297, 1483-1492`
    (class doc, child-component capture)
  - `Engine/Plugins/VirtualProduction/Takes/Source/TakeRecorderSources/Private/TakeRecorderNearbySpawnedActorSource.cpp:202`
    (Epic-internal use of the same two-call pattern)
  - `Engine/Source/Runtime/MovieScene/Public/MovieScene.h:648` (`AddTrack` no-GUID for UI master)
  - `Engine/Source/Editor/MovieSceneTools/Public/MovieSceneEventUtils.h:54, 72, 115`
    (`BindEventSectionToBlueprint`, `SetEndpoint`)
  - `Engine/Source/Runtime/Engine/Classes/Components/WorldPartitionStreamingSourceComponent.h:16`
    + `Private/Components/WorldPartitionStreamingSourceComponent.cpp:25-41`
    (auto-registers on game world)
  - `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionSubsystem.h:96`
    (`RegisterStreamingSourceProvider`)
  - `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionStreamingSource.h:94-106, 217-221, 343`
    (`FStreamingSourceShape`, `EStreamingSourceTargetState`)
  - `Engine/Plugins/MovieScene/MovieRenderPipeline/Source/MovieRenderPipelineCore/Private/MoviePipelineRendering.cpp:635`
    (MRQ blocks per frame on existing streaming sources; does not author its own)
- Epic docs:
  - Take Recorder, Record Gameplay
  - Cinematic Event Track
  - World Partition, World Partition Streaming Source Component
- Community:
  - Marvel Rivals trailer pipeline @ Unreal Fest 2024 (different
    pipeline -- replay-based -- but confirms Take Recorder is the
    backbone)
- ALIS docs:
  - `CLAUDE.md` (public-repo migration policy, no-Alis-prefix rule)
  - `docs/agents/canonical.md` section 3 ("Use UE engine primitives.
    Do not reinvent.")
