# Skeletal Assembly Architecture

SOT for the modular skeletal assembly framework: lifecycle, registry, types, naming.

---

## Purpose

Move skeletal-actor orchestration, lifecycle, and assembly into C++ while keeping content assets data-driven. Characters are wrappers that consume a definition + capability specs. The framework supports player heroes, NPCs, and future non-character skeletal actors.

This is NOT a rewrite of Blueprint content into C++. It is a move of orchestration and lifecycle into C++.

---

## Plugin Placement

| Plugin | Tier | Owns |
|--------|------|------|
| `ProjectSkeletalAssembly` | Systems | Assembly lifecycle, state machine, debug capture |
| `ProjectSkeletalCapabilities` | Gameplay | Adapter capabilities (Mutable, MotionMatching, LocalFirstPerson) |
| `ProjectObjectCapabilities` | Gameplay | Generic capabilities (Lockable, Pickup, Hinged) + FCapabilityRegistry |
| `ProjectObject` | Resources | ObjectDefinition, meshes, spawn utility |
| `ProjectCharacter` | Gameplay | Legacy AProjectCharacter + modular ADefinitionCharacter |

ProjectSkeletalAssembly must NOT depend on: GAS, Vitals, Mutable, PoseSearch, specific skeletons or bone names.

---

## Ownership Model

**Assets choose configuration. C++ owns runtime policy.**

The framework owns:
- Component graph assembly from definition
- Attachment and visibility policy
- Lifecycle state machine (Idle -> Assembling -> Ready -> TearingDown)
- Capability registration and activation ordering
- Debug capture

Assets and Blueprints keep:
- Motion Matching AnimBP graphs
- Retarget AnimBPs
- Mutable content graphs (CO/COI authoring)
- Skeleton-specific asset choices

---

## One Registry Model

All capabilities -- object and skeletal -- use the same `FCapabilityRegistry`.

A door capability and a skeletal capability are the same concept. JSON declares what the thing has, the registry finds the class, the framework attaches it. The difference is runtime lifecycle: some actors need orchestration (hero with assembly phases), most don't (door, simple NPC).

There is NO separate skeletal registry.

**Registration:**
- Core always-enabled plugins listed in registry scan config by FName: `ProjectObjectCapabilities`, `ProjectMotionSystem`, `ProjectSkeletalAssembly`
- External plugins call `FCapabilityRegistry::RegisterCapabilityModule()` in StartupModule()

**Capability IDs** (FPrimaryAssetId("CapabilityComponent", "...")):

| ID | Plugin | Purpose |
|----|--------|---------|
| Lockable, Pickup, Hinged, Sliding, LootContainer, Audio, ActorWatcher | ProjectObjectCapabilities | Generic object capabilities |
| SkeletalAssembly | ProjectSkeletalAssembly | Assembly lifecycle coordinator |
| DebugCapture | ProjectSkeletalAssembly | Runtime debug capture |
| MutableCustomization | ProjectSkeletalCapabilities | Mutable body customization |
| MotionMatching | ProjectSkeletalCapabilities | Motion Matching driver (v1 stub) |
| LocalFirstPerson | ProjectSkeletalCapabilities | First-person body handling |

**Anti-patterns:**
- Separate registry per domain
- Hardcoding external module names in registry
- Making the registry a UObject (unnecessary GC)
- Forcing all skeletal actors through assembly orchestration

---

## Assembly Component

`USkeletalAssemblyComponent` is an opt-in lifecycle coordinator (flat peer on the actor, not a wrapper).

**State machine:** Idle -> Assembling -> Ready -> TearingDown

**When present (`SkeletalAssembly` capability in definition):**
1. ObjectSpawnUtility processes it first (regardless of JSON array order)
2. Other skeletal capabilities targeting assembly-managed meshes are deferred
3. Assembly activates deferred capabilities when state reaches Ready
4. Assembly deactivates them on TearingDown

**When absent:** all capabilities fire-and-forget at spawn (simple objects).

This matches the Lyra init state coordinator pattern: peer component manages sibling initialization ordering.

---

## Core Types

| Type | Location | Purpose |
|------|----------|---------|
| `USkeletalAssemblyComponent` | ProjectSkeletalAssembly | Lifecycle state machine, capability management |
| `IAssemblyCapability` | ProjectCore | Interface for assembly lifecycle (DIP boundary) |
| `IAssemblyViewConfigSource` | ProjectCore | Interface for view config data |
| `FAssemblyViewConfig` | ProjectCore | Camera offset data struct |
| `UCharacterDebugCaptureComponent` | ProjectSkeletalAssembly | Debug overlay + JSON capture |

---

## Acceptance Criteria

**Static architecture:**
- ProjectSkeletalAssembly in Plugins/Systems/, no third-party deps
- FCapabilityRegistry is the single registry for ALL capabilities
- SkeletalAssembly is a capability (flat peer coordinator pattern)
- No separate skeletal registry

**Runtime:**
- `project.character.switch modular` respawns into modular pawn
- `project.character.switch legacy` returns cleanly
- Capture command writes screenshot + JSON sidecar

**Reuse:**
- Non-player NPC can use the same assembly core without local-player capabilities

---

## Non-Goals (V1)

- Rewrite Motion Matching AnimBP graphs into C++
- Rewrite Mutable content graphs into C++
- Hot-swap live pawn between systems without respawn
- Redesign traversal, foley, or smart-object behavior
- Force all skeletal actors through assembly orchestration

---

## Naming Standard

### Component Layout IDs

Flat PascalCase: `DriverBody`, `WorldBody`, `LocalBody`, `Head`, `BodyCustomization`, `HeadCustomization`, `LocalBodyCustomization`

### Capability IDs

Flat PascalCase FName via FPrimaryAssetId("CapabilityComponent", "..."): `MotionMatching`, `MutableCustomization`, `LocalFirstPerson`, `DebugCapture`

### Section Categories

Flat PascalCase: `Animation`, `Customization`, `View`, `Debug`

### Definition IDs

FPrimaryAssetId with type ObjectDefinition: `ObjectDefinition:Hero`, `ObjectDefinition:Door_GrandPa`

### Rules

1. PascalCase everywhere
2. No underscores in IDs
3. Singular nouns
4. No prefixes on IDs (no `SK_`, `SA_`, `Comp_`)
5. No legacy abbreviations (`BodyCustomization` not `Body_CSK`)

---

## Cross-References

- [Capability rationale](../../Gameplay/ProjectSkeletalCapabilities/docs/rationale.md) - adapter dependency isolation
- [Layer contract](../../Resources/ProjectObject/docs/layer_contract.md) - Kind/Role/Visibility, spawn behavior
- [Character design](../../Gameplay/ProjectCharacter/docs/design.md) - legacy vs modular, switching
- [Parity testing](../../../../docs/testing/character_parity.md) - automated capture, debug commands
