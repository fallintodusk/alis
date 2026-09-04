# Generate material assets from owner JSON

**Status:** DONE - IMPLEMENTED AND ISOLATED RE-ACCREDITATION PASSED - 2026-08-28

The implementation and its post-baseline isolated re-accreditation are complete.
Post-baseline review found an orphan-parent acceptance hole, a hand-maintained compiler
fingerprint, insufficient SHA vectors, and a UBT schema-dependency invalidation gap.
Those defects are fixed. The accepted generated assets and both manifest records bind
compiler fingerprint `6ddb657c...`; the complete C++ and host-transaction gates,
production zero-write rerun, exact rollback, owner cleanup, WorldData non-propagation,
and Shipping IoStore census pass. The active stabilization queue retains the compact
receipt and routes the next owner flag.
**Change type:** Editor tooling and generated resource authority
**Owning black box:** Existing `ProjectMaterial` resource plugin
**Operator sequencing prerequisite before implementation of:** [Landscape](../../02_backlog/world/world_present_landscape.md),
[Water](../../02_backlog/world/world_present_water.md),
[Roads](../../02_backlog/world/world_present_roads.md),
[Vegetation](../../02_backlog/world/world_present_vegetation.md), and
[Buildings](../../02_backlog/world/world_present_buildings.md)

## Goal

Add one reusable, Editor-only material generation system that compiles constrained JSON
recipes into persistent Unreal parent materials and material instances. V1 generates
only universal resources owned by `ProjectMaterial`. Generation must be deterministic,
idempotent, transactional, and confined to that plugin mount.

The first material-dependent concern selected by the campaign becomes the first real
consumer after the material core is independently accepted. Landscape, Water, roads,
buildings, vegetation, ProjectObject, and other domains may consume the same contract,
but only after their own research selects a material change.

Implementation is operator-approved. The isolated core may mutate only ProjectMaterial
and its test mount; Kazan and every production consumer remain frozen until the core is
independently accepted.

## Implementation result - 2026-08-27

The isolated core is accepted. No Kazan or production material package was mutated by
the core gate.

- The empty Runtime module was removed; `ProjectMaterialEditor` is Editor-only and has
  no consumer `Build.cs` dependency.
- Closed recipe and manifest schemas, deterministic identities, final-path generation,
  manifest-last promotion, output integrity, dependency locality, and bounded orphan
  cleanup are implemented.
- The host wrapper shares ProjectWorld's global generated-content lock, refuses another
  same-project Editor, snapshots exact prior state, journals before mutation, launches
  one hidden launcher-engine commandlet, authenticates its receipt, restores after
  rejection/timeout/crash, retains only one production `-1`, and owner-cleans scratch.
- All 11 exact `Project.Material.Generation.*` tests passed in the launcher engine.
- All 4 wrapper integration cases passed: cross-owner lock contention, same-project
  Editor refusal, interrupted-journal recovery, and idempotent generation plus injected
  post-save rollback and clean replacement.
- `scripts/ue/standalone/build.ps1`, data/schema validation, no-`Alis*` governance, and
  ASCII checks passed. The final scratch census found `tmp/material` absent.
- Stable ProjectMaterial, Resources-tier, data, ProjectObject, ProjectWorld, and
  ProjectWorldData documentation now owns the durable architecture.

Architecture and release-market audits independently selected Landscape K1 as the first
consumer because replacing the green debug grid produces the largest immediate Kazan
presentation gain with minimal geography and runtime risk.

## Product and flow decision

Research across Kazan presentation concerns may continue without implementation. The
implementation gate is:

```text
review and accept this material-system packet
-> finish presentation concern research
-> select implementation priority
-> implement and accept the small material core
-> implement the selected material-dependent concern as the first real consumer
-> migrate other selected concerns one at a time
```

Concern research is not blocked. Landscape, Water, Roads, Vegetation, and Buildings
implementation is blocked until the core material compiler and its
rollback/idempotency evidence are accepted. The core is implemented once; each concern
then consumes universal ProjectMaterial resources instead of adding another generator.

This is the operator-approved campaign sequence, not a claim that every internal task
technically recompiles a material. A later concern finding that needs no persistent
material generation does not silently waive this foundation-first order; changing that
order requires an explicit operator decision.

## Stable routes

- [Data architecture](../../../docs/data/README.md)
- [Data structure](../../../docs/data/structure.md)
- [Plugin dependency rules](../../../docs/architecture/plugin_rules.md)
- [Existing ProjectMaterial owner](../../../Plugins/Resources/ProjectMaterial/README.md)
- [ProjectObject definitions](../../../Plugins/Resources/ProjectObject/README.md)
- [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture_overview.md)
- [ProjectWorldData owner](../../../Plugins/World/ProjectWorldData/README.md)
- [Definition generator boundary](../../../Plugins/Editor/ProjectDefinitionGenerator/README.md)
- [World generation pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md)
- [Landscape research](../../02_backlog/world/world_present_landscape.md)

