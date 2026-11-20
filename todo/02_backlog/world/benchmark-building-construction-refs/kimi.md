# Data-driven modular building reconstruction

**Status:** REVIEW REQUIRED  
**Architectural judgment:** PATCH - retain the World/Assembly ownership and lifecycle, but do not scale the current rectangular four-part planner into the building system.  
**Scope:** `ProjectBuildingAssembly`, the World buildings projection, concrete building recipes/kits, and conditional canonical Terrain refinement.  
**Stable documentation owners:** `Plugins/World/ProjectBuildingAssembly/docs/architecture/structure.md`, `Plugins/World/ProjectBuildingAssembly/docs/decision_record.md`, and `Plugins/World/ProjectWorld/docs/territory_contract.md`; conditional Terrain work also belongs to `tools/World/CanonicalCompilation/terrain_refinement.md`.

## Contents

- Goal and authority
- Non-goals
- Verified evidence and unresolved questions
- Current architecture and assessment
- Recommended architecture
- Required invariants
- Ordered mock implementation plan
- Test-first and verification plan
- Documentation, rollout, and completion

## Goal

Reconstruct selected canonical buildings from versioned, data-driven building recipes and rights-cleared modular assets, including explicit interiors and building-specific static details, while preserving unselected massing and authored content. Provide Terrain adaptation only when a qualified building/site design cannot obtain honest support from the accepted final Terrain.

The first implementation should prove one source-shaped synthetic building and one operator-selected real building end to end. It should establish reusable contracts without claiming city-wide admission, arbitrary footprint support, or surveyed reconstruction. Unsupported shapes and relationships must remain explicit refusals until a real selected case justifies their extension.

## Authority register

### Operator decisions

- **D1** Investigate the current architecture and develop both an architectural assessment and a detailed mock implementation plan for data-driven modular building reconstruction, including interiors, unique details, and necessary Terrain adaptation.
  - Effect: defines the deliverable; does not select a production building, kit, Terrain correction, or implementation.
  - Reason: not stated.
  - Date/source: current operator request.
- **D2** Only `tmp/alis-benchmark/candidate.md` may be written. All other files are read-only; no implementation, tests, Unreal execution, or project changes are authorized.
  - Effect: this is a planning artifact, not approval to implement or promote anything.
  - Reason: not stated.
  - Date/source: current operator request.

### Operator gates

- **Q1 [OPEN - BLOCKING]:** Which real building, reference evidence, modular asset set, and redistribution/provenance boundary should be admitted?
  - Gates: concrete production recipe/kit authoring, native inspection of the selected real assets, and any real-neighborhood admission. It does not block synthetic contract work.
- **Q2 [OPEN - BLOCKING]:** If the selected site needs Terrain correction, what target elevation, support/apron footprint, transition, cut/fill budget, and Water/civil relationships are acceptable?
  - Gates: authoring or promoting a building-specific Terrain correction. The default while open is no Terrain mutation and no automatic flattening.

### Working assumptions

- **A1 [ACTIVE]:** The first vertical slice supports one ground-level, single-cell rectangular building volume, a flat roof, explicit straight interior partitions, one stair connection, and explicit static attachments. Other canonical cases remain refused rather than approximated.
- **A2 [ACTIVE]:** Existing `ProjectObject` content is the default candidate owner for reusable architectural source meshes; `ProjectWorldData` owns concrete selections/recipes. No new content plugin is introduced without demonstrated need.
- **A3 [ACTIVE]:** The first slice proves static interiors, physical collision, and navigation using open portals. Interactive doors, hinged windows, loot, and other capabilities remain in their gameplay owners and require a separate building-local placement contract.
- **A4 [ACTIVE]:** PCG is not the outer architecture or a second generation authority. A bounded native PCG subdivision experiment may select an internal facade-run implementation; no PCG graph/component lifecycle is introduced by default.
- **A5 [ACTIVE]:** Foundation/site design is preferred over Terrain modification. A bounded canonical building-pad operation is conditional on a real failing case and failure of the existing correction route.
- **A6 [ACTIVE]:** `Apartment_Hrushev` is a candidate source of facade, room, stairway, roof, and detail modules because those assets exist. It is not an admitted kit or an operator-selected visual target.

## Non-goals

- City-wide procedural reconstruction or publication.
- Inferring faithful rooms, entrances, roof forms, or unique details from OSM tags alone.
- A general BIM/IFC model, arbitrary CSG editor, room-layout solver, inheritance system, or new shape-grammar framework.
- Runtime generation at game startup. Output remains editor-generated, saved, cooked World Partition content.
- A second Terrain writer, an Unreal sculpt pass, or PCG feedback from realized Terrain into source evidence.
- A parallel interiors layer that can disagree with exterior geometry or massing suppression.
- Copying source kit assets into generated layer roots, or retiring them through World regeneration.
- Interactive behavior inside static ISM instances.
- Migration of `BP_BuildingHrushev`, City17 placements, or the vendor `InstanceArrayTool`; that is a separate consumer task.
- A general asset cache, scheduler, HLOD/proxy system, or room/floor actor partitioning before measurements justify one.
- Durable manifest enrollment, canonical promotion, packaging, or release certification.

## Verified evidence

### Repository facts

