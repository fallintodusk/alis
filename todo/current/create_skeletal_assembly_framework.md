# Create Skeletal Assembly Framework

Decision-complete architecture note for a new modular skeletal assembly framework.

Status: planning only. This document defines the target architecture and migration path. It does not authorize code or asset edits by itself.

Last updated: 2026-03-31

---

## Triggering Investigation Snapshot

This framework plan was triggered by the recent first-person local-body clipping investigation.

The important architectural takeaways were:

- the local first-person body bug exposed unstable mixed ownership between Blueprint defaults, C++ runtime policy, and Mutable rebuild state
- repeated attempts to repair the issue with local post-copy anim math proved that the deeper problem was architecture shape, not only tuning
- repeated PIE forensics on this PC were stable enough that the issue was not best explained as random init drift here
- the recovered `BP_Hero_Motion` camera path showed that important behavior had lived in Blueprint asset state, not in a clear reusable runtime layer
- this became the concrete example that justified moving skeletal assembly and lifecycle orchestration into a reusable C++ framework

The practical result is: preserve the legacy Artur-era path as baseline, but stop extending the mixed Blueprint/C++ ownership model and replace it with an explicit skeletal assembly framework.

---

## 1. Goal

Create a strong unified C++ base for skeletal-actor setup, while keeping the current Artur-era character path as the legacy baseline until the new modular path reaches parity.

The new system must:

- stop relying on mixed ownership between Blueprint asset state and C++ runtime policy
- support player-character wrappers, NPC wrappers, and future non-character skeletal actors
- stay data-driven through stable IDs and definition assets, not hardcoded Blueprint graphs
- keep Motion Matching and Mutable content reusable without rewriting their content graphs into C++
- allow clean runtime switching between legacy and modular paths through project-specific console commands
- provide built-in debug capture so parity and regressions can be compared quickly

In this model, a character is a wrapper that consumes a skeletal assembly definition plus capability specs, not the identity of the framework itself.

This is not a "rewrite all character Blueprints into C++" task.

This is a "move orchestration, lifecycle, assembly, and switching into C++, while keeping content assets data-driven" task.

---

## 2. Why This Is Needed

The current first-person investigation exposed a structural problem, not just a bug.

### 2.1. Current ownership is mixed and fragile

Current `BP_Hero` is not a thin presentation wrapper. It still owns important runtime behavior:

- movement tuning variables and gait thresholds
- input-state replication variables
- `SetupCamera`
- multiple `UpdateSkeletalMeshAsync` calls
- `SetLeaderPoseComponent`
- local-body related component graph

At the same time, `AProjectCharacter` owns:

- camera component creation
- driver/world/local mesh components
- local-body visibility logic
- Mutable rebuild recovery
- local-body anim installation

This means real behavior is split across:

- C++ constructor state
- Blueprint component defaults
- Blueprint event graph calls
- Mutable async rebuild state
- local runtime possession state

That is the exact shape that created the recent mismatch between remembered behavior and reproduced behavior.

### 2.2. The repo already proved this is not just random init drift

Repeated PIE forensics on this machine showed effectively stable runtime state:

- camera parent remained `CollisionCylinder`
- `WorldBodyMesh` and `LocalBodyMesh` remained attached to `CharacterMesh0`
- local anim install path fired deterministically after spawn and after Mutable rebuild

So the current problem is not best explained as "sometimes init differs on this PC".

It is better explained as:

- the current committed runtime shape is stable enough
- but that stable shape is wrong for the intended first-person behavior

### 2.3. Git and Blueprint forensics proved a real behavior difference existed

Recovered `BP_Hero_Motion` showed a real older camera path that current `BP_Hero` does not have:

- `UserConstructionScript` attached `FirstPersonCamera` to `Head`
- socket name was `spine_05`
- then applied local offset `(15, 20, 0)`

Current `BP_Hero` has no equivalent construction-script camera attach path.

This is the clearest proof that:

- behavior changed in a meaningful way
- the current runtime path is not the same effective path Artur demonstrated

Therefore the solution is not "more ad hoc bone math inside LocalBodyAnimInstance".

The solution is a new framework with strict ownership and explicit assembly policy.

---

## 2.4. Current Blueprint landscape confirms the wrapper is too heavy

Current relevant hero Blueprints in this repo:

- `BP_Hero`
- `BP_Hero_Motion`
- `BP_Hero_Metahuman`

Observed `BP_Hero` component and behavior footprint includes:

- `FirstPersonCamera`
- `CharacterMesh0`
- `WorldBodyMesh`
- `LocalBodyMesh`
- `Body_CSK`
- `Head_CSK`
- `Local_Body_CSK`
- `MotionWarping`
- `AC_PreCMCTick`
- `AC_TraversalLogic`
- `AC_FoleyEvents`
- `AC_SmartObjectAnimation`
- `BP_VisualOverrideManager`
- `LODSync`

This confirms the current wrapper is not a thin skin around a stable C++ base.

It is a mixed runtime owner that combines:

- skeletal assembly concerns
- gameplay-side movement and traversal concerns
- audio and smart-object integration
- local first-person presentation concerns
- customization rebuild concerns

That is exactly why the new framework must separate generic skeletal assembly from gameplay wrapper logic.

---

## 3. Placement And Naming

### 3.1. New plugin

Create a new plugin:

- `Plugins/Systems/ProjectSkeletalAssembly`

Do not name it `ProjectCharacterFramework`.

Reason:

- the intended core is not only for player characters
- the repo already treats `Systems/` as the place for reusable runtime services and reusable runtime infrastructure
- character-specific policy belongs above the core, not inside it

This follows existing repo structure:

- `ProjectMotionSystem` in `Plugins/Systems/` already owns skeleton-agnostic motion primitives
- `ProjectAnimation` in `Plugins/Resources/` already owns skeleton-specific animation assets and thin runtime glue
- `ProjectObjectCapabilities` already owns stable-ID capability registration
- `ProjectSinglePlay` already owns mode and pawn selection orchestration

### 3.2. Existing plugins keep their roles

Keep:

- `Plugins/Gameplay/ProjectCharacter` as the legacy gameplay-side character adapter and baseline
- `Plugins/Resources/ProjectAnimation` as the skeleton-specific asset library
- `Plugins/Resources/ProjectObject` as the content-definition owner
- `Plugins/Gameplay/ProjectSinglePlay` as the mode and spawn switch host

