# Present Kazan buildings

**Status:** RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED
**R1 candidate:** B1 - one universal textureless Default-Lit building-massing material on frozen `project_building_massing:v1` geometry
**Universal material owner:** ProjectMaterial recipes, generated resources, and material manifest
**World geometry owner:** ProjectWorld `project_building_massing:v1`
**World presentation consumer:** ProjectWorld `presentation:v1` building binding, authentication, reference assignment, and migration
**Concrete geography owner:** ProjectWorldData footprints, height, topology inputs, and geometry policy only
**Concrete-object owner:** ProjectObject only for genuinely unique authored object identities; not for generic massing
**Campaign:** [Kazan presentation research](world_plan_kazan_presentation_campaign.md)
**Implementation prerequisite:** [Shared material generation core](../content/material_generate_assets_from_json.md)
must be implemented and accepted first if Buildings is selected. Research may continue now.

## Research decision

Keep `project_building_massing:v1` geometry exactly as accepted. The first presentation
candidate changes only the material resource and its stored reference:

```text
ProjectMaterial universal Building massing parent/default MIC
        ->
ProjectWorld closed building.default binding
        ->
ProjectWorld presentation:v1 assignment/authentication
        ->
existing cell-owned Nanite building meshes
```

B1 is intentionally textureless and uses only face orientation already present on the
mesh. ProjectMaterial owns the wall/roof colors. The existing hard-coded World vertex color
stays byte-identical for geometry locality but is not consumed as visual authority. B1 makes
walls and flat roof caps read differently under ordinary Default-Lit shading without adding
material sections, textures, actors, Custom Primitive Data, per-building components,
decals, or a generator version change.

Do not infer district classes, facade styles, roof shapes, or landmark identity that the
canonical contract does not contain. If B1 is visually insufficient, return Buildings to
focused research. Do not automatically expand this slice into vertex-color generation,
facades, procedural roofs, modular assembly, PCG, or `project_building_massing:v2`.

This packet is research only. It authorizes no production code, recipe, generated material,
map, mesh, manifest, profile, or package mutation.

### Newly classified source-semantic limitation

The F6 scale audit on 2026-08-28 disproved a global metre-to-centimetre or Building
realization-scale defect, but found a separate skyline limitation that B1 cannot fix:

```text
pinned Kazan source
  -> low admitted base way with building + height
  -> taller related ways with building:part + height/min_height only
current nwr/building source selector
  -> admits the base
  -> excludes part-only vertical volumes
canonical / project_building_massing:v1
  -> correctly realizes only the admitted low base
```

The verified controls were source ways `228963085` and `230132591`; both have taller
nearby part-only volumes in the same pinned source snapshot. This classifies the defect
as generic source admission/canonical representation, not landmark-specific bad scale.
Do not hardcode landmark heights and do not create one runtime actor per source feature.

B1 remains a valid material-only candidate for making the accepted blockout easier to
read, but it cannot claim landmark or skyline fidelity. Before selecting Buildings for a
release that requires that fidelity, reopen only the source-semantic branch and design a
generic `building:part` admission/association/overlap contract with synthetic and real
controls. Keep that contract independent from universal material assignment. Regenerate
Building authority only after that separate contract passes review and focused tests.
The future contract must follow Simple 3D Buildings ownership: associate parts with
their containing `building=*` outline, use the outline as semantic/2D footprint
authority, and do not blindly extrude both the outline and its overlapping parts as
duplicate 3D mass. Incomplete part coverage needs an explicit tested fallback instead
of implicit double rendering.

## Product outcome

Make Kazan's accepted building massing read clearly as a city at player, oblique, and aerial
views while remaining an honest blockout:

- roofs and vertical walls are visually distinguishable;
- existing footprint and height variation is easier to perceive;
- dense blocks do not collapse into one uniform debug-colored mass;
- protected hero overlays remain the path for genuinely unique landmarks;
- the result still looks like generated massing, not falsely finished architecture;
- collision, Nanite, World Partition, topology, and the physical RTX 4070 1440p/60 target
  remain accepted.

"District and landmark readability" is bounded by the source contract. B1 may improve the
readability of district-scale massing and height silhouettes, but it cannot truthfully
invent district categories or landmark facades that are absent from canonical data.

## Stable routes