| Evidence | Verified fact | Architectural consequence |
|---|---|---|
| `ProjectBuildingAssemblyPlan.h` and `ProjectBuildingAssemblyPlan.cpp` | The public input is `FeatureId`, width, depth, height, support span, and a scalar kit. The only part families are Wall, Window, Floor, and Roof. | Width/depth/height is a massing adapter, not a sufficient architectural contract. Adding more enum values would only extend the wrong abstraction. |
| `ProjectBuildingAssemblyPlan.cpp` | The planner rounds the floor count, divides four rectangular facades, omits the first ground bay as an entrance, selects windows periodically, and creates no interior partitions, doors, stairs, roof variants, or attachments. | The current entrance/window/layout policy is hard-coded and cannot faithfully reconstruct a specific building. |
| `ProjectBuildingAssemblyKit.cpp` | Kit admission accepts only `original_box_modules_v1` and validates a fixed 10 x 10 x 6 m probe. | The existing reader is intentionally a prototype reader, not target-independent real-kit intake. |
| `ProjectBuildingAssemblyRealization.cpp` | Native output is one actor with exactly four ISM components. Every component receives one building material at slot zero; instances scale normalized boxes/frames. | Real modules need asset/material/collision-aware grouping, not one global material or arbitrary normalized scaling. |
| `ProjectWorldBuildingAssembly.cpp` | The World adapter requires one owned, ground-level, single-cell, four-corner volume; rejects holes, elevated/multipart geometry, civil grades, constructed surfaces, authored feature overlays, Water overlap, and incomplete Terrain support. | These are useful explicit prototype bounds. They must not silently become permanent domain assumptions. |
| The same adapter | It computes complete footprint/Terrain-triangle intersection coverage and evaluates height at overlap vertices, then spans the foundation between support extrema. | Full-support qualification and fail-closed behavior are worth retaining. |
| `project_world_building_selection.schema.json` and `ProjectWorldBuildingSelection.cpp` | Selection is opt-in, limited to five records, and maps stable feature IDs to kit JSON paths. No concrete `assembly_source` was found in current production or TestData JSON. | A recipe reference can extend this seam; no current production selection establishes compatibility requirements. |
| `ProjectWorldBuildingSelectionPersistence.cpp` | The buildings layer owns selected actor replacement, shared generated kit packages, deselection, cold reconstruction, and package census in the same producer as remaining massing. Selected generated actors have stable GUIDs and spatial ownership. | This is the strongest part of the prototype and should be extended rather than replaced by a separate interior pipeline. |
| `ProjectBuildingMeshBuilder.cpp` and `ProjectWorldBuildingRealization.cpp` | Logical-building classification and unselected massing remain in the World buildings producer. Selected exclusions are supplied after classification. | Exterior/interior/attachments and massing suppression should remain one building projection and one outer transaction. |
| `ProjectBuildingAssembly/Tests/README.md` and `run_lifecycle.ps1` | The repository defines host, native, and registered lifecycle checks for deterministic layout, actual-output no-op, kit-local changes, saved-actor rollback, three-building sharing, cold reconstruction, and deselection. | These behaviors form the regression envelope. They were inspected, not executed in this investigation. |
| `tools/World/CanonicalCompilation/app/buildings.py` and `adapters.py` | Canonical buildings preserve logical outlines/parts, effective volume polygons, min/max heights, height bases, source feature IDs, and source refs. Height may be source-derived, inferred from levels, or a procedural default. | Canonical volumes are approximate geographic envelopes, not room/facade evidence. Height basis and vertical datum must remain visible to admission. |
| `canonical-feature.schema.json` and `ProjectWorldCanonicalBundle.h` | The Unreal canonical feature exposes effective volumes, not a general architectural tag/property graph or room model. | Detailed architecture needs an authored sidecar recipe, not hidden source-tag interpretation in Unreal. |
| `territory_contract.md` sections Terrain/feature separation, layered regeneration, and Building geometry authority split | Final canonical Terrain is the ground authority. Detailed objects consume it; adaptation must use canonical correction. Selected assemblies and remaining massing share one layer transaction. | No direct Mesh Terrain edit or separate assembly authority is acceptable. |
| `authored_terrain.py` and its schema | Existing authored dry correction applies bounded radial height deltas to immutable grid nodes, refuses Water and stale dependent supports, and publishes refined quads through the existing candidate route. | It may solve a selected case, but it is not an exact flat foundation-pad operation. Reuse it if it meets the case; do not invent another writer if it does. |
| `manhattan_showcase_v1.compile.json` | Manhattan uses 30 m terrain samples, 31 x 31 quad cells, 0.01 m coordinate quantization, and 0.1 m height quantization. Kazan's contract also uses 30 m samples. | Flattening whole grid nodes can affect much more than a small footprint. Exact pad boundaries/transitions require refined geometry and measured site acceptance. |
| `project_world_gameplay_placement.schema.json` and `ProjectWorldGameplayPlacement.cpp` | Gameplay placement stores canonical XY, cell, yaw, and a Terrain surface offset. It has no building-local frame, upper-floor Z, recipe anchor, or assembly-plan binding. | Existing gameplay placement cannot correctly put interactive objects on arbitrary interior floors. |
| `ProjectObject/README.md` and door/window JSON definitions | `ProjectObject` already owns building parts and composed interactive objects. Door/window definitions compose frame/sash/glass meshes and Hinged capabilities. | Static kit modules should reference this content read-only. Moving parts and capabilities must not be duplicated as fake static assembly modules. |
| `ProjectObject/Content/HumanMade/Building/` | Facade, roof, fenestration, drainage, signage, and other parts exist. `Apartment_Hrushev` includes facade, room, stairway, basement, and roof meshes plus `BP_BuildingHrushev`. | A real candidate kit exists, but filenames do not establish dimensions, pivots, collision, visual suitability, or rights. |
| Installed UE 5.8 `PCGGrammar.h`, `PCGSubdivisionBase.h`, `PCGSubdivideSegment.h`, and `PCGGrammarParser.cpp` | Native grammar supports sequences, repetition, priority/stochastic choices, fixed/scalable modules, and linear segment subdivision. Direct tokenization helpers are not all exported as a simple public planning API. | PCG may be useful inside facade expansion, but its grammar is not a whole-building ownership, interior, Terrain, or transaction contract. |
| `PCGMeshPartitionInterop.uplugin` | The installed Mesh Partition interop is Experimental and disabled by default. | It is not evidence for adopting a second Terrain or assembly pipeline. |
| `20260918-1702_world_replace_vendor_instance_arrays.md` | A separate backlog item identifies `BP_BuildingHrushev` and several repeated construction Blueprints for future project-owned tooling migration. | The Assembly boundary should accommodate that future consumer without folding City17 migration into this work or creating another catalog. |

### Inferences

- The principal defect is an undersized planning model, not a lack of procedural randomness or PCG features.
- A small set of explicit operations plus explicit attachments is likely to be easier to author, review, hash, and debug than either a general room solver or a large per-building instance list.
- Existing `Apartment_Hrushev` parts may support a useful first real-kit proof at room/facade-module granularity. Forcing those parts into normalized two-metre panels would probably reduce fidelity and create unnecessary geometry work.
- One actor per selected building remains a reasonable first streaming unit. Detailed interiors alone do not justify floor/room actor partitioning without residency and performance evidence.

### Assumptions and unverified areas

