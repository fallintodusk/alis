# Data-driven modular building reconstruction: architecture assessment and mock plan

Origin: `todo/02_backlog/world/20261112-1332_world_extend_building_assembly.md`.
This file is a disposable investigation artifact. No implementation, tests, Unreal
execution, or project changes were performed. All repository files other than this
one were treated as read-only. Nothing here is an accepted decision, a standing
contract, or authorization to build.

## How to read this document

Labels are used deliberately (authority vocabulary):

- `[FACT]` - verified from a file I read; the path is named.
- `[DECISION]` - an explicit operator choice recorded in a stable owner; cited,
  never restated as current behavior.
- `[FINDING]` - an evidence-backed issue with the current design or the premise.
- `[PROPOSAL]` - a recommendation, not accepted.
- `[Q<n>]` - unresolved question (operator gate). `[A<n>]` - working assumption.
- `[MOCK]` - illustrative artifact (schema shape, signature, step). Descriptive
  only; it has not been compiled, run, or validated against the installed engine.

"Verified" means read from the current checkout. "Proposal" means my design
judgment. Where runtime/streaming behavior is asserted as a design property and
was not measured, it is marked as such.

## Contents

- 0. Verdict
- 1. Task framing and authority
- 2. Verified current architecture
- 3. Critical assessment (findings)
- 4. Recommended architecture (proposals)
- 5. Mock implementation plan (phases P0-P4)
- 6. Invariant-to-proof traceability and verification matrix
- 7. Unresolved questions and assumptions
- 8. Investigation limits and evidence index

---

## 0. Verdict

The existing `ProjectBuildingAssembly` prototype is a well-encapsulated,
correctly-bounded *integration and lifecycle proof*: it demonstrates that a
selected building can be planned, realized as ISMs, enrolled in the World
buildings-layer transaction, saved/reloaded, reused as a no-op, replaced
transactionally, and retired with rollback. Its ownership split is clean. As a
base for "data-driven modular reconstruction of buildings, including interiors,
unique details and necessary Terrain adaptation", it is **not** a maintainable
foundation, and the starting TODO is not a plan for that goal.

Three verdicts, in order:

1. `[FINDING]` **Premise mismatch.** The starting TODO explicitly forbids what
   the operator goal requires: "This task does not authorize city publication,
   new Terrain authority, complex facade/interior systems or a general asset
   cache" (`todo/02_backlog/world/20261112-1332_world_extend_building_assembly.md`).
   Its five "Retained follow-ups" are prototype-hardening items (kit-reader probe,
   one unsupported-footprint regression, one bounded neighborhood, material-only
   adoption, reuse measurement). None delivers interiors, unique detail, or
   Terrain adaptation. Treating this TODO as the goal's implementation plan would
   silently narrow the goal. This is a `BLOCKER` on "extend the prototype
   incrementally to reach the goal", not on the goal itself.
2. `[FINDING]` **The prototype is not actually data-driven.** The composition
   algorithm is hard-coded C++ in `Build()` (one rectangular prism, four sides,
   uniform `WindowEvery`, one ground entrance, flat roof, floor slabs). The only
   data is a kit of scalar dimensions plus one selected feature's footprint and
   height. The accepted "JSON building blueprints" target
   (`Plugins/World/ProjectBuildingAssembly/docs/decision_record.md`) is *planned*,
   not implemented: `Data/Buildings/` does not exist (`[FACT]` `Data/` contains
   only `Kits/`, `Schemas/`, `README.md`). So the goal is largely a request to
   realize an already-accepted decision, not to invent a new system.
3. `[PROPOSAL]` **Recommended direction.** Keep the accepted authority split and
   transaction, but split the conflated responsibilities: introduce a data-owned
   building *definition/blueprint* (composition rules as versioned data), replace
   the fixed 4-part enum with an open module-id set, turn the portable layout core
   into a small deterministic *interpreter* of that data, and generalize native
   realization to N modules keyed by stable id. Add interiors as a distinct
   admitted-use class, route unique details through blueprint overrides plus the
   existing protected authored overlay, and keep Terrain adaptation strictly on
   the canonical single-writer correction path, gated behind a demonstrated RED.
   Stage this as P0-P4 with per-phase proof; do not grow `Build()` into a general
   grammar engine.

What this plan deliberately does **not** do: add a second Terrain writer, a
general PCG/grammar framework, a general asset cache/scheduler, a parallel
building-definition authority, or a speculative streaming rewrite. Each is
justified in section 4.12.

---

## 1. Task framing and authority

### 1.1 Operator goal (as given)

"Critically assess and develop a maintainable approach to data-driven modular
reconstruction of buildings, including interiors, unique details and necessary
Terrain adaptation." Act as architect and engineer; investigate real
implementation and contracts; challenge weak assumptions; recommend a sound
architecture; produce a detailed mock implementation plan. Disposable output only.

### 1.2 Binding operator decisions (cited, not restated as current behavior)

- `[DECISION]` Fully procedural modular assembly, not monolithic hand-built
  building meshes; JSON building blueprints describe each building; reusable mesh
  kits supply modular pieces; reference photography drives blueprint authoring,
  not asset sculpting (`Plugins/World/ProjectBuildingAssembly/docs/decision_record.md`
  "Decision"). Note: these are *accepted targets*, not implemented facts.
- `[DECISION]` Authority split: ProjectWorld owns canonical footprints, height
  evidence, massing constraints, disposable blockouts; ProjectBuildingAssembly
  owns optional modular assemblies for SELECTED stable feature IDs through a
  stable public contract and never owns geographic truth; assembled features
  replace/suppress their blockout; unselected buildings keep massing; authored
  hero buildings are protected overlays never forced through the generator
  (`decision_record.md` "Authority split", and
  `Plugins/World/ProjectWorld/docs/territory_contract.md#building-geometry-authority-split`).
- `[DECISION]` Assembly output is a generated layer under the ProjectWorld layered
  regeneration contract: profile-owned, disposable, manifest-owned, per-layer
  admission before first territory/playable use (`decision_record.md`
  "Regeneration governance").
- `[DECISION]` Legacy art kit intake is conditional on a licensing/provenance
  audit and measured integration prototype; the original procedural test kit is
  not a production art kit (`decision_record.md` "Conditional intake",
  `docs/legal/world_data_and_asset_policy.md`).
- `[DECISION]` Terrain adaptation is object-driven, bounded, and routes through
  the existing canonical correction owner as the single Terrain writer; generated
  object meshes never become a second writer; no PCG framework is authorized for
  the current baseline (`territory_contract.md#terrain-and-feature-separation`,
  `todo/02_backlog/world/20261112-1259_world_support_demonstrated_terrain_influence.md`).
- `[DECISION]` Project design philosophy: "future-proof, data-driven, fully
  modular, black-boxed, decoupled, component-driven" with the right separation of
  concerns and dependency direction (release slice D5,
  `todo/00_current/00_release_3.0.0.md`, and its archived baseline).

Note the latent tension between two decisions: "fully procedural" (D above) and
"unique details" (a goal term). The resolution is ownership, not a compromise:
procedural for generic stock, authored/protected for genuinely unique content
(section 4.7). This is a `[FINDING]`, flagged here because it must be settled by
the operator (Q3).

### 1.3 Hard boundaries inherited from stable owners

- Canonical data is immutable geographic truth; realization is a disposable
  consumer; generated state is never repaired by hand
  (`territory_contract.md#staged-authority-and-persistence`,
  `Plugins/World/ProjectWorld/docs/architecture/structure.md` invariant 3).
- One active Terrain producer; no fallback/parallel writer (structure.md
  invariant 5).
