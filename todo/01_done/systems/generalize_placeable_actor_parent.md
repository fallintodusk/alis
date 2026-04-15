# Generalize Placeable Actor Parent Plan

## Status

Completed (core implementation, integration coverage, and validation checks landed)

Validated on 2026-02-24:
- `scripts/ue/build/build.bat AlisEditor Win64 Development` -> success
- `ProjectIntegrationTests.ObjectParentGeneralization` -> 17/17 pass
- `scripts/ue/test/integration/validate_spawnclass_cook.bat` -> success
- `scripts/ue/check/validate_legacy_object_parent_generalization.bat` -> success
- `ProjectIntegrationTests.UI.Framework.WidgetBinder.RequiredValidation` -> pass

## Goal

Allow placeable ObjectDefinition-based objects to use any `AActor` parent class, not only `AInteractableActor`, while preserving current behavior for existing content.

## Problem Statement

Current object spawn and sync flow is coupled to `AInteractableActor` and `AProjectWorldActor` assumptions:

- Spawn defaults to `AInteractableActor`.
- Mesh and capability assembly path runs fully only for `AInteractableActor`.
- Definition sync iterates `AProjectWorldActor` actors.
- Interaction routing is actor-oriented and not guaranteed for non-project base classes.

This blocks valid use cases where the desired parent is another actor type.

## Scope

In scope:

- ObjectDefinition spawn parent generalization.
- Backward-compatible ObjectDefinition schema and parser extension.
- Safe spawn and sync support for non-`AInteractableActor` parent classes.
- Component-based interaction routing fallback.

Out of scope:

- Full gameplay redesign of capabilities.
- Removing `AInteractableActor` (kept as compatibility path).
- Large content migrations during initial rollout.

## SOLID Design Direction

Use composition over inheritance:

- Keep `AInteractableActor` as compatibility wrapper.
- Introduce a host contract for definition state.
- Introduce component-based interaction routing.
- Make spawn assembly target generic `AActor` plus explicit contracts.

Key principle:

`ObjectDefinition` defines what to assemble. Actor inheritance does not define whether assembly is possible.

## Legacy Marking Contract (Required)

All temporary compatibility behavior must be explicitly marked in both code and plugin-relative docs.

Code marking standard:

- Use marker token `LEGACY_OBJECT_PARENT_GENERALIZATION`.
- Required format:
  - `// LEGACY_OBJECT_PARENT_GENERALIZATION(L###): <reason>. Remove when <condition>.`
- `L###` is a stable legacy ID (for example `L001`).

Documentation marking standard:

- Each affected plugin README must include a `## Legacy Paths` section.
- Each legacy entry must include:
  - legacy ID,
  - file/location,
  - why it still exists,
  - exact removal trigger.

Minimum plugin docs to update during this effort:

- `Plugins/Resources/ProjectObject/README.md`
- `Plugins/Editor/ProjectPlacementEditor/README.md`
- `Plugins/Gameplay/ProjectInteraction/README.md`
- `Plugins/World/ProjectWorld/README.md`

Tracking requirement:

- Keep a legacy registry table in this TODO and maintain it through all phases.
- Do not leave placeholder entries (`TBD`) after Phase 0 completion.

## Legacy Registry (Maintain During Implementation)

| Legacy ID | Location | Reason | Remove Condition | Owner | Status |
|-----------|----------|--------|------------------|-------|--------|
| L001 | `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp` (`SpawnObjectFromDefinitionInternal`: default class fallback to `AInteractableActor`) | Backward compatibility for definitions without `spawnClass` | Remove after migration window once all targeted definitions set explicit parent and smoke tests pass without fallback | ProjectObject | Active |
| L004 | `Plugins/World/ProjectWorld/Source/ProjectWorld/Public/ProjectWorldActor.h` and `Plugins/World/ProjectWorld/Source/ProjectWorld/Private/ObjectDefinitionHostHelpers.cpp` (`AProjectWorldActor` direct field authority branch) | Definition metadata hosting is inheritance-based and still required by legacy flow | Remove when metadata ownership is moved to `IObjectDefinitionHostInterface`/`UObjectDefinitionHostComponent` and legacy bridge is deleted | ProjectWorld | Active |