- No real mesh was loaded or measured. Bounds, pivots, sockets, UVs, materials, collision, navigation collision, Nanite suitability, and actual visual quality are unknown.
- No asset provenance or redistribution approval was established for `Apartment_Hrushev`, fenestration, or any other candidate content.
- No selected building's reference photographs, actual vertical datum, support span, entrance clearance, or Terrain correction need was measured.
- No PCG experiment was run. Native suitability for deterministic facade expansion and serialized plan output remains unproven.
- No performance, draw-call, streaming, cook, packaged collision, or navigation measurement was taken.
- Multi-volume, nonrectangular, holed, raised, and cross-cell buildings are deliberately unimplemented in the proposed first slice. Their required dependency/dirty semantics need real cases before design completion.

### Refuted premises

- **"More `EPart` values make the prototype a building system."** False: the input cannot express facade-specific sequences, interior topology, module identity, source assets, attachments, or vertical connections.
- **"Canonical effective volumes can drive faithful interiors automatically."** False: they preserve approximate massing and limited vertical evidence, not architectural interiors or unique detail.
- **"A building footprint is an automatic Terrain-flatten mask."** False: the Territory contract explicitly rejects that interpretation and requires bounded canonical correction with dependent-support checks.
- **"Existing gameplay placement can consume interior anchors."** False: its current contract is Terrain-relative XY plus surface offset.
- **"Installed PCG makes a separate PCG building pipeline the native-first answer."** Not established: native linear grammar exists, but the required plan identity, real asset admission, massing replacement, persistence, and Terrain dependencies remain Assembly/World responsibilities.

## Current architecture and source of truth

### Existing path

```text
Provider source
  -> SourceIngestion
  -> Canonical logical Building + effective volumes + final Terrain
  -> World buildings producer
       -> unselected logical-building massing
       -> optional selected feature
            -> rectangular support adapter
            -> four-part Assembly plan
            -> generated kit packages + selected external actor
       -> one buildings-layer inventory/transaction
  -> saved World Partition content
```

Relevant owners:

- Canonical Compilation owns logical footprints, effective volumes, height evidence, and final Terrain.
- `ProjectWorldData` owns concrete profiles, selections, canonical bundles, generated packages, and protected overlays.
- `ProjectBuildingAssembly` owns portable layout and native module realization, with no dependency on ProjectWorld internals.
- `ProjectWorldBuildingEditor` adapts canonical/support data, classifies massing, and owns selection persistence in the registered buildings layer.
- `ProjectObject` owns reusable building parts and interactive object composition.
- Canonical Terrain refinement owns supported post-canonical corrections; Mesh Terrain only realizes the result.

### Boundary declaration for the proposal

```text
Owning black box:
  ProjectBuildingAssembly planning and native realization, integrated through
  the existing ProjectWorldBuildingEditor buildings projection.

Public contract to replace/version:
  The width/depth/height + scalar-kit FInput and four-family FPlan are replaced
  by versioned recipe/kit contracts and a building-local deterministic plan.

Expected components CHANGED:
  ProjectBuildingAssembly contracts, planner, kit admission, realization, tests,
  schemas and docs; ProjectWorldBuildingEditor selection/qualification/hashing/
  persistence/tests/docs; concrete TestData and, after separate admission,
  ProjectWorldData selections/recipes. Canonical refinement changes only if the
  conditional Terrain branch is selected and the existing operation is unfit.

Expected components UNTOUCHED:
  SourceIngestion provider semantics, canonical logical-building topology,
  massing classification, generic layer registry/transaction machinery,
  MeshTerrain realization, ProjectMaterial graph compilation, gameplay runtime,
  City17, and unrelated generated layers/outputs.
```

Architectural red conditions:

- a second Terrain or building writer appears;
- interiors require a separate suppression/lifecycle path;
- source meshes must be copied into or deleted from generated roots;
- a kit/recipe edit dirties unrelated cells or layers;
- room/art semantics must be added to provider canonical geography;
- interactive capabilities must be embedded in static instances;
- a local selected-building change requires unrelated producer edits.

## Problem and root cause

The prototype answers "can a selected rectangular massing be replaced by deterministic native modular instances inside the existing transaction?" The repository evidence supports that limited purpose.

The requested outcome instead requires four distinct models that the current `FInput` cannot represent:

1. **Architectural intent:** named levels, facade runs, partitions, stairs, roof and unique attachments.
2. **Module contract:** asset identity, nominal placement dimensions, pivot/axes, allowed scaling, materials, collision/navigation, and provenance.
3. **Site contract:** canonical volume/frame, final support, envelope constraints, and explicit refusals.
4. **Realization contract:** stable plan identity, native output census, package ownership, exact dirty scope, and recovery.

Keeping those models separate prevents a kit from becoming a building recipe, a source footprint from becoming an art asset, or a generated actor from becoming editable authority. The existing World transaction should bind them only at realization.

## Decision

### Recommended architecture: recipe -> deterministic plan -> existing World transaction

Use a **building recipe**, not a UE Blueprint and not a second canonical building model. A selection maps one stable canonical `feature_id` to a concrete recipe. A recipe references one or more kit contracts and contains only architectural/site intent. Assembly expands that intent into a fully deterministic building-local plan. The existing buildings producer realizes that plan and suppresses exactly the selected massing in the same transaction.

```text
Canonical volume + final Terrain -------\
                                        \
Measured source-kit contracts -----------+-> Assembly Prepare
Selection + building recipe ------------/         |
                                                  v
                                      versioned building-local plan
                                                  |
                                                  v
                           native kit admission + staged native output
                                                  |
                                                  v
                     World buildings transaction + massing exclusion
                                                  |
                                                  v
                               saved/cooked World Partition content

Optional, acyclic site branch:
canonical volume + explicit support intent
  -> Canonical Terrain refinement candidate
  -> final Terrain
  -> Assembly Prepare above
```

### Ownership of durable facts

| Fact | Proposed owner |
|---|---|
| Logical footprint, volumes, source height basis, canonical feature ID | Existing Canonical Compilation and canonical schemas |
| Final ground and any accepted Terrain correction | Existing Canonical Terrain owner/refinement publisher |
| Concrete feature-to-recipe selection | `ProjectWorldData/Data/BuildingAssembly/`; confined TestData equivalents for fixtures |
| Per-building architectural recipe | Same concrete data owner as the selection; schema owned by `ProjectBuildingAssembly` |
| Reusable kit descriptor and source asset provenance | The source content owner, initially candidate `ProjectObject/Data/BuildingKits/`; generic schema remains in Assembly |
| Source meshes/materials | Existing content owner, referenced read-only; never a generated layer artifact |
| Generated filler/foundation meshes and selected actors | Existing World buildings artifact root and manifest |
| Planner and native realization | `ProjectBuildingAssembly` |
| Selection hashing, support qualification, massing exclusion, external actors, rollback | `ProjectWorldBuildingEditor` and the existing outer transaction |
| Interactive doors/windows/loot/capabilities | `ProjectObject` definitions and the gameplay-placement owner, after a separate building-local anchor contract exists |