- Building projection + selected assemblies are owned by ONE buildings-layer
  transaction; selection cannot publish suppression independently of geometry
  (`territory_contract.md#building-geometry-authority-split`).
- Classification precedes selected massing exclusion
  (`ProjectWorldBuildingEditor/README.md`).
- Cell identity is the dirty unit; unchanged inputs -> no-op; manifests own
  artifacts; external actors are owned only when named exactly
  (`Plugins/World/ProjectWorld/docs/world_partition.md#generated-authority-and-transactions`).
- `Project*` naming for reusable code; no `Alis*`; ASCII-only docs; every JSON
  data file carries a relative `$schema`; scratch under `tmp/`
  (`E:/repos_alis/alis/AGENTS.md`).

---

## 2. Verified current architecture

### 2.1 Ownership map

`[FACT]` From `Plugins/World/ProjectWorld/docs/territory_generation.md`,
`architecture/structure.md`, `decision_record.md`:

- `CanonicalCompilation` (Python, `tools/World/CanonicalCompilation/`) compiles
  provider/OSM inputs into immutable canonical cells: terrain surface, water,
  roads, and building features with effective volumes.
- `ProjectWorld` (C++) owns reusable contracts, realization, layered
  regeneration, transactions, manifests, World Partition policy.
- `ProjectWorldBuildingEditor` (leaf editor module inside ProjectWorld) is the
  adapter: massing classification, the qualified assembly rectangle + support,
  selection admission, shared kit assets, persistent assembly actors.
- `ProjectBuildingAssembly` (editor-only, C++20, no ProjectWorld dependency) owns
  the portable layout core and native ISM realization from a dimension kit.
- `ProjectMaterial` owns presentation materials; `ProjectWorldData` owns concrete
  profiles/canonical/generated packages; `ProjectWorldTestData` owns synthetic
  fixtures; `Authored Unreal overlay` owns protected hero content.

### 2.2 Data flow (verified)

`[FACT]` Source -> canonical feature with `effective_volumes`
(`tools/World/CanonicalCompilation/app/buildings.py`,
`contracts/canonical-feature.schema.json`) -> C++ bundle
`FProjectWorldCanonicalFeature::BuildingVolumes`
(`Source/ProjectWorldEditor/Public/ProjectWorldCanonicalBundle.h`) -> adapter
`ProjectWorldBuildingAssembly::Prepare`
(`Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingAssembly.cpp`) ->
portable `ProjectBuildingAssembly::Build`
(`Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyPlan.cpp`) ->
`FPlan` (instances) -> `ProjectBuildingAssembly::Apply` (native ISM) via
selection persistence
(`Private/ProjectWorldBuildingSelectionPersistence.cpp`) -> buildings-layer
transaction (`Private/ProjectWorldBuildingLayerProducer.cpp`). Relationship graph:
`docs/architecture/diagrams/main.md`.

### 2.3 What the layout core actually does

`[FACT]` `Build(FInput, FPlan&, Error)` in
`ProjectBuildingAssemblyPlan.cpp` (64 lines):

- Validates a closed dimension/capacity envelope; refuses without a partial plan
  (`Out = {}` on failure). `FInput` = `FeatureId, Width, Depth, Height,
  SupportSpan, FKit`; `FKit` = 9 scalars (`PanelWidth, FloorHeight,
  WallThickness, SlabThickness, FoundationEmbed, MaximumSupportSpan,
  FrameFraction, WindowEvery, MaximumInstances`).
- Computes `Floors = round(Height/FloorHeight)`, `NX/NY` panels from
  `PanelWidth`, emits a foundation slab spanning
  `Bottom = -SupportSpan - FoundationEmbed` up to `SlabThickness`, then for each
  floor emits 4 sides of wall/window panels (`Panel % WindowEvery == 0` -> Window),
  skipping one ground-level bay as an entrance, then a flat roof slab.
- `EPart` is a **closed 4-value enum** `{Wall, Window, Floor, Roof}`.
  `FInstance` = `{Part, X, Y, Z, Width, Depth, Height, Yaw}` with `Yaw` only
  `0` or `90`.

This is one hard-coded composition algorithm. There is no room, door, stair,
interior wall, roof type, corner piece, bay variation, or per-building rule. The
"one implementation shared by host tests and native editor module"
(`Source/ProjectBuildingAssemblyEditor/README.md`) is a real invariant: the host
`Tests/plan_test.cpp` compiles this exact `.cpp` and checks captured dimensions,
determinism, four part families, bounds, story/slab contacts, a kit change, and
six refusal cases.

### 2.4 Kit and kit reader

`[FACT]` `Data/Kits/original_test_v1.json` + `Data/Schemas/modular-kit.schema.json`:
one `generator` const `original_box_modules_v1`, `provenance` const
`original_procedural_test_geometry`/`project`, and exactly 9 parameters. `ReadKit`
(`Private/ProjectBuildingAssemblyKit.cpp`) is deliberately strict: exact JSON
field counts (5/2/9), exact types, finite numbers, relative `$schema` resolving
to the plugin's schema, then it validates the kit by building a **fixed probe**:
`FInput Probe{ FeatureId="kit-admission", Width=10, Depth=10, Height=6,
SupportSpan=0, Kit=K }` and calling `Build`. This is the "10 x 10 x 6 m probe"
the TODO names. It is target-*dependent* but hard-coded, so a kit valid only for
another building size can refuse here or pass here yet fail the real building.

`[FACT]` Native `Shape(EPart, FKit)` and `ModuleMatches` hard-code the same 4
families; `Apply` requires `Options.Modules.Num() == 4`; `Matches` requires
exactly 4 `UInstancedStaticMeshComponent` named `Part0..Part3`. The shared kit
package in selection persistence always creates 4 `UStaticMesh` objects named
`Module0..Module3`, keyed by `GeometryIdentity("shared-kit", {}, Kit, Identity)`.

### 2.5 Qualification wall in the adapter

`[FACT]` `ProjectWorldBuildingAssembly::Prepare` refuses unless ALL hold:
exactly one "building" volume owned by this cell; no `CivilGrade`; no
`ConstructedSurface`; no authored overlay anchored to the feature;
`MinHeightMeters == 0`; exactly one geometry polygon; no holes; a 4-corner
rectangle fully inside one cell; fitted inscribed rectangle (inset =
`max(0.03, 2*quantization)`) still inside the original volume; no unresolved
Water overlap; complete Terrain triangle coverage of the foundation
(`Area == W*D`); finite support; `SupportSpan = Maximum - Minimum` within the
kit's `MaximumSupportSpan`. It sets `Transform` at `(Origin, Maximum)` (the
highest terrain point). It then optionally rebuilds remaining massing excluding
the selected feature and verifies the exclusion union.

So single-cell, single-volume, no-hole, grounded rectangles only. The canonical
data and the massing builder are already richer than this.

### 2.6 Canonical building data actually available

`[FACT]` `tools/World/CanonicalCompilation/app/buildings.py` builds "logical
buildings" from outlines + `building:part` relations + containment, with a
"complete parts" vs "outline fallback" selection; each of possibly MANY
`effective_volumes` carries `geometry` (Polygon/MultiPolygon, possibly with
holes), `min_height_m`, `height_m`, `height_basis`, `min_height_basis`,
`source_feature_id`. Height is `source-derived`, `inferred_from_levels`
(`levels*3.0`), or `procedural_default` (`house/industrial/apartments`/9.0)
(`app/adapters.py::_building_attributes`).
`FProjectWorldCanonicalBuildingVolume` in C++ keeps only `VolumeId,
SourceFeatureId, GeometryType, GeometryPolygons, MinHeightMeters, HeightMeters`
(basis/class/levels are dropped at the C++ boundary).

