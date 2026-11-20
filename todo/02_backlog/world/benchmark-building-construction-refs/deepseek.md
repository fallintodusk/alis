# Candidate: data-driven modular building reconstruction (World)

Status: investigation and proposal. No implementation, no Unreal execution, no
canonical or generated-authority change.

Markers used throughout:

- `[V]` verified from current code, schemas, data or SOT text in this repository.
- `[M]` measured by me from the promoted canonical bundle by reproducing the
  code's predicates. A measurement, not a native gate receipt.
- `[P]` proposal. Not accepted, not implemented, may be wrong.
- `[?]` unresolved question that changes the design if answered differently.

---

## 0. Verdict

The current pair - `ProjectWorldBuildingEditor` adapter plus
`ProjectBuildingAssembly` - is a sound *transactional skeleton* around an
*insufficient building model* for the stated goal.

`[V]` What exists is a parameterized box generator: one inscribed rectangle per
building, four normalized part families, one material, one actor, one producer
tuple. `[V]` What does not exist is any authority for a building *program*:
there is no `Data/Buildings/`, no archetype document, no interior, no detail
placement, no foundation strategy beyond "one slab spanning the support range",
and no consumer that requires enterable interiors (no navigation, interaction or
lighting contract for them).

`[V]` The two claims the backlog item rests on do not survive inspection:

1. "JSON building blueprints will describe each building" is an accepted
   *direction*, not a contract. The only building data document in the tree is
   a nine-parameter kit. Extending the proof is therefore not "adding a case to
   an existing workflow"; it is defining the missing data contract first.
2. "Terrain development prerequisite is satisfied" is true only in the narrow
   sense that no canonical Terrain writer is needed to place a building. It is
   false for building support: the object layer today can express exactly one
   foundation - a rectangular slab from `support_max` down to
   `support_min - embed` - and refuses everything wider than 0.5 m of span.
   Building-side Terrain adaptation is unbuilt object geometry, not satisfied
   infrastructure.

`[M]` The binding constraint is the *input* contract, not the kit. Of 30,191
canonical buildings in the promoted Kazan territory, 7,297 (24.2%) pass the
current geometry gates before Terrain coverage and span are considered; 63% are
simply not quadrilaterals. Interiors, details and terrain strategies add value
only inside the class that survives the input gate, so generalizing the envelope
comes first.

`[P]` Recommended shape, in one sentence: keep the owner split and the layer
transaction, and replace the plan/kit contract with (a) a qualified envelope,
(b) a kit catalogue that declares dimensional semantics per module, (c) a
deterministic program resolution over policy -> archetype -> per-building
override, (d) two realization classes - shared kit instances and exact
per-building generated prisms - and (e) explicit fidelity tiers with refusals
instead of silent degradation.

---

## 1. Evidence and its limits

Read in full or in the relevant sections:

| Area | Sources |
|---|---|
| Backlog item | `todo/02_backlog/world/20261112-1332_world_extend_building_assembly.md` |
| Assembly contract and history | `Plugins/World/ProjectBuildingAssembly/docs/decision_record.md`, `Plugins/World/ProjectBuildingAssembly/docs/architecture/structure.md`, `Plugins/World/ProjectBuildingAssembly/docs/pitfalls.md`, `Data/README.md`, `Data/Kits/README.md`, `Tests/README.md` |
| Assembly implementation | `Source/ProjectBuildingAssemblyEditor/Public/ProjectBuildingAssemblyPlan.h`, `Public/ProjectBuildingAssemblyRealization.h`, `Private/ProjectBuildingAssemblyPlan.cpp`, `Private/ProjectBuildingAssemblyKit.cpp`, `Private/ProjectBuildingAssemblyRealization.cpp` |
| World adapter and persistence | `Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingAssembly.{h,cpp}`, `ProjectWorldBuildingSelection.cpp`, `ProjectWorldBuildingSelectionPersistence.cpp`, `ProjectWorldBuildingLayerProducer.cpp`, `ProjectWorldBuildingMeshBuilder.cpp` |
| Canonical input contract | `ProjectWorldCanonicalBundle.h/.cpp`, `Data/Producers/project_building_massing.json`, `Data/Schemas/project_world_building_selection.schema.json`, `Plugins/World/ProjectWorld/docs/territory_contract.md` |
| Upstream building data | `tools/World/CanonicalCompilation/app/buildings.py`, `app/adapters.py` |
| Verification contracts | `docs/testing/world_pipeline_layers.md`, `Plugins/World/ProjectWorldTestData/Data/Fixtures/building_neighborhood/README.md`, `Tests/run.py`, `Tests/plan_test.cpp` |

Limits that matter for reading the numbers in section 3:

- No editor session, no native test, no compile. Nothing here is acceptance
  evidence for the current implementation.
- All `[M]` numbers come from the promoted bundle
  `Plugins/World/ProjectWorldData/Data/Canonical/kazan_territory_v1/` (active
  bundle `75b8eefd...287b8`, 642 outputs, 30,191 buildings) by re-implementing
  the code's predicates in Python. The rectangle test, the cross-cell test and
  the water-bounding-box test are faithful transcriptions. The Terrain span
  figure comes from 5x5 bilinear sampling of core samples with edge clamping,
  not from the refined triangle field and not from the full coverage test; it
  is an order-of-magnitude indicator.
- I did not inspect the installed engine's PCG or Mesh Terrain API. Nothing here
  depends on those.

---

## 2. What exists today (verified)

### 2.1 Authority chain

```text
source (OSM) -> canonical logical Building + effective_volumes    [canonical compiler]
             -> ProjectWorld canonical bundle (volumes only)       [native reader]
             -> World adapter: qualify rectangle + Terrain        [ProjectWorldBuildingEditor]
             -> Assembly: layout + ISM realization               [ProjectBuildingAssembly]
             -> World: selection doc, kit package, actor package  [World layer transaction]
             -> massing builder: suppress the selected blockout   [project_building_massing]
```

`[V]` The dependency direction holds: `ProjectBuildingAssemblyEditor.Build.cs`
has no ProjectWorld dependency; `ProjectWorldBuildingEditor.Build.cs` depends on
`ProjectBuildingAssemblyEditor`.

`[V]` Selected identity is opt-in through the producer setting
`assembly_source`, whose value must be a confined relative `*.json` under
`BuildingAssembly/` or `tmp/world/inputs/`; the document is capped at five
`{feature_id, kit_source}` records.

### 2.2 The admitted input (the gate)

`[V]` `ProjectWorldBuildingAssembly::Prepare` refuses the whole feature unless
all of the following hold:

| Condition | Consequence |
|---|---|
| feature exists, class `building`, owned by the target cell, in `OwnedFeatureIds` | otherwise refuse |
| exactly one effective volume | multi-volume refused |
| `CivilGrade` unset, `ConstructedSurface` unset | buildings on civil grades or constructed surfaces refused |
| `MinHeightMeters == 0` | elevated / stacked volumes refused |
| exactly one polygon, no holes | multipolygon and holed footprints refused |
| ring has 4 corners after dropping a closing duplicate | any other vertex count refused |
| all four corners strictly inside the owning cell bounds | cross-cell buildings refused |
| the quad is a rectangle within `inset/2` (inset = max(0.03, 2*quantization)) | quads that are not rectangles refused |
| the inset footprint lies inside the original ring | non-convex or skewed quads refused |
| no Water feature's accumulated XY bounding box intersects the footprint box | over-broad water refusal |
| the final Terrain triangle field covers the footprint with exact total area | unknown or partial support refused |
| `support_max - support_min <= kit.maximum_support_span_m` (0.5 m) | sloped ground refused |

`[V]` Then `Input.SupportSpan = support_max - support_min`, `Input.Height =
volume.HeightMeters`, and the actor origin sits at `(footprint origin,
support_max)` with yaw from the first edge.

