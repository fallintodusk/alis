# Data-driven building reconstruction: architectural investigation and mock plan

Status: REVIEW REQUIRED. Proposal only; no implementation is authorized by this document.
Investigation date: 2026-11-17.
Starting point: `todo/02_backlog/world/20261112-1332_world_extend_building_assembly.md`.

## Recommendation

Retain the existing World buildings projection and its transaction. Replace the
prototype's implicit rectangle-to-four-box-families recipe with a small,
validated building-description contract: explicit stories, facade runs,
openings, circulation and named detail placements, consuming admitted modular
kits. Keep geographic qualification in World and architectural composition in
ProjectBuildingAssembly. Treat Terrain adaptation as an upstream canonical
operation, followed by fresh support qualification, never as a side effect of
spawning a building.

The original TODO is useful as a bounded lifecycle follow-up, but insufficient
as a plan for the requested reconstruction capability. Its retained items do
not define architectural data, usable interiors, real module fitting, or the
ordering of building intent and Terrain correction. Expanding the selection
limit or accepting more footprint shapes alone would scale the prototype's
limitations.

First prove one reference-backed, enterable, two-story building with a stair,
an entrance connected to exterior ground, repeated architectural modules and
one distinctive detail. Then prove reuse on another building. Introduce a
bounded Terrain correction only where the selected case actually needs it.
This is a vertical implementation sequence, not a city-wide grammar project.

## Contents