Do not fold the new core into `ProjectCharacter`.

Do not create a parallel content-definition owner outside `ProjectObject`.

### 3.3. Plugin category

Use a systems-style category in the new `.uplugin`.

Recommended:

- `"Category": "Framework.Project"`

Reason:

- matches the intent used by `ProjectMotionSystem`
- communicates reusable runtime framework, not game-specific content

---

## 4. Final Ownership Model

### 4.1. Core rule

Assets choose configuration.

C++ owns runtime policy.

That means the new core owns:

- component graph assembly
- attachment policy
- visibility policy
- lifecycle state transitions
- capability registration and activation
- Mutable rebuild orchestration
- debug capture and comparison output
- legacy/modular switching support hooks

Assets and Blueprints keep:

- Motion Matching graphs
- retarget AnimBPs
- Mutable content graphs
- skeleton-specific asset choices
- designer-tuned curves and thresholds that are still specific to the legacy wrapper

### 4.2. What moves first to C++

These responsibilities move first into the new framework or wrapper code:

- camera attachment policy
- ownership of skeletal component graph
- local body vs world body visibility policy
- Mutable rebuild recovery
- lifecycle state machine around assembly readiness
- debug capture and structured runtime snapshots

### 4.3. What stays legacy for v1

These stay in the legacy path until parity is proven:

- Motion Matching AnimBP graphs
- retarget AnimBPs
- Mutable content graphs and authoring
- current `BP_Hero` movement tuning curves and thresholds
- traversal heuristics
- foley and smart-object behavior unless needed for wrapper parity

### 4.4. Core must stay dependency-clean

`ProjectSkeletalAssembly` must not hard-depend on:

- GAS
- Vitals
- Mutable
- Motion Matching content
- specific skeletons
- specific bone names
- project-specific gameplay capabilities

It may depend on:

- `ProjectCore`
- Engine runtime modules needed for generic component assembly and diagnostics

It may optionally expose integration points that adapters use from outside the core.

---

## 5. New Core Types And Contracts

The later implementation must define the following public types.

### 5.1. `USkeletalAssemblyComponent`

Main runtime host for the assembled skeletal actor.

Responsibilities:

- owns the assembly state machine
- creates or resolves runtime component graph from definition
- tracks registered skeletal capabilities
- applies attachment and visibility policy
- exposes runtime inspection state for debug capture

This is the main reusable host for:

- player-controlled characters
- AI/NPC skeletal actors
- future non-character skeletal mechanisms

### 5.2. `FSkeletalCapabilitySpec`

Stable-ID capability entry stored in the definition.

Fields must include:

- `CapabilityId`
- capability-local config payload
- optional target component IDs
- enabled flag
- ordering / phase hint if needed

This mirrors the existing capability pattern:

- stable identifier in data
- runtime class resolution through registry

### 5.3. Capability registry: one registry, one path

#### 5.3.1. Core principle

All capabilities -- object and skeletal -- use the same registry: `FCapabilityRegistry`.

A door capability and a skeletal capability are the same concept. JSON declares what the thing has, the registry finds the class, the framework attaches it. The difference is not concept -- it is runtime lifecycle. Some actors need orchestration (hero with assembly phases), most don't (door, NPC with just Mutable).

There is no separate skeletal registry.

#### 5.3.2. Registry with shared kernel

`FCapabilityRegistry` uses `FRegisteredClassScan` kernel from ProjectCore for CDO scan mechanics.

```
ProjectCore (Foundation/)
  Public/Registry/RegisteredClassScan.h   -- stateless scan kernel
  Private/Registry/RegisteredClassScan.cpp

ProjectObjectCapabilities (Gameplay/)
  Public/CapabilityRegistry.h             -- one registry for all capabilities
  Private/CapabilityRegistry.cpp
```

Built-in modules (`ProjectObjectCapabilities`, `ProjectMotionSystem`) are hardcoded in the registry scan config -- they ship with the registry and are always loaded.

External plugins (skeletal adapters, future capability packs) call `RegisterCapabilityModule()` in their `StartupModule()` so the registry discovers their classes without hardcoding their names.

#### 5.3.3. How external capability modules register

```cpp
// In any adapter plugin's StartupModule()
FCapabilityRegistry::RegisterCapabilityModule(TEXT("MyAdapterPlugin"));
```

If called after the registry has already been built, it auto-invalidates so the next lookup rescans.

#### 5.3.4. Capability IDs

All capabilities use `FPrimaryAssetId("CapabilityComponent", "MyId")`.

Object capabilities (existing):

| ID | Purpose |
|----|---------|
| `Lockable` | Access control |
| `Pickup` | World pickup |
| `Hinged` | Hinged door motion |
| `Sliding` | Sliding motion |
| `LootContainer` | Container interaction |
| `Audio` | Spatial audio |
| `ActorWatcher` | Event observation |

Skeletal capabilities (new, from adapter plugin):

| ID | Purpose |
|----|---------|
| `MotionMatching` | Motion matching animation driver |
| `MutableCustomization` | Mutable body customization rebuild orchestration |
| `LocalFirstPerson` | Owner-only first-person body handling |
| `DebugCapture` | Runtime debug capture and overlay |

Same registry, same resolution, same `GetPrimaryAssetId()` contract.

#### 5.3.5. Assembly component is opt-in

Simple objects and simple skeletal actors: capability attached at spawn, no lifecycle needed.

Complex skeletal actors (hero with local/world body, rebuild phases): `USkeletalAssemblyComponent` added as opt-in orchestrator that manages capability activation, teardown, and rebuild sequencing.

The assembly component is not a gate for capabilities -- it is an optional lifecycle manager.

#### 5.3.6. Anti-patterns to avoid

- separate registry per domain -- adds complexity with no benefit
- hardcoding external module names in the registry -- external plugins must register themselves (built-in modules are fine to hardcode)
- making the registry a UObject -- unnecessary GC for a static lookup table
- forcing all skeletal actors through assembly orchestration -- simple actors don't need it

### 5.4. `USkeletalDebugCaptureComponent`

Runtime instrumentation component for:

- overlay information
- structured capture output
- screenshot sidecar metadata

This belongs in the new core because it is framework-level instrumentation, not game-specific content logic.

### 5.5. `USkeletalAssemblyDefinition`

New data asset contract for composed skeletal actors.

Ownership:

- stored in `ProjectObject`
- authored for `Human/` first
- later reusable for `Animal/`