### 2.3 The plan

`[V]` `Build` in `ProjectBuildingAssemblyPlan.cpp` is ~40 lines and does this:

- `Floors = max(1, floor(Height / Kit.FloorHeight + 0.5))`, then
  `Step = Height / Floors` - the kit's story height is *overridden*, it is only
  a divisor guess. The retained Manhattan case (height 7.5, kit 3.0 m) yields
  three 2.5 m stories, and `plan_test.cpp` asserts exactly that.
- `NX = ceil(Width/PanelWidth)`, `NY = ceil(Depth/PanelWidth)`; every panel is
  stretched to `length/count`.
- Per story, per side: `EPart::Window` when `panel % WindowEvery == 0`, else
  `EPart::Wall`, all with `Yaw in {0, 90}`.
- One entrance: the first ground-floor panel of side 0 is *omitted*, leaving a
  full-story opening with no frame, no lintel, no door, and nothing behind it.
- One foundation box from `-(span+embed)` to slab thickness, per-story slabs,
  one roof.
- Refusals are structural: non-finite/non-positive dimensions, support span,
  thickness vs footprint, capacity, instance budget.

`[V]` Realization makes exactly four procedural meshes: a unit box scaled per
instance, and a "window" made of four normalized bars at
`FrameFraction = 0.12` of the module. `Matches` re-derives the expected triangle
set, checks per-instance transforms, component names `Part0..Part3`, one
material, `CTF_UseComplexAsSimple`, non-double-sided, query-and-physics, and the
actor transform. `GeometryIdentity` hashes feature id, transform, frame
fraction, every instance, and all four modules' triangles.

### 2.4 What the current proofs actually cover

`[V]` Portable host test: one captured Manhattan footprint, determinism, four
part families, bounds, slab contact, one kit change, six refusals.
`[V]` Native isolated case: one Manhattan building, no-op, drift, unowned
refusal, authored protection, Water width, strict types, physical support,
save/reload, winding.
`[V]` Registered lifecycle: two cells, one then three buildings, four shared
meshes, kit-only change, failed apply after a real actor save, cold
reconstruction, selection removal, restoration.
`[V]` Not covered anywhere: non-rectangular footprints, multi-volume buildings,
cross-cell buildings, slope above 0.5 m, interiors, details, more than five
selected buildings, per-cell budgets, or any production territory admission.

### 2.5 Invariants worth keeping unchanged

These are the parts of the current design that are genuinely good and that a
redesign must not weaken:

1. `[V]` No-op proof is behavioural: actual meshes, instance transforms,
   materials, collision configuration, actor transform, owner tags. A stale tag
   cannot fake it.
2. `[V]` Replacement is staged and verified before the previous actor is
   removed; an unowned actor with the feature tag refuses.
3. `[V]` Kit packages are shared, owned under the layer artifact root, referenced
   by actors, retired only when no admitted selection uses them.
4. `[V]` Capture refuses when the admitted actor set, geometry, spatial flags,
   navigation flags, HLOD flags, external package identity or physical roof
   contact differ from the plan.
5. `[V]` Massing suppression follows the complete classifier and refuses if the
   selected feature is absent, authored-masked or not admitted massing.
6. `[V]` Terrain artifacts participate in per-cell input identity.
7. `[V]` Assembly is engine-free in its core and has no Terrain writer.

---

## 3. Measured admission funnel

`[M]` Promoted Kazan territory, 30,191 canonical buildings:

| Stage | Count | Share |
|---|---|---|
| all buildings | 30,191 | 100% |
| single effective volume | 29,960 | 99.2% |
| ... and single polygon without holes | 29,855 | 98.9% |
| ... and exactly 4 unique corners | 18,763 | 62.1% |
| ... and an actual rectangle | 7,719 | 25.6% |
| ... and all corners strictly inside the owning cell | 7,447 | 24.7% |
| ... and no Water bounding-box intersection | 7,297 | 24.2% |
| ... and Terrain span <= 0.5 m (approximate, 4 dense cells) | ~59% of the above | ~14% |

Supporting facts that the design has to respect:

| Fact | Value |
|---|---|
| multi-volume buildings | 231 (max 22 volumes) |
| multipolygon or holed footprints | 105 |
| heights | 2.0 .. 150.0 m, median 9.0 m |
| buildings with source-derived `levels` | 13,330 (44%) |
| `building_class` present | all 30,191 |
| class distribution | `yes` 16,323; `apartments` 4,115; `detached` 2,418; `house` 2,187; `garages` 1,016; `retail` 690; `commercial` 590; `roof` 284; `office` 183; ... |
| approximate Terrain span over rectangular footprints | p50 0.37 m, p90 1.65 m, p99 3.58 m, max 5.50 m in sampled cells |

Interpretation:

- `[M]` Generalizing the envelope from "inscribed rectangle" to "the actual
  polygon" is worth roughly three times the addressable building set. Nothing
  about interiors or details changes that ratio.
- `[M]` The Terrain gate is not a rounding problem. About 8% of rectangular
  footprints in dense cells span more than 2 m, and the current foundation
  vocabulary cannot express a stepped or skirted base at all.
- `[V]` `building_class`, `levels`, `height_basis`, `min_height_basis` and
  `volume_selection` exist in the canonical JSON but are dropped by the native
  reader, so the planner cannot yet honor the source's own story count.
- `[V]` The canonical compiler already produces part-based effective volumes and
  a `volume_selection` decision (`complete_parts` / `outline_no_parts` /
  `outline_fallback_incomplete_parts`); the native side discards that
  provenance, which is precisely the information a faithful program needs.

---

## 4. Critical assessment

### F1. "Data-driven" has no data contract yet

`[V]` The only building data document is `Data/Kits/original_test_v1.json`:
`kit_id`, `generator`, `provenance`, and nine numbers. There is no
`Data/Buildings/`, no archetype, no room or facade program, no detail palette.
The native kit reader is a closed hand-written parser that requires exactly five
top-level fields and exactly nine parameters, and `Build` validates each of them
by name.

Consequence: any new expressive capability is a C++ change to the reader *and*
the validator *and* the schema *and* the identity function. That is acceptable
for one or two fields; it is not a foundation for a kit catalogue. Verdict:
keep the closed-document discipline, replace the hand-written positional parse
with a versioned, table-driven parse of a catalogue document.

### F2. The input gate, not the kit, is the binding constraint

`[V]` One volume, no holes, exactly four corners, rectangle, single cell, no
Water bbox overlap, full Terrain coverage, <= 0.5 m span.
`[M]` 24.2% of the city survives the geometry stages alone.

Consequence: the backlog's "choose a bounded real neighborhood" is not a free
choice - the neighborhood has to be *searched* for five buildings that qualify.
The current proof's area (15.77 x 5.31 m, 3 stories) is at the easy end of the
distribution. Verdict: envelope generalization first; it also reduces the
selection-search risk for every later phase.

Also verified: the rectangle test silently shrinks the building by 3 cm per
side and the "inside the original volume" check then re-validates the inset
ring. That convention is fine, but it is undocumented in the schema and must be
carried into the polygon version explicitly.

### F3. The plan is a box generator, not a modular grammar

`[V]` Four `EPart` values, exactly four ISM components named `Part0..Part3`,
exactly one material, `Yaw in {0, 90}`, wall panels stretched to
`length/count`, entrance = "skip a panel".

Three consequences, in order of severity:

1. `[V]` All modules are normalized and scaled per instance. Any detail whose
   metric size matters in world space - a 45 mm door jamb, a 180 mm stair rise,
   a handrail - is corrupted by proportional scaling. `FrameFraction = 0.12` is
   the existing proof of the problem: the window frame gets thicker with the
   window.
