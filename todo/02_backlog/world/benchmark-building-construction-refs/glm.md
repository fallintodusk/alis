# Data-driven modular building reconstruction - assessment and mock implementation plan

Replaces the disposable copy of `todo/02_backlog/world/20261112-1332_world_extend_building_assembly.md`.
Read-only investigation deliverable: no implementation, no tests run, no Unreal
execution, no project changes. Everything below marked FACT was verified against
the current checkout; PROPOSAL is a design recommendation; OPEN is an unresolved
question that needs an operator or a demonstrated consumer.

Entry point investigated: the backlog TODO "Extend the bounded building assembly
proof", its retained follow-ups, and every owner it links to. The goal assessed
is broader than the TODO itself: a maintainable approach to data-driven modular
reconstruction of buildings, including interiors, unique details, and necessary
Terrain adaptation.

---

## 0. Verdict

The bounded prototype is architecturally sound and correctly bounded: one
owner per concern, opt-in selection, refusal-first qualification, identity-based
reuse, one layer transaction, and an engine-independent planner core that host
tests execute directly. None of that should be redesigned.

The TODO's five retained follow-ups are necessary but not sufficient for the
stated goal. The missing axis is the one the standing decision record already
commits to and the backlog task never mentions: JSON building blueprints
(`Data/Buildings/`, `Data/Schemas/`) consumed with piece-catalog kits. Today a
"kit" is 9 scalars parameterizing one closed generator
(`original_box_modules_v1`, 4 part families, yaw in {0,90}); "data-driven" today
means "dimension-driven". Interiors, unique details, and facade variety cannot
be reached by scaling parameters; they need a piece catalog plus a blueprint
contract, planned in the portable core and realized through the existing ISM
and transaction machinery.

Measured evidence (FACT, section 3) sets the priorities: ~62% of Kazan buildings
are single-volume 4-corner shapes, ~48% of true rectangles sit within the
current 0.5 m support-span bound, 99.8% of building heights are inferred or
defaulted (not surveyed), and 5.6% of buildings cross cells. Consequence:
(a) footprint decomposition and a stepped/plinth foundation policy cover far
more real buildings than Terrain adaptation does; (b) Terrain adaptation stays
the last resort, routed through canonical correction exactly as the accepted
contract and the sibling influence TODO already require; (c) vertical fidelity
claims must stay honest - this is stylized reconstruction over approximate
envelopes, not survey reconstruction.

Recommended shape, in dependency order:

1. Slice A - execute the TODO as written on one bounded real neighborhood, led
   by a canonical distribution evidence pack that turns the TODO's "select only
   demonstrated follow-ups" into numbers.
2. Slice B - planner v2: piece-catalog kit (v2) + blueprint (v1) contracts and a
   polygon-footprint decomposition in the portable core; native realization
   generalized to per-piece ISM groups with content-addressed shared packages.
3. Slice C - interiors as assembly-owned shell geometry (partitions, stairs,
   openings) driven by blueprint rules; gameplay props/loot stay in the gameplay
   placement layer via a feature-anchored placement kind (separate owner).
4. Slice D - unique details via identity-seeded deterministic variation plus
   bounded per-feature authored overrides in selection v2; hero buildings remain
   authored overlays.
5. Slice E - Terrain adaptation only as the demonstrated consumer of the sibling
   influence task: selection emits a bounded support intent, canonical
   correction produces the final surface, qualification consumes it. No second
   Terrain writer, ever.

---

## 1. Evidence basis

Read (current checkout): the backlog TODO and its two sibling Terrain TODOs;
`Plugins/World/ProjectBuildingAssembly` (all docs, all source, all tests,
kit data and schema); `Plugins/World/ProjectWorld` territory contract
(layered regeneration, terrain/feature separation, authority split, anchor
semantics, manifest lifecycle, proof split), territory_generation router,
pitfalls; `ProjectWorldBuildingEditor` module (producer, selection,
qualification, persistence, massing builder, realization); canonical bundle
types and terrain surface API; `tools/World/CanonicalCompilation` building
adapter and authored-overlay contract; `docs/testing/world_pipeline_layers.md`;
`docs/agents/canonical.md` sections 7 and 10; realization/validation profiles;
`docs/legal/world_data_and_asset_policy.md`.

Measured (read-only Python over the promoted canonical bundle
`Plugins/World/ProjectWorldData/Data/Canonical/kazan_territory_v1/bundles/75b8e...zip`,
authority `kazan_territory_v1:75b8eefd...`): building footprint shape, volume,
height-basis, cross-cell, and terrain-span distributions reported in section 3.
The span numbers use bilinear corner+center sampling of the 30 m terrain core
grid, not the production triangle-field intersection; they are diagnostic
evidence for planning, not acceptance evidence.

Not done: no builds, no Unreal execution, no test runs, no data changes.

---

## 2. Verified current state (FACT)

### 2.1 Data flow end to end