- [Authority register](#authority-register)
- [Investigation scope and evidence](#investigation-scope-and-evidence)
- [Current architecture](#current-architecture)
- [Assessment of the starting TODO](#assessment-of-the-starting-todo)
- [Proposed ownership and contracts](#proposed-ownership-and-contracts)
- [Architectural planning](#architectural-planning)
- [Terrain adaptation](#terrain-adaptation)
- [Native engine selection](#native-engine-selection)
- [Identity, lifecycle and runtime](#identity-lifecycle-and-runtime)
- [Required invariants](#required-invariants)
- [Mock implementation sequence](#mock-implementation-sequence)
- [Verification plan](#verification-plan)
- [Documentation and rollout](#documentation-and-rollout)
- [Open questions and completion](#open-questions-and-completion)

## Authority register

### Operator decisions

- D1: Investigate current ALIS World architecture from the specified backlog
  task; critically assess a maintainable approach to data-driven modular
  reconstruction including interiors, unique details and necessary Terrain
  adaptation; develop a detailed mock implementation plan.
  Reason: not stated. Source: current operator request.
- D2: Only this candidate document is writable. All other files are read-only;
  no implementation, tests, Unreal execution or project changes.
  Reason: not stated. Source: current operator request.

The standing Assembly decision record describes an accepted project direction.
It is evidence of existing intent, not a new operator decision manufactured
from this investigation. Recommendations below remain proposals under D1.

### Assumptions

- A1 ACTIVE: Reconstruction means source-constrained architectural credibility,
  with explicit approximation where evidence is absent. It does not imply
  surveyed interior accuracy from OSM or exterior photographs.
- A2 ACTIVE: Editor-time generation and saved native output remain the target;
  runtime building generation and structural destruction are not initial goals.
- A3 ACTIVE: A bounded initial class can exclude basements, curved walls and
  overlapping elevated volumes while the architecture leaves their authority
  questions explicit. This is sequencing, not a claim that the full requirement
  is satisfied by rectangles.

Questions Q1-Q5 are recorded at the end with their implementation consequences.
None blocks this read-only deliverable.

## Investigation scope and evidence

Initial repository state inspection reported no tracked changes. Final inspection
showed unrelated staged ProjectMaterial changes and its active material task;
they were left untouched and were not reviewed. The stated disposable copy did
not exist at the requested path when inspected; this document was created there.
No alternate TODO or report was written.

Evidence is static source/schema/document inspection, including selected
installed engine source. Existing tests were read, never executed. Their
assertions describe intended coverage, not a fresh passing result. No city,
asset usability, visual quality, collision, timing, or release readiness was
verified at runtime. No production canonical bundle was materialized to select
or certify a new neighborhood.

Paths in the evidence register are repository-relative; links resolve from this
scratch document. Symbols identify the relevant behavior without unstable line
number promises.

| ID | Inspected evidence | What it establishes |
|---|---|---|
| E1 | [World structure](../../Plugins/World/ProjectWorld/docs/architecture/structure.md), [territory router](../../Plugins/World/ProjectWorld/docs/territory_generation.md), [territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md) | Authority split, separate producers, single building projection and admission obligations. |
| E2 | [Assembly standing direction](../../Plugins/World/ProjectBuildingAssembly/docs/decision_record.md), [current structure](../../Plugins/World/ProjectBuildingAssembly/docs/architecture/structure.md) | JSON building descriptions and modular kits are the accepted direction; implemented capability is narrower. |
| E3 | [Plan types](../../Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Public/ProjectBuildingAssemblyPlan.h), [Build](../../Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyPlan.cpp) | Actual portable input, module families, placement rules and refusal behavior. |
| E4 | [Kit reader](../../Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyKit.cpp), [kit schema](../../Plugins/World/ProjectBuildingAssembly/Data/Schemas/modular-kit.schema.json) | Original procedural geometry only; strict closed reader; hardcoded admission probe. |
| E5 | [World assembly Prepare](../../Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingAssembly.cpp) | Geographic and support qualification, fitting, water refusal and transform construction. |
| E6 | [Selection](../../Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingSelection.cpp), [selection schema](../../Plugins/World/ProjectWorld/Data/Schemas/project_world_building_selection.schema.json) | At most five feature/kit records; owner-cell input hashing and qualification. |
| E7 | [Native realization](../../Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyRealization.cpp), [public realization](../../Plugins/World/ProjectBuildingAssembly/Source/ProjectBuildingAssemblyEditor/Public/ProjectBuildingAssemblyRealization.h) | Four ISM components, generated box/frame meshes, actual-output matching and persistent/isolated update paths. |
| E8 | [Selection persistence](../../Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingSelectionPersistence.cpp), [producer](../../Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingLayerProducer.cpp), [massing builder](../../Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingMeshBuilder.cpp) | Shared assets, stable external actor identity, capture, retirement, classification before suppression, material-only refusal. |
| E9 | [Canonical buildings](../../tools/World/CanonicalCompilation/app/buildings.py), [compiler boundary](../../tools/World/CanonicalCompilation/README.md) | Logical-building/part association, effective volumes, complete-parts versus outline fallback, source provenance. |
| E10 | [Overlay schema](../../tools/World/CanonicalCompilation/contracts/authored-overlay.schema.json), [base surface finalizer](../../tools/World/CanonicalCompilation/app/terrain_surface.py) | Provenance-bound radial deltas and Water-clearance enforcement during compilation. |
| E11 | [Refinement structure](../../tools/World/CanonicalCompilation/refinement/docs/architecture/structure.md), [authored dry refinement](../../tools/World/CanonicalCompilation/refinement/app/authored_terrain.py), [civil preservation](../../tools/World/CanonicalCompilation/refinement/app/civil_grade.py) | Immutable-base candidate construction; restrictions on dry corrections and retained civil support. |
| E12 | [Host regression](../../Plugins/World/ProjectBuildingAssembly/Tests/plan_test.cpp), [native and lifecycle routes](../../Plugins/World/ProjectBuildingAssembly/Tests/README.md), [lifecycle runner](../../Plugins/World/ProjectBuildingAssembly/Tests/run_lifecycle.ps1), [selection-input test](../../Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/Tests/ProjectWorldBuildingSelectionInputsTest.cpp) | Existing test definitions and their bounded acceptance claims. |
| E13 | [Neighborhood fixture](../../Plugins/World/ProjectWorldTestData/Data/Fixtures/building_neighborhood/README.md), [Assembly pitfalls](../../Plugins/World/ProjectBuildingAssembly/docs/pitfalls.md), [World pitfalls](../../Plugins/World/ProjectWorld/docs/pitfalls.md) | Synthetic fixture scope; cold reconstruction, save/retirement, orientation and physical-support failure history. |
| E14 | [World Partition contract](../../Plugins/World/ProjectWorld/docs/world_partition.md), [test layers](../../docs/testing/world_pipeline_layers.md), [architecture principles](../../docs/architecture/principles.md) | Native reuse rule; grid separation; no HLOD; proof depth, locality and promotion boundaries. |

The inspected Assembly plugin declares only an Editor module and cannot contain
content. Its module depends on native mesh/editor facilities and has no World
dependency. ProjectWorldBuildingEditor explicitly depends on Assembly and
ProjectWorldEditor. Preserve this composition direction; do not interpret
generic interface guidance as a reason to move these concrete editor types
into ProjectCore.

## Current architecture

### Authority and flow: verified

```text
SourceIngestion: admitted provider evidence
  -> CanonicalCompilation: logical buildings, effective volumes, final surfaces
  -> ProjectWorld editor: classification, source/support qualification, selection
       -> ProjectBuildingAssembly: neutral layout -> native static geometry
       -> remaining building massing
  -> existing World layer inventory, save/recovery and active authority
  -> saved native actors/assets consumed by World Partition/runtime
```

World tooling and reusable Unreal logic are separate owners. Concrete profiles,
authored territory inputs and generated packages belong to WorldData;
permanent synthetic inputs belong to TestData. MeshPartition is isolated behind
ProjectWorldMeshTerrain. Assembly neither determines geographic truth nor writes
Terrain. ProjectMaterial owns material resources; World resolves and authenticates
their references rather than compiling material graphs (E1, E14).

The compiler already has richer building semantics than Assembly consumes:
logical feature IDs, effective volumes with vertical ranges, relation/containment
association, and an explicit fallback when parts do not completely cover an
outline. Production association uses pinned native spatial operations; the
fixture-affine path has simpler fixture logic. Neither path establishes rooms,
stairs, actual floor elevations or facade module dimensions (E9).

### What the prototype actually does

- `Build` receives width, depth, height, support span and scalar kit parameters.
  There is no per-building architectural document in that input (E3).
- Story count is rounded from total height / preferred floor height, with at
  least one story. Actual story height is redistributed to fit the total.
  Wall bays are similarly divided to fit width/depth. This is acceptable for
  procedural boxes, not evidence that real art modules can be stretched (E3).
- The first bay of one ground-level side is omitted as an entrance. Windows
  alternate by a scalar frequency. Floors and roof are complete rectangular
  slabs; there are no partitions, door definitions, stairs or stairwell holes.
  A multistory count is not an enterable multistory building (E3).
- The foundation slab extends from below the support minimum to its top near
  the support maximum. That handles a bounded height span; it does not prove
  that the entrance is reachable from nearby exterior ground (E3, E5).
- World requires one ordinary volume, zero minimum height, one polygon without
  holes, four rectangular corners strictly inside one owner cell, and no
  civil-grade/constructed-surface exception. It fits a slightly inset rectangle.
  It rejects feature-anchored authored content and unresolved Water bounding-box
  overlap. Broad-phase Water rejection is conservative, not an exact overlap
  model (E5).
- Foundation qualification intersects physical Terrain triangles with the
  rectangle, sums overlap area and evaluates height extrema. This is stronger
  than center sampling. Its area equality is interpreted against an already
  admitted surface; it is not by itself a proof against arbitrary overlapping
  and missing triangles cancelling in the area total (E5, E14).
- The reader admits only the original procedural generator and provenance,
  then invokes `Build` on 10 x 10 x 6 m. Kit validity is therefore entangled
  with one synthetic building. It is not an external-mesh catalog (E4).
- Realization assumes exactly four part families, four components, one assigned
  material, unit-generated box/frame geometry and restricted instance yaw.
  Persistent updates also expect those four component names. Replacing only
  the planner would leave these downstream assumptions intact (E7).
- Native output uses query/physics collision; generated modules disable mesh
  navigation data and persistent components cannot affect navigation. The
  registered producer's settings require `no_navigation`. AI-ready interiors
  would change that contract (E7, E8).

### What is worth retaining

The complete classifier runs before selected massing is suppressed. A selected
feature must have admitted massing; excluding it earlier could change unrelated
overlap/containment decisions. Selection capture refuses missing assemblies,
so suppression cannot be certified alone (E8).

Shared generated kit packages, stable feature-derived actor identity, per-cell
dirty planning, stale-input checks, actual output comparison, cold reconstruction
and deselection retirement are valuable existing seams. Actual reuse examines
meshes/transforms/material/collision, not just tags. External updates rely on the
caller's enclosing transaction; do not describe Assembly's own update as an
independently atomic disk transaction (E7, E8, E13).

## Assessment of the starting TODO

| Retained item | Assessment and necessary development |
|---|---|
| Select a real neighborhood | Keep, but begin with a documented building/reference set and an explicit fidelity goal. Geographic qualification, architectural authoring and art admission are different work. |
| Add unsupported footprint/support case when needed | Keep demand-driven extension. Separate footprint topology, effective-volume interpretation, support-domain coverage and runtime partitioning; one generic 'complex footprint' task hides four contracts. |
| Replace the 10 x 10 x 6 m probe | Required before general intake. Split intrinsic kit validation from target-specific fitting and from native resource admission. Do not just remove validation. |
| Material-reference-only adoption | A concrete current refusal, not an optional workaround to ignore when adopting multi-material art. Keep its independent reference-only acceptance and recovery path. |
| Measure before caching | Keep. Existing kit sharing already avoids duplication. No evidence supports a new asset cache, scheduler or general incremental compiler. |
| Preserve lifecycle proof | Keep and extend to openings, stable architectural identities, removed modules, corrected Terrain and new resource ownership. The old fixture remains a control, not a city/interior certificate. |

Additional required design decisions are building-description ownership,
evidence-versus-inference, architectural connectivity, module fit policy,
stable override targets and acyclic Terrain preparation.

### Premise / KISS gate and alternatives

The existing building projection already owns replacement and persistence.
Add architectural source data and planning where that responsibility is missing;
retain the lifecycle whose failure modes have concrete regression coverage.
This adds no service, scheduler, runtime generator or independent transaction.
It knowingly gives up automatic reconstruction of arbitrary buildings from
incomplete geographic evidence.

A larger procedural-box recipe would be cheaper but cannot express the requested
interiors and unique architectural constraints. One hand-authored building mesh
per feature conflicts with the standing modular direction and weakens reuse.
A general grammar/constraint framework is premature before two concrete cases.
Native composition alternatives remain viable and are evaluated below rather
than dismissed in favor of familiar custom code.

### Findings: evidence -> consequence -> smallest safe response

1. E3 and E7 encode fitting by free scaling and four families throughout the
   planner and realization. Real kits would either distort or require scattered
   special cases. Introduce measured module descriptors and explicit fit rules,
   then make realization group resolved module placements rather than expand
   the enum once per architectural ornament.
2. E3 produces slabs without vertical circulation. Calling that output an
   interior would certify inaccessible floors. Introduce explicit openings and
   circulation, with a physical route as acceptance; do not use room count as
   its substitute.
3. E10/E11 expose circular additive corrections with deliberately restricted
   composition. They cannot be assumed to flatten a footprint or connect a
   threshold. First test existing correction semantics against the selected
   requirement; add one bounded model only when that requirement is unmet.
4. E5/E6 bind support and replacement to one owner cell while E9 admits more
   general features. Removing the strict-inside check would omit support and
   suppression dependencies. Extend the explicit affected-cell set together
   with qualification, hashing, retirement and classifier integration.
5. E12/E13 retain source-shaped synthetic lifecycle coverage and a captured
   Manhattan rectangle. These do not establish neighborhood variety, bare-earth
   accuracy or production streaming. Add evidence at the actual selected fidelity
   and envelope without discarding existing focused controls.

These are capability gaps relative to D1, not assertions that the intentionally
bounded prototype is defective for its original contract.

## Proposed ownership and contracts

### Boundary declaration

Owning black box: the existing World buildings projection composes geographic
qualification with the Assembly architectural compiler and native realization.
Its public input boundary remains authenticated canonical data plus profile-owned
building selections and admitted resources. Assembly receives neutral local
architectural/support constraints, never a World bundle or provider tags.

Expected CHANGED in a future approved implementation:

- Assembly building/kit schemas, reader, portable planning and native matching.
- World building selection, qualification, hashing, persistence and verification.
- TestData architectural fixtures and their focused preparers/lifecycle checks.
- Canonical correction contracts and constructors only if a selected support
  requirement demonstrates the gap; existing correction publication remains owner.
- Presentation binding only for admitted multi-slot/reference-only requirements.
- Stable owning docs and generated output revision declarations.

Expected UNTOUCHED for the first architectural slice:

- Provider acquisition, source pins and canonical building-part association.
- MeshPartition internals and Terrain realization algorithms.
- World transaction/locking/recovery framework and active authority format.
- Runtime save, inventory, interaction, character and destruction systems.
- Production bundles, generated cities, HLOD policy and release packaging.

Playable doors, navigation or below-ground excavation may invalidate that
untouched set. Reassess those boundaries before implementation; do not silently
absorb their systems into Assembly.

### Authored data versus generated data

Proposed authoritative inputs:

| Concern | Owner and meaning |
|---|---|
| Canonical feature and effective volumes | Existing canonical owner. Geographic envelope and evidence, with accuracy limits retained. |
| Selection and feature-to-description binding | WorldData for territories; TestData for fixtures. World validates existence, protection and complete replacement. |
| Architectural building description | Concrete data owner, e.g. its `Data/BuildingAssembly/Buildings/`; schema and portable semantics owned by Assembly. No city-specific architecture in reusable C++. |
| Reusable architectural templates | Assembly-owned reusable data only when actually shared; explicit bounded template expansion with per-building overrides. No deep inheritance tree. |
| Kit/module catalog | Assembly-owned contract; reusable original procedural definitions may remain in Assembly data. External mesh/material assets stay with their admitted art/resource owner. Concrete catalog records live with that resource/data owner. |
| Terrain correction input | Canonical correction owner, bound to building intent and immutable surface revision. Never encoded in generated actor transforms. |
| Generated plans, actors and procedural module assets | Disposable projections and diagnostic plans; existing World inventory owns generated territory artifacts. |

The standing direction's planned Assembly `Data/Buildings/` path does not yet
implement concrete city description ownership. Clarify that record when this
proposal is accepted: reusable definitions can live there, while a specific
city's per-feature descriptions belong to its data plugin. Do not maintain two
independent copies of a building description.

### Minimal contract contents (proposal, not implemented schema)

Use ordinary versioned JSON and closed schemas. Distinguish an architectural
building description from an Unreal Blueprint asset. Each JSON data file has
the project-required relative `$schema` reference.

1. **Building description:** stable description ID; binding-independent local
   frame; explicit story elevations; referenced kit; named volume/wing bindings;
   facade runs and ordered bays; entrances; slab/opening topology; partitions;
   stair/landing connections; named detail placements; evidence annotations and
   declared inference. Include an explicit interior disposition, such as
   enterable extent versus inaccessible shell.
2. **Module descriptor:** stable module ID; procedural recipe or admitted mesh
   reference; measured bounds and pivot/basis; nominal dimensions; connection
   planes; material slot semantics; collision role; allowed variation/stretch
   axes and bounded limits; resource provenance. Bounds alone are not snap planes.
3. **Qualified World input:** stable canonical feature/volume references; local
   metric geometry and vertical envelope; frame/quantization; resolved base
   elevation; authenticated final support and access constraints. World resolves
   canonical-to-local placement once. Assembly operates in local metres.
4. **Resolved plan:** named placements with module ID, transform and resolved
   bindings; floor/slab/opening geometry where needed; logical access connections;
   required support/contact domains; diagnostic refusals. It contains no owning
   disk paths, World actor pointers or geographic correction commands.

Do not store the same facade wall both as a raw placement list and as an
independently editable grammar expansion. The description is source; expanded
placements are derived. If a native authoring frontend exports JSON, require
an explicit export/import operation and one authoritative version, not live
bidirectional synchronization.

Intrinsic kit checks cover finite dimensions, unique IDs, complete references,
pivot/frame validity, legal fit rules and bounded capacities without a target
building. Target admission then proves actual spans, openings, joins, clearances
and resource compatibility. Native resource inspection separately proves that
the loaded mesh/material/collision matches the descriptor. Missing resources
must refuse before suppression; diagnostic fallback cubes are not accepted art.

## Architectural planning

### Fidelity and authoring

Start from the chosen source footprint and available references. Annotate facade
orientation, floor evidence, entrance positions, roof form and visible unique
details. Record which interior layout is inferred. Photographs constrain visible
appearance; they do not establish hidden rooms or absolute ground elevations.
OSM `height` or a fallback volume does not establish precise story counts.

Author one compact description, preview its resolved plan, then iterate that
description or its kit. Never manually repair generated actors. Repetition
belongs in a reusable story/facade template only after two actual cases establish
the shared pattern. Explicit local detail placements handle exceptions.

Reject a fit that exceeds the canonical/evidence envelope under the admitted
policy. If architectural evidence contradicts an approximate source height,
resolve it through an explicit owning input correction and provenance; do not
quietly enlarge the envelope in the spawner. For balconies/cornices extending
beyond massing, require a declared bounded projection allowance and collision
clearance check. A blockout footprint is not automatically a cadastral boundary
or permission for arbitrary overhangs.

### Staged deterministic planning

```text
validate description + intrinsic kit
  -> resolve World frame, envelope and explicit architectural bindings
  -> reserve entrances, circulation, landings and required openings
  -> expand explicit stories/facade bays
  -> solve bounded module fitting and corner junctions
  -> resolve partitions, shared walls, openings and circulation
  -> add roof, foundation and named details
  -> validate complete geometry/access/support requirements
  -> immutable plan OR structured refusal
```

Proposed local functions have these responsibilities; names are illustrative:

- `ValidateKitDefinition`: no arbitrary target probe.
- `ResolveArchitecture`: deterministic template expansion and named references.
- `PlanFacadeRun`: module selection/fitting for one bounded run.
- `PlanInterior`: partition/slab openings and declared circulation.
- `ValidatePlan`: containment, contacts, clearances, capacities and graph checks.
- World `QualifyBuilding`: geographic, authored, Water and final support checks.
- Native `ResolveResources`, `Apply` and `Matches`: admitted resource references,
  exact generated population and actual saved output.

These are cohesive functions/files, not proposed new services or plugin layers.
Keep one production planning implementation shared by host and Unreal checks
where practical. If a chosen native geometry/grammar operation needs Unreal,
isolate that operation and test it natively; do not duplicate it in a host clone
merely to preserve the existing test arrangement.

### Module fitting

Use nominal modules with explicit end/corner/closure pieces. Fix important
stations first: entrances, corners, stair interfaces and visible reference
features. Fill the remaining declared spans using the kit's admitted choices.
Allow scaling only on designated axes and within admitted limits; unit scale
is the default for doors, stairs and distinctive trim. A residual gap is a
fit refusal unless an admitted closure piece or bounded stretch can consume it.

Do not add a general constraint solver initially. For the first kit use explicit
bay sequences or a bounded run subdivision. If automatic fitting is needed,
define deterministic tie-breaking and capacity limits. Evaluate native PCG
subdivision before implementing a custom combinatorial fitter.

Normalize polygon orientation and local facade names independently of raw ring
start index. Bind an authored entrance to a named facade/metric station, with
explicit geometric matching tolerance. A changed footprint that invalidates a
binding requires rebase/refusal. Silently moving the entrance to another side
is worse than rejecting stale data.

### Interiors

The minimum useful interior has a route from exterior ground through a real
opening to occupied rooms and the declared upstairs destination. Provide:

- Explicit floor elevations and headroom; do not recompute them by rounded height.
- Room/partition boundaries with shared-wall ownership, avoiding coincident walls.
- Door and window openings cut into the owning wall assembly. Frame plus solid
  wall behind it is a failed opening, even if the frame looks correct.
- Stair footprint, landings, upper-slab hole and headroom exclusion. Slabs and
  stairs must agree at the opening edges and walking surfaces.
- Structural floor/ceiling/wall collision with intended facing on both occupied
  sides. Visual double-sidedness is not a collision or lighting proof.
- An access graph for early validation, followed by actual capsule traversal.
  Graph connectivity does not certify collision, step height or slope behavior.

An economical first implementation can use kit floor tiles and authored opening
tiles to form a stairwell, plus explicit partition runs. It need not introduce
arbitrary polygon CSG. If the selected footprint requires polygon floors, use
native triangulation with holes and exact boundary validation; do not replace
holes with overlapping solids.

Do not infer a room layout for every effective volume independently. Stacked
volumes, shared floors and touching wings need a single architectural description
of interfaces. Start with explicitly authored compatible wings if required;
general footprint decomposition is a later capability backed by real cases.

### Unique details and interaction

Preserve identity through named bays, entrances, roof/detail anchors and module
placements. A distinctive doorway, cornice, balcony, sign mounting or roof profile
can be assembled from reusable pieces or an original procedural detail recipe.
Unique composition does not require a monolithic hand-built building.

Use bounded explicit overrides: replace a named bay, choose a trim variant, add a
detail at a named anchor. Validate target existence and prevent two overrides
from claiming the same structural span. Avoid arbitrary scripts embedded in JSON.
If variation is needed, derive its seed from stable building/placement identity
and an explicit variation version; adding one detail must not reshuffle others.

Static geometry belongs to Assembly output. A working door, loot container,
light behavior or destructible object belongs to its gameplay/runtime owner.
Initially emit stable placement anchors and prove static enterability; do not
invent a gameplay spawn API without investigating the actual owning contract.
Persistent runtime state must key to logical IDs, not ISM indices or generated
actor/component allocation order. Runtime persistence integration is unverified
and outside the first slice, not implicitly supplied by editor generation.

## Terrain adaptation

### Three distinct problems

1. **Bad Ground source:** A DSM building/canopy hump is not a normal foundation
   grading request. Qualify bare-earth or an explicit canonical inference with
   its own provenance. Do not bury an uncorrected source-role defect under a
   large foundation or grade away an entire building-shaped observation.
2. **Architectural support:** Choose an admitted base level, foundation span,
   steps, plinth or retaining structure. Small natural relief may need no Terrain
   change. A vertical retaining body is separate geometry, not a steep fake
   Terrain wall or an XY height-field constructed surface.
3. **Necessary Ground adaptation:** A bounded pad, threshold approach or apron
   changes canonical Ground. Its correction must prove seams, neighboring
   support and Water/civil constraints before the building uses it.

The current max-support anchor can raise a floor above nearby exterior ground.
Measure entrance threshold-to-ground step and the full approach corridor;
foundation contact alone cannot accept the route.

### Acyclic preparation: proposed sequence

```text
immutable accepted Ground G0 + canonical building + authored architectural intent
  -> resolve one explicit base/entrance level and required support domains
  -> existing canonical correction OR one bounded new correction constructor
  -> immutable candidate Ground G1 and affected-consumer qualification
  -> final building plan and actual support/access checks against G1
  -> existing paired canonical/World realization and recovery route
```

The pre-correction intent is stable input, not a mesh measured after realization.
For a derived base level, declare the exact statistic/domain on G0 and freeze
that result in the prepared operation. Final placement verifies it against G1;
it must not sample a new anchor and silently move the building after grading.
Changing the description/base choice creates a fresh candidate from G0 or an
explicit rebase. Never repeatedly feed G1 back as the next unqualified source.

Keep base, prepared operation and final surface identities separate. A building
description can declare support requirements; CanonicalCompilation owns their
terrain interpretation and output. If portable building preparation supplies an
intent document, pass it through a public input schema. Do not import Unreal
Assembly into the Python compiler or implement a second building solver there.

### Reuse before extension

The existing base overlay supports radial constant deltas and applies Water
clearance checks (E10). The inspected immutable dry-refinement path additionally
requires regular unchanged grids, no feature overrides, retained provenance,
and no intersecting Water or incompatible retained civil/constructed support
(E11). A radius covering a footprint does not imply that the result is planar;
constant deltas retain the original relief and can introduce boundary creases.

Use that route only if its actual semantics meet the selected support requirement.
Do not generate dozens of circles as an undocumented substitute for a graded
pad. If it fails, the smallest proposed extension is one explicit bounded
pad-and-approach correction under canonical refinement, with:

- Declared source revision, canonical frame, building/entrance binding, pad
  polygon, target elevation/plane, transition domain and cut/fill/slope limits.
- A final field represented by the existing canonical surface contract, with
  boundary stations and enough resolution to satisfy the actual threshold and
  transition tolerance. A tiny pad between coarse nodes must refuse or undergo
  admitted local refinement, never disappear without changing a sample.
- Exact affected-domain coverage and unchanged outer boundary; no duplicated
  boundary samples with divergent heights across cells.
- Explicit treatment of overlapping requests. Initially refuse incompatible
  overlap; compatible shared-domain preparation is deliberate input, not
  last-writer-wins or an unbounded priority system.
- Complete Water, Road/civil, constructed, nearby building and protected-anchor
  impact census using physical domains. Retained center anchors do not establish
  separation; World pitfall 70 is direct evidence of that failure mode (E13).
- Refusal without publication when cut/fill, access slope, conflicts or evidence
  limits cannot be met. No hidden fallback correction in Unreal.

Those are proposed requirements, not an existing generic influence API.
Select the numerical interpolation/native geometry implementation only after
the chosen domain and cheapest fixture establish what must be represented.
Do not widen Water/civil composition to make a pad pass; that is a separate
modeling decision requiring its own bounded proof.

### Excavation limit

A below-ground room is not solved by hiding Ground visually under a floor.
Terrain collision would still occupy the room. The current inspected final
surface contracts do not establish arbitrary excavation holes, vertical cavity
walls or multiple Ground heights at one XY. Basements, tunnels and underpasses
require an explicit excavation/solid-boundary representation and consumer proof.
Keep them unsupported initially; do not advertise them as an extension of a
radial height patch. Above-ground interior slab holes are a separate, supported
architectural requirement in this proposal.

## Native engine selection

Selected installed source was inspected under `<UE_PATH>/Engine/` using the
repository engine configuration. Source evidence:

- `Plugins/PCG/Source/PCG/Public/Grammar/PCGGrammar.h`: native module grammar
  descriptors, size/repetition and scalability.
- `Plugins/PCG/Source/PCG/Public/Elements/Grammar/PCGSubdivideSegment.h` and its
  private implementation: segment subdivision with module indices, endpoints,
  finite remaining span, and explicit incomplete-fit policy.
- `Plugins/PCG/Source/PCG/Public/Elements/PCGStaticMeshSpawner.h`: native mesh
  selection/descriptors, material/component overrides and ISM spawning.
- `Source/Runtime/Engine/Public/PackedLevelActor/PackedLevelActor.h`: packed
  level actor contract; its header explicitly warns that other components can
  be unsupported and recommends ordinary Level Instances for those cases.
- `Source/Runtime/Engine/Public/LevelInstance/LevelInstanceActor.h`: world asset
  reference and native runtime behavior controls.

Official documentation consulted: [PCG shape grammar](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-shape-grammar-with-pcg-in-unreal-engine),
[Level Instancing](https://dev.epicgames.com/documentation/unreal-engine/level-instancing-in-unreal-engine?lang=en-US),
and [City Sample PCG](https://dev.epicgames.com/documentation/unreal-engine/city-sample-pcg-for-unreal-engine?lang=en-US).
The pages resolved to 5.9 despite 5.8-version requests. They are capability
references, not proof of the selected engine's compatibility. Local source
supports the narrower API observations above. No native experiment was run
under D2.

| Candidate | Architectural judgment | Required next evidence |
|---|---|---|
| Existing native ISM realization | Retain as the baseline projection for static modular output. It already fits World ownership; extend resolved descriptors and matching rather than write a renderer. | Multi-module/material grouping, saved collision and exact population. |
| Native PCG grammar/subdivision | Closest option for repeated facade runs; evaluate before writing a new fitter. It does not by itself solve architectural circulation or ALIS authority. | Same bounded facade inputs, ordered output, fit refusal, deterministic reconstruction, transaction-owned output. |
| PCG mesh spawner | Viable native projection candidate if it can emit under the existing layer's ownership. Existing direct ISMs may remain simpler for already-resolved placements. | Managed-resource creation/removal, stale/no-op validation, save/reload and rollback integration; no second autonomous output lifecycle. |
| Level Instance / Packed Level Actor | Useful comparison for repeated complex static assemblies or authored protected content. Do not create one source map/Blueprint per building without demonstrating benefit. Generated versions must derive from JSON; static packing cannot stand in for interactive contents. | Asset/dependency count, supported components, deterministic rebuild and actual streaming behavior. |
| Native geometry utilities | Reuse for polygon operations/triangulation where the selected topology needs them; World already depends on GeometryCore/GeometryAlgorithms. | Holes, concavity, touching edges, native precision and collision. |
| Landscape Patch | Different backend from accepted Mesh Terrain; no proposed adoption for building grading. | A backend change would be a separate architecture decision. |

City Sample is a useful authoring reference, not a production admission template:
its official documentation explicitly describes disabled streaming by default
and unpartitioned PCG-generated actors. Its existence does not prove ALIS
interiors, city streaming or recovery.

There is no evidence here that PCG is unsuitable. The proposed owner/JSON
contract is independent of that backend choice. Defer automatic run-fitting
implementation until the native comparison; retain explicit sequences meanwhile.
Any custom exception must name an observed unmet invariant, not portability
preference, familiarity or the feature's maturity label.

## Identity, lifecycle and runtime

### Input identities and dirty scope

Separate action/provenance identity from consumed semantics. Authenticate exact
source and resource bytes for admission, but do not make a documentation edit,
provenance annotation or template reserialization rebuild every building.

Proposed semantic dependencies include normalized resolved architecture,
consumed kit module geometry/pivots/fit rules, geographic envelope/frame,
final physical support and access domains, collision policy and output contract
revision. Resource provenance and current producing implementation are retained
for acceptance without automatically becoming cross-owner geometry keys.

Current selection hashing includes the entire kit file hash (E6). When general
kit intake introduces metadata and multi-module catalogs, distinguish the
consumer contract projection from that authentication hash. A geometry-neutral
kit metadata change should not force geographic reconstruction. Do not claim
this granularity already exists.

For cross-cell support, enumerate all intersecting support/access/transition
cells and retain a deterministic canonical owner for logical building identity.
The owner's assembly input must consume every actual support cell. Suppress
the selected feature's fragments in every occupied massing cell, after the
complete classifier, not just where the actor is owned. Dirty/remove scope is
the union of old and new affected cells. A support-only neighboring cell can
invalidate the building owner even when no facade crosses it.

For one building, start with one spatial generated actor and grouped components
when its measured residency is acceptable. Canonical ownership cells are not
runtime grid cells. If a large building's bounds force excessive residency,
split generated geometry by a declared physical/spatial chunk key while keeping
one logical building and complete replacement set. Do not choose one actor per
module, one building per canonical fragment, or one interior streamer by default.
No HLOD introduction is proposed; the current World contract disables it.

### Resource and output ownership

Group static placements by resolved mesh, material bindings, collision and other
native component-descriptor requirements. A fixed four-component population is
no longer the contract. Stable logical placement IDs survive grouping; ISM
instance indices are transient storage.

Generated procedural kit assets remain shared, manifest-owned World output.
Imported mesh/material resources remain owned by their art/resource provider;
World authenticates references and dependencies without taking ownership or
deleting source assets when the last building is deselected. Retire only owned
generated assets after a current referencer/population census. Avoid a catalog
path allowlist permanently restricted to the original Assembly test kit folder.

Actual no-op validation must compare descriptor bindings, mesh identity,
component/instance transforms, collision, ownership and exact expected module
population. Imported meshes cannot be validated by the current generated-box
triangle comparator. Admit a resource identity appropriate to imported content
and inspect actual saved references/configuration; preserve direct shape checks
for generated procedural modules.

Extend the existing material-reference-only path when the first admitted kit
needs it. Multiple facade/interior slots require deliberate binding semantics;
do not multiply global role names per building or couple World to a material
compiler. Reference-only adoption must preserve geometric payloads and physical
behavior and refuse unsupported bindings before writes.

### Transaction and runtime obligations

Preflight the entire selected batch and its resources, then apply geometry and
complete massing replacement through the existing layer transaction. Detect
stale descriptions/kits/support between planning and apply/capture, including
selection removal. A failed new description must not silently restore or retain
unreported partial geometry.

Canonical correction and Unreal output are two authorities coordinated by the
existing paired route. Do not promise a new cross-filesystem atomic operation:
retain its journal/snapshot/recovery guarantees, authenticate both sides and
test failure after the first durable mutation on either side. A correction-only
candidate does not authorize activation or enrollment.

Current roof rays and static scene comparisons do not qualify player or AI
access. Prove actual capsule movement, contact and passage through entrances,
stairs, landings and rooms. World pitfall 56 explicitly distinguishes building
reference rays from the possessed capsule's collision response (E13).
Navigation is optional only if the selected fidelity excludes AI access; enabling
it requires changing and proving the current no-navigation contract. Saved
editor output does not by itself certify cooked physics or packaged residency.

## Required invariants

| ID | Invariant |
|---|---|
| I1 | Geographic evidence, architectural inference and art provenance remain distinguishable; no silent correction or guessed survey claim. |
| I2 | One authoritative architectural description; generated output is reproducible and contains no unique manual truth. |
| I3 | Admitted fits respect nominal module rules, envelope/projection limits, shared interfaces and capacities; failures return no partial accepted plan. |
| I4 | Every declared enterable destination has a valid opening/circulation route and actual player collision clearance. |
| I5 | Foundation and entrance/approach domains have complete accepted final support, including all relevant cells. |
| I6 | Canonical corrections are acyclic, bounded and conflict-aware; seams and protected neighboring support remain valid. |
| I7 | Selected geometry and all selected massing fragments replace together; unselected classifier outcomes and authored content remain intact. |
| I8 | Identical semantic inputs reconstruct equivalent output; geometry-neutral provenance changes preserve consumer output identity. |
| I9 | Exact owned population is verified; missing output reconstructs; removal retires only owned generated artifacts. |
| I10 | Refusal/recovery preserves prior authority, package path/hash population and protected inputs, including prior absence. |
| I11 | Runtime identity, collision, residency and any admitted navigation are proved in the actual consuming envelope. |

## Mock implementation sequence

All steps below are future work, not commands to execute in this investigation.
Keep one active slice at a time; each slice has a reviewable output and exit
condition. Do not make every possible extension a prerequisite for slice 1.
The first case should use already sufficient Ground where possible. If its
entrance requires correction, bring slice 3's focused Terrain work forward
before slice 2's traversal acceptance. Do not accept an inaccessible interior
temporarily merely to preserve the numbering.

### 0. Freeze a bounded acceptance case and native comparison

- [ ] Select one real feature and its pinned canonical authority, actual volume
  topology, final Ground source role, surrounding approach and protected content.
  Use the captured Manhattan feature only if it meets the reference/interior goal;
  its existing proof is not evidence that it does.
- [ ] Gather admissible references and a tiny original/licensed kit. Record
  dimensions, provenance, permitted reconstruction fidelity and inferred rooms.
  Preserve an unselected neighbor and an unrelated cell as controls.
- [ ] Define the exterior-to-upper-room route, required distinctive detail and
  qualitative/reference review views. Establish measured native budgets for this
  case before assigning city-wide numerical promises.
- [ ] Compare native PCG subdivision/spawning with direct resolved placements on
  that facade. Evaluate Level Instance/packing only if repeated composite content
  makes it relevant. Record real fit/refusal, ownership and reconstruction results.
- [ ] Produce a permanent source-shaped fixture and exact expected properties,
  keeping preparation/output under the existing TestData/scratch route.

Exit: accepted case brief, explicit evidence/inference boundary and justified
backend choice. No durable authority changes. Unresolved Q1/Q2 may block this exit.

### 1. General kit admission and explicit facade description

- [ ] Add Assembly-owned closed kit/building schemas and target-independent
  validation. Distinguish missing, unsupported and invalid inputs.
- [ ] Extend World selection to bind a feature to an architectural description
  and its data-owned catalog references. Validate confined paths, schema targets,
  duplicates, provenance and complete references before resource creation.
- [ ] Implement explicit stories, named facade runs/bays, nominal module fit,
  opening variants and one detail anchor. Retain the current original procedural
  kit as a valid control, not a second production mode indefinitely.
- [ ] Replace four-family native grouping/matching/update assumptions coherently.
  Preserve stable actor identity and actual-output no-op checks.
- [ ] Split kit authentication from consumed geometry projection and extend
  per-cell identity accordingly. Bump the owning output revision where output
  changes; do not edit generated manifests manually.

Expected files: siblings near `ProjectBuildingAssemblyPlan.*` for description,
kit admission and facade planning; existing realization adapter; World selection
and persistence consumers; owning schemas/tests. Names/number of siblings follow
actual cohesion, not one type per file. Check line counts before editing.

Exit: one reference-backed exterior, nominal fitting/refusal and unique detail;
saved native output and fixture lifecycle remain deterministic. Unsupported input
refuses before suppression. No claim of interiors yet.

### 2. Enterable interior and access

- [ ] Extend the same description/plan with partitions, shared boundaries,
  door openings, explicit floor elevations, stairs and upper-slab openings.
- [ ] Implement the minimum geometry required by the selected floor topology:
  opening tiles first if suitable, native hole-aware triangulation when needed.
- [ ] Validate access graph, headroom and stair interfaces before realization;
  retain physical capsule probes and real movement as independent acceptance.
- [ ] Add static interior material/collision roles. Qualify reference-only
  adoption if resource bindings now require it; preserve geometry bytes.
- [ ] Expose stable detail/gameplay anchors without creating runtime behavior.
  Decide Q3 before adding door state, loot or AI integration.

Exit: saved/reloaded real-route entry, stairs and upstairs destination work;
negative controls detect a closed doorway, blocked stair opening and insufficient
headroom. Reference views establish intended exterior/interior composition.

### 3. Necessary Terrain adaptation for the selected case

- [ ] Measure the uncorrected foundation and entrance/approach. Prefer a supported
  architectural step/plinth when it satisfies the goal without Ground edits.
- [ ] Attempt existing canonical correction semantics in an isolated fixture.
  If unsuitable, retain the failed requirement as evidence for one bounded
  correction model; do not weaken the current admission guards.
- [ ] Implement immutable pre-correction intent, fixed base elevation, full domain
  and boundary/seam validation in the canonical owner if needed.
- [ ] Prove Water/civil/neighbor conflicts refuse without output or publication.
  Use all physical footprints, including features owned outside the domain.
- [ ] Requalify final assembly support/access on the corrected candidate and
  exercise paired recovery through the existing World route.

Exit: the selected approach is physically usable, changed Ground is localized,
neighbors/seams remain valid and rollback restores both authorities exactly.
If no correction is necessary, record that result; this slice does not create
an influence framework merely to fill a roadmap box.

### 4. Reuse and the first necessary topology extension

- [ ] Apply the same kit/template to a second reference-backed building; change
  a distinctive feature through named data rather than conditional C++.
- [ ] Select the first genuine nonrectangular, cross-cell or multivolume case.
  Extend only the required domain, with explicit supported topology.
- [ ] For a cross-cell case, add complete support/affected-cell binding,
  owner-input invalidation, whole-feature suppression and old/new removal scope
  together. For multiple wings, define shared walls/slabs and story interfaces.
- [ ] Test ring-order stability and deliberate binding refusal when geographic
  changes invalidate authored station/volume references.
- [ ] Measure classification, planning, asset resolution, saves and runtime
  residency separately. Optimize only the measured bottleneck inside its owner.

Exit: demonstrated reuse without per-building code, focused evidence for the
new topology, unchanged neighbors and bounded runtime residency. City-wide
automatic decomposition and generalized grammar remain deferred.

### 5. Admission and production integration

- [ ] Update exact native verifiers and the affected EndToEndValidation expectations
  for the changed output revision and concrete admitted fidelity.
- [ ] Run owning tests and the selected L1 lifecycle, then common Check and the
  affected L2 Matrix mode selected from actual changed contracts.
- [ ] Review exact candidate, architectural fidelity, refusal evidence and locality.
  Stop before L3 enrollment for the operator's explicit production decision.
- [ ] After separate authorization, promote through existing wrappers and audit
  realized authority. Package only on an explicit release/package request;
  until then mark cooked behavior unverified.

Exit: production admission only for the concrete selected cases and fidelity,
not a general certificate for all kits, interiors or city geography.

## Verification plan

No verification commands below were run. Existing test names are real; proposed
case names describe new tests to add and must be registered before invocation.
Follow the exact-filter native lane, not a broad prefix.

| Changed behavior | Red/known-bad control to observe before implementation | Green evidence and layer | Invariants |
|---|---|---|---|
| General kit intake | A valid kit fitting the selected target but failing the old arbitrary probe; wrong dimensions/resource identity also reject | Intrinsic versus target admission host/native checks; parser/schema agreement, missing-resource refusal before writes (L0/L1) | I1, I3, I10 |
| Explicit architectural description | Current planner cannot honor fixed story elevations, a designated entrance and an unscaled named detail | Production planning tests with independent stations/bounds, deterministic expansion and capacity/refusal cases (L0) | I2, I3, I8 |
| Real module fitting | Oversized rigid door or unmatched residual span; legal bounded closure as positive control | Fitting tests plus saved native module/pivot/slot identity (L0/L1) | I3, I9 |
| Interior traversal | Solid slab across stairwell; doorway visually present but collision-sealed; low stair headroom | Topological checks plus native capsule sweeps and actual possessed-character route after reload (L0/L1, affected L2 gameplay) | I4, I11 |
| Terrain support | Missing triangle region, gap/overlap cancellation, support extremum between sample points; inaccessible entrance despite admitted foundation | Domain coverage/nonoverlap and extrema oracle on an analytic surface; actual final native contacts/approach (L0/L1) | I5 |
| Bounded grading | Tiny pad missed by coarse samples; correction exceeds cut/fill; crossing neighbor or Water/civil conflict | Canonical domain/seam/cut/fill/refusal checks, cold/operation-order equivalence, physical access on G1 (L0/L1) | I5, I6, I10 |
| Cross-cell assembly | Change only support in non-owner cell; move selection across boundary; deselect old crossing feature | Exact dirty owner/fragment set; all selected massing removed; old/new union retirement; unaffected controls unchanged (L0/L1) | I5, I7, I8, I9 |
| Unique overrides | Missing/duplicate bay anchor, geographic rebase invalidating facade binding | Explicit refusal; stable surviving logical IDs under unrelated detail insertion (L0) | I2, I3, I8 |
| Reference-only appearance | Swapped material/slot and stale resource input | Accepted bindings update without geometry/physics reconstruction; unsupported inputs leave bytes/population unchanged (L1) | I8, I9, I10 |
| Exact lifecycle | Missing actor/module with unchanged inputs; stale description after dirty planning; unowned feature actor | Retain no-op, stale-input refusal, cold reconstruction, capture and ownership cases; exact generated population (L1) | I7, I9, I10 |
| Recovery/removal | Fail after a real kit/actor save and after canonical-side mutation; remove last and non-last shared-kit user | Existing wrapper recovery plus paired authority tests, path/hash equality including prior absence; never retire imported source assets (L1/L2) | I7, I9, I10 |
| Appearance and residency | Inward module normals, floor gaps, missing detail, near-entry unloaded collision | Saved/reloaded player-level reference views, actual partition residency, native collision; cooked route separately at L4 | I1, I4, I11 |

The gap/overlap control challenges the stronger *generalized support validator*
claim. It is not a reproduced defect in the currently admitted canonical triangle
field. Use independent expected geometry, not the same clipping routine on both
sides of an assertion.

Useful existing regression entry points to retain:

```powershell
# Host layout/schema control; use the pinned World environment.
python -B Plugins/World/ProjectBuildingAssembly/Tests/run.py

# Fixture preparation, then existing registered lifecycle wrapper.
python -B Plugins/World/ProjectBuildingAssembly/Tests/prepare_lifecycle.py tmp/world/building_assembly/lifecycle_case
Plugins/World/ProjectBuildingAssembly/Tests/run_lifecycle.ps1 -CaseRoot tmp/world/building_assembly/lifecycle_case

# Inspect the complete candidate change closure before choosing L2 scope.
python -S tools/World/EndToEndValidation/bootstrap.py plan --base <slice-base>
```

Existing exact native tests include
`Project.World.BuildingAssembly.CanonicalPrototype`,
`Project.World.BuildingAssembly.SelectionSettings`,
`Project.World.BuildingAssembly.SelectionInputs` and
`Project.World.BuildingAssembly.SavedRender` (E12).
Their prepared inputs, environment authentication and safe serialized invocation
are owned by the linked Tests README; do not run them without those preconditions.

At an implementation slice exit, follow E14: World-only consumption may qualify
for canonical-authority Matrix; canonical-producing changes require their actual
producing or explicitly authenticated immutable-candidate boundary. No package
build for routine iteration. One initial expensive gate and one necessary
replacement is the default budget, not a discovery loop. A failure returns to
the first failing owner's focused case.

Visual fidelity is a reviewer/operator judgment against references and stated
inference, not a scalar screenshot test. A count of rooms/modules, matching hash,
green ray test or a bright diagnostic render cannot replace that judgment.

## Documentation and rollout

Future durable changes belong in existing owners:

- Assembly `docs/architecture/structure.md`: implemented architectural compiler,
  immediate parts, public semantics and explicit supported limits.
- Assembly `docs/decision_record.md`: update accepted target/data placement only
  after the architecture is approved; keep target versus implementation distinct.
- Assembly schemas/public types: exact machine-readable contract shapes; docs
  reference them instead of manually repeating every member.
- World `territory_contract.md`: revised selection/support/cross-cell replacement
  and resource ownership relationship. Its building module README routes to
  concrete integration owners.
- Canonical `terrain_refinement.md` and its existing child architecture/contracts:
  any admitted pad/approach model, ordering, limits and recovery implications.
- Assembly Tests README/TestData fixture owner: preparation, exact cases and the
  claims those cases actually prove.
- Relevant existing pitfalls: only newly reproduced nontrivial failure mechanisms.

Do not add this scratch plan to stable navigation or link it from living code.
It is disposable. No stable documentation was edited in this investigation.

Migrate the prototype contracts and their known consumers together once an
implementation is approved. Do not introduce legacy reflected fields,
CoreRedirects or a dual building pipeline for hypothetical content. If actual
tracked consumers require migration, enumerate and convert them in that change.
Original test-kit coverage can remain as an executable case under the current
contract without preserving obsolete production APIs.

All candidate native output stays isolated/reversible until its L3 decision.
Retain the current snapshot/recovery mechanism and verify exact restoration;
do not add another transaction framework. Rollback of an admitted candidate uses
its authenticated previous authority and owned population through the existing
route, not manual deletion or edits to manifests.

## Open questions and completion

| ID/status | Question | Consequence and proposed default |
|---|---|---|
| Q1 OPEN - NON-BLOCKING for investigation; required before case acceptance | Which first building/reference set and what interior fidelity are intended? | Recommend a small above-ground two-story case with a defensible inferred layout and one signature detail. Do not certify the old synthetic neighborhood as real geography. |
| Q2 OPEN - NON-BLOCKING | Must all selected buildings be enterable, or only declared extents? | Recommend explicit per-description enterable extent; complete traversal proof for every advertised destination. Evidence may require more authoring, not a larger generator. |
| Q3 OPEN - NON-BLOCKING; blocking before runtime integration | Are AI navigation, working doors, persistent loot or destruction part of first playable admission? | Default to static human traversal. Any requested behavior triggers investigation of its owning runtime contract and affected acceptance; no implicit support. |
| Q4 OPEN - NON-BLOCKING; blocking before kit admission | Which art kit/provenance and appearance target should the first reconstruction meet? | Prefer a tiny admitted kit and distinctive composition; do not infer usability from legacy asset names or download a general pack. |
| Q5 OPEN - NON-BLOCKING; blocking before corresponding terrain scope | Are basements/excavation required in the first case? | Default to above-ground interiors and bounded surface adaptation. Required excavation changes the surface/solid contract and needs its own architectural slice. |

The native backend comparison, geographic/source-role quality of the selected
case, actual art fitting, material-slot integration, gameplay behavior and
measured runtime budget remain unverified. These are named future work, not
reasons to invent infrastructure now.

Investigation completion means this document separates inspected behavior from
proposals, names direct owners/consumers, challenges the prototype assumptions,
and supplies an ordered implementation/proof path. Implementation completion
would require I1-I11 for the concretely admitted cases, focused regressions,
native saved/runtime evidence, affected gates and durable owning documentation.
That later completion has not been claimed here.

Review record: self-review against inspected contracts and D1/D2 only. No
independent architectural approval or runtime PASS is asserted.