## Verified current state

### Existing owner

- `Plugins/Resources/ProjectMaterial` already exists and is enabled in
  `Alis.uproject`.
- The plugin owns 78 tracked material, material-instance, function, layer, and effect
  assets.
- Its current `ProjectMaterial` Runtime module has only empty startup/shutdown methods.
- No source module currently declares a dependency on that Runtime module.
- The README calls the plugin content-only even though the descriptor contains the
  empty Runtime module. That documentation and descriptor mismatch must be resolved
  during implementation.
- The plugin has no JSON recipe schema, `Data/` recipes, generation service, tests, or
  BuildUnit metadata today.

These facts make the existing Resources plugin the correct reusable owner. A second
material plugin or a `ProjectWorld`-owned universal compiler would duplicate authority.

### Existing duplicate generation

- `ProjectWorldWaterRealization.cpp` directly creates and connects material expressions,
  sets Single Layer Water, compiles, and saves `M_ProjectWorldWater`.
- `ProjectWorldPresentationMaterialRealization.cpp` independently creates or updates the
  generated Kazan terrain material instance and its parameters.
- ProjectObject JSON already supports final material soft-path overrides on meshes.
- ProjectInteraction already consumes an asset owned by ProjectMaterial.

The repository therefore has multiple real material consumers and two World-local
generation implementations, but no reusable material generation contract.

### Existing data behavior

- Plugin-root `Data/` is source authority; generated Unreal assets are derived,
  persistent, and versioned under the owning plugin mount.
- Every JSON data file requires a relative `$schema` field. The repository validator
  already scans plugin `Data/**/*.json` files.
- ProjectDefinitionGenerator converts registered JSON definitions into reflected
  DataAssets. It assumes definition metadata fields and contains ProjectObject-specific
  behavior. It is not a material graph compiler.
- ProjectObject's generator scans generic JSON beneath its current content source.
  Mixing material recipes into that tree would create discovery collisions and couple
  runtime object definitions to Editor-only compiler metadata.

### UE 5.8 native surface

The installed launcher engine exposes Editor material APIs for creating/deleting
expressions, connecting properties, setting material-instance parent and typed
parameters, recompiling, laying out graphs, and returning compile errors. Native runtime
mechanisms also include material instances, material parameter collections, Custom
Primitive Data, and Per Instance Custom Data.

Official references:

