# ProjectMind Architecture

## 1. Context

ProjectMind lives in Gameplay tier:
- `Plugins/Gameplay/ProjectMind`

Design constraints from current decisions:
- multiplayer-capable in future, but initial logic is client-side only
- data-driven runtime behavior
- reuse ProjectUI and ProjectHUD patterns, no custom ad-hoc UI stack
- save/load is planned but blocked for final stage
- localization-ready text from start
- dialogue events are first-class input signals for thought generation

## 2. Gameplay Tier Ownership

ProjectMind is a gameplay plugin:
- runtime service registration lives in `ProjectMind` module
- no `FFeatureRegistry` dependency for core runtime

Reason:
- plugin tier should describe ownership and runtime boundary
- mind runtime should be self-owned and portable across game modes
- single-play controller should not know runtime details of mind internals

## 3. Runtime Bootstrap Pattern

Bootstrap is owned by ProjectMind runtime:
- `UProjectMindRuntimeBootstrapSubsystem` resolves local player pawn
- subsystem applies pawn context through `IMindRuntimeControl`
- `ProjectSinglePlayClient` is decoupled from `ProjectMind` runtime module

Why this is the best current fit:
- keeps dependencies one-way (game mode/input -> UI only)
- keeps mind runtime boxed and reusable in single-player and multiplayer clients
- no per-controller component injection logic needed outside ProjectMind

## 4. Runtime/UI Boundary

To avoid tight coupling:
- runtime thought generation stays in ProjectMind
- UI rendering stays in UI plugins through ProjectUI framework
- shared contracts should be defined in ProjectCore interfaces

Recommended contract direction:
- UI consumes events/snapshots through core-level abstractions
- runtime never calls widgets directly
- source adapters implement `IMindThoughtSource`-style contracts in `ProjectCore`

## 5. Multiplayer Posture

Current plan:
- generate and display mind hints client-side only
- scoped to local controller/pawn context

Future stricter multiplayer posture (optional):
- tighter visibility/validation rules for competitive scenarios
- keep this deferred unless there is a concrete gameplay need

## 6. Perception Policy

Single-player baseline:
- practical camera-context hints
- avoid over-engineered anti-cheat filtering (YAGNI)

Multiplayer baseline:
- same architecture supports stricter checks later
- policy should be configurable, not hardcoded per mode

## 7. Localization Policy

All player-facing thought text should be localization-ready from day one:
- use `FText`
- avoid hardcoded user-visible `FString` paths in runtime logic
- keep template keys/data separate from code

## 8. Guidance Sources and Priority

Guidance should merge two source families:
- beaconized world components (interactables, useful world objects, locations)
- contextual progression intents (optional quest/objective source)

Design stance:
- no mandatory centralized "quest controller" is required
- POI/beacon guidance is valid as the primary model
- objective-style guidance is additive and can be introduced per content need
- `IMindQuestThoughtSource` can be absent with fail-soft runtime behavior
- prefer existing actor tags and existing world data
- avoid introducing new world entities only for mind scan unless a proven need appears

Priority model (highest first):
- P0: critical quest blockers / immediate fail-risk guidance
- P1: active quest objective guidance
- P2: nearby useful interactable guidance
- P3: ambient orientation hints

Tie-breakers:
- visibility confidence (camera direction/LOS)
- distance relevance window
- recency cooldown and dedupe

Design rule:
- both source families feed one ranked stream
- final output should be deterministic and data-driven

## 9. Save/Load Policy

History persistence is intentionally staged:
- runtime and UI contracts should be designed to allow persistence
- actual save integration is blocked until save system milestone is ready

## 10. Dialogue Event Source

ProjectMind must listen to dialogue events, including existing grandpa conversations.

Integration direction:
- subscribe through `IDialogueService` contract from ProjectCore
- map dialogue context to thought templates through data
- feed dialogue thoughts into the same ranked stream as beacons and quest guidance

Example expectation:
- grandpa asks for water in `DLG_GrandPa_Door` -> emit high-priority internal hint

Hard rule:
- do not key logic by localized dialogue text
- use stable identifiers (tree/node context)

See dedicated details in:
- `dialogue_integration.md`
- `boundaries.md`