This must intentionally mirror the existing `UObjectDefinition` philosophy:

- stable primary asset ID
- component/layout entries
- capability list by stable ID
- extensible sections for domain-specific data

---

## 6. Definition Model

### 6.1. Definition owner

Definition ownership stays in `ProjectObject`, not `ProjectCharacter`.

Reason:

- the project already uses `ProjectObject` as the composition-definition owner for game entities
- `ProjectObject` already owns stable-ID data-definition patterns
- characters are content compositions, not just one gameplay class

### 6.2. Authoring model

Extend the existing universal generator. Do not create a skeletal-specific generator or hand-maintain DataAssets as the primary authoring path.

The project data pipeline is split into three stages (see `docs/data/README.md`):

- SYNC: source data acquisition
- GENERATION: `ProjectDefinitionGenerator` converts JSON + schema into DataAssets
- PROPAGATION: runtime systems consume generated assets by stable ID

`ProjectDefinitionGenerator` is documented as a universal JSON-to-DataAsset generator. Resource plugins provide:

- a runtime definition class (`USkeletalAssemblyDefinition`)
- source JSON files
- a JSON schema with an `x-alis-generator` block

The generator owns parsing, field mapping, incremental generation, and orphan cleanup.

Practical shape:

- source JSON under `Plugins/Resources/ProjectObject/Content/Human/<Name>/`
- JSON schema at `Content/Data/Schemas/SkeletalAssembly.schema.json` with `x-alis-generator` metadata
- `ProjectDefinitionGenerator` generates `USkeletalAssemblyDefinition` DataAssets from those JSON files
- no hand-maintained DataAssets as the main authoring flow
- no new generator framework or forked data pipeline

File and ID rule:

- character ID equals authored name / filename, same spirit as object definitions

### 6.3. Minimum fields of `USkeletalAssemblyDefinition`

The later implementation must include these conceptual lanes.

#### Lane A: component layout

Component layout entries define:

- component ID
- component kind
- parent component ID
- attachment policy
- default transform
- visibility role
- optional asset refs

Example roles:

- `DriverBody`
- `WorldBody`
- `LocalBody`
- `Head`
- `Camera`
- `BodyCustomization`
- `HeadCustomization`
- `LocalBodyCustomization`

The framework should not rely on hardcoded names like `CharacterMesh0` or `Local_Body_CSK`.

It should rely on stable component IDs and roles declared in the definition.

#### Lane B: capabilities

Capability entries define:

- stable capability ID
- config payload
- target component IDs if applicable

Examples:

- `MotionMatching` driver capability
- `MutableCustomization` body customization capability
- `LocalFirstPerson` owner-only first-person body capability
- `DebugCapture` capability

#### Lane C: sections

Sections hold extensible, domain-specific data.

Required v1 section categories:

- `Animation`
- `Customization`
- `View`
- `Debug`

Possible later section categories:

- `GameplayBridge`
- `Ragdoll`
- `LOD`

### 6.4. Stable-ID policy

Use the same style as `ObjectDefinition` plus `CapabilityRegistry`.

Meaning:

- definitions are referenced by stable ID
- capabilities are referenced by stable ID
- runtime classes are resolved by registry
- asset paths are payload details, not orchestration identity

---

## 6.5. Relation to the existing capability pattern

Skeletal capabilities ARE object capabilities. Same registry, same contract, same `GetPrimaryAssetId("CapabilityComponent", ...)` pattern. See section 5.3.

The only difference is runtime lifecycle:

- simple capabilities (door, chest, simple NPC): attach at spawn, done
- orchestrated capabilities (hero): `USkeletalAssemblyComponent` manages activation, teardown, rebuild

This means:

- `ProjectObject` remains the owner of skeletal definition data
- all capability IDs live in one `FCapabilityRegistry`
- runtime assembly is opt-in via `USkeletalAssemblyComponent`, not forced
- a simple NPC with just `MutableCustomization` works the same as a chest with `LootContainer`

Motion Matching and Mutable are not "player character only" technologies. They can apply to NPCs, creatures, and future skeletal actors. The one-registry model makes this natural.

---

## 7. Adapter Strategy

### 7.1. Core vs adapters

The new core provides generic assembly and lifecycle.

Capability-specific behavior stays in adapters outside the core.

Initial adapters to support later:

- Motion Matching adapter
- Mutable adapter
- local first-person body adapter
- debug capture adapter

### 7.2. Adapter residency

Capability implementations live in dedicated plugins, not in character wrappers.

Placement:

- `FCapabilityRegistry` (registry owner) lives in `ProjectObjectCapabilities`
- built-in object capabilities (Lockable, Pickup, Hinged, etc.) live in `ProjectObjectCapabilities`
- skeletal adapter capabilities live in ONE bridge plugin: `ProjectSkeletalCapabilities` (Gameplay tier)
- `DebugCapture` and `LocalFirstPerson` may live in `ProjectSkeletalAssembly` if they have no third-party deps

```
ProjectObjectCapabilities (Gameplay/)
  FCapabilityRegistry                    -- registry owner
  Lockable, Pickup, Hinged, etc.         -- built-in capabilities

ProjectSkeletalCapabilities (Gameplay/)  -- ONE bridge plugin
  depends on: ProjectObjectCapabilities  -- for RegisterCapabilityModule()
  depends on: PoseSearch                 -- for Motion Matching
  depends on: CustomizableObject         -- for Mutable
  MotionMatchingCapability               -- adapter
  MutableCustomizationCapability         -- adapter
  (future skeletal adapters here)
```

Each adapter plugin calls `FCapabilityRegistry::RegisterCapabilityModule()` in `StartupModule()`.

This means:

- no capability classes in `ProjectCharacter`
- no plugin-per-adapter sprawl (one bridge plugin for all skeletal adapters)
- third-party deps isolated in the bridge plugin, not in registry or core
- any definition can reference any capability by stable ID
- capabilities are reusable by NPCs, creatures, and future skeletal actors

### 7.3. Legacy wrapper rule

Keep `ProjectCharacter` as the legacy gameplay-facing character plugin.

The new modular character will be a separate pawn class that consumes `ProjectSkeletalAssembly`. Once the modular path reaches parity, the legacy path in `ProjectCharacter` can be retired.

Do not extend `ProjectCharacter` with new skeletal assembly behavior. New behavior goes into the framework or into capability-owner plugins.

---