2. `[V]` There is no negative space except the four-bar window frame, so a door
   opening cannot be built from the current catalogue at all.
3. `[V]` Interiors and details need more families, which breaks
   `Components.Num() != 4`, the `Part<N>` naming, and the identity hash that
   folds exactly four modules.

Verdict: replace the part enum with a catalogue of families and modules, and
make "how this module may be scaled" an explicit property. This is the single
most important structural change after the envelope.

### F4. Story fidelity is contradicted by the retained proof

`[V]` `Floors = round(Height / Kit.FloorHeight)` then `Step = Height / Floors`.
The kit's story height affects only the rounding. Source `levels` is dropped
before the planner sees the building.

`[V]` The retained Manhattan feature `alis:osm:way:250440683` asserts
`height_m = 7.5` with `height_basis = source-derived` and asserts **no**
`levels` (class `yes`, `volume_selection = outline_no_parts`). With a 3.0 m kit
story the planner builds three 2.5 m stories, and `plan_test.cpp` asserts it.

Consequence: two authorities contradict each other and the planner silently
picks neither. Fitting stories to height gives 3 x 2.5 m and violates the kit's
story module; fitting height to stories gives 2 x 3.75 m and violates the kit's
nominal size. Where the source does assert `levels` (44% of Kazan buildings),
the current code ignores it entirely. For a survival game where a player counts
floors from the street and enters rooms, this is not cosmetic. Verdict: story
resolution must be an explicit, recorded decision - honor `levels` when
source-derived, otherwise declare whether it fits stories to height or height to
stories - and refuse when the resulting story height falls outside the kit's
declared range instead of silently rescaling.

### F5. Interiors have no authority, no consumer, and no tier

`[V]` OSM carries no interior data. The canonical model has no interior
concept. The native side has none. `territory_contract.md` states building
massing is "territory blockouts only: no interiors, doors, gameplay placement".
The producer settings require `navigation=no_navigation`. The facade's entrance
today is a missing panel.

Consequence: "including interiors" is three different features that the backlog
conflates:

1. **Interior shell** - floors, partitions, stairs, door openings; physics
   collision is already on for every module, so the shell is physically
   enterable the moment a door exists, but has no navigation, no interactive
   door, no lighting, no gameplay anchors.
2. **Enterable interior** - adds navigation, interaction, lighting and a reason
   to enter. This is a gameplay decision with a real consumer, not a geometry
   feature.
3. **Interior contents** - placeable objects, loot, spawn points; belongs to the
   gameplay-placement layer, not to assembly.

Verdict: build (1) inside a declared "non-navigable shell" tier, refuse any
attempt to treat it as (2) until a consumer exists, and leave (3) to the object
layer.

### F6. "Terrain adaptation" is object-side work, and the vocabulary is missing

`[V]` The only adaptation today is a single foundation slab spanning
`support_min..support_max`; above 0.5 m the building refuses. `[V]` Assembly
cannot write Terrain and should not.
`[P]` Nearly all real building-ground adaptation is object-side: a level datum
at the high side, a skirt or plinth down to the low side, stepped footings at
bay boundaries, a threshold stoop at the entrance, a retaining wall along the
uphill face. These are geometry the building owner can produce without touching
the single Terrain writer.

Consequence: the correct design question is not "how do buildings influence
Terrain" but "what foundation strategies can the building express, what limit
does each have, and when must the case refuse and hand the residual to the
separate canonical-influence task". Verdict: turn the single span gate into a
declared strategy table (section 5.6.6), keep the Terrain writer untouched, and
record the residual refusal rather than inventing a second writer.

### F7. Authored uniqueness cannot coexist with a generated assembly

`[V]` `Prepare` refuses any feature that has a feature-anchored authored overlay.
The only way to make a building unique is therefore to *not* assemble it.

Consequence: "unique details" is currently a binary choice between procedural
sameness and fully authored hero geometry. A hybrid - generated shell plus
authored detail intent for a named building - has no contract. Verdict: either
uniqueness lives *inside* the generated program (authored override document,
still disposable) or the overlay contract gains an explicit "detail overlay on
an assembled feature" mode with a parent transform rule. Pick one; do not leave
the refusal as the de-facto answer.

### F8. Identity is strong but not attributable

`[V]` `GeometryIdentity` folds the whole plan and all four modules' triangles
into one digest. `Matches` re-derives the same triangles. This is excellent: no
stale tag can pass. But everything is one string, so:

- `[V]` any module geometry change invalidates every selected building;
- `[V]` there is no way to say "the facade changed, the interior did not";
- `[V]` generated per-building geometry (if added) has no home in the identity
  unless it is folded in, and the current persisted path *requires* shared
  modules (`Options.Modules.Num() != 4` refuses external actors).

Verdict: keep re-derivation as the proof mechanism, but make identity a
structured digest: program digest, kit catalogue digest, per-family module
digests, generated-surface digests, support band, transform. Then dirty scope
and diagnostics become attributable.

### F9. Ownership is single-actor by construction

`[V]` `Apply` refuses a second actor carrying the same feature tag; capture
refuses duplicates; retirement is per feature. External actors must use exactly
four shared modules.

Consequence: interiors as a second actor, or per-building generated surfaces in
the persistent path, need explicit ownership work: either embed generated meshes
in the actor package (natural, and already proven in the isolated path) or add a
role suffix to the ownership tag domain and teach capture/retire about it.
Recommendation: one actor, embedded generated meshes, families as components;
no second actor.

### F10. Scale is unmeasured and unbudgeted

`[V]` A per-plan capacity check exists (kit `maximum_instances`, default 4096,
checked against the planned instance count) plus a hard 65536 in `Apply`. There
is no per-cell budget and no actor/mesh budget. `[V]` The lifecycle receipt keeps
per-run timings, but only for one and three selected buildings; nothing measures
a neighborhood, an interior, or a prism-heavy plan.

Consequence: interiors can multiply instance counts by an order of magnitude;
without budgets the next phase discovers the limit in a failed gate. Verdict:
the backlog's "measure selected-cell reclassification/massing" is the right
instinct - keep it, and add instance/mesh/actor budgets to the phase that first
needs them.

### F11. The retained follow-up list is mis-prioritized and incomplete

| Backlog follow-up | Assessment |
|---|---|
| faithful regression for unsupported footprint / cross-cell / Terrain span | correct, but it is a *test*, not a design; keep as the gate of the envelope phase |
| replace the 10x10x6 kit probe with target-independent admission | correct and small; the real fix is per-placement qualification plus dimensional classes, not a bigger probe |
| choose a bounded real neighborhood and qualify it | correct, but needs a native admission-funnel report first; 24% survival makes manual choice unreliable |
| qualify material-reference-only adoption | presentation axis; keep deferred, must not gate geometry |
| measure reclassification/massing before reuse | correct; promote to an explicit budget step in the interior phase |
| *missing*: building program authority (policy/archetype/override) | the actual blocker for "data-driven" |
| *missing*: story fidelity from `levels` | contradicts the retained proof today |
| *missing*: interior/capability tiers | prevents an accidental gameplay commitment |
| *missing*: authored-detail coexistence | makes "unique details" impossible for generated buildings |
| *missing*: foundation strategies and their limits | the real Terrain-adaptation owner |
| *missing*: per-building generated geometry ownership/identity | required for exact slabs, roofs and foundations |

### F12. Smaller verified defects worth fixing in passing

- `[V]` `Prepare` builds its remaining-massing cross-check with a default
  `FProjectWorldBuildingSettings()` (300 m) instead of the layer's settings, so
  the cross-check can disagree with the layer's own `maximum_height_m`.
- `[V]` Water refusal is bounding-box based across the whole Water feature, so a
  long river's bbox refuses buildings far from the water; 150 buildings are lost
  this way in Kazan.