## Legacy Traceability Gates (Required)

Each legacy compatibility path must satisfy all gates:

1. Code gate:
   - Mark the exact branch with `LEGACY_OBJECT_PARENT_GENERALIZATION(L###)`.
   - Add a concise removal condition in the same comment.
2. TODO gate:
   - Add/update the `L###` row in this file with concrete location and owner.
   - No `TBD` values allowed after implementation starts.
3. Plugin-doc gate:
   - Add the same `L###` to the owning plugin README `Legacy Paths` section.
   - Include exact file path and removal trigger text matching code intent.
4. Cleanup gate:
   - Every `L###` must include a verification step proving safe removal.
   - Removal task must reference the same `L###` IDs.

Definition of done for this refactor:

- No untagged legacy branches remain in touched code.
- No legacy entries exist only in code or only in docs; mapping is bidirectional.
- `Legacy Paths` sections exist and are populated in all required plugin-relative docs.

## Non-Negotiable Host Contract

Generalized spawned actors must provide exactly one host capability:

- Implements `IObjectDefinitionHostInterface`, or
- Owns `UObjectDefinitionHostComponent` implementing `IObjectDefinitionHostInterface`.

Host responsibilities:

- Store and expose `ObjectDefinitionId`.
- Store and expose `AppliedStructureHash`.
- Store and expose `AppliedContentHash`.

Spawner responsibility:

- Ensure host exists before applying definition metadata.
- If missing and allowed, inject/register host component.
- If missing and not injectable, fail with clear error.

## Target Architecture

### 1) Spawn Class Selection (Data Driven + Cook Safe)

Add optional spawn class to ObjectDefinition:

- Schema field: `spawnClass`.
- C++ field: `TSoftClassPtr<AActor> SpawnClass`.

Resolution order:

1. Explicit runtime override argument.
2. Definition `spawnClass`.
3. Fallback `AInteractableActor` (legacy default).

Cook/packaging requirement:

- `spawnClass` references must be retained for cooked builds through AssetManager or equivalent cook rules.
- On class load failure, log explicit error and fallback reason.

### 2) Definition Host Contract

Create reusable host abstraction:

- `IObjectDefinitionHostInterface`.
- `UObjectDefinitionHostComponent`.

Compatibility:

- `AProjectWorldActor` can implement host directly.
- Non-`AProjectWorldActor` types can use host component.

### 3) Attachment Policy

Root replacement is forbidden by default. Attachment needs explicit policy:

- Default: attach generated meshes to actor root.
- Override option A: attach to first component tagged `ObjectAttachRoot`.
- Override option B: optional definition field `attachToComponentTag`.

If target attach component is missing:

- Log warning and fallback to root.

### 4) Generic Actor Assembly

Refactor spawn assembly to work for generic `AActor`:

- Mesh creation and attachment with attachment policy.
- Capability component creation independent of `AInteractableActor`.
- Definition metadata written via host contract.
- Optional interaction cache refresh via router interface/component if present.

### 5) Interaction Routing by Composition

Add component-aware interaction entry:

- If actor implements interactable target interface, use actor path.
- Else scan components for router/handler interface and use it.
- Else fail interaction cleanly.

This is required for parents that cannot be modified to forward interface calls.

### 6) Sync System Generalization

Refactor editor sync actor discovery to host-based filtering:

- No strict dependence on `TActorIterator<AProjectWorldActor>`.
- Match actors by `IObjectDefinitionHostInterface` data.
- Keep current reapply/replace behavior and update modes.

### 7) Spawn Class Security Policy

Prevent unsafe class usage:

- Class must derive from `AActor`.
- Class must not be abstract.
- Block known forbidden framework classes (for example world settings, game mode classes).
- Optionally require host availability or host component injection success.

## Implementation Plan

## Phase 0 - Baseline and Safety

- [x] Build regression checklist from current behavior:
  - [x] Lockable + Hinged chain behavior.
  - [x] Pickup flow through inventory.
  - [x] Dialogue capability behavior.
  - [x] Definition reapply and replace.