## 8. Legacy Baseline Rule

Phase 0 is mandatory:

- restore and freeze the legacy baseline around the Artur merge
- stop adding new first-person math experiments to `LocalBodyAnimInstance`

Important interpretation:

- legacy is the compatibility baseline
- modular is the new implementation under comparison

Default runtime behavior remains legacy until modular parity exists.

`BP_Hero_Motion` is a forensic reference, not the production default.

Its recovered camera attach path is useful because it proves a real older behavior existed, but it should not become the new long-term architecture by itself.

---

## 9. Runtime Switching

### 9.1. Switching model

Choose respawn-based switching.

Do not attempt hot live mutation of the active pawn graph.

Do not require map travel for each switch.

Reason:

- safer than in-place mutation
- much faster for parity testing than full travel
- compatible with `ProjectSinglePlay` ownership of mode and pawn selection

### 9.2. Command namespace

Use:

- `project.character.switch legacy`
- `project.character.switch modular`

Optional later:

- `project.character.switch modular Hero`

### 9.3. Switch host

Switching is hosted by `ProjectSinglePlay`.

Reason:

- `ProjectSinglePlay` already owns data-driven pawn selection
- `FSinglePlayModeConfig` already owns default pawn configuration
- single-player runtime already decides which pawn class is spawned

### 9.4. Command routing

Later implementation should split responsibilities like this:

- command parsing lives in client-facing single-player code
- authoritative switch application lives in single-player runtime orchestration

Practical split:

- local exec or console command entry in `ProjectSinglePlayClient`
- runtime application and respawn in `ProjectSinglePlay`

### 9.5. Switch behavior

Switching must:

1. set the active character system selection
2. choose the corresponding pawn or definition path
3. perform a clean respawn
4. preserve gameplay session when possible
5. log the selected system and resulting pawn/definition

Do not mutate a live pawn from legacy to modular in place.

---

## 10. Debug And Capture

### 10.1. Commands

Later implementation must provide:

- `project.character.debug 0`
- `project.character.debug 1`
- `project.character.capture [label]`

### 10.2. Debug mode requirements

Debug mode must support:

- detached debug camera while the observed character continues ticking and animating
- overlay text with high-signal state
- compare-friendly runtime capture output

The intended behavior is "inspect the active character from outside while it still simulates", not "pause and inspect components manually in the editor".

### 10.3. Overlay fields

Overlay must show at minimum:

- active system: `legacy` or `modular`
- pawn class
- definition ID if modular
- capability IDs active in the assembly
- camera parent component and socket
- main skeletal component parents and sockets
- skeletal mesh asset names
- anim class and anim instance class per relevant mesh
- Mutable instance identity if applicable
- selected bone transforms for quick comparison

Selected bones for first-person comparison:

- `root`
- `pelvis`
- `spine_03`
- `spine_05`
- `neck_01`
- `head`

If a later modular definition uses different skeleton naming, the debug adapter must expose an equivalent mapped set.

### 10.4. Capture output

`project.character.capture [label]` must produce:

- screenshot image
- structured sidecar text or JSON
- console-readable summary lines

Target output folder:

- `Saved/Validation/CharacterDebug/`

Filename requirements:

- timestamp
- active system
- profile or definition ID
- optional label

Example intent:

- `2026-03-31_22-15-03_legacy_BP_Hero_stoplean.png`
- `2026-03-31_22-15-03_legacy_BP_Hero_stoplean.json`

### 10.5. Sidecar content

Structured capture must include at minimum:

- system ID
- pawn class
- definition ID if any
- capability list
- camera parent/socket/transform
- component parent/socket/mesh/anim state
- selected bone transforms
- current movement / locomotion summary if adapter exposes it
- current Mutable instance / generated mesh identifiers if applicable

This is required so regressions can be compared without relying only on screenshots or raw log hunting.

---

## 11. Migration Phases

### Phase 0 - Freeze legacy

- restore and freeze the legacy baseline around the Artur-era merged behavior
- stop experimental local-body math changes
- keep `ProjectCharacter` as current production path

Exit criteria:

- legacy remains default
- no new modular code is required yet

### Phase 1 - Scan kernel + capability registry upgrade + `ProjectSkeletalAssembly` plugin

Phase 1a: add `FRegisteredClassScan` kernel to `ProjectCore/Public/Registry/`.

Phase 1b: migrate `FCapabilityRegistry` internals to use `FRegisteredClassScan`. Add `RegisterCapabilityModule()`, `DumpToLog()`, `ForEach()`, `Num()`. Public API backward-compatible.

Phase 1c: create `ProjectSkeletalAssembly` in `Plugins/Systems/` with `USkeletalAssemblyComponent`.

This phase adds:

- `FRegisteredClassScan` shared kernel in ProjectCore
- `FCapabilityRegistry` migrated onto kernel with `RegisterCapabilityModule()` for external plugins
- `USkeletalAssemblyComponent` (opt-in lifecycle orchestrator)
- `ProjectSkeletalAssembly` plugin (no separate registry -- uses `FCapabilityRegistry`)

This phase does not add:

- Mutable content behavior
- Motion Matching behavior
- character-specific heuristics
- separate skeletal capability registry (not needed -- one registry for all)

Test gate (Phase 1):

- compile succeeds
- existing object spawn flow works unchanged (spawn a door/chest with capabilities in PIE)
- `FCapabilityRegistry::DumpToLog()` shows the same 8 entries as before migration
- `USkeletalAssemblyComponent` can be added to a test actor in PIE without crash
- assembly state machine initializes to idle state
- automation test: lifecycle state transitions (idle -> assembling -> ready -> teardown)

### Phase 1.5 - JSON body decision (DECIDED)

#### 1.5.1. Questions answered

All open questions have been researched and decided:

| Question | Decision | Reason |
|----------|----------|--------|
| Separate definition class? | No. Extend `UObjectDefinition`. | Characters ARE objects in ALIS. GrandPa NPC already uses ObjectDefinition with skeletal mesh + animClass. One definition class, one schema, one generator. |
| Separate `ComponentLayout`? | No. Use existing `meshes` array. | Existing `FObjectMeshEntry` already supports skeletal meshes, parent hierarchy, animClass, materials. Only needs 3 optional fields: `kind`, `role`, `visibility`. |
| Separate capabilities format? | No. Same `capabilities` array, same `type`/`scope`/`properties` format. | One registry, one pattern. Skeletal capabilities are just capabilities. |
| How is assembly expressed? | `SkeletalAssembly` is a capability in the capabilities array. | Industry standard: flat peer coordinator (Lyra, Fortnite ASC, Overwatch). Not a top-level field, not a wrapper. |
| Separate sections mechanism? | No. Same `TMap<FName, FInstancedStruct>` as Item and Storage. | Add new section types: Animation, Customization, View. Same pattern. |
| Camera in meshes? | No. Camera goes in `view` section. | Camera is view policy, not a visual body piece. |
| Extend ObjectSpawnUtility? | Yes. Add kind/role/visibility handling + assembly lifecycle routing. | One spawn path. When SkeletalAssembly capability is present, route other capabilities through assembly lifecycle instead of simple attach. |

#### 1.5.2. Why these decisions

Three alternative approaches were considered and rejected:

**Rejected: Separate `USkeletalAssemblyDefinition` class**

Would mean two definition types, two schemas, two generator entries, two spawn paths. GrandPa (simple skeletal NPC) would use ObjectDefinition while Hero would use SkeletalAssemblyDefinition -- but they're both characters, both objects, both have skeletal meshes and capabilities. The split adds complexity with no benefit.

**Rejected: Camera as a mesh entry**

Camera is not a visual component. It has no asset, no materials, no physics. Putting it in `meshes` pollutes the mesh array with non-mesh entries. Camera attachment and mode belong in the `view` section as policy configuration.

**Rejected: Assembly as implicit (detect from `role` presence) or top-level field (`"assembly": "skeletal"`)**