### Recipe contract

Use closed, versioned JSON with semantic validation beyond JSON Schema. The first version needs only these concepts:

| Area | Required semantics |
|---|---|
| Identity | `recipe_id`, schema version, provenance, and bounded budgets. Stable human-reviewed IDs; no display-name identity. |
| Site | Canonical `volume_id`, an expected canonical frame anchor and X-axis point with tolerance, vertical policy, and admitted envelope tolerance. The canonical ring remains the footprint authority. |
| Kits | Role-to-kit-source bindings. Repository paths are confined to admitted kit roots or the TestData scratch namespace. |
| Levels | Explicit named floor elevations/heights. Their total envelope must agree with the canonical volume; different evidence requires a canonical/site correction first. |
| Envelope/roof | Initial single-volume shell and flat slab/roof operation. Unsupported roof forms refuse. |
| Facade runs | Named volume edge plus ordered `module`, `repeat`, and bounded `fill` tokens. No random or priority choices in v1. |
| Interior partitions | Explicit level, start/end segment, thickness/height policy, and the same run tokens with real opening modules. No room solver is required. |
| Vertical connection | Explicit stair module, lower/upper levels, footprint/orientation, landings, and headroom requirements. |
| Attachments | Unique static module ID plus explicit building-local transform and variant/material policy. No inheritance or random detail pass. |
| Validation | Named envelope/support/clearance limits and finite module/component/instance/triangle budgets. |

A frame must not be inferred from whichever ring edge happens to be longest or first. Pin an expected canonical origin and direction, resolve them against the current volume, and refuse movement beyond tolerance. Convert the resulting building-local metre frame to Unreal once in the World adapter. This keeps recipes independent of CRS and UE handedness while preventing silent rotation of unique details.

Explicit `module` and `repeat` tokens cover intentional repetition. A `fill` token covers only a declared filler role with an allowed width range and UV/scale policy. Negative remainder, excess filler, corner overlap, out-of-envelope geometry, unresolved module IDs, and duplicate placement IDs refuse. Do not stretch detailed windows, doors, stairs, or props to make a footprint fit.

Avoid recipe inheritance and archetype overrides until several real recipes demonstrate a concrete duplication problem. Copying a small explicit run is cheaper than debugging a generalized template language.

### Kit contract and admission

A kit descriptor is a placement contract, not a mesh catalog duplicated for its own sake. Each module declares:

- stable `module_id` and source asset soft path;
- allowed roles and nominal placement dimensions;
- pivot convention, forward/up axes, and any required socket names;
- scale policy (`forbidden` by default, or bounded only for declared filler/simple pieces);
- material-slot policy, initially preserve source slots;
- collision and navigation policy;
- provenance/distribution identity and revision.

Native admission loads the real asset and compares its actual bounds, pivot/axes, sockets, material slots, collision, navigation collision, and required build settings against that contract. The portable planner consumes a normalized measured descriptor; host tests supply synthetic descriptors. No 10 x 10 x 6 m probe and no persistent general asset cache are needed.

Nominal dimensions are a contract pin, while the mesh remains geometry authority. A mismatch refuses instead of silently rescaling. Relevant measured values enter plan identity. Same-path material-only content changes do not dirty geometry; a changed placement dimension/socket/collision contract must invalidate or refuse the affected consumers. Saved-output verification independently inspects actual references and native state.

Room-sized and facade-sized meshes are valid modules when they are the real reusable unit. Do not force every asset into a two-metre panel grid or split a unique mesh merely to appear procedural. The modular boundary should follow reuse and independent placement, not an arbitrary size.

### Deterministic plan

Replace the four-part enum-centric plan with neutral records conceptually equivalent to:

```text
Plan
  feature_id, recipe identity, kit-contract identities, support binding
  local frame, envelope bounds, finite metrics
  module placements[]
    placement_id, kit/module ID, local transform, role
    collision/navigation policy, material-override identity
  generated mesh requests[]
    mesh_id, operation, polygon/levels, material/collision/navigation policy
  normalized semantic identity
```

The planner performs no UObject access and emits no actors. It sorts normalized output by stable semantic IDs, rejects duplicate IDs and nonfinite values, and produces the same plan across cold runs. All consumed recipe, measured-kit, frame, canonical-volume, support, and planner-revision inputs participate in identity; provenance-only hashes do not become rebuild keys.

The first implementation may generate simple foundation/slab/filler meshes. More complex polygon operations belong behind a native geometry adapter and are admitted only for a demonstrated shape. Do not write a general CSG stack for flat slabs and straight partitions.

### Native realization

- Keep one stable external actor per selected building in the first slice.
- Group static placements into ISM components by actual mesh, material-override set, collision/navigation policy, and mobility. Do not force exactly four components or overwrite slot zero globally.
- Preserve source mesh materials by default. Generated simple meshes may consume the existing World building role. Add per-slot overrides only with an actual consumer and qualify material-reference-only adoption at the same time.
- Keep source asset packages read-only. Generated filler/foundation packages live below the buildings artifact root and follow existing retirement rules.
- Require finite component/instance/triangle budgets. Measure before considering per-floor/per-room actors, merging, HLOD, or a new cache.
- Keep static mobility and no HLOD. Preserve the existing actual-output no-op comparison, staged validation, stable GUID, save/reload census, and outer rollback.
- Give physical shell/stair collision an explicit assembly navigation policy distinct from unselected massing's `no_navigation`. Source assets must already provide the declared collision; generated physical meshes create native navigation collision as the constructed producer does. Decorative/nonphysical modules remain navigation-irrelevant.

### Interiors and unique details

Interiors are explicit partition/opening/slab/stair placements in the same plan as the shell. This avoids interior/exterior drift and allows one transaction and one support/frame identity.

Unique static details are explicit attachments or purpose-specific modules, not a stochastic decoration layer. A room/facade module with proven unique geometry is also acceptable. If detail is independently hand-authored, protected, interactive, or stateful, it belongs to the authored overlay or gameplay owner instead of being forced into the recipe.