```
OSM (Geofabrik) polygons
  -> SourceIngestion -> CanonicalCompilation
       building attrs: height | levels*3.0 | class default (house 6 / industrial 8
                       / apartments 12 / else 9); min_height analog; class; levels
                       (app/adapters.py::_building_attributes)
  -> canonical bundle: FProjectWorldCanonicalFeature with BuildingVolumes
       (VolumeId, SourceFeatureId, polygons+holes, MinHeightMeters, HeightMeters)
  -> realization layer "buildings": project_building_massing:v2
       (ProjectWorldBuildingLayerProducer.cpp; depends_on = {terrain}, halo 0,
        dirty unit = canonical cell)
       massing: classification (duplicate/contained/conflict/malformed/authored
       mask) -> one Nanite StaticMesh + one external AStaticMeshActor per cell
       (ProjectWorldBuildingRealization.cpp)
       selection: optional assembly_source doc -> qualified assemblies replace
       their blockout in the same layer transaction
```

Canonical building features carry `effective_volumes` with
`volume_selection` = outline_no_parts | complete_parts |
outline_fallback_incomplete_parts, `logical_building_id`, and
`intersecting_cell_ids` (cross-cell evidence).

### 2.2 Selection contract (today)

`project_world_building_selection.schema.json`: closed v1 document, at most 5
records of `{feature_id, kit_source}`; kit paths confined to
`Plugins/World/ProjectBuildingAssembly/Data/Kits/` or (TestData only)
`tmp/world/inputs/`. Setting `assembly_source` on the buildings layer is
opt-in; omission keeps massing-only. An empty configured selection still
checks its planned-input identity.

### 2.3 Qualification (ProjectWorldBuildingAssembly.cpp::Prepare)

Refuses anything except: building class feature, exactly one ordinary owned
volume, no civil grade, no constructed surface, no authored overlay anchor on
the feature, MinHeight == 0, one polygon without holes, 4-corner ring, all
corners strictly inside one cell, rectangle within tolerance (opposite-corner
fit error <= Inset/2, Inset = max(0.03, 2*CoordinateQuantizationMeters)),
inscribed rectangle contained in the original ring, no water-bounds overlap,
complete terrain-triangle coverage of the fitted rectangle (area equality),
SupportSpan = max-min terrain Z over the footprint <= kit
MaximumSupportSpan (0.5 m in the original kit). Transform is anchored at the
maximum terrain Z corner.

### 2.4 Planner (ProjectBuildingAssemblyPlan.cpp, portable C++20)

Floors = round(Height/FloorHeight); perimeter divided into PanelWidth panels;
window every WindowEvery-th panel; first ground-level bay on side 0 is an
entrance gap; floor slabs per story plus a foundation block spanning from
-SupportSpan-FoundationEmbed; roof slab. Strict bounded-dimension validation;
refusal leaves no partial plan. Instance budget <= 65536.

### 2.5 Native realization (ProjectBuildingAssemblyRealization.cpp)

Editor-only module. Four UInstancedStaticMeshComponent per actor (Part0..3),
one per family. Procedural meshes built from FMeshDescription boxes (window =
open frame with FrameFraction members). GeometryIdentity = SHA-1 over feature
id, transform, kit frame fraction, every instance, and every part triangle
key. `Matches` revalidates tags, transforms, materials, collision config,
instance arrays, and triangle-by-triangle mesh equality. `Apply` stages a new
actor, verifies it, then either updates the previous external actor in place
(keeping GUID and components) or destroys the previous one; unowned
feature-associated actors and duplicate ownership refuse.

### 2.6 Selection persistence and transaction (ProjectWorldBuildingSelectionPersistence.cpp)

All selections qualify before any asset creation or massing suppression.
Shared kit package per kit identity under the layer artifact root
(`Kits/SK_<identity>`, 4 meshes, RF_Public|Standalone), verified via
ModuleMatches before use. Stable actor GUID from grid id + feature id.
External spatially-loaded actor with runtime tags (BuildingMassing.v2,
BuildingCell, Cell, Grid; static mobility; no HLOD; no navigation). Kit-only
and actor packages are manifest artifacts; deselection retires actor and
then-referenced unused kit packages with loader reset and file deletion.
Per-cell input identity = massing cell hash + per selected feature: feature
id + kit file SHA-256 + GeometryIdentity. Capture re-derives the plan,
re-checks identity, and line-traces the roof for physical support.
Material-only adoption for modular ISMs refuses before mutation.

### 2.7 Tests (current envelope)

- Host: `Tests/run.py` compiles the production layout core with a host C++20
  compiler; `plan_test.cpp` proves the captured Manhattan rectangle
  (15.77 x 5.31 x 7.5 m, way/250440683), determinism, four families, bounds,
  story/slab contacts, kit-change regeneration, six refusal cases. Seconds,
  no engine.
- Native: `Project.World.BuildingAssembly.CanonicalPrototype` via
  prepare_case.py + the serialized safe lane; checks canonical authentication,
  classification, suppression, kit-local regeneration, no-op validation,
  drift, unowned-actor refusal, authored protection, water width, strict kit
  types, physical support, save/reload, actual rendering, signed volume and
  outward normals.