Industry research across Lyra, Fortnite, Overwatch, Unity, and Game Programming Patterns shows the universal pattern: the orchestrator is a flat peer component, not a wrapper and not an implicit behavior. Making `SkeletalAssembly` a capability achieves this:
- Explicit in JSON (you see it in the capabilities list)
- Consistent with existing patterns (it's a capability like Hinged or Lockable)
- Resolved via same registry (`FCapabilityRegistry`)
- No new JSON concepts (no top-level field)
- No magic (no implicit role-detection)

**Rejected: Wrapping capabilities inside assembly in JSON (nested model)**

Would break the flat peer coordinator pattern that every shipped game uses. Components are peers on the actor. The assembly component discovers and coordinates siblings at runtime, it does not contain them in data.

#### 1.5.3. New fields on `FObjectMeshEntry`

Three optional fields added to existing mesh entry struct:

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `kind` | `FName` | auto-detect from asset | Component type override. Needed for `CustomizableSkeletalMesh` (can't auto-detect from asset). Values: `SkeletalMesh`, `StaticMesh`, `CustomizableSkeletalMesh`. Omit for auto-detect. |
| `role` | `FName` | none | Semantic role for assembly. Values: `DriverBody`, `WorldBody`, `LocalBody`, `BodyCustomization`, `HeadCustomization`, `LocalBodyCustomization`. Only valid when `SkeletalAssembly` capability is present. |
| `visibility` | `FName` | default (all see) | Visibility policy. Values: `OwnerOnly`, `SkipOwner`. Only valid when `SkeletalAssembly` capability is present. |

Validation rule: if any mesh has `role` or `visibility` but no `SkeletalAssembly` capability is present, validation fails. No half-assembly objects.

#### 1.5.4. New sections

| Section | Purpose | Fields (v1) |
|---------|---------|-------------|
| `animation` | Locomotion and traversal config | `locomotionProfile`, `traversalProfile` |
| `customization` | Mutable source config | `mutableSource` |
| `view` | Camera and first-person policy | `defaultMode`, `cameraParent`, `attachmentPolicy`, `relativeOffset` |

Future sections: `debug`, `ragdoll`, `lod`.

#### 1.5.5. Assembly spawn behavior

When `ObjectSpawnUtility` processes capabilities:

Without `SkeletalAssembly`:
- all capabilities attached simple (fire-and-forget, current behavior)

With `SkeletalAssembly`:
1. create mesh components (using `kind`, `role`, `visibility`)
2. attach `USkeletalAssemblyComponent` (the assembly capability itself)
3. assembly reads mesh roles and builds component graph
4. other skeletal capabilities (`MotionMatching`, `MutableCustomization`, etc.) routed through assembly lifecycle
5. assembly activates them when state reaches Ready
6. assembly deactivates them on TearingDown

This matches the Lyra init state coordinator pattern: peer component manages sibling initialization ordering.

#### 1.5.6. Capability ordering rule

`SkeletalAssembly` is always processed first, regardless of JSON array order. `ObjectSpawnUtility` must:

1. scan the capabilities array for `SkeletalAssembly`
2. if found, create and initialize it before any other capability
3. defer other capabilities that target assembly-managed meshes (those with `role`) until assembly signals readiness
4. capabilities targeting `["actor"]` scope that are NOT assembly-managed are attached immediately (e.g. `Dialogue` on an NPC that also has assembly)

This ordering is enforced at spawn time, not at JSON authoring time. Authors can list capabilities in any order.

#### 1.5.7. Validation rules

Negative (already stated):

- if any mesh has `role` or `visibility` but no `SkeletalAssembly` capability exists, fail validation

Positive (required):

- if `SkeletalAssembly` capability exists, at least one mesh must have a valid `role`
- all capability `scope` entries must reference existing mesh `id` values (or `"actor"`)
- if `LocalFirstPerson` capability exists, require a mesh with `role: "LocalBody"` and a `view` section
- if `MotionMatching` capability exists, require a mesh with `role: "DriverBody"`

These are schema-level or generator-level validation rules. They prevent silently broken definitions.

#### 1.5.8. Module registration

`ProjectSkeletalAssembly` is a core systems plugin (always enabled, like `ProjectMotionSystem`). It is hardcoded in `FCapabilityRegistry::Build()` alongside the other built-in modules so the CDO scan discovers `USkeletalAssemblyComponent`.

Built-in modules (hardcoded in registry scan config):
- `ProjectObjectCapabilities` (self)
- `ProjectMotionSystem` (motion capabilities)
- `ProjectSkeletalAssembly` (assembly capability)

External adapter plugins (like `ProjectSkeletalCapabilities` when created) call `FCapabilityRegistry::RegisterCapabilityModule()` in their `StartupModule()` to self-register.

Rule: core always-enabled plugins are hardcoded. Optional/external plugins self-register.

#### 1.5.9. Final JSON examples

Hero (with assembly):

```json
{
  "$schema": "../../Schemas/object.schema.json",
  "id": "Human.Hero",
  "spawnClass": "/Game/Characters/BP_ModularHero.BP_ModularHero_C",
  "meshes": [
    { "id": "DriverBody",        "kind": "SkeletalMesh",             "asset": "/Game/Characters/Human/SKM_Hero_Body", "role": "DriverBody" },
    { "id": "WorldBody",         "kind": "SkeletalMesh",             "parent": "DriverBody", "role": "WorldBody", "animClass": "/Game/Animation/ABP_Hero_Retarget" },
    { "id": "LocalBody",         "kind": "SkeletalMesh",             "parent": "DriverBody", "role": "LocalBody", "visibility": "OwnerOnly" },
    { "id": "BodyCustomization", "kind": "CustomizableSkeletalMesh", "parent": "WorldBody",  "role": "BodyCustomization" },
    { "id": "HeadCustomization", "kind": "CustomizableSkeletalMesh", "parent": "WorldBody",  "role": "HeadCustomization" }
  ],
  "capabilities": [
    { "type": "SkeletalAssembly", "scope": ["actor"] },
    { "type": "MotionMatching",       "scope": ["DriverBody"] },
    { "type": "MutableCustomization", "scope": ["BodyCustomization", "HeadCustomization"] },
    { "type": "LocalFirstPerson",     "scope": ["LocalBody"], "properties": { "HiddenBones": "head,neck_01" } },
    { "type": "DebugCapture",         "scope": ["actor"] }
  ],
  "sections": {
    "animation":     { "locomotionProfile": "Human.Default", "traversalProfile": "Human.Parkour" },
    "customization": { "mutableSource": "/Game/Mutable/CO_Hero_Body" },
    "view":          { "defaultMode": "FirstPerson", "cameraParent": "Root", "attachmentPolicy": "CapsuleFixed", "relativeOffset": "(X=23 Y=0 Z=62)" }
  }
}
```

Simple NPC (no assembly, no lifecycle):

```json
{
  "$schema": "../../Schemas/object.schema.json",
  "id": "Human.Trader",
  "spawnClass": "/Game/Characters/BP_NPC.BP_NPC_C",
  "meshes": [
    { "id": "body", "asset": "/Game/Characters/Human/SKM_NPC_Body", "animClass": "/Game/Animation/ABP_NPC_Retarget" }
  ],
  "capabilities": [
    { "type": "MutableCustomization", "scope": ["body"] },
    { "type": "Dialogue", "scope": ["actor"], "properties": { "DialogueTreeAsset": "/Game/Dialogue/DLG_Trader" } }
  ],
  "sections": {
    "customization": { "mutableSource": "/Game/Mutable/CO_NPC_Trader" }
  }
}
```

Door (unchanged, current behavior):

```json
{
  "$schema": "../../Schemas/object.schema.json",
  "id": "Door_Inner",
  "meshes": [
    { "id": "frame", "asset": "/ProjectObject/.../SM_Frame" },
    { "id": "door",  "asset": "/ProjectObject/.../SM_Door", "parent": "frame" }
  ],
  "capabilities": [
    { "type": "Hinged", "scope": ["door"], "properties": { "OpenAngle": "-85" } }
  ]
}
```

All three use the same schema, same definition class, same generator, same spawn entry point. The only difference is complexity: door has no assembly, NPC has simple capabilities, hero has assembly-orchestrated capabilities.

### Phase 2 - Add skeletal definitions in `ProjectObject` via universal generator

Add skeletal definition support under `ProjectObject`.

Extend `ProjectDefinitionGenerator` to generate skeletal assembly DataAssets from JSON:

- add JSON schema with `x-alis-generator` block based on Phase 1.5 decisions
- add source JSON for `Human/Hero` as first definition
- verify incremental generation works through existing generator flow

Scope:

- `Human/` first
- `Animal/` later

This phase defines:

- component layout lane
- capability lane
- section lane
- stable primary asset ID

Test gate (Phase 2):

- generator discovers skeletal assembly JSON files
- generated `USkeletalAssemblyDefinition` DataAsset has correct ObjectId, component layout, capabilities, sections
- incremental regeneration: modify JSON, regenerate, verify asset updates
- orphan cleanup: delete JSON, regenerate, verify asset removed
- automation test: `FDefinitionJsonParser` parses skeletal assembly fields correctly

### Phase 3 - Build one modular hero wrapper

Create one modular hero path using:

- `ProjectCharacter` as the gameplay wrapper
- `ProjectSkeletalAssembly` as the runtime core
- one `USkeletalAssemblyDefinition` for the hero

Keep `BP_Hero` as the legacy reference path.

Test gate (Phase 3):

- modular hero spawns in PIE with correct component graph
- `FCapabilityRegistry::DumpToLog()` shows skeletal capabilities alongside object capabilities
- component IDs from definition match runtime component tags
- assembly state machine reaches ready state after spawn
- legacy `BP_Hero` still works unchanged

### Phase 4 - Wire switching in `ProjectSinglePlay`

Add respawn-based switching:

- default remains legacy
- `project.character.switch modular` uses modular wrapper
- `project.character.switch legacy` returns to legacy wrapper

Test gate (Phase 4):

- PIE starts with legacy hero (default unchanged)
- `project.character.switch modular` respawns into modular pawn
- `project.character.switch legacy` returns to legacy pawn
- switch preserves gameplay session (no map travel)
- switch logs selected system and resulting pawn/definition
- rapid switch cycling (legacy -> modular -> legacy -> modular) does not leak actors or components

### Phase 5 - Add debug capture

Add overlay plus structured capture.

Use it as the official parity harness between:

- legacy hero
- modular hero

Test gate (Phase 5):

- `project.character.debug 1` shows overlay with system ID, pawn class, capabilities, camera state
- `project.character.debug 0` hides overlay
- `project.character.capture test_label` writes screenshot + JSON sidecar to `Saved/Validation/CharacterDebug/`
- sidecar JSON contains all fields from section 10.5
- capture works for both legacy and modular systems
- two captures (legacy + modular) produce diff-comparable structured output

### Phase 6 - Migrate one concern at a time

Only after parity instrumentation exists, migrate concerns individually:

1. camera/body orchestration
2. Mutable orchestration
3. local first-person body handling
4. later, broader movement/traversal pieces only if still justified

Do not migrate all current Blueprint behavior in one pass.

Test gate (Phase 6, per concern):

- capture comparison between legacy and modular shows parity for the migrated concern
- legacy path remains functional after each migration
- no regressions in unmigrated concerns

### Post-implementation: update all affected documentation

After each phase lands, scan and update ALL documentation that references the affected architecture. This is mandatory, not optional -- stale docs cause wrong decisions in future sessions.

Architecture docs to scan:

- `docs/architecture/plugin_architecture.md` -- add ProjectSkeletalAssembly to plugin map
- `docs/architecture/plugin_rules.md` -- verify tier placement is documented
- `docs/architecture/core_principles.md` -- verify capability model description matches one-registry reality
- `docs/architecture/data_driven.md` -- update with extended ObjectDefinition fields (kind, role, visibility)
- `docs/architecture/conventions.md` -- add skeletal capability naming conventions
- `docs/data/README.md` -- update generation pipeline with new section types
- `docs/systems/loading_pipeline.md` -- if assembly affects loading phases

Plugin docs to scan:

- `Plugins/Foundation/ProjectCore/README.md` -- add FRegisteredClassScan kernel reference
- `Plugins/Gameplay/ProjectObjectCapabilities/README.md` -- update registry description (now serves all capabilities, add RegisterCapabilityModule)
- `Plugins/Resources/ProjectObject/README.md` -- update ObjectDefinition with new optional fields, new section types
- `Plugins/Resources/ProjectObject/docs/layer_contract.md` -- update capability/section contract with skeletal additions
- `Plugins/Systems/ProjectSkeletalAssembly/README.md` -- create (new plugin)
- `Plugins/Gameplay/ProjectCharacter/README.md` -- update legacy vs modular status
- `Plugins/Gameplay/ProjectCharacter/docs/design.md` -- update character architecture to reference assembly framework

CLAUDE.md routing to update:

- `CLAUDE.md` Quick Routes -- add skeletal assembly routing
- `CLAUDE.md` Cross-Plugin Boundaries -- add assembly framework boundaries

Rule: if a doc references capabilities, object definitions, character creation, or plugin architecture, it must be checked after each phase.

---

## 12. Acceptance Criteria

### 12.1. Static architecture acceptance

- `ProjectSkeletalAssembly` lives in `Plugins/Systems/`
- it does not depend on third-party systems (PoseSearch, Mutable, etc.)
- no separate `USkeletalAssemblyDefinition` -- skeletal actors use extended `UObjectDefinition`
- `FRegisteredClassScan` kernel lives in `ProjectCore/Public/Registry/`
- `FCapabilityRegistry` is the single registry for all capabilities (object + skeletal)
- `FCapabilityRegistry` supports `RegisterCapabilityModule()` for external adapter plugins
- `SkeletalAssembly` is a capability in data, but `ObjectSpawnUtility` gives it coordinator semantics during spawn (processed first, routes other skeletal capabilities through assembly lifecycle)
- no separate skeletal registry exists

### 12.2. Legacy runtime acceptance

- `Medium` mode still spawns legacy hero by default
- no behavior change in legacy path before explicit system switch

### 12.3. Modular runtime acceptance

- `project.character.switch modular` respawns into the modular pawn cleanly
- `project.character.switch legacy` returns cleanly
- modular path reports the correct system and definition in debug overlay
- capture command writes screenshot plus sidecar output

### 12.4. Reuse acceptance

- a non-player NPC wrapper can use the same assembly core without local-player-only capabilities

### 12.5. Regression-comparison acceptance

Capture output must be sufficient to compare:

- camera parent
- mesh parents
- anim classes
- selected bone transforms
- active capability set

without requiring manual Blueprint graph inspection.

---

## 13. Explicit Non-Goals For V1

V1 does not do the following:

- rewrite Motion Matching graphs into C++
- rewrite Mutable content graphs into C++
- replace all `BP_Hero` gameplay tuning in one pass
- hot-swap a live pawn between systems without respawn
- redesign traversal, foley, or smart-object behavior unless needed for wrapper parity
- create a separate skeletal capability registry (one registry for all capabilities)
- force all skeletal actors through assembly orchestration (opt-in only)

---

## 14. Naming Standard

### 14.1. Naming direction

Naming conventions were informed by patterns observed in Lyra, MetaHuman, and the existing ALIS capability registry.

Chosen direction:

- PascalCase per segment is dominant for slot and capability names
- dot separator is dominant for hierarchical identity (FGameplayTag, Flecs paths, Lyra tags)
- flat single-word or compound-word IDs for leaf-level entries
- runtime resolves via integer or FName, string paths are for authoring and debug

This standard aligns with the existing project pattern where capability IDs are flat PascalCase FNames (`Pickup`, `Lockable`, `Hinged`) and definition IDs are `Type:Category.Name` FPrimaryAssetIds.

### 14.2. Component layout IDs

Flat PascalCase. No dots. These are leaf identifiers within a single definition scope.

| ID | Purpose |
|----|---------|
| `DriverBody` | Primary skeletal mesh that owns the skeleton |
| `WorldBody` | Third-person visible body mesh |
| `LocalBody` | First-person owner-only body mesh |
| `Head` | Head mesh component |
| `Camera` | Camera component |
| `BodyCustomization` | Mutable customization skeletal component (world) |
| `HeadCustomization` | Mutable customization skeletal component (head) |
| `LocalBodyCustomization` | Mutable customization skeletal component (local) |
| `LODSync` | LOD synchronization component |

### 14.3. Capability IDs

Flat PascalCase FName. Same registry as object capabilities (`FCapabilityRegistry`). Each ID is globally unique.

Capabilities are optional, project-specific behaviors. Camera attachment and mesh visibility are core assembly behavior, not capabilities (see section 4.1 and 5.3.5).

| ID | Purpose |
|----|---------|
| `MotionMatching` | Motion matching animation driver |
| `MutableCustomization` | Mutable body customization rebuild orchestration |
| `LocalFirstPerson` | Owner-only first-person body handling |
| `DebugCapture` | Runtime debug capture and overlay |

### 14.4. Section categories

Flat PascalCase single word. These are partition keys within a definition.

| ID | Purpose |
|----|---------|
| `Animation` | Locomotion profile, retarget ABP, montage slots |
| `Customization` | Mutable source, appearance config |
| `View` | Camera and first-person view config |
| `Debug` | Debug overlay and capture config |

Later additions: `Ragdoll`, `LOD`, `GameplayBridge`.

### 14.5. Definition IDs

`Category.Variant` using `FPrimaryAssetId` with type `ObjectDefinition` (same as all objects).

| Full ID | Meaning |
|---------|---------|
| `ObjectDefinition:Human.Hero` | Player hero character |
| `ObjectDefinition:Human.Trader` | NPC trader |
| `ObjectDefinition:Human.Bandit` | NPC bandit |
| `ObjectDefinition:Animal.Dog` | Animal (later) |
| `ObjectDefinition:Door_GrandPa` | Door (existing) |

Characters and objects share the same definition type. The presence of `SkeletalAssembly` capability differentiates complex skeletal actors from simple objects.

### 14.6. Naming rules

1. PascalCase everywhere -- matches existing capability IDs
2. No underscores in IDs -- consistent with existing registry
3. Dots only for definition IDs -- `Human.Hero` expresses category membership
4. Singular nouns -- `Head` not `Heads`
5. No prefixes on IDs -- no `SK_`, `SA_`, `Comp_` (those belong on asset filenames)
6. No legacy abbreviations -- `BodyCustomization` not `Body_CSK`

### 14.7. JSON authoring example (hero with assembly)

```json
{
  "$schema": "../../Schemas/object.schema.json",
  "id": "Human.Hero",
  "spawnClass": "/Game/Characters/BP_ModularHero.BP_ModularHero_C",
  "meshes": [
    { "id": "DriverBody",        "kind": "SkeletalMesh",             "asset": "/Game/Characters/Human/SKM_Hero_Body", "role": "DriverBody" },
    { "id": "WorldBody",         "kind": "SkeletalMesh",             "parent": "DriverBody", "role": "WorldBody", "animClass": "/Game/Animation/ABP_Hero_Retarget" },
    { "id": "LocalBody",         "kind": "SkeletalMesh",             "parent": "DriverBody", "role": "LocalBody", "visibility": "OwnerOnly" },
    { "id": "BodyCustomization", "kind": "CustomizableSkeletalMesh", "parent": "WorldBody",  "role": "BodyCustomization" },
    { "id": "HeadCustomization", "kind": "CustomizableSkeletalMesh", "parent": "WorldBody",  "role": "HeadCustomization" }
  ],
  "capabilities": [
    { "type": "SkeletalAssembly",     "scope": ["actor"] },
    { "type": "MotionMatching",       "scope": ["DriverBody"] },
    { "type": "MutableCustomization", "scope": ["BodyCustomization", "HeadCustomization"] },
    { "type": "LocalFirstPerson",     "scope": ["LocalBody"], "properties": { "HiddenBones": "head,neck_01" } },
    { "type": "DebugCapture",         "scope": ["actor"] }
  ],
  "sections": {
    "animation":     { "locomotionProfile": "Human.Default", "traversalProfile": "Human.Parkour" },
    "customization": { "mutableSource": "/Game/Mutable/CO_Hero_Body" },
    "view":          { "defaultMode": "FirstPerson", "cameraParent": "Root", "attachmentPolicy": "CapsuleFixed", "relativeOffset": "(X=23 Y=0 Z=62)" }
  }
}
```

### 14.8. JSON authoring example (simple NPC, no assembly)

```json
{
  "$schema": "../../Schemas/object.schema.json",
  "id": "Human.Trader",
  "spawnClass": "/Game/Characters/BP_NPC.BP_NPC_C",
  "meshes": [
    { "id": "body", "asset": "/Game/Characters/Human/SKM_NPC_Body", "animClass": "/Game/Animation/ABP_NPC_Retarget" }
  ],
  "capabilities": [
    { "type": "MutableCustomization", "scope": ["body"] },
    { "type": "Dialogue", "scope": ["actor"], "properties": { "DialogueTreeAsset": "/Game/Dialogue/DLG_Trader" } }
  ],
  "sections": {
    "customization": { "mutableSource": "/Game/Mutable/CO_NPC_Trader" }
  }
}
```

---

## 15. Final Defaults

Chosen defaults for this plan:

- framework scope: generic skeletal core first, character wrappers on top
- plugin name: `ProjectSkeletalAssembly`
- plugin placement: `Plugins/Systems/`
- definition class: extended `UObjectDefinition` (no separate skeletal definition class)
- definition generation: extend existing `object.schema.json` with optional kind/role/visibility fields
- JSON body: same `meshes` + `capabilities` + `sections` as all objects
- registry: one `FCapabilityRegistry` for all capabilities, migrated onto `FRegisteredClassScan` kernel
- `SkeletalAssembly` is a capability (flat peer coordinator, Lyra pattern)
- `RegisterCapabilityModule()` lets adapter plugins register without hardcoding
- assembly orchestration: opt-in via `SkeletalAssembly` capability (not every skeletal actor needs it)
- camera: in `view` section, not in `meshes`
- core owns: attachment policy, visibility policy, lifecycle state machine
- capabilities are: optional behaviors (MotionMatching, MutableCustomization, LocalFirstPerson, DebugCapture, plus existing object ones)
- switch strategy: respawn-based
- switch host: `ProjectSinglePlay`
- command namespace: `project.character.*`
- legacy posture: preserve Artur-era baseline until modular parity exists
- adapter residency: capability implementations live in reusable capability-owner plugins, not in character wrappers

---

## 16. References Used For This Plan

Current repo evidence this plan is based on:

- `Plugins/Gameplay/ProjectCharacter/README.md`
- `Plugins/Gameplay/ProjectCharacter/docs/design.md`
- `Plugins/Systems/ProjectMotionSystem/README.md`
- `Plugins/Resources/ProjectAnimation/README.md`
- `Plugins/Resources/ProjectObject/README.md`
- `Plugins/Gameplay/ProjectObjectCapabilities/README.md`
- `Plugins/Gameplay/ProjectSinglePlay/README.md`
- `docs/architecture/plugin_rules.md`
- `docs/architecture/conventions.md`
- `docs/architecture/principles.md`
- `todo/current/fix_fp_body_clipping_v2.md`

Blueprint and asset investigation facts baked into this plan:

- current `BP_Hero` owns substantial runtime behavior
- recovered `BP_Hero_Motion` proved an older camera attach path existed
- repeated PIE forensics on this PC showed stable runtime state, so the problem is not best treated as random init drift here