The first playable proof uses open portals and static stair geometry. Existing ObjectDefinition door/window compositions show the correct owner for later interactive parts. A later gameplay contract needs a stable feature/recipe anchor, building-local transform, expected plan/site identity, and failure on moved geometry; the current Terrain-relative placement schema is insufficient.

### Terrain adaptation

Apply this order to every selected site:

1. **No correction:** sample complete final-Terrain support. Use a bounded foundation spanning extrema when entrance, floor, clearance, and visual contact remain acceptable.
2. **Redesign the object/site:** use foundation, porch, stairs, or a separate constructed retaining body when physically justified. Do not disguise a vertical retaining wall as a steep Terrain triangle.
3. **Use existing canonical correction:** if the existing authored dry correction can satisfy the actual support/transition requirements, use its normal candidate and dependent-regeneration route and stop there.
4. **Add one bounded building-pad operation only on demonstrated failure:** extend Canonical Compilation refinement, never Unreal realization.

A conditional `building_support_pad` request should reference the canonical feature/volume and base-feature hash rather than duplicating footprint coordinates. Its authored inputs are target elevation in the canonical vertical datum, support inset/outset, transition width, maximum cut/fill, maximum transition slope, quad/triangle budgets, and provenance. It must not include a plan hash that depends on the resulting Terrain; that would create a cycle.

The bounded constructor should:

1. authenticate complete original support and all affected cells;
2. derive the pad from the canonical volume and declared dimensions;
3. use the existing refined-quad candidate/publisher and native constrained-geometry environment to create an exact flat contact region and bounded transition;
4. preserve the complete outside-influence field and shared cell edges;
5. refuse Water, civil-grade, Road/constructed-support conflicts, unresolved neighbors, vertical Terrain faces, excessive cut/fill/slope/triangles, or a redundant zero change;
6. emit normal canonical identities/receipts and exact dirty cells;
7. require Assembly preflight and isolated native support/interior evidence against the candidate before any paired canonical/World promotion.

Initially restrict the operation to one ground-level rectangular volume whose complete pad and transition stay inside one owner cell and an unrefined compatible base. Do not design arbitrary correction chains, cross-cell pads, or general object influence APIs without a real case.

### Premise / KISS gate

`ProjectBuildingAssembly` already owns the required planning/realization lifecycle, and `ProjectWorldBuildingEditor` already owns selection plus massing replacement. Extend those owners.

The proposal adds two data contracts (recipe and real-kit descriptor), a normalized plan, and asset-aware realization. It adds no service, process, runtime scheduler, PCG component lifecycle, second layer, second Terrain writer, general cache, or compatibility framework. It removes the need to grow hard-coded box special cases and avoids separate monolithic building-generation Blueprints.

The knowingly dropped capability is automatic arbitrary floor-plan synthesis. Designers must specify meaningful facade/interior/detail intent. That is desirable for faithful reconstruction and is not a missing technical feature.

### Native engine evaluation and alternatives

| Option | Judgment |
|---|---|
| Extend width/depth/height and `EPart` | Reject: cannot express the required semantics without accumulating special cases. |
| Make a PCG graph the building authority | Reject: it would add graph/component output lifecycle without owning recipe identity, massing suppression, Terrain correction, or recovery. |
| Use native PCG linear subdivision inside Assembly | Conditional: run one disposable experiment against explicit module/repeat/fill runs. Adopt only if the installed public API produces exact deterministic output, strict refusal, and a serializable plan without direct spawning or a second lifecycle. Otherwise implement the much smaller closed run expander and record the concrete native gap. |
| Fully explicit per-instance JSON | Reject as the only interface: possible but verbose and error-prone. Keep explicit attachments; use three simple run tokens for repetition. |
| General room graph/shape grammar/BIM | Reject for this slice: no demonstrated need and much larger validation/authoring surface. |
| Put full architectural recipes into canonical provider features | Reject: mixes geographic evidence with authored art and UE asset references. |
| Generate one monolithic mesh per building | Reject as the default: loses module reuse and detail-level inspection. Large reusable room/facade modules remain valid where that is the natural asset unit. |
| Flatten Terrain under every assembly | Reject: unnecessary geographic churn, coarse-grid artifacts, Water/civil risk, and hidden authority coupling. |
| Reuse City17/vendor actor generation | Reject for new World work: City17 is frozen and the vendor migration is a separate task. |

## Required invariants

1. Canonical footprints/volumes and final Terrain remain the only geographic authorities. Recipes cannot silently override them.
2. One buildings projection owns selected exterior, interior, attachments, generated dependencies, remaining massing, and deselection in one recoverable transaction.
3. An assembled feature never renders or collides as both massing and assembly. Unselected buildings and protected authored content remain unchanged.
4. Interactive/stateful objects are never baked into static assembly instances or given a fake Terrain-relative interior transform.
5. Plans are deterministic, normalized, bounded, building-local, and independent of UObject lifetime and JSON formatting/order where order is not semantic.
6. Every failure before promotion leaves accepted canonical, generated, authored, and source-asset state unchanged. Unsupported or unknown input fails closed with feature/recipe/module context.
7. Final Terrain has one canonical writer. No Unreal/PCG sculpt or realized-output feedback becomes source evidence.
8. Terrain request identity is independent of the Terrain result; support planning is acyclic.
9. Consumed recipe, measured-kit contract, frame, canonical-volume, support, and output-revision changes dirty exactly affected consumers. Provenance-only changes do not.
10. Source kit assets are read-only dependencies and are never retired by World regeneration. Only manifest-owned generated packages/actors are replaced or deleted.
11. Detailed modules are not silently scaled to fit. Actual asset bounds, pivots, materials, collision, and navigation are admitted and rechecked in saved output.
12. No-op means actual native output matches, not merely a hash tag. Cold reconstruction restores missing expected actors even when input identity is unchanged.
13. Complete support, entrance clearance, floor/headroom, and stair/path continuity are proved on the actual native collision surface. Presence counts and screenshots alone are not acceptance.
14. Assembly navigation policy is explicit and separate from unselected massing. Navigation evidence includes saved/reloaded dynamic Recast behavior when playable admission requires it.
15. Initial unsupported shapes, cross-cell dependencies, Water/civil conflicts, and authored overlays refuse rather than silently degrade to a rectangle or another cell's support.
16. No runtime generation, hidden fallback kit, general cache, HLOD path, or broad regeneration is introduced without a measured requirement.
17. Concrete production/profile enrollment and canonical/World promotion remain separate operator approvals after reversible candidate evidence.
18. Existing unrelated working-tree changes are preserved, and stable code/docs never depend on this candidate file.

