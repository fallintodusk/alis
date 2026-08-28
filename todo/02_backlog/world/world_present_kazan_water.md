# Present Kazan water

**Status:** RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED
**R1:** PASS - universal ownership, bounded candidate, and authority migration accepted
**Universal material owner:** ProjectMaterial recipes and generated resources
**World consumer/geometry owner:** ProjectWorld semantic binding, assignment, and cell-local water realization
**Concrete geography owner:** ProjectWorldData polygons, elevations, behavior, and sourced semantic facts only
**Campaign:** [Kazan presentation research](world_plan_kazan_presentation_campaign.md)
**Implementation prerequisite:** [Shared material generation core](../content/material_generate_assets_from_json.md)
must be implemented and accepted first. Research may continue now.

## Product outcome

Make canonical water read clearly as water in normal play and footage at blockout quality,
including surface, edge, shoreline, distance, and cross-cell continuity. Preserve accepted
water geometry, elevation, World Partition ownership, collision policy, and RTX 4070 frame
budget.

This packet is research only. It selects no implementation priority and authorizes no
production code, recipe, profile, asset, map, manifest, or generated-package mutation.

## Stable routes

- [ProjectMaterial owner](../../../Plugins/Resources/ProjectMaterial/README.md)
- [ProjectWorld ownership](../../../Plugins/World/ProjectWorld/README.md)
- [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [World Partition contract](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [World pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md)
- [Plugin rules](../../../docs/architecture/plugin_rules.md)
- [World proof layers](../../../docs/testing/world_pipeline_layers.md)
- [Realization profile](../../../Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json)

## Verified current state

### ALIS implementation and persisted authority

- `ProjectWorldWaterRealization.cpp` owns both water geometry realization and a local
  `BuildMaterial()` graph compiler. That is the duplicate material authority to remove.
- The local compiler creates
  `/ProjectWorldData/Generated/Territory/Water/M_ProjectWorldWater`, sets
  `MSM_SingleLayerWater`, and connects only scattering/absorption plus flat base-color and
  roughness constants. Its own comment calls the graph a legibility placeholder.
- The current material package is 6,606 bytes with SHA-256
  `8DBFB392D849EE07AD6EAF82A9F430E5483DD9492A36CCCC6F3498CE741F4D20`.
- The Kazan realization profile stores `material_shading_model`, `nanite`,
  `scattering_coefficients`, and `absorption_coefficients` in the water layer settings.
  The schema and `realization_layer_operation.ps1` require all four.
- Those appearance fields participate in the normalized water-layer contract. Changing
  them currently makes the whole water layer dirty instead of only a material resource.
- `ProjectWorldLayerInventory.cpp` treats `M_ProjectWorldWater` as an artifact owned by
  the World water layer and derives its semantic identity from the full normalized layer
  contract. Cleanup/manifest authority must be transferred before retiring this artifact.
- The repository currently contains 145 generated water cell StaticMesh packages. Each
  mesh stores its material in slot zero, is non-Nanite, and has no collision/navigation.
  The actors refer to those meshes; current realization does not set an actor/component
  material override.
- A first move from the old WorldData material identity to a ProjectMaterial identity
  therefore requires saving those 145 mesh references. It cannot preserve their package
  SHA-256 values, even when vertex/index/collision semantics stay identical.
- Existing native-twin tests already prove Single Layer Water, non-Nanite water meshes,
  material-slot persistence, lake-hole triangulation, save/reload, and rollback behavior.

### Installed UE 5.8 evidence

- The launcher engine is UE 5.8.1, changelist `56057345`. Daily work must not use the
  source-built engine.
- Core Engine exposes `UMaterialExpressionSingleLayerWaterMaterialOutput` with
  `ScatteringCoefficients`, `AbsorptionCoefficients`, `PhaseG`, and
  `ColorScaleBehindWater`. World-position and Time expressions are also core Engine
  material expressions.
- Single Layer Water therefore supports the required volume appearance and a bounded
  textureless animated normal graph without the Water plugin.
- The installed Water plugin descriptor is Experimental and enables Landmass, Niagara,
  GeometryProcessing, and BlueprintMaterialTextureNodes. Its runtime module is not a
  free material-only dependency.
- `AWaterBodyCustom` exists, but its component explicitly reports that it cannot affect
  the Water Mesh or Water Info systems. It wraps a custom render mesh and still brings the
  Water runtime lifecycle. It is not a justified first-pass replacement for accepted ALIS
  cell-local geometry.

Official reference:

- [Single Layer Water](https://dev.epicgames.com/documentation/en-us/unreal-engine/single-layer-water-shading-model-in-unreal-engine)

## Reviewer feedback audit

### Accepted

- Keep canonical/cell-local water geometry and improve presentation through the existing
  Single Layer Water route first.
- Remove the ProjectWorld-local graph compiler after the shared compiler owns the material.
- Use one closed `surface_single_layer_water/water_surface_basic_v1` archetype only if
  Water is selected as the first material consumer.
- Keep W1 textureless, bounded, continuous in world space, and free of WPO, foam, flow,
  underwater, buoyancy, custom HLSL, and Epic Water dependencies.
- Compare a fresh current control and candidate visually and on physical RTX 4070.
- Preserve a future Epic-first research return rather than authoring an ALIS water engine.

### Corrected or strengthened

1. A Kazan recipe in ProjectWorldData is the wrong owner. Water rendering is universal.
   ProjectMaterial owns the parent/default MIC; ProjectWorld owns `water.default` binding
   and assignment; ProjectWorldData owns no material implementation.
2. The first path migration cannot leave all 145 water mesh packages byte-identical because
   they serialize the old material reference. Acceptance must prove reference-only change
   and byte/semantic equality for mesh descriptions, triangle counts, bounds, Nanite,
   collision, navigation, and actor state. Later same-path material updates must produce
   zero World writes.
3. Scattering/absorption are currently inside the geometry layer contract. Removing them
   changes the normalized contract hash. Use the existing metadata-migration mechanism for
   geometry identity; do not let that administrative change rebuild all cells.
4. The material package digest belongs to ProjectMaterial authority and operation evidence,
   not the water geometry dirty key. Otherwise every visual tuning rewrites geography.
5. Dawn/noon/dusk/night is not a current Water acceptance gate because runtime day/night is
   researched but unimplemented. Test the accepted current environment now; the later
   Environment implementation must run a focused Water compatibility check.
6. UE's Single Layer Water renderer owns its refraction path. W1 must not invent a separate
   refraction graph input unless installed-source/visual proof identifies a real need.
7. `AWaterBodyCustom` is not a preferred deferred solution merely because it exists. Return
   to Epic-first research only when a named capability such as runtime water queries,
   underwater, or true surface displacement is actually required.

## Decision and KISS gate

The smallest design lets the existing universal material owner compile one reusable Water
parent/default MIC and lets the existing World owner assign it to accepted geometry:

```text
ProjectMaterial universal recipes
  Data/Materials/Water/
    M_ProjectWater.material.json
    MI_ProjectWater_Default.material.json
        |
ProjectMaterialEditor compiler + ProjectMaterial manifest
        |
ProjectMaterial universal generated resources
  /ProjectMaterial/Generated/Water/M_ProjectWater
  /ProjectMaterial/Generated/Water/MI_ProjectWater_Default
        |
ProjectWorld closed binding: water.default -> MI_ProjectWater_Default
        |
ProjectWorld cell-local StaticMesh assignment/authentication
        |
canonical water surfaces

ProjectWorldData -> polygons, elevations, behavior, sourced semantic facts only
```

This adds no plugin, runtime service, actor type, spline system, water-zone lifecycle, or
second geometry authority. It removes the local graph compiler and appearance fields from
the geometry profile. The consciously dropped capability is a Kazan-specific shader look;
no current product requirement proves Kazan water is visually unique.

Future sourced facts such as `water_type` or measured turbidity may remain in WorldData.
ProjectWorld may translate such semantics through a reviewed bounded parameter/variant
contract, but WorldData never names graph nodes, shader parameters, recipes, or material
paths.

## Closed W1 material contract

If Water wins campaign priority, the shared material core admits exactly:

```text
family: surface_single_layer_water
archetype: water_surface_basic_v1
```

The universal parent graph contains only:

- Single Layer Water shading with its native output node;
- base color, roughness, and opacity inputs;
- scattering, absorption, PhaseG, and ColorScaleBehindWater inputs;
- one small analytic normal-ripple function driven by absolute world position and Time so
  adjacent cell meshes sample the same phase;
- typed scalar/vector parameters exposed through `MI_ProjectWater_Default`.

The first candidate has no texture dependency, WPO, geometry displacement, tessellation,
foam, flow map, shoreline decal, underwater post-process, buoyancy, Water plugin, Niagara,
custom HLSL, or runtime graph generation. Two competing wave stacks are not allowed. If the
single analytic ripple fails cost or visual gates, simplify/remove it before considering a
new mechanism.

## Authority and locality migration

### End state

- ProjectMaterial manifest owns the universal parent/MIC packages and their digests.
- ProjectWorld water layer manifest owns only water meshes/external actors and geometry
  semantics. It does not list a material package as a layer artifact.
- ProjectWorld's `water.default` binding names the final MIC identity. Material digest is
  authenticated in validation/operation receipts but excluded from cell dirty identity.
- New/rebuilt water cells validate and assign the accepted MIC. Missing/corrupt material
  fails before any geometry mutation; ProjectWorld never rebuilds it locally.
- A same-path material recipe update writes only ProjectMaterial recipe/output/manifest
  authority. Existing World meshes resolve the updated material without resave.

### One-time cutover

1. Generate and authenticate the universal parent/default MIC through the accepted material
   transaction at their final ProjectMaterial identities.
2. Remove `BuildMaterial()` and the appearance-only `FWaterSettings` fields from
   `ProjectWorldWaterRealization`; replace them with fail-closed `water.default` resolution.
3. Remove material shading/scattering/absorption from the realization profile, schema,
   parser, and PowerShell contract. Retain only real geometry policy such as non-Nanite.
4. Remove `M_ProjectWorldWater` from World water artifact inventory and cleanup ownership.
   ProjectMaterial cleanup must never reach into the WorldData mount.
5. Prepare the normalized water-layer contract metadata migration inside the candidate
   transaction. Do not promote a metadata-only active manifest while serialized package
   references still disagree with its authority.
6. Recount the current water mesh packages and stop on any mismatch from the researched
   census of 145. Under one exact `-1` snapshot, update only slot-zero material references.
   Do not call `BuildFromMeshDescriptions`, retriangulate, move actors, or save external
   actor/map packages.
7. Reload and compare each mesh's semantic mesh description, triangle/section count, bounds,
   Nanite, collision, navigation, and assigned material. External actor/map hashes must stay
   exact; any actor/map write is an architectural stop.
8. Write one new immutable water-owner manifest generation with the new mesh-package
   SHA-256 values, unchanged geometry semantic identities, and exact existing actor/map
   hashes. Link `accepted_operation_id` to a receipt naming the reference-only migration,
   atomically promote its entry in `active_set.json`, and pass the read-only authority audit.
   The prior manifest remains immutable provenance.
9. Run an Asset Registry/reference scan. Retire the old WorldData material only after zero
   referencers and after the promoted World manifest no longer claims it.
10. Change one harmless default MIC parameter at the same final path and prove zero World
   dirty units, package writes, or mtimes. Restore the accepted candidate value through the
   material transaction.

Any failure before or after `active_set.json` promotion restores the exact 145 package bytes
and prior active-set authority, then passes the read-only authority audit before retry. Later
same-path material tuning advances only ProjectMaterial authority: zero World package writes
and zero World manifest advancement.

## Candidate funnel

### W0 - fresh control

Capture the current placeholder material before migration from identical water-heavy views:

- player-height shore/edge;
- oblique surface and shoreline;
- distant/aerial water extent;
- a view crossing at least one generated cell boundary;
- the existing dense-centre/product traversal where water is visible.

Record current material/profile/manifest/package hashes, executable hash, settings, and a
complete reference/geometry census. W0 is the visual/performance control and exact `-1`
rollback state, not a compatibility path in the new parser.

### W1 - universal Single Layer Water candidate

Generate the closed universal parent/default MIC, perform the bounded reference migration,
and capture the same views. W1 passes visual review only when:

- surface and shore edge read clearly without opaque-plastic appearance;
- adjacent water cells show no phase seam, crack, or discontinuity;
- distant water remains legible without noisy aliasing or excessive reflection;
- canonical extents/elevations and surrounding terrain/roads/buildings are unchanged;
- current accepted lighting/environment renders correctly.

Run the water-heavy paired performance proof first. Only a surviving W1 receives the full
packaged centre-to-edge-to-centre traversal and Shipping menu-to-Kazan proof. No open-ended
W2 feature matrix is authorized. A failed W1 restores exact `-1` and returns to research.

## Expected future change boundary

Expected changed components if Water is selected:

- ProjectMaterial Water archetype, universal recipes, generated parent/default MIC,
  manifest, focused tests, and stable docs;
- ProjectWorld water material resolution/assignment, removal of local graph generation,
  material-artifact inventory transfer, metadata migration, and focused tests;
- the normal `ProjectWorld.uplugin` content dependency on `ProjectMaterial` when required
  by packaged activation/cook proof; zero ProjectWorld `Build.cs` dependency on
  ProjectMaterial or ProjectMaterialEditor and zero material runtime-service dependency;
- ProjectWorld realization profile schema/parser/script removal of appearance fields;
- ProjectWorldData Kazan/test realization profiles only to remove material implementation
  fields, plus the old material retirement and 145 reference-only mesh package migrations;
- generated-asset authority metadata and stable docs for the new universal collection.
- one new immutable affected water-owner manifest generation and atomic
  `active_set.json` promotion for the one-time reference cutover.

Must remain untouched:

- canonical water polygons, ribbons, surface groups, widths, behaviors, and elevations;
- mesh descriptions, triangulation, sections, bounds, transforms, actor GUIDs, external
  actor/map packages, collision/navigation, and World Partition ownership;
- terrain, roads, vegetation, buildings, gameplay objects, loading, and menu contracts;
- accepted `512/1536` runtime profile and project Nanite/Lumen/rendering policy;
- Epic Water/WaterEditor, Landmass, Niagara, and unrelated plugins;
- `Alis.uproject` and packaged runtime module dependencies.

Unexpected propagation across those boundaries is a stop condition.

## Test-first and acceptance evidence

Add exact focused tests before production mutation:

| Proposed exact test | Proof |
|---|---|
| `Project.Material.Generation.WaterArchetypeGraph` | Native output and bounded graph compile/save/reload semantically. |
| `Project.World.Realization.Water.MaterialBinding` | `water.default` resolves/authenticates the accepted universal MIC and fails closed. |
| `Project.World.Realization.Water.ProfileGeometryOnly` | WorldData water settings reject material implementation fields after cutover. |
| `Project.World.Realization.Water.InventoryOwnership` | World manifest owns geometry only; Material manifest owns parent/MIC only. |
| `Project.World.Realization.Water.ReferenceMigration` | Recounted 145 mesh references move while geometry/collision semantics and actor/map hashes stay fixed. |
| `Project.World.Realization.Water.AuthorityPromotion` | New mesh hashes live in a new immutable owner manifest; active-set promotion is atomic and the authority audit passes. |
| `Project.World.Realization.Water.MaterialUpdateLocality` | Same-path material change produces zero World dirty units/writes. |
| `Project.World.Realization.Water.Rollback` | Failure restores old/new asset presence, all mesh packages, profiles, and manifests exactly. |

Retain and rerun the exact existing native-twin Water persistence/geometry tests. Run every
new exact test through:

```powershell
scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-full-test-name>
```

After focused green evidence, run the relevant module build and data/governance checks via
project wrappers, then one bounded Check plus affected Matrix only at the slice-exit gate.
Do not use broad filters during iteration and do not invoke UE `Build.bat` with custom args.

Physical performance acceptance is RTX 4070, High, 2560x1440, D3D12, packaged Development
for the paired W0/W1 comparison and Shipping for final product-route correctness. Record
Frame p95/p99/max, Game/Render/GPU p95, process/GPU memory, shader diagnostics, package/cook
delta, streaming failures, and wrong-cell/reload events. Hard requirements remain Frame p95
`<= 16.67 ms` and zero streaming failures. Historical timings are risk indicators only.

Rendered evidence must authenticate map/profile/material/executable identities and use the
same machine, driver, settings, route, camera transforms, and capture method for W0/W1.
MCP/VisualVerification captures are acceptance only when they exercise the real rendered
surface; logs and structural tests alone cannot prove appearance.

## Rollback and temporary-file ownership

Before cutover, keep one exact `-1` snapshot covering changed code/schema/scripts, production
and test profiles, old WorldData material, new ProjectMaterial recipe/output/manifest
presence or bytes, affected immutable World manifests, `active_set.json`, all 145 changed
water mesh packages, and executable identity. Snapshot only the named migration surface;
do not duplicate unrelated map/content.

The host owns rollback after the one-shot commandlet/editor child exits. A failure, crash,
timeout, semantic mismatch, unexpected actor/map write, authority-audit failure, or
visual/performance rejection restores exact old bytes/absence and the prior
`active_set.json`, then authenticates them before another mutation.

Scratch lives under:

```text
tmp/world/presentation/water/<run-id>/receipts/**
tmp/world/presentation/water/<run-id>/rollback/**
tmp/world/presentation/water/<run-id>/diagnostics/**
```

The owner keeps only the active accepted receipt and one required `-1` snapshot. It deletes
shader dumps, duplicate backups, rejected captures, abandoned candidates, and every other
run-owned temporary file on success, handled failure, and retry. After closure it removes
obsolete rollback data. Nothing under `tmp/` is a committed input.

## Stable documentation promotion

Accepted implementation must update durable owners:

- `Plugins/Resources/ProjectMaterial/README.md` and material-generation docs: universal
  Water recipe/resource, archetype, manifest, and compiler contract;
- `Plugins/World/ProjectWorld/docs/architecture_overview.md`, `territory_generation.md`,
  and `world_partition.md`: `water.default` binding, geometry/material authority split,
  assignment, locality, and non-Nanite/collision policy;
- `Plugins/World/ProjectWorldData/README.md`, `Data/README.md`, and profile docs:
  geography/semantic facts only and no water material implementation fields;
- `Plugins/World/ProjectWorld/docs/pitfalls.md`: preserve native Single Layer Water output,
  inventory transfer, reference-only migration, and no geometry rebuild on material tuning;
- `docs/architecture/plugin_rules.md` and the C4 model: ProjectMaterial is a lower-level
  Resources content leaf consumed through stable soft identity/content dependency, with no
  ProjectWorld `Build.cs` or material runtime-service dependency;
- World authority docs: changed serialized references create a new immutable affected-owner
  manifest, atomic `active_set.json` promotion, and a read-only authority audit;
- generated-asset authority indexes for the ProjectMaterial collection.

Stable docs, code, tests, configuration, and schemas must not reference this todo. The todo
does not close until these updates and their router links are verified.

## Ordered future implementation

1. [ ] Accept/archive the shared material core and lock Water as its one initial archetype
       only if campaign priority selects Water.
2. [ ] Capture W0 plus the exact reference/geometry census and `-1` rollback surface;
       recount the researched 145 mesh packages and stop on mismatch.
3. [ ] Add and observe the focused red tests above.
4. [ ] Generate/authenticate universal ProjectMaterial Water parent/default MIC.
5. [ ] Separate World geometry identity from material validation/assignment identity.
6. [ ] Remove WorldData appearance fields and ProjectWorld's local `BuildMaterial()` path.
7. [ ] Prepare the water contract metadata migration without promoting stale authority.
8. [ ] Rebind the 145 water mesh material slots without rebuilding geometry; authenticate
       semantic parity and unchanged actor/map hashes.
9. [ ] Write the new immutable water-owner manifest, atomically promote `active_set.json`,
       pass the authority audit, prove zero old refs, then retire the old material.
10. [ ] Prove same-path material-update locality, zero World manifest advancement, and all
        focused green tests.
11. [ ] Run W0/W1 paired visual/performance evidence and same-reviewer implementation check.
12. [ ] Run the bounded slice-exit gates and packaged product route only for the survivor.
13. [ ] Promote durable knowledge into stable docs and clean all owner temporary files.

## Non-goals

- any change to canonical water geometry, elevation, triangulation, or World Partition;
- a Kazan/ProjectWorldData water recipe, material path, MIC, or material manifest;
- Epic Water, WaterBody actors, zones, splines, landscape carving, or runtime queries;
- WPO/displacement, flow, foam, underwater, swimming, buoyancy, or hydrology simulation;
- textures, custom HLSL, Niagara, a runtime material compiler, or a new plugin;
- implementation before all concern research closes and campaign priority is selected.

## Completion criteria

Research can return to `RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED` only after the same
reviewer accepts this R1 packet. The todo remains backlog and is not done at research close.

Implementation completion later requires all of the following:

- universal ProjectMaterial parent/default MIC is the sole Water material authority;
- ProjectWorld owns only semantic binding, authentication, assignment, and geometry;
- ProjectWorldData contains no water material implementation fields/assets/manifests;
- local `BuildMaterial()` and World material-artifact ownership are removed;
- all 145 existing meshes reference the universal MIC with geometry/collision parity;
- actor/map packages are byte-identical through the reference migration;
- a new immutable water-owner manifest records changed mesh-package hashes and unchanged
  semantic identities, `active_set.json` promotes atomically, and the authority audit passes;
- subsequent same-path material changes advance no World manifest;
- same-path material tuning causes zero World dirty units/package writes;
- W1 passes focused tests, visual review, physical RTX 4070 gates, streaming, and product route;
- exact rollback and owner cleanup are proved;
- stable docs are updated without any todo back-reference.

## Research close-out

- [x] Reviewer supplied a Water-only proposed architecture and candidate.
- [x] Agent verified current code, profile/schema/script, generated assets, layer inventory,
      installed UE 5.8.1 Single Layer Water output, and Experimental Water plugin surface.
- [x] Corrected WorldData ownership, stored-reference migration, geometry dirty locality,
      environment timing, refraction scope, and `AWaterBodyCustom` assumptions.
- [x] Recorded implementation-ready actions, tests, runtime evidence, rollback, cleanup,
      stable-document promotion, and completion rules.
- [x] Same reviewer rechecked this completed R1 packet and returned PASS.
- [x] Set `RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED`; no implementation began.

R1 PASS does not overturn the installed-source finding for `AWaterBodyCustom`: its current
component cannot affect Water Mesh or Water Info. If a later requirement needs those native
systems, that concern returns to Epic-first research rather than treating this actor as an
already valid deferred solution.
