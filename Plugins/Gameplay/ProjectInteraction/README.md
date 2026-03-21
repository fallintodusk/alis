# ProjectInteraction

Player interaction system for ALIS.

## Purpose

- Detects interactable actors via traces
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
  -> Calls IInteractionComponentInterface::Execute_TryInteract() on pawn's component

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
IInteractionComponentInterface::Execute_TryInteract()
    |
    v
UInteractionComponent::TryInteract_Implementation()
    |
    +-- Has Authority? --> ExecuteInteraction_ServerAuth()
    |
    +-- No Authority? --> Server_TryInteract() RPC
                              |
                              v
                         ExecuteInteraction_ServerAuth()
    |
    v
Server re-traces (don't trust client's target)
    |
    v
IInteractionService::OnInteraction.Broadcast(Target, Instigator)
    |
    +--> ProjectInventory (pickups, loot containers, doors)
    +--> DialogueFeature (checks for DialogueComponent)
```

**Server-Authoritative Design:**
- Client calls `TryInteract()`, which routes to server via `Server_TryInteract()` RPC
- Server re-traces from camera to find target (anti-cheat)
- Only server broadcasts `OnInteraction` - features handle server-side only
- Sidesteps "RPC must be called on owned actor" - component is on player's pawn

## Key Classes

| Class | Location | Purpose |
|-------|----------|---------|
| `IInteractionComponentInterface` | ProjectCore | Interface for component |
| `IInteractionService` | ProjectCore | Event subscription interface |
| `UInteractionComponent` | This plugin | Trace detection, highlight, implements interface |
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
- [x] `Server_TryInteract()` - server-authoritative interaction (re-traces on server)

## Legacy Paths

Canonical ID registry for this refactor:
- [todo/done/generalize_placeable_actor_parent.md](../../../todo/done/generalize_placeable_actor_parent.md)

Code marker format:
- `// LEGACY_OBJECT_PARENT_GENERALIZATION(L###): <reason>. Remove when <condition>.`

| Legacy ID | Location | Why It Exists | Remove Trigger |
|-----------|----------|---------------|----------------|
| _(none active)_ | n/a | Interaction now uses strict interface-only mesh targeting | n/a |