- `[V]` `ReadKit` requires exactly five top-level and nine parameter fields, and
  the schema path must resolve to this plugin's own
  `Data/Schemas/modular-kit.schema.json`; a second kit generation cannot be
  admitted by data alone.
- `[V]` The entrance is an omitted panel, not a door: no lintel, no threshold,
  no frame.
- `[V]` The two document classes use different path roots: the selection document
  is data-root-relative (`BuildingAssembly/...`), while `kit_source` must be
  spelled repo-relative (`Plugins/World/ProjectBuildingAssembly/Data/Kits/...`).
  Adding an override document would have to pick one rule deliberately.

---

## 5. Recommended architecture

### 5.1 Ownership and data

`[P]` Keep four owners and add no fifth:

| Owner | Owns | Does not own |
|---|---|---|
| ProjectWorld (canonical + adapter) | geography, volumes, final Terrain, support qualification, selection, layer transaction, package ownership | building grammar, interior layout, foundation geometry |
| ProjectBuildingAssembly (pure core) | envelope -> program -> plan, determinism, refusals | JSON parsing, Unreal assets, Terrain |
| ProjectBuildingAssembly (native edge) | catalogue reading, module geometry, ISM realization, identity re-derivation | canonical types, selection |
| Data | kit catalogue, archetypes, per-building overrides | engine behavior |

`[P]` Three data documents, each with one job:

```text
Data/Kits/<kit>.json          what parts exist and how they may be used
Data/Archetypes/<name>.json   how a class of building is composed from parts
Data/Buildings/<feature>.json sparse authored intent for one named building
```

Plus the existing `Data/Schemas/*.schema.json` and World's selection document.
Nothing else becomes a building authority.

### 5.2 The qualified envelope (World -> Assembly)

`[P]` Replace `FInput{FeatureId, Width, Depth, Height, SupportSpan, Kit}` with a
plain-data envelope. It is still engine-free and still produced only by World.

```cpp
struct FPoint2 { double X, Y; };

struct FSupportSample { FPoint2 P; double Z; };   // optional, per bay or per vertex

struct FEnvelope
{
    std::string FeatureId;
    std::vector<FPoint2> Outer;        // local metres, CCW, no closing duplicate
    double Height = 0;                 // above the local datum (max support)
    double BaseOffset = 0;             // reserved: stacked/elevated volumes
    double SupportMinZ = 0, SupportMaxZ = 0;
    std::vector<FSupportSample> Support;   // empty in v1 strategies that need only the range
    int EntranceEdge = -1;             // -1 = let the program decide
    double EntranceStation = 0;
};

struct FSourceSemantics            // what the source actually asserted
{
    std::string BuildingClass;     // "apartments", "retail", "yes", ...
    int Levels = 0;                // 0 = not asserted
    ERuleBasis LevelsBasis = ERuleBasis::Unknown;   // SourceDerived | Inferred | Default
    ERuleBasis HeightBasis = ERuleBasis::Unknown;
};
```

`[P]` Two rules for the World adapter:

1. Keep every refusal it has today, generalized: self-intersection, duplicate
   vertices, minimum area, holes (v1), `BaseOffset != 0` (v1), no full Terrain
   coverage, unknown support height. Replace "rectangular" with "simple ring
   with at least three distinct vertices and no self-intersection".
2. `[P]` Supply the *support field* only as far as the chosen strategy needs it.
   A conform strategy needs the range; a stepped strategy needs per-bay maxima;
   a skirt strategy needs the range and the perimeter. Do not turn World into a
   geometry service.

`[P]` Entrance intent: derive from road adjacency in World (nearest road segment
within a declared distance, projected onto the nearest footprint edge), record
it as derived intent, and allow a per-building override. `[?]` If road adjacency
is deemed out of scope, the fallback must be deterministic and honest: the
longest edge, recorded as `default_longest_edge`, not passed off as observed.

### 5.3 Kit catalogue v2: dimensional classes

`[P]` The core problem in the current kit is that every module is normalized and
scaled. The catalogue fixes that by declaring, per module, how geometry may be
placed.

```jsonc
{
  "schema_version": 2,
  "kit_id": "original_test_v2",
  "generator": { "id": "procedural_box_modules", "version": 2 },
  "provenance": { "kind": "original_procedural_test_geometry", "owner": "project" },
  "units": "metres",
  "families": [
    { "id": "facade", "component": "Facade", "collision": "query_and_physics" },
    { "id": "interior", "component": "Interior", "collision": "query_and_physics" },
    { "id": "detail", "component": "Detail", "collision": "query_only" }
  ],
  "modules": [
    {
      "id": "wall_panel",
      "family": "facade",
      "shape": { "kind": "box" },
      "dimensional_class": "ratio",
      "stretch_axes": ["x", "z"],
      "nominal_size_m": [2.0, 0.18, 3.0],
      "min_size_m": [0.4, 0.18, 2.2],
      "max_size_m": [4.0, 0.18, 4.2],
      "slot": "facade.solid"
    },
    {
      "id": "window_frame",
      "family": "facade",
      "shape": { "kind": "frame", "jamb_m": 0.08, "sill_m": 0.05 },
      "dimensional_class": "metric",
      "metric_size_m": [1.2, 0.18, 1.5],
      "anchor": "sill_center",
      "slot": "facade.window"
    },
    {
      "id": "door_frame",
      "family": "facade",
      "shape": { "kind": "frame", "jamb_m": 0.07, "sill_m": 0.0, "head_m": 0.12 },
      "dimensional_class": "metric",
      "metric_size_m": [1.0, 0.18, 2.1],
      "anchor": "base_center",
      "slot": "facade.door"
    }
  ]
}
```

`[P]` Three properties do all the work:

- `dimensional_class: ratio` - geometry is defined in the unit cube and may be
  scaled only along `stretch_axes`, within declared min/max. Instanced as one
  ISM family.
- `dimensional_class: metric` - geometry has a world size; instances are placed
  at declared size only. If a wall opening of that size is needed, the wall is
  *composed* from piers and a lintel (both `ratio` or `metric` modules), never
  by stretching the frame.
- `slot` - the vocabulary the program resolves against. Slots, not part enums,
  are what a facade or interior rule names.

`[P]` Families are the ISM component axis and the identity axis. In the migration
the four existing parts become four families (`facade`, `opening`, `slab`,
`roof`) with the same generated geometry, so phase 2 can be visually identical
before any new module is added; the `interior` and `detail` families in the
sketch above arrive later.

### 5.4 Program resolution: policy -> archetype -> override

`[P]` A building program is resolved in three levels, most specific wins:

1. **Policy** - a table in the archetype document mapping source semantics to an
   archetype: `building_class` + height/levels band + `volume_selection`.
   Unknown class or missing data resolves to a declared fallback archetype, and
   the fallback is recorded in the program digest.
2. **Archetype** - how the building is composed: bay rhythm and filler policy,
   story resolution rule, facade slots per story and side, roof type, interior
   pattern, detail palette and weights.
3. **Override** - sparse, per feature id, authored: stories, entrance, roof,
   interior pattern, specific detail records. Closed schema, bounded record
   count, hashed into identity.

`[P]` Resolution is deterministic and pure (no engine RNG): a stable FNV-1a over
`feature_id + archetype_id + slot + occurrence index` selects from weighted
palettes. Same input, same plan, on any host.

`[P]` Every resolved value carries its basis (`source_derived`, `inferred`,
`default`, `authored`) and the basis is part of the program digest. This is what
stops a default from being remembered later as if it were observed - the same
discipline the canonical compiler already applies to height and width bases.

### 5.5 Plan v2: two realization classes

`[P]` The plan exposes two kinds of output, because they have different
correctness and ownership semantics:

```cpp
struct FKitInstance
{
    std::string Family, ModuleId;
    double X, Y, Z;            // local metres, module anchor
    double Yaw;                // degrees
    double ScaleX, ScaleY, ScaleZ;   // 1.0 for metric modules
};

struct FGeneratedPrism            // exact, per building
{
    std::string Family;                  // "slab", "roof", "foundation", "skirt"
    std::vector<FPoint2> Outer;          // may be the footprint inset/offset
    std::vector<std::vector<FPoint2>> Holes;
    double ZBottom, ZTop;
};

struct FPlan
{
    std::vector<FKitInstance> Instances;
    std::vector<FGeneratedPrism> Prisms;
    std::string ProgramDigest;
    int Stories = 0;
    double StoryHeight = 0;
    double FloorDatumZ = 0;              // above support_max
};
```

- `[P]` Kit instances realize repetition: wall panels, window frames, doors,
  partitions, stairs, details. Shared meshes, ISM, reused across buildings.
- `[P]` Generated prisms realize exactness: floor slabs, ceiling/roof caps,
  foundation skirt, plinth rings, stair openings. One mesh per building per
  family, embedded in the building actor's package.
- `[P]` A prism must be triangulated deterministically in the pure core (ear
  clipping with a stable vertex order) so the identity digest and the native
  mesh are the same function of the same input.

Why this split and not "everything instanced": slabs and roofs must match the
actual polygon boundary exactly. An inside-fit rectangle bay leaves a visible
slot against a slanted facade from inside, which is precisely what an interior
proof would expose. `[V]` The isolated path already builds per-actor meshes, so
the mechanism exists; the persistent path does not allow it yet (F9).

### 5.6 Geometry model

`[P]` One model serves exterior, interior and details:

#### 5.6.1 Local frame and bay grid

- Local origin = the envelope's first vertex; `+x` along the first edge; `+z`
  up from the support datum.
- Oriented bounding rectangle in that frame. Bay grid over it at the kit's
  nominal bay pitch.
- A bay is *inside* when its centre is inside the ring (with a declared
  tolerance) - ring clipping is used only for slab and prism boundaries, not for
  bay classification. This keeps bay logic simple and makes the exactness burden
  live in one place (the prism).

#### 5.6.2 Exterior walls

- One wall run per ring edge, split into panels of nominal bay pitch; the
  remainder is a filler panel whose width must lie in the module's declared
  min/max, else the edge switches to a different pitch (declared in the
  archetype) or refuses.
- Corners: a declared corner module (or a generated corner pier) sized from the
  wall thickness and the joint angle. `[P]` v1 supports convex corners only;
  reflex corners refuse with a named reason.
- Openings occupy whole panels: `facade.door`, `facade.window`,
  `facade.shopfront` are separate modules, and a panel is never a wall *and* an
  opening.

#### 5.6.3 Stories

- If `LevelsBasis == SourceDerived`: `Stories = Levels`, `StoryHeight =
  Height / Stories`; refuse unless every story height lies in the catalogue's
  declared range.
- Else `Stories = round(Height / nominal_story_height)`, `StoryHeight = Height /
  Stories`, same range check.
- Ground story may be taller by a declared multiplier (shopfront archetypes);
  the multiplier is data, not code.

#### 5.6.4 Interior shell

- Corridor pattern from the archetype: `none`, `single_load` (corridor along one
  long edge), `double_load` (centred). Corridor width in bays.
- Stair core occupies one bay at the corridor end; stairs are metric modules per
  story, and the slab above gets a stair opening (a hole in the prism).
- Rooms are the remaining inside bays; partitions follow bay lines; each room
  gets one `interior.door` on the corridor partition.
- Slabs are prisms inset by the wall thickness; ceiling of a story is the floor
  of the next.
- Interior geometry is budgeted: `max_interior_bays` per building and
  `max_instances` per building; exceeding either refuses with `interior_budget`
  rather than emitting a partial interior.

#### 5.6.5 Details ("unique")

- A detail is a kit instance from a family with `slot` `detail.*`, placed by a
  rule: perimeter station on a given story, or an interior wall station.
- The palette is data (weights per archetype); selection is seeded (5.4).
- A named building may also carry explicit detail records in its override, with
  exact slot, station and module - still disposable, still identity-hashed.
- `[P]` Authored *geometry* that must not be regenerated stays in the protected
  overlay path; if it must sit on a generated building, the overlay contract
  needs the new "detail overlay on assembled feature" mode (F7). Do not solve
  this by writing authored props into generated output.

#### 5.6.6 Foundation and Terrain adaptation

`[P]` Replace the single span gate with a strategy the archetype declares and
the envelope qualifies:

| Strategy | Geometry | Admit when | Refuse when |
|---|---|---|---|
| `conform` | one slab, datum at `support_max + threshold`; skirt down to `support_min - embed` | `span <= kit.conform_span_max` | span exceeds |
| `skirt` | same datum, skirt extruded around the ring down to `support_min - embed`, with a declared maximum visible skirt height | `skirt_height <= kit.skirt_max` | skirt exceeds, or the ring is thinner than the skirt |
| `stepped` | per-bay base datums; risers at bay boundaries; stairs between internal levels | each bay's span within `kit.conform_span_max` and every riser within `kit.riser_max` | any bay or riser exceeds |
| `plinth` | explicit plinth ring(s) inset by data, plus general steps at the entrance | plinth height and each inset are within kit limits | outside limits |

`[P]` Invariants that must hold for every strategy and be verified, not assumed:

1. No generated geometry lies below the final Terrain triangle field at the same
   XY (sample the prism bottom and the skirt base against the triangle field).
2. For `conform` and `skirt`, the floor datum is at or above `support_max +
   threshold`. For `stepped`, every bay datum is at or above that bay's support
   maximum + threshold.
3. The entrance threshold is within `kit.stoop_max_steps` of the adjacent
   exterior ground, or the case refuses with `entrance_unreachable`.
4. No Terrain write, ever. A case that cannot be made walkable object-side is a
   named residual for the separate canonical-influence task.

`[?]` Whether `stepped` is needed at all depends on the first real neighborhood.
Do not build it before a case fails `skirt`.

### 5.7 Identity and no-op proof v2

`[P]` Keep behavioural re-derivation; make the digest structured:

```text
identity = digest(
  "project_modular_building_v2",
  feature_id, transform,
  envelope digest (ring, height, support band, entrance),
  program digest (archetype id, resolved values, bases, palette choices),
  kit catalogue digest,
  per-family module digest (geometry re-derived, not read from a tag),
  prism digest (triangulated generated geometry),
  instance list
)
```

`[P]` Consequences:

- A kit change dirties only the families it changes, and the diagnostic names
  the family.
- A Terrain change that moves the support band changes the envelope digest and
  therefore the identity - no stale reuse across a regrade.
- Generated meshes are validated by re-deriving their triangles, so they are as
  tamper-evident as the shared modules are today.

`[V]` Preserve: staged replacement, unowned-actor refusal, duplicate-ownership
refusal, per-cell input identity, and the capture checks. `[P]` The capture's
"trace down onto the roof at the footprint centre" check must change with
interiors, because the centre may be a stair opening; trace a declared solid
station instead, and keep the physical-contact property.

### 5.8 Fidelity tiers and refusal contract

`[P]` Four tiers, each with its own admission and its own proof, because the
cost and the consumers differ by an order of magnitude:

| Tier | Content | Consumers | Explicit non-claims |
|---|---|---|---|
| T0 blockout | current massing extrusion | city scale | no openings, no interiors |
| T1 facade | polygon envelope, modular walls/openings/roof, foundation strategy | playable street, visual evidence | no interiors, no navigation |
| T2 interior shell | partitions, stairs, door openings, exact slabs | visual/physical proof, gameplay staging | not navigable, no interactive doors, no lighting |
| T3 enterable | doors as gameplay objects, navigation, lighting, contents | an actual gameplay location | requires its own admission |