- Registered lifecycle: prepare_lifecycle.py + run_lifecycle.ps1 on the
  7-building 2-cell synthetic fixture; legs: no-op, kit-only change, failed
  apply after actor save, three buildings sharing four meshes, cold
  reconstruction, selection removal; resume flags for cold-boundary and
  removal failures; test_lifecycle_inputs.ps1 covers six input-admission
  paths.
- Settings/inputs: `Project.World.BuildingAssembly.SelectionSettings`,
  `Project.World.BuildingAssembly.SelectionInputs`
  (PROJECT_BUILDING_SELECTION_CASE).

### 2.8 Terrain authority and influence (contract)

Final canonical Terrain is the single ground authority
(territory_contract.md#terrain-and-feature-separation). Assembly performs no
Terrain edits; it reports a bounded support refusal. The accepted extension
direction is object-driven, bounded Terrain adaptation through canonical
correction; the authored overlay contract v2 currently supports radial
terrain_patches (center, radius, delta, surface weights) plus
feature_overrides, applied by the surface finalizer after hydro-conditioning,
rejecting Water-support-breaking adjustments. The influence backlog task
(20261112-1259) owns the request/response seam and requires a demonstrated
consumer. Terrain vertical source for Kazan: Copernicus GLO30, 4.0 m accuracy
at 90% confidence, 30 m samples, 0.1 m quantization.

### 2.9 Scale (validation profile receipts)

Kazan production: 210 canonical cells, 31,927 candidate building fragments,
31,764 accepted, 171 massing cell actors, 665,006 triangles. Manhattan
showcase: 113,938 candidates, 141 cell actors, 3,040,188 triangles. Assembly
actors in production today: 0.

### 2.10 Standing decision record (ProjectBuildingAssembly/docs/decision_record.md)

Commits to: fully procedural modular assembly (not monolithic meshes); JSON
building blueprints in `Data/Buildings/` + `Data/Schemas/`; reusable mesh kits
in `Data/Kits/`; C++ assembly; reference photography drives blueprint
authoring, not asset sculpting. Owed before first production use: production
selection through the existing profile/manifest/recovery route; kit and
footprint qualification at intended fidelity; deterministic I/O and
regeneration evidence wired into the world validation route plus the required
admission matrix.

---

## 3. Measured Kazan evidence (FACT, diagnostic)

Promoted canonical bundle kazan_territory_v1, 30,191 building features:

| Property | Value |
|---|---|
| Single volume | 29,960 (99.2%); 2+ volumes: 231 |
| Outer ring = 4 corners | 18,971 (62.8%); single-volume grounded unholed 4-corner: 18,759 (62.1%) |
| True rectangle within 3 cm (of those 4-corner) | 13,685 (13,201 fully single-cell, ~484 cross-cell); 5,074 are trapezoids/parallelograms |
| Holes (courtyards) | 69 polygons |
| Multi-polygon volumes | 42 |
| Volumes with min_height > 0 | 46 |
| Cross-cell buildings (intersecting cells > 1) | 1,681 (5.6%) |
| Height basis | source-derived 74 (0.2%), inferred_from_levels 13,294 (44%), procedural_default 16,823 (56%) |
| Height | median 9 m, p90 15 m, max 150 m |
| Terrain span over true-rectangle single-cell buildings (13,201 sampled, bilinear proxy) | <= 0.5 m: 48.3%; <= 1.0 m: 72.2%; <= 2.0 m: 89.6%; p99 4.98 m |

Reading: roughly one third of Kazan buildings fail the current footprint shape
rule before terrain is even sampled; among true rectangles, about half fail the
0.5 m support-span bound. A 2.0 m foundation-adaptation policy would cover
~90% of true rectangles without touching Terrain. Height evidence is 99.8%
non-surveyed: reconstruction fidelity is bounded by source envelopes, and the
blueprint must own story heights and massing style rather than pretend survey
precision.

---

## 4. Critical assessment

### 4.1 What is sound and must be kept

- Authority split: World owns geography, selection, transaction; Assembly owns
  planning and realization; no Assembly->World dependency; no Terrain writes.
- Opt-in, explicitly enumerated selection with refusal-first qualification and
  no partial plans.
- Identity-based reuse with actual-reference validation; staged replacement;
  external-actor in-place updates inside the caller's transaction.
- One buildings-layer transaction owning suppression, shared kits, and actors
  together; manifest-owned artifacts; deterministic per-cell identity.
- Portable planner core executed by host tests; native tests reserved for
  realization properties (save/reload, rendering, signed volume, normals).
- Closed schemas with strict types, bounded sizes, and provenance fields.
- Editor-only module; shipped actors are plain ISM actors with no runtime
  assembly dependency.
- Refusal of material-only adoption for modular ISMs (prevents presentation
  workarounds becoming geometry debt).

### 4.2 Weak assumptions and gaps (challenged)

- W1 - "Kit" is parameters, not modularity. The kit contract admits exactly one
  generator with 9 scalars; there is no piece catalog, no per-piece material
  role, no sockets/snapping. The TODO's follow-ups (probe replacement, kit
  admission) improve the existing seam but cannot deliver interiors or facade
  variety. The decision record's blueprint axis is absent from the TODO.
