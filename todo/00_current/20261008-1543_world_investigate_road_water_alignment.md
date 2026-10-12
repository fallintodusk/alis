# Road and Water Alignment

**Status:** CURRENT - Q1 owner-local correctness implemented and verified; production water acceptance, I5/I6 and Q2/Q3 remain open
**Scope:** terrain/road/water geometric alignment, intersections, and layering  
**Release:** required 3.0.0 concern; release authority stays in the [release plan](00_release_3.0.0.md#authority-register)  
**Stable documentation owners:** [World territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md), [World pitfalls](../../Plugins/World/ProjectWorld/docs/pitfalls.md), and the owner identified by the dedicated investigation

Current coordination: BrownEarth's bounded city visual checkpoint is accepted
and frozen; remaining surfaces use isolated and transient city previews under
the presentation concern's D31-D33. Its separately authorized building-owner
facing correction has isolated proof; existing city meshes remain unchanged and
its eventual building-only update is not road/water alignment acceptance.
Alignment inputs remain unchanged. Keep this
todo's correctness, water-policy and grading/
crossing gates separate; material screenshots do not accept geometry or trigger
canonical/World regeneration.

## Contents

- Goal - Authority register - Non-goals
- Verified evidence - Current architecture - Problem and root cause - Decision
- Required invariants - Investigation tasks - Proposed implementation - Test-first and verification plan
- Documentation plan - Completion criteria - Review record

## Goal

Make road and water surfaces meet terrain coherently at edges, crossings, and
intersections, without gaps, unintended overlap, or unstable coplanar layering.
Preserve correct geographic meaning, collision, and streamed cell continuity.
Judge whether roads appear coherently constructed on slopes and at junctions,
as well as whether their mesh heights match terrain. Exact drape alone may
preserve an implausible cross-slope; grading remains a candidate, not a requirement.

## Authority register

### Operator decisions

- **D1** Keep geometric alignment and layering separate from material presentation.
  - Reason: its implementation design is not decided yet.
  - Source: operator request, 2026-10-08.
- **D2 [SUPERSEDED IN PART by D5 - read-only investigation timing]** Create a current investigation todo now; agree on the material todo first,
  then conduct this dedicated investigation separately.
  - Reason: not stated.
  - Source: operator request, 2026-10-08.
- **D3 [SUPERSEDED IN PART by D5 - bounded read-only editor evidence]** This step is planning/documentation only; do not implement or run expensive
  verification. Development later uses owner-local tests, scratch previews, and
  focused integration.
  - Reason: avoid repeatedly rebuilding/cooking/regenerating the World for debugging.
  - Source: operator request, 2026-10-08.
- **D4** Audit the attached alignment review against actual code, architecture,
  project goals, and previous agreements; fix these todos and explicitly refute
  incorrect reviewer points.
  - Effect: revise this existing investigation, its presentation handoff, and release
    routing. This source audit does not approve an alignment implementation or
    supersede D2/D3's dedicated-investigation sequence and documentation boundary.
  - Reason: not stated.
  - Source: operator follow-up, 2026-10-08.
- **D5 [SUPERSEDED IN PART by D8 - Q1 implementation]** Update the todo and run the read-only editor investigation now.
  - Supersedes: D2's requirement to wait for material agreement before this bounded
    read-only investigation, and D3's documentation-only limit for this step.
  - Effect: source/candidate comparison and bounded editor geometry/visual evidence
    are authorized now. No implementation, saved asset changes, generation, broad
    gates, or release work is authorized; Q1 remains open.
  - Reason: not stated.
  - Source: operator answer to the scope question, 2026-10-08.
- **D6 [SUPERSEDED IN PART by D8 - Q1 implementation]** Continue investigation and prepare the existing todo for comprehensive
  realization, evaluating the supplied investigation-stage review.
  - Effect: additional owner-local read-only diagnosis and this implementation
    proposal; D5's no-implementation/no-generation boundary remains binding.
    Analytical controls of existing behavior are permitted; candidate geometry
    prototypes and changed algorithms remain behind Q1.
  - Reason: not stated.
  - Source: operator follow-up, 2026-10-08.
- **D7** Critically evaluate the attached simplification review against code,
  architecture, goals and prior agreements; apply valid in-scope corrections and
  explicitly refute incorrect points.
  - Effect: update this existing planning artifact. No Q1 implementation choice or
    asset mutation is approved by the review's proposed Q1-Q3 answers.
  - Reason: not stated.
  - Source: operator follow-up, 2026-10-08.
- **D8** Implement the accepted todo through implement-approved-change for the
  bounded Q1 correctness slice: small RED/GREEN fixtures and owner-local road
  conformity, consumed-input identity and water coverage.
  - Supersedes: D5/D6's no-implementation boundary for this bounded slice only.
  - Effect: Q2 grading and Q3 ambiguous crossing reconstruction remain deferred;
    persistent terrain mutation requires accepted water distortion/source-quality
    limits. No production geography promotion, whole-World rebuild or release work.
  - Reason: not stated.
  - Source: operator skill invocation and accepted alignment todo, 2026-10-08.

Release inclusion, the historical R5, final frozen R5/R6, and unchanged performance
policy are owned by release-plan D19-D22, not this investigation.

### Operator gates

- **Q1 [CLOSED by D8 - bounded correctness only]:** What implementation should be approved after the dedicated
  investigation identifies the defect and owning boundary? Gates implementation only.
  No alignment design is endorsed by creating this file.
- **Q2 [OPEN - BLOCKING for road grading]:** What derived road crossfall/longitudinal
  grade, maximum cut/fill, corridor/transition extent, and canonical surface contract
  should be accepted? Existing source heights/widths do not supply a road-design law.
  Gates grading prototypes and production changes, not the read-only diagnosis.
- **Q3 [OPEN - BLOCKING for ambiguous crossings]:** Should available source crossing
  metadata be admitted through the existing canonical/typed contracts, or should
  ambiguous crossings be explicitly excluded/refused? Do not invent ordinary
  junctions, bridge clearance, or tunnel elevations from XY overlap alone.

Q2/Q3 do not block Q1-approved independent road-consumption/identity and water-
coverage fixtures outside ambiguous crossings. Water terrain mutation has its own
source-quality and distortion acceptance before persistent promotion; it must not
borrow a road grade law. The following are proposals, not closed gates:

| Gate | Proposed resolution for operator review |
|---|---|
| Q1 | Accepted by D8 for staged owner-local correctness fixtures: road full-surface consumption and bounded tangent/neighbor identity, plus independent canonical water coverage. Representative integration and policy acceptance remain required before persistent production output or a representation migration. |
| Q2 | Keep grading optional and separate. A named paved-road visual fixture may compare about 2% normal crossfall only after explicit prototype/input approval. Accept road-class/source-quality-specific longitudinal, transition and cut/fill limits before persistent grading; report infeasible segments. |
| Q3 | Admit available bridge/tunnel/layer/ford distinctions through existing source/canonical/typed seams. Preserve authenticated node connectivity only where present; otherwise keep connectivity unknown. Exclude/report ambiguous features from automatic merging/grading while unrelated correctness proceeds. |

The roughly 2% reference comes from [FHWA's tangent-pavement cross-slope guidance](https://highways.dot.gov/field-offices/missouri/interchange-design-promptlist).
It is not an ALIS surveyed fact, a universal road-class rule, or proof that an admitted
road is paved. [OSM's layer guidance](https://wiki.openstreetmap.org/wiki/Key:layer)
describes crossing order, not metric elevation; coincident XY is not shared-node identity.

### Working assumptions

- **A1 [ACTIVE]:** The reported concerns can be diagnosed using existing maps and a
  small representative fixture before any production geography is regenerated.
- **A2 [ACTIVE]:** Surface color/reflection may make an intersection look wrong;
  final attribution must compare geometry/depth with lit presentation.
- **A3 [SUPERSEDED IN PART by D8]:** Conformity is approved for Q1 owner-local
  fixtures. Bounded feature-driven shaping remains conditional on Q2 and is not
  an automatic second phase.

## Non-goals

- Shader, texture, palette, roughness, or default-material changes.
- Unapproved grading or crossing reconstruction, a WaterBody system,
  mesh-layering framework, or additional geometry authority.
- Full territory regeneration, packaging, or publication as an investigation step.

## Verified evidence

### Reported concerns before the bounded editor probe

- The operator reports terrain/road and terrain/water alignment concerns, including
  intersections and layering. The probe below establishes four local ordering
  counterexamples; junctions, seams, and broader failure categories remain open.
- The supplied proposal mentions shoreline intersections, z-fighting, and mesh layering
  as separate work. Those descriptions do not prove a current defect in either city.

### Verified repository facts

- [Shared terrain sampling](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp)
  uses bilinear interpolation and clamps queries to the current cell's core grid.
  [Mesh Terrain construction](../../Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp)
  authors triangles A,C,B and B,C,D per quad. These are different surfaces on a
  non-planar quad. Analytical control: A=B=C=0 m, D=10 m gives 2.5 m bilinear
  height at the center but 0 m on the shared B,C diagonal. This proves a source-model
  mismatch, not its severity on a loaded city's compiled/rendered/collision surface.
- [Road construction](../../Plugins/World/ProjectWorld/Source/ProjectWorldRoadEditor/Private/ProjectWorldRoadRealization.cpp)
  subdivides centerline segments to a configured maximum, samples endpoints, and
  assigns each endpoint height to both lateral edges. Even a planar cross-slope
  can therefore put one edge below terrain and the other above it. Its offset is
  added to height (vertical Z), not applied along a surface normal. Current
  [Kazan](../../Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json)
  and [Manhattan](../../Plugins/World/ProjectWorldData/Data/Profiles/Realization/manhattan_showcase_v1.realization.json)
  recipes set 0.15 m and 7.5 m, `terrain_drape`, `overlap_same_owner`, Nanite, and
  complex-as-simple collision. Recipe values must be authenticated against the
  loaded map/realization receipt before attributing a reported symptom to them.
- The [road producer](../../Plugins/World/ProjectWorld/Source/ProjectWorldRoadEditor/Private/ProjectWorldRoadLayerProducer.cpp)
  declares a one-cell terrain dependency halo. The existing
  [dirty planner](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationProfile.cpp)
  propagates dependency dirty units through that halo. This does not make the
  road builder sample adjacent terrain: its current sampler clamps at cell edges.
  Any new neighbor consumption needs authenticated inputs and output identity,
  as well as closure tests; halo declaration alone is not a conformity proof.
- `SampleTerrain` is shared by buildings, vegetation, gameplay placement,
  [anchor placement](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldAnchorPlacement.cpp),
  and [runtime route/anchor realization](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeRealization.cpp).
  A global interpolation change would affect these consumers too. The dedicated
  investigation must choose an explicit surface contract and consumer scope;
  a road fix must not silently change their elevations or depend on a private
  Mesh Terrain implementation helper.
- [Water/terrain pitfall 28](../../Plugins/World/ProjectWorld/docs/pitfalls.md#28-a-visible-water-surface-can-still-z-fight-with-terrain)
  documents a previously fixed vertical-order defect and its canonical clearance owner.
  A reflective material did not fix it; a constant offset could not cover arbitrary
  terrain intrusion. Read the existing guard before proposing another correction.
- [Canonical terrain finalization](../../tools/World/CanonicalCompilation/app/terrain_surface.py)
  lowers covered terrain samples using canonical water height and clearance, then
  applies authored patches and rejects a sampled clearance violation. It handles
  water footprint membership and holes; it does not establish continuous clearance
  over every intersecting terrain/water triangle. The current
  [Kazan compilation recipe](../../Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_territory_v1.compile.json)
  specifies 30 m sample spacing and 0 m clearance. The realization recipes above
  specify a 0.25 m water lift. These are recipe inputs, not newly measured map state.
- [Water mesh construction](../../Plugins/World/ProjectWorld/Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterMeshBuilder.cpp)
  uses native polygon offset/intersection and constrained Delaunay triangulation.
  It prepares feature/domain triangles, then clips them to cells and evaluates
  heights at the resulting vertices. Standing height is fixed; flowing height is
  quantized nearest-centerline-segment knot interpolation. Actual triangle interiors,
  clipped vertices, seams, and holes must be compared with terrain, not merely
  checked independently. Water remains non-Nanite and has no collision.
- [Territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md#layered-regeneration-contract-scale-out-precondition)
  separates canonical water elevation from a bounded realization surface offset.
- The [admitted typed feature](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldCanonicalBundle.h)
  has road class, width, and XY geometry but no bridge/tunnel/vertical-layer field.
  This does not prove upstream sources lack those semantics. Trace raw admission,
  canonical normalization, and realization before proposing crossing behavior.

### Reviewer corrections and remaining hypotheses

- Accept the source-supported road interpolation and full-width test targets.
  Refute the reviewer's "normal offset": the code adds vertical height.
- A narrow water footprint between terrain lattice samples may be occluded despite
  correct sampled lowering. This is a plausible hypothesis, not an established
  cause of the reported blue areas. No screenshot was supplied in this follow-up;
  the reviewer's visual attribution cannot substitute for matched depth evidence.
- Same-owner overlap is a policy, not proof of clean junctions. A declared halo is
  not proof of neighbor sampling. Test both before designing corrections.
- Refute the closing suggestion that this todo becomes CURRENT/required only after
  approval: release D19 already made it required. Reviewer recommendations neither
  approve implementation nor establish a road-only mutation envelope for a shared fix.

The earlier source-only audit inspected recipes and existing test assertions, and checked the
analytical non-planar/control arithmetic. No production regression test, alignment
capture, map clearance measurement, or runtime experiment ran during that audit.
The separately authorized editor probe is recorded below; a source-model
counterexample alone is not real-map acceptance evidence.

### Production references and their limits

- [Erwin Heyms's first-person Wildlands production account](https://80.lv/articles/procedural-world-building-in-ghost-recon-wildlands)
  describes turning a road network into terrain heightmaps, materials, and masks.
  It supports testing infrastructure-driven terrain integration. Its designer-led
  road pathfinding is not authority to redirect ALIS's reconstructed road network.
- [Epic's UE 5.8 City Sample PCG](https://dev.epicgames.com/documentation/unreal-engine/city-sample-pcg-for-unreal-engine)
  solves junctions and partially levels terrain in its roads stage, retriggering
  terrain consumers. Its default demo has streaming disabled and generated actors
  require partitioning. Study geometry/grade decisions, not its generation graph
  or default streaming configuration as an ALIS replacement.
- These examples motivate A3; they do not prove Level B fixes ALIS's reported
  defects. ALIS already has canonical water-driven terrain conditioning. The new
  question is whether its existing surface contract needs bounded road grading
  or finer water coverage, not whether to introduce a second terrain authority.
- [Terrain elevation acceptance](../../Plugins/World/ProjectWorld/docs/territory_contract.md#terrain-elevation-acceptance)
  checks compiled Nanite/fallback samples against canonical elevations and separately
  proves collision. The canonical water seam above requires the adapter to realize
  the immutable final surface. An adapter-only grade change with unchanged input
  identities cannot truthfully retain those fidelity claims.
- Reviewer boundary correction: Level B cannot remain entirely inside the Road,
  Water, and Mesh Terrain adapters while retaining today's canonical contract.
  Changing final terrain elevations also involves the canonical terrain owner and
  its accepted provenance/fidelity contract. Keep native engine dependencies in the
  terrain adapter; this does not justify moving geographic authority there.

### Bounded read-only editor evidence - 2026-10-08

Scope: the existing Kazan territory map, cells `grid_413718bc833994e5:x1:y0`
and `grid_413718bc833994e5:x1:y1`, four loaded source-LOD meshes, eight
owner-isolated vertical collision traces, and four fixed lit captures. No candidate
was implemented. This is partial T0/T1 evidence, not full investigation acceptance.

- Configured launcher engine: 5.8.3. The live map and georeference were queried:
  `/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory`,
  EPSG:32639, projected origin `(381409, 6185051)`, vertical origin `0 m`.
- The current active bundle was authenticated by size and SHA-256:
  `995fb0fb1d6550925efaf55111a6936e600658d0b9cb3938461a3e6cac47b8dc`;
  authority `kazan_territory_v1:75b8eefd6a99e53ff0db65ad9efaccf029759bd098dbe5e4b8e8651565a287b8`.
  Its georeference matches the live map. This authenticates current inputs, not
  identity with the historical frozen R5 artifact.
- Read the existing meshes through native `StaticMeshDescription`, resolving each
  triangle's three vertex-instance IDs to validated vertex IDs. Compare vertices,
  triangle centroids, and edge midpoints with the owning cell's canonical A,C,B /
  B,C,D terrain surface. 36,752 samples were inside the cells; 82 road samples
  outside their owning cell were excluded, not silently clamped or counted as safe.
  Sampling is diagnostic and does not certify continuous clearance.

Negative clearance below is feature height minus terrain height at the same XY.
Coordinates are Unreal world centimeters; values identify the selected diagnostic
point, not a global minimum. Terrain traces hit `MeshPartitionEditor.PreviewMeshComponent`;
road-only traces independently hit the corresponding generated road mesh.

| Feature / cell | Diagnostic world XY (cm) | Feature Z (cm) | Terrain preview collision Z (cm) | Signed clearance (m) |
|---|---|---|---|---|
| Road x1:y0 | 14061.723, -929.068 | 7941.382 | 8146.826 | -2.0544 |
| Road x1:y1 | -37006.920, -66654.362 | 5615.460 | 5780.483 | -1.6502 |
| Water x1:y0 | 7451.500, 68945.000 | 5555.000 | 5832.325 | -2.7733 |
| Water x1:y1 | -39015.000, -64159.000 | 5025.000 | 5745.470 | -7.2047 |

The road points are triangle edge midpoints. Water x1:y0 is an edge midpoint;
water x1:y1 is a vertex. Water has no collision; its Z comes from the loaded source
mesh, compared with terrain-only collision. Terrain preview hits agree with the
canonical triangle model at these points; this is not packaged/Nanite collision proof.

Fixed 1280x720 oblique captures show interrupted road ribbons and exposed water
fragments among terrain in these neighborhoods. They support investigating geometry
ordering independently of materials; they do not isolate ordinary junction defects,
z-fighting, or the exact contribution of bilinear sampling versus lateral-edge heights.
The native capture receipt reports all 210 terrain subjects drawn and none pending.
Plan/image hashes were checked. The repeat-pose hash differs (temporal state), so these
images are diagnostic views and do not satisfy a deterministic visual gate.

Reproducible local record: `tmp/world/alignment/production-comparison-20261008/`.
`dumps/editor_mesh_read.py`, `dumps/analyze_source.py`, and `dumps/editor_traces.py`
produce `receipts/source-clearance.json` and `receipts/editor-traces.json`;
`dumps/vantages.json` records the exact poses for `ProjectWorld.CaptureEvidence`.
`receipts/capture.json`, `screenshots/`, and `receipts/editor.log` retain the terminal
capture evidence. These disposable diagnostics are not future test inputs or release
authority; the identities, points, measurements, and limitations are recorded above.

No dirty content/map packages were reported before capture. The agent-owned editor
exited normally without saving. Before/after SHA-256 inventories prove all 3,739 files
under ProjectWorldData Data/Content and ProjectMaterial/ProjectTexture Data/Content
are unchanged, with no additions or removals (`receipts/no-mutation.json`).
No PIE, World regeneration, canonical compilation, broad rebuild, cook, or release
gate was run. Manhattan, continuous clearance, seams, semantic crossings, junction
construction, neutral/depth views, and A/B grade/transition quality remain unverified.

### Continued diagnosis and feature attribution - 2026-10-08

Read-only scope: the same two Kazan cells, two Manhattan cells (`x1:y0`, `x1:y1`),
their required neighbor terrain, existing production calculations on disposable
in-memory inputs, and saved source-LOD geometry. No A/B geometry prototype was made.

Current Manhattan authority is
`manhattan_showcase_v1:22db753ab53d21e6355633bc3e0b4d1424549a87e50736b2a6bacd0988b98c40`,
bundle SHA-256 `84b0b6be76427baf31b0087d91e5f88c66cf9237a776acbf59cb92587e032067`.
The live map was `/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase`;
its EPSG:32618 origin is `(585628, 4511322, 0)`.
Both bundles were authenticated by size/hash. Six active road/water/terrain manifests
match their current compile receipts and realization-profile byte hashes; all 2,166
listed artifact byte digests match saved files. Four road unit-input hashes and actor
semantic tags independently match the current canonical features/settings.
This is current-state authentication, not a replay or release gate.

#### Road - first failing calculations

Reconstructing the existing source arithmetic, including local float32 storage,
reproduces all 14,472 road triangles in the four sampled meshes with zero coordinate
delta. This rules out stale road output relative to those authenticated inputs in
these four cells. Terrain preview collision agrees with the triangle surface at the
queried points. It does not establish universal terrain/render/collision correctness.

The builder first substitutes a bilinear surface for the consumed terrain triangles,
then copies each centerline endpoint height to both lateral edges and linearly spans
segments. Both errors contribute; changing interpolation alone is insufficient.
The decomposition below is exact source arithmetic at the selected saved-mesh point:
`clearance = (bilinear - triangle) + lateral/segment error + 0.15 m lift`.

| Map / cell | Canonical road feature | Bilinear minus triangle (m) | Lateral/segment error (m) | Source-mesh clearance (m) |
|---|---|---|---|---|
| Kazan x1:y0 | alis:osm:way:124954339 | -1.1217 | -1.0828 | -2.0544 |
| Kazan x1:y1 | alis:osm:way:195408844 | -1.3594 | -0.4409 | -1.6502 |
| Manhattan x1:y0 | alis:osm:way:5671393 | +0.0007 | -4.6183 | -4.4676 |
| Manhattan x1:y1 | alis:osm:way:195743208 | -2.9459 | -2.7321 | -5.5280 |

Manhattan point XYZ in projected meters: x1:y0
`(585513.106328, 4510602.427891, 62.794041)`; x1:y1
`(585212.744663, 4511611.720859, 36.555161)`.
Terrain-only traces give 67.261659 m and 42.083198 m respectively. At x1:y0,
road-only collision agrees with the selected road. At x1:y1 it first hits a different
overlapping road, `alis:osm:way:1051750427`, at 39.311881 m: 2.756720 m above the
selected saved ribbon and 2.771317 m below terrain. The selected lower ribbon height
there is source-LOD evidence, not an isolated collision result.
This proves multiple road elevations at that XY; it does not prove an ordinary
junction or justify collapsing a bridge/tunnel. The current
[road canonical adapter](../../tools/World/CanonicalCompilation/app/adapters.py)
retains class, inferred width, and name, not bridge/tunnel/layer attributes; the
provider-preserving source adapter retains properties. Q3 owns that unresolved scope.

All 82 previously excluded Kazan road samples are now evaluated against their proper
neighbors: 56 from x1:y0, 26 from x1:y1, across eight distinct neighbor cells.
Their clearances range from -0.6537 to +1.0437 m and -0.8989 to +0.4661 m respectively.
All 82 terrain-only preview collision queries hit; their maximum difference from the
neighbor triangle model is less than 0.000001 cm. Four original-point rechecks also
agree. No out-of-cell sample is treated as safe by clamping.
The two Manhattan meshes have a further 309 out-of-cell samples, all analytically
assigned to authenticated neighbor terrain; they have not all had live collision probes.

The current [road unit hash](../../Plugins/World/ProjectWorld/Source/ProjectWorldRoadEditor/Private/ProjectWorldRoadRealization.cpp)
includes local fragment parts but omits the full feature geometry consumed by
`RoadTangent`. A source-arithmetic control keeps fragment `(0,0)->(10,0)` unchanged
and rotates its continuation: endpoint tangent changes from `(1,0)` to
`(0.707107,0.707107)` while the fields hashed by this unit remain unchanged.
This is a latent consumed-input identity gap, not the demonstrated cause of today's
road heights. Repair it alongside any new neighbor consumption; do not depend on
broader regeneration to cover it.

#### Water - sample coverage versus continuous surface

The selected water triangles' three vertex heights match the corresponding canonical
function, quantization, and 0.25 m lift (maximum difference below 0.000001 m).
Terrain preview collision matches canonical triangle interpolation at each selected
point. For these points the first demonstrated gap is canonical finalization's
lattice-point coverage, not a stale water height or displaced terrain preview.

| Map / cell | Canonical water feature | Function height (m) | Mesh height (m) | Terrain triangle height (m) | Covered quad corners / 4 |
|---|---|---|---|---|---|
| Kazan x1:y0 | alis:osm:relation:3974314 | 55.3 | 55.55 | 58.32325 | 1 |
| Kazan x1:y1 | alis:osm:relation:7493502 | 50.0 | 50.25 | 57.45470 | 2 |
| Manhattan x1:y0 | alis:osm:way:576035677 | 17.8 | 18.05 | 21.03140 | 0 |
| Manhattan x1:y1 | alis:osm:way:702073775 | 24.0 | 24.25 | 26.02780 | 0 |

Kazan's examples are flowing polygons, not narrow ribbon features. Manhattan's are
standing polygons with `low_confidence_point_fallback` height provenance. Their
fitted heights are declared derived inputs; these results do not prove surveyed
water levels or justify refitting them from already-shaped terrain.
Manhattan water points are `(585468.69,4510677.71)` and `(585752.11,4511942.79)`.

An existing-finalizer control uses a flat 12 m, 30 m terrain quad and a constant
10 m water function. A 3 m ribbon at Y=15 m covers no lattice corners; the production
`finalize_terrain_cells` returns normally, leaving terrain 1.75 m above water including
its lift. A 100 m wide control lowers corners to 10 m and gives +0.25 m clearance.
This exercises current production code on in-memory fixture data, not a candidate
algorithm or generated geometry. It establishes the sample-only invariant gap.
Continuous clearance must also cover polygon banks, holes, sloping functions,
quantization, authored patches, and final clipped water-triangle interiors; selected
vertex agreement does not exonerate the water leaf for those other cases.

#### Conformity versus constructed road geometry

Source-only predictions sample the current terrain triangle heights at both ends
and both lateral edges of each attributed road segment. Maximum cross-slope ratios
are 27.9%/32.1% for the Kazan examples and 58.1%/118.0% for Manhattan; selected
centerline longitudinal ratios are 27.2%/14.5% and 42.4%/16.3% respectively.
These are terrain-conformity predictions, not surveyed road grades or prototype
results. They demonstrate why exact conformity alone cannot be assumed to meet
the agreed constructed-appearance goal. Q2 must establish the derived design limits.

Four Manhattan lit captures were hashed and reviewed. Three show terrain/water or
fragmented-road context; `manhattan_road_x1_y0.png` is obstructed by a building and
is excluded from visual conclusions. The native receipt's accepted capture status
does not certify framing or visibility. Repeat-pose hashes differ; no deterministic
visual gate, neutral/depth proof, or packaged proof is claimed.

Receipts and exact coordinates/poses/scripts remain under
`tmp/world/alignment/owner-diagnosis-20261008/`: `receipts/*-attribution.json`,
`road-unit-authentication.json`, `manifest-authentication.json`,
`controls-and-grades.json`, `kazan-neighbor-traces.json`, `manhattan-traces.json`,
and `manhattan-capture.json`, with dumps/screenshots in separate directories.
Disposable diagnostics are not permanent test inputs. Both agent-owned editors
exited normally without saving; dirty package lists are empty. All 3,739 protected
files remain byte-identical, with no added/removed paths.

## Current architecture and source of truth

Start at [World territory generation](../../Plugins/World/ProjectWorld/docs/territory_generation.md).
Follow the owning source/canonical or leaf contract after measuring the failing
representation. Keep these existing black boxes:

| Owner | Responsibility |
|---|---|
| ProjectWorldData / CanonicalCompilation | Concrete geography/provenance and canonical footprints, elevation functions, terrain samples, and admitted semantics |
| ProjectWorld | Shared coordinates/representation contracts, dependency identity, orchestration, and transaction |
| ProjectWorldRoadEditor | Road ribbon construction, drape, intersections, meshes, and collision |
| ProjectWorldWaterEditor | Water preparation, clipping, triangulation, elevation consumption, and meshes |
| ProjectWorldMeshTerrain | Realized terrain topology, compiled sections, and terrain geometry adapter |
| ProjectTexture / ProjectMaterial | Appearance; never geographic elevation, alignment, or geometric truth |

Expected change in this phase: this investigation record only. Production code,
profiles, generated World/canonical packages, Material/Texture outputs, and release
artifacts remain untouched. Declare a narrower CHANGED/UNTOUCHED boundary only after
the investigation identifies the cause.

## Problem and root cause

Localized causes: road bilinear-versus-triangle consumption plus endpoint height
reuse across width/segments; water finalization's sample-only footprint coverage.
Saved road geometry/current inputs and terrain preview agree in the sampled chain.
Full junction/crossing classification, continuous final-water approximation, visual
acceptance, and a grading law remain open. A latent road tangent-input identity gap
must be repaired in the implementation's consumed-input closure.

## Decision

### Simplification review - source-grounded assessment

- Accept the sequencing correction: I0 incorrectly made Q2/Q3 prerequisites for all
  implementation. Independent correctness fixtures need Q1; grading and ambiguous
  crossing reconstruction retain their own gates. The proposed resolutions above
  remain operator proposals, not authorizations from reviewer text.
- Accept triangle-intersection extrema as the continuous height oracle for finite,
  nondegenerate triangles that are height functions over XY. This is exact in the
  planar model, not an exact-arithmetic or compiled-collision guarantee. Coverage,
  holes, missing triangles, degenerate/vertical projections and seam contacts need
  explicit controls; otherwise an empty intersection can falsely pass.
- Accept bounded tangent context as the preferred identity repair. Current
  RoadTangent's whole-feature averaging/fallback must change with that consumption;
  locally hashing its unchanged global reads would preserve the defect.
- Accept separate water quality/distortion acceptance. Manhattan's low-confidence
  examples have 2.9814 m and 1.7778 m intrusion. The 7.2047 m example is Kazan's
  flowing polygon, not Manhattan's low-confidence standing-water case. None of
  those gaps alone authorizes equivalent terrain carving or proves surveyed levels.
- Confirm [terrain-cell-v2](../../tools/World/CanonicalCompilation/contracts/terrain-cell.schema.json)
  has regular-grid core/halo heights and no constrained-triangle field; the
  [typed terrain](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldCanonicalBundle.h)
  agrees. Breaklines/refinement are conditional representation candidates, not a
  requirement or an adapter-only shortcut. A coarse grid can obtain clearance by
  lowering support vertices, but may fail the accepted distortion policy.
- Confirm SourceIngestion preserves exported geometry/properties and canonical
  road admission drops the crossing tags. Its normalized record has no explicit
  original-node connectivity field. Available tags should be preserved through
  the existing seam if Q3 selects admission; shared-node identity remains unknown
  until authenticated source evidence proves it. Do not manufacture connectivity.

### Recommended sequence

Recommendation: staged correctness first. Fix Road's full-ribbon triangle/neighbor
consumption and consumed-input identity; independently correct continuous water
coverage in the canonical final-terrain owner. Compare grading only where corrected
conformity still fails constructed appearance. The predicted extreme terrain grades
justify that comparison, not mandatory grading in the correctness slice.
Q1 gates correctness implementation; Q2 gates road grading and Q3 gates ambiguous
crossing work. All remain open. A water-quality/distortion policy is required before
persistent conditioning; numerical limits are not invented from the measured gaps.
No general compositor, selected topology, prototype or production mutation is approved.

Compare a narrower road surface-sampling contract with a deliberately shared one;
the latter needs evidence for every affected placement consumer. Evaluate bounded
edge sampling/subdivision only if it resolves the measured error, including triangle
interiors; matching a few vertices alone does not prove whole-ribbon clearance.
For water, compare existing-owner input/topology corrections before any modifier
stack. Native [Mesh Terrain](https://dev.epicgames.com/documentation/unreal-engine/mesh-terrain-in-unreal-engine)
and [local remeshing](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MeshPartitionEditor/URemeshModifier)
are experimental options, not an approved fix. Validate configured-engine behavior,
influence/section extent, triangle coverage, determinism, and lifecycle before
selecting one. No WaterBody migration or independent elevation authority is implied.

### Conformity versus feature-driven shaping

| Candidate | Owning boundary and moving parts | Capability given up / selection condition |
|---|---|---|
| A - Conform-only | Correct terrain-surface consumption and complete road-ribbon approximation in existing owners; no terrain grade contract or new lifecycle | Gives up grading/bank shaping. First controlled candidate; accept only if geometry and constructed appearance meet the agreed goal |
| B - Bounded feature-driven shaping | Accepted canonical road/water geometry drives derived local grade/channel intent through the existing canonical terrain owner; native modifiers may implement the accepted representation inside the terrain adapter | Adds a derived-surface contract and terrain/consumer invalidation cost. Compare seriously if A cannot meet measured grade/clearance/appearance needs; not an automatic phase |
| C - General terrain composition | New overlay/compositor framework, broad dependency graph, and authoring/runtime lifecycle | Outside demonstrated 3.0.0 need; exclude instead of creating speculative machinery |

For B, first evaluate reuse of canonical terrain finalization and its existing
water-fit/authored-input authority. Preserve admitted road XY, class, width, source
provenance, water footprints/functions, and available crossing distinctions. Label
inferred grading as derived geometry, not surveyed elevation or new source facts.
Define the final surface's one owner, version/hash, derivation, baseline-versus-final
height evidence, geometry/collision expectations, and dependency closure before Q1.
Do not silently shape terrain in the adapter while claiming unchanged canonical
fidelity. A representation-contract extension requires explicit design acceptance.

Keep the flow acyclic: canonical features plus authenticated terrain inputs ->
final derived terrain -> road/water realization and other placement consumers.
Do not derive terrain from already-realized road meshes that depend on that terrain.
Do not independently refit canonical water elevation from newly shaped output.
No Houdini, global PCG, Geometry Scripting orchestration, or modifier-authority
framework is needed merely to compare these two candidates.

## Required invariants

1. One canonical geographic authority and one active terrain realization owner.
2. No presentation parameter becomes an independent road/water elevation authority.
3. Preserve road width/class/XY/provenance, water footprints/holes/elevation meaning,
   coordinate/quantization rules, and admitted crossing semantics.
4. Read-only investigation leaves generated/canonical path/hash sets unchanged.
5. Preserve front-facing winding, normals/tangents, road collision, water no-collision,
   cell ownership, and streaming. Existing verification must be extended where it
   does not measure the proposed contract.
6. Changed-input identity covers every consumed terrain cell/representation. Bound
   dirty closure, output count, topology growth, and cost; preserve unaffected bytes.
7. An approved mutation uses the owning promotion/transaction route, is deterministic
   and idempotent, and restores exact protected state on refusal/injected failure.
8. Derived grading/topology has explicit provenance and an authenticated final-surface
   contract. Canonical-to-realized fidelity remains truthful and dependencies acyclic.

## Investigation tasks

D5 permits the bounded read-only editor investigation now; Q1 still gates changes:

T0-T3 have the diagnosis evidence above. D8 permits Q1 owner-local correctness
fixtures in T1/T2/T4; grading and crossing prototypes remain behind Q2/Q3.

- [ ] **T0 - Existing-map diagnosis, no persistent mutation:** authenticate Kazan and
  Manhattan map/canonical/realization identities. Locate representative feature IDs,
  owning cells, source ribbons/polygons, and loaded actors. Record canonical samples,
  compiled/rendered/collision terrain, road/water surfaces, signed clearances across
  complete footprints, coverage, bounds, and depth ordering. Compare matched lit and
  neutral/depth ground/oblique/overhead/cell-edge views. Classify source absence,
  occlusion, sampling, topology, seams, streaming, or appearance. Put captures and
  receipts separately under `tmp/world/alignment/<run>/`. Do not save World changes.
- [ ] **T1 - Road candidate proof:** reproduce non-planar and planar controls in the
  existing fixture infrastructure; compare the authored triangle diagonal with actual
  Mesh Terrain output. Test full-width cross-slope, steep/diagonal segments, bends,
  cell-edge clamping, and junction overlaps. Derive tolerances from the consumed
  surface/collision contract. Compare narrow versus shared sampling scope, neighbor
  inputs/identity, bounded subdivision, and topology alternatives against measured
  footprint error. Preserve admitted width/XY/provenance; do not increase a global
  offset to conceal mismatch. No road correction is approved by this task list.
  Judge lateral/longitudinal grade and terrain transitions in fixed views: does
  exact drape look constructed, or just reproduce an implausible ribbon accurately?
  Assess ordinary junction topology/overlap separately; sampling cannot alone prove
  a coherent intersection. Keep inferred design choices distinct from sourced facts.
- [ ] **T2 - Water candidate proof:** test a narrow ribbon entirely between terrain
  lattice samples, plus a broad covered-water control, standing polygons/holes,
  sloping flowing knots, overlapping triangle interiors, and adjacent-cell clipping.
  Establish continuous signed clearance and compare prepared triangles with final
  re-heighted vertices. Trace the first failure: missing/wrong canonical footprint ->
  source/compiler owner; terrain intrusion -> canonical/terrain owner; water topology
  or height approximation -> water leaf. If geometry/depth is correct, hand appearance
  back to [World Surface Presentation](20261008-1543_world_improve_surface_materials.md).
  Compare the smallest existing-owner correction before native local refinement;
  never use masking, blanket lifting/flattening, or render priority as geometric proof.
- [ ] **T3 - Crossing semantics:** trace available bridge/tunnel/layer attributes from
  source through canonical admission and typed realization. Test ordinary junctions
  and supported crossings; preserve sourced distinctions. Record absent/unknown
  metadata instead of inferring bridges or forcing roads above water. Any new
  semantic contract or unsupported-crossing requirement needs a scope decision.
  Include ford distinctions and trace original node/way connectivity separately
  from exported tags/XY; absence of explicit topology remains unknown connectivity.
- [ ] **T4 - Plan and review:** extend this same todo with actual repro/root cause,
  approved public boundary, exact CHANGED/UNTOUCHED owners/consumers, KISS comparison,
  named red/green tests, tolerances, bounded cost, rollout/rollback, and traceability.
  Before Q1, compare A/B analytically on the same authenticated features and terrain
  inputs. Geometry prototypes, measured candidate generation cost, changed artifacts,
  and failure/rollback experiments begin only after approval; baseline metrics and
  source-derived estimates may be recorded now.
  Record signed footprint clearance, grade/transition appearance, junction quality,
  topology/triangle cost, affected cells/sections, generation cost, changed artifacts,
  rollback, and canonical-versus-final/realized surface evidence. Reject B if its
  extra scope is not justified by a demonstrated need or its contract remains unknown.
  If the proven defect requires terrain deformation or local refinement, compare
  native UE 5.8.3 [USplineModifier](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MeshPartitionEditor/USplineModifier),
  [USplineRemeshModifier](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MeshPartitionEditor/USplineRemeshModifier),
  and `URemeshModifier` before custom geometry algorithms. Verify the public API in
  the configured pinned engine source, not documentation alone. Keep Mesh Partition
  integration/dependencies inside `ProjectWorldMeshTerrainEditor`, with no new
  dependency from ProjectWorld or Road/Water modules and no independent modifier
  authority/lifecycle. Measure affected sections, triangle influence coverage,
  cell seams, deterministic rebuild/cache identity, collision, streaming, cost,
  and rollback before adoption. This is conditional candidate evaluation, not a
  requirement to introduce deformation or remeshing.
  Read [scientific debugging](../../docs/agents/scientific_debugging.md) if real output
  contradicts a green gate or hypotheses repeatedly fail. Obtain independent architect
  review, resolve Q1, and stop for implementation approval.

## Proposed implementation - staged approval

### Premise and KISS

The canonical final-terrain owner handles water coverage; Road consumes that final
surface and owns ribbons. Grading and reconstructed junctions are separately gated
extensions, not prerequisites for independent correctness fixtures. Extend those seams,
using native engine geometry within leaf adapters where it actually helps. Add no
service, compositor graph, geography authority, polling lifecycle, or replacement
WaterBody/PCG/Houdini system. Give up general overlay authoring and automatic bridge
inference. Local remeshing is conditional, not a mandatory phase.

### Expected owners and consumer closure

| Boundary | Expected change after approval | Expected preservation |
|---|---|---|
| CanonicalCompilation | Correctness: water support/quality policy, influence/old-new dirty closure and focused fixtures. Conditional: versioned surface representation if needed; road grade intent after Q2 and crossing admission after Q3 | Sourced XY, admitted widths, immutable pre-shaping water-fit baseline and water functions, provenance/holes |
| ProjectWorld reusable contract | Correctness: plain terrain triangle/neighbor consumption and identity/verification seams. Conditional: selected representation or typed admitted semantics | No MeshPartition dependency; existing bilinear `SampleTerrain` behavior for other placements |
| ProjectWorldRoadEditor | Correctness: full footprint conformity, bounded tangent context/complete consumed-input hashes, boundary ownership and geometry-consistent shading basis. Junction reconstruction only after Q3 | Road class/width/XY, collision, Nanite and streaming policy; no water/terrain implementation imports |
| ProjectWorldMeshTerrainEditor | Realize the authenticated final surface; contract/fidelity fixtures; native modifier integration only if the accepted representation requires it | Sole terrain adapter; no independent grade authority or geographic edits |
| ProjectWorldWaterEditor | Extend continuous final-output proof; change its builder only if that proof localizes a separate approximation failure | Canonical height function, no collision, footprint/holes and cell ownership |
| ProjectWorldData and existing placements | Approved profiles/contracts, changed canonical and generated authority through existing wrappers; re-realize consumers selected by changed heights | Unaffected path/hash sets, authored content and runtime-state boundaries |
| ProjectTexture / ProjectMaterial | No geometry change | Presentation remains in its separate todo and existing backlog owners |

Terrain-dependent building, vegetation, gameplay/anchor/route placements can change
because final elevations change. Include their existing dependency declarations,
identities and focused output checks; do not claim they are byte-identical consumers
while changing their consumed surface. Their unrelated implementation is out of scope.
An unexpected owner or undeclared dependency is ARCHITECTURAL RED, not permission
for a manual rebuild or a broader workaround.

### Ordered work and gates

- [X] **I0 - Approve a bounded correctness slice:** D8 closes Q1 for selected fixtures
  and owners. Establish surface/collision/clearance tolerances, footprint/holes,
  quantization, seam ownership and numerical predicates. I2 water and I3 road
  correctness can be evaluated independently; Q2/Q3 apply only to I4's grading/
  crossing extension. Record unknown crossing exclusions and remaining visual
  coverage explicitly. Before persistent water conditioning accept its cut/fill,
  bank/influence extent and source-confidence exception policy; no road grade law
  is needed. A representation change or wider migration requires explicit acceptance.
- [X] **I1 - Land focused RED proof:** the Q1 checkpoint below records reproduced
  terrain-diagonal/ribbon, tangent identity and between-samples coverage failures.
  Migrate the attributed points/features and
  faithful small controls into owning permanent fixtures. Observe the broken full
  ribbon, neighbor/tangent identity and water-between-samples cases fail on production
  builders/output. Add planar/wide-water positive controls and the continuous oracle
  below. Grade/crossing fixtures are added only under their separate accepted gates.
  Keep diagnostic source arithmetic separate from the acceptance oracle.
- [ ] **I2 - Canonical water coverage (owner-local implementation/proof complete;
  production quality and adapter integration pending):** preserve the original terrain/water-fit
  baseline and fitted functions/provenance. Establish continuous support over terrain
  triangles/corners, including boundaries, holes, sloping functions and authored
  patches. Cover the final realized water triangles, not just lattice membership.
  Use the canonical function plus an accepted realization-error envelope; do not
  derive canonical heights from UE output or refit water from newly shaped terrain.
  Measure maximum/volume cut/fill and bank/influence extent against I0's water policy;
  refuse/report quality exceptions before promotion. Increasing water_clearance_m
  alone does not fix absent lattice coverage. Define old/new shared-border closure.
  First measure bounded existing-grid conditioning in a small fixture. If it distorts
  terrain beyond policy, compare a small canonical constrained-surface/breakline
  fixture. The current terrain-cell-v2 schema and typed terrain contain regular-grid
  heights, not local constrained triangles: version/schema/loader/fidelity/consumer
  work needs explicit acceptance. Native adapter refinement alone is insufficient.
  Do not blanket-lower 30 m cells or select a persistent modifier stack by default.
- [X] **I3 - Road realization and identity (bounded Q1 fixtures; production rollout
  remains I5/I6):** sample the accepted final triangle
  surface at every consumed ribbon location, including proper neighbors. Clip or
  consistently own width extending across cell boundaries. Partition/subdivide at
  terrain-surface transitions as needed to prove interior conformity, not only
  vertex agreement. Minimize tangent consumption to the fragment's source-part and
  necessary adjacent-segment context, with deterministic boundary continuity and
  explicit identity. Builder and hash must consume the same bounded projection.
  Test remote unrelated edits leave units unchanged and newly relevant/moved segments
  invalidate them. Current RoadTangent scans the full feature, averages nearby
  segments and has a global nearest fallback: simply hashing local parts while
  retaining that algorithm remains unsound. Retain full-feature identity only if
  narrower consumption cannot yet preserve the accepted behavior, and disclose its
  fanout. Authenticate every consumed neighbor/contract; bump the owning output
  revision. Use normals/tangents consistent with geometry (current triangles supply
  fixed +Z/+X); prove the actual
  static-mesh build result. Preserve the old sampler for unrelated consumers.
- [ ] **I4 - Conditional grading/crossings:** after conformity evidence, seek Q2
  only for segments whose grade/transition still fails the constructed-appearance
  goal. Accept crossfall, longitudinal, cut/fill and transition limits; label grade
  intent as derived, preserve the pre-shaping baseline and finalize terrain once.
  Under Q3, admit available bridge/tunnel/layer/ford distinctions; construct ordinary
  junctions only with proven connectivity and accepted semantics. Tags alone do not
  recover node identity, and neither layer numbers nor XY supply bridge heights.
  Unknown cases remain unclassified, excluded/reported, not silently merged or graded.
  The terrain adapter realizes the authenticated final surface for I2/I4; compare
  native spline/remesh only when the accepted representation needs it. Prove section
  influence, lifecycle, render/collision fidelity, determinism and seams.
- [ ] **I5 - Focused integration and cost:** use a small multi-cell fixture with
  attributed Kazan and Manhattan shapes. Prove complete footprint ordering,
  collision, water holes/functions, selected placement elevations,
  neighbor identity and exact unchanged units. Record changed cells/sections,
  triangles, time and memory, then inspect matched lit and neutral/depth views with
  validated unobstructed framing. Grade/transition and reconstructed-junction proof
  applies only if I4 is included; otherwise report the remaining appearance/semantic
  exceptions. A camera receipt alone is insufficient. Correctness acceptance does
  not itself close the required release concern or accept those exceptions for 3.0.0.
- [ ] **I6 - Rollout and acceptance:** review the measured plan before durable
  promotion. Bump owning output/contract revisions, update baselines through owners,
  use the planner's required slice gates once at slice exit, then the existing
  transactional affected-output promotion. Prove refusal leaves authority unchanged,
  injected failure restores exact protected state, save/reload and no-op identity,
  and streaming/collision through the real route. Retain prior authority for the
  existing rollback route. Final frozen R5 remains future release work after both
  visual concerns are accepted; do not debug through packaging.

### Regeneration estimate and stop conditions

Q1 fixture timing and proof are recorded below; production regeneration time,
triangle growth, adapter cost and memory remain unmeasured. Current inventory provides
a scope ceiling, not a duration estimate: 210 canonical terrain cells per city;
161/132 road output cells and 145/115 water output cells (Kazan/Manhattan).
The four sampled road meshes contain 14,472 triangles. A shared road output-revision
change can select all 293 current road output cells; a shared canonical final-surface
change can require evaluation of all 420 terrain cells and their actual dependency
closure, even if final numeric changes are sparse. Do not promise a four-cell rollout
or unaffected consumer bytes before comparing the resulting consumed identities.

During development use one quad, then a small cell plus neighbors. After approval,
measure an affected fixture and use the real dirty planner to report changed heights,
road/water units, placements and compiled sections, including quantization/border
support. A 30 m quad has a 42.43 m diagonal: support-corner conditioning can reach
beyond a thin footprint; this geometric bound is not an approved influence radius.
Stop if that fanout or cut/fill exceeds I0's envelope. Algorithm provenance changes
alone must not masquerade as geometric change; preserve the existing consumed-input
locality contract rather than adding a second identity/planner.

## Test-first and verification plan

### Continuous triangle clearance oracle

For each actual feature/terrain triangle pair with overlapping XY bounds, compute
the projected intersection polygon using native robust geometry primitives. For
finite nondegenerate projections, each Z is affine in XY, so their difference is
affine and its minimum/maximum over the convex intersection occur at its vertices.
Evaluate those vertices, including newly introduced edge intersections; comparing
only the input mesh vertices is insufficient. Use signed feature-minus-terrain
height and the accepted lift/deviation tolerances. Spatial indexing bounds candidate
pairs; report expected/covered footprint area, pair/vertex counts, worst clearance,
maximum deviation, owning features/cells and numerical tolerance.

Prove coverage of the intended clipped footprint separately, excluding authenticated
holes: missing/empty geometry must not pass by producing no pairs. Include touching
edges/points as seam controls and reject/report non-finite or degenerate/vertical
projections rather than treating them as clear. Use predicates/tolerances appropriate
to the coordinate quantization and storage precision; mathematical linearity does
not make floating-point clipping exact. Controls must include a buried triangle
interior with safe original vertices, disjoint/missing coverage, a hole, cell-edge
contacts, quantized sloping water and an accepted positive-clearance pair.

This proves the planar static-surface relationship for the triangles read, not
Nanite simplification, compiled sections or collision. Retain separate authenticated
native compiled/fallback/render/collision and save/reload proof on their real routes.

| Invariant | Acceptance surface | Execution envelope | Cheapest proof | Final investigation proof | Stop condition |
|---|---|---|---|---|---|
| 1 | Canonical and terrain realization authority | Read-only owner/representation trace | Inspect active contracts | One authority and explicit consumed surface | Competing authority |
| 2 | Canonical versus presentation elevation | Read-only maps plus profiles | Compare heights and offsets | No shader/offset substitution for geometric correction | Unknown cause or fabricated elevation |
| 3 | Width, provenance, footprints, holes, crossings | Named small fixtures and source trace | Bad case plus good control | Supported semantics preserved; unsupported ones explicit | Invented semantics or incorrect coverage |
| 4 | Protected path/hash inventory | Read-only investigation | Before/after inventory | Exact state preserved | Any unintended persistent mutation |
| 5 | Actual surfaces, winding, collision, seams | Normal RHI editor/PIE and owner fixtures | Triangle-interior/edge comparisons | Reproducible error/acceptance oracle on actual output | Vertex-only or mesh-presence-only proof |
| 6 | Input identity, closure, unchanged consumers, cost | Exact fixture tests and focused integration | Input mutation/neighbor/consumer controls | Bounded closure/output cost and unchanged identities/bytes | Unexplained propagation or unbounded subdivision |
| 7 | Persisted state and recovery | Owning transaction in disposable fixture after approval | No-op rerun and injected failure | Deterministic outputs and exact restoration | Promotion/refusal/recovery gap |
| 8 | Derived versus source surface, fidelity, dependency order | Existing canonical contract plus bounded candidate fixture | Input/output/provenance and graph comparison | One truthful final-surface contract and acyclic consumed identities | Unauthenticated adapter deformation or feedback cycle |

The following are required candidate tests, not invented completed regressions:

| Candidate | Failure to reproduce or guard | Control / acceptance oracle |
|---|---|---|
| Non-planar terrain quad | Bilinear differs from A,C,B / B,C,D | Planar quad agrees; actual compiled surface comparison |
| Cross-slope road ribbon | Equal endpoint edge heights bury/float an edge | Full-width and interior signed deviation, bounded topology |
| Exact drape versus local grade | Correct heights still look unnaturally banked/constructed | Matched slope/transition views; grade evidence and explicit derived provenance |
| Road cell edge, bend, junction | Clamping/seam/overlap or unstable coplanarity | Neighbor continuity, winding, valid collision; halo closure |
| Narrow river between samples | Sample-only lowering permits triangle intrusion | Broad covered footprint control; continuous signed clearance |
| Standing hole and sloping flowing water | Wrong footprint, interpolation, clipped heights, or seam | Holes excluded, source elevation function, final mesh continuity |
| Ordinary/supported crossing | Wrong sourced grade or junction collision | Admitted semantics; missing metadata reported |
| New shared/neighbor input | Missed identity or unrelated invalidation | Consumer census and exact changed/unchanged unit set |
| Approved mutation lifecycle | Non-determinism, unsafe refusal, partial outputs | Idempotence, exact inventory, failure restoration |

Reuse existing assertions as guards, not evidence that the missing continuous-surface
property already passes. Candidate starting points (run individually only when the
actual investigation/implementation needs them):

- `Project.World.Realization.Verify.Road`.
- `Project.World.Realization.NativeTwin.WaterGeometryAndSeams`.
- `Project.World.Realization.Water.CanonicalSurfaceMesh`.
- `Project.World.Realization.Layers.PersistentWater`.
- [Terrain surface tests](../../tools/World/CanonicalCompilation/tests/test_terrain_surface.py),
  especially sampled clearance refusal, hole membership, and transition-neighbor dirtying.

### Exact regression targets and deferred integration

The Q1 checkpoint below names implemented tests and bounded results. Other rows
remain proposals, especially production adapter/consumer integration and Q2/Q3.
Permanent tests own their inputs; execution receipts in `tmp/` are not test inputs.

| Proposed exact test ID | Owning test home | RED on current behavior / GREEN after approved change |
|---|---|---|
| `tools.World.CanonicalCompilation.tests.test_terrain_surface.TerrainSurfaceTests.test_narrow_water_between_samples_has_continuous_clearance` | Existing `test_terrain_surface.py` | Current 3 m ribbon remains below 12 m terrain; accepted final surface clears the entire footprint, wide control stays correct |
| `tools.World.CanonicalCompilation.tests.test_water_coverage.WaterCoverageTests.test_hole_flowing_and_shared_halo_support` | `tests/test_water_coverage.py` | Conservative monotonic-flow support, holes and shared halo nodes agree |
| `tools.World.CanonicalCompilation.tests.test_road_surface.RoadSurfaceTests.test_derived_grading_preserves_source_and_water_fit` | Proposed sibling `tests/test_road_surface.py`, conditional I4/Q2 | Current compiler has no approved derived grading guarantee; accepted bounds/transition are met while sourced geometry and immutable water-fit inputs remain identical |
| `tools.World.CanonicalCompilation.tests.test_road_surface.RoadSurfaceTests.test_infeasible_grade_or_crossing_refuses_before_promotion` | Same proposed sibling, conditional I4/Q2/Q3 | Current pipeline has no accepted grading/crossing refusal contract; infeasible/conflicting or unknown intent per Q3 is explicit and protected authority is unchanged |
| `Project.World.Realization.Road.FullFootprintConformity` | Road `Private/Tests/Verify/ProjectWorldRoadVerifyTest.cpp` | Non-planar source, compiled fallback and collision conform over the complete ribbon; save/reload, winding, shading and no-op controls |
| `Project.World.Realization.Road.ConsumedGeometryIdentity` | Same Road test owner | Tangent and consumed neighbor changes invalidate; remote geometry retains identity; indexed batch agrees with single-unit identity; outer clipping and missing interior coverage are explicit |
| `Project.World.Realization.Alignment.WaterTriangleEnvelope` | Water `Private/Tests/Verify/ProjectWorldWaterVerifyTest.cpp` | Final native clipped triangles respect the conservative floor; standing polygon coverage excludes its hole |
| `Project.World.Realization.Water.ContinuousTerrainClearance` | Existing Water native-twin fixture infrastructure, small sibling test file | Current narrow/polygon-bank intrusion fails despite matching vertex functions; final clipped triangle intersections clear terrain, holes/sloping functions and cross-cell seams satisfy the accepted contract |
| `Project.World.Realization.Alignment.FinalSurfaceConsumerClosure` | Existing editor realization-profile fixture infrastructure, small sibling test file | Current full-ribbon/coverage counterexamples fail; small Kazan/Manhattan-shaped fixtures share one final authority, selected placements/neighbor identities and unchanged units agree; no-op/refusal/failure restoration remains exact, I4 exceptions remain explicit |
| `Project.World.Realization.Alignment.TriangleClearanceOracle` | Core `Private/Tests/ProjectWorldTriangleClearanceTests.cpp` | Overlap extrema detect interior burial; canonical-scale translation, disjoint/touching triangles and degenerate/non-finite refusal |
| `tools.World.CanonicalCompilation.tests.test_water_coverage.WaterCoverageTests.test_quality_and_authored_refusal_leave_all_cells_unchanged` | `tests/test_water_coverage.py` | Missing policy, cut/distance/accuracy exceptions and authored-height violations refuse with input cells unchanged |

For Python run `python -B -m unittest <exact-test-ID>`; for UE run
`scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-full-test-name>`, one target
per invocation. Preserve the existing water-fit, authored-clearance refusal,
promotion, material-identity and terrain-fidelity guards as affected by the change.
The conditional grading/crossing tests cannot establish GREEN until their applicable
Q2/Q3 provide the oracle; independent road/water correctness tests do not wait for them.
Do not invent slope/clearance limits inside a test. Cross-owner integration must use
the public fixture route; Road/Water unit tests must not import MeshPartition or a
private terrain adapter helper. Check permanent tests against production builders
and realized output, rather than duplicated diagnostic formulas.
Do not run a suite, Common Check, territory Matrix, raw reacquisition, full World
regeneration, cook, package, or R5 to discover/debug a local defect. A genuinely changed
canonical/terrain contract must use its authoritative compilation and affected-cell
dependency closure. Changes to a shared producer may legitimately invalidate all
its consumers: measure that scope and resolve it explicitly before promotion.
Focused evidence does not bypass the existing
[promotion layers](../../docs/testing/world_pipeline_layers.md) or
[World transaction](../../scripts/ue/world/README.md).

Measure affected triangle count, generation time, memory, streaming, and collision
cost at owner/fixture scale. No numerical budget is invented before the surface
contract and actual workload are known. In T4 name each wrong behavior, representative
input, exact test, expected RED, and GREEN acceptance, then obtain Q1 approval.
Editor/PIE evidence does not certify cooked collision or packaged performance.
Final release evidence remains with release-plan D19-D22.

## Documentation plan

Update the existing stable owner only if a verified contract/operational fact changes.
Use World pitfalls for a reproduced non-trivial failure with its regression. Avoid
copying the already-documented water-clearance mechanism or linking durable docs to
this todo. The release plan alone routes release sequencing and acceptance.

## Completion criteria

Investigation closes when the known visual concerns are reproduced or explicitly
refuted, each defect has a verified owner/root cause, and a reviewed implementation
plan with focused tests and bounded rollout/rollback is ready for Q1. The required
release concern closes only after the subsequently approved correction is implemented
and accepted, or the operator explicitly accepts evidence that no correction is needed.
Creating this file satisfies neither condition.

## Review record

### 2026-10-08 - required concern recorded

- D1-D3 recorded. Dedicated investigation and implementation remain unstarted.
- Stable ownership and previously fixed water-clearance incident linked as context.
- No expensive checks or runtime mutations performed.
- Independent architect R1 returned PASS for this queued investigation boundary;
  it does not approve an alignment implementation or resolve Q1.

### 2026-10-08 - attached review audited against source

- D4 recorded; source-supported road mismatch/width findings promoted to explicit
  fixture targets. Vertical-offset wording corrected; water intrusion remains a
  hypothesis. Recipe values distinguished from loaded-map evidence.
- Added shared-sampler consumer scope, clamping versus dirty-halo distinction,
  feature preparation versus clipped water heights, and crossing admission limits.
- Updated T0-T4 and invariant/test traceability; no implementation chosen, production
  test run, persistent runtime mutation, or release action performed.
- Independent architect re-review returned PASS for this expanded planning scope
  after correcting the Manhattan recipe link. It confirmed source findings,
  diagnostic-versus-acceptance limits, shared-consumer scope, and open Q1/D2 sequence;
  it does not approve an algorithm or certify runtime alignment.
- ASCII, authority IDs, local links/fragments, baseline preservation, and diff
  hygiene checked across the three documents; Git index remains unchanged.

### 2026-10-08 - native spline/remesh clarification

- Added the reviewer's conditional native-candidate clarification to T4. Configured
  launcher/source engines report 5.8.3; the three modifier headers match. Public
  declarations and current module dependency boundaries were inspected.
- The external reviewer's PASS is planning evidence; it does not close Q1 or
  supersede D2. Material-first agreement and dedicated investigation remain queued.
- Documentation checks only; no additional architect cycle, World gates, runtime
  mutation, staging, or release action. Pre-existing staged content is preserved.

### 2026-10-08 - production alternatives and read-only investigation

- D5 records the operator's explicit timing exception; D2/D3 wording retained with
  narrow partial supersessions. Q1 and material implementation approval remain open.
- Added A/B/C comparison, constructed-appearance and independent junction criteria,
  canonical fidelity/provenance, and acyclic feature-to-terrain dependency proof.
  Native modifiers remain conditional adapter choices, not adopted authority.
- Selected first-person production and Epic sample references checked. No claim
  is made that those pipelines establish ALIS's defect or justify a replacement.
- Read-only Kazan probe completed: four source-LOD ordering counterexamples agree
  with terrain-preview collision; road-only collision confirms the road heights.
  Four fixed lit captures reviewed, with temporal-control limitations recorded.
- Protected-byte inventories match for 3,739 files; editor exited without saving.
  No implementation or generation performed. T0/T1 remain partial; Q1 stays open.
- Independent architect returned PASS for the planning diff and separately for
  receipt-grounded evidence reporting. These verdicts do not certify candidate,
  continuous-clearance, compiled/package, or release acceptance.
- ASCII, authority IDs, 54 local links/fragments, unchanged historical/final R5
  definitions, unchanged empty Git index, and diff hygiene checked across the three
  documents. No staging, commit, tag, sign, push, or publication performed.

### 2026-10-08 - continued diagnosis and comprehensive proposal

- D6 recorded; the reviewer's A/B prototype wording clarification is valid and now
  explicit. Reviewer PASS grants no implementation authority.
- Attributed canonical IDs, authenticated current manifests/unit identities, and
  reconstructed all four saved road meshes. Localized water support coverage using
  actual canonical functions/corners and the production narrow/wide-water control.
- Resolved all 82 excluded Kazan samples with neighbor inputs and live preview
  collision; added Manhattan numerical comparison, overlap and grade predictions.
- Proposed the bounded combination and I0-I6 with ownership, consumer closure,
  identity/revision, refusal/recovery and regeneration ceilings. Q1-Q3 remain open.
- Three exact existing terrain-surface tests passed: pre-shaping water-fit preservation,
  authored-height clearance refusal, and line-width cutoff membership. They do not
  cover the new continuous/full-footprint guarantees; I1 must add those regressions.
  Each ran with `python -B -m unittest` and prefix
  `tools.World.CanonicalCompilation.tests.test_terrain_surface.TerrainSurfaceTests.`:
  `test_hand_height_runs_after_hydro_and_preserves_water_fit_base`,
  `test_hand_height_that_breaks_water_clearance_fails_closed`, and
  `test_prepared_distance_preserves_line_width_at_cutoff`.
- Independent architect review returned PASS for the final planning diff against
  source and receipts. It confirms the proposal's boundary and evidence limits,
  not an approved or implemented geometry algorithm.
- ASCII, 88 local links/fragments across the three current documents, stable authority
  IDs, open Q1-Q3, and diff hygiene checked. Only this todo changed; the material todo,
  release router, historical R5/performance definitions and empty Git index are preserved.
- No implementation/prototype, production generation, Common Check/Matrix, cook,
  R5, staging, commit, tag, sign, push or publication. Read-only state proof covers
  the same 3,739 files; both agent-owned editors exited without saving.

### 2026-10-08 - simplification review corrections

- D7 recorded. Proposed Q1-Q3 answers remain unapproved; correctness is staged
  independently of grading/crossing reconstruction. Release completion still requires
  accepted appearance and explicit treatment of any remaining semantic exceptions.
- Replaced mandatory A+B sequencing and whole-feature hashing with scoped correctness,
  matching bounded tangent consumption/identity and conditional grading/representation.
- Added planar intersection-extrema proof with separate coverage/numerical/native
  guards, and water-quality refusal before persistent conditioning. The 7.2047 m
  water example belongs to Kazan; Manhattan's low-confidence cases are smaller.
- Independent architect correction recheck returned PASS for this planning diff;
  no new algorithm or runtime acceptance is certified. No additional architecture
  framework, todo, prototype, editor run, production test run or broad gate was added.
- ASCII, local links/fragments, stable authority IDs, open Q1-Q3, unchanged material/
  release documents and Git index, and diff hygiene checked. Only this todo changed;
  no implementation, staging, commit, generation, cook or release action occurred.

### 2026-10-08 - Q1 implementation checkpoint

- D8 authorizes this correctness implementation. CHANGED owners: Road editor
  realization/identity and its output revision, Core's plain canonical triangle
  geometry, Canonical Compilation's hydro support/profile schema, owner-local
  tests and existing stable documentation. UNTOUCHED: Mesh Terrain implementation,
  Water realization algorithm, other leaf geometry, ProjectMaterial/ProjectTexture,
  production profiles, canonical bundles, generated production packages/manifests,
  material todo, release router and historical R5/performance authority.
- Road ribbon preparation and hashing share bounded tangent context. The global
  nearest fallback is removed. An indexed neighboring-cell projection authenticates
  positive-area width dependencies without building finished meshes during hashing.
  Output splits at canonical terrain triangles; geometry supplies normals/tangents.
  The admitted outer bounding rectangle clips width; missing interior coverage
  refuses without emitting a partial description. Road output revision is 2;
  production revision-1 output has not been regenerated or accepted as revision 2.
- Canonical hydro support constrains incident quads on the existing lattice and
  covers the extra incident support ring needed for shared halo equality. Old/new
  support influence participates in dirty closure. Prepared footprint bounds,
  segments and cached feature height floors are reused. Standing levels and the
  minimum finite flowing knot bound final affine water triangles; no Water refit,
  adapter deformation or constrained-surface schema was introduced.
- Additional conditioning requires explicit maximum cut, support distance and
  accepted function accuracy. Missing policy, exceeded limits and authored-height
  violations refuse before any input terrain cell changes. No production limits
  were selected. A long/sloping feature can therefore be infeasible on this coarse
  lattice; report that result rather than silently choosing a large carve.

RED evidence, against production functions/builders:

| Exact test | Expected failure observed before its fix |
|---|---|
| `Project.World.Realization.Road.FullFootprintConformity` | Continuous road error 2.55555542 m on the terrain-diagonal/ribbon fixture |
| `Project.World.Realization.Road.ConsumedGeometryIdentity` | Consumed tangent continuation changed while unit hash stayed identical |
| `tools.World.CanonicalCompilation.tests.test_terrain_surface.TerrainSurfaceTests.test_narrow_water_between_samples_has_continuous_clearance` | 3 m Water ribbon between 30 m samples left terrain at 12 m over a 10 m water function |
| `tools.World.CanonicalCompilation.tests.test_water_coverage.WaterCoverageTests.test_between_samples_neighbor_dirty_closure` | Neighbor water edit omitted the incident-support cell from the dirty set |

Additional coverage control caught and corrected a polygon contained wholly inside
a quad whose corners were outside water. This was an intermediate implementation
defect, distinct from the baseline ribbon RED. Refusal tests assert exact in-memory
cell equality, not only an error code. The permanent fixtures contain their own
inputs and do not read investigation receipts.

GREEN execution uses exact UE IDs through
`scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter <one-exact-ID>` with normal
RHI, `-TimeoutSeconds 180`, and a per-test `-LogDirectory` under
`tmp/world/alignment/q1-correctness/receipts/`. Python uses
`python -B -m unittest <exact-test-ID>` or the scratch runner loading only the
12 explicitly named owning tests; no module discovery/suite wildcard.

- `Road.ConsumedGeometryIdentity`: tangent and consumed-neighbor invalidation,
  remote geometry invariance, batch/single identity agreement, outer clipping and
  missing interior coverage refusal. 200 indexed fixture unit hashes took
  0.020491 seconds; this is a two-cell fixture cost, not a territory/runtime budget.
- `Road.FullFootprintConformity`: full source-ribbon height ordering, actual
  StaticMesh build, disposable save/reload, compiled fallback and collision
  triangle ordering, winding/normals, and three cooked collision hits. Height
  tolerance is 0.0001 m for this fixture. Complete analytic footprint area and
  unchanged-input no-op checks are included in the final run.
- `Alignment.TriangleClearanceOracle`: overlap-intersection burial, canonical-scale
  translation, disjoint and point-touch controls, degenerate/non-finite refusal.
- `Alignment.WaterTriangleEnvelope`: final native standing/flowing triangulation
  and cell clipping, positive coverage, hole exclusion and the conservative floor.
- Twelve focused Python checks cover narrow/polygon interior coverage, monotonic
  flow and shared halos, dirty neighbor closure, quality/authored refusal and exact
  unchanged inputs, schema limits and existing sampled/semantic guards.
- The narrow fixture declares maximum cut 2 m, maximum support distance 45 m,
  and accuracy `fixture`. Its 30 m x 30 m core lowers uniformly by 2 m: core cut
  volume 1,800 m3, no fill. This conservative fixture result is not a production
  bank-shaping policy or approval to flatten production cells.

Stable contracts are in World territory contract/pitfalls and Canonical Compilation
README. Producer descriptors parse; incremental builds use the project wrapper.
ASCII/local link targets, diff hygiene, machine-path and reusable-name governance
are checked. The same 3,739 protected production Data/Content paths remain
byte-identical and the Git index remains empty. Independent non-author final
review returned PASS for the bounded Q1 source and receipts with no required
gap within this slice. All four exact native targets and 12 Python checks passed.

Remaining acceptance stays explicit: I2's production water quality policy and
compiled terrain/collision comparison, I5's representative city/placement/visual
integration and measured production cost, and I6's transactional rollout remain
open. Grading and ambiguous crossing reconstruction remain Q2/Q3. Q1 fixtures do
not close this release concern, authorize production promotion, change the
16.67 ms/INCONCLUSIVE policy, or authorize final frozen R5/R6/publication.
