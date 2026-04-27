# Interaction Targeting: Proximity Gather + Aim-Scored Focus

**Status:** Slices 1-4 implemented locally. Slice 1 landed 2026-04-25;
priority/hysteresis, server resolver parity, and cleanup landed 2026-04-26.
`TraceDistance` and `TraceRadius` were removed because ALIS interaction
targeting is C++-owned and no Blueprint/map/JSON migration path exists.
Build/test verification is recorded in the session notes.
**Priority:** Major (UX + architectural alignment)
**Date opened:** 2026-04-24
**Trigger:** user pain - "pixel hunting" when hovering the reticle on small
interactable objects to get the highlight / prompt. The current detection is
a zero-radius camera ray (`LineTraceSingleByChannel`) against `ECC_Visibility`,
so only the exact pixel under the crosshair can trigger focus. Research
(Lyra, Epic docs, industry consensus) recommends a proximity overlap + aim
scoring pattern for non-combat interactions, with line-of-sight kept as a
separate gate. See `canonical.md` engine primitives guidance.

> **Note on name.** Earlier draft was "Replace Camera Line Trace with Overlap
> + Score". That was misleading -- we still need a line trace, just for the
> line-of-sight gate, not for picking candidates. The current title
> describes the actual change.

---

## Goal

1. Replace the single camera `LineTraceSingleByChannel` in
   `UInteractionComponent::UpdateTrace` with:
   - one `OverlapMultiByObjectType` sphere around the pawn / camera
     (candidate gather against object channels that can carry
     interactables -- WorldDynamic / PhysicsBody / Pawn),
   - a per-candidate **line-of-sight gate run BEFORE scoring** using the
     existing `TraceChannel` (default `ECC_Visibility`),
   - a deterministic aim-scoring pass with explicit clamps and
     tiebreakers to pick ONE winner per tick.
2. Extract a **pure** resolver (no side effects) that both the client
   tick and the server RPC re-validation call. `UpdateTrace` keeps the
   "apply focus" side effects; the server validator calls only the
   resolver and never touches highlight state.
3. Keep the interaction pipeline downstream of `SetFocusedActor` **unchanged**
   (label, highlight mesh, hold progress, service broadcast, execution spec,
   tests using `TestOnly_SetFocusedActor`) -- the refactor is only about
   "how we choose the focused actor," not "what we do with it."
4. Stay inside `ProjectInteraction`'s current module graph (no new plugin
   deps). **Do not introduce new public framework contracts, plugin-level
   abstractions, or ProjectCore interfaces.** Private helper structs,
   functions, and responsibility-named helper classes inside
   `ProjectInteraction`'s `Private/` implementation are allowed.
5. Preserve the hover-outline fix from 2026-04-24 (tick retry of
   `SetupPostProcess` after dynamic camera creation).

### Not in scope

- Gameplay-ability-based interaction options (Lyra's
  `UInteractionAbility` / `InteractionOption` concept). We keep the current
  synchronous `IInteractableTarget::OnInteract` / `IInteractable-
  ComponentTargetInterface::OnComponentInteract` surface.
- Aim-assist for shooting -- unrelated, stays line trace.
- Changing the `IInteractableTarget` / `IInteractableComponentTargetInterface`
  contracts. All existing implementers continue to work.
- Net-replication changes. Current `Server_TryInteract` server re-trace is
  untouched except the server also uses the new query (see slice 3).
- A new ProjectCore interface. Existing interfaces are sufficient.

---

## Context (what already exists -- reuse, do not reinvent)

All helpers below live in `Plugins/Gameplay/ProjectInteraction/` and stay
as-is:

