# ProjectInteraction

Player interaction system for ALIS.

## Purpose

- Detects interactable actors via proximity gather, line-of-sight gating,
  and deterministic aim/distance/priority scoring
- Broadcasts events via `IInteractionService`
- Features (Inventory, Dialogue) subscribe to events
- Decoupled via interfaces in ProjectCore

## Architecture

```
ProjectCore (interfaces only)
  -> IInteractionComponentInterface  # PlayerController queries this
  -> IInteractionService             # Features subscribe to this

PlayerController (SinglePlayController, etc.)
  -> Depends on ProjectCore
  -> Owns IA_Interact input (E key)
  -> Calls BeginInteractInput()/EndInteractInput() on pawn's interaction component

ProjectInteraction (this plugin)
  -> FFeatureRegistry("Interaction")  # Attaches component on GameMode init
  -> UInteractionComponent            # Implements IInteractionComponentInterface
  -> FInteractionService              # Implements IInteractionService

Features (Inventory, Dialogue, etc.)
  -> Subscribe to IInteractionService::OnInteraction()
```

## Flow (Server-Authoritative)

```
Player Input (IA_Interact in PlayerController)
    |
    v
PlayerController finds UInteractionComponent on Pawn
    |
    v
IInteractionComponentInterface::Execute_BeginInteractInput()
    |
    v
UInteractionComponent starts hold if the focused execution spec requires it
    |
    +-- Release early? --> cancel prompt/progress
    |
    +-- Hold completes? --> DispatchInteract(HoldTarget, HoldComponent)
    |                       (held target captured at hold start)
    |
    +-- Otherwise --> TryInteract() -> DispatchInteract(FocusedActor, FocusedComponent)
    |
    +-- Has Authority? --> ExecuteInteraction_ServerAuth(Target, HitComponent)
    |
    +-- No Authority? --> Server_TryInteract(Target, HitComponent) RPC
                              |
                              v
                         ExecuteInteraction_ServerAuth(Target, HitComponent)
    |
    v
Server validates the client's focused (Actor, Component) - the same
target the local resolver picked and highlighted - and dispatches that
target. Server does NOT re-resolve from a different view: highlight and
interaction share a single source of truth on the client.
    |
    v
IInteractionService::OnInteraction.Broadcast(Target, Instigator)
    |
    +--> ProjectInventory (pickups, loot containers, doors)
    +--> DialogueFeature (checks for DialogueComponent)
```

**Single source of truth for highlight + interaction:**
- The client's local targeting resolver picks `FocusedActor` and
  `FocusedComponent`. That pick drives the highlight (custom depth on the
  primitive) AND is the exact target sent into interaction.
- `TryInteract()` reads `FocusedActor` / `FocusedComponent` and dispatches
  through `DispatchInteract(Target, HitComponent)`. On authority the call
  goes straight into `ExecuteInteraction_ServerAuth(Target, HitComponent)`;
  remote clients send the pair via `Server_TryInteract(Target, Component)`.
- The server does NOT re-resolve from its own view. Re-resolving was the
  dresser regression: a single actor with 5 - 10 cm sibling drawer-slot
  primitives, the client highlighted slot A, the server's eye view
  (`GetActorEyesViewPoint`, no FOV interp / camera lag, BaseEyeHeight
  offset) landed the cone on slot B and either opened the wrong drawer or
  rejected everything as out-of-cone.
- Server-side `ExecuteInteraction_ServerAuth` is now a validation gate, not
  a re-resolver. It checks: target non-null, distance from pawn within
  `InteractionRadius * 1.5` (anti-cheat range bound), target is actually
  interactable (actor interface or capability components present). If
  validation passes, dispatch goes through actor interface or capability
  selector; the server then broadcasts `OnInteraction`.
- Hold interactions capture `(HoldTargetActor, HoldTargetComponent)` at
  hold start and dispatch that captured target on completion - the SOT
  for a hold is the target the player started the hold on, not whatever
  is focused at the instant the hold finishes.
- Only the server broadcasts `OnInteraction` - features handle server-side
  only.
- Component is on the player's pawn so the RPC has a valid path.

## Targeting