- [x] Capture baseline logs for interaction and actor sync.
- [x] Initialize legacy registry IDs for known compatibility paths before first code change.
- [x] Define and document the legacy marker format in team-facing notes for this refactor.
- [x] Replace all legacy registry placeholders with concrete file paths and owners (no `TBD`).

## Phase 1 - Data Model Extension

- [x] Extend `object.schema.json` with optional `spawnClass`.
- [x] Add `TSoftClassPtr<AActor> SpawnClass` to `UObjectDefinition`.
- [x] Parse and normalize `spawnClass` in definition parser.
- [x] Validate class constraints (actor, non-abstract, not forbidden type).
- [x] Document cook expectation for `spawnClass` references.
- [x] Keep schema backward compatible.
- [x] Mark and register any temporary parser/schema compatibility branches with `L###` IDs. (No additional parser legacy branch retained.)

## Phase 2 - Definition Host Contract

- [x] Add `IObjectDefinitionHostInterface` interface.
- [x] Add `UObjectDefinitionHostComponent`.
- [x] Integrate host path into existing `AProjectWorldActor` flow.
- [x] Add host helper utilities for get/set id and hashes.
- [x] Define replication/editor persistence policy for host data.
- [x] Tag and document any legacy host-bridge code paths (`L###` in code + README + TODO registry).

## Phase 3 - Generic Spawn Core

- [x] Refactor ObjectSpawnUtility to use host contract.
- [x] Ensure host exists (direct or injected) before metadata write.
- [x] Implement attachment policy:
  - [x] root default,
  - [x] `ObjectAttachRoot` tag override,
  - [x] optional `attachToComponentTag` override.
- [x] Keep legacy fallback behavior when `spawnClass` not set.
- [x] Add class resolution and fallback logs.
- [x] Mark each retained legacy branch in spawn code with `LEGACY_OBJECT_PARENT_GENERALIZATION(L###)` comments.
- [x] Mirror every spawn legacy ID into `Plugins/Resources/ProjectObject/README.md` `Legacy Paths`.

## Phase 4 - Interaction Routing Extraction

- [x] Introduce reusable interaction routing path for capability dispatch. (Implemented directly in `UInteractionComponent`; no separate router component kept.)
- [x] Keep `AInteractableActor` behavior while generic interaction resolves through component-aware selection.
- [x] Update interaction system to do component-aware lookup when actor interface is absent.
- [x] Add tests for actor path and component fallback path.
- [x] Mark retained legacy interaction paths with explicit legacy IDs in code comments. (No active legacy interaction branch retained.)
- [x] Mirror every interaction legacy ID into `Plugins/Gameplay/ProjectInteraction/README.md` `Legacy Paths`.

## Phase 5 - Sync Subsystem Generalization

- [x] Refactor actor discovery to host contract filtering.
- [x] Preserve update modes:
  - [x] Mode 0 manual countdown.
  - [x] Mode 1 passive load.
  - [x] Mode 2 batch save path.
- [x] Keep ObjectDefinition replace behavior for structural mismatch.
- [x] Mark retained legacy sync behavior in code with legacy IDs.
- [x] Mirror every sync legacy ID into `Plugins/World/ProjectWorld/README.md` `Legacy Paths`.

## Phase 6 - Pilot on Non-Interactable Parent

- [x] Add one pilot actor class outside `AInteractableActor` inheritance chain.
- [x] Set pilot ObjectDefinition `spawnClass`.
- [x] Verify:
  - [x] spawn and assembly,
  - [x] interaction routing,
  - [x] capability behavior,
  - [x] definition sync.

## Phase 7 - Docs and Migration

- [x] Document host contract, attachment policy, and `spawnClass`.
- [x] Keep legacy fallback for at least one release cycle.
- [x] Add follow-up cleanup task after validation window.
- [x] Add `Legacy Paths` sections to required plugin READMEs with mapped legacy IDs.
- [x] Ensure each legacy code marker ID appears in plugin docs and in this TODO registry.
- [x] Add explicit cleanup playbook: per-ID removal steps and verification checklist.
- [x] Verify `Plugins/Editor/ProjectPlacementEditor/README.md` includes placement/editor-only legacy IDs.
- [x] Perform final legacy traceability audit: code markers <-> TODO registry <-> plugin README sections (exact ID match).