- W2 - Rectangle-only qualification excludes ~38% of Kazan by footprint/volume class
  (non-4-corner rings, holes, multivolume, multipolygon, elevated), and among the
  18,759 4-corner quads, 5,074 (27%) are non-rectangular within a 3 cm proxy test
  while the production fit tolerance (Inset/2, Inset >= 0.03 m) is tighter still,
  so at least that share of quads is refused as "unsupported footprint". These are
  ordinary OSM drawing shapes, not exotic buildings. Footprint decomposition is
  named "requires its own bounded work" in structure.md but no task owns it. This
  is the single largest coverage gap.
- W3 - Support span 0.5 m vs measured distribution: ~52% of true rectangles
  exceed it. The TODO treats "Terrain span" as a regression to add "when a
  chosen real feature requires it"; the data says most chosen features will
  require it. A planner-side foundation policy (plinth / stepped foundation /
  buried basement within a bounded span budget) is the proportionate answer;
  Terrain flattening is not (see W9).
- W4 - Height evidence is 99.8% inferred/default. Any "reconstruction" framing
  that implies surveyed fidelity is wrong; blueprints must carry explicit
  height-basis handling (e.g., accept default heights but record basis in
  identity metadata; refuse assembly for buildings where the operator demands
  source-derived heights).
- W5 - Cross-cell assemblies are refused while massing supports them by per-cell
  clipping. 1,681 Kazan buildings cross cells. The one-actor-per-feature model
  with a spatially-loaded external actor can represent a cross-cell building
  (World Partition loads by bounds), but dirty locality and the
  duplicate-ownership check are per-feature today; a fragment model would
  change identity semantics. Needs a decision when a selected neighborhood
  actually contains one (W-OPEN-3).
- W6 - Kit admission probe (10 x 10 x 6 m) is target-coupled (TODO already
  flags it). Deeper issue for asset kits: ModuleMatches regenerates expected
  geometry and compares triangles; that works for procedural pieces but cannot
  scale to admitted art meshes. Asset kits need content-addressed identity
  (source hash + parameters) with admission-time geometric checks (signed
  volume, normals, dimensions) and reference-based reuse validation.
- W7 - Interiors have no owner today. The conflation risk is real: interior
  shell geometry (partitions, stairs, openings) is building geometry;
  furniture/loot is gameplay content. Assembly must not grow an object/placement
  subsystem, and gameplay placement must not tessellate walls. The existing
  gameplay placement schema is terrain-anchored (cell + easting/northing +
  surface_offset); it cannot express "third floor, room 2" today.
- W8 - Navigation: buildings settings pin no_navigation and massing is solid.
  Interiors that are meant to be walkable change the admitted use (world
  pipeline layers: required playable support has stronger obligations). The
  navigation policy for interiors is undecided and must not be silently
  inherited.
- W9 - Terrain adaptation ordering. The contract and sibling TODO are right
  that canonical correction is the only writer. But note the iteration cost:
  selection-driven patches make the selection document an input to canonical
  compilation, so selection changes trigger canonical authority transitions
  (measured stage times in the preparation-cost TODO: tens to hundreds of
  seconds). Prefer geometry adaptation first; reserve Terrain adaptation for
  demonstrated residuals (entrance grade mismatch, burying, retaining needs).
- W10 - Facade/material variety: one material slot ("Module") per mesh, one
  presentation role ("building"). Fine for the prototype; blocks facade
  variety later. Kit v2 should name per-piece material roles resolved through
  the presentation profile, not direct references (keeps material authority in
  ProjectMaterial/presentation and preserves the material-only rollout
  boundary).
- W11 - Reclassification cost: HashCell re-runs Prepare (plan + massing
  exclusion) per selected building per cell; a selection change dirties the
  cell and rebuilds the remaining massing mesh even when its identity is
  unchanged (kit-only leg proves the no-op validation path). Acceptable at
  five buildings; measure before growing (TODO bullet 5 already owns this).
- W12 - Selection cap of 5 is fine for a proof; a real neighborhood needs a
  bounded raise, and possibly a rule-based selector later. A rule-based
  selector moves selection policy into World and risks unbounded assembly
  growth; keep explicit enumeration until a demonstrated need (W-OPEN-5).
- W13 - Actor/component scale: 4 ISMC per building actor is fine for a
  neighborhood; at city scale (30k buildings -> 120k components) it would not
  be. Per-cell consolidation is a later, separately-admitted change; do not
  design it now (the Manhattan Shipping watch item exists for unrelated
  reasons but reminds us to measure, not assume).

### 4.3 Non-problems (look wrong, are right)

- Empty configured selection still validates planned inputs - keeps stale
  selection docs from silently passing.