## Implementation tasks (mock, ordered)

### 0. Freeze acceptance and select the case

- [ ] Obtain Q1: select one real canonical feature with reference evidence, a candidate kit, and a permitted provenance/distribution boundary.
- [ ] Record its canonical volume IDs, height bases, owner cell, final-Terrain support, protected overlays, Water/civil/constructed relationships, and current massing output. Do not infer fidelity from its rectangular shape.
- [ ] Inspect the candidate `Apartment_Hrushev` or alternative meshes through a bounded editor asset probe: actual bounds, pivots, sockets, materials, collision/navigation, current referencers, and provenance. Do not migrate `BP_BuildingHrushev` or vendor code.
- [ ] Choose one small expected interior route and two unique static details. Define their independent visual/physical acceptance, not just module counts.
- [ ] Complete independent R1 review of the recipe/kit/site contracts, identity closure, first supported subset, and stop conditions before production code.

### 1. Add failing acceptance and regression evidence first

- [ ] Add a source-shaped two-story fixture with an open entrance, one interior doorway, a stair to an upper room, and two explicitly placed details. Include one nearby unselected building and one protected overlay.
- [ ] Capture the current failure: the four-part planner cannot represent the required placements and the native route cannot traverse the interior. Do not encode the current box output as the desired result.
- [ ] Add host cases for deterministic normalized plans, exact facade/partition runs, filler limits, stair/landing bounds, envelope excess, duplicate IDs, unknown modules, and refusal without partial output.
- [ ] Add native admission cases for wrong bounds/pivot/socket/material/collision/navigation and for a target-shaped kit that the fixed probe rejects although its actual target is valid.
- [ ] Add lifecycle cases for recipe-only locality, source-asset preservation, material-slot drift, missing interior collision, and cold/deselection restoration.
- [ ] If support correction is required, add a small coarse-grid sloped fixture whose required pad is smaller than a grid quad and whose neighbor/Water control must remain unchanged. Observe refusal before implementing any new operation.

### 2. Introduce versioned data contracts

- [ ] Add `building-recipe.schema.json` and the real static-mesh kit schema under `ProjectBuildingAssembly/Data/Schemas/`. Keep the original procedural kit only as a synthetic control, not a production fallback.
- [ ] Extend the World selection schema to map `feature_id` to `recipe_source`; recipe resolution authenticates all referenced kit sources. Keep finite selection and document-size budgets.
- [ ] Put concrete recipes/selections in the owning WorldData/TestData `Data/BuildingAssembly/` tree. Put admitted real-kit descriptors beside their owning content data, candidate `ProjectObject/Data/BuildingKits/`.
- [ ] Resolve every `$schema` relatively and every content path through confined owner roots. Reject traversal, aliases, unknown fields, duplicate IDs, and changed-during-read files.
- [ ] Extend `python scripts/ue/check/data/validate_all.py` coverage through its existing plugin `Data/` discovery; add semantic cross-reference checks where JSON Schema cannot prove feature/volume/module uniqueness.
- [ ] Migrate the synthetic selection/lifecycle inputs in the same change. No CoreRedirects, deprecated reflected fields, or hypothetical production compatibility path.

### 3. Implement target-independent kit admission

- [ ] Split `ProjectBuildingAssemblyKit.cpp` into closed JSON reading and native asset measurement if it exceeds its current cohesive role. Keep the module small and retain its existing strict-type checks.
- [ ] Produce normalized measured module descriptors from real `UStaticMesh` assets. Refuse mismatches rather than repairing source assets or normalizing them through hidden scaling.
- [ ] Validate source material slots and preserve them by default. Verify declared collision and navigation data on the actual asset, including stair collision suitability.
- [ ] Include consumed measured contract values in Assembly identity. Treat file byte hashes as provenance unless their contents are actually consumed by planning.
- [ ] Run the bounded PCG subdivision experiment now if facade expansion may use it. Record the exact API, deterministic output, refusal behavior, and added dependency; choose the smallest adapter or closed local expansion based on that evidence.

### 4. Implement the recipe planner

- [ ] Replace the width/depth/height-only public input with neutral site, recipe, and measured-kit types. Keep the actual production planning core host-testable where the operation is engine-independent.
- [ ] Resolve and validate the canonical frame in the World adapter; pass building-local normalized geometry to Assembly. Keep CRS and Unreal conversion out of the portable planner.
- [ ] Implement named levels, flat shell/slab, facade and partition `module`/`repeat`/`fill` expansion, explicit stair/landing placement, and explicit attachments.
- [ ] Generate stable placement/mesh IDs and a normalized plan. Check envelope, corners, openings, stair landings/headroom, finite geometry, and component/instance/triangle budgets before returning success.
- [ ] Add only simple foundation/slab/filler mesh requests needed by the selected case. Use native GeometryCore/GeometryAlgorithms for any subsequently demonstrated polygon operation instead of writing a general geometry kernel.
- [ ] Preserve a diagnostic refusal receipt with feature, recipe, module/placement, and reason. Do not add a new error framework or fallback plan.

### 5. Implement asset-aware native realization

- [ ] Generalize `FNativeOptions` and `Apply` to consume measured source modules plus generated mesh requests. Retain the stable actor GUID/name and external-package behavior.
- [ ] Build ISM groups by mesh/material/collision/navigation identity; preserve per-slot materials and all actual instance transforms. Do not require four components or one building material.
- [ ] Generate only owned filler/foundation assets below the buildings root. Never copy, save, or retire source kit packages from this path.
- [ ] Apply static mobility, spatial loading, collision and assembly navigation policy. Create navigation collision for generated physical meshes and verify source-provided collision for source modules.
- [ ] Extend `Matches` to census every expected component, instance, mesh reference, material slot, transform, collision/navigation flag, generated mesh, actor identity, and source dependency. Refuse extras and duplicates as well as missing output.
- [ ] Keep candidate validation before old-output removal and retain outer rollback for external in-place updates. Add failure after a real actor save and after a generated mesh save.

### 6. Integrate exact World identity and lifecycle