`[P]` Tier is a property of the selection record and participates in identity
and in the producer's normalized settings. A building never silently degrades
from T2 to T1: the plan either satisfies the tier or refuses with a named
reason.

`[P]` Refusal vocabulary (closed, extended only with a schema version):
`envelope_self_intersection`, `envelope_holes_unsupported`,
`envelope_too_small`, `cross_cell_unsupported`, `support_incomplete`,
`support_span_exceeds_strategy`, `skirt_exceeds_kit`,
`story_height_out_of_kit_range`, `bay_pitch_unsupported`,
`reflex_corner_unsupported`, `interior_budget`, `entrance_unreachable`,
`kit_slot_unsatisfied`, `asset_unsupported`.

### 5.9 What stays untouched

`[V]` and `[P]`:

- Canonical Terrain, Water and Mesh Terrain writers.
- The canonical compiler's building topology (`effective_volumes`), except that
  the native reader starts *reading* fields it already receives.
- The layer transaction, manifest ownership, rollback and restoration paths.
- The massing classifier and suppression order.
- The producer's registration tuple shape (settings fields may be added, but the
  producer stays one producer with one transaction).
- Gameplay/navigation owners, until T3.

### 5.10 What not to build

`[P]` Each of these would be a plausible-sounding mistake here:

- A general shape grammar or PCG graph front end. The accepted direction allows
  one later, not now; a closed archetype document covers the demonstrated need.
- A authored-geometry format per building (hand-modelled shells).
- A Terrain influence API for buildings. Object-side foundations first; the
  residual belongs to the separate conditional task.
- A second actor per building for interiors/details.
- Proportional scaling of metric-detail modules.
- Engine RNG or wall-clock seeding for variety.
- A second parser of canonical JSON.
- A material/variation system as a prerequisite for geometry (keep the existing
  refusal for material-only adoption).
- Vendor/art kit intake before the dimensional-class contract exists.

---

## 6. Mock implementation plan

### 6.1 Sequencing rule

`[P]` Each phase must be independently admissible, because each changes identity
and therefore the acceptance surface. The repo's expensive-gate budget is one
initial run plus one replacement; a multi-phase single commit would exhaust it
on a failure whose owner is ambiguous.

```text
P0  native admission funnel (diagnostic, no product change)
P1  envelope generalization + exact prisms + source stories
P2  kit catalogue v2 + families (geometry-compatible)
P3  program resolution: attributes, archetypes, overrides
P4  interior shell (T2), budgets
P5  foundation strategies (conform/skirt, then stepped if a case demands)
P6  enterable tier (T3) - conditional on a consumer
P7  authored/imported kit meshes - conditional on an art need
```

Reaching the stated goal (interiors + unique details + Terrain adaptation) means
P1 through P5. P0 is not optional: it is how the bounded neighborhood gets
chosen without guessing.

Boundary declaration per phase (the review surface, not a wish list):

| Phase | CHANGED | UNTOUCHED |
|---|---|---|
| P0 | one new diagnostic test | everything; no saved output, no schema |
| P1 | assembly core plan contract, assembly identity, World adapter input qualification, canonical reader (read-only fields) | Terrain, Water, massing classifier, canonical compiler, schemas |
| P2 | kit schema, kit document, kit reader, realization components, kit package naming | World adapter, canonical reader, Terrain |
| P3 | archetype/override schemas and documents, program resolver, selection document | kit geometry, realization, Terrain, massing |
| P4 | catalogue interior modules, core interior generation, capture invariants, metrics | selection schema, World adapter gates, Terrain, gameplay owners |
| P5 | envelope support field, World support classification, core foundation geometry | Terrain writer, canonical compiler, Water |
| P6 | navigation/interaction/lighting owners, producer settings | envelope, catalogues, Terrain |
| P7 | catalogue asset route, `ModuleMatches` alternative, asset admission | core layout, World adapter, Terrain |

If a phase cannot be completed without editing a cell marked UNTOUCHED, that is
an architectural red: stop and revisit the boundary instead of widening the
diff.

### 6.2 P0 - native admission funnel

| Item | Content |
|---|---|
| Goal | report, per cell and per feature, why `Prepare` refuses; rank cells by admitted rectangles |
| Files | new test `Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/Tests/ProjectWorldBuildingAssemblyFunnelTest.cpp` |
| Change | iterate `CellBuildingFeatureIds`, call the real `Prepare`, bucket failures by the existing error strings mapped to the refusal vocabulary; print counts and the top admitted features per cell. Diagnostic only: no actor, no package, no save |
| Proof | L1 native run on the promoted Kazan bundle; receipt under `tmp/world/building_assembly/funnel/` |
| Stop | do not "fix" refusals in this step; the report is the input to the operator's neighborhood choice |
| Why native | a Python reimplementation of the gate would be a second authority; my section 3 numbers are evidence for this document only |

### 6.3 P1 - envelope, prisms, stories

| Step | Files | Change |
|---|---|---|
| 1.1 | `Public/ProjectBuildingAssemblyPlan.h` | add `FPoint2`, `FEnvelope`, `FSourceSemantics`, `FGeneratedPrism`; keep `FKit` until P2 |
| 1.2 | `Private/ProjectBuildingAssemblyPlan.cpp` | new `Build(const FEnvelope&, const FSourceSemantics&, const FKit&, FPlan&, Error)`; ring validation (>= 3 vertices, no self-intersection, finite, min area); oriented frame; bay grid; per-edge panel runs with filler clamp; convex corner piers; slab/roof/foundation prisms triangulated by ear clipping |
| 1.3 | `Tests/plan_test.cpp` | keep current invariants; add non-rectangle ring, 6-corner ring, filler-below-min refusal, self-intersection refusal, story-from-levels case, story-height-out-of-range refusal |
| 1.4 | `ProjectWorldBuildingAssembly.cpp` | replace the rectangle gate with ring + single-cell + full-coverage gates; pass `SupportMin/Max`, entrance edge (if available), and source semantics |
| 1.5 | `ProjectWorldBuildingSelectionPersistence.cpp` | allow per-building generated prisms embedded in the actor package; fold their digest into the identity |
| 1.6 | `ProjectWorldCanonicalBundle.{h,cpp}` | read `building_class`, `levels`, `levels_basis`, `height_basis`, `volume_selection` from attributes with strict validation; keep them out of the massing producer's per-cell identity unless the massing consumes them |

Identity/regeneration impact: selected buildings only. `[P]` Deliberately *not*
adding the new attributes to `HashCellInput`, so unselected cells keep their
massing identity and the regeneration stays scoped. Prove that with a test that
checks an unrelated cell's input hash is unchanged when only the new attributes
are read.

Verification: host plan test (L0); the Manhattan native case re-baselined with
polygon semantics (expected triangle counts become invariants on coverage,
bounds and winding, not on a frozen box); the lifecycle fixture (L1) with one
non-rectangular building added; `canonical_authority` Matrix (L2) because the
building layer's identity function changed.

Refusal cases to add: self-intersecting ring, ring with holes,
`min_height > 0`, cross-cell ring, filler width below the module minimum,
coverage gap, span above strategy.

Stop conditions: any need to edit Terrain, Water, the massing classifier, or the
canonical compiler beyond reading existing attributes -> architectural red, stop
and return to the boundary.

### 6.4 P2 - kit catalogue and families