- [Material instances](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine)
- [Landscape materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine)
- [Material properties](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-material-properties)
- [Single Layer Water](https://dev.epicgames.com/documentation/en-us/unreal-engine/single-layer-water-shading-model-in-unreal-engine)
- [Custom Primitive Data](https://dev.epicgames.com/documentation/en-us/unreal-engine/storing-custom-data-in-unreal-engine-materials-per-primitive)
- [Instanced Static Mesh custom data](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine)

## Architecture decision

### Ownership

```text
ProjectMaterial
  owns universal material recipes and generated resources, closed archetype compiler,
  validation, transactions, manifests, and explicit Editor/commandlet entry points

ProjectWorld
  owns semantic-to-material binding and assignment for reusable World concepts

ProjectWorldData
  owns geography and sourced semantic facts; it owns no material recipe or asset

ProjectObject
  composes concrete entity meshes/textures/materials; an object-specific recipe/output
  is a future owner-local extension compiled by ProjectMaterialEditor

Unreal Engine
  owns material expressions, shader compilation, materials, and instances
```

`ProjectMaterial` is a reusable Resources owner, not Foundation `ProjectCore`.
Material/Editor dependencies do not belong in Foundation.

The ownership invariant is:

- `ProjectMaterial` owns every universal/reusable material family, shared recipe,
  archetype builder, schema/compiler mechanism, transaction, and manifest contract.
- `ProjectMaterialEditor` is the sole compiler mechanism, but it does not own every
  concrete recipe or generated material that it compiles.
- Universal concepts such as terrain, water, asphalt, foliage, wood, metal, and glass
  remain ProjectMaterial resources even when Kazan is their first consumer.
- `ProjectWorld` maps semantic facts such as terrain, water, or road class to stable
  ProjectMaterial resource identities and owns assignment. It does not compile graphs.
- `ProjectWorldData` may provide geographic/semantic facts, including a future sourced
  value such as turbidity, but never graph nodes, material paths, shader parameters, or
  a Kazan-only MIC for a universal concept.
- `ProjectObject` owns the concrete composition of an object entity: its meshes,
  per-slot material selections/overrides, object-specific textures, and any owner-local
  material recipe/output that cannot use a universal resource unchanged.
- ProjectObject never implements graph builders or a second compiler; a future approved
  object-side recipe is an input to `ProjectMaterialEditor`. World has no material recipe.
- `ProjectMaterial` must not learn about concrete ObjectIds, Kazan layers, or other
  consumer identities. V1 discovery stays inside ProjectMaterial; a future concrete
  extension remains dependency-inverted rather than teaching ProjectMaterial ObjectIds.

The specialization rule is identity-based, not first-use-based. A universal concept
stays in ProjectMaterial even if only one map currently consumes it. Only a visual asset
that is intrinsically part of one concrete entity identity may stay with ProjectObject.
If that resource later becomes reusable, it is generalized into ProjectMaterial (or the
matching universal ProjectMesh/ProjectTexture owner), references are migrated, and the
obsolete concrete duplicate is removed through its owning transaction.

### Module shape

Add an Editor-only `ProjectMaterialEditor` module inside the existing plugin. Before
removing the current empty Runtime module, repeat the repository-wide reference check.
If it is still unused, remove it in the same clean migration rather than preserving an
empty compatibility module.

### Resource dependency direction and packaging

`ProjectMaterial` is a lower-level Resources content leaf. It may know Engine material
APIs, but it must not know World, Kazan, ObjectIds, or any consumer. A runtime consumer
loads a stable generated material identity through a soft object path; it never calls a
ProjectMaterial runtime service or compiler API.

The isolated material-core implementation keeps `ProjectWorld` completely untouched.
When the first separately approved World consumer lands, that change must add the normal
content-plugin dependency from `ProjectWorld.uplugin` to `ProjectMaterial` if the packaged
activation/cook proof requires it. It must add zero `Build.cs` dependencies on either
`ProjectMaterial` or `ProjectMaterialEditor`. The Editor compiler remains private to the
resource plugin, and the currently empty Runtime module is removed if the final reference
audit remains empty. `Alis.uproject` is already configured and must not change.

The implementation must update `docs/architecture/plugin_rules.md` and the architectural
C4 model, including `docs/architecture/c4/model_plugins.dsl`, so Resources is an explicit
lower-level reusable tier for leaf owners such as ProjectMaterial, ProjectMesh, and
ProjectTexture. The model must show content/soft-reference consumption without transferring
ownership upward or creating a runtime service dependency. A packaged
Development and Shipping preflight must resolve the selected soft identity and prove the
ProjectMaterial content is staged; a missing plugin or asset fails closed before World
mutation.

Do not:

- add a second plugin;
- add another `Alis.uproject` entry;
- compile graph recipes in runtime, PIE, game boot, cook runtime, or packaged gameplay;
- stage recipe JSON into a packaged build;
- make ProjectWorld or ProjectObject the generic compiler;
- add an automatic file watcher in v1.

V1 exposes an explicit `Validate` and `Regenerate Changed` shared service through a
commandlet/test entry point. It has no Editor UI action. Explicit execution prevents
unexpected shader work on ordinary file saves; UI is added only if real iteration proves
that a human-facing action is needed.

Production mutation has one additional lifecycle boundary:

- read-only `Validate` may run in an appropriate Editor or commandlet process;
- production `Regenerate Changed` runs only in a dedicated one-shot
  `UnrealEditor-Cmd` Editor-commandlet process;
- the shared mutating C++ service refuses production owner mounts unless
  `IsRunningCommandlet()` is true and the request is explicitly owned by the dedicated
  material commandlet entry point;
- isolated automation may use the mutating service only against its registered test mount;
- no normal long-lived Editor caller, PIE caller, or packaged runtime caller can mutate;
- process exit discards every loaded UObject/package state on success or failure.

The commandlet must not run beside another Editor process for this project. Its host wrapper
holds the existing project-global generated-content mutation lock for the whole transaction
and refuses when any other `UnrealEditor` or `UnrealEditor-Cmd` process has this
`Alis.uproject` open. It may gracefully stop and later restore only the known
project-managed persistent test Editor; it must never kill an arbitrary interactive Editor.

`ProjectMaterialEditor` privately depends on Epic's `MaterialEditor`. It adds a private
dependency on Epic's `Landscape` module only if the implemented graph actually constructs
`UMaterialExpressionLandscapeLayerCoords` or another Landscape-owned class. General Engine
world-position/normal expressions require no speculative Landscape dependency. Neither
choice creates an ALIS `ProjectWorld` module dependency.

### Universal recipes and outputs

V1 discovers recipes only inside ProjectMaterial and writes only inside its mount:

```text
Plugins/Resources/ProjectMaterial/Data/Materials/<family>/<asset>.material.json
-> /ProjectMaterial/Generated/<family>/<asset>.<asset>
```

Expected universal examples are:

```text
Terrain/M_ProjectTerrain + Terrain/MI_ProjectTerrain_Default
Water/M_ProjectWater + Water/MI_ProjectWater_Default
Road/M_ProjectRoad + bounded reusable road instances
Foliage/<family> + bounded reusable foliage instances
```

Only the archetype selected with the campaign's first material-dependent consumer is
admitted in the first implementation. These examples define ownership, not mandatory V1
scope. The compiler validates that the stable material ID matches the derived leaf object
name. A recipe cannot choose an arbitrary package path, escape ProjectMaterial, or collide
with another output.

`ProjectWorldData/Data/Materials/**` is explicitly forbidden. Kazan profiles may retain
geography and sourced semantic facts, but not material paths, graph parameters, recipes,
instances, or generated material assets. ProjectWorld owns the closed semantic binding
from reusable World concepts to final ProjectMaterial resource identities.

### Future concrete ProjectObject extension

ProjectObject JSON already composes meshes with final per-slot material soft paths. If a
real object later needs a visual resource intrinsic to that ObjectId, a separately reviewed
extension may admit:

```text
Plugins/Resources/ProjectObject/Data/Materials/<ObjectId>/**
-> ProjectMaterialEditor compiler
-> /ProjectObject/Generated/Materials/<ObjectId>/**
-> final soft path in the ObjectDefinition composition
```

That extension is not part of V1 discovery, schema, or tests. Its sidecar stays outside
the runtime ObjectDefinition scan, binds unambiguously to the ObjectId, and cannot add a
ProjectObject compiler. Concrete texture bytes may be ProjectObject-owned only when they
are part of that entity identity; reusable textures remain in ProjectTexture.

### Closed recipe contract

Use `family` for broad rendering behavior and `archetype` for a supported graph shape.
Do not overload Unreal's `MaterialDomain` term.

V1 knows exactly one family/archetype selected after the seven-concern campaign chooses
its first material-dependent implementation. The accepted visual audit selected
Landscape, so V1 admits only `surface_opaque/landscape_basic_v1`. This selection does not
authorize Kazan Landscape assignment during the isolated core. Unselected water,
foliage, decal, road, and building families remain absent from the executable schema,
registry, and tests.

The schema is a closed union with:

- schema version and stable material ID;
- `artifact_kind` of `parent` or `instance`;
- the one implemented family and archetype ID with compiler version;
- explicit parent soft path for an instance;
- typed scalar and vector parameter sets;
- only archetype-specific validated fields.

A texture parameter is added only if the selected concern's bounded candidate requires it.
A static switch is added only if a reviewed real consumer requires it. Neither is V1
speculative schema surface.

Do not expose arbitrary Unreal class names, expression types, graph nodes, pins, HLSL,
package paths, or unrestricted properties in JSON. Builders in C++ own graph topology.
This keeps recipes reviewable and prevents the data layer from becoming an unsafe visual
scripting language.

Use a few shared parent archetypes and bounded material instances. Do not create one
parent per mesh. Per-object runtime variation belongs later in consumer-owned material
instances, Custom Primitive Data, Per Instance Custom Data, or parameter collections.

Material Layers are deferred until a real composition requirement proves they reduce
complexity. They are not required for v1.

## Determinism and authority contract

`.uasset` byte identity cannot decide whether generation is a no-op. Unreal may change
package bytes during an unnecessary save.

Before any write, compute a normalized semantic identity from:

1. Parsed recipe semantics with insignificant JSON formatting/key order removed.
2. Family, archetype, schema, and compiler versions.
3. Referenced parent, texture, and function object paths plus package digests.
4. Target owner and output object identity.
5. Exact Unreal Engine version and changelist.

V1 keeps the material manifest under ProjectMaterial's
`Data/Manifests/Materials/` hierarchy. Each accepted record contains:

- recipe path and normalized recipe hash;
- family, archetype, and compiler version;
- dependency paths and package digests;
- output soft object path;
- semantic identity;
- generated package SHA-256;
- compiler fingerprint;
- exact engine version and changelist.

Required behavior:

- Skip performs zero package writes and zero shader compiles only when recipe semantics,
  dependency digests, compiler/archetype identity, and exact engine version/changelist
  match the accepted manifest, every output exists, and every output package SHA-256
  equals its accepted manifest SHA-256.
- Missing, moved, or corrupted output fails the skip check and enters regeneration even
  when the recipe is unchanged.
- Formatting-only JSON changes are no-ops.
- A changed dependency invalidates only recipes that bind that dependency digest.
- Parent recipes compile before dependent material-instance recipes.
- A compile error prevents save and manifest promotion.
- Save/reload validation authenticates graph shape, parent, typed parameters, and output.
- Orphans are reported by default and deleted only through an explicit unreferenced-safe
  cleanup action.
- Material generation never automatically regenerates World geography or object
  definitions.

The material manifest is independent resource authority. A later World presentation
manifest binds the accepted material object path and digest as an input. That makes
material acceptance independent from geography realization while preserving locality.

## Transaction, rollback, and temporary-file contract

Every generation run is transactional:

```text
host wrapper
-> acquire project-global generated-content mutation lock
-> refuse any other Editor process for this project
-> validate paths and write an interrupted-transaction journal
-> snapshot exact current final packages + manifest as -1
   (record exact absence for a new output)
-> launch dedicated UnrealEditor-Cmd material commandlet

commandlet child
-> validate all recipes, dependencies, and final identities
-> create or update at the FINAL Unreal package/object identity
-> wait for the narrow required material compile result
-> reject compile errors without saving changed outputs
-> save and reload final outputs
-> verify semantic structure and package digests
-> promote the manifest LAST
-> write an authenticated result receipt and exit

host wrapper after child exit
-> success: verify receipt/output/manifest and clean transaction files
-> failure, crash, or timeout: terminate child, then restore exact -1 bytes/absence
-> remove journal only after accepted success or authenticated rollback
-> release mutation lock
```

V1 must not compile under a temporary package identity and then move or rename `.uasset`
bytes into the final package. Unreal references bind package/object identity; binary
promotion is rejected unless a future Epic-supported technique is independently proved
against the installed engine and reviewed as a contract change. If execution stops after
asset save but before manifest promotion, the next run cannot skip because package and
accepted-manifest state do not match.

The host, not the mutating commandlet, owns disk rollback. A commandlet cannot guarantee
its own recovery after a crash or forced termination. Restoring only after the child has
exited also prevents restored disk bytes from disagreeing with live objects in that child.
If the host itself is interrupted, the journal and `-1` snapshot remain; the next mutation
refuses until the recovery path restores and authenticates the prior state.

Implementation must investigate and use the narrowest verified UE material compile/wait
surface. It must not copy the current broad global compilation wait without proving that
it is necessary.

The owning script keeps one exact accepted `-1` rollback snapshot and the accepted
receipt. All other run-owned temporary files are deleted on success, handled failure,
and retry. Scratch lives only under:

```text
tmp/material/generation/<run-id>/receipts/**
tmp/material/generation/<run-id>/rollback/**
tmp/material/generation/<run-id>/diagnostics/**
```

Tests may register a temporary material package mount rooted under project `tmp/`; they
compile directly at the final identity inside that test-only mount and must not binary-move
candidate assets. They must not write fixtures into `Saved/`, system temp, or production
content. A failed run restores the exact `-1` asset/manifest state and leaves no accepted
partial output. Any in-process test that restores package bytes and continues must call the
verified UE package reload surface, reacquire every package/material pointer, and prove
readback from the reloaded objects. It must never reuse pre-restore UObject pointers.

## Reviewer feedback audit

### Accepted

- JSON is source authority and generation is Editor-only.
- Use a small number of closed material families/archetypes and many bounded instances.
- Runtime changes use standard UE dynamic parameter mechanisms, not graph generation.
- Single Layer Water requires its native output node, not only a shading-model enum.
- Every selected concern should consume the common system rather than add a local compiler.

### Corrected

1. A universal `ProjectWorld` material compiler is the wrong owner. The existing
   `ProjectMaterial` Resources plugin and non-World consumers prove cross-domain scope.
2. Material recipes should be near ProjectObject conceptually, but not embedded in its
   runtime object JSON or scanned content tree. Owner-local `Data/Materials` sidecars
   preserve intuition without confusing ProjectDefinitionGenerator.
3. ProjectDefinitionGenerator is not the v1 backend. It creates reflected DataAssets and
   contains definition-specific behavior; material graph compilation needs its own
   Editor service. Reuse its data-authority pattern, not its responsibility.
4. Material Layers and a generic arbitrary graph DSL are not v1 requirements. One
   campaign-selected closed parent archetype plus instances is the KISS proof.

### R1 corrections accepted before the universal-resource decision

1. Landscape must migrate both parent and persistent MIC generation to
   `ProjectMaterialEditor`; ProjectWorld becomes a consumer/authenticator and removes its
   parallel terrain MIC generator.
2. The former same-path WorldData terrain MIC proposal was safe for bytes but assigned
   universal presentation ownership to a concrete geography plugin. R2 supersedes it.
3. No-op requires accepted output existence and package integrity plus exact
   recipe/dependency/compiler/engine identity.
4. Production generation mutates the final Unreal package identity under an exact `-1`
   snapshot and promotes the manifest last; temporary-package binary promotion is not a
   V1 mechanism.
5. The former Landscape-specific V1 archetype is superseded. V1 still admits exactly one
   closed archetype, but the campaign-selected first material consumer determines it.

### Accepted operator sequencing decision

The suggestion to make the core optional for the five named Kazan concerns is not adopted.
It is reasonable as a generic technical dependency rule, but it conflicts with the
operator-approved campaign sequence: establish and accept one material authority before
Landscape, Water, Roads, Vegetation, or Buildings implementation. The gate prevents those
concerns from creating another local material path while research is still converging. It
does not claim every internal concern task invokes the compiler, and Environment/demo
capture remain explicitly independent.

The final R1 reviewer accepts this distinction and the five-concern sequencing gate.

### Final R1 safety correction accepted and strengthened

1. Production mutation is one-shot Editor-commandlet-process-owned; read-only validation
   remains callable without mutation.
2. Process exit is the V1 stale-UObject boundary. An in-process test that restores and
   continues explicitly reloads/reacquires packages and pointers.
3. The reviewer correctly identified live-object state, but restoring inside the child is
   insufficient for child crash/timeout. The host wrapper owns the snapshot, journal,
   child lifetime, and exact rollback after child exit.
4. The commandlet alone does not exclude a second long-lived Editor. The host therefore
   holds the project-global generated-content mutation lock and refuses while another
   Editor has this project open.
5. Epic `Landscape` is an Editor-only private dependency only when the selected graph uses
   a Landscape-owned expression class. `ProjectWorld` never becomes a dependency.

### R2 universal-resource correction requiring review

1. `ProjectMaterial` owns universal recipes and generated resources, not only compiler
   machinery. Terrain and water remain universal even when Kazan is their first user.
2. ProjectWorldData owns geography and sourced semantic facts only. It must contain zero
   Landscape/water material recipes, paths, graph parameters, MICs, or material manifests
   after each selected migration.
3. ProjectWorld owns the closed semantic-to-resource binding and assignment contract. It
   consumes accepted ProjectMaterial assets without depending on ProjectMaterialEditor.
4. ProjectObject is the bounded concrete-identity extension. ObjectId-owned sidecars and
   outputs may be added only by a later reviewed extension; V1 does not scan them.
5. Moving current WorldData materials to ProjectMaterial changes stored references. Each
   concern must use a one-time reference migration that proves geometry/collision semantics
   unchanged; it must not claim every consuming package remains byte-identical.

## Alternatives rejected

| Alternative | Decision | Reason |
|---|---|---|
| New universal material plugin | Reject | `ProjectMaterial` already owns reusable material resources. |
| Compiler inside ProjectWorld | Reject | Violates reuse and creates the wrong dependency direction. |
| Material fields inside every object JSON | Reject | Pollutes runtime definitions and couples unrelated generators. |
| WorldData-owned terrain/water recipes | Reject | Geography instances do not own universal rendering behavior. |
| Scan every enabled plugin in V1 | Reject | Adds unneeded ownership and collision surface before a concrete extension exists. |
| Extend generic definition reflection into graph compilation | Reject | Different lifecycle, validation, output, and failure semantics. |
| Runtime graph generation | Reject | Shader compilation and package writes are Editor authority. |
| Automatic watcher regeneration | Defer | Hidden expensive work is unsafe for the first version. |
| Editor UI action in v1 | Defer | Service plus commandlet/test proves the lifecycle with less surface. |
| Long-lived Editor production mutation | Reject | Failure/rollback can leave loaded UObject state stale. |
| Commandlet-owned rollback only | Reject | The child cannot recover its own crash or forced termination. |
| Arbitrary node-graph JSON | Reject | Large unsafe DSL, unstable UE API coupling, unnecessary scope. |
| Material Layers in v1 | Defer | No proven composition need yet. |
| Temporary-package binary promotion | Reject | It does not preserve proven Unreal package/object identity. |
| Keep concern-local material generators indefinitely | Reject | Already duplicated across Landscape and Water. |

## Expected implementation boundary

Expected changed components after approval:

- `Plugins/Resources/ProjectMaterial` descriptor, README, BuildUnit metadata, Editor
  module, schemas, shared recipe(s), and focused tests;
- one material-generation host wrapper that reuses or cleanly lifts the existing
  project-global generated-content mutation lock contract without creating a second lock;
- generic data validation only if the existing `$schema` route proves insufficient;
- developer generated-asset authority/public mirror metadata if the new persistent
  generated collection requires it;
- stable data/material documentation and architectural/C4 indexes;
- later, in a separate accepted consumer change, universal ProjectMaterial recipes and
  generated assets, plus consumer authentication of the final object path/digest and
  removal of any parallel compiler. The campaign router selects that consumer.

Expected untouched during core implementation:

- Kazan map, Landscape, relief, collision, roads, buildings, water, and vegetation;
- ProjectObject runtime schema and generated object definitions;
- ProjectWorld runtime streaming and geography generation;
- the `ProjectWorld` plugin descriptor during the isolated material-core implementation;
- current material assets unless a reviewed migration explicitly names them;
- arbitrary interactive Editor process lifecycle or unsaved Editor state;
- `Alis.uproject`;
- packaged-game runtime behavior.

Unexpected propagation across these boundaries is a stop condition.

## Ordered implementation tasks after approval

1. Add characterization tests proving current owner/module state and the absence of a
   common material recipe compiler.
2. Recheck module references; replace the unused Runtime stub with the Editor module if
   the reference result is still empty.
3. Add the closed recipe and manifest schemas with relative `$schema` validation.
4. Implement ProjectMaterial-only recipe discovery, final-path mapping, mount confinement,
   stable identity, and duplicate-ID/collision rejection. Do not scan WorldData or
   ProjectObject in V1.
5. Implement normalized recipe/dependency/compiler/engine identity, accepted output
   existence/integrity checks, and no-write/no-compile skip logic.
6. Implement the one closed parent archetype recorded after campaign selection using native
   UE Material Editor APIs. Add a native module dependency only when that graph uses its
   class; never depend on ALIS `ProjectWorld`.
7. Implement scalar/vector material-instance generation and parent-first dependency
   ordering. Do not add texture/static-switch schema until a reviewed consumer requires it.
8. Add compile-error rejection, final-identity save/reload verification, manifest-last
   promotion, orphan reporting, and owner cleanup. Do not binary-move a temporary-package
   asset into production identity.
9. Add the dedicated mutating Editor commandlet plus shared service. Production mounts
   require commandlet process ownership; tests are restricted to their temporary mount;
   do not add UI, a watcher, or long-lived Editor mutation.
10. Add the host wrapper with the project-global generated-content mutation lock,
    same-project Editor-process exclusion, transaction journal, exact `-1` snapshot,
    child timeout/crash handling, post-exit rollback, and authenticated cleanup.
11. Add BuildUnit/governance/asset-authority integration. Update the stable plugin rules
    and C4 model with the Resources leaf tier and its soft/content dependency direction.
12. Prove the core with synthetic recipes under a temporary registered mount; do not
    mutate Kazan.
13. Request implementation/evidence review. Only after acceptance may the campaign's
    selected material-dependent concern generate its assets through this owner, remove
    any local compiler, and become the first real consumer.

## Regression-first verification plan

Add exact focused automation tests for:

| Proposed exact test | Proof |
|---|---|
| `Project.Material.Generation.RecipeContract` | Unknown fields, family, archetype, or parameter fail closed. |
| `Project.Material.Generation.OwnerConfinement` | Path escape and cross-plugin output are rejected. |
| `Project.Material.Generation.SelectedArchetypeGraph` | The one admitted parent graph compiles, reloads, and has the expected semantic structure. |
| `Project.Material.Generation.InstanceParameters` | Parent and typed instance parameters survive save/reload. |
| `Project.Material.Generation.IdempotentNoWrite` | Second run has zero writes/compiles and stable package hash/mtime. |
| `Project.Material.Generation.OutputIntegrity` | Missing/corrupt output cannot pass the semantic skip gate. |
| `Project.Material.Generation.EngineIdentity` | Engine version/changelist change invalidates accepted identity. |
| `Project.Material.Generation.DependencyLocality` | Only exact dependency consumers invalidate. |
| `Project.Material.Generation.ProcessOwnership` | Production mutation is rejected outside the dedicated one-shot material commandlet. |
| `Project.Material.Generation.ReloadAfterRollback` | In-process analogue releases loaded packages, restores exact bytes, and reacquires fresh objects before readback. |
| `Project.Material.Generation.TransactionRollback` | Host restores exact `-1` assets/absence and manifest after child exit. |
| `Project.Material.Generation.OrphanSafety` | Default reports; explicit cleanup removes only unreferenced output. |

Additional required cases:

- runtime or PIE invocation is refused;
- graph compile errors cannot save changed outputs or promote manifests;
- formatting-only recipe changes are semantic no-ops;
- duplicate IDs and output collisions fail before mutation;
- derived output remains confined to the ProjectMaterial generated mount;
- an interrupted asset-save/manifest-not-promoted state cannot pass the next skip check;
- a held content-mutation lock or another Editor for this project rejects production
  mutation before snapshot/write;
- child compile failure, crash, and timeout all restore through the host after child exit;
- an interrupted host journal blocks further mutation until authenticated recovery;
- temporary diagnostics, rollback copies beyond the retained `-1`, and obsolete run files
  are owner-cleaned;
- no production Kazan package changes during core tests.

Run each new exact test through:

```powershell
scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-full-test-name>
```

The commandlet lifecycle also requires one focused wrapper integration proof using a
synthetic owner/test mount. It launches a real one-shot `UnrealEditor-Cmd`, authenticates
success, injects one post-save/pre-manifest failure, and proves post-exit rollback. It must
also prove lock contention and same-project Editor exclusion without terminating an
arbitrary interactive Editor.

Then run the bounded acceptance checks appropriate to the changed module:

```powershell
scripts/ue/standalone/build.ps1
python scripts/ue/check/data/validate_all.py
scripts/ue/check/governance/validate_no_alis_prefix.bat
```

Use project build wrappers only. Do not invoke UE `Build.bat` with custom arguments and
do not switch to a source-built engine. Broad automation is reserved for the accepted
end-of-slice gate.

## Documentation and migration plan

Implementation must move durable decisions into stable owners:

- ProjectMaterial README: universal/reusable resource ownership; sole compiler mechanism;
  ProjectMaterial-only V1 discovery; promotion rules; module and runtime boundaries;
- ProjectObject README and layer-contract docs: concrete entity composition owns mesh,
  texture, and per-slot material selection/overrides plus ObjectId-bound sidecars when a
  universal resource cannot be used unchanged; that extension is not V1 and never owns
  compiler/archetype logic;
- `Plugins/World/ProjectWorld/docs/architecture_overview.md` and
  `territory_generation.md`: reusable semantic-to-ProjectMaterial binding,
  assignment/authentication, and prohibition on World-owned graph compilation;
- `Plugins/World/ProjectWorldData/README.md` and `Data/README.md`: geography and sourced
  semantic facts only, with no material path, graph parameter, recipe, generated material,
  or material manifest authority after each selected concern migration;
- material-generation operations: host/commandlet lifecycle, global mutation lock,
  interrupted recovery, and managed-persistent-Editor handling;
- ProjectMaterial schema documentation: closed families/archetypes and versioning;
- data docs: universal ProjectMaterial recipes and persistent generated outputs, plus the
  separately reviewed concrete-entity extension and explicit promotion/migration rule;
- architecture/C4 indexes: existing plugin and new Editor-module responsibility;
- `docs/architecture/plugin_rules.md` and `docs/architecture/c4/model_plugins.dsl`:
  Resources leaf ownership, allowed soft/content consumption, and the prohibition on
  consumer `Build.cs` or runtime-service coupling to ProjectMaterial;
- World pitfalls: retain semantic no-op and Single Layer Water requirements;
- developer asset mirror/authority: add only the reviewed generated collection.

Do not leave stable architecture dependent on this todo. Do not migrate authored,
imported, third-party, or existing generated materials merely because the compiler
exists. Each consumer migration must be selected and verified separately.

## Completion criteria

This todo is complete only when all of the following are true:

- the architecture packet is reviewed and implementation is explicitly approved;
- the existing ProjectMaterial plugin is the sole common compiler owner;
- focused tests fail first for the missing capability and pass after implementation;
- one synthetic parent and one instance generate, compile, save, reload, and validate;
- production mutation is accepted only through a dedicated one-shot Editor commandlet;
- the host holds the global mutation lock, excludes other same-project Editors, and owns
  rollback after child exit for handled failure, crash, and timeout;
- interrupted host recovery is journaled and must finish before another mutation;
- continuing in-process tests reload/reacquire restored packages and pointers;
- a second unchanged and integrity-matching run causes zero writes and zero shader compiles;
- missing/corrupt output and engine-version/changelist changes cannot pass the skip gate;
- dependency-local invalidation, rollback, orphan safety, and cleanup are proved;
- final package identity is used directly under the ProjectMaterial generated mount;
- no runtime recipe reader, Editor UI, watcher, new plugin, or `Alis.uproject` change exists;
- the unused Runtime stub is removed after a repeated zero-reference audit, with no
  ProjectWorld `Build.cs` dependency or ProjectMaterial runtime service introduced;
- the stable plugin rules and C4 model contain the Resources leaf tier before a consumer
  adopts it; the first World consumer owns any required `ProjectWorld.uplugin` content
  dependency and packaged soft-reference/cook proof;
- stable docs and generated-asset authority are updated;
- the core is independently accepted before any material-dependent concern migration;
- the campaign's final priority selection, rather than this packet, routes the first
  production consumer.

Passing research review does not move this file to `todo/01_done/`. It remains backlog
until the approved implementation and all completion evidence are finished.

## Implementation close-out

Implementation and isolated re-accreditation passed on 2026-08-28. The final compiler
fingerprint is source-derived, schema edits participate in UBT makefile invalidation,
the accepted manifest matches the executing compiler, production regeneration is
idempotent and rollback-safe, generated World authority stayed untouched, required
assets cook into Shipping, authoring/compiler payload stays out, and owner scratch is
bounded and cleaned. The BuildUnit descriptor correctly remains a dirty
last-successful-source-build baseline until a real BuildService source-release build;
launcher-only re-accreditation never rewrites it.

## Review record

**Current result:** IMPLEMENTATION ACCEPTED - 2026-08-27.

R1 accepted the following review surface:

1. existing ProjectMaterial ownership versus ProjectWorld ownership;
2. owner-local sidecar recipes versus embedding in ProjectObject definitions;
3. closed v1 archetype scope and absence of an arbitrary graph DSL;
4. semantic idempotency and dependency-local invalidation;
5. one-shot commandlet mutation, host-owned exact `-1` rollback, global lock/exclusivity,
   interrupted recovery, and temporary-file cleanup;
6. exact engine identity and accepted output-integrity skip conditions;
7. final package identity and the then-proposed stable Landscape MIC mapping;
8. material-core prerequisite and campaign-owned first-consumer selection.

R1 remains evidence for the compiler transaction and safety design, but its concrete
WorldData ownership and Landscape-first path decisions are superseded. R2 accepted the
universal ProjectMaterial ownership, ProjectWorld semantic binding, ProjectWorldData
geography-only boundary, and ProjectObject extension. It also requires the stable Resources
leaf dependency model and immutable World-authority transactions in each separately selected
reference migration. Those requirements are now explicit in this packet and its consumer
packets.