- [ ] Update `ProjectWorldBuildingAssembly::Prepare` to resolve recipe/site/support and return the new plan plus remaining massing. Retain full physical support coverage and current explicit unsupported-case refusals.
- [ ] Update selection loading, `HashCell`, `BuildCell`, `Apply`, and `Capture` to authenticate recipe/kit/plan/source identities and exact expected population before mutation.
- [ ] Keep classification before exclusion. All selected recipes must qualify before any selected massing can commit as suppressed.
- [ ] Register the appropriate next `project_building_massing:v2` output revision and verification baseline for the changed saved-output behavior; do not create a parallel interior producer.
- [ ] Preserve massing-only output when no selection is configured. A first opt-in profile/settings revision may have a deliberately bounded migration scope; subsequent recipe/kit edits must affect only their selected cells.
- [ ] Extend `run_lifecycle.ps1` and its preparer for one then three detailed selections, shared source modules, no-op, recipe-only change, generated-mesh change, rollback, cold reconstruction, and complete deselection.
- [ ] Assert that deselection restores massing and retires only owned generated output, while every pre-existing source kit asset and unrelated generated/authored path remains byte-identical.

### 7. Prove the detailed real candidate reversibly

- [ ] Author the selected real recipe using measured modules and explicit interior/detail intent. Do not alter canonical source evidence to make the recipe fit.
- [ ] Prepare a fresh isolated canonical/native case through the existing preparation/wrapper route. No production profile enrollment or hand-edited generated output.
- [ ] Prove exact module placements, interior route, floor/headroom, stair landings, source materials, detail transforms, support contact, and absence of competing massing after save/reload.
- [ ] Capture player-level exterior and interior images with a neutral-material control where useful. Visual review is a separate operator gate; a census or render alone is not visual approval.
- [ ] Measure actor/component/instance/triangle counts, cook/reference closure, and actual streaming residency for the candidate. Stop without optimization if the bounded acceptance budget is met.
- [ ] Complete independent R2 review before selecting any production Matrix or durable authority operation.

### 8. Add Terrain adaptation only if the selected case proves it necessary

- [ ] Record why foundation/site redesign and the existing authored correction cannot meet the required support. Obtain Q2 before authoring a new pad policy.
- [ ] If the existing operation suffices, prepare its candidate and prove Assembly against the resulting final Terrain; do not add a building-specific constructor.
- [ ] Otherwise add the closed `building_support_pad` request/result variants to the existing refinement schemas, operation admission, publisher, and `terrain-refinement` construction enum. Allow only the bounded single-volume/single-cell base case.
- [ ] Implement exact pad/transition construction through the existing refined-quad and constrained-geometry helpers. Include cut/fill/slope/triangle limits, full outside preservation, neighbor/seam closure, and all dependent-support refusals.
- [ ] Add focused Python tests for cold/no-op equivalence, exact subgrid pad coverage, unchanged exterior, shared edges, Water/civil/constructed refusal, stale base/domain, excessive correction, failed promotion, and frozen replay.
- [ ] Add native tests for final stored triangles, foundation contact, interior route, navigation, and no hidden MeshTerrain-side deformation.
- [ ] Preflight Assembly and all declared dependent consumers before promotion. Use the existing exact dirty closure and, only with later explicit approval, the paired canonical/World authority route.

### 9. Migrate durable knowledge

- [ ] Update Assembly `structure.md` with recipe/kit/plan/native boundaries and current supported subset.
- [ ] Update Assembly `decision_record.md` with the hybrid modular direction, source-asset ownership, and why arbitrary floor-plan inference is excluded.
- [ ] Update World `territory_contract.md` building split and Terrain/feature sections with recipe identity, static/gameplay separation, and any accepted support operation.
- [ ] Update Assembly Tests README and lifecycle fixture README with exact preparation and proof envelopes.
- [ ] Update source-kit provenance/release records under the asset policy. A mesh filename or private availability is not redistribution approval.
- [ ] Add verified non-obvious defects to the owning `pitfalls.md`; do not narrate implementation history in architecture docs.
- [ ] Ensure stable docs/code/tests do not reference this candidate or either backlog TODO.

### 10. Final verification and stop

- [ ] Run the red-to-green matrix below, selecting exact tests during iteration and only the required L2 gate at slice exit.
- [ ] Inspect the final diff against the changed/untouched declaration and current SOTs. Stop on architectural red.
- [ ] Report verified properties, unverified dimensions, all refusals, and remaining operator gates separately.
- [ ] Stop before L3 promotion, packaging, vendor migration, city-wide selection, or any optional follow-up not required by the accepted case.

## Test-first and verification plan

### Red evidence matrix

Every permanent guard is written and observed failing for the stated reason before its production implementation. Acceptance-only visual/performance judgments are reviewed, not converted into arbitrary allowlist tests.

| Case | Wrong/missing behavior captured before implementation | Proof type |
|---|---|---|
| Two-story interior recipe | Current plan has no partition, stair, attachment, or module identity and cannot produce the required route | Acceptance and permanent planner regression |
| Explicit facade/partition runs | Hard-coded periodic windows/first-bay entrance cannot represent intentional sequences | Permanent planner regression |
| Real target kit | Fixed 10 x 10 x 6 probe and original-only provenance reject an otherwise valid target kit | Permanent admission regression |
| Asset contract drift | Current normalized box matcher cannot detect a real module's wrong pivot, socket, source slot, or navigation collision | Permanent native regression |
| Interior physical route | Current roof-only trace cannot prove entrance, doorway, upper floor, landing, or headroom; route is absent | Acceptance plus saved native regression |
| Source materials | Global slot-zero assignment destroys the declared per-slot appearance | Permanent native regression |
| Recipe locality | Existing identity has no recipe contract; required selected-only invalidation is unavailable | Permanent lifecycle regression |
| Source ownership | Current shared generated kit logic must not copy/delete read-only `ProjectObject` dependencies | Permanent lifecycle regression |
| Failure recovery | Detailed plan needs refusal after real actor/generated-mesh save with exact prior-state restoration | Permanent transaction regression |
| Coarse-grid support | Current support-span refusal blocks the selected sloped case; existing radial correction cannot certify the required exact pad if that is demonstrated | Conditional canonical regression |
| PCG native candidate | Direct/native output is compared with the same explicit run and a known over-constrained run; no fit claim without a discriminating case | Disposable native-first experiment |

### Green evidence and execution envelope