`UInteractionComponent` keeps orchestration, focus state, highlighting, hold
progress, and service broadcasts. Private helpers keep the selection logic
separate:
- `FInteractionTargetResolver` gathers candidates with
  `OverlapMultiByObjectType` for `WorldStatic`, `WorldDynamic`,
  `PhysicsBody`, and `Pawn`. It then filters to interactable actors or
  components, resolves the target point per primitive (see "TargetPoint
  resolution"), applies the aim-cone gate and visibility LOS gate, and picks
  a winner with a deterministic comparator chain.
- `FInteractionCapabilitySelector` maps the winning actor/component to the
  correct interactable component, focus info, execution spec, and fallback
  component interaction.

Player rule: **real ray collision wins. A bounds-only ray hit is a visual
fallback. A cone fallback is last. Within a bucket the front-most (or
most-centered) candidate wins. Priority is only a final tiebreak.**

The comparator (`IsBetterCandidate`) is a 3-bucket deterministic chain. The
hit-kind enum is `EInteractionViewRayHitKind { None, Bounds, Collision }`,
ranked `Collision (2) > Bounds (1) > None (0)`:

1. Higher hit-kind rank wins. A closer `Bounds` hit must NOT outrank a
   further `Collision` hit - that was the backpack regression where a loose
   AABB clipped above a smaller collision-pierced item.
2. Among same-bucket `Collision` or `Bounds`: smaller `ViewRayHitDistance`
   wins (front-most along the ray).
3. Among same-bucket `None`: higher `AimDot` wins (most centered in cone).
4. Tiebreak: smaller `Distance`.
5. Tiebreak: higher `Priority` (never overrides aim).
6. Final: deterministic `Actor->GetUniqueID()`.

### TargetPoint resolution

For each primitive, `ResolveTargetPoint` picks the most honest "I'm visually
pointing at this" point and a `ViewRayHitKind` in three steps:

1. `UPrimitiveComponent::LineTraceComponent` against the candidate's collision.
   If the ray hits: `ViewRayHitKind = Collision`, `ViewRayHitDistance =
   ComponentHit.Distance`. Highest trust - real ray pierce against the
   physics shape.
2. AABB ray intersection (`FMath::LineExtentBoxIntersection`) against the
   primitive's `Bounds`. Catches meshes whose visible geometry has no
   collision (window glass on a frame-only collision body, decorative meshes,
   etc.): `ViewRayHitKind = Bounds`, `ViewRayHitDistance =
   |HitPoint - ViewOrigin|`. AABB is loose for thick or rotated meshes, so a
   `Bounds` hit ranks below `Collision` even when its hit distance is closer.

   **Authoring rule:** if a foreground interactable should reliably beat
   other collision-hit objects, author collision on the intended interactable
   surface. Bounds is only a visual fallback and intentionally ranks below
   real collision; it cannot compensate for missing collision on a hero
   surface (e.g. a foreground window with frame-only collision will lose to
   a background object with full-mesh collision under the same crosshair).
3. Closest point on collision to view origin. Final fallback when neither the
   ray nor the bounds intersect: `ViewRayHitKind = None`. The candidate
   competes in the cone-fallback bucket on `AimDot`.

Hysteresis (`FInteractionTargetResolver::ShouldKeepCurrentCandidate`) only
smooths within a single bucket. Cross-bucket switches (different
`ViewRayHitKind`) are deliberate aim transitions and always switch. Within a
bucket:
- `Collision`/`Bounds`: distance-based multiplicative threshold on
  `ViewRayHitDistance` - the incumbent keeps focus if its hit distance is
  within `FocusSwitchHysteresis` fraction of the challenger's. Distance is
  unbounded so a ratio scales correctly.
- `None`: angular leeway scaled to the cone half-angle. Convert both
  `AimDot` values back to angles via `acos`, then keep the incumbent only if
  its angle is within `FocusSwitchHysteresis * acos(MinAimDot)` of the
  challenger's. With defaults that is `0.10 * 31.8 deg ~= 3.18 deg` of
  stickiness. The earlier ratio threshold (`NewWinner.AimDot * (1 - h)`) was
  unusable here because `AimDot` lives in `[MinAimDot, 1.0]` (a 0.15-wide
  range with defaults), so any ratio `< 1` kept the entire cone sticky and
  prevented legitimate aim switches.

Default targeting tunables:

| Property | Default | Meaning |
|---|---:|---|
| `InteractionRadius` | `200.0` | Candidate overlap sphere radius (~2 m: arm's reach plus floor-pickup margin). |
| `ShortCircuitRadius` | `60.0` | Bypass LOS only for directly touched targets; aim still applies. |
| `MinAimDot` | `0.85` | Minimum dot between view direction and target direction (cone half-angle ~31.8 deg). |
| `FocusSwitchHysteresis` | `0.10` | Within a bucket, keep current focus unless a new candidate beats the discriminator (`ViewRayHitDistance` for pierced, `AimDot` for non-pierced) by more than 10 percent. Cross-bucket switches always switch. |
| `TraceIntervalSeconds` | `0.15` | Passive local focus refresh cadence. Input refreshes immediately before interaction. |

Debug CVars:
- `alis.Interaction.Debug 1` logs overlap, candidate, rejection, and winner
  counts.
- `alis.Interaction.Draw 1` draws only the overlap sphere and winner/miss ray.
- `alis.Interaction.Draw 2` also draws rejected candidate rays.

Timed interaction rules
- Interaction timing is described by `FInteractionExecutionSpec`.
- World interaction providers own search/open timing.
- Default searchable world storage uses a short hold (`1.0s`).
- `0.0s` means instant interaction.
- HUD prompt shows progress through the shared interaction prompt path.
- Releasing `E` cancels timed interaction; no extra cancel key is required.
- If a nearby world-container session is already open, `E` closes it.

## Key Classes

| Class | Location | Purpose |
|-------|----------|---------|
| `IInteractionComponentInterface` | ProjectCore | Interface for component |
| `IInteractionService` | ProjectCore | Event subscription interface |
| `UInteractionComponent` | This plugin | Focus orchestration, highlight, implements interface |
| `FInteractionTargetResolver` | This plugin private | Overlap gather, gates, scoring, debug draw |
| `FInteractionCapabilitySelector` | This plugin private | Component selection, focus/spec resolution, fallback execution |
| `FInteractionService` | This plugin | Broadcasts events |

## Feature Initialization

Registered with `FFeatureRegistry` as "Interaction" feature:
- Module startup registers init function with FFeatureRegistry
- GameMode (ProjectSinglePlay) calls `InitializeFeature("Interaction", Context)`
- Init function attaches `UInteractionComponent` to pawn

To enable in a mode, add "Interaction" to `ModeConfig.FeatureNames`

## Highlight System

Focused actors are outlined via post-process material using Custom Depth.

Setup:
1. Set `OutlineMaterial` on `UInteractionComponent` to your PP material (e.g. `/ProjectMaterial/Effect/MI_Outline`)
2. `bEnableHighlight = true` (default)
3. **Project Settings**: Custom Depth-Stencil Pass = "Enabled"
4. **Material Settings**: Blendable Location = "After Tonemapping" (required for camera PP)

How it works:
- Component adds PP material to owner's camera on BeginPlay
- On focus change, toggles `SetRenderCustomDepth()` on target actor's primitives
- PP material reads Custom Depth buffer to draw outline

## Dependencies

- `ProjectCore` - IInteractionComponentInterface, IInteractionService, ServiceLocator
- `ProjectFeature` - FFeatureRegistry for GameMode-driven initialization

Related docs
- Inventory/world-storage behavior SOT:
  `Plugins/Features/ProjectInventory/docs/design_vision.md`
- HUD composition root:
  `Plugins/UI/ProjectHUD/README.md`

## Decoupling

| Plugin | Depends On |
|--------|-----------|
| ProjectCharacter | ProjectCore, ProjectGAS |
| ProjectInteraction | ProjectCore, ProjectFeature |
| Features | ProjectCore (+ ProjectGAS if using GAS) |

No direct dependencies between Character <-> Interaction <-> Features.

## TODO

- [x] `UInteractionComponent` - trace detection, `TryInteract()` method
- [x] `IInteractionComponentInterface` - decoupled interface in Core
- [x] Highlight/focus system (PP material + Custom Depth)
- [x] FFeatureRegistry integration (GameMode-driven init)
- [x] `Server_TryInteract(Target, HitComponent)` - server-authoritative interaction (validates the client's focused target; does not re-resolve)

## Legacy Paths

Code marker format:
- `// LEGACY_OBJECT_PARENT_GENERALIZATION(L###): <reason>. Remove when <condition>.`

| Legacy ID | Location | Why It Exists | Remove Trigger |
|-----------|----------|---------------|----------------|
| _(none active)_ | n/a | Interaction now uses strict interface-only mesh targeting | n/a |