`[FACT]` The massing builder
(`Private/ProjectWorldBuildingMeshBuilder.cpp`, 749 lines) already handles
arbitrary footprints with `UE::Geometry::FGeneralPolygon2d`, holes, cell
clipping, constrained Delaunay triangulation, and pair overlap classification
(duplicate/contained/conflict/authored-mask). So general *footprint* geometry is
a solved World-side concern; the assembly adapter re-implements a narrow
inscribed-rectangle fit instead of consuming it.

`[FACT]` `territory_contract.md#fidelity-and-overlapping-physical-roles`: "OSM
building footprints and explicit height tags, floor estimates or defaults
describe approximate effective volumes, not LiDAR building meshes"; "Detailed
object reconstruction and surveyed microtopography are not prerequisites". There
is **no canonical data** for rooms, facades, openings, interior topology, roof
shape, or unique details. Any such data must be a NEW authored owner with its own
provenance.

### 2.7 Realization, identity, and the one-writer transaction

`[FACT]` `ProjectBuildingAssemblyRealization.cpp` (218 lines):

- `GeometryIdentity` = SHA1 over `project_modular_building_v1 | FeatureId |
  transform | frame | every instance | every expected shape triangle`. It is the
  no-op/reuse key.
- `Matches` is strict: generated-owner tag, geometry tag, transform, exactly 4
  ISM components, per-part shared mesh, collision, material, and an **actual
  triangle-level** `MeshMatches` (position + normals + one-sided collision), plus
  per-instance transform equality. "A stale tag cannot establish a no-op"
  (`docs/architecture/structure.md`).
- `Apply` stages a new actor, verifies it realizes the plan, then either reuses
  the previous actor (no-op), updates it in place (external actor, `Modify()`,
  stable GUID/name), or destroys the previous actor. Refuses duplicate ownership
  and unowned feature-associated actors.

`[FACT]` `ProjectWorldBuildingSelectionPersistence.cpp`:

- Shared kit packages live under the layer `ArtifactRoot` at
  `Kits/SK_<kit-key>` with 4 modules; actor GUID = stable hash of
  `GridId|building-assembly|<FeatureId>`; external actor package, runtime tags
  (`ProjectWorld.BuildingMassing.v2`, cell/grid tags), `SetIsSpatiallyLoaded(true)`,
  HLOD off, static mobility, no navigation.
- `Apply` qualifies every selection BEFORE creating assets or suppressing any
  massing; `Capture` re-derives each plan, checks GUID/ownership/`Matches`, does a
  roof line trace for physical support, and registers external-actor + kit
  package artifacts. Retirement destroys deselected actors, releases loaders,
  deletes owned kit packages no longer used.
- `HashCell` (`Private/ProjectWorldBuildingSelection.cpp`) = base
  `HashCellInput` + `|FeatureId|KitHash|GeometryIdentity(...)`. This is the exact
  per-cell identity/dirty seam.