| Step | Files | Change |
|---|---|---|
| 2.1 | `Data/Schemas/modular-kit.schema.json` -> v2 | families, modules, dimensional class, slots, per-family limits; v1 document remains valid or is migrated in one commit |
| 2.2 | `Data/Kits/original_test_v2.json` | the four existing families expressed as modules with the current geometry; no new visual content |
| 2.3 | `Private/ProjectBuildingAssemblyKit.cpp` | versioned table-driven parse; reject unknown fields, unknown families, duplicate module ids, slot/module mismatch, missing provenance |
| 2.4 | `Private/ProjectBuildingAssemblyRealization.cpp` | components named by family; per-family module re-derivation; per-family identity digest; `Options.Modules` becomes a map keyed by family instead of exactly four |
| 2.5 | `ProjectWorldBuildingSelectionPersistence.cpp` | kit package naming and reuse per catalogue digest; `SK_` package per kit, meshes named by module id |
| 2.6 | `Tests/run.py`, `plan_test.cpp` | schema validation for v2; host test over the catalogue; catalogue-change determinism |

Identity impact: every selected building's kit digest changes once. The visual
result must be *unchanged* for the v2 migration, and that equivalence is the
proof: same plan geometry, same triangles, different container.

Refusals: unknown family, duplicate id, slot unsatisfied, metric module asked to
stretch, ratio module beyond max, family without a component.

### 6.5 P3 - program resolution

| Step | Files | Change |
|---|---|---|
| 3.1 | `Data/Schemas/building-archetype.schema.json` (new) | policy table, story rules, bay rules, facade slots, roof, interior pattern, detail palette, foundation strategy |
| 3.2 | `Data/Archetypes/*.json` (new, 3-5 documents) | e.g. `khrushchevka_5`, `stalinka_7`, `panel_9`, `shed_1`, `retail_2`; each states its fallback behavior and its declared limits |
| 3.3 | `Data/Schemas/building-override.schema.json` (new) | sparse per-feature intent; bounded record count; closed fields |
| 3.4 | `Private/ProjectBuildingAssemblyProgram.cpp` (new, pure core) | `Resolve(envelope, semantics, catalogue, archetypes, override, FProgram&, Error)`; stable FNV-1a seeding; basis tracking; program digest |
| 3.5 | `Private/ProjectBuildingAssemblyRenderPlan.cpp` or equivalent | move layout emission out of `Plan.cpp` so resolution and emission can change independently; keep both under 700 lines |
| 3.6 | `ProjectWorldBuildingAssembly.cpp` | pass semantics; refuse when a selected building's class has no policy and no declared fallback |
| 3.7 | `ProjectWorldBuildingSelection.cpp` | read `tier` and optional `override_source` per record; include both in the cell hash |

Identity impact: selected buildings again; add a test that a policy change to an
*unselected* archetype does not dirty anything.

Refusals: unknown class without fallback, levels/height conflict, story height
outside kit range, archetype referencing a missing slot, override referencing a
missing module, override record above the bound.

### 6.6 P4 - interior shell (T2)

| Step | Files | Change |
|---|---|---|
| 4.1 | catalogue | `interior` family modules: partition panel, interior door frame, stair flight, landing, railing |
| 4.2 | core | corridor patterns, room partitions, stair core, slab holes for stairs; interior budget |
| 4.3 | archetypes | `interior: none | single_load | double_load` plus corridor width and stair rules, per class |
| 4.4 | realization | allow slab prisms with holes; no navigation, no interactive door, no interior lighting; assert `no_navigation` still holds |
| 4.5 | capture | replace the roof-centre trace with a declared solid station; add interior invariants: every room reachable from the corridor opening, no partition outside the ring, stair opening inside the core |
| 4.6 | metrics | `assembly_instances`, `interior_instances`, `prism_triangles`, `assembly_actors` per cell; budgets enforced before emission |

Verification: host test with a 3-story rectangle and a 6-corner footprint
(room count, corridor connectivity, stair rises sum to story height, slab holes
inside the core, instance budget); native isolated case with an interior camera
and a physical walk-in probe; lifecycle with one T2 building; a measured budget
receipt.

Explicit non-claims to record in the test README and the component docs: not
navigable, no doors, no lighting, no contents.

### 6.7 P5 - foundation strategies

| Step | Files | Change |
|---|---|---|
| 5.1 | envelope | per-bay support maxima (only for `stepped`); perimeter support samples (only for `skirt`) |
| 5.2 | World adapter | classify support: `conform` / `skirt` / `stepped` / refuse, using the archetype's declared strategy preference; compute the classification in World, not in Assembly, because it needs the triangle field |
| 5.3 | core | skirt prism, plinth rings, stoop steps, riser placement; invariants 1-3 of 5.6.6 |
| 5.4 | tests | slope fixtures: 0.1 m, 0.4 m, 1.2 m, 3.0 m over one footprint; assert built strategy, refusal reason above the limit, and that the foundation base samples are not below the supplied surface |

Verification: host test for prism/step construction; native case with a sloped
synthetic Terrain (TestData fixture) and a physical threshold probe; assert no
generated vertex below the surface.

`[P]` Order within P5: `conform` and `skirt` first, `stepped` only if a real
selected building fails `skirt`. `plinth` is a variant of `skirt` with visible
rings and should not be a separate phase.

### 6.8 P6 - enterable tier (conditional)

`[P]` Not authorized by this task. Entry condition: a named gameplay location
that must be entered, a navigation decision that does not conflict with the
producer's `no_navigation` per-building contract, an interaction owner for the
door, and a lighting decision. Then and only then: door as a gameplay object,
navigation contribution for the interior tier, lighting anchor, contents via the
gameplay-placement layer.

### 6.9 P7 - authored/imported kit meshes (conditional)

`[P]` Entry condition: a demonstrated art need that procedural modules cannot
meet, plus an asset-provenance admission under the world asset policy. Changes:
`ModuleMatches` gains a pinned mesh-digest route instead of re-deriving from a
generator; kit JSON gains an asset reference with a digest; identity includes the
pinned digest. Do not mix this into P1-P5.

### 6.10 Re-baselining the retained proofs

`[P]` The retained Manhattan case is disposable (prepared fresh each run), so its
`case.json` and expectations can move - but the proof must move *deliberately*:

- keep: determinism, four-or-more family participation, bounds inside the
  envelope, slab contact, refusal set, outward winding, physical roof contact,
  save/reload, no-op, staged replacement, ownership refusals;
- change: the story decision becomes explicit and recorded (the pinned feature
  has no `levels`, so the test states which resolution policy it exercises);
  exact triangle counts become invariants on coverage, volume and winding rather
  than on a frozen box;
- add: one non-rectangular building, one sloped-support case at the strategy
  limit, one interior-shell case, one budget refusal.

`[P]` Do not delete the old expectations silently; each changed expectation
should state which contract changed.

### 6.11 Contract sketches (proposal only)

Selection v2 record:

```jsonc
{
  "feature_id": "alis:osm:way:250440683",
  "kit_source": "Plugins/World/ProjectBuildingAssembly/Data/Kits/original_test_v2.json",
  "tier": "facade",
  "override_source": "BuildingAssembly/Buildings/way_250440683.json"
}
```

Per-building override:

```jsonc
{
  "$schema": "../Schemas/building-override.schema.json",
  "schema_version": 1,
  "feature_id": "alis:osm:way:250440683",
  "stories": 2,
  "entrance": { "edge": 0, "station_m": 3.4, "basis": "authored" },
  "interior": "double_load",
  "details": [
    { "slot": "detail.balcony", "story": 1, "edge": 1, "station_m": 5.0 },
    { "slot": "detail.sign", "story": 0, "edge": 0, "station_m": 1.2 }
  ]
}
```

Proposed producer settings addition (not a new producer):

```jsonc
{ "tier": "facade", "foundation": "skirt", "interior_max_bays": 24 }
```

`[V]` Note that `ProjectWorldBuildingLayerProducer::ValidateSettings` parses an
exact field list, so any new field is a code change and changes the normalized
layer contract, which is part of every building cell's input hash. That is the
intended mechanism for a deliberate regeneration - not a side effect.