Use the existing commands and wrappers; proposed new exact test names are placeholders until registered in source. Do not run broad filters during ordinary iteration.

| Boundary | Evidence and command route | What it proves |
|---|---|---|
| JSON contracts | `python scripts/ue/check/data/validate_all.py` plus owner semantic-reference tests | Closed schemas, valid relative schemas, and cross-document identity. Not native asset validity. |
| Portable planner | Expanded `python -B Plugins/World/ProjectBuildingAssembly/Tests/run.py` | Actual production planning core, deterministic replay, detailed placements, bounds, and refusal. Not UE geometry/collision. |
| Native kit admission | `scripts/ue/test/unit/iterate.ps1 -TestFilter Project.World.BuildingAssembly.RealKitAdmission` | Real asset measurement and drift refusal in the editor. |
| Recipe-to-plan native seam | `... -TestFilter Project.World.BuildingAssembly.DetailedPlan` | Canonical frame/support to native-plan conversion and identity. |
| Saved detailed interior | `... -TestFilter Project.World.BuildingAssembly.DetailedInterior` | Actual modules/materials/collision/navigation, route, headroom, save/reload, and failure restoration. |
| Existing source-bound case | Retained or migrated `... -TestFilter Project.World.BuildingAssembly.CanonicalPrototype` | Real canonical feature and unchanged unrelated massing in a disposable scene. |
| Presentation | Retained or migrated `... -TestFilter Project.World.BuildingAssembly.SavedRender` | Exact saved-scene exterior/interior/detail presentation and neutral control. Not visual approval by itself. |
| Registered lifecycle | Fresh `prepare_lifecycle.py` case followed by `Tests/run_lifecycle.ps1 -CaseRoot <tmp-case>` | Normal profile/producer/manifest path, no-op, locality, rollback, cold reconstruction, deselection, and outer restoration. |
| Canonical pad, conditional | `python -B -m unittest World.CanonicalCompilation.tests.test_building_support` with `tools/` on `PYTHONPATH` and the pinned environment | Canonical candidate identities, exact field/influence, refusal and promotion recovery. Not native gameplay. |
| Native pad, conditional | Exact building-support test through `iterate.ps1`, followed by only affected MeshTerrain/building verification | Stored final Terrain, support, collision/navigation, and absence of a second writer. |
| Real playable route | Focused uncooked gameplay admission on the selected candidate after its native acceptance | Actual character/camera/collision and required navigation/streaming properties in the real route. Not packaged proof. |
| Slice exit | Read-only `EndToEndValidation/bootstrap.py plan`, then only the selected common Check/Matrix required by changed owners | Changed-owner acceptance and exact restoration. No automatic L3/L4 escalation. |

Sabotage at least the following guards and observe the expected failure: remove an opening, block a stair landing, alter one module pivot, clear one material slot, remove stair navigation collision, move Terrain support beyond tolerance, drift one instance, delete a source reference, and inject a post-save failure. Restore the exact fixture after each control.

## Documentation plan

- **Authoritative stable owners:** Assembly architecture/decision record and Tests README; World Territory contract; conditional Canonical `terrain_refinement.md` and refinement router; source-kit provenance records under the World asset policy.
- **Router updates:** Existing Assembly `docs/README.md`, `architecture/README.md`, `Data/README.md`, and refinement `README.md` only when their child set changes.
- **Content:** durable contracts, current supported subset, ownership, identity/failure semantics, and operating route only.
- **Duplication avoided:** schemas own field shapes; code owns algorithms; tests own executable examples. Documentation links to them instead of copying JSON fields or test matrices.
- **Forbidden dependency:** no stable code, comment, test, configuration, or documentation may link to this candidate or either TODO.

## Rollout and rollback

All development and first real-candidate acceptance remain under `tmp/world/` or isolated TestData output with outer snapshot restoration. Source kit assets are never mutated. No production profile selects the recipe until the candidate and independent review pass.

Canonical correction and World realization are separate candidates first. If durable promotion is later approved, use the existing paired lineage/publication and recovery route; do not hand-edit canonical or generated files, and do not claim filesystem atomicity. Deselection must restore selected massing and remove only owned generated output. A failed candidate never becomes a fallback authored building or a permanent Terrain override.

## Completion criteria

The first implementation increment is ready for `PASS` only when:

1. The approved real building and a source-shaped fixture reconstruct from explicit recipes with an interior route and unique static details.
2. Real kit admission is target-independent and validates actual asset contracts and provenance.
3. Selected massing, exterior, interior, attachments, generated dependencies, and remaining massing have one deterministic lifecycle with proven no-op, locality, cold recovery, rollback, and deselection.
4. Actual saved/reloaded collision, floor/headroom, stair/path, required navigation, materials, and visual evidence pass on their intended acceptance surfaces.
5. Support is accepted without Terrain modification, or any demonstrated correction passes its canonical and native dependent-consumer proofs with exact outside preservation.
6. No unsupported shape, interactive object, source asset, unrelated building, or protected authored content is silently changed.
7. Required stable documentation and provenance records are updated, the final diff matches the declared owners, and independent R1/R2 findings are closed.
8. All claims are bounded to the evidence. City admission, cross-cell/multivolume coverage, interactive placement, packaged behavior, and release certification remain unclaimed unless separately proved and authorized.

## Review record

### 2026-11-17 - Initial architecture investigation

- **Trigger:** operator requested a critical architecture assessment and detailed mock plan beginning with the bounded Assembly backlog item.
- **Investigation:** inspected current Assembly/World code, schemas, tests and lifecycle scripts; canonical building and Terrain contracts; gameplay placement; ProjectObject building/interactive content; the related vendor-array backlog; and installed UE 5.8 PCG grammar/interop source.
- **Finding:** the transaction and ownership foundation is reusable, but the rectangular four-part planner is not a viable detailed-building model. A recipe/kit/plan split is required.
- **Decision:** no implementation decision is authorized. The recommended architecture and all first-slice defaults remain proposals under A1-A6; Q1 and conditional Q2 remain open.
- **Verification:** read-only `rg`, file reads, `git status`, `git log`, and `git show` only. No tests, builds, Unreal execution, asset mutation, generation, or promotion were run. Pre-existing generated-world and TODO working-tree changes were left untouched.
- **Artifact:** `tmp/alis-benchmark/candidate.md` was absent in this checkout and was created as the sole writable output from the supplied backlog starting point.