- Producer `FProjectWorldBuildingLayerProducer`:
  `generator_id = project_building_massing`, `generator_version = 2`,
  `output_revision = 3`, `CanonicalSelectors = {buildings}`,
  `DependencyHaloCells = 0`, `PermittedDependencies = {terrain}`,
  `SpatialOwnership = cell_local`, `RuntimeMapping = world_partition_spatial`.
  Material-only adoption of modular ISMs **refuses** (`LayerProducer.cpp`:
  "Material-only adoption of modular ISM assemblies is unsupported in this
  prototype").
- Selection schema `Data/Schemas/project_world_building_selection.schema.json`:
  `schema_version 1`, `assemblies` array `maxItems: 5`, each
  `{feature_id, kit_source}`; kit paths confined to
  `Plugins/World/ProjectBuildingAssembly/Data/Kits/` (or TestData scratch
  `tmp/world/inputs/`).

### 2.8 Terrain handling and the adaptation boundary

`[FACT]` `Prepare` samples final canonical Terrain triangles under the fitted
rectangle (`ProjectWorldTerrainSurface::Triangles/IntersectXY/Height`), requires
complete coverage, computes `Minimum`/`Maximum` Z, sets
`SupportSpan = Maximum - Minimum`, anchors at `Maximum`, and refuses if the span
exceeds `MaximumSupportSpan` (kit default `0.5 m`). The foundation slab is a flat
box reaching down to `Minimum - FoundationEmbed`; it does **not** conform to the
terrain surface. It performs no Terrain edits and exposes no influence API
(`docs/architecture/structure.md` "Terrain influence boundary": "this prototype
instead reports a bounded support refusal; it provides no influence API or hidden
correction loop").

`[FACT]` `territory_contract.md#terrain-and-feature-separation`: "The accepted
extension direction is object-driven, bounded Terrain adaptation ... Start with
the existing canonical corrections; extend them only for a demonstrated consumer
failure ... A future PCG adapter must route accepted influence through that
authority ... A PCG graph must not independently sculpt the generated mesh after
canonical verification". Authored `terrain_patches` (radial, provenance-bound) and
`feature_overrides` are the existing correction seam
(`tools/World/CanonicalCompilation/contracts/authored-overlay.schema.json`);
`terrain_surface.py` applies patches after hydro-conditioning. Installed
`PCGMeshPartitionInterop.uplugin` is Experimental and needs a consumer proof
before adoption. The backlog
`20261112-1259_world_support_demonstrated_terrain_influence.md` explicitly gates
any influence seam behind a named failing consumer and a smallest RED fixture.

### 2.9 Verification owners and current limits

`[FACT]` `Plugins/World/ProjectBuildingAssembly/Tests/README.md`: host check
`python -B .../Tests/run.py` (seconds, no Unreal); source-bound native case uses
Manhattan `alis:osm:way:250440683` from a pinned OSM/Geofabrik release, prepared
via `prepare_case.py` into `tmp/world/building_assembly/proof/case.json`, run
through the safe serialized lane with exact test
`Project.World.BuildingAssembly.CanonicalPrototype`; saved-render preview
`Project.World.BuildingAssembly.SavedRender`; selection tests
`SelectionSettings`/`SelectionInputs`; registered lifecycle via
`prepare_lifecycle.py` + `run_lifecycle.ps1` on the seven-building two-cell
`ProjectWorldTestData/Data/Fixtures/building_neighborhood/` fixture. The README
states this "does not qualify a production layer, packaged building generation,
city streaming, detailed facades, interiors, or general Terrain grading."

`[FACT]` `docs/testing/world_pipeline_layers.md` defines L0-L4, "proof follows
admitted fidelity" (nonphysical contextual envelope vs required playable
route/support), R1/R2 independent review, proof traceability, and change
locality (`CHANGED`/`UNTOUCHED`, ARCHITECTURAL RED). `territory_contract.md#add-or-change-a-layer-producer`
requires bumping `output_revision` and re-recording the producer verify baseline
(`scripts/ue/world/record_verify_baseline.ps1`) when saved output behavior
changes.

### 2.10 Good assumptions to RETAIN (challenge-tested, kept)

`[FINDING]` Several TODO guardrails are correct and must survive any redesign:

- One buildings-layer transaction owning exclusion + kits + actors together.
- No speculative cache or scheduler; reuse existing shared kit package ownership.
- "Unchanged outputs do not imply skipped validation or sparse total I/O" - the
  sibling backlog `20261112-1259_world_reduce_authenticated_preparation_cost.md`
  backs this with measured preparation costs; do not claim a general incremental
  compiler.
- Deterministic `GeometryIdentity`-based no-op, staged replacement, fail-closed
  refusals, actual-reference validation, and "another cell's Terrain proof is not
  foundation proof" (`decision_record.md` "Prototype preflight").
- Native Engine First / no custom equivalents before qualifying installed
  capability (`docs/architecture/principles.md`, referenced by the World skill).

---

## 3. Critical assessment (findings)

F1 `[FINDING]` **Premise mismatch (BLOCKER on incremental extension).** The TODO
cannot reach the goal by construction (section 0, verdict 1). Evidence: TODO "Owner and
scope" and "Retained follow-ups"; `decision_record.md` "Owed before first
production use". Consequence: following the TODO as written yields a harder
prototype, not interiors/unique details/adaptation. Smallest safe fix: treat this
document as the architecture decision layer, re-scope to phased P0-P4, and keep
the TODO's items as P1 hardening within a broader slice.

F2 `[FINDING]` **"Data-driven" is currently nominal.** Composition rules live in
C++ (`Build`), not data; `Data/Buildings/` does not exist; the accepted blueprint
target is unimplemented. Consequence: adding a room type, roof type, or facade
rule means editing the portable core, the schema, the native `Shape`/`Matches`,
the identity digest, the 4-module shared package, and every test - a wide
cross-owner change that is ARCHITECTURAL RED under the current boundary. Fix:
move composition into versioned data and make the core an interpreter (P0).

F3 `[FINDING]` **The 4-part enum is a closed extension set.** `EPart`,
`Shape`, `ModuleMatches`, `Matches` (`Components.Num() != 4`),
`Options.Modules.Num() != 4`, and the shared-kit `Module0..Module3` are all
coupled to "exactly four families". Interiors (interior wall, door, stair,
ceiling, roof variants) and unique details cannot be expressed without breaking
that invariant everywhere. Fix: open module-id set keyed by stable string id
(P0); keep strict per-module validation.

F4 `[FINDING]` **Interiors have no data source and no admitted-use class.**
Canonical features carry only polygons + heights + basis; no rooms/openings/roof
(`2.6`). "Proof follows admitted fidelity" (`world_pipeline_layers.md`) means a
playable interior (walkable, collision, navigation, streaming) has far stronger
obligations than a nonphysical contextual shell, and the current buildings layer
sets `SetCanEverAffectNavigation(false)` on ISM parts. Fix: a NEW authored
blueprint owner (provenance-bound), plus an explicit operator gate on whether
interiors are contextual or playable (Q1) before designing their geometry/collision.

F5 `[FINDING]` **Unique details are structurally opposite to the uniform shared
kit.** Value here comes from one kit -> 4 shared meshes -> reused across
buildings. Per-building unique meshes would each need their own package/manifest
entry and defeat the shared-identity reuse. Two existing facts point to the right
owner: authored hero buildings are protected overlays never forced through the
generator (`decision_record.md`), and the only detailed building content in the
repo is legacy City17 Blueprints (`BP_BuildingHrushev`, `HruSideDetailed_Bp`,
etc.) tied to vendor instance arrays, slated for removal (backlog
`20260918-1702_world_replace_vendor_instance_arrays.md`; release slice D9). Those
Blueprints are NOT a reusable owner. Fix: stock variation via blueprint
overrides (generated, data-owned); genuinely unique content via the protected
authored overlay; the assembly must refuse/hand-off authored overlays (already
does). Do not bake unique meshes into the shared kit.

F6 `[FINDING]` **"Necessary Terrain adaptation" must be read narrowly.** The goal
word "necessary" plus the stable SOT forbid a general influence API, an editor
sculpt layer, or a second writer (`2.8`); the backlog
`20261112-1259_world_support_demonstrated_terrain_influence.md` gates it behind a
named failing consumer and a RED fixture. Consequence: proposing an Assembly-side
"terrain conform" pass would be an ARCHITECTURAL RED. Fix: consume final Terrain;
generalize *support qualification* (P0/P1); only when a real case fails existing
corrections, submit bounded intent to the canonical owner (P4).

F7 `[FINDING]` **Foundation support is approximate, not surveyed.** The building
is anchored at the MAX terrain Z; the slab is a flat box down to `MIN - embed`;
`SupportSpan` is one scalar (`2.8`). On sloped ground this neither conforms to the
surface nor proves whole-foundation contact beyond complete-area coverage and a
roof line trace. `decision_record.md` itself notes "whole-foundation support ...
[is] not established" by the ground-anchor regression. Fix: qualify a bounded
support surface (per-corner/per-cell heights) rather than a single scalar, while
keeping refusal on incomplete coverage; do not reshape Terrain.

F8 `[FINDING]` **Kit-reader probe is target-dependent and mis-sited.** The fixed
10x10x6 probe (`2.4`) both under- and over-qualifies. Fix: replace probe
admission with target-independent kit admission (declare module dimensions,
snap/anchor, capacity, provenance, strict types) and qualify the *actual*
selected footprint/height at plan time (P0/P1) - this matches the TODO's
follow-up but must be done during the schema redesign, not bolted onto the probe.

F9 `[FINDING]` **One bounded neighborhood is necessary, not sufficient.** The
TODO correctly says "D36's source-shaped synthetic fixture proves lifecycle
behavior, not city geography, art variety or streaming". A single real
neighborhood anchors footprints/support but does not prove art variety, city
scale, or streaming. Fix: keep it as the P1 acceptance anchor; add explicit,
separate proof for streaming/runtime only when a playable interior or city
publication is admitted (Q4), per `world_pipeline_layers.md` uncooked-gameplay
admission.

F10 `[FINDING]` **"Fully procedural" vs "unique details" is unresolved.**
`[DECISION]` fully procedural; `[goal]` unique details. Evidence shows authored
hero buildings already exist as protected overlays. Fix: settle ownership by
operator (Q3); default recommendation is procedural stock + authored heroes.

F11 `[FINDING]` **Footprint generalization is an Assembly problem, not a data
problem.** The canonical compiler and massing builder already handle
multipolygon/holes/multivolume/cell clipping (`2.6`); the assembly adapter is the
narrow consumer. Fix: extend the adapter's qualification toward the existing
polygon contract (P1) rather than adding a new decomposition framework. Keep
"classification precedes exclusion".

---

## 4. Recommended architecture (proposals)

### 4.1 Guiding principle

`[PROPOSAL]` One authority per concern, one explicit extension axis. Separate
four things the prototype conflates:

```
geographic/physical truth  (canonical: footprints, volumes, final Terrain)  - unchanged owner
composition intent         (building definition/blueprint: NEW data owner)   - new
deterministic planning     (portable interpreter: plan = module instances)    - refactor
native realization         (N modules by stable id, strict validation)       - generalize
```

Keep the buildings-layer transaction, manifests, dirty scope, no-op identity,
and rollback exactly as owned today. Every new input (blueprint, asset library)
participates in `HashCell` so unchanged inputs stay a no-op and a changed
blueprint dirties only its building's cell.

### 4.2 New owner: building definition / blueprint

`[PROPOSAL]` Realize the already-accepted `Data/Buildings/` decision. A blueprint
is authored, provenance-bound, versioned data (NOT canonical geography, NOT a
mesh). It references canonical `feature_id`s or a stable archetype selector, a kit
/ asset-library id, and a closed rule set expressing: story count/height policy,
bay rhythm and opening placement, room/interior partition layout, roof type, and
unique-detail hooks. It must carry `$schema` (relative), a `provenance` record
consistent with `docs/legal/world_data_and_asset_policy.md` (photography-driven,
no Google/scraped/NoDerivatives inputs), and a `schema_version`. Its hash joins
selection identity.

`[PROPOSAL]` Keep the rule set SMALL and closed (KISS/YAGNI): enough to express
the demonstrated cases (stories, bays, rooms, openings, roof). Do NOT build a
general shape-grammar engine or PCG graph asset; `structure.md` "Terrain
influence boundary" explicitly says the bounded proof "does not require a graph
asset or a general grammar engine", and Native Engine First rules out a custom
grammar framework before a demonstrated need. The interpreter is a portable C++20
function over this data, still host-testable without Unreal.

### 4.3 Open module set / asset library

`[PROPOSAL]` Replace `EPart`'s 4-value enum with an open set keyed by stable
string `module_id`. The kit becomes an *asset library*: an ordered, hashed list
of module declarations `{module_id, kind, dimensions, anchor/snap metadata,
material role, collision policy, provenance hash, optional mesh reference}`. The
original procedural test kit remains one library whose four modules map to the
old Wall/Window/Floor/Roof, so the existing prototype/fixtures stay valid. This
is the single extension axis: interiors and unique details add modules, not enum
values or synchronized edits across six files.

`[PROPOSAL]` Two admission tiers, matching `decision_record.md` "Conditional
intake": (a) original procedural geometry (current, no vendor intake); (b)
rights-cleared art kits admitted only after the licensing/provenance audit and a
measured integration prototype. Material references stay soft/role-based
(`territory_generation.md`); material-only adoption for modular ISMs stays an
explicit refusal until qualified (the current TODO follow-up).

### 4.4 Portable planner as a deterministic interpreter

`[PROPOSAL]` Refactor `Build` into `Build(FInput, FDefinition, FLibrary, FPlan&,
Error)` (names `[MOCK]`) where `FPlan` is a list of `{module_id, transform, slot}`
rather than `{EPart, ...}`. Input validation stays fail-closed (refuse without a
partial plan). `FPlan` remains `==`-comparable and deterministic for the host
tests. Keep the layout core thin; put rule interpretation in a new sibling file
(e.g. `Private/ProjectBuildingAssemblyInterpreter.cpp`) to respect the project
file-size guardrail (the current `Plan.cpp` is 64 lines; a full interpreter must
not silently grow it past ~700).

`[PROPOSAL]` `FInput` generalizes from a scalar `Width/Depth/Height/SupportSpan`
toward a qualified footprint descriptor (polygon(s) in a local frame, effective
volume(s), per-corner support heights, owner cell, quantization). This is where
F7 and F11 land. Keep the single-inscribed-rectangle path as the first admitted
case so the lifecycle proof is preserved.

### 4.5 Native realization generalized to N modules

`[PROPOSAL]` `Shape`/`ModuleMatches`/`Matches`/`Apply` operate over the library's
module list: `Options.Modules` becomes a map keyed by `module_id`; components are
named deterministically from `module_id`; `GeometryIdentity` hashes the ordered
module declarations and every planned instance. Invariants to PRESERVE: staged
replacement, actual triangle/normal/one-sided-collision validation, no-op reuse
via identity, unowned/duplicate actor refusal, stable external actor GUID/name,
shared kit package ownership, and "a stale tag cannot establish a no-op". This is
a widening, not a relaxation.

### 4.6 Interiors as a distinct admitted-use class

`[PROPOSAL]` Interiors are expressed by the blueprint as additional modules
(interior wall, door, stair, ceiling) placed inside the admitted volume. Default
admitted use = "nonphysical contextual envelope" (`world_pipeline_layers.md`):
prove schema, role, extent, frame, provenance, consumed identity, determinism; do
NOT claim walkable support. If the operator admits playable interiors (Q1),
escalate to collision/navigation/streaming/uncooked-gameplay proof and reconsider
the current `SetCanEverAffectNavigation(false)` and `cell_local` granularity.
Visibility grouping (Data Layers) is presentation only: `territory_contract.md`
states Data Layers "do not establish generation ownership by themselves".

### 4.7 Unique details: two channels, one rule

`[PROPOSAL]` (a) Stock variation: per-feature blueprint override records,
generated, data-owned, hashed into cell identity - the generator stays
deterministic. (b) Genuinely unique/hero content: existing protected authored
overlay; the assembly detects the anchor and refuses/hands off, exactly as
`Prepare` does today. Never force hero content through the generator; never bake
per-building unique meshes into the shared kit (F5). Legacy City17 Blueprints are
migration input, not an owner.

### 4.8 Terrain adaptation: consume, then (only if demonstrated) request

`[PROPOSAL]` Default path: qualify the final canonical Terrain support and refuse
unsupported cases (unchanged). Generalize the scalar `SupportSpan` to a bounded
support surface with per-corner/per-cell heights and keep complete-coverage
refusal (F7). NO Assembly/World-adapter Terrain writes, NO hidden correction loop,
NO PCG graph sculpting. When and only when a real case demonstrates a residual
that `terrain_patches` / `feature_overrides` / refinement cannot serve, add the
smallest request/response seam through the canonical owner per
`20261112-1259_world_support_demonstrated_terrain_influence.md`: name the
consumer, capture a source-faithful RED, preserve source vs authored provenance,
prove deterministic reconstruction, refusal/no-mutation, unaffected neighbors,
exact dirty scope, and recheck only affected consumers. Final Terrain, dependent
Water, and object support must agree before admission.

### 4.9 Identity / dirty-scope seam

`[PROPOSAL]` Extend `ProjectWorldBuildingSelection::HashCell` from
`base | FeatureId | KitHash | GeometryIdentity` to also cover
`BlueprintHash | LibraryHash | SupportSurfaceIdentity | plan identity`. Every
selection record gains `blueprint_source` (and library ref). Selection schema
moves to `schema_version 2` with a bounded, closed document; raise
`maxItems` only when a real neighborhood demonstrates the need (do not remove the
bound speculatively). Producer `output_revision` bumps and the
`project_building_massing` verify baseline is re-recorded once saved output
behavior changes (per `territory_contract.md#add-or-change-a-layer-producer`).

### 4.10 Streaming / World Partition

`[PROPOSAL]` Keep `cell_local` + `world_partition_spatial` + HLOD off + static
mobility + Nanite-for-compatible-opaque as the default. Interiors that stay within
one building's cell need no new granularity. Cross-cell/multivolume buildings
(F11) need explicit cross-cell qualification and a decision on whether interior
actors get per-building or per-cell spatial ownership; this is a P1/P2 concern,
not a P0 framework. Do not introduce a custom streaming/HLOD mechanism; HLOD is
disabled for generated territories by policy (`world_partition.md`).

### 4.11 Boundary declaration (required before design is accepted)

Per the World engineering skill and `world_pipeline_layers.md` "Change Locality":

```
owning black box:   ProjectBuildingAssembly (planning + native realization)
                    + ProjectWorldBuildingEditor adapter (qualification/integration)
public interface / contract:
                    stable public footprint/massing contract (decision_record.md);
                    buildings-layer producer registry + HashCell identity;
                    blueprint/library/selection schemas
expected components CHANGED:
                    ProjectBuildingAssembly Plan/Realization/Kit + Data/Schemas + Tests;
                    ProjectWorldBuildingEditor Assembly/Selection/Persistence/LayerProducer
                    + selection schema + producer descriptor/verify baseline;
                    new Data/Buildings blueprint owner + docs
expected components UNTOUCHED:
                    CanonicalCompilation geography (footprints/volumes/terrain) - READ ONLY;
                    canonical-feature schema; terrain single-writer producer;
                    Water/Road/Vegetation/Gameplay producers;
                    transaction/manifest/active-set/shared pipeline module;
                    ProjectMaterial authority; authored overlay;
                    massing builder general polygon handling (consumed, not rewritten)
```

If implementation needs to edit an UNTOUCHED owner or regenerate its outputs,
that is ARCHITECTURAL RED: stop and re-review the boundary. In particular, if
"Terrain adaptation" starts requiring a Terrain producer edit, that is a separate
gated slice (P4), not this black box leaking.

### 4.12 Alternatives considered and rejected

- `[PROPOSAL]` **Grow `Build()` with `if/else` for rooms/roofs/unique bits.**
  Rejected: violates the "one portable core" invariant cleanly only in the small
  case; at scale it becomes a general engine inside one file, breaks F2/F3, and
  makes the host test the only guard. No extension axis.
- **General PCG shape-grammar / graph-asset frontend.** Rejected for now:
  `structure.md` says the bounded proof does not require a grammar engine;
  `PCGMeshPartitionInterop` is Experimental and needs a consumer proof; Native
  Engine First forbids a custom framework before a demonstrated invariant fails.
  Reconsider only if the closed blueprint schema demonstrably cannot express an
  admitted case.
- **Second Terrain writer / in-editor conform pass / PCG sculpt.** Rejected:
  violates structure.md invariant 5 and `territory_contract.md` single-writer
  rule; explicitly a separate gated backlog.
- **General asset cache / scheduler / incremental compiler.** Rejected: the TODO
  and `20261112-1259_world_reduce_authenticated_preparation_cost.md` both warn
  that unchanged outputs do not imply sparse total I/O; reuse shared kit package
  ownership instead.
- **Per-building unique meshes in the shared kit.** Rejected: destroys the
  shared-identity no-op and reuse that is the prototype's main value (F5).
- **New parallel "building definition" authority alongside canonical features.**
  Rejected: canonical is immutable geographic truth; blueprint is authored
  composition intent with a DIFFERENT owner/provenance, not a second geography
  SOT. It references feature ids; it never edits them.

---

## 5. Mock implementation plan (phases P0-P4)

Every file/signature/schema below is `[MOCK]`: descriptive, not compiled or run.
Each phase states goal, touched owners, illustrative artifacts, invariants,
test-first Red/Green evidence, verification commands (from the docs), and a stop
condition. Phases are gated; a phase does not start before the previous one's
acceptance and the operator gates it needs (section 7) are closed.

### P0 - Architecture decision, data contracts, portable interpreter generalization

Goal: make composition data-driven and the module set open, WITHOUT changing any
admitted behavior/output for the current rectangle case (so existing lifecycle
proof stays green). This is an L0/L1 refactor with a preserved no-op.

Touched (CHANGED):
- `Plugins/World/ProjectBuildingAssembly/Data/Schemas/` - add
  `building-definition.schema.json`, `module-library.schema.json` (`[MOCK]`).
- `Plugins/World/ProjectBuildingAssembly/Data/Buildings/` - NEW owner dir, one
  version-1 blueprint mapping the retained rectangle case; `Data/Kits/` gains a
  library-form expression of `original_test_v1` (backward-compatible).
- `.../Source/ProjectBuildingAssemblyEditor/Public/ProjectBuildingAssemblyPlan.h`
  and `Private/ProjectBuildingAssemblyPlan.cpp` - `FPlan`/`FInstance` move to
  `module_id`; add `FDefinition`, `FLibrary`, `FSupportSurface`.
- `Private/ProjectBuildingAssemblyInterpreter.cpp` (new sibling, SRP seam) -
  interpreter over the closed rule set; `Plan.cpp` stays the thin validation core.
- `Private/ProjectBuildingAssemblyKit.cpp` - target-independent library admission
  (drop the 10x10x6 probe; strict types + actual-input qualification) (F8).
- `Private/ProjectBuildingAssemblyRealization.cpp` + `.h` - N modules keyed by
  `module_id`; `GeometryIdentity`/`Matches` preserved invariants.
- `Tests/plan_test.cpp` + `Tests/run.py` - host coverage for the interpreter.
- Docs: `decision_record.md`, `docs/architecture/structure.md`, `Data/README.md`,
  `docs/pitfalls.md` (module-set trap), `docs/architecture/diagrams/main.md`.

Illustrative artifacts `[MOCK]`:

```
// building-definition.schema.json (sketch, not authoritative)
{ "$schema": ".../building-definition.schema.json", "schema_version": 1,
  "definition_id": "...", "selector": {"feature_ids": [...]} | {"archetype": "..."},
  "library_source": "Data/Kits/<...>.json",
  "provenance": {"kind": "authored", "authority": "...", "evidence": "..."},
  "rules": { "stories": {...}, "bays": {...}, "rooms": [...], "openings": [...],
             "roof": {...}, "detail_refs": [...] },
  "additionalProperties": false }
```

```
// module-library member (sketch)
{ "module_id": "wall.solid", "kind": "panel", "dims_m": [2.0, 0.18, 3.0],
  "anchor": "edge.center", "material_role": "building",
  "collision": "complex_as_simple", "provenance_hash": "<sha256>" }
```

```
// Plan.h surface (sketch)
namespace ProjectBuildingAssembly {
  struct FPlacement { std::string ModuleId; double X,Y,Z,W,D,H,Yaw; /* == */ };
  struct FPlan { std::vector<FPlacement> Instances; std::string LibraryHash; /* == */ };
  bool Build(const FInput&, const FDefinition&, const FLibrary&, FPlan&, std::string& Error);
}
```

Invariants: deterministic `FPlan`; refuse without partial plan; closed
dimensions/capacities; unchanged original-kit case produces byte-equivalent
`GeometryIdentity` for the retained rectangle (regression anchor); `Project*`
names; relative `$schema`; ASCII docs; no `Alis*`.

Test-first (L0, Red -> Green):
- Red: host test that feeds a blueprint declaring an interior-wall module and
  asserts a placement with that `module_id` - fails today (enum has 4 families).
- Red: host test for a kit valid at 12x8x9 but not at 10x10x6 - fails today
  (fixed probe).
- Green: `python -B Plugins/World/ProjectBuildingAssembly/Tests/run.py` passes
  (existing captured dimensions/determinism/refusals + new cases); a golden
  plan for the retained rectangle matches the pre-refactor plan exactly.

Verification: host `run.py`; schema checks via the pinned World Python env
(`tools/World/ExecutionEnvironment/README.md`); `validate_all.py` for new
`$schema` cross-refs; no Unreal needed at L0. Stop: no native/native-save proof
yet; do not claim engine behavior.

### P1 - Qualification generalization and the bounded real neighborhood

Goal: extend the adapter to consume the existing polygon/volume contract (F11),
land the TODO's hardening items, and anchor on one real neighborhood. This is the
TODO's valid contribution, now correctly scoped.

Touched (CHANGED):
- `Private/ProjectWorldBuildingAssembly.cpp/.h` - qualified footprint descriptor
  from `BuildingVolumes` (multipolygon/holes/multivolume/cross-cell); bounded
  support surface instead of scalar span (F7); keep single-inscribed-rectangle as
  first admitted case. Reuse `ProjectWorldBuildingMeshBuilder` polygon handling;
  do NOT rewrite it (UNTOUCHED).
- `Private/ProjectWorldBuildingSelection.cpp/.h` + `...Persistence.cpp` - add
  `blueprint_source`/library to selection records, extend `HashCell` (4.9),
  generalize shared kit package to N modules, keep retirement/loader discipline.
- `Data/Schemas/project_world_building_selection.schema.json` - `schema_version 2`
  bounded document; raise `maxItems` only on demonstrated need.
- `Private/ProjectWorldBuildingLayerProducer.cpp` - `output_revision` bump;
  re-record `project_building_massing` verify baseline.
- `Tests/`: source-faithful regression for one unsupported footprint,
  cross-cell/multivolume support, and Terrain span (TODO follow-up 1); prepare a
  fresh case from admitted authority via `prepare_case.py`; lifecycle via
  `prepare_lifecycle.py` + `run_lifecycle.ps1`.
- Docs: `ProjectWorldBuildingEditor/README.md`, `territory_contract.md`
  `#building-geometry-authority-split`, `Tests/README.md`, `docs/pitfalls.md`.

Invariants: classification precedes exclusion; selected-only suppression;
unselected massing preserved; authored/protected content preserved; exact dirty
scope; deterministic regeneration; failure recovery; unchanged shared module
packages reused; no sparse-total-I/O claim; selection identity covers all
consumed inputs; refusal leaves protected state unchanged.

Test-first (L1, exact native tests via safe serialized lane):
- Red: a real source-shaped fixture with a holed/multivolume/cross-cell footprint
  and a >0.5 m support span is refused today; capture it before the fix.
- Green: `Project.World.BuildingAssembly.CanonicalPrototype`,
  `SelectionInputs`, `SelectionSettings`, `Project.World.MeshTerrain.EvidenceDestinations`
  via `scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-name>` /
  `run_cpp_tests_safe.ps1`; registered lifecycle `run_lifecycle.ps1` (no-op,
  kit-only change, failed apply after save, three buildings sharing meshes, cold
  reconstruction, selection removal, resume paths); exact dirty units and asset
  inventory receipts.
- Measure reclassification/massing before adding any reuse (TODO follow-up 5);
  report total I/O honestly.

Verification: L1 fixture with rollback, L2 `plan`-selected Matrix ONLY at slice
exit (`python -S tools/World/EndToEndValidation/bootstrap.py plan --base ...`);
`canonical_authority` mode for a World-only change, `full` only if a producing
upstream change is accepted. Stop: still no city publication, no playable interior
claim, no Terrain adaptation (UNTOUCHED owners stay shut).

### P2 - Interiors (contextual by default)

Goal: express interior modules through the open module set, qualified footprint,
and blueprint rules; prove as a nonphysical contextual envelope first.

Touched (CHANGED): `Data/Buildings/` blueprint rules (rooms/openings/stairs),
library modules (interior wall/door/stair/ceiling), interpreter, realization
(N modules), selection identity, Tests, docs. UNTOUCHED: canonical geography,
Terrain, other producers, transaction. Gate: Q1 (contextual vs playable) and Q2
(provenance) MUST be closed before P2 geometry work.

Invariants: interiors stay within the admitted effective volume; no competing
building geometry; deterministic plan; fail-closed on unknown module id; no
navigation/collision/streaming claim unless playable admission is granted.

Test-first (L0/L1):
- Red: a blueprint with a room partition produces interior placements the current
  4-family core cannot express (fails today).
- Green: host interpreter + exact native case with actual mesh/normal/collision
  validation; saved/reload; deterministic regeneration; unselected massing and
  authored overlays unchanged; exact dirty scope.
- If playable (Q1 = yes): escalate to `world_pipeline_layers.md` "Uncooked
  gameplay admission" (real possession, collision, cell residency, no cooking),
  reconsider `SetCanEverAffectNavigation(false)`, and add streaming/clearance
  proof. Otherwise stop at contextual proof and record the non-goal.

### P3 - Unique details

Goal: stock variation via per-feature blueprint overrides; hero content via
protected authored overlay hand-off. Gate: Q3 (owner decision).

Touched (CHANGED): selection records (override ref), blueprint owner, interpreter
(detail hooks), `Prepare` overlay detection, Tests, docs. UNTOUCHED: shared kit
package identity (do not bake unique meshes), authored overlay bytes, canonical.

Invariants: an authored-overlay-anchored feature always refuses assembly and
keeps its protected content byte-identical; overrides are hashed into cell
identity; no per-building unique mesh enters the shared kit; deterministic.

Test-first (L0/L1):
- Red: an override that changes a facade detail must change the plan and the
  cell hash, and an authored-overlay feature must refuse assembly - assert both
  against current behavior.
- Green: exact native tests for override regeneration and authored protection;
  no-op for unchanged overrides; retirement restores massing.

### P4 - Necessary Terrain adaptation (gated, may legitimately not start)

Goal: ONLY when a real consumer fails existing canonical corrections, add the
smallest bounded influence seam through the canonical single writer. This is a
SEPARATE slice with its own RED and acceptance; it is not a prerequisite for the
current Terrain baseline (`20261112-1259_world_support_demonstrated_terrain_influence.md`).

Trigger: name the real consumer, failing input, required support/clearance, and
the residual `terrain_patches`/`feature_overrides`/refinement cannot represent;
capture the smallest source-faithful RED first. Gate: Q5.

Touched (CHANGED, in that separate slice): canonical correction owner
(`terrain_surface.py` / authored-overlay contract) and the dependent consumer
identity; NOT the Assembly native realization. UNTOUCHED here: everything in
P0-P3's UNTOUCHED set except the explicitly admitted correction seam.

Invariants: final Terrain, dependent Water, and object support agree before
admission; source vs authored provenance preserved; no feedback loop (a PCG graph
must not feed its own output back as source); deterministic reconstruction;
refusal/no-mutation; unaffected neighbors; exact dirty scope; measured local
execution; compare installed PCG MeshPartition Query/Write and patch/projection
spawners and distinguish foliage exclusion from real Terrain modification.

---

## 6. Invariant-to-proof traceability and verification matrix

`[MOCK]` traceability table (one row per materially new invariant; final proof
must match the acceptance surface and execution envelope per
`world_pipeline_layers.md`):

| Invariant | Acceptance surface | Execution envelope | Cheapest proof (L0/L1) | Final proof | Stop condition |
|---|---|---|---|---|---|
| Composition is data-driven; plan deterministic | `FPlan` | host C++20 / native editor | `run.py` golden plan + interpreter unit | exact native `CanonicalPrototype` | refusal/partial-plan or drift |
| Open module set validates each module actually | realized ISM actor | safe serialized lane, save/reload | new module placement unit | native `Matches`/triangle/normal check | stale-tag no-op or unowned actor accepted |
| Selected-only suppression; unselected + authored preserved | buildings-layer output | L1 fixture w/ rollback | selection unit tests | registered lifecycle + exact dirty scope | any unrelated massing/overlay change |
| Identity covers blueprint/library/support | `HashCell` | host + L1 | hash unit for each new input | cold reconstruction + no-op | unchanged input dirties a consumer |
| Interior stays within admitted volume (contextual) | `FPlan`/actor | host + native case | bounds unit | exact native case | any claim of walkable support without gameplay proof |
| Terrain adaptation has one writer | canonical surface + consumers | L1/L2 gated slice | RED fixture + refusal/no-mutation | affected Matrix + authority audit | a second writer or hidden correction loop |
| Shared kit reuse / rollback | packages + manifests | normal wrapper | kit-only change receipt | cold reconstruction + retirement | orphan kit package or dirty input mistaken for output |

Verification layers (from `world_pipeline_layers.md`), applied by phase:

- L0 (P0): `python -B Plugins/World/ProjectBuildingAssembly/Tests/run.py`;
  schema/`$schema` checks; `validate_all.py`. No persistent world mutation.
- L1 (P1-P3): `prepare_case.py` + exact `run_cpp_tests_safe.ps1` tests;
  `prepare_lifecycle.py` + `run_lifecycle.ps1`; `scripts/ue/test/unit/iterate.ps1
  -TestFilter <exact-name>`; `record_verify_baseline.ps1` when output revision
  bumps. Transient outputs + TestData only; rollback proven.
- L2 (slice exit): `bootstrap.py plan --base <slice-base>` selects
  `canonical_authority` (World-only) or `full` (if producing upstream accepted);
  one common Check receipt reused; uncooked-gameplay admission only if a playable
  route/support changed.
- L3/L4: only with explicit durable-authority/package request; NOT part of this
  investigation or the default phases.
- R1/R2 independent review before production code and after L0/L1 pass; reviewer
  reconstructs the `CHANGED`/`UNTOUCHED` boundary from SOTs.

Do NOT rerun a broad gate merely because an edit happened; rerun only when a
changed input/authority invalidates what the gate authenticated.

---

## 7. Unresolved questions and assumptions

Operator gates (answers materially change scope/architecture/safety; ask before
the gated phase):

- `Q1 [OPEN - BLOCKING for P2]` Are interiors a nonphysical contextual envelope,
  or required playable/walkable space (collision, navigation, streaming)? This
  selects the entire proof envelope and whether `SetCanEverAffectNavigation(false)`
  and `cell_local` must change. Default while open: treat as contextual only
  (`A1`).
- `Q2 [OPEN - BLOCKING for P0/P2]` What is the provenance/licensing basis for
  blueprints and interior art? `decision_record.md` says photography-driven and
  not asset sculpting; `docs/legal/world_data_and_asset_policy.md` forbids
  Google/scraped/NoDerivatives/unknown inputs and requires a source ledger.
  Confirm the intended authored data source and distribution class before adding
  `Data/Buildings/`.
- `Q3 [OPEN - BLOCKING for P3]` Are "unique details" an operator decision to keep
  "fully procedural" for stock and use protected authored overlays for heroes
  (recommended), or to allow per-feature generated overrides? Resolves the F10
  tension with the `[DECISION]` "fully procedural, not monolithic meshes".
- `Q4 [OPEN - NON-BLOCKING]` Is city publication / streaming / art-variety
  acceptance in scope now, or deferred? A single real neighborhood (P1) does not
  prove these. Default: deferred (`A2`).
- `Q5 [OPEN - BLOCKING for P4]` Is there a named real building whose required
  support/clearance existing canonical corrections cannot serve? Without it, P4
  does not start and "necessary Terrain adaptation" resolves to "qualify and
  refuse on final Terrain" only.

Working assumptions (agent-selected, never authority):

- `A1 [ACTIVE]` Interiors are contextual unless Q1 says otherwise.
- `A2 [ACTIVE]` City publication/streaming is out of the near-term slice.
- `A3 [ACTIVE]` Blueprint is authored composition data (separate owner), not a
  change to canonical geography or its schema.
- `A4 [ACTIVE]` The existing rectangle case and the seven-building neighborhood
  fixture remain valid as regression anchors through the refactor.
- `A5 [ACTIVE]` No new Terrain writer, PCG graph, cache, or scheduler.

---

## 8. Investigation limits and evidence index

Limits (state plainly): this is a READ-ONLY investigation. No Unreal execution,
no automation tests, no schema validation run, no host compile, no implementation.
Runtime/streaming/collision claims about interiors are design proposals, not
acceptance evidence. Where a plan step says "verify via command X", that command
was read from a stable owner but was NOT executed here. Installed-engine specifics
(e.g. current PCG experimental status, exact Nanite/ISM behavior for interiors)
must be qualified against the actual installed engine before implementation
(`docs/architecture/principles.md` Native Engine First; `structure.md` "Terrain
influence boundary" warns Epic docs may target a newer engine). The `[MOCK]`
schemas/signatures are descriptive and must be checked against the project's
schema conventions and installed engine before use.

Pre-existing working-tree changes were observed via `git status` and left
untouched. This file is the only writable output.

Evidence index (all `[FACT]`, paths repo-relative):

- Starting task: `todo/02_backlog/world/20261112-1332_world_extend_building_assembly.md`.
- Accepted targets / limits: `Plugins/World/ProjectBuildingAssembly/docs/decision_record.md`,
  `docs/architecture/structure.md`, `docs/architecture/diagrams/main.md`,
  `docs/pitfalls.md`, `Source/ProjectBuildingAssemblyEditor/README.md`,
  `Tests/README.md`, `Data/README.md`, `Data/Kits/README.md`,
  `Data/Kits/original_test_v1.json`, `Data/Schemas/modular-kit.schema.json`.
- Layout/realization/kit code:
  `Source/ProjectBuildingAssemblyEditor/Public/ProjectBuildingAssemblyPlan.h`,
  `Public/ProjectBuildingAssemblyRealization.h`,
  `Private/ProjectBuildingAssemblyPlan.cpp`,
  `Private/ProjectBuildingAssemblyRealization.cpp`,
  `Private/ProjectBuildingAssemblyKit.cpp`, `Tests/plan_test.cpp`,
  `Tests/case.schema.json`.
- Adapter/producer/persistence:
  `Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/README.md`,
  `Private/ProjectWorldBuildingAssembly.cpp/.h`,
  `Private/ProjectWorldBuildingSelection.cpp/.h`,
  `Private/ProjectWorldBuildingSelectionPersistence.cpp`,
  `Private/ProjectWorldBuildingLayerProducer.cpp`,
  `Private/ProjectWorldBuildingMeshBuilder.h/.cpp`,
  `Private/ProjectWorldBuildingRealization.cpp`;
  `Plugins/World/ProjectWorld/Data/Schemas/project_world_building_selection.schema.json`,
  `Data/Producers/project_building_massing.json`.
- Contracts/SOTs: `Plugins/World/ProjectWorld/docs/territory_contract.md`
  (`#ownership-layers`, `#terrain-and-feature-separation`,
  `#fidelity-and-overlapping-physical-roles`, `#staged-authority-and-persistence`,
  `#layered-regeneration-contract...`, `#add-or-change-a-layer-producer`,
  `#canonical-feature-input-compatibility`, `#building-geometry-authority-split`),
  `docs/architecture/structure.md`, `docs/world_partition.md`,
  `docs/territory_generation.md`, `docs/architecture/diagrams/terrain_features.md`.
- Canonical data model / compiler:
  `Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldCanonicalBundle.h`,
  `tools/World/CanonicalCompilation/app/buildings.py`,
  `app/adapters.py::_building_attributes`,
  `contracts/canonical-feature.schema.json`,
  `contracts/authored-overlay.schema.json`, `app/terrain_surface.py` (referenced
  by `territory_contract.md`).
- Policy: `docs/legal/world_data_and_asset_policy.md`,
  `docs/testing/world_pipeline_layers.md`,
  `AGENTS.md` (project + universal: `skills/.shared/engineering-change-principles.md`,
  `review-contract.md`, `content-rules.md`, `documentation-structure.md`),
  `.agents/skills/world-engineering/SKILL.md`.
- Related backlog: `todo/02_backlog/world/20261112-1259_world_support_demonstrated_terrain_influence.md`,
  `todo/02_backlog/world/20261112-1259_world_reduce_authenticated_preparation_cost.md`,
  `todo/02_backlog/world/20260918-1702_world_replace_vendor_instance_arrays.md`,
  `todo/02_backlog/world/fix_landscape.md`,
  `todo/00_current/00_release_3.0.0.md` (D5/D9, "No building/PCG expansion").
- Fixture: `Plugins/World/ProjectWorldTestData/Data/Fixtures/building_neighborhood/README.md`.