Cleanup playbook tasks:
- [x] `L001` removal follow-up: remove fallback to `AInteractableActor` after all targeted definitions define explicit `spawnClass` and fallback-free smoke tests pass.
- [x] `L004` removal follow-up: migrate remaining inheritance-host metadata to host component/interface path, then delete legacy metadata block from `AProjectWorldActor`.
- [x] Removal verification: rerun `ProjectIntegrationTests.ObjectParentGeneralization`, cook validation, and legacy audit script after each `L###` cleanup.

## Testing Plan

## Unit/Automation

- [x] Spawn class resolution tests (override > definition > fallback).
- [x] Schema/parser tests for `spawnClass` and `attachToComponentTag`.
- [x] Host contract tests for actor and component implementations.
- [x] Interaction router tests for priority chain and component fallback.
- [x] Add cooked-build validation path for `spawnClass` (`scripts/ue/test/integration/validate_spawnclass_cook.bat`).
- [x] Security validation tests for forbidden/abstract classes.
- [x] Add audit check (script or grep rule) to detect legacy markers and compare against registry IDs.
- [x] Add audit check for plugin docs to ensure every `L###` appears in corresponding README `Legacy Paths`.

## Integration

- [x] Editor drag-drop placement from ProjectPlacementEditor.
- [x] Runtime spawn via `IObjectSpawnService`.
- [x] Definition regenerate to actor sync (reapply and replace).
- [x] World Partition passive update path.

## Manual Smoke

- [x] Existing door prefab behavior unchanged.
- [x] Existing pickup behavior unchanged.
- [x] Existing dialogue-on-object behavior unchanged.
- [x] New non-`AInteractableActor` parent works end to end.
- [x] Legacy marker audit: all known compatibility paths are tagged and documented.
- [x] Legacy cross-doc audit: no `L###` mismatch between code, TODO registry, and plugin READMEs.

## Risks and Mitigations

Risk: interaction regressions.
Mitigation: preserve compatibility wrapper and add actor-plus-component path tests.

Risk: host not present on spawned class.
Mitigation: mandatory host contract and explicit injection/failure policy.

Risk: root/attachment issues on character-like actors.
Mitigation: explicit attachment policy with deterministic fallback and logs.

Risk: missing Blueprint spawn class in cooked builds.
Mitigation: AssetManager cook retention rules plus cook-time test.

Risk: unsafe class spawning.
Mitigation: strict class validation and forbidden class filters.

## Acceptance Criteria

- [x] Existing ObjectDefinition content works without changes.
- [x] At least one ObjectDefinition spawns on a non-`AInteractableActor` parent.
- [x] Interaction works for both actor-interface and component-fallback entry paths.
- [x] Definition sync works for both legacy and host-component actors.
- [x] Cooked build spawns `spawnClass` Blueprint correctly.
- [x] Host metadata behavior in multiplayer is correct by policy (replicated or editor-only by design).
- [x] ACharacter-like pilot attaches visuals to expected attach root by policy.
- [x] Instance-added host component persists correctly after editor save/load.
- [x] World Partition sync finds both legacy and host-component actors reliably.
- [x] No required direct inheritance from `AInteractableActor` for new placeable object types.
- [x] Every retained compatibility path has `LEGACY_OBJECT_PARENT_GENERALIZATION(L###)` in code.
- [x] Every legacy ID is listed in this TODO registry and in the relevant plugin README `Legacy Paths` section.
- [x] Every legacy ID has a concrete removal trigger and cleanup verification notes.
- [x] No placeholder legacy entries (`TBD`) remain in this TODO after implementation starts.

## Deliverables

- Archived completed plan in `todo/done`.
- Ordered implementation checklist with explicit contracts.
- Validation checklist that covers cook, multiplayer, interaction entry, and sync.
- Legacy registry with tracked IDs and removal triggers.
- Plugin-relative legacy documentation sections ready for future cleanup refactor.
- Legacy traceability matrix proving one-to-one mapping across code comments, TODO IDs, and plugin docs.