- Qualification cost (full plan build during HashCell) - buys identity that
  makes no-op reuse trustworthy; optimize only on measured need.
- One shared kit package per kit identity - correct deduplication unit.
- Strict closed JSON (additionalProperties false, exact field counts) -
  deliberate admission contract, not parser fragility.

---

## 5. Recommended architecture (PROPOSAL)

### 5.1 Contracts

Three closed, versioned, provenance-bearing JSON contracts, owned by
ProjectBuildingAssembly under `Data/Schemas/` (kit, blueprint) and by
ProjectWorld (selection schema, because selection is the World-side admission
document). All keep: `$schema` first, strict types, bounded sizes, closed
fields, provenance, deterministic identity participation.

Sketch - `modular-kit.schema.json` v2 (piece catalog; superset of v1 which
remains valid for the original kit):

```
{
  "$schema": ".../modular-kit.schema.json",
  "schema_version": 2,
  "kit_id": "kazan_panel_v1",
  "generator": "piece_catalog_v1",
  "provenance": { "kind": "original_procedural_test_geometry" | "admitted_art_kit",
                  "owner": "project", "receipt": "<path>" },
  "units": { "panel_width_m": 2.0, "floor_height_m": 3.0, ... },
  "pieces": [
    { "piece_id": "wall_panel",
      "family": "wall",                      // wall|window|floor|roof|interior|stair|detail|foundation
      "source": { "procedural": "box" } | { "asset": "<content path>", "sha256": "..." },
      "bounds_m": [2.0, 0.18, 3.0],
      "snap": "wall_run",                    // placement class consumed by the planner
      "material_role": "building.wall",      // resolved by presentation profile
      "collision": "complex_as_simple" },
    ...
  ],
  "limits": { "maximum_support_span_m": 2.0, "maximum_instances": 65536,
              "maximum_pieces_per_building": 64 }
}
```

Sketch - `building-blueprint.schema.json` v1 (per building or per archetype;
referenced by selection):

```
{
  "$schema": ".../building-blueprint.schema.json",
  "schema_version": 1,
  "blueprint_id": "kazan_walkup_v1",
  "kit_id": "kazan_panel_v1",
  "provenance": { "kind": "authored_blueprint", "owner": "project",
                  "photography_refs": [...] },
  "stories": { "policy": "fit_height", "story_height_m": 3.0,
               "min_stories": 1, "max_stories": 12 },
  "facade": { "window_pattern": "every_n", "n": 2, "corner_pieces": true },
  "foundation": { "policy": "plinth", "max_span_m": 2.0, "embed_m": 0.1 },
  "interior": { "enabled": true, "partition_rule": "perimeter_ring_rooms",
                "stair_rule": "center_core", "door_openings": true,
                "max_partitions_per_story": 8 },
  "details": [ { "piece_id": "balcony", "rule": "per_story_edge",
                 "probability": 0.5, "seed": "feature" } ],
  "height_basis_policy": "accept_inferred_or_default"
}
```

Sketch - selection v2 (World-owned schema; bounded superset of v1):

```
{
  "$schema": ".../project_world_building_selection.schema.json",
  "schema_version": 2,
  "assemblies": [
    { "feature_id": "alis:osm:way:...",
      "kit_source": "Plugins/World/ProjectBuildingAssembly/Data/Kits/kazan_panel_v1.json",
      "blueprint_source": "BuildingAssembly/Blueprints/kazan_walkup_v1.json",   // optional, defaults per kit
      "overrides": { "facade.n": 3 },                                          // bounded scalar overrides, provenance-tagged
      "support_policy": "plinth_2m" | "terrain_intent:<patch_ref>" }
  ]
}
```

### 5.2 Module decomposition (owners and files)

```
ProjectBuildingAssemblyEditor (editor-only, no World dependency)
  ProjectBuildingAssemblyPlan.h/.cpp          (v1 rectangle planner; unchanged until B)
  + ProjectBuildingAssemblyKit2.h/.cpp        (piece-catalog reader/admission; target-independent)
  + ProjectBuildingAssemblyBlueprint.h/.cpp   (blueprint reader + override merge)
  + ProjectBuildingAssemblyDecompose.h/.cpp   (polygon footprint -> wall runs/corners/holes;
                                               multivolume; cross-cell partitioning decision input)
  + ProjectBuildingAssemblyInterior.h/.cpp    (story partitions, stairs, openings from blueprint)
  + ProjectBuildingAssemblyDetails.h/.cpp     (seeded deterministic variation; bounds)
  ProjectBuildingAssemblyRealization.h/.cpp   (generalized: ISMC group per piece id,
                                               content-addressed kit packages; keep staged replacement)
ProjectWorldBuildingEditor
  ProjectWorldBuildingSelection.*             (v2 record: kit + blueprint + overrides + support policy)
  ProjectWorldBuildingAssembly.cpp            (qualification: shape class dispatch, water, support;
                                               rectangle-fit logic retires when Decompose lands)
```