- [ProjectMaterial owner](../../../Plugins/Resources/ProjectMaterial/README.md)
- [ProjectWorld owner](../../../Plugins/World/ProjectWorld/README.md)
- [World realization SOT](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition contract](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [World generation pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md)
- [World proof layers](../../../docs/testing/world_pipeline_layers.md)
- [Plugin dependency rules](../../../docs/architecture/plugin_rules.md)
- [ProjectObject owner](../../../Plugins/Resources/ProjectObject/README.md)
- [Kazan realization profile](../../../Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json)
- [Kazan validation profile](../../../Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/kazan_territory_v1.validation.json)
- [Kazan presentation profile](../../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json)
- [Shared material core](../content/material_generate_assets_from_json.md)
- [Procedural building assembly decision](../../../Plugins/World/ProjectBuildingAssembly/docs/decision_record.md)

Official UE 5.8 references:

- [Nanite Virtualized Geometry](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)
- [Material Instances](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine)
- [VertexColor material expression](https://dev.epicgames.com/documentation/en-us/unreal-engine/constant-material-expressions-in-unreal-engine)
- [Custom Primitive Data](https://dev.epicgames.com/documentation/en-us/unreal-engine/storing-custom-data-in-unreal-engine-materials-per-primitive)

## Frozen boundary

### May change if B1 is later selected

- ProjectMaterial Building archetype support, universal Building recipes, generated parent
  and default MIC, material manifest, focused tests, and stable material docs;
- ProjectWorld `building.default` binding, authentication, presentation reference-assignment
  pass, and focused tests;
- `ProjectWorld.uplugin` only if packaged activation/cook evidence requires the normal
  content-plugin dependency on `ProjectMaterial`; include it in the exact rollback surface
  only in that case;
- shared presentation schema/parser/fingerprint separation if an earlier selected concern
  has not already landed it;
- Kazan and synthetic presentation inputs only to remove legacy building material
  implementation fields;
- all and only preflight-discovered building StaticMesh package material references during
  the one-time cutover;
- one new immutable active Building owner-manifest generation plus atomic active-set
  promotion for the changed mesh package hashes;
- focused validation/receipt surfaces and stable owner docs after acceptance.

### Must remain untouched

- canonical building footprints, ownership, height values, source provenance, and topology;
- `project_building_massing:v1` generator version and geometry semantics;
- accepted duplicate/contained/conflict/malformed classification policy;
- vertex positions, indices, normals, tangents, UVs, vertex colors, polygon groups,
  sections, triangle counts, mesh bounds, Nanite, collision, actor transforms, GUIDs,
  spatial loading, HLOD state, navigation state, and external actor/map package bytes;
- terrain, water, roads, vegetation, gameplay placement, and authored-overlay authority;
- selected `512/1536` World Partition profile and HLOD prohibition;
- ProjectObject assets and object definitions;
- ProjectBuildingAssembly, PCG, modular facades, interiors, roofs, doors, windows, props;
- `Alis.uproject`, ProjectWorld `Build.cs`, ProjectMaterialEditor staging, and any
  ProjectMaterial runtime service/API;
- project-wide Lumen/Nanite/VSM/quality settings as a way to buy back candidate cost.

Unexpected propagation into an untouched owner is a stop condition.

## Verified current ALIS state

### Generator and geometry contract

`project_building_massing:v1` is a cell-local generated-geography layer depending only on
terrain. The current Kazan profile freezes:

```text
generator: project_building_massing:v1
spatial ownership: cell_local
dirty granularity: canonical_cell
dependency halo: 0
runtime mapping: world_partition_spatial
maximum height: 300 m
terrain anchor: owner_cell_clamped_bounds_center
topology: cell_local_classify_v1
duplicates: stable_feature_id
contained parts: associate_with_container
conflicts: reject_affected_fragments
Nanite: true
collision: complex_as_simple
navigation: no_navigation
```

The mesh builder accepts canonical `building` Polygon/MultiPolygon features with a positive
finite `HeightMeters`. Each accepted clipped polygon part is an extrusion:

```text
terrain-sampled base height
        +
flat bottom cap
flat top cap at base + HeightMeters
vertical outer/hole walls
```

There is no roof-shape implementation. "Roof" in current massing is the flat top cap.
There is no canonical roof field in `FProjectWorldCanonicalFeature`.

The active canonical JSON contains `building_class`, `height_m`, `height_basis`, and, where
available, `levels`/`levels_basis`. The current `FProjectWorldCanonicalFeature` projection
retains only general geometry, `FeatureClass`, and scalar `HeightMeters`; it does not expose
those other facts to Building realization. It also contains no district, facade style, roof
shape, material class, or landmark identity. Therefore none is a valid B1 selector. A later
semantic candidate would require an explicit loader/generator/output-channel contract
revision, not an undocumented material inference.

### Current mesh representation

For each populated cell, ProjectWorld builds one persistent StaticMesh and one spatial
StaticMeshActor. A cell mesh can contain many accepted building fragments.

Current mesh construction uses:

- one polygon group / one material slot named `Building`;
- one UV channel from local XY coordinates scaled by `0.001`;
- per-face normals from triangle geometry;
- tangents from triangle edge direction;
- constant vertex color `(0.42, 0.38, 0.32, 1.0)` on every generated vertex;
- flat top/bottom caps and vertical walls;
- Nanite enabled;
- complex-as-simple BodySetup;
- component collision `QueryAndPhysics` / `BlockAll`;
- navigation disabled;
- spatial loading enabled;
- HLOD disabled and no HLOD layer;
- stable cell-derived actor identity and external-actor packaging.

The building mesh semantic digest records the cell origin, triangle output, accepted/rejected
feature identities, and vertex positions. Material identity is not geometry semantic
identity.

### Accepted production census

The current validation contract records:

```text
210 canonical cells
171 building cell actors
171 building StaticMesh assets
655,330 triangles
31,932 candidate fragments
31,769 accepted fragments
1 duplicate fragment
11 contained fragments
150 conflict fragments
1 malformed fragment
0 authored-mask exclusions
342 Building layer artifacts
```

The active Building scope is `layer_kazan_territory_v1_buildings`, generation 7. Its active
manifest records the current compile identity, authored-overlay identity, and the full
legacy presentation-profile hash. The active set points to
`scopes/layer_kazan_territory_v1_buildings.7.json`.

`342` artifacts matches `171 mesh + 171 external actor` ownership. Implementation must
re-resolve the then-active manifest and recount instead of hard-coding these numbers.

The synthetic E2E twin also has an active Building expectation: two cell actors, two meshes,
36 triangles, two candidate/accepted fragments, and generator tuple
`project_building_massing:v1`. Building code tests additionally construct synthetic v1
profiles directly. Any clean parser/profile change must census all active production/test
consumers rather than assuming Kazan is the only one.

### Agent-authenticated local evidence - 2026-08-26

- Active canonical authority:
  `kazan_territory_v1:86821a9914b758e4c536f72fd549333b1953605decbb74ba9c71c2c32360809b`;
  compile result `c781bd87c98c63c475191b7890b4d405904ed622332fa11fd017a6daacc4b2c6`.
- The production height/provenance census is the 30,196-feature result recorded in the
  presentation diagnosis below.
- A read-only UE 5.8.1 commandlet loaded active-manifest mesh
  `SM_ProjectWorldBuildings_grid_413718bc833994e5_x_6_y4` whose SHA-256 matches the
  generation-7 manifest. Binary readback returned one material at Engine
  `VertexColorMaterial`, Nanite enabled, vertex colors present, one UV channel, 192 LOD0
  vertices, and complex-as-simple collision. Source construction independently fixes one
  `Building` polygon group/section.
- The exact legacy Engine material path appears in both active presentation profiles and
  two focused C++ tests. The closed field is also consumed by the presentation schema,
  parser/resource loader, Building realization caller, production/synthetic validation
  routes, generator-fingerprint surface, active manifest, and generated Building meshes.
  Historical manifests are provenance, not executable compatibility requirements.
- The agent then cold-started the editor on the Kazan territory map. MCP authenticated the
  exact map, 860 loaded actors, and all 171 actors matching the Building layer. A fresh
  1061x696 oblique capture (SHA-256
  `b35a6d3bf71541da6d9efd23055bf90f9226be69ea7d47a246dbbaf76beded71`)
  shows the current near-white uniform massing surface. Lighting makes some roof/wall faces
  distinguishable, so B1 is correctly an improvement candidate rather than a fabricated
  claim that roofs are wholly unreadable.

### Existing focused tests

Current Building tests already cover:

- `Project.World.Realization.Buildings.TopologyAdmission`;
- `Project.World.Realization.Buildings.PersistentLayer`;
- `Project.World.Realization.Buildings.InputLocality`.

They prove deterministic topology classification, holes/multipolygons, duplicate/contained/
conflict/malformed behavior, authored masks, persistent cell output, Nanite, complex-as-simple
collision, no navigation/HLOD, no-op application, height-driven dirty replacement,
retirement, and cross-cell clipping without duplicate seam walls.

B1 extends these contracts; it does not replace them.

## Verified presentation weakness and bounded visual claims

The presentation profile currently points Building meshes at:

`/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial`

The generator writes the same vertex color to every building vertex. Therefore the verified
presentation limitation is a single uniform debug-style color input across all generated
massing. That constant is also presentation implementation embedded in World geometry;
B1 leaves it untouched for byte locality but does not promote it into the universal
material contract.

Source alone did not prove the exact rendered appearance. Fresh B0 now confirms a uniform
near-white massing surface and also shows that lighting already distinguishes some roof and
wall faces. It does not prove junction shimmer, roof disappearance, or landmark
misrepresentation; B1 must not use those unobserved claims as scope.

The active canonical bundle does expose height provenance. The 2026-08-26 local census
found 30,196 unique Building features, all with `height_m`: min 2 m, median 9 m, p95 27 m,
p99 48 m, max 150 m, and 61 distinct values. Height basis is 16,822
`procedural_default`, 13,289 `inferred_from_levels`, and 85 `source-derived`; all source
references identify the accepted OpenStreetMap/Geofabrik snapshot. The 31,769 accepted
count is clipped realization fragments, not unique source buildings. B1 must not claim to
repair the substantial default-height population.

### Presentation gap split

Treat the following as separate diagnoses:

1. **Uniform debug surface** - verified from material path plus constant vertex color.
2. **Limited wall/roof readability** - B0 shows some lighting separation, leaving a bounded
   improvement opportunity rather than a geometry defect.
3. **Insufficient height silhouette variation** - source/compile issue if proved; B1 cannot
   repair it and must not fake heights.
4. **Missing pitched/complex roofs** - current generator/source contract limitation; out of
   B1 scope.
5. **Landmark-specific appearance** - concrete authored-object concern. Use protected
   overlays/ProjectObject when a real unique object is selected; do not teach generic
   massing a Kazan landmark table.
6. **Facade/window/interior detail** - later modular-building/art concern, not blockout
   presentation.

## Important existing material-assignment defect

`ProjectWorldBuildingRealization::Apply` receives a `BuildingMaterial`, but both existing
geometry no-op paths can return before the mesh material slot is reassigned:

```text
cell not dirty + expected actor/mesh exist
    -> continue

existing actor semantic == Build.SemanticDigest
    -> continue
```

A geometry-current mesh can therefore retain an obsolete material reference.

Do not solve this by marking every Building cell dirty or calling
`BuildFromMeshDescriptions()` merely to update a material. B1 requires an independently
checkable `presentation:v1` Building consumer pass that executes after either geometry
mutation or geometry no-op.

## Ownership decision

```text
ProjectWorldData
  canonical footprints + HeightMeters + sourced semantic facts
  Building geometry policy only
        ->
ProjectWorld project_building_massing:v1
  accepted cell geometry, Nanite, collision, World Partition ownership
        |
        v
ProjectWorld presentation:v1 Building consumer
  building.default binding
  accepted-material authentication
  material-slot reference assignment/migration
        ^
        |
ProjectMaterial
  universal Building-massing recipe
  parent/default MIC
  compiler/manifest authority
```

ProjectWorldData ends with no Building material path, graph parameter, recipe, MIC, or
material manifest. ProjectWorld knows only the closed semantic binding and accepted final
resource identity. ProjectMaterial has no Kazan feature IDs, cell IDs, or World-layer
knowledge.

ProjectObject does not participate in B1. It becomes relevant only for a future visual
resource intrinsically tied to one concrete object/landmark identity. Generic building
massing, brick, plaster, concrete, roof color, glass, wood, and similar reusable concepts
remain ProjectMaterial territory even if Kazan is their first consumer.

## Selected B1 material contract

If Buildings wins campaign priority, the accepted shared material core admits one closed
Building archetype if it is the first material consumer:

```text
family: surface_opaque
archetype: building_massing_basic_v1
```

Universal recipes:

```text
ProjectMaterial/Data/Materials/Building/
  M_ProjectBuildingMassing.material.json
  MI_ProjectBuildingMassing_Default.material.json

-> /ProjectMaterial/Generated/Building/M_ProjectBuildingMassing
-> /ProjectMaterial/Generated/Building/MI_ProjectBuildingMassing_Default
```

The parent graph is intentionally small:

- Surface / Opaque / Default Lit;
- use ProjectMaterial-owned typed wall and roof colors; do not read the hard-coded World
  vertex color as the new visual authority;
- derive a bounded upward-facing roof mask from the existing face/vertex normal versus
  world up, excluding downward bottom caps;
- apply one subtle roof-versus-wall tint/value difference;
- constant high roughness, non-metallic, bounded specular;
- no texture dependency;
- no normal texture, procedural Noise, world-position district pattern, decals, emissive,
  opacity, WPO, PDO, displacement, tessellation, runtime MID service, MPC, or Material Layers.

The material core owns the graph topology; JSON exposes only the exact typed values admitted
by this archetype. It is not an arbitrary node DSL.

The default MIC owns the universal blockout look. Kazan supplies no colors or shader
parameters.

### Why this is the selected smallest candidate

- ProjectMaterial-owned colors plus existing face orientation need no new mesh channel.
- The dormant constant vertex-color bytes remain untouched, preserving geometry locality.
- Roof/wall orientation already exists in generated normals, so no mesh-data change is
  required to distinguish them.
- One material slot/section stays one material slot/section.
- No additional actors/components/draw-structure are added.
- No textures or texture memory are added.
- `project_building_massing:v1` geometry stays valid.
- Same-path material tuning after cutover can update ProjectMaterial only, with zero World
  package writes.

### Explicitly deferred next step

Do **not** encode new per-building palette IDs into vertex colors in B1.

Vertex colors are the only current persisted per-building-capable channel inside the merged
cell mesh, but changing their semantic meaning would change generated mesh bytes/output
semantics. That is a materially different generator/presentation contract and likely a
new generator-version decision. It returns to focused research only if B1 passes all
correctness/performance gates but is still visually too uniform.

## UE 5.8 capability decisions

| UE 5.8 capability | Decision | Reason |
| --- | --- | --- |
| Nanite Static Mesh | Keep | Already accepted; Nanite supports StaticMesh vertex colors and material swapping. No Nanite experiment belongs in B1. |
| Material Instances | Select | Native parameterized reuse without recompiling the parent for ordinary instance changes. |
| VertexColor material expression | Do not consume in B1 | The channel remains byte-identical, but its hard-coded World color is not universal visual authority. |
| Vertex/Pixel normal orientation | Select | Existing face orientation is enough for a bounded roof-vs-wall mask with no extra mesh channel. |
| Custom Primitive Data | Reject B1 | Data is per primitive/component. One cell component contains many buildings, so CPD would vary the whole cell, not individual buildings. |
| PerInstanceCustomData / PerInstanceRandom | Reject | Building massing is not represented as ISM/HISM instances. |
| Multiple material slots/sections | Reject B1 | Adds draw/material-section structure and requires mesh regeneration for a problem one material can test first. |
| World-position procedural district variation | Reject B1 | Would invent presentation categories/patterns not present in canonical authority. |
| Mesh Paint | Reject | Manual non-regenerable authoring conflicts with generated authority. |
| Decals | Reject B1 | New projected artifacts/ownership and performance surface with no demonstrated need. |
| Nanite displacement/tessellation | Reject | Changes apparent/actual geometry and cost; no blockout requirement. |
| PCG / PCG Biome | Reject | A second generation/composition authority; massing already exists and is accepted. |
| Procedural Building / ProjectBuildingAssembly | Reject here | Separate later generator milestone; roadmap explicitly keeps current massing simple and no interiors/final art. |
| HLOD | Reject | Frozen ALIS World Partition policy. |

UE Custom Primitive Data remains useful elsewhere, but Epic defines it as data stored on a
scene primitive. With one Building primitive per populated cell, it cannot provide honest
per-building variation without first changing the representation.

## B1 dependency and cook closure

ProjectWorld consumes `building.default` only as a stable soft object identity. It has zero
`Build.cs` dependency on `ProjectMaterial` or `ProjectMaterialEditor`, calls no material
runtime service/API, and never loads recipe/compiler code in packaged gameplay.
`Alis.uproject` remains unchanged.

The current descriptors are evidence, not the future answer: `ProjectWorld.uplugin` has no
ProjectMaterial dependency while ProjectMaterial is enabled separately in `Alis.uproject`.
After the accepted parent/MIC and binding exist, but before any Building mesh-reference
mutation, run packaged Development and Shipping preflight through the normal Kazan product
route. Both configurations must:

- resolve the final `building.default` MIC;
- authenticate its accepted ProjectMaterial manifest/digest;
- prove the generated parent and MIC are staged/cooked;
- prove ProjectMaterialEditor, material recipe JSON, and any new material runtime service
  are absent from staged gameplay.

If current activation/cook closure passes, do not add a speculative plugin dependency. If
it fails specifically because ProjectMaterial is not activated/cooked for the consumer,
the only admitted correction is a normal `ProjectWorld.uplugin` plugin dependency on
`ProjectMaterial`, followed by both preflights again. Any proposed `Build.cs` dependency,
runtime material service, `Alis.uproject` edit, or broader plugin change is a stop condition.

Acceptance surface:

`Project.World.Realization.Buildings.MaterialCookClosure`

This is a package/launch/IoStore receipt surface, not an in-Shipping Automation test. The
installed UE 5.8 UBT disables development/performance automation tests for Shipping by
default. Record configuration, executable/archive/IoStore hashes, resolved material path,
accepted manifest/digest, staged parent/MIC evidence, and the forbidden staged-content
census. A preflight failure must occur before any Building mesh-reference write.

If the conditional descriptor dependency is added, the owner-scoped `-1` snapshot and
rollback include the exact prior `ProjectWorld.uplugin` bytes. Otherwise the descriptor is
hashed as expected-untouched evidence and is not copied.

## B0 - mandatory read-only diagnosis

No production bytes change.

### Structural census

Resolve the current active Building manifest/profile and record:

- active scope, generation, manifest/active-set hashes;
- mesh/actor/artifact counts;
- current material object path on every Building mesh;
- one-slot/one-section expectation;
- Nanite/collision/navigation/HLOD/spatial-load state;
- triangle count and per-cell triangle distribution;
- unique canonical building feature count versus 31,932 clipped candidate fragments;
- HeightMeters min/median/p95/max and histogram;
- any machine-readable height provenance/fallback signal available from compile receipts;
- topology/rejection counts and authored-mask coverage;
- cells with the highest building triangles/fragments for performance views.

If a large fraction of building heights is a single fallback value, record that as a
source/compile limitation. Do not compensate with material tricks or geometry changes in
this concern.

### Visual diagnosis

Capture identical authenticated views from the current accepted package:

- player-height street wall/facade massing;
- oblique roof + height silhouette;
- densest building cell/block;
- cross-cell building boundary;
- aerial city/district massing;
- one protected-overlay/hero boundary where applicable.

Add a dedicated Building diagnostic camera if current stable viewpoints do not expose the
concern; do not overload `road_oblique` and do not change generated geometry for a camera.

Record whether the actual problem is:

- uniform/debug shading;
- poor roof-wall separation;
- weak lighting response;
- insufficient height/source variation;
- geometry artifacts;
- missing landmark-specific authored content.

Any geometry, collision, topology, cell-boundary, or source-data defect reproduced in B0 is
not permission to repair it in B1. Stop and route it to the owning accepted contract.

### Transient material diagnosis

Before persistent cutover, use an isolated Editor fixture/live diagnostic to apply a simple
Default-Lit textureless material to the same mesh without saving production state. B0 passes
the material diagnosis only if lighting/roof-wall readability improves while the accepted
silhouette, geometry, collision, and actor state remain unchanged.

## B1 - universal material candidate

After the material core is accepted and Buildings is selected:

1. Generate/authenticate the closed ProjectMaterial parent/default MIC.
2. Capture and freeze the exact B0 views and building-heavy packaged runtime route before
   any World write.
3. Perform the clean presentation-profile/binding ownership cutover.
4. Rebind only the material reference on every active Building mesh package.
5. Capture B1 with the identical views and runtime route.

B1 visual acceptance requires:

- the uniform debug/placeholder appearance is materially improved;
- vertical walls and flat roof caps are distinguishable without facade fakery;
- existing footprint/height silhouettes remain readable at ground/oblique/aerial scales;
- the material does not make all roofs look like a false sourced roof category;
- no new cell seam, lighting discontinuity, shimmer, z-fighting, or clipping appears;
- protected hero overlays remain visually and spatially intact;
- no implication that blockout massing is final architecture;
- all structural, authority, streaming, and performance gates pass.

There is no automatic B2. If B1 is cheap and correct but still insufficient, return
Buildings to research with the failed captures.

## Presentation binding and producer locality

Freeze these identities:

```text
project_building_massing:v1
  = common geography envelope
  + Building geometry/topology contract
  + Building geometry-generation sources only

presentation:v1 Building consumer
  = building.default binding
  + material authentication
  + reference assignment/migration contract
```

ProjectMaterial recipe/compiler/output digests must never enter
`project_building_massing:v1` geometry dirty identity or producer fingerprint.

The current active Building manifest records the full legacy presentation-profile SHA.
After the common presentation separation is accepted, generated Building layer input
evidence uses the current schema's absence sentinel (`presentation_profile_sha256: none`)
rather than unrelated presentation data. If Landscape/Road implementation lands first,
Buildings inherits that accepted common seam; it does not implement a second version.

If the source-set split is landed before any serialized Building reference changes, use the
existing reviewed metadata/fingerprint migration route with exact unchanged-artifact proof.
Once a material reference changes mesh package bytes, metadata-only migration is forbidden.

## One-time immutable reference migration

The initial Engine-material -> ProjectMaterial cutover is a real package migration, not a
geometry regeneration.

Under the existing recoverable generated-authority transaction:

1. Resolve the active Building scope from `active_set.json`; recount all current mesh and
   external-actor artifacts and discover any map/reference package in the mutation surface.
2. Census every active consumer of the legacy Building presentation field/path: production
   and test presentation profiles, parser/schema sources, validation expectations, focused
   tests, generator/source-set declarations, and direct material-path references.
3. Capture one exact owner-scoped `-1` snapshot before mutation.
4. Generate/reload/authenticate `building.default` against the accepted ProjectMaterial
   material manifest before any World write.
5. Require every current Building mesh to match its accepted geometry semantic identity
   and exactly one `Building` material slot before mutation.
6. Replace only that slot's material reference. Do not call `BuildFromMeshDescriptions()`.
7. Reload and compare every changed mesh:
   - positions/indices;
   - normals/tangents/UV0/vertex colors;
   - polygon groups/sections/triangle count;
   - bounds;
   - Nanite settings/built state;
   - BodySetup/collision;
   - navigation state;
   - geometry semantic identity.
8. Prove external Building actor packages and the generated map package remain byte-identical.
9. Write one new immutable Building owner-manifest generation with new mesh SHA-256 values
   and unchanged geometry semantic identities; atomically promote only the Building entry in
   `active_set.json`, then run the normal read-only authority audit.
10. Remove the legacy `materials.building` profile field/consumer only after the accepted
    `building.default` path is active and Road/other users of Engine `VertexColorMaterial`
    are not mistaken for Building references.
11. Prove a same-path ProjectMaterial parameter change advances only ProjectMaterial
    authority: zero Building dirty units, zero Building package writes/mtimes, and zero
    Building manifest advancement. Restore the accepted B1 value through the material
    transaction.

Any failure before or after active-set promotion restores exact `-1` bytes/absence and the
prior active set, then passes the authority audit before another attempt.

## Active-consumer census

B1 does not change the Building generator tuple, but it cleanly removes legacy material
implementation from WorldData/presentation parsing. Before cutover, produce a
machine-readable census containing:

```text
consumer path
consumer kind
legacy field/path
production/test/synthetic classification
active/provenance classification where applicable
selected replacement
```

At minimum, authenticate:

- Kazan presentation profile `materials.building`;
- presentation schema/parser and relevant tests;
- Building realization caller/material parameter path;
- `project_building_massing:v1` source/fingerprint declarations;
- Kazan realization + E2E validation consumers;
- synthetic E2E Building v1 expectations;
- `ProjectWorldBuildingMassingTests.cpp` code-constructed profiles;
- active Building manifest generation selected by `active_set.json`;
- historical immutable manifests separately as provenance.

The research census authenticated those routes. The exact old Engine object path is held
by the Kazan and synthetic presentation profiles plus two focused C++ expectations; the
schema/parser/resource loader and Building caller consume the field structurally. Repeat
the census at implementation preflight because the shared presentation seam may land first
through another concern. Migrate every then-active production/test/synthetic consumer in
one clean logical cutover; do not rewrite historical immutable manifests.

After cutover, repository scan plus structured loading must prove no active WorldData
Building material implementation remains. Historical manifests retain their old
presentation hashes/evidence as immutable provenance; do not rewrite them.

## Test-first implementation contract

Add exact red tests before production mutation.

### ProjectMaterial

`Project.Material.Generation.BuildingMassingArchetypeGraph`

Prove:

- closed `surface_opaque/building_massing_basic_v1` graph;
- Opaque + Default Lit;
- no VertexColor input is connected to visual output in B1;
- normal/up roof-wall mask exists;
- only admitted scalar/vector parameters exist;
- no texture/WPO/PDO/decal/runtime service dependency;
- generate/save/reload semantic determinism.

### ProjectWorld presentation binding

`Project.World.Realization.Buildings.MaterialBinding`

Prove `building.default` resolves and authenticates the accepted ProjectMaterial MIC plus
digest; missing/wrong/corrupt/unaccepted resource fails before World mutation.

`Project.World.Realization.Buildings.MaterialReferenceUpdate`

Given geometry-current v1 output using the old material, the independent presentation pass:

- runs despite both geometry early-outs;
- changes only the `Building` material slot;
- never calls mesh geometry rebuild;
- never saves the actor/map;
- leaves all geometry/collision/Nanite/spatial semantics exact.

`Project.World.Realization.Buildings.MaterialUpdateLocality`

A same-path accepted MIC update changes ProjectMaterial manifest/output only and causes:

```text
Building dirty units = 0
World package writes = 0
Building manifest generation delta = 0
project_building_massing:v1 fingerprint delta = 0
```

`Project.World.Realization.Buildings.ProfileGeometryOnly`

After cutover, WorldData rejects Building material path/recipe/graph/MIC/manifest fields but
retains Building geometry policy.

`Project.World.Realization.Buildings.ReferenceMigration`

Preflight-discovered mesh packages change only material reference while complete mesh,
Nanite, collision, actor/map, and semantic parity passes.

`Project.World.Realization.Buildings.AuthorityPromotion`

Changed mesh SHA values appear in one new immutable Building manifest; semantic geometry
identities remain stable; Building active-set promotion is atomic and the authority audit
passes.

`Project.World.Realization.Buildings.ActiveConsumerCutover`

Census every active legacy material consumer; after migration, zero active WorldData
Building material implementation remains and production/synthetic validation loads.

`Project.World.Realization.Buildings.Rollback`

Injected failures before and after reference save/manifest promotion restore exact `-1`
source/profile/material/manifests/mesh packages/active authority and pass the read-only audit.

### Retain existing authority tests

Keep green:

- `Project.World.Realization.Buildings.TopologyAdmission`;
- `Project.World.Realization.Buildings.PersistentLayer`;
- `Project.World.Realization.Buildings.InputLocality`;
- Slice 4 packaged building collision/player-boundary checks;
- generated-authority and producer-local fingerprint tests;
- centre -> edge -> centre streaming/reload acceptance.

Use exact filters while iterating. Broad World gates remain slice-exit evidence, not the
debugging loop.

## MCP, VisualVerification, and human evidence

MCP/live Editor is state/visual evidence, not performance authority.

For B0/B1 authenticate:

```text
map + active World authority
512/1536 runtime profile
Building active manifest/generation
cell actor identities/transforms
mesh package/object identities
material slot/path
Nanite/collision/navigation/HLOD state
camera transforms
candidate/material manifest identity
```

Use identical B0/B1 screenshots for:

- player-height wall readability;
- roof/wall oblique view;
- dense block;
- cross-cell boundary;
- aerial city massing;
- protected overlay/hero boundary.

The final human gate decides whether the material gain is worth the measured cost. MCP or
Editor FPS is diagnostic only.

## Packaged RTX 4070 performance gate

Historical accepted Slice 4 timing is a risk indicator, not a reusable candidate baseline.
Buildings cover a large amount of screen area, so even a structurally simple material can
change BasePass/GPU cost materially.

Run a fresh paired B0/B1 comparison:

```text
physical RTX 4070
D3D12
High
2560x1440
packaged Development
World Partition 512/1536
same driver
same build configuration/toolchain
same route/cameras/warm-up/sample floor
record B0 and B1 executable SHA-256 independently
```

Include the densest Building view and the existing centre/diagonal/perimeter/backtrack/
return product route.

Record:

- Frame p50/p95/p99/max;
- Game/Render/GPU p95;
- process working set and peak memory;
- GPU memory;
- BasePass/material shader instructions/permutations;
- Nanite triangles/clusters/streaming diagnostics;
- draw/material-pass counts;
- VSM/shadow diagnostics;
- package/cook size delta;
- loaded Building actor/mesh counts;
- World Partition activation/churn/readiness;
- streaming failures and wrong-cell/reload events.

Hard candidate requirements remain:

```text
Frame p95 <= 16.67 ms
streaming failures == 0
all existing stable correctness/memory/readiness ceilings pass
```

B1 structurally adds no actors, mesh sections, textures, or geometry, but measurement is
authority. Do not reclaim budget by lowering Lumen, Nanite, VSM, foliage, resolution,
streaming range, environment quality, or another owner's settings.

Shipping separately proves cook/staging and normal menu -> ProjectLoading -> Kazan product
correctness. It does not replace instrumented Development performance evidence.

## Exact rollback and temporary-file ownership

Scratch owner:

```text
tmp/world/presentation/buildings/<run-id>/
  baseline/
  candidate/
  receipts/
  screenshots/
  diagnostics/
  rollback/
```

Before mutation, capture one exact `-1` snapshot of only the selected branch's mutation
surface:

- changed ProjectWorld presentation/binding/schema/parser/test files;
- changed ProjectWorldData presentation/validation files;
- ProjectMaterial Building recipe/output/manifest prior bytes or proof of absence;
- every preflight-discovered Building mesh package that will receive the new reference;
- active Building manifest and `active_set.json`;
- active-consumer census receipt;
- executable/tool identities.

Expected-untouched external actor/map and non-Building layer packages are hashed, not copied.

Rollback restores the implementation patch plus exact old profile/material/reference mesh
bytes and prior Building active authority, then rebuilds only if source changed and runs the
read-only authority audit plus focused Building tests. It never:

- regenerates canonical building data;
- calls geometry rebuild merely to restore a material;
- hand-edits the map;
- mutates external actor packages;
- uses destructive Git reset/clean;
- duplicates unrelated content as rollback data.

The owner deletes rejected screenshots, shader/Nanite dumps, temporary package copies,
transient config, duplicate backups, failed candidate outputs, and obsolete logs on success,
handled failure, and retry. Retain only compact accepted evidence and one active `-1`
recovery state until transaction closure; then delete obsolete rollback bytes. Nothing
under `tmp/` becomes a committed input.

## Ordered future implementation actions

Only if Buildings is selected after all seven concern researches are complete:

1. [ ] Implement, review, accept, and archive the shared material core first. If Buildings
       is the first material consumer, admit only the closed
       `surface_opaque/building_massing_basic_v1` archetype required here.
2. [ ] Re-authenticate the release goal against the classified `building:part` limitation.
       If skyline fidelity is required, stop B1 and complete the generic source-semantic
       research first. If readable honest blockout massing is sufficient, record that B1
       does not solve omitted vertical parts and continue.
3. [ ] Run B0 read-only structural/height/active-consumer census and fresh paired visual +
       packaged performance baseline. Authenticate the material defect independently from
       known source/semantic limitations.
4. [ ] Add the exact red tests above, including both current material-assignment early-out
       regressions and producer/presentation locality.
5. [ ] Add/inherit the shared presentation schema/parser/fingerprint split so
       `project_building_massing:v1` is geometry-only and `presentation:v1` owns
       `building.default` assignment.
6. [ ] Generate and authenticate universal ProjectMaterial Building parent/default MIC.
7. [ ] Add the closed ProjectWorld `building.default` binding, then pass the Development +
       Shipping dependency/cook closure before mutation. Add the conditional
       `ProjectWorld.uplugin` content dependency only if that proof requires it; fail closed
       on invalid authority or staged content.
8. [ ] Add the independent Building presentation-reference pass so it executes after
       geometry mutation or no-op and cannot be bypassed by either current early-out.
9. [ ] Clean-cutover the legacy `materials.building` profile/schema/parser consumers and
       normalize Building geometry manifest presentation identity without regenerating
       geometry.
10. [ ] Under one exact transaction, rebind all preflight-discovered Building mesh material
       slots, prove full semantic parity and unchanged actor/map bytes, write one new
       immutable Building manifest, atomically promote the Building active-set entry, and
       pass the authority audit.
11. [ ] Prove same-path ProjectMaterial update locality: zero World dirty units/writes and
        zero Building manifest advancement.
12. [ ] Run exact focused tests and request implementation/evidence review before expensive
        rendered gates.
13. [ ] Run identical B0/B1 visual evidence plus paired physical RTX 4070 packaged
        Development performance.
14. [ ] Run affected Check/Matrix and Shipping product-route proof only for the survivor.
15. [ ] Promote durable contracts into stable owners and clean every owner temporary file.

## Alternatives rejected in this concern

| Alternative | Decision | Reason |
| --- | --- | --- |
| Keep Engine VertexColorMaterial | Control/rollback only | It is the verified uniform debug-style presentation path, not the target. |
| ProjectWorldData Building material recipe/MIC | Reject | Geography/data owner cannot own a universal rendering resource. |
| ProjectWorld graph compiler | Reject | Shared ProjectMaterialEditor is the sole compiler mechanism. |
| Per-cell Material Instance | Reject B1 | Cell is not Building identity; creates needless instances and cell-level visual seams. |
| Custom Primitive Data | Reject B1 | One primitive contains many buildings, so it cannot provide per-building variation. |
| Encode per-building color IDs in vertex colors now | Defer/new research | Requires generated mesh byte/semantic changes and likely generator-version review. |
| Multiple roof/wall material sections | Reject B1 | Adds mesh sections/material passes and regeneration; normal orientation can test readability first. |
| Procedural world-position district colors | Reject | Invents unsourced district semantics/patterns. |
| Textures/window/facade atlases | Reject B1 | Final-art/facade scope and texture cost without demonstrated need. |
| Decals | Reject B1 | New projected artifact and performance ownership. |
| PCG / PCG Biome | Reject | Duplicate generation/placement authority; no missing massing capability. |
| ProjectBuildingAssembly / procedural buildings v2 | Reject here | Separate later architecture milestone; current concern is presentation of accepted massing. |
| Interiors, windows, doors, balconies, props | Reject | Outside blockout presentation. |
| Pitched/semantic roofs | Reject | No current canonical roof semantics; requires new source/generator research. |
| Split one building per actor/component | Reject | Reopens streaming/draw/ownership architecture. |
| Nanite tuning/tessellation/displacement | Reject | Geometry policy is accepted and not the diagnosed first problem. |
| HLOD | Reject | Explicitly outside ALIS territory scale strategy. |

## Stop conditions

Stop B1 and return to the owning research/contract if:

- B0 proves the main visible defect is canonical height/source coverage rather than material;
- B0 reproduces footprint/topology/collision/cross-cell defects;
- a convincing blockout requires per-building semantic palette data absent from canonical
  authority;
- pitched/complex roofs are required;
- a unique landmark requires genuine object identity/content rather than massing;
- B1 requires changing vertex colors, sections, positions, or generator version;
- the material core cannot express the closed B1 graph without broadening into an arbitrary
  graph DSL;
- external actor/map packages change during reference migration;
- non-Building layer manifests/artifacts change;
- same-path material tuning writes World packages;
- physical RTX 4070 acceptance fails;
- implementation proposes PCG, modular assembly, HLOD, or another Building authority.

Do not make the implementation slice larger to work around a stop condition.

## Stable documentation propagation after accepted implementation

Only after B1 implementation and acceptance:

- ProjectMaterial README/material docs own the universal Building massing archetype,
  generated resource identities, compiler/manifest/rollback contract;
- ProjectWorld architecture/territory-generation docs own `building.default`, material
  authentication/assignment, reference migration, and separation from geometry dirty
  identity;
- ProjectWorldData README/Data docs state geography + sourced semantic facts only and no
  Building material implementation;
- World pitfalls record the material-assignment early-out/reference-migration trap if it is
  confirmed as a reusable hazard;
- architecture/C4 and generated-resource authority indexes record ProjectMaterial as the
  lower-level universal resource owner;
- Building presentation/profile documentation records removal of the legacy Engine material
  field and clean no-compatibility cutover.

Stable code/docs/tests/schemas must not link back to this transient todo.

## R1 authentication result

Repository source, profiles, manifests, the active canonical bundle, direct UE 5.8.1 asset
readback, local consumer census, official UE capability contracts, and fresh live-editor
MCP/visual evidence now support the decision. B0 confirms the material-only premise and
does not justify geometry, topology, collision, source, or generator-version work.

Authentication is complete. The same reviewer returned `R1 PASS - 2026-08-26`; no
implementation was selected or started.

## Research close-out

- [x] Reviewer reconstructed `project_building_massing:v1` source, profile, tests,
      validation, active manifest, geometry, material, collision, Nanite, and ownership.
- [x] Reviewer separated verified material weakness from unverified visual/source issues.
- [x] Reviewer compared UE 5.8 Nanite, material instances, VertexColor, Custom Primitive
      Data, and rejected larger procedural-building paths.
- [x] Reviewer selected the smallest material-only B1 architecture and universal ownership.
- [x] Reviewer defined reference migration, active-consumer census, tests, visual/runtime
      evidence, rollback, cleanup, stop conditions, and stable-doc propagation.
- [x] Agent authenticated the canonical height/provenance census, active legacy consumers,
      and a representative serialized mesh against installed UE 5.8.1.
- [x] Agent authenticated fresh visual B0 through a connected live editor/MCP session.
- [x] Same reviewer performed the R1 recheck and returned PASS on 2026-08-26.
- [x] Set `RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED` and propagated only
      campaign/current status. No Buildings implementation began.