---

## 7. Risks

| Risk | Why it bites | Mitigation |
|---|---|---|
| Corner handling on arbitrary polygons | the classic modular-assembly failure: gaps or double walls at joints | explicit corner module or generated pier; convex-only in v1 with a named refusal; a host test with 30, 60, 120 degree joints |
| Bay filler at odd edge lengths | stretched or gap-panel facades | declared pitch alternatives and a min/max filler; refuse rather than stretch a metric module |
| Exact prisms vs shared kit reuse | generated meshes break the "shared asset" cost model | prisms are small (tens of triangles), embedded, per selected building only; budget and measure before scaling |
| Identity churn | every structural change invalidates all selected identities | structured digest; a test that unscoped cells stay clean |
| Interior budget blow-up | instance counts scale with area x stories | `interior_max_bays`, `max_instances`, measured per-cell receipt, T1 fallback is an explicit refusal, not silent |
| Story fidelity vs kit geometry | honoring `levels` changes every story height | declared story-height range in the catalogue; refuse outside; do not silently rescale |
| Entrance semantics | wrong-facing entrances are immediately visible | World-derived intent with a recorded basis; authored override; honest default label |
| Authored detail coexistence | the overlay contract currently refuses | decide in P3 (override document) or extend the overlay contract; do not hand-place actors |
| Scope creep into gameplay | interiors invite doors, loot, nav, lighting | tiers, non-claims in the contract, P6 entry condition |
| Second authority temptation | "just read the canonical JSON in Assembly" is easy and wrong | Assembly stays engine-free and canonical-ignorant; World qualifies, Assembly plans |

---

## 8. Open questions

1. `[?]` **First interior consumer.** Visual/physical shell only, or a named
   enterable location? This decides whether P6 exists in this slice at all.
2. `[?]` **Which neighborhood.** Needs the P0 funnel run and an operator choice.
   The Manhattan showcase building is a good fidelity case but a poor
   neighborhood: one rectangle is not geography.
3. `[?]` **Roof authority.** Canonical has no roof shape. Accept flat-parapet
   only for now (recommended), or extend the provider adapter to read
   `roof:shape` (upstream contract + identity change)?
4. `[?]` **Art kits in scope?** If yes, P7 and the asset policy gate come earlier
   and `ModuleMatches` must change; if no, keep the procedural-only rule.
5. `[?]` **Selection bound.** Five is a prototype bound. What is the target for a
   neighborhood, and what per-cell actor/instance budget accompanies it?
6. `[?]` **Cross-cell buildings.** 272 rectangles cross a cell boundary; the
   massing layer already clips volumes per cell. Is one assembly per logical
   building (owner cell) acceptable, with suppression in every intersecting
   cell, or must assembly remain single-cell?
7. `[?]` **Authored detail route.** Per-building override document, detail
   overlay mode, or both? They solve different problems and should not both
   exist by accident.
8. `[?]` **Story height policy.** When a source asserts both a height and levels
   that disagree (e.g. 7.5 m with 2 levels), which authority wins - the height,
   the levels, or a refusal when neither fits the kit's story module? This is a
   product decision, not a code detail.

---

## 9. What to do with the current backlog item

`[P]` The item's *retained follow-ups* are mostly right but the item should not
remain one task. Split it:

1. **Admission funnel and neighborhood qualification** (P0 + the existing third
   bullet). Blocking prerequisite for any extension. Keep the existing wording
   that D36 proves lifecycle, not geography.
2. **Envelope generalization** (P1). Absorbs the first bullet (unsupported
   footprint, cross-cell, Terrain span regressions) and makes the second bullet
   (kit admission) a sub-step instead of a headline.
3. **Building program authority** (P3). New. The actual gap behind
   "data-driven".
4. **Kit catalogue v2** (P2). Replaces "10x10x6 probe" with per-placement
   dimensional qualification.
5. **Interior shell and budgets** (P4). New. Carries the "measure
   reclassification/massing" bullet into the phase that needs budgets.
6. **Foundation strategies** (P5). New. The real building-side Terrain
   adaptation; explicitly does not touch the Terrain writer.
7. **Enterable tier** (P6) and **art kit intake** (P7). Conditional; require a
   named consumer or a demonstrated art need, plus the asset policy route.
8. **Material/presentation adoption.** Keep as its own deferred item; it must
   not gate geometry.

If adopted, the durable knowledge lands in the existing owners, not in a new
document: `Plugins/World/ProjectBuildingAssembly/docs/decision_record.md`
(product direction, tiers, refusal contract),
`docs/architecture/structure.md` (boundary and realization classes),
`Data/README.md` plus the two schemas (catalogue and archetype ownership),
`Plugins/World/ProjectWorld/docs/territory_contract.md#building-geometry-authority-split`
(the tier and the foundation-vs-Terrain boundary), and `Tests/README.md` (what
each proof does and does not cover).

---

## 10. Appendix - measurements and code anchors

### 10.1 Funnel reproduction

Method: read `canonical/features/*.json` from the promoted bundle; for each
`feature_class == "building"`, apply, in order: single effective volume; single
polygon without holes; drop a closing duplicate vertex; four remaining corners;
signed-area orientation swap exactly as the adapter does; rectangle test with
`inset = 0.03`; strict cell containment of every corner; accumulated Water
feature bounding boxes per cell intersected with the footprint box.

Result: 30,191 -> 29,960 -> 29,855 -> 18,763 -> 7,719 -> 7,447 -> 7,297.

### 10.2 Terrain span approximation

Method: four dense cells; rectangular footprints only; bilinear interpolation of
`core_samples` at a 5x5 grid inside the inset footprint, clamped at cell edges;
span = max - min. Not the refined triangle field, not the coverage test.

Result: `<= 0.5 m` 59.4%, `> 2 m` 8.3%, p50 0.37 m, p90 1.65 m, p99 3.58 m,
max 5.50 m.

### 10.3 Code anchors for each verified claim

| Claim | Anchor |
|---|---|
| rectangle-only input, single volume, single cell | `ProjectWorldBuildingAssembly.cpp::Prepare` |
| support span limit, foundation extent, story rounding | `ProjectBuildingAssemblyPlan.cpp::Build` |
| four components, one material, triangle re-derivation | `ProjectBuildingAssemblyRealization.cpp::Matches`, `Shape`, `GeometryIdentity` |
| four shared modules required for external actors | `ProjectBuildingAssemblyRealization.cpp::Apply` (`Options.Modules.Num() != 4`) |
| five-record selection bound, kit path confinement, double hash | `ProjectWorldBuildingSelection.cpp::Load`, `Data/Schemas/project_world_building_selection.schema.json` |
| shared kit package identity and retirement | `ProjectWorldBuildingSelectionPersistence.cpp` |
| exact settings tuple, `no_navigation`, Nanite, complex-as-simple | `ProjectWorldBuildingLayerProducer.cpp::ValidateSettings` |
| per-cell identity includes the Terrain artifact | `ProjectWorldBuildingRealization.cpp::HashCellInput` |
| canonical reader drops class/levels/bases | `ProjectWorldCanonicalBundle.cpp` (building-volume branch) |
| canonical produces class/levels/bases and part selection | `tools/World/CanonicalCompilation/app/adapters.py::_building_attributes`, `app/buildings.py` |
| no-interiors contract at territory scale | `Plugins/World/ProjectWorld/docs/territory_contract.md`, building massing bullet list |
| authored feature anchor refuses assembly | `ProjectWorldBuildingAssembly.cpp` overlay loop |
| proofs do not cover polygons, slopes, interiors | `Plugins/World/ProjectBuildingAssembly/Tests/README.md`, `Tests/plan_test.cpp` |
