# ALIS World — extending modular building reconstruction

**Status:** architectural assessment and **mock implementation plan only**. No implementation, tests, Unreal execution, source modification, or production admission performed.

**Source baseline:** public [`fallintodusk/alis`](https://github.com/fallintodusk/alis) `main` at commit [`3f06366febd1a7987d73a91d9f3187e5a8105ef9`](https://github.com/fallintodusk/alis/commit/3f06366febd1a7987d73a91d9f3187e5a8105ef9) (mirror commit dated **2026-11-16 11:08 UTC**). This is an inspected public snapshot, **not a claim about the user's uninspected local working tree**. References at the end are pinned to this commit. The original backlog task is [R1].

## 1. Verdict and recommended decision

**PATCH — retain the existing World/Assembly split, but change what is planned before increasing procedural breadth.** The implemented rectangle/ISM prototype is a useful, bounded engineering proof. It is **not** a data-driven reconstruction system for distinct buildings, and its passing tests cannot establish that it is one. Extending its `FKit` with more flags or scattering exceptions into `ProjectWorldBuildingAssembly::Prepare` would be the wrong long-term architecture.

Recommended direction:

1. **Keep geographic truth in canonical World** (logical building IDs, exact effective volumes, height/source evidence, final Terrain, owned/referenced cells). Keep the existing World building producer responsible for selection, overlapping-building classification, blockout suppression, generated assets/actors, manifests, rollback and regeneration.
2. **Introduce an explicitly authored, versioned building blueprint as optional *territory-owned data***, selected by the existing canonical feature ID. The blueprint describes only what geographic inputs do not: facade openings/character, level semantics, room boundaries, circulation, unique placements and provenance. Assembly compiles it, plus a reusable kit, into an engine-independent **semantic plan**; World provides canonical geometry/support and realizes the accepted plan through Assembly's editor adapter.
3. **Support fidelity in stages.** Preserve massing everywhere by default; compose a plausible modular shell for a few selected buildings; admit *walkable* interiors only with explicit layout and tested access; preserve protected authored hero buildings as separate overlays. Do not label inferred interiors as surveyed reality.
4. **Treat Terrain adaptation as a separate upstream canonical selection**, not as an effect of constructing a building. First try a supported foundation, step, plinth or entrance transition against accepted final Terrain. Only a demonstrated failure may justify a separately qualified canonical patch/refinement. Final building geometry must consume the *accepted revised Terrain*, never write to the Mesh Terrain directly.
5. **Implement one real bounded case end-to-end before generalization.** Add exact capabilities demanded by that case. A small verified example involving one facade entrance and one genuinely walkable interior space is more valuable now than generic city rules, random room generation, multi-cell architecture, or a new cache.

**Decision to reject:** “Automatically recover accurate interiors and unique facades from OSM footprint + height + a procedural seed.” Those inputs do not contain such information. Randomized furnishing can produce plausible *fiction*, not reconstruction; photography commonly reveals only the exterior. Reference images, floor plans, surveys or deliberate authored decisions must be admitted as distinct inputs with source and uncertainty labels.

**Boundary:** do **not** reorganize canonical source ingestion, implement a PCG terrain writer, replace World Partition, add a general grammar language, build an asset marketplace/cache, or auto-populate the whole city in this work. Those do not solve the first missing contract.

## 2. Verified implementation and gaps

### 2.1 Actual route today

```text
Admitted provider source + provenance
              |
   CanonicalCompilation (Python)
   logical building ID + effective_volumes; final Terrain cells
              |
ProjectWorldBuildingEditor (World; editor-only)
   normalized producer/profile/manifest + authored protection
   massing admission/classification (full logical building population)
   optional selection feature_id -> kit_source [max 5]
   qualify ONE rectangular, single-cell volume + full triangle support
              |                             |
     selected: suppress blockout        unselected: massing cell mesh
              |
ProjectBuildingAssemblyEditor (editor-only; no World dependency)
   C++20 Build(FInput,FKit) -> FPlan
   four box/frame StaticMeshes -> four ISM components
              |
World registered layer transaction
   shared generated kit packages, selected OFPA actors,
   captured inventory, manifests, dirty plan, rollback/recovery
```

- **Canonical is richer than Assembly.** It already represents logical buildings with `BuildingVolumes[]`; each volume has its own geometry polygons, holes, base/top height and source-feature identity [R2, R3]. Canonical Compilation resolves provider building parts/relations and a controlled fallback [R4]. Massing can classify/clip multi-volumes across cells, while the assembly adapter refuses them [R5, R6]. There is no independent survey-level room/facade model in the inspected canonical feature type.
- **Assembly plan has only four part types:** `Wall`, `Window`, `Floor`, `Roof`. Inputs are width, depth, height, whole-foundation support span and one parameter kit. The planner rounds `height / floor_height` to infer floor count, creates one full slab per level, a full roof, regularly patterned windows and an *empty first wall bay* as its entrance; it has no door, stairs, rooms, holes in floors, usable circulation, facade evidence, or material families [R7, R8]. The test geometry is boxes and an open window frame; its native path validates actual mesh, transform, normals, collision and materials [R9].
- **Selection is presently just feature ID + kit source.** The closed selection schema caps `assemblies` at five, version 1. The reader only accepts the original `original_box_modules_v1` kit with its original-test provenance and tests it by constructing an unrelated **10 x 10 x 6 m** probe. The limits are explicitly admitted in the docs; there is no finished `Data/Buildings/` blueprint contract [R10, R11, R12].
- **World support is strict and intentionally local.** The adapter demands one owned volume, one four-corner hole-free rectangle strictly inside one canonical cell, no explicit civil grade or constructed surface, no authored feature anchor, and no unresolved Water bounds collision. It computes an inscribed rectangle, intersects its area with final physical Terrain triangles, requires full support and obtains min/max Z. Unsupported or excessive span refuses before suppression [R5]. This is not a graded foundation or a guarantee of a usable threshold at the entrance.
- **The massing exclusion ordering is correct and important.** Full overlap/duplicate/containment/authored-mask classification happens *before* selected massing is suppressed, so selecting a building cannot change unrelated classification [R6]. Keep that invariant.
- **Persistence is genuinely integrated, but only at prototype scope.** Selection hashes and qualifies; the World producer owns shared generated mesh packages and feature-stable external-actor GUIDs, reuses verified existing actors, reconstructs missing actors even after input-no-op, captures actual instance/mesh/material state, and retires deselected actors and unreferenced kit packages [R13, R14]. Current generated assemblies have no navigation contribution and no HLOD layer. The module-material-only adoption route deliberately refuses modular ISMs [R14, R15].
- **Registered tests are meaningful but not a city certificate.** Host/native proof uses a captured Manhattan footprint, and registered lifecycle uses **seven synthetic buildings over two cells**, selecting one then three, proving no-op, kit change, failure recovery, cold reconstruction and removal. They do not prove an imported real neighborhood, detailed exteriors, interiors, cross-cell assemblies or packaged playable traversal [R16, R17].
- **Production currently takes the massing-only route.** The inspected Kazan and Manhattan production realization profiles declare `project_building_massing` v2 and **do not** set `assembly_source`; no production blueprint or selection directory appears in the inspected `ProjectWorldData/Data/` tree [R18, R19]. The selected prototype is not already a production building feature.
- **Terrain remains a separate immutable input to realization.** It is produced by CanonicalCompilation; Mesh Terrain, Water, Roads and Buildings are separate consumers. The source currently includes Copernicus GLO-30 **DSM**, not surveyed bare-earth Ground: a returned support height can represent roofs, vegetation or other surface artifacts [R20, R21]. Existing authored canonical patches are bounded radial height deltas, not arbitrary per-building grading [R22].
- **Existing gameplay composition is separate.** `ProjectObject` owns placeable objects and capability behavior, including openable doors/locks [R23]. The current `project_gameplay_placement` producer's schema is surface-snapped and its native actor validation includes pickup capability [R24]. Therefore “just send a door or interior object through gameplay placement” is **not proven**; a building-local placement contract would need explicit qualification.

### 2.2 Required findings, with smallest safe correction

| Finding | Evidence | Consequence if generalized unchanged | Smallest safe design fix |
|---|---|---|---|
| **F1 — no building description** | [R7–R12] | Height-derived anonymous boxes cannot express reconstruction or interiors | Add an optional versioned *blueprint* input/reader and semantic planner; keep existing test kit v1 working |
| **F2 — false geometric certainty** | [R8], [R3], [R20] | Guessed floors/window rhythm/entrance become visually convincing but false claims | Separate source-confirmed, authored approximate and generated placeholder choices, with provenance and admission grade |
| **F3 — unsupported topologies** | [R5] vs. [R2, R6] | Cross-cell, holes and parts either refuse or invite accidental massing duplication | Preserve refusal for v1; add polygon/volume handling only after a real selected regression; retain post-classification suppression |
| **F4 — Terrain ≠ floor support** | [R5, R20–R22] | Full XY support and small min/max span do not prove an accessible doorway, bare-earth or safe grade | Explicit grade/foundation/entrance-support checks; dry foundation before canonical correction; never a second Terrain writer |
| **F5 — protected content collision** | [R5], [R25] | Current blanket refusal blocks legitimate composition; dropping it would risk overwriting authored hero content | Define explicit *non-overlap or opt-in* authored coexistence contract before touching this guard; leave existing refusal until proved |
| **F6 — gameplay ownership absent** | [R8, R9, R23, R24] | A geometry-only “door” cannot open, persist, replicate or stream correctly | Static frames in Assembly; dynamic/interactive semantics in ProjectObject or its public interface, with stable object IDs and lifecycle |
| **F7 — unsafe partial update / dirty assumptions** | [R13–R15] | Adding more packages/actors can break rollback, cold rebuild, retirement and material binding | Extend same World transaction and exact inventory; hash all consumed blueprint, kit and source identities; keep capture checking real outputs |
| **F8 — kit admission is a test shortcut** | [R11] | A valid kit for the real selected building can be rejected by the unrelated 10×10×6 probe | Separate schema/semantic kit validation from target-dependent fit; qualify fit against actual target plan |
| **F9 — performance proof not general** | [R16, R26] | Unchecked I/O or one actor per decorative item could dominate iteration/cook/streaming | Measure actual preparation, assets, persistent packages, ISM counts, actor count, CPU/GPU memory, render, collision and streaming before optimization |
| **F10 — visual vs playable proof** | [R9, R16, R17] | Roof trace and saved render can pass while interior doors collide, stairs fail or nav does not connect | Add traversal, collision, visibility and packaged streaming acceptance **for walkable fidelity only** |

**Disposition:** no demonstrated defect in the *accepted bounded prototype* justifies changing current production now. Findings F1/F2/F4/F6 are **requirements for the proposed richer target**, not regressions proven in the existing scope. Do not treat the backlog list as a mandatory shopping list.

## 3. Target architecture and single-source ownership

### 3.1 Preserve the existing composition, add one missing data seam

```text
      Source/OSM/elevation              Licensed survey, photo, plan, authored decision
               |                                    |
       SourceIngestion                    Territory-owned Building Blueprint
               |                                    | (feature_id + expected geometry + provenance)
       CanonicalCompilation                           |
   (buildings + final Terrain)                        |
               |                                    |
               +-------------- World ---------------+
                              | building selection, canonical identity,
                              | effective volumes, protection, final support,
                              | massing classification and exclusion
                              v
                   ProjectBuildingAssembly
                   validated blueprint + reusable kit
                       pure C++20 semantic plan
                              |
                 World-owned realization transaction
                              |
        static shell/facade/interior geometry + authored links
                              |
                 optional separate gameplay objects
             (ProjectObject capability/runtime authority)

   **Conditional correction path, not a feedback loop:**
   read-only building support preflight -> operator-reviewed canonical Terrain
   correction/refinement -> accepted revised final Terrain -> re-preflight -> realize
```

| Concept | Proposed owner | Not its responsibility |
|---|---|---|
| Raw OSM/survey records + licenses | SourceIngestion and relevant source owner | Direct UE asset layout |
| Canonical footprint, effective volumes, ID, geometry provenance | CanonicalCompilation | Inferring room locations from OSM height |
| Final dry Ground, water conditioning, accepted Terrain corrections | CanonicalCompilation/refinement | Building mesh output; downstream procedural sculpt |
| **Building blueprint JSON** (territory-specific observations, authored choices and unique local description) | **`ProjectWorldData/Data/BuildingAssembly/Blueprints/`** or equivalent data-plugin root | Implementation logic or source geography redefinition |
| Reusable kit definition, schema and reusable module logic | `ProjectBuildingAssembly/Data/Kits/`, its schema and editor module | Choosing a real site; selecting accepted Terrain |
| Selection, canonical qualification, author protection, suppression, persistent packages, manifest/rollback | `ProjectWorldBuildingEditor`/registered buildings producer | Room-generation algorithms or kit internals |
| Pure geometric/semantic planning and native representation of modules | `ProjectBuildingAssemblyEditor` (portable core + UE adapter) | ProjectWorld internals, canonical writes, runtime state |
| Reusable material graphs/identities | ProjectMaterial (existing) | Building-specific source facts |
| Authored hero packages and protected anchors/masks | WorldData protected authored overlay | Editable generated root |
| Openable/lockable/interactive object behavior and mutable persistence | ProjectObject/ProjectCore service contracts and existing gameplay owners | Building shell generator creating its own gameplay framework |

The standing Assembly decision [R27] proposes `Data/Buildings/` as a future location. **Refine rather than copy that path blindly:** real, unique descriptions belong in the *territory data plugin* so reusable Assembly does not acquire Kazan/Manhattan content knowledge. Keep the intended JSON+kit+C++ model; document this location choice when implementation begins. If the project already has a more precise accepted owner in the private checkout, use it rather than creating a second place.

**No new registered producer for the shell.** Reuse `project_building_massing` for exclusive building geometry because it already owns the classifier, selection and replacement. A separate “interior geometry” producer would make correctness of shell/blockout replacement depend on cross-producer transaction ordering, reintroducing the competing-building geometry problem. Static interior surfaces belong in the same selected assembly; **independent gameplay objects can remain a separate capability/producer** after a stable local-anchor contract is proved.

### 3.2 Four fidelity states — not four generators

| State | Data evidence | Output and admission |
|---|---|---|
| **Massing** (default) | Existing canonical logical building/volumes | Existing blockout; no claims about windows, interior or access |
| **Modular shell** | Canonical geometry + kit + explicit facade decisions (measured, inferred or stylized) | Exterior walls, roof, openings and foundation; visual and collision-qualified, interior *not automatically usable* |
| **Walkable selected interior** | Complete bounded authored level topology and access/egress decisions | Shell + floor/ceiling holes, partitions, stair/landing or flat single-level access, door slots, required collision/nav/streaming proof |
| **Authored hero** | Protected art/experience with its own anchor/mask | Protected Authored package; **not forced into** generated Assembly; only explicitly compatible separate details may coexist |

Use **explicit per-selected-feature state**; non-selected features never become a procedural interior by global default. “Inferred” is a source-quality label, not a machine-generated floor plan that masquerades as measured truth. A facade can be historically faithful while an interior is fictional; label these independently.

### 3.3 Geometry is data; planner is rules; generated instances are output

- **Blueprint** decides: exact selected feature, expected source topology, which levels/rooms are represented, facade entrance/window positions, roof/detail features, their references and uncertainties. Distinguish required, approximate and decorative records.
- **Kit** decides: available module families, their physical dimensions, pivots/snap axes, allowable adjustment ranges, material slots, collision and license policy, and optional mesh references. Avoid storing arbitrary Unreal transforms/absolute geographic points in reusable kits.
- **Planner** decides: valid fit, seam joints, openings, wall/slab intersections, room adjacency, clearance and instance budget. Produce a deterministic intermediate plan with *stable semantic element IDs*, not just anonymous `EPart` instances.
- **World adapter** decides: canonical-to-local transform, accepted Terrain support, actor and package ownership, material bindings and optional object-definition links. Unreal coordinates and `UStaticMesh` pointers are output, not the source of building identity.
- **Runtime** loads the baked geometry/objects. No shipping dependence on editor C++ or JSON planning is implied. Gameplay state/replication stays with existing runtime owners.

This split permits a source-derived shell with authored details without embedding geospatial parsers into the Assembly module or making World aware of facade algorithm implementation.

## 4. Mock data contracts (proposals, not implemented)

### 4.1 Selection v2: opt in without breaking v1

**Design:** retain missing `assembly_source` = massing only and existing selection v1 = original rectangle prototype. Introduce a **separately versioned v2 document/reader** only where required, keeping feature ID as the join key:

```jsonc
// Illustrative only; not accepted by current schema/reader.
{
  "$schema": "../../../../ProjectWorld/Data/Schemas/project_world_building_selection_v2.schema.json",
  "schema_version": 2,
  "assemblies": [
    {
      "feature_id": "alis:osm:way:250440683",
      "blueprint_source": "BuildingAssembly/Blueprints/selected_one_v1.json",
      "kit_source": "Plugins/World/ProjectBuildingAssembly/Data/Kits/kit_v2.json",
      "fidelity": "walkable_partial"
    }
  ]
}
```

`alis:osm:way:250440683` is **a known captured Manhattan feature ID**, not proof its interior is known or that the example will fit. Paths are conceptual and must be admitted by the same WorldData-root confinement and canonical schema resolver as v1. A v2 reader must reject any extra keys, unsupported versions, duplicate feature IDs, path traversal, nonfinite numbers, identity changes during read, and binding to a non-building feature. Keep a bounded selection; **do not mechanically preserve the cap of five** as an eternal product limit, nor raise it before batch and timing evidence warrants it.

### 4.2 Blueprint: per-feature authored description

Suggested **minimal first slice** (not a general architectural CAD schema):

| Field/group | Purpose and invariants |
|---|---|
| `schema_version`, `blueprint_id` | Closed stable document version and ID |
| `feature_id`, `expected_geometry` | Match canonical ID, expected consumed effective-volume IDs and *normalized geometry semantic digest*; reject changed footprint/volume until explicitly rebased |
| `evidence` | For each recorded exterior/interior decision: source reference, provenance/license, observation vs inference vs design intent; missing does **not** become fabricated fact |
| `local_frame` | Canonically derived building origin/orientation; no authoritative UE transform; explicit convention for front/reference edge |
| `levels` | Bounded level IDs, elevations/clear heights, floor-use category; **no floor count inferred from height** where level fidelity is claimed |
| `envelope` | For first slice, one existing supported rectangle/volume; later, recognized polygon rings and volume references, never silently simplified away |
| `exterior_openings` | Door/window slot ID, boundary edge anchor, station/width, sill/head, opening direction/type; tested for intersecting bays/corners |
| `interior` | Optional explicit room polygons, partitions, slab openings, entrances and vertical circulation with stable IDs; no runtime semantics in room labels |
| `details` | Optional bounded roof/frieze/cornice/porch/balcony placement spec referencing an admitted kit role or protected authored overlay; no arbitrary asset path execution |
| `fit_policy` | Explicit allowed geometric approximation and refusal thresholds, not permissive runtime “stretch until it fits” |
| `source_quality` | Independent exterior, interior and Ground evidence labels in output metadata/receipt |

**Possible first actual blueprint data structure (illustrative typed JSON, not a proposed schema ready for copying):**

```jsonc
{
  "schema_version": 1,
  "blueprint_id": "one_building_walkable_slice_v1",
  "feature_id": "<actual selected canonical feature ID>",
  "expected_geometry": {
    "volume_ids": ["<actual canonical volume ID>"],
    "semantic_sha256": "<generated expected-volume digest>"
  },
  "evidence": [
    {"id": "front_photo", "kind": "owned_photo", "reference": "<admitted receipt>", "scope": "exterior_only"},
    {"id": "room_design", "kind": "authored_inference", "reference": "<author decision>", "scope": "partial_interior"}
  ],
  "local_frame": {"origin": "canonical_footprint_corner", "front_edge": "<stable normalized edge anchor>"},
  "levels": [{"id": "ground", "floor_z_m": 0, "clear_height_m": 2.7}],
  "exterior_openings": [
    {"id": "front_door", "level": "ground", "edge": "<edge anchor>",
     "station_m": 1.1, "width_m": 1.0, "sill_m": 0.0, "head_m": 2.1,
     "type": "door", "evidence_ref": "front_photo"}
  ],
  "interior": {
    "coverage": "partial_authored",
    "rooms": [{"id": "entrance_room", "level": "ground", "polygon_local_m": "<closed validated ring>"}],
    "connections": [{"from": "outside", "to": "entrance_room", "opening_ref": "front_door"}]
  },
  "details": [],
  "fit_policy": "reject_unqualified"
}
```

The example deliberately leaves **unknown** IDs/rings as placeholders. It must **not** be treated as valid input or real survey of the captured Manhattan building. For production, the schema must require concrete finite arrays/IDs, consumed-source receipts and limits. Use one schema SOT with a strict native reader; do not build a second schema dialect in C++.

**Critical topology rule:** never bind authored doors to “polygon vertex 2” alone. Canonical winding/start point can change without moving the wall. For the initial rectangle, normalize deterministic start edge and orientation, bind expected footprint digest; for later polygons, use a stable edge geometry signature + station and reject/explicitly rebase after source topology movement. Do not silently relocate a historic doorway by nearest-edge fallback.

**Canonical height vs levels:** a blueprint cannot change canonical height/volume by declaring another storey. If its declared floor stack exceeds the admitted envelope, refuse the selected case, reconcile the evidence with canonical source at its owner, or explicitly change the authored scope. The source-derived `height_m` may be approximate; neither code nor authoring can assume architectural exactness from it.

### 4.3 Kit v2: a bounded module catalog, not a new language

Replace the original-test-specific intake **only when the first non-test kit is required**, without mutating `original_test_v1` identity. V2 validates two independent things:

1. **Catalog admission** — closed schema, provenance, licensed asset soft refs or original-procedural generator, bounded part count, unique role IDs, module local geometry bounds/pivots, material slots, allowed scale/trim axes, collision and LOD policy. Explicitly refuse missing or modified mesh provenance and unsupported meshes.
2. **Target fit** — planned span, opening, corner and contact checks against the actual building/blueprint, using kit metrics; the old arbitrary 10×10×6 test is not admission authority.

First needed roles: `wall_solid`, `wall_opening`, `window_frame`, `door_frame`, `slab`, `ceiling_or_roof`, `interior_partition`, `foundation_or_step`; add `stair_flight`/`landing` **only when the chosen walkable case actually has another storey**; add cornices, parapets, balcony/porch later per source. The module catalog may carry several compatible assets/styles for one role, selected explicitly by blueprint or deterministic site-independent policy. Shared meshes have one immutable kit identity; actor instances retain semantic role IDs even when several roles share a mesh.

**Sizing policy:** prefer exact modular spans and a documented bounded end/trim module; prohibit nonuniform scaling of doors/stairs or arbitrary shearing of decorative assets. A residual seam outside accepted kit tolerance refuses or requires one measured custom module. A mesh being “available” does not mean it geometrically or legally fits.

**Material policy:** ProjectMaterial owns reusable families. The blueprint/kit refers to semantic slots or admitted soft references; the World presentation binding or a future small extension resolves them. Material keys and referenced package identities participate in output authentication. The current one-material-for-four-ISMs layout is insufficient for a genuinely varied facade; do not bolt materials onto the global building role and call it solved. A true material-only update must be tested separately from geometry regeneration; until then, the current explicit refusal remains correct.

### 4.4 Semantic planner output (portable C++20)

Mock design, not code to implement now:

```text
FQualifiedBuildingInput
  feature_id, canonical_volume_semantics, footprint / volumes in canonical metres,
  final_terrain_support_receipt, accepted datum, chosen blueprint, admitted kit
      -> BuildPlan(...) : Expected<FBuildingPlan, FRefusal>

FBuildingPlan
  feature_id, versioned blueprint/kit/terrain identities, fidelity,
  local frame, canonical coverage, story/room connection graph,
  static_elements[{element_id, role_id, kit_part_id, transform, size, material_slot}],
  openings[{opening_id, wall_id, cutout and closure semantics}],
  support_elements[{id, foundation/step and checked elevation}],
  object_slots[{slot_id, kind, local transform, required object definition}],
  consumed_cell_ids, exact output bounds, counts, semantic digest
```

**Invariants of the plan:** all finite; bounded polygons/instances/triangles; no overlapping opening slots or negative wall residuals; slabs do not cover authorized stair openings; unique IDs; windows/doors physically open in collision as well as visible rendering; support footprint completely sampled; no shell element outside the admitted effective-volume policy without explicit documented overhang; room polygons neither cross the exterior nor overlap conflicting partitions; every admitted playable destination is reachable through intentional door/stair connections; deterministic ordering independent of JSON object member order and map iteration; refusal returns no partial plan.

Keep portable core reusable by the current host tests; the native adapter maps semantic role to UStaticMesh, instance/component, material and collision. Do not make the engine-independent planner depend on World or Unreal types. Prefer `Expected/Result`-style typed refusal and a machine-readable reason code rather than special-case error substrings.

## 5. Detailed geometry strategy

### 5.1 Shell geometry — extend by required capability, not by feature explosion

**Stage 1 / same selected rectangle:** reuse current rectangle support and basic box/frame mesh emitter. Change planning first: explicitly authored entrance/window bays rather than the fixed `WindowEvery` pattern; real door *opening* (static frame only); variable bay widths with a bounded residual; floor and roof that respect intended interior volumes. Produce deterministic part IDs and physical cutouts. No need for triangulating every OSM polygon on day one.

**When a real case requires an irregular outline:** walk its normalized boundary rings, segment each wall into admitted bays, solve corners with a finite join policy, triangulate floor/roof caps with hole handling, and prove surface containment and no gaps/overlaps. The existing World massing geometry utilities may be reused at the **adapter** where its dependencies already exist; do not import `ProjectWorld` internal mesh helpers into Assembly. First prove one concave polygon or one hole; do not write a universal constructive-solid-geometry solver by anticipation.

**When effective volumes are genuinely multipart:** treat `effective_volumes[]` as a *set of solid envelopes*, each with its own min/top heights and polygon(s). Build separate vertical segments with seam elimination where adjacent volumes meet; preserve source-authorized overhangs. Refuse unresolved positive intersections, vertical disconnections or inaccurate entrances; do not merge to a single bounding rectangle. Each volume uses an explicit source ID and per-part plan identity.

**Roof and facade details:** static, noninteractive detail can use ISM/HISM under measured module budgets. Uniqueness should primarily come from blueprint choices of frontage, openings, proportions, roof type and selected details, **not a random per-feature seed**. A deterministic seed can choose purely decorative equivalent variants only if the blueprint permits them, and its value becomes part of semantic identity.

**Connectivity constraints:** for multi-floor interiors, do not produce floors with no real stair shaft, stairs ending inside a slab, inaccessible balconies or window holes blocked by invisible collision. A 3D model that looks correct in an exterior screenshot is not a playable interior. Keep the first playable slice **single-level** unless a selected real example positively demands a stair, then add a bounded staircase with geometry/contact and floor-hole checks.

### 5.2 Interiors — physical static shell versus interactive world

Suggested simplest useful interior case: **one entrance, one room, one interior partition or door, physical threshold and floor support**. Upgrade to two rooms, then a second floor plus verified stairs only when useful for ALIS survival gameplay.

- Geometry plan owns internal fixed walls, ceiling/floor, door/window *cutouts*, thresholds and stair/landing geometry where required.
- Blueprint owns stable `room_id`, `level_id`, `opening_id`, `slot_id` and room adjacency. IDs do not depend on array position or module emission order; changing facade tessellation must not rename a saved door.
- Static decorative pieces that do not interact can be instanced. **Doors that open/lock, inventory items, loot containers, light switches and similar entities should be runtime objects**, not merged into ISMs that have no stable mutable state or capability lifecycle.
- The ProjectObject definitions and capability service already exist [R23]. Use those contracts for runtime behavior. A building-local object slot may ultimately be emitted by an evolved gameplay placement producer or a tightly scoped adapter; **the current gameplay placement schema and pickup validation do not yet support arbitrary building-local doors** [R24]. Demonstrate one object and its save/replication before declaring compatibility.
- Regeneration must **not delete or reset player-owned runtime state**. Stable object ID derives from stable `(world/profile, feature_id, semantic_slot_id)` with a versioned namespace. A blueprint revision that deletes/moves a slot needs an explicit compatibility/migration policy; do not infer that a new door is the previous door because it occupies a nearby location.
- Nav data is generated/validated from saved collision and admitted traversal policy, not inferred from a room graph alone. Current assembly ISMs set `CanEverAffectNavigation(false)` [R13] and production building profile says `navigation: no_navigation` [R18]. **Do not flip that on globally.** Introduce navigation only for a selected walkable fidelity, with measured generation cost and runtime tests of portal continuity.
- Interiors must have an actual streaming/residency acceptance rule: external actor(s) load before entering from outside, remain loaded while player is inside, and restore collision, door state and visible floor after travel/unload/reload. Current one-actor-per-feature, cell-owned policy is acceptable for a small single-cell building only after proving that case.

### 5.3 Original + reused assets without hidden lock-in

Use original procedural primitives as the initial kit, then opt into owned/open-license modular meshes with recorded source and stable soft references. The legal policy [R28] distinguishes OSM/ODbL geometry obligations, generated work, third-party art payloads, and ALIS-authored packages. A geometry mesh that fits a role is *not* a safe input until its origin and redistribution rights are known. Scraped photos may guide private observation only if lawful; they are not automatically redistributable building-blueprint evidence. Do not have a reproducible pipeline depend on an unpinned account-only marketplace asset.

### 5.4 Footprint, facade and room measurements must stay honest

Classify each fact as **observed/sourced**, **authored approximation**, **pure gameplay design**, or **unknown**, with source scope and confidence. In particular:

- OSM footprints and `height_m` are outside building outline/elevation data, not room layout or guaranteed window counts.
- Ground-source DSM samples may include buildings; a top surface under the building does not prove foundation bearing ground.
- Photography can constrain facade openings/roof style; it does not constrain internal partition positions without other observations.
- Do not force a detailed room to fit a wrong canonical envelope silently. Either correct source authority under its provenance rules or lower the admitted fidelity.

## 6. Terrain adaptation: exact authority boundary and process

### 6.1 Make support a first-class *read-only* input and result

The current adapter computes all-triangle coverage and min/max ground [R5], which is a good safety base. A playable entrance needs a richer **support receipt** (proposed):

```text
SupportReceipt
  canonical_base/selected_child_identity, final_ground_artifact_hashes_by_cell,
  footprint and boundary ring hash, sampled triangle coverage and provenance,
  min/max support Z, foundation plane/datum,
  entrance threshold vs exterior approach/road/ground and required step/ramp,
  cut/fill estimate only if an actual correction is proposed,
  water and authored constraints, explicit unresolved causes
```

Support must cover the actual *support and access region*, not only the first rectangle's floor XY. The current `ProjectWorldTerrainSurface::Triangles` is cell-scoped [R29]; a later cross-cell case must query **every** overlapped final-surface cell and close seams with identical shared-edge Z. Do not use a center sample or extend the owner cell’s terrain beyond its actual coverage.

Choose vertical placement policy deliberately. “Use max Terrain height as origin + extrude the foundation to min” currently prevents a buried floor but can leave a floating door/step at the low side. Separate a **finished-floor reference**, underside foundation and exterior approach/threshold; their required offsets have different gameplay meanings.

### 6.2 Decision ladder (apply in order)

| Option | Use when | Authority/work |
|---|---|---|
| **A. No correction: foundation/plinth/entrance steps** | Ground is qualified and can support the selected footprint while tolerating slope and threshold | Building kit geometry only; validate visible contact, no unsupported spans and accessible entrance |
| **B. Existing authored canonical correction** | Operator has qualified a local Ground error; bounded radial delta patches and accepted surface semantics express the needed change | `CanonicalCompilation` authored/refinement input, with provenance, water checks, new accepted canonical identity [R22] |
| **C. A new narrowly typed canonical influence** | A real selected building proves a requirement impossible under A/B (e.g. finite footprint-shaped support pad with controlled transitions) | New **upstream** CanonicalCompilation/refinement operation with explicit policy, source provenance, budget, cells/seam/Water dependency closure, approval and tests [R30] |
| **D. Refuse/defer** | DEM suitability or grade unknown; flood/road/Water conflict; cut/fill excessive; no traceable evidence | Preserve canonical Ground and existing massing/authored state; no fake terrain shape |

For option C, avoid unconstrained terrain `delta` splats that convert a DSM roof to a plausible floor. A properly qualified input should say *what is to be supported and why*, for example footprint/approach geometry, target grade evidence or desired threshold, permitted max cut/fill, transition width/slope, no-touch Water/protected intervals, source accuracy/role, and expected canonical base identity. A typed read-only **request** is not Ground authority; only accepted CanonicalCompilation output is.

### 6.3 No hidden feedback or circular compiler

```text
1. Authenticate accepted canonical building + final Terrain, blueprint and kit.
2. Read-only preflight -> OK (A) or detailed REFUSAL/NEED_CORRECTION receipt.
3. Operator resolves the failure: existing patch (B), a justified upstream model (C),
   or deferral (D). No UE actor is modified.
4. CanonicalCompilation/refinement prepares an independent candidate from an
   authenticated immutable base; admits source, seams, Water and affected cells.
5. Explicitly select/promote the qualified candidate through existing authority.
6. World reloads THAT immutable accepted result; recomputes support and geometry.
7. Only after successful complete selection preflight: registered layer Apply,
   captured inventory, snapshot/journal, manifest and recovery.
```

Never let `BuildPlan` repeatedly ask Terrain to modify itself until the shape fits; no same-transaction feedback from generated building mesh to a Terrain writer. Each canonical selection change is a distinct, traceable operation. When a correction changes a building's support and nearby road/Water/vegetation, dirty the exact affected dependency closure, including old and new footprints/transition halos, then validate the participating consumers; do not regenerate a whole territory merely for confidence if existing provenance proves a bounded scope.

**Native PCG alternative:** Epic UE **5.8** documents Mesh Partition Query/Write and priority-layer semantics, and cautions against inclusive self-feedback. This is a *possible editor backend* to evaluate **only when C is needed**, not an independent authority or the starting implementation. Epic Shape Grammar can arrange meshes, but its grammar cannot manufacture surveyed indoor topology or reconcile canonical feature and terrain ownership; a second DSL/graph would add work without solving the data problem. See official [PCG Mesh Terrain 5.8](https://dev.epicgames.com/documentation/unreal-engine/pcg-and-mesh-terrain-in-unreal-engine) and [Shape Grammar 5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-shape-grammar-with-pcg-in-unreal-engine). If later using native PCG, validate behavior in **installed** 5.8 and retain the same canonical checksums and transaction; editor direct-write bypasses are unacceptable.

## 7. Integration, identity, regeneration and failure behavior

### 7.1 Keep selected massing replacement atomic

Selection admission remains all-or-nothing for the selected batch: authenticate all selected IDs and their blueprint/kit references, **build all selected plans against the same accepted canonical snapshot**, perform full massing classification, determine the precise suppression union, and stage the generated output through the **existing World layer transaction**. The invariant is: **a selected feature has exactly one admitted building geometry representation, or the transaction fails without changing the accepted output**. An invalid plan never suppresses its massing silently.

Do not conflate a product decision to retain blockout on refusal with committing a partially selected batch. Current behavior fails a bad configured selection. In a future best-effort workflow an operator could explicitly **deselect** the unsupported feature and re-run a separately documented batch; implicit fallback within a claimed admitted selection would conceal incomplete reconstruction.

Preserve GUID/OFPA policy for a stable feature. Keep assembly actors generated and disposable, with actual mesh/material, instance transforms, collision and package provenance captured. The existing `Matches()` validates actual saved output (good); it must grow to validate the v2 static geometry, component counts, materials, semantic element IDs/slots and referenced packages. A tag/string hash alone is not proof. Retain staged replacement, loader release, cold reconstruction, deselection retirement and external actor save rules from the existing pitfalls [R31].

### 7.2 Input identity and dirty planning

A selected feature's effective plan identity should hash **consumed semantics** only, with explicit version domains:

```text
feature_id + normalized effective-volume identity + relevant source provenance
+ normalized blueprint bytes/semantics + blueprint schema/planner version
+ kit catalog ID + mesh/package revisions actually used + material binding identity
+ terrain support artifact identities for every affected cell
+ semantic static plan (stable IDs, geometry, collision policy)
+ runtime slot contract revisions (when enabled)
```

A read-only recheck of source and required outputs can still cost I/O; do not claim “no-op” means no validation. Untouched buildings/cells and unselected massing must retain semantic identity. A change to a kit role that no selected building consumes should not dirty unrelated cells *once the actual selected usage is proven*. The simplest first v2 approach may hash the whole selected kit document; only refine dependency precision after measurements show this matters.

**One stable building identity, many semantic element identities.** Static module tessellation may change without changing `feature_id` or a gameplay door's slot ID. The existing geometry SHA1 is an output identifier; it should not be misused as geographic or mutable-world identity. Semantic plans can use the World SHA-256 utilities/format where suitable, but introducing another hashing service is unnecessary.

**Cross-cell future trigger:** a canonical logical feature can affect several cells, but the v1 selected adapter refuses cross-cell support. For the *first* proven crossing, decide between (i) one feature-owned external actor with verified World Partition bounds/residency and dependency on all touched canonical cells, or (ii) per-cell static fragment actors with a stable feature+fragment identity and crack-free seams, while runtime portals retain distinct stable IDs. The second is more scalable for large buildings but materially more expensive to implement. **Do not choose by habit**; measure one real crossing and ensure layer inventory/rollback remains atomic. `FProjectWorldLayerUnitInputs` already exposes `UnitsByCanonicalCell`, but the current buildings hasher fills per-cell `HashByUnit`; assess the *actual* registry dependency contract before using a reverse mapping for cross-cell selected features. At minimum, an owner-cell unit hash must cover every intersected final-Terrain support artifact so a neighbor change cannot leave stale geometry. Never split the logical building into unrelated source identities or rely on a single-cell halo 0 when the footprint actually crosses several cells.

### 7.3 Authored hero coexistence

Current `Prepare` refuses **every feature-anchored overlay** on a selected building [R5]. This is conservative and prevents accidental loss. The authored overlay contract anchors against canonical features rather than generated actor pointers and owns protected packages [R25]. Keep that rule through the first shell slice, choosing a feature with no conflict.

When a real building requires both generated shell and handcrafted details, define a narrow opt-in: authored package remains under Authored; explicit overlay/mask identifies which region/semantic surface it owns; generated planner excludes overlapping shell/detail geometry; anchor drift and source confidence are checked; failure retains authored bytes and previously accepted output. **Never** let generated actors adopt or delete authored packages or attach directly to regenerated actors as their geographic authority.

### 7.4 Material-only and persistence dependencies

The current buildings producer fails `UpdateMaterialReferences` whenever modular selected assemblies exist [R15]. Leave this as a known explicit limitation unless the selected case genuinely needs independently changing materials. When implementing, require a standalone material-only test proving that static geometry hashes/GUID/instance transforms/physical collision and source selection remain identical while all saved material overrides/references and manifest semantics update correctly; stage/rollback any touched external actors/packages. Do not use geometry reassembly as a concealed material binding workaround.

Gameplay objects need their own generated-data lifecycle and persistent player-state contracts. If a door is tied to a building slot, construction rebase must not erase unlocked/open state simply because the shell was reassembled; introduce migration only when a real slot change occurs.

## 8. Mock implementation sequence, by independent reviewable slices

**Every item below is a plan. Nothing has been implemented or executed in this review.** Stop at the first useful bounded accepted feature. Each slice ends with source-faithful regression, small diff, evidence and explicit PASS/PATCH/BLOCKER/UNVERIFIED disposition.

### Slice 0 — select and describe one actual case (no code prerequisite)

**Owner:** World operator/data owner.

1. Pick one **real, modest, selected neighborhood building** within an already authenticated target, preferably footprint entirely in one cell and outside authored hero masks and Water. Specify source snapshot, feature ID, effective volumes and geometry/height uncertainty, actual Terrain triangle field, exterior references and license. Determine whether a walkable partial interior is desirable for ALIS gameplay.
2. Freeze the **expected fidelity** (e.g. source-accurate footprint; authored approximate facade; fictional explicitly labeled entry room), and define a specific visible success: doorway accessible from exterior, no blockout duplicates, one useable room; explicitly list what remains unknown. The Manhattan captured proof may be used if suitable; its 7.5 m height **does not disclose its floor count**.
3. Record exact failing behavior against the existing prototype (missing door/interior vs unsupported footprint, support span vs civil grade, etc.) and minimal photo/plan evidence. This decides whether geometry topology, Terrain, or material intake must be in the first implementation.
4. Baseline the current registered synthetic/native proof, production profile state and per-cell preparation cost from existing receipts; **do not infer performance** from last month's numbers. Record the intended map/profile and generated-authority scope.

**Exit:** one selected real feature plus its measurable requirements and no unresolved question that changes architecture. If evidence for real interior fidelity is absent, explicitly admit a *designed playable partial interior*, not a real reconstruction.

### Slice 1 — blueprint seam and pure planner, retain v1 compatibility

**Owner:** Assembly for reader/planner; World for source selection/qualification. Suggested touched files in a future implementation (not written here):

- Add a closed blueprint schema/portable model under Assembly **schemas**, with concrete documents in data-plugin `Data/BuildingAssembly/Blueprints/`.
- Add selection v2 schema/reader in World only if a chosen case uses it; keep v1 reader intact.
- Extend `ProjectBuildingAssemblyPlan.h/.cpp` through a **separate versioned planning entry** (`BuildV2` or equivalent) rather than changing v1 semantics. Plan static wall bays/entrance and one room; stable element IDs; full refusal reasons. Keep no-UE host compilation.
- Extend kit admission to schema-first plus selected-input fit, rather than universal `10 x 10 x 6 m`; preserve original kit pathway and identities as a regression.
- World adapter derives local frame from authenticated canonical feature, verifies expected semantic geometry and final Ground, and passes a neutral input. No canonical mutation or UE generation in this slice.

**Mock checks (not run):** strict schema/path/finite-number/limits; swapped footprint digest; changed height; wrong volume ID; inferred story count forbidden; duplicate semantic IDs; doorway too close to corner; invalid room polygon; deterministic plan after JSON member reorder; v1 byte/geometry equivalence; out-of-bounds refusal with empty plan; kit valid for selected building but not 10×10×6. One clean v2 host fixture.

**Exit:** portable plan with a real selected opening/room and stable identities, without UE or Terrain writes; v1 tests unchanged.

### Slice 2 — static native shell, one accessible entry and interior cell

**Owner:** Assembly realization, World producer/persistence.

- Provide a small role-to-static-mesh mapping, using procedural box/framed pieces first; treat door opening geometry distinctly from an interactive door actor.
- Emit floor/wall/roof/one partition with actual holes and collision; validate face winding/normals, UV/material slots and module placement. No HISM change unless measurement shows need; ISMs are adequate for repeated static parts initially.
- Stage and save feature-stable assembly actor/shared kit with the current transaction. Extend Matches/Capture to exact new components/roles/assets and preserve previous no-op/update/deselection behavior.
- Resolve foundation/threshold to accepted final triangles **without grading**; a step/plinth may be an extra static module if supported. Reject too-steep/no-approach cases rather than sculpt.
- Add a native test that walks/traces through the open doorway, stands on its floor, cannot fall through walls, and confirms exterior building blockout is gone, while neighboring massing/authored bytes are unchanged.

**Exit:** native saved/reloaded selected shell with one interior space, an entry and correct collision; measured XY deviation from the canonical source footprint (including the existing minimum 3 cm inset where retained), within an explicit acceptance policy, and clear inference labels. Do **not** claim fully playable without residency/nav tests.

### Slice 3 — lifecycle and real case admission

**Owner:** World building projection and validation owner.

- Extend the existing registered lifecycle to blueprint edits, kit edits, selected-only dirty cells, complete deselection, cold reconstruction, an intentional failure after saving an external actor, rollback of all new shared packages/actors, stale/corrupt blueprint rejection, and manifest exact population.
- Extend input hash/capture and selection preparation consistently. Do not admit massing exclusion before the **whole** selected batch is validated.
- Validate real neighborhood (not only source-shaped synthetic fixture): canonical feature coverage, independent Terrain support, actual map/OFPA package ownership, load/unload/reload, saved collision and packaged runtime presentation. Measure real host/native prep costs separately.
- Keep the new feature opt-in; production profiles without `assembly_source` remain massing-only. Change selected production data only after explicit operator admission and whole-world recovery gates.

**Exit:** reproducible selected real case with exact dirty/rollback proof, accepted material/collision/presentation and runtime streaming; **not** a general city release.

### Slice 4 — one interactive object, only if gameplay requires it

**Owner:** ProjectObject + World gameplay composition; Assembly supplies stable slot identity only.

- Define one `door` slot from the blueprint. Qualify the existing ObjectDefinition and openable capability for it. **Do not assume** `project_gameplay_placement` supports building-local coordinates or a door; it currently accepts absolute canonical XY with final-surface offset and validates pickup capability [R24].
- Either minimally extend that one producer with an authenticated `building_feature_id + slot_id + local_transform` variant and correct capability-specific admission, **or** introduce a narrowly owned object placement adapter if the existing contract demonstrably cannot express it. Choose one SOT, not two competing registries.
- Prove runtime stable ID, replicated state, save/load after shell regeneration, access/collision while open/closed, doorframe alignment, and cell streaming.

**Exit:** one actually interactive persisted door; no interior-wide gameplay framework.

### Slice 5 — conditional geometry breadth / Terrain correction, triggered by evidence

- **Footprints/volumes:** choose the *first* real unsupported concavity, hole, multi-volume or crossing, capture a source-faithful RED case and implement only that capability, retaining old cases, selected-only suppression and deterministic seams.
- **Stairs/upper levels:** only with explicit floor plan/evidence and a gameplay requirement; include slab openings, reachable landings, room graph and nav/collision checks.
- **Terrain adaptation:** require proof option A/B cannot work. Prepare the typed canonical correction under [R30] with water/edge/ground-role validation, accepted authority switch, and dependent consumer evidence. No grading API invented by Assembly.
- **Kit art/materials:** only after provenance and actual role fit. Test material-only binding if needed; prioritize source-character differences over random decoration.
- **Optimization:** use timings and the actual consumer graph to determine whether per-cell reclassification, sparse authenticated reads, ISM grouping or package sharing need change. The separate preparation-cost todo already owns general canonical authentication optimization [R26].

**Exit for each conditional branch:** one demonstrated real case newly supported; no invalidation of old source/massing/terrain authority.

## 9. Verification and acceptance matrix (future work, not run)

| Test/gate | What it must prove | Scope/result |
|---|---|---|
| Portable C++ host | Deterministic semantic IDs, blueprint fit, openings, rooms and refusal; v1 unchanged | Fast per-commit |
| JSON/schema/native reader | Exact schema-version support, bounded paths/types, provenance, license/read identity, source shape digest, no partial document | Fast per-commit |
| Independent canonical geometry | Selected feature exists, owner and referenced cells complete, effective volumes/height/provenance match; every support triangle accounted | Source-bound |
| Building massing classifier | Pre-classification overlap semantics unchanged; selected only suppressed; unselected and authored masks untouched | Regression + semantic hashes |
| Static-mesh realization | Outward normals, correct material slots, exact openings, physics collision and instance/material fidelity after save/reload | Native isolated |
| Walkability | Threshold/step, open doorway, floor/roof contact, internal partition, and stairs/portal if present; no hidden blocker | Native trace + player route |
| Navigation | Intended reachable portals only, valid floors/access, cost measured; avoid forcing all massing into nav | When walkable fidelity enrolled |
| World transaction | No-op, changed blueprint, changed kit, changed Terrain, cold absent actor, deselect, retirement, mutation failure, rollback, manifest exact | Registered World lifecycle |
| Authored protection | Authored bytes identical, mask/feature-anchor conflict refusal; no generated refs become geographic anchors | Registered + hostile fixture |
| Cross-cell (later) | Equal edge support, no cracks/duplicate shell, exact dirty cells, correct WP residency while crossing, stable actor/slot IDs | Triggered by selected feature |
| Terrain correction (later) | Qualified source Ground, max cut/fill/transition, Water/road/dependency and seam checks, accepted immutable child, no direct mesh write | Canonical/refinement tests + native consumers |
| Runtime gameplay object | One stable slot ID through regeneration/restart; capability state persists and replicates; no duplicate actors | When one slot exists |
| Product/map/packaged | Correct streamed mesh/collision/material, no doubled massing, realistic but explicitly labeled fidelity, no missing door in cook | Selected real map before release |
| Performance | Cold/no-op/change total elapsed, full authenticated I/O, generated package bytes, actors/components/instances, CPU/GPU memory, frame p95, streaming stall | Baseline + selected case; budgets decided from data |

**Adversarial cases to retain:** malformed JSON but valid hash; truncated/nonfinite fields; unknown version; swapped source footprint; altered legacy kit with stale tag; unsupported vertical/civil grade; Water overlap; missing triangle/halo; edge point outside cell; zero room portal; doorway hidden by wall collision; stair cutout covered by slab; copied actor GUID/duplicate `slot_id`; output actor missing after accepted no-op; modified material saved but cached reference stale; external package save failure; partial kit write and cold cleanup; authored package accidentally in generated root; asset provenance/license unavailable; unrelated cell source change should leave selected building unchanged; source geometry modification with same feature ID must force explicit rebase.

**Acceptance classification:**

- **PASS / prototype continuation:** one bounded validated selected feature, source/provenance truthful, correct shell/entry, stable World lifecycle; no new Terrain writer, no regression in massing or authored content.
- **PATCH:** a finite, evidenced failure remains in blueprint semantics, geometry fit, recovery, required collision or verified source/support for the chosen case; name file/line/evidence and smallest correction.
- **BLOCKER:** selected building can silently replace incorrect canonical geography, mutate Terrain outside accepted authority, erase authored or player runtime state, double-own geometry, or commit without exact rollback.
- **UNVERIFIED:** no real selected feature, no licensed detail reference, no packaged player route, no performance or terrain-support evidence; native/synthetic proofs alone cannot upgrade this state to production admission.

## 10. Risks and decision thresholds

| Risk / temptation | Recommended control |
|---|---|
| Over-generating a full interior city | Selected/opt-in fidelity, first one playable room; no fake surveyed status |
| Replacing building topology with a bounding box | Exact canonical effective-volume match; refusal before suppression |
| Inventing floor levels from source height | Explicit level evidence/design and reconciliation against volume envelope |
| Importing proprietary asset dependencies | Kit/provenance SOT, legal audit, references_only where required; original procedural fallback |
| Handling all polygons/stairs before one case | Source-faithful case first, one geometric capability per PR |
| Iterating Terrain until object fits | Explicit two-stage accepted canonical correction; no Unreal-side Terrain writer |
| Merging all door behavior into shell | Semantic slot identity + ProjectObject capabilities + separate mutable runtime data |
| Large generated actor/component count | Static ISM grouping by exact role/mesh/material; separate interactive actors only where necessary, profile before HISM/HLOD |
| Hidden materials/asset drift | Persisted reference verification and dedicated material-only contract if admitted |
| Build performance overclaim | Measure total authenticated preparation and native save/cook separately; no speculative caches |
| Private checkout differs from mirror | Re-read active source/diffs and revisit this assessment before modifying code |

## 11. Unresolved high-value questions — answer with evidence, not guesses

These do **not** block writing this mock plan but **do** determine the first implementation slice:

1. **Which actual real building/neighborhood is next?** Canonical feature IDs, polygons/parts, source bundle, Terrain source role and active WorldData profile. The existing Manhattan single-feature proof and synthetic seven-building fixture may or may not be representative.
2. **What is required fidelity for its interior?** Source-faithful floor plan, plausible clearly labeled ALIS gameplay room, or only exterior shell? If the interior must be reconstructed, what legally usable measured plan/inspection exists?
3. **What is its real access/ground problem?** Unsupported final Terrain, high DSM roof returns, excessive slope, retaining wall/threshold issue, or no Terrain problem at all? A general grading request must not be designed before this is observed.
4. **Is an openable door a first milestone or only a physical entrance opening?** That decision controls whether ProjectObject/Gameplay producer integration is in the first accepted slice or deferred.
5. **Are hero/authored overlaps present?** If so, decide explicit authored exclusion/coexistence, not automatic preference for generated geometry.
6. **What are the actual source and art redistribution rights?** A photographic facade reference, OSM-derived geometry and kit mesh may each carry distinct obligations.
7. **What is the measured size/performance of one accepted real shell?** CPU/IO prep, component count, collision, WP streaming and packaged view decide whether to invest in batching, HISM/HLOD or sparse caching.

## 12. Scope and handoff summary

**Recommend the smallest coherent first win:** one real, canonical-selected building with measured footprint, an explicitly described doorway, one declared partial interior room, admitted simple kit, preserved World massing classifier, verified full physical Terrain contact, safe generated persistence and no Unreal-side Terrain writes. Keep every other building as existing massing. Use this as a repeated end-to-end acceptance case before increasing complexity.

**Restructure the original backlog into:**

- **NOW (only after operator chooses the building):** Slice 0 (source-faithful case), Slice 1 (blueprint seam and kit-fit contract), Slice 2 (native shell + room), Slice 3 (registered World admission).
- **CONDITIONAL:** object capability slot/door (Slice 4), nonrectangular/multivolume/cross-cell, multi-floor interiors, Terrain influence, material-only update, and performance reuse (Slice 5).
- **OUT:** automatic reconstruction of unobserved real interiors, citywide procedural intake, new geospatial truth, a PCG/DSL mandatory framework, arbitrary Terraform API, general artist marketplace/cache, monolithic runtime procedural generator, and unrelated product-release certification.

**No source code, TODO, Unreal map, schema, test, manifest or generated artifact was changed.** This file is a disposable design candidate, not an accepted architecture decision or an executed test report.

---

## Evidence index (public source snapshot)

All repository links below are pinned to SHA `3f06366febd1a7987d73a91d9f3187e5a8105ef9` and should be rechecked against the active private checkout before coding.

- **R1** — [Original backlog extension TODO](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/todo/02_backlog/world/20261112-1332_world_extend_building_assembly.md).
- **R2** — [Canonical World C++ types — features, effective building volumes, Terrain](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldCanonicalBundle.h#L134-L228).
- **R3** — [Canonical building schema — massing v2 and effective volumes](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/tools/World/CanonicalCompilation/contracts/canonical-feature.schema.json#L59-L79).
- **R4** — [Building source logical-part association](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/tools/World/CanonicalCompilation/app/buildings.py); [canonical effective-volume policy](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/docs/territory_contract.md#L879-L886).
- **R5** — [World selected building: rectangle, owner cell, authored/Water and physical Terrain qualification](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingAssembly.cpp#L8-L114).
- **R6** — [Full massing classifier and post-classification selection suppression](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingMeshBuilder.cpp#L603-L746).
- **R7** — [Assembly portable plan types and kit parameters](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Public/ProjectBuildingAssemblyPlan.h).
- **R8** — [Assembly C++20 rectangular layout algorithm](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyPlan.cpp).
- **R9** — [Native four-module ISM and geometry validation](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyRealization.cpp).
- **R10** — [Building selection v1 schema, max five](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Data/Schemas/project_world_building_selection.schema.json); [World selection loader and hashes](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingSelection.cpp).
- **R11** — [Kit reader: original-only semantics and 10×10×6 admission probe](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyKit.cpp#L11-L49).
- **R12** — [Kit schema](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/Data/Schemas/modular-kit.schema.json); [current Assembly limits](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/docs/architecture/structure.md).
- **R13** — [World assembly persistence, GUID, assets, capture, no-op/reconstruction/deselection](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingSelectionPersistence.cpp).
- **R14** — [World layer producer integration](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingLayerProducer.cpp).
- **R15** — [Material-only selected ISM refusal](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingLayerProducer.cpp#L18-L27).
- **R16** — [Assembly verification/readme: Manhattan case, lifecycle, scope](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/Tests/README.md).
- **R17** — [Seven-building synthetic two-cell fixture](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorldTestData/Data/Fixtures/building_neighborhood/README.md).
- **R18** — [Kazan realization profile, massing-only buildings producer](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json).
- **R19** — [Manhattan realization profile, massing-only buildings producer](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorldData/Data/Profiles/Realization/manhattan_showcase_v1.realization.json).
- **R20** — [World Terrain-feature separation and DSM caveat](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/docs/territory_contract.md#L369-L439); [geographic/datum precision](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/docs/territory_contract.md#L54-L116).
- **R21** — [Terrain and feature authority flow](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/docs/architecture/diagrams/terrain_features.md).
- **R22** — [Canonical authored Terrain overlay patch contract](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/tools/World/CanonicalCompilation/contracts/authored-overlay.schema.json).
- **R23** — [ProjectObject responsibility, openable actors/capabilities](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/Resources/ProjectObject/README.md).
- **R24** — [Gameplay placement schema](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Data/Schemas/project_world_gameplay_placement.schema.json); [native actor qualification including pickup](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldGameplayEditor/Private/ProjectWorldGameplayPlacement.cpp#L175-L202).
- **R25** — [Protected Unreal authored anchors/masks](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Data/Schemas/project_world_authored_overlay.schema.json); [authoring resolver](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldAuthoredOverlay.h).
- **R26** — [Separate measured authenticated-preparation-cost TODO](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/todo/02_backlog/world/20261112-1259_world_reduce_authenticated_preparation_cost.md).
- **R27** — [Assembly standing decision record](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/docs/decision_record.md).
- **R28** — [World source/art provenance, license and distribution policy](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/docs/legal/world_data_and_asset_policy.md).
- **R29** — [Final-surface physical triangle query](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldTerrainSurface.cpp); [API](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldTerrainSurface.h).
- **R30** — [Conditional Terrain influence backlog](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/todo/02_backlog/world/20261112-1259_world_support_demonstrated_terrain_influence.md); [refinement architecture](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/tools/World/CanonicalCompilation/refinement/docs/architecture/structure.md).
- **R31** — [Assembly persisted-output pitfalls/recovery](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectBuildingAssembly/docs/pitfalls.md); [World-generated manifest/transaction invariants](https://github.com/fallintodusk/alis/blob/3f06366febd1a7987d73a91d9f3187e5a8105ef9/Plugins/World/ProjectWorld/docs/territory_contract.md#L895-L944).