File-size guardrail: every new concern is a new sibling file; the existing
realization file (218 LOC) grows only by the per-piece grouping generalization.
The planner stays standard C++20 so the host lane keeps executing production
code.

### 5.3 Terrain adaptation route (paired with the influence TODO)

```
selection v2 support_policy = terrain_intent
  -> World adapter derives a bounded support request from canonical feature
     geometry + terrain sample (footprint polygon, target plane, max cut/fill,
     provenance authored_operator)                    [building slice owns]
  -> canonical authored-overlay extension: polygonal flatten patch op with
     bounded delta, hydro-check integration           [influence task owns]
  -> canonical recompile -> final surface             [CanonicalCompilation]
  -> terrain layer dirty cells + dependants (buildings, water, roads,
     vegetation) via existing DAG                     [existing machinery]
  -> assembly qualifies against the corrected final surface (single writer,
     single read)                                     [existing machinery]
```

The selection document that drives an intent becomes an admitted canonical
input (like today's authored overlay) so canonical identity changes are
explicit and hashed. Batch selection edits to amortize canonical transitions.
Precondition remains the influence TODO's: demonstrated residual that geometry
adaptation (plinth/stepped foundation, W3) cannot serve.

### 5.4 Interiors split

- Assembly owns interior shell geometry only: partitions, stair flights,
  door/window openings, per-story slabs (already exist). Realized as additional
  piece families in the same actor/transaction; collision policy per piece;
  navigation policy decided per W-OPEN-4 before any walkable claim.
- Gameplay props/loot belong to the gameplay placement layer. Extend
  `project_world_gameplay_placement.schema.json` with a feature-anchored
  placement kind (feature_id + expected class + building-local frame: story,
  local XY, yaw + tolerance), reusing authored-anchor semantics (fail closed on
  feature disappearance/movement). Separate slice, separate owner, only when a
  gameplay consumer demonstrates the need.

### 5.5 Unique details - three tiers

1. Deterministic variation: seed derived from feature id + blueprint id
   (already implicit in identity); details rules bounded by probability and
   count caps; identical inputs -> identical output (host-testable).
2. Authored per-feature overrides: bounded scalar/enum overrides in selection
   v2 with provenance; participate in identity.
3. Hero buildings: unchanged authored overlays, protected, never forced
   through the generator.

### 5.6 Deliberately not built

No PCG graph engine or shape-grammar frontend (structure.md already scopes
this as a future qualified option; requires a consumer proof under Native
Engine First). No runtime assembly code. No second Terrain writer. No
rule-based selection engine. No general kit/asset cache or scheduler. No
CoreRedirects or legacy compatibility scaffolding (public repo migration
policy). No interior prop ownership in Assembly.

---

## 6. Mock implementation plan (PROPOSAL)

Layer references: L0 edit / L1 fixture / L2 slice / L3 promote / L4 release
per docs/testing/world_pipeline_layers.md. Every slice keeps the dev-loop
contract (exact test filters), receipts under tmp/world/..., and refusal
no-mutation proofs.

### Slice 0 - Canonical distribution evidence pack (L0, read-only)

Goal: replace assumptions W2/W3/W5 with per-neighborhood numbers before any
code changes.

- Script (new, `Plugins/World/ProjectBuildingAssembly/Tests/analyze_neighborhood.py`,
  pinned World Python environment): input = canonical bundle + optional
  feature-id list; output = JSON + markdown receipt under
  `tmp/world/building_assembly/neighborhood/<id>/`: shape classes (rect /
  non-rect 4-corner / N-corner / holed / multivolume / multipolygon /
  min_height>0 / cross-cell), height-basis histogram, terrain-span histogram
  (triangle-based, reusing the qualification sampling approach), water-overlap
  counts.
- Acceptance: receipt reproduces from a clean checkout against the promoted
  bundle; numbers for the chosen neighborhood decide which A2 regressions are
  "demonstrated necessary".
- No production code changes.

### Slice A - Execute the TODO on one bounded real neighborhood

- A0 (operator decision): choose the neighborhood (a handful of contiguous
  Kazan cells with mixed classes). Run Slice 0 on it.
- A1 - Target-independent kit admission (TODO bullet 2):
  - Replace the 10 x 10 x 6 probe in ReadKit with structural admission:
    parameter ranges + a minimal synthetic build proving every family the kit
    declares can emit at least one instance; per-selection qualification
    (already calls Build with the real input) stays the dimension gate.
  - Keep strict JSON types and provenance checks. Host test: extend plan_test
    or add kit_admission_test.cpp with a kit that only fits a large building
    (must pass admission, refuse only at selection).
- A2 - Faithful regressions for demonstrated unsupported cases (TODO bullet 1):
  one RED fixture per class the neighborhood actually contains, each pinned to
  a real feature id from the promoted bundle:
  - non-rectangular 4-corner ring -> today: refusal with reason; expected
    outcome after B: decomposition. The regression pins the refusal text and
    no-partial-plan behavior now.
  - cross-cell and/or multivolume -> refusal regression (no-mutation proof:
    massing unchanged, no actor, no kit package written).
  - Terrain span beyond kit bound -> refusal regression; drives the
    foundation-policy decision in B (or E if plinth is insufficient).
- A3 - Bounded selection growth: raise the selection schema maxItems to the
  neighborhood size (still explicit enumeration, still closed); qualification
  unchanged. SelectionInputs/SelectionSettings tests updated with a v1 doc at
  the new bound.
- A4 - Measure reclassification/reuse (TODO bullet 5): lifecycle receipts
  (already retain dirty units, inventories, timings) extended with per-cell
  prepare/apply timings for the neighborhood; report changed/unchanged outputs;
  decide whether any reuse optimization is justified. No speculative cache.
- A5 - Material-reference-only adoption (TODO bullet 4): keep refusing; record
  the decision and the triggering consumer condition in the decision record.
- Slice exit (L2): common Check + buildings-layer Matrix legs for the affected
  territory profile; admission matrix extended for the buildings layer with
  assemblies enabled (per proof-split: selected-feature assembly stage extends
  the matrix before first territory use). L3 (enrollment of production
  assemblies) is a separate operator gate.

### Slice B - Planner v2: piece-catalog kits, blueprints, polygon decomposition

- B0 - Contracts first (L0): schemas above + reader tests (host lane) with
  malformed/oversized/unknown-field refusals; identity rules documented (kit
  content hash, blueprint hash, overrides in GeometryIdentity v2 string).
- B1 - Decomposition (portable core, host tests):
  - ring -> wall runs along edges (panel fitting with cut/extend within piece
    tolerance), corner pieces, hole rings (courtyard inner walls);
  - non-rect 4-corner rings decomposed as general polygons (kills the
    inscribed-rectangle special case; the World adapter passes the qualified
    polygon through instead of fitting a rectangle);
  - multivolume: per-volume stories; shared/overlapping volumes refuse
    (bounded first pass; no boolean union);
  - cross-cell: whole-building actor owned by owner cell, spatially loaded
    (decision W-OPEN-3); identity records all intersecting cells so the
    buildings-layer dirty mapping can fan out (adapter-side).
  - Host tests: synthetic polygons + one source-faithful neighborhood feature
    per class; determinism; capacity refusals; containment within the
    qualified volume.
- B2 - Native realization generalization:
  - ISMC per piece id (bounded: max components per actor; refuse beyond),
    keeping the staged-replacement and Matches discipline generalized to
    piece groups;
  - kit packages content-addressed per piece set (shared across buildings);
    ModuleMatches for procedural pieces; content-hash + admission-time checks
    for asset pieces (W6);
  - geometry identity v2 (versioned string) covering kit hash + blueprint
    hash + overrides + instances.
- B3 - Foundation policy (W3): plinth / stepped foundation pieces for spans up
  to the kit bound (2.0 m proposed); explicit refusal beyond; measured span
  histogram from Slice 0 sets the bound. Host tests: stepped foundation
  geometry contacts terrain extrema exactly like today's foundation block.
- B4 - Qualification move: ProjectWorldBuildingAssembly.cpp dispatches on
  shape class (rect fast path kept); passes polygon(s) to the core; rectangle
  fitting code retired. Refusal set unchanged except shape classes now
  supported.
- Slice exit (L1/L2): native focused tests (new
  `Project.World.BuildingAssembly.PolygonPrototype` with a prepared real case),
  lifecycle legs extended (kit-with-multiple-pieces change, blueprint change),
  Matrix legs for the buildings layer, admission matrix update.

### Slice C - Interiors (shell geometry)

- C0 - Scope decision (W-OPEN-4): interior shells only; navigation policy
  explicit (recommend: keep no_navigation until an AI/gameplay consumer
  requires otherwise; walkable floors still get collision, so the admitted-use
  table applies to any claim of traversability).
- C1 - Blueprint interior rules + planner (host tests): partitions per story
  (bounded count), stair placement, door openings aligned with the entrance
  bay; capacity and overlap refusals; identical determinism requirements.
- C2 - Native: interior piece families in the same actor and transaction;
  per-piece collision config; capture checks extended (interior floor
  line-trace support, like today's roof trace).
- C3 - Gameplay seam (separate slice, only on demonstrated consumer):
  feature-anchored placement kind in the gameplay placement schema; placement
  producer resolves against the building feature and its canonical frame;
  never against the generated actor (authored-anchor semantics).
- Slice exit: same envelope as B; plus an explicit admitted-fidelity statement
  (contextual shell vs. required playable interior) in the decision record.

### Slice D - Unique details

- D1 - Seeded variation rules in blueprint details (bounded probabilities and
  caps; seed = feature id + blueprint id); host tests prove determinism and
  cap enforcement.
- D2 - Selection v2 overrides (bounded scalar/enum set with provenance);
  identity participation; SelectionInputs test gains an override-change leg.
- Hero overlays: no change.

### Slice E - Terrain adaptation consumer (paired with influence TODO 20261112-1259)

- E1 - Demonstrated consumer: a selected building whose residual cannot be
  served by plinth (e.g., entrance grade mismatch > bound, or required burying);
  RED fixture first (canonical + selection + expected refusal), per the
  influence TODO's trigger contract.
- E2 - Support-intent derivation in the World adapter (footprint polygon,
  target plane, bounded cut/fill, provenance authored_operator) and the
  canonical polygonal-flatten patch op in CanonicalCompilation (influence task
  owns the op; building slice supplies the consumer and its acceptance).
- E3 - Identity/dirty wiring: selection doc (or a derived intent document)
  admitted as canonical input; terrain cells dirty -> dependants via existing
  DAG; lifecycle proof: intent change -> terrain + buildings regenerate,
  unrelated cells unchanged, refusal leaves state untouched, water support
  rejection refuses the patch (existing hydro check).
- Slice exit: influence TODO's acceptance list verbatim (deterministic
  reconstruction, refusal/no mutation, unaffected neighbors, exact dirty
  scope, measured local execution, rechecked affected consumers).

### Sequencing and stop conditions

```
Slice 0 -> A (neighborhood proof; produces evidence + regressions)
        -> B (data-driven core; largest slice; contracts first)
        -> C (interiors) and D (details) in either order, after B
        -> E only on demonstrated residual (may trigger inside A/B when a
             selected building needs it)
```

Stop conditions (return to architecture, do not force through): a slice needs
to edit an owner declared UNTOUCHED (e.g., planner needing World internals);
identity semantics break reuse guarantees; a proof shows the acceptance
surface is not the real execution envelope; cross-cell fragment semantics
(W-OPEN-3) cannot preserve the one-actor-per-feature ownership invariant.

---

## 7. Traceability to the original TODO

| TODO retained follow-up | Covered by |
|---|---|
| One faithful regression for unsupported footprint / cross-cell / multivolume / Terrain span | A2 (refusal regressions now), B1 (support later), E3 (span via intent) |
| Target-independent kit admission; strict types; qualify against actual input | A1; B0 piece-catalog admission extends it |
| Bounded real neighborhood; qualify actual footprints/support before production admission | A0/A3; Slice 0 evidence pack |
| Material-reference-only adoption for modular ISMs | A5 (keep refusing); W10 (future per-piece material roles through presentation profile) |
| Measure reclassification/massing before reuse; shared kit package ownership; no speculative cache | A4; W11 |
| (Decision record) JSON blueprints, kits, C++ assembly, photography-driven authoring; owed admission items | Slice B contracts; A slice exit admission matrix; section 5 |

---

## 8. Open questions (OPEN)

1. W-OPEN-1 - Neighborhood choice: which Kazan cells constitute the bounded
   real neighborhood (operator decision; mixed classes + at least one
   demonstrated terrain-span and one non-rectangular case recommended).
2. W-OPEN-2 - Foundation policy bound: plinth maximum span (proposed 2.0 m
   from the measured histogram; above that, refuse or escalate to E).
3. W-OPEN-3 - Cross-cell assemblies: whole-building actor owned by owner cell
   (recommended; identity records intersecting cells; buildings-layer dirty
   fan-out in the adapter) vs. per-cell fragments matching massing locality
   (changes one-actor-per-feature and duplicate-ownership invariants). Decide
   when the neighborhood contains one.
4. W-OPEN-4 - Interior navigation: keep no_navigation until a gameplay
   consumer requires interior navmesh (recommended), or admit walkable
   interiors with the stronger proof obligations up front.
5. W-OPEN-5 - Selection growth: explicit bounded list (recommended now) vs.
   rule-based selector admitted later with its own bounded proof.
6. W-OPEN-6 - Asset kit intake: provenance/licensing audit path and admission
   criteria for art kits (world asset policy; conditional). Blocks any
   non-procedural piece catalog use; original procedural pieces remain the
   default until then.
7. W-OPEN-7 - Height fidelity: accept inferred/default heights for assembled
   buildings with basis recorded in identity metadata (recommended), or
   restrict selection to source-derived heights (74 buildings in Kazan).
8. W-OPEN-8 - Facade material roles: whether kit v2 material_role resolution
   through the presentation profile lands with B0 contracts (recommended) or
   is deferred to the first facade-variety consumer.

---

## 9. Verification status of this document

- Verified by reading current code/docs: everything in sections 2, 4.1-4.3,
  and the contract citations. No claim in those sections relies on memory.
- Measured read-only from the promoted canonical bundle: section 3 table.
  Terrain-span figures are a bilinear corner+center proxy over the 30 m core
  grid, not the production triangle-field qualification; treat as planning
  evidence (diagnostic, non-authoritative).
- Not verified: runtime behavior, performance at neighborhood scale, actual
  qualification pass rates through the production code path, asset-kit
  licensing feasibility. These are exactly what Slices 0 and A measure first.
- All section 5/6 content is proposal, not accepted architecture; it follows
  the existing owners and extension axes and adds no parallel machinery, but
  it requires the normal planning/approval route before implementation.