| Building block | Location | Role |
|---|---|---|
| `FInteractionFocusInfo` | [IInteractableTarget.h:69-83](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IInteractableTarget.h#L69-L83) | Label + highlight mesh contract |
| `FInteractionExecutionSpec` | [IInteractableTarget.h:42-62](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IInteractableTarget.h#L42-L62) | Hold duration / active label / cancel-on-release |
| `IInteractableTargetInterface` | [IInteractableTarget.h:112-133](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IInteractableTarget.h#L112-L133) | Actor-level interact / focus / spec |
| `IInteractableComponentTargetInterface` | [IInteractableTarget.h:167-232](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IInteractableTarget.h#L167-L232) | Component-level priority / label / mesh |
| `GatherInteractableComponents` | [InteractionComponent.cpp:82-104](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L82-L104) | Sort components by `GetInteractPriority()` |
| `SelectBestInteractableComponent` | [InteractionComponent.cpp:106-159](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L106-L159) | Mesh-scoped then actor-scoped selection |
| `ResolveFocusFromComponents` | [InteractionComponent.cpp:161-186](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L161-L186) | Build `FInteractionFocusInfo` |
| `ResolveInteractionExecutionSpecFromComponents` | [InteractionComponent.cpp:188-208](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L188-L208) | Build `FInteractionExecutionSpec` |
| `SetFocusedActor` | [InteractionComponent.cpp:365-464](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L365-L464) | Focus change + custom-depth toggle + broadcast. Sole call site of everything above. |
| `IInteractionService::BroadcastFocusChanged` | [IInteractionService.h:185](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IInteractionService.h) | HUD prompt hook (unchanged) |
| `TestOnly_SetFocusedActor` / `TestOnly_ExecuteInteraction` | [InteractionComponent.h:66-79](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Public/InteractionComponent.h#L66-L79) | Deterministic test hooks (unchanged) |

### Current UE-native primitives we use (do not reinvent)

- `UWorld::OverlapMultiByObjectType` (returns `TArray<FOverlapResult>`; each
  result carries `Actor`, `Component`, `ItemIndex`). We use this for
  gather, NOT `OverlapMultiByChannel`. See "Channel correctness" below.
- `FCollisionObjectQueryParams{ ECC_WorldDynamic, ECC_PhysicsBody, ECC_Pawn }`
  -- object-type query for gather.
- `FCollisionShape::MakeSphere(Radius)`.
- `FCollisionQueryParams` with `AddIgnoredActor(Owner)` -- already used by
  the current `UpdateTrace` at [InteractionComponent.cpp:362](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L362).
- `LineTraceSingleByChannel(TraceChannel, ...)` for the line-of-sight gate
  ONLY. `ECC_Visibility` stays a trace channel; walls keep blocking it.
- `FVector::DotProduct` for forward-vs-candidate angle scoring.
- `UClass::ImplementsInterface` -- engine-native; used for filtering without
  per-actor hard deps. Matches the "interface-first" rule in
  [docs/architecture/plugin_rules.md](../../docs/architecture/plugin_rules.md)
  "Composition Ownership vs Consumption" section.

### Channel correctness (blocker fix, slice 1)

UE separates **Trace Channels** (trace queries answer "what does this beam
hit?") from **Object Channels** (object queries answer "what objects are
inside this volume?"). `OverlapMultiByChannel(ECC_Visibility, ...)` is
legal but semantically wrong for a gather: it asks "what would a visibility
trace overlap," which depends on every actor's per-object response to
the Visibility channel -- not something interactables are configured for.

Lyra's reference interaction query works because Lyra ships a dedicated
`Interaction` trace channel and targets overlap it. We do not ship that
channel. Two paths:

- **Slice 1 (KISS, no config migration):** gather with
  `OverlapMultiByObjectType` against `{ WorldDynamic, PhysicsBody, Pawn }`.
  Interactable meshes are already in one of those object types today --
  this works out of the box. Filter by `IInteractableTargetInterface` /
  `IInteractableComponentTargetInterface` right after gather.
- **Slice 4 (optional):** add a dedicated `Interaction` trace channel in
  `Config/DefaultEngine.ini` and have interactable meshes overlap it.
  Lets you query a narrower set and ignore scene clutter. Not required
  by slice 1 -- only useful if slice 2/3 shows the interface filter has
  meaningful overhead.

Line-of-sight gate keeps using `TraceChannel` (default `ECC_Visibility`)
-- that is correct because walls already block Visibility traces.

### Design heritage (Lyra-style, not a direct port)

Epic's Lyra interaction task does: sphere `OverlapMulti` around the pawn,
filter by a target interface, collect options, let the client pick. Our
project does not ship Lyra (verified -- not under `<ue-path>/`)
so this plan adapts the **pattern** without copying Lyra types. We keep
our current synchronous `OnInteract` / priority model.

Canonical guidance this obeys:
- "Use UE engine primitives. Do not reinvent." -- `docs/agents/canonical.md`
  section 3.
- "Systems vs World... Features self-contained; consumers only via
  interfaces." -- `docs/architecture/plugin_rules.md`.
- Mega-file rule: `InteractionComponent.cpp` is currently ~884 LOC. The
  refactor keeps it under 1000 by extracting the scorer to a namespace
  helper alongside the existing anonymous helpers
  ([InteractionComponent.cpp:17-245](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L17-L245)).

---

## Design

### Data flow (new)

**Critical ordering:** line-of-sight gate runs BEFORE scoring so a hidden
high-scored candidate cannot knock out a visible lower-scored one.

```
World timer (respects TraceIntervalSeconds)
  |
  v
[Gather]   OverlapMultiByObjectType(
  |            sphere at ViewOrigin, radius = InteractionRadius,
  |            { ECC_WorldDynamic, ECC_PhysicsBody, ECC_Pawn },
  |            AddIgnoredActor(Owner))
  |        Returns TArray<FOverlapResult>.
  |
  v
[Filter]   For each FOverlapResult, build one FInteractionCandidate per
  |        unique actor, KEEPING the FOverlapResult's Component as the
  |        candidate's HitComponent. Actor qualifies if:
  |          Actor.Implements<IInteractableTargetInterface>  OR
  |          any ActorComponent implements IInteractableComponentTargetInterface
  |        When multiple overlap results hit the same actor, keep the one
  |        whose Component matches an IInteractableComponentTargetInterface
  |        mesh; fall back to closest overlap point.
  |        Then resolve and cache Candidate.TargetPoint via
  |        ResolveInteractionTargetPoint(Actor, Component, ViewOrigin)
  |        so aim gate, LOS gate, and distance score share one stable
  |        point. Mesh pivot is unsafe (can be inside walls or below
  |        floor); closest collision point is the contract.
  |
  v
[Aim gate] Per-candidate vector and dot use the cached TargetPoint:
  |          ToTarget = (Candidate.TargetPoint - ViewOrigin)
  |          Candidate.Distance = ToTarget.Size()
  |          Candidate.AimDot   = FVector::DotProduct(
  |                                  ToTarget.GetSafeNormal(),
  |                                  ViewForward)
  |        Drop candidates with AimDot < MinAimDot. ShortCircuitRadius
  |        does NOT bypass this; you cannot focus something behind your
  |        head.
  |
  v
[LOS gate] For each remaining candidate, one LineTraceSingleByChannel
  |        from ViewOrigin to Candidate.TargetPoint using TraceChannel
  |        (default ECC_Visibility). Drop candidates blocked by geometry.
  |        Bypass the LOS gate (only) when Candidate.Distance
  |        <= ShortCircuitRadius, so you can focus an object you are
  |        literally touching. Aim gate above is still enforced.
  |
  v
[Score]    Per-candidate scalar score with explicit clamps (see formula
  |        in "Scoring" block below). Tiebreak: higher priority, then
  |        smaller distance, then stable actor id for determinism.
  |
  v
[Resolve]  Winner.Actor + Winner.HitComponent -> existing
  |        SelectBestInteractableComponent to pick the specific
  |        mesh/component inside that actor.
  |        Existing ResolveFocusFromComponents /
  |        ResolveInteractionExecutionSpecFromComponents build FocusInfo
  |        + ExecutionSpec exactly as today.
  |
  v
SetFocusedActor(Winner.Actor, Winner.HitComponent)   <-- UNCHANGED from here down
```

Key property: one `OverlapMultiByObjectType` + up to N short
`LineTraceSingleByChannel` (N = interface-filtered candidates,
realistically <= 5). Same order of magnitude as today's one line trace,
still throttled by `TraceIntervalSeconds = 0.15` (about 6.7 Hz).

### Scoring formula (deterministic)

```cpp
// Per-candidate values
const float AimScore =
    FMath::Clamp((Cand.AimDot - MinAimDot) / (1.0f - MinAimDot), 0.0f, 1.0f);

const float DistanceScore =
    1.0f - FMath::Clamp(Cand.Distance / InteractionRadius, 0.0f, 1.0f);

const float PriorityScore =
    FMath::Clamp(static_cast<float>(Cand.Priority) / 100.0f, 0.0f, 1.0f);

Cand.Score =
    AimWeight      * AimScore      +
    DistanceWeight * DistanceScore +
    PriorityWeight * PriorityScore;
```

`Cand.Priority` comes from the best
`IInteractableComponentTargetInterface::GetInteractPriority()` found on
the actor (docs say "100+ = access control, 0-50 = actions", so dividing
by 100 keeps the normalized value in a sane range without clamping typical
values).

Tiebreak order when scores are within `FLT_EPSILON`:

1. higher raw `Priority`
2. smaller `Distance`
3. stable `Actor->GetUniqueID()` -- deterministic, platform-stable.

### Focus hysteresis (jitter guard, slice 2)

```cpp
if (CurrentFocusedActor.IsValid()
    && CurrentFocusedScore >= NewBestScore * (1.0f - FocusSwitchHysteresis))
{
    // keep current focus; do not switch
}
```

Default `FocusSwitchHysteresis = 0.10f` (10 %). Prevents flicker between
two similarly-scored candidates as the player nudges the reticle.

### New tunables (added as UPROPERTY on UInteractionComponent)

Single group of properties under `Category = "Interaction"`. No new config
files. No new subsystem.

| Property | Type | Current default | Meaning |
|---|---|---:|---|
| `InteractionRadius` | float | 300.0 | Sphere radius around the pawn. |
| `ShortCircuitRadius` | float | 60.0 | Within this distance, bypass LOS gate only (still subject to aim gate). |
| `MinAimDot` | float | 0.85 | Minimum dot(forward, to-target). Always enforced, cannot be bypassed. |
| `AimWeight` | float | 1.0 | Scoring weight for aim alignment. |
| `DistanceWeight` | float | 0.25 | Scoring weight for proximity. |
| `PriorityWeight` | float | 0.5 | Scoring weight for component priority. |
| `FocusSwitchHysteresis` | float | 0.10 | Jitter guard. Keep current focus unless new score beats current by > 10 %. |

Existing:
- `TraceChannel` kept, same semantics (used ONLY by the LOS line trace,
  NOT by the overlap gather).
- `TraceIntervalSeconds` is `0.15` by default for responsive passive local
  focus refresh. Input refreshes focus immediately before interaction.
- `bEnableHighlight`, `OutlineMaterial`, `bPostProcessReady`, etc. --
  untouched (the post-process fix landed 2026-04-24).

### Candidate struct and pure resolver (preserve component identity)

Gather must NOT collapse to `TArray<AActor*>` -- `FOverlapResult` carries
the specific `UPrimitiveComponent*` that overlapped, and we pass that
component into `SelectBestInteractableComponent` downstream. Collapsing
early loses mesh-scoped selection inside actors with multiple
interactable components.

```cpp
// Private to InteractionComponent.cpp (sits near existing anonymous
// helpers; not exposed via the header).
//
// Raw pointers intentional: this struct is stack/local only, never a
// UPROPERTY, never serialized, never kept across frames, not ownership.
// Epic guidance: TObjectPtr is for member variables (reflected UObjects);
// local variables and function parameters use raw pointers.

struct FInteractionCandidate
{
    AActor*                Actor       = nullptr;
    UPrimitiveComponent*   Component   = nullptr; // from FOverlapResult
    FVector                TargetPoint = FVector::ZeroVector; // resolved aim/LOS target
    float                  Distance    = 0.0f;
    float                  AimDot      = -1.0f;
    int32                  Priority    = 0;
    float                  Score       = 0.0f;
};

struct FInteractionScoringWeights
{
    float MinAimDot;
    float AimWeight;
    float DistanceWeight;
    float PriorityWeight;
    float InteractionRadius;
    float ShortCircuitRadius;
};

// Resolves a stable point on the candidate's collision volume that we
// trace to (LOS) and direction-test against (AimDot). Mesh pivot is
// unreliable -- it can sit far from visible geometry, inside a wall,
// or below the floor. We prefer the closest collision point along the
// ray from the view origin, falling back to the bounds origin and
// finally the actor location.
//
// Pure helper, called once per surviving candidate during the filter
// pass before the aim gate. Result is cached on FInteractionCandidate
// so the aim gate, LOS gate, and distance scoring all share the same
// stable target.
//
// Epic API note: UPrimitiveComponent::GetClosestPointOnCollision is
// const and returns linear distance (negative on failure, e.g. when
// the component has no valid simple collision). The squared-distance
// variant is GetSquaredDistanceToCollision -- do not confuse them.

static FVector ResolveInteractionTargetPoint(
    const AActor*               Actor,
    const UPrimitiveComponent*  Component,
    const FVector&              ViewOrigin)
{
    if (Component)
    {
        FVector ClosestPoint = Component->Bounds.Origin;

        const float Distance =
            Component->GetClosestPointOnCollision(ViewOrigin, ClosestPoint);

        if (Distance >= 0.0f)
        {
            return ClosestPoint;
        }

        return Component->Bounds.Origin;
    }

    return Actor ? Actor->GetActorLocation() : ViewOrigin;
}

// Pure: reads world state, returns a candidate. No side effects on
// FocusedActor / HighlightMesh / Service broadcasts. Callable from
// both client tick AND server RPC validation.
bool ResolveBestInteractionTarget(
    UWorld*                               World,
    const FVector&                        ViewOrigin,
    const FVector&                        ViewForward,
    ECollisionChannel                     LineOfSightChannel,
    AActor*                               IgnoreActor,
    const FInteractionScoringWeights&     Weights,
    FInteractionCandidate&                OutWinner);
```

`UpdateTrace` (client tick) becomes a thin wrapper:

```cpp
// Compute ViewOrigin / ViewForward from PlayerCameraManager (client).
FInteractionCandidate Winner;
if (ResolveBestInteractionTarget(GetWorld(), View.Origin, View.Forward,
        TraceChannel, GetOwner(), BuildWeights(), Winner))
{
    // Apply hysteresis (slice 2+), then:
    SetFocusedActor(Winner.Actor, Winner.Component);
}
else
{
    SetFocusedActor(nullptr, nullptr);
}
```

Server authority uses the SAME pure resolver with a **server-sourced
view** (see "Server view source" under Slice 3) and never touches
highlight state:

```cpp
// ExecuteInteraction_ServerAuth
FInteractionCandidate Winner;
if (ResolveBestInteractionTarget(GetWorld(), ServerView.Origin,
        ServerView.Forward, TraceChannel, GetOwner(), BuildWeights(), Winner))
{
    // Execute directly, no SetFocusedActor on server.
    IInteractableTargetInterface::Execute_OnInteract(
        Winner.Actor, GetOwner(), Winner.Component);
}
```

Implementation note (2026-04-25): slice 1 landed with private helper
types `FInteractionTargetResolver` and `FInteractionCapabilitySelector`
under `ProjectInteraction/Private/`. `UpdateTrace` is now a short
orchestrator and the component TU stays below the mega-file guardrail.

### Debug observability (mandatory in slice 1)

Manual verification alone is not enough -- a single mis-authored mesh
collision or wrong object channel will silently strip a candidate, and
without instrumentation the symptom is "highlight just doesn't appear"
with no signal about which gate dropped the candidate. We add two
opt-in dev hooks under the existing `LogInteraction` category, both
zero-cost when disabled.

**1. Console variable to toggle structured logs:**

```cpp
static TAutoConsoleVariable<int32> CVarInteractionDebug(
    TEXT("alis.Interaction.Debug"),
    0,
    TEXT("Enable interaction targeting debug logs (overlap, filter, aim, LOS, winner counts)."),
    ECVF_Default);
```

**2. One Verbose log per resolver invocation when enabled:**

```cpp
if (CVarInteractionDebug.GetValueOnGameThread() != 0)
{
    UE_LOG(LogInteraction, Verbose,
        TEXT("[InteractionTargeting] overlaps=%d candidates=%d aimRejected=%d losRejected=%d winner=%s score=%.3f distance=%.1f aim=%.3f"),
        NumOverlaps,
        NumCandidates,
        NumAimRejected,
        NumLosRejected,
        *GetNameSafe(Winner.Actor),
        Winner.Score,
        Winner.Distance,
        Winner.AimDot);
}
```

The resolver tracks five counters during its passes:
`NumOverlaps` (raw `OverlapMultiByObjectType` results),
`NumCandidates` (after interface filter and per-actor dedup),
`NumAimRejected`, `NumLosRejected`, and the winner snapshot.
Counters are local stack ints, zero overhead when the CVar is off.

**3. Optional debug draw (slice 1 stretch, slice 2 if deferred):**

```cpp
static TAutoConsoleVariable<int32> CVarInteractionDraw(
    TEXT("alis.Interaction.Draw"),
    0,
    TEXT("Draw interaction targeting debug shapes (overlap sphere, rejected/winner rays)."),
    ECVF_Default);
```

When enabled, draw with `DrawDebugSphere` / `DrawDebugLine` (lifetime
matches `TraceIntervalSeconds` so shapes do not pile up):
- overlap sphere at ViewOrigin
- `alis.Interaction.Draw 1`: winner/miss line only
- `alis.Interaction.Draw 2`: also draw candidate rejection lines, red if
  aim-rejected and yellow if LOS-rejected

Both CVars stay off by default and are not shipped to players. They
exist so a single bad collision setup or one mis-configured object
channel can be diagnosed in seconds instead of hours.

**4. Preserve E-press debug flash (mandatory in slice 1):**

Current editor behavior must not regress: pressing interact today draws
a one-shot 5-second debug trace whenever `bDrawDebug` is true (see
[InteractionComponent.cpp:512-553](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L512-L553)).
Slice 1 keeps that everyday dev feedback, but updates it to visualize
the new resolver instead of the old standalone forward line trace.

Rules:
- Keep `bDrawDebug` on `UInteractionComponent` (existing UPROPERTY,
  default `true`).
- Keep the `BeginInteractInput` / E-press debug hook in
  `DrawInteractionDebugTraceOnInput`.
- Keep it editor-only (`#if WITH_EDITOR`).
- Do NOT keep a second independent forward `LineTraceSingleByChannel`
  that can disagree with the resolver. The current standalone trace at
  [InteractionComponent.cpp:543-549](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L543-L549)
  is deleted.
- Reuse the same resolver and candidate debug data used by
  `ResolveBestInteractionTarget`. One source of truth for visuals.

E-press flash draws for 5 seconds:
- overlap sphere at `ViewOrigin`
- green line to winner `TargetPoint` when resolver finds a winner
- red forward line / red sphere when no candidate survives
- yellow lines for LOS-rejected candidates if debug data is captured
- red lines for aim-rejected candidates if debug data is captured

Two debug paths share one helper:

```cpp
// Pure draw helper, called by both modes.
void DrawInteractionResolverDebug(
    UWorld*                                  World,
    const FInteractionResolverDebug&         Debug,
    const FInteractionCandidate*             Winner,
    float                                    LifeTime);

// E-press path (5 s, gated by bDrawDebug, editor only)
#if WITH_EDITOR
void UInteractionComponent::DrawInteractionDebugTraceOnInput() const
{
    if (!bDrawDebug) { return; }

    FInteractionResolverDebug Debug;
    FInteractionCandidate     Winner;
    const bool bFound = ResolveBestInteractionTarget(
        GetWorld(), ViewOrigin, ViewForward,
        TraceChannel, GetOwner(), BuildWeights(),
        Winner, &Debug);

    DrawInteractionResolverDebug(
        GetWorld(), Debug,
        bFound ? &Winner : nullptr,
        /*LifeTime=*/5.0f);
}
#endif

// Tick path (short lifetime, gated by alis.Interaction.Draw)
if (CVarInteractionDraw.GetValueOnGameThread() != 0)
{
    DrawInteractionResolverDebug(
        World, Debug,
        bFound ? &Winner : nullptr,
        /*LifeTime=*/GetTraceDebugLifetime());
}
```

`FInteractionResolverDebug` is a private POD captured by an optional
`FInteractionResolverDebug*` out-param on `ResolveBestInteractionTarget`.
When the pointer is null (server path, normal client tick without draw
CVar) the resolver skips the bookkeeping and the cost is zero. When non-
null it records overlap sphere data, per-candidate target points, and
gate-rejection reasons -- enough to render the shapes above.

Why a shared helper instead of two parallel implementations: the whole
point of slice 1 is one resolver, one source of truth for "what is
focusable right now." A second debug-only line trace would re-create
the divergence we are trying to eliminate.

`alis.Interaction.Draw` remains separate from this E-press flash:
- off by default
- timer-driven forensic mode
- short lifetime matching `TraceIntervalSeconds`
- used only when diagnosing why candidates are filtered/rejected
  across many frames (e.g. "this door overlaps but never wins")

E-press = "did my interact land?" feedback. Tick draw = "why is this
candidate not being picked?" forensics. Different jobs, different
lifetimes, same shapes.

---

## Slices

Each slice is landable independently; slice 1 alone fixes pixel hunting.

### Slice 0 -- prep extraction (no behavior change)

Before slice 1, extract the existing selection and execution helpers
into private implementation units with explicit responsibilities.
The landed split uses:
- `FInteractionCapabilitySelector` for component gathering, mesh-scoped
  selection, focus resolution, execution-spec resolution, and fallback
  interaction execution.
- `FInteractionTargetResolver` for overlap gather, LOS gate, scoring,
  and debug observability.

Zero public API expansion beyond the resolver test hook and tunables.
Keeps the main TU under 1000 LOC after slice 1 lands.

### Pre-Slice 1 -- object-type audit (mandatory)

`OverlapMultiByObjectType({ WorldDynamic, PhysicsBody, Pawn })` only
sees meshes whose primary collision object type is one of those three.
Before writing any gather code, verify each real interactable category
in the project is actually reachable by that set.

Audit targets (one representative actor per category is enough):

- Door / gate (e.g. luxury apartment door used in current release TODO).
- World loot container (chest, footlocker, crate).
- Small pickup (ammo, medical, food item).
- Dialogue target (NPC such as GrandPa).
- Shelter / player-placed object.
- Hazard / interactable environmental actor (e.g. GasCloud).

For each, read the actor's primary interactable `UPrimitiveComponent`
and record `GetCollisionObjectType()`. Use the Unreal Editor (Collision
panel) OR a one-off `UE_LOG` in the actor's `BeginPlay`. No build tests
needed for this; it is a manual inspection task.

Accept Slice 1 gather only if every audited interactable is in
`{ WorldDynamic, PhysicsBody, Pawn }`. Otherwise pick one:

- **A (preferred).** Move the interaction primitive of the offending
  actor to `WorldDynamic`. This is the KISS fix and matches the Lyra
  convention of "interactables overlap a forgiving object type."
  Usually a 1-line change in the actor's constructor:
  `SetCollisionObjectType(ECC_WorldDynamic)` on the interactable
  `UPrimitiveComponent` only. Does NOT move the whole actor from
  static to dynamic -- only the component we query on.
- **B (fallback).** Add `ECC_WorldStatic` to the gather's object-type
  set, then profile for noise (floors/walls/clutter can flood results)
  and mitigate with per-candidate interface filter. Avoid unless A is
  impossible for a specific actor category.

Document the audit result as a short table appended to this section
before slice 1 lands.

Audit result (2026-04-25, code audit):

| Category | Representative evidence | Object type result |
|---|---|---|
| Door / gate / loot container / shelter object | `ProjectObject` world meshes are spawned through `ObjectSpawnUtility`; physics-enabled meshes explicitly use `PhysicsActor`, while trigger-only helpers default to `OverlapAllDynamic`. | `PhysicsBody` or `WorldDynamic` |
| Small pickup / loose world prop | `ObjectSpawnUtility` assigns `PhysicsActor` when no explicit physics profile is authored. | `PhysicsBody` |
| Dialogue target / NPC | Character interaction runs against pawn collision; `DefinitionCharacter` keeps the capsule on the pawn object type and explicitly makes it visible to interaction LOS traces. | `Pawn` |
| Hazard / interactable environmental trigger | `ObjectDefinition` trigger collision profile defaults to `OverlapAllDynamic`. | `WorldDynamic` |
| Conclusion | Audited interactable categories resolve to `{ WorldDynamic, PhysicsBody, Pawn }`. Slice 1 may gather with `OverlapMultiByObjectType` on that set without adding `WorldStatic`. | Accepted |

### Slice 1 -- proximity gather + aim-scored focus (ships the UX win)

In `UpdateTrace` (client tick only):

```
ViewOrigin  = PlayerCameraManager->GetCameraLocation()
ViewForward = PlayerCameraManager->GetCameraRotation().Vector()

ResolveBestInteractionTarget(World, ViewOrigin, ViewForward,
    TraceChannel, Owner, Weights, OutWinner):

  1. Gather:  OverlapMultiByObjectType(
                 sphere at ViewOrigin, radius = InteractionRadius,
                 { WorldDynamic, PhysicsBody, Pawn },
                 IgnoreOwner)
  2. Filter:  per actor, keep best overlap component, require
                  Actor.Implements<IInteractableTargetInterface>  OR
                  any component implements IInteractableComponentTargetInterface
  3. Aim gate: drop AimDot < MinAimDot
  4. LOS gate: LineTraceSingleByChannel(ViewOrigin -> Candidate.TargetPoint,
                                        TraceChannel).
               Bypass LOS (only) when Distance <= ShortCircuitRadius.
               Aim gate above is still enforced.
  5. Score:   AimWeight * AimScore + DistanceWeight * DistanceScore.
               PriorityWeight = 0 in slice 1.
  6. Tiebreak: raw priority, then distance, then ActorUniqueID.

SetFocusedActor(Winner.Actor, Winner.Component)   if resolver returned true
SetFocusedActor(nullptr, nullptr)                  otherwise
```

Slice 1 changes `UpdateTrace`'s body and introduces a pure private
resolver path plus private capability-selection helpers. No new
subsystem, no collision config migration, no Lyra-style Interaction
channel, and no new public cross-plugin contract.

### Slice 2 -- priority scoring + hysteresis

Implemented 2026-04-26:
- `PriorityWeight` default is `0.5`, enabling the existing
  `PriorityScore` term in the unified scoring function.
- `FocusSwitchHysteresis = 0.10` is applied before `SetFocusedActor`:
  current focus is kept unless a new score beats it by more than 10 %.
- Test coverage:
  `ProjectIntegrationTests.Interaction.Targeting.PriorityCanBeatSlightlyBetterAim`
  and
  `ProjectIntegrationTests.Interaction.Targeting.HysteresisKeepsCurrentFocusOnNearTie`.

### Slice 3 -- server-authoritative parity (pure resolver shared)

**Do NOT call `UpdateTrace` from the server path.** `UpdateTrace` is a
client-tick side-effect function. Server validation must call the pure
resolver directly with a **server-sourced view**:

```cpp
// In ExecuteInteraction_ServerAuth
AController* Ctrl = Cast<APawn>(GetOwner())->GetController();
FVector     Origin;
FRotator    Rot;
Ctrl->GetActorEyesViewPoint(Origin, Rot);  // server-safe source
FVector     Forward = Rot.Vector();

FInteractionCandidate Winner;
if (ResolveBestInteractionTarget(GetWorld(), Origin, Forward,
        TraceChannel, GetOwner(), BuildWeights(), Winner))
{
    // execute directly; DO NOT touch FocusedActor / highlight on server
    ...
}
```

Why not `PlayerCameraManager->GetCameraLocation()` on server: the
PlayerCameraManager is client-side presentation; on a dedicated server
it can be null or reflect a replicated (possibly stale) state. Also
directly relevant to this todo -- the dynamic-camera timing already
caused one bug (the 2026-04-24 post-process fix). Keep server validation
off anything camera-component-dependent.

`Controller::GetActorEyesViewPoint` is engine-native and populates from
the controller / pawn on both authorities, so client and server agree.

Touch points:
- `ExecuteInteraction_ServerAuth` switches to the pure resolver.
- `UpdateTrace` also switches to the pure resolver (same function).
- Tests using `TestOnly_SetFocusedActor` remain correct because they
  bypass both the trace AND the resolver entirely.

Implemented 2026-04-26. `ExecuteInteraction_ServerAuth` now resolves
server view through actor eyes and calls `FInteractionTargetResolver`
directly. It does not touch focus/highlight state. Test coverage:
`ProjectIntegrationTests.Interaction.Targeting.ServerAuthUsesOverlapResolver`.

### Slice 4 -- cleanup

Implemented 2026-04-26:
- Removed `TraceDistance`.
- Removed `TraceRadius`.
- No CoreRedirects were added because ALIS interaction targeting is
  C++-owned and no Blueprint/map/JSON migration path exists for those fields.
- Dedicated `Interaction` trace channel was not added. Profiling/content
  evidence did not show a need; the object-query gather remains simpler.
- No `TODO(Interaction): dynamic-camera retry` comments were present.

---

## Landing status

Original review guidance asked for separate slice landings to isolate risk.
Implementation ultimately landed in one local pass at user request after
Slice 1 had already built, passed focused tests, and was manually checked
in-editor for pickup targeting.

Slice 1 contained:

```
Slice 0 (if needed):
  anonymous-namespace extraction only, zero behavior change.

Pre-Slice 1:
  object-type audit (manual, no code); results documented in this file.

Slice 1:
  OverlapMultiByObjectType gather
  interface filter
  aim gate
  LOS gate BEFORE score
  aim + distance score only (PriorityWeight = 0, no hysteresis)
  pure resolver + FInteractionCandidate struct (private)
  SetFocusedActor pipeline UNCHANGED
  Debug observability:
    - alis.Interaction.Debug log CVar (off by default)
    - alis.Interaction.Draw shape CVar (off by default)
    - E-press editor flash preserved via shared resolver debug helper
      (5 s lifetime, gated by existing bDrawDebug, NOT a second
      independent forward line trace)
```

Slice 1 must preserve editor E-press debug UX:
- pressing E with `bDrawDebug=true` still produces an immediate
  5-second visual diagnostic in the editor.
- the visual reflects the new overlap/score resolver, not the deleted
  old standalone forward line trace
  ([InteractionComponent.cpp:543-549](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L543-L549)
  is removed).
- E-press flash and `alis.Interaction.Draw` share the same draw helper
  so they cannot drift apart visually.

Slice 2, 3, and 4 were then completed together with dedicated tests for
priority weighting, hysteresis, and server-authoritative resolver parity.
Dedicated-server/manual validation is still recommended before release
because automation cannot prove authored-map collision on every asset.

---

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Using `ECC_Visibility` for overlap gather returns wrong candidates (Visibility is a trace channel, not an object channel) | Use `OverlapMultiByObjectType({WorldDynamic, PhysicsBody, Pawn})` in slice 1. Keep `ECC_Visibility` for the LOS line trace only. Dedicated `Interaction` trace channel is optional slice-4 follow-up. See "Channel correctness" above. |
| Winner is chosen, then LOS kills it, leaving NO focus even when a valid visible candidate existed | LOS gate runs **before** scoring. Only visible candidates are scored. See "Data flow" ordering. |
| Gather collapses overlap results to `TArray<AActor*>` and loses the specific primitive that overlapped | `FInteractionCandidate` preserves `UPrimitiveComponent*` from `FOverlapResult` and feeds it into `SelectBestInteractableComponent`, so mesh-scoped component selection still works. |
| Server authoritative re-trace diverges from client or depends on client-only camera | Pure `ResolveBestInteractionTarget` resolver is called by both. Server sources the view from `Controller->GetActorEyesViewPoint`, NOT `PlayerCameraManager`, so server validation is independent of the dynamic-camera timing issue. |
| Existing Interactable actors rely on line-trace hit component | `SelectBestInteractableComponent` is called with the overlap hit component. For actor-only candidates (no primitive hit), behavior matches today's "no HitComponent" branch at [InteractionComponent.cpp:121-124](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L121-L124). |
| Perf regression | Throttled by `TraceIntervalSeconds=0.15`; overlap + N short line traces, N typically <= 5. Input refreshes immediately before interaction. Passive focus does not run on server or remote pawns. |
| Highlight jitter (winner flips frame-to-frame among near-tied candidates) | Slice 2 `FocusSwitchHysteresis = 0.10` keeps current focus until new score beats it by > 10 %. |
| Scoring picks behind-head or peripheral target when looking away | `MinAimDot = 0.85` is always enforced in the aim gate (cannot be bypassed by `ShortCircuitRadius`). |
| Non-deterministic tiebreak when two candidates have identical scores | Tiebreak uses raw `Priority`, then `Distance`, then `Actor->GetUniqueID()` -- stable within a play session. |
| Net code change in InteractionComponent pushes file over 1000 LOC | Mitigation: keep orchestration in `InteractionComponent` and move targeting / capability-selection logic into private helper files with explicit responsibilities. Landed shape keeps `InteractionComponent.cpp` at 622 LOC. |
| New tunables hand-set per-map or per-character | Keep defaults on the component; do not push into `DefaultGame.ini`. If characters need different radii, put it on the pawn's default subobject. |

---

## Verification

Slice 1 ships with manual verification + ONE resolver regression test
(see "Required automated test" below). Existing
`TestOnly_SetFocusedActor` suites bypass the resolver entirely, so they
cannot catch a regression in the new gather/filter/aim/LOS path -- a
fresh test that drives the resolver itself is the only way to keep the
new behavior locked.

### Manual verification

1. **Pixel hunting gone:** walk within 3 m of any interactable (door,
   loot container, dialogue NPC); reticle within 25 deg of the object
   -> highlight appears. Outline fix from 2026-04-24 continues to work.
2. **Two items close together:** hover between two interactables 30 cm
   apart; the one the reticle is closest to (highest dot) wins. Slight
   aim shift switches focus predictably -- no jitter.
3. **Through-wall guard:** place an interactable directly behind a wall
   2 m away; reticle aimed at it -> no highlight.
4. **Directly-touching LOS bypass:** stand very close to a small
   interactable inside `ShortCircuitRadius` and aim roughly toward it.
   LOS may be bypassed (no wall block check), but `MinAimDot` is still
   enforced. If the player looks 90+ degrees away or straight at the
   ceiling, no focus is expected -- short-circuiting LOS does NOT
   short-circuit player intention.
5. **Back-of-pawn guard:** interactable at pawn back; reticle forward
   -> no highlight (aim dot < `MinAimDot`).
6. **Existing suites unchanged:** `TestOnly_SetFocusedActor` /
   `TestOnly_ExecuteInteraction` tests still pass -- they bypass
   `UpdateTrace` entirely.
7. **Server-authoritative execution:** client clicks interact on a
   highlighted target; server `ExecuteInteraction_ServerAuth` succeeds
   and the object responds.

### Required automated tests (land with or immediately after slice 1)

Resolver regression tests in `ProjectIntegrationTests` exercise the pure
resolver directly (no `UpdateTrace`, no `SetFocusedActor`). They are tagged
`[Fast][Integration][Interaction]` and must pass before slice 2 opens:
- `ProjectIntegrationTests.Interaction.Targeting.OverlapFindsOffAxisInteractableWithoutDirectLineHit`
- `ProjectIntegrationTests.Interaction.Targeting.VisibleCandidateBeatsOccludedCenteredCandidate`
- `ProjectIntegrationTests.Interaction.Targeting.BackCandidateRejectedByAimGate`
- `ProjectIntegrationTests.Interaction.Targeting.PeripheralCandidateRejectedByAimGate`
- `ProjectIntegrationTests.Interaction.Targeting.ShortCircuitBypassesLosButNotAim`
- `ProjectIntegrationTests.Interaction.Targeting.PriorityCanBeatSlightlyBetterAim`
- `ProjectIntegrationTests.Interaction.Targeting.HysteresisKeepsCurrentFocusOnNearTie`
- `ProjectIntegrationTests.Interaction.Targeting.ServerAuthUsesOverlapResolver`

```
Test: Interaction.Resolver.OverlapScoreBasic

Arrange:
  - Spawn a controllable pawn (no AI / camera deps).
  - Spawn interactable A 1 m in front of pawn (visible).
  - Spawn interactable B 1.3 m in front, 30 cm to the side of A
    (visible, slightly off-axis).
  - Spawn a thin wall 2 m in front, occluding a third interactable C
    placed directly behind it on the same forward axis.
  - Build view origin/forward from the pawn so the reticle aims at A.

Act:
  - Call ResolveBestInteractionTarget(World, ViewOrigin, ViewForward,
                                      ECC_Visibility, Pawn,
                                      DefaultWeights, OutWinner).

Assert:
  - Resolver returned true.
  - OutWinner.Actor == A (visible, best-aimed candidate wins).
  - OutWinner.Actor != C (LOS gate dropped the wall-occluded candidate).
  - OutWinner.AimDot >= MinAimDot.

Bonus assertion (slice 1 contract guard):
  - Existing TestOnly_SetFocusedActor suite continues to pass
    unchanged -- proves the SetFocusedActor pipeline downstream of the
    resolver was not disturbed.
```

Why this test is non-negotiable: the only thing existing tests cover
is what happens **after** `SetFocusedActor` is called. The bug class
this todo introduces (wrong object channel, gate ordering, lost
`UPrimitiveComponent`, mesh-pivot trace target) lives entirely
**before** that call. Without one resolver-level test, slice 2's
priority/hysteresis work has no regression net.

Only land slice 1 as a single PR (with this test). Slices 2-4 in
follow-ups with their own PRs.

---

## Review log

- **2026-04-24 initial draft** -- rated "good direction, not golden yet"
  with five blockers: (1) wrong channel type for overlap gather,
  (2) LOS gate ordered after scoring, (3) gather collapsed to
  `TArray<AActor*>`, (4) server path called `UpdateTrace` side-effect
  function, (5) scoring formula lacked explicit clamps and tiebreakers.
- **2026-04-24 revision (this file)** -- switched gather to
  `OverlapMultiByObjectType`, moved LOS before score, introduced
  `FInteractionCandidate` with preserved `UPrimitiveComponent*`,
  extracted pure `ResolveBestInteractionTarget` resolver shared by
  client tick and server validation (server view from
  `Controller::GetActorEyesViewPoint`, not `PlayerCameraManager`),
  wrote explicit scoring formula with clamps + 3-level tiebreak, added
  Slice 0 extraction prep, split priority + hysteresis into Slice 2.
- **2026-04-24 revision 2 (this file)** -- required-edit pass before
  implementation: (1) swapped `TObjectPtr` for raw pointers in the
  private transient `FInteractionCandidate` (local/stack data, not
  UPROPERTY, not cross-frame, matches Epic guidance); (2) added a
  mandatory pre-Slice-1 object-type audit for every real interactable
  category so gather's object-type set is verified against content
  instead of assumed; (3) clarified the "do not invent new types" rule
  -- forbids new public framework contracts, allows private helper
  structs; (4) added a hard landing rule: Slice 1 ships alone, Slices
  2/3/4 ship as separate PRs; (5) called out that Slice 3 server
  validation must be tested in dedicated-server PIE or a real
  `AlisServer` build, not listen-server PIE.
- **2026-04-25 revision 3b (this file)** -- preserve editor E-press
  debug UX. Today `BeginInteractInput` -> `DrawInteractionDebugTraceOnInput`
  draws a 5-second forward line in editor when `bDrawDebug` is true
  ([InteractionComponent.cpp:512-553](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L512-L553)).
  Earlier revisions added `alis.Interaction.Draw` for passive targeting
  shapes but inadvertently dropped the E-press flash, which is daily
  dev feedback. Fix: slice 1 keeps `bDrawDebug` + the E-press hook,
  but the hook now reuses the new resolver (no second independent
  forward `LineTraceSingleByChannel`). Both modes -- E-press 5 s flash
  and timer-driven `alis.Interaction.Draw` -- share one
  `DrawInteractionResolverDebug` helper, fed by an optional
  `FInteractionResolverDebug*` out-param on `ResolveBestInteractionTarget`
  (null when not drawing, so server / non-debug client tick pays
  zero). Added explicit landing-rule constraint: slice 1 must not
  regress the editor E-press visual.
- **2026-04-25 revision 3a (this file)** -- approval-cleanup pass:
  (1) fixed stale Slice 1 pseudocode -- LOS gate now traces to
  `Candidate.TargetPoint`, matching the design contract that aim/LOS/
  distance all share the cached resolved target instead of mesh pivot;
  (2) corrected `ResolveInteractionTargetPoint` helper -- Epic's
  `UPrimitiveComponent::GetClosestPointOnCollision` is `const` and
  returns linear distance (not squared), so the helper uses a
  `Distance` local and treats negative return values as failure;
  squared-distance API is `GetSquaredDistanceToCollision`. Note added
  inline to prevent future mix-up.
- **2026-04-25 revision 3 (this file)** -- review pass; required edits
  before implementation: (1) fixed verification step 4 contradiction
  -- "directly-touching" only bypasses the LOS gate, the aim gate
  (`MinAimDot`) is still always enforced, ceiling-aim wording removed;
  (2) added `FInteractionCandidate::TargetPoint` and a
  `ResolveInteractionTargetPoint` helper that prefers
  `GetClosestPointOnCollision` -> `Bounds.Origin` -> actor location,
  so the aim gate, LOS gate, and distance score share one stable
  target instead of trusting mesh pivot (which can sit inside a wall
  or below the floor); (3) added mandatory debug observability for
  slice 1 -- `alis.Interaction.Debug` CVar gates a structured Verbose
  log with overlap/candidate/aim-rejected/LOS-rejected/winner counts,
  plus optional `alis.Interaction.Draw` for debug shapes; (4) added a
  mandatory resolver-level regression test
  (`Interaction.Resolver.OverlapScoreBasic`) shipping with or
  immediately after slice 1, since existing `TestOnly_SetFocusedActor`
  tests bypass the resolver and cannot catch the new bug class.
  Server view source (`GetActorEyesViewPoint` vs `GetPlayerViewPoint`)
  flagged for re-evaluation when slice 3 starts -- both populate from
  the controller / pawn, but Epic docs describe `GetPlayerViewPoint`
  as the explicit player point of view (camera for humans, pawn eyes
  for AI), which may be the cleaner contract on a dedicated server.
  Decide in slice 3 with a parity test, not now.

## Related

- Hover-outline post-process retry (landed 2026-04-24) --
  [InteractionComponent.cpp:309-322](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L309-L322).
- Plugin architecture principles -- `docs/architecture/plugin_rules.md`.
- Canonical engine-primitive guidance -- `docs/agents/canonical.md` section
  "Use UE engine primitives. Do not reinvent."
- ProjectInteraction README -- `Plugins/Gameplay/ProjectInteraction/README.md`.
- Stable-doc / living-code rule: this file is a todo, so no SOT doc links
  back to it. When slice 1 lands, move relevant design notes (radius
  defaults, scoring formula) into `Plugins/Gameplay/ProjectInteraction/README.md`
  before archiving this todo, per `docs/agents/canonical.md` todo-file policy.
