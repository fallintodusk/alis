# Generate Kazan Territory and First Playable Scenario

Status: active. Three separate products, in order: (1) `kazan_territory_v1` -
the broad generated geographic envelope; (2) `kazan_playable_v1` - one
bounded scenario envelope inside it that receives gameplay-grade collision,
navigation, overlays, and acceptance; (3) the packaged prototype - menu flow,
micro-scenario, footage, downloadable build. The full territory is NOT
required to become gameplay-ready for the first playable build. Wider
coverage demonstrations stay out of this milestone entirely.

Profile authority split (one geographic SOT, no duplicates):

- `kazan_territory_v1` - source, compiler, and territory validation profiles.
- `kazan_playable_v1` - ProjectWorld spatial/runtime profile referencing the
  territory grid and a bounded cell/coordinate subset; never a second
  source/compiler authority.
- Scenario profile/data - ProjectSinglePlay or the owning Feature plugin,
  referencing the playable spatial anchors.

Sequencing: all Slice 1 contract, estimation, and read-only inventory work
may run with Slice 0A. Read-only discovery and bounded verification-cache
downloads may run during Slice 1. No source is admitted into the accepted
source ledger, promoted from the disposable cache, compiled into canonical
state, or used for Unreal territory mutation until Slice 0A and Slice 1
both pass. From Slice 2 on, a slice starts only when the previous slice's
exit gate is accepted.

## Permanent contracts (read before any slice)

- [Territory generation order, layered regeneration contract, acceptance](../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [ProjectWorld world-build boundary](../../Plugins/World/ProjectWorld/README.md#world-build-boundary)
- [World Partition contract](../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [End-to-end evidence route](../../tools/World/EndToEndValidation/README.md)
- [World data and generated-asset policy](../../docs/legal/world_data_and_asset_policy.md)
- [Loading pipeline](../../Plugins/Systems/ProjectLoading/README.md)
- [Plugin rules + feature orchestration](../../docs/architecture/plugin_rules.md)
- [Single-player game mode](../../Plugins/Gameplay/ProjectSinglePlay/README.md)
- [Procedural building assembly decision record](../../Plugins/World/ProjectBuildingAssembly/docs/decision_record.md)
- [Focus scope](00_focus.md)

## Slice 0A - CRITICAL: generic authored-layer isolation

The one unaffordable failure: generate a huge region, regenerate it later,
and silently break layers above it - including a manual polish layer applied
above everything. Contract SOT: territory doc "Layered regeneration
contract". This gate covers the layers that exist TODAY; generators that do
not exist yet are admitted per-layer inside Slice 3.

- [ ] Freeze authored storage roots and the manual-polish physical
  definition (content root, Data Layer / Level Instance policy,
  external-actor ownership, allowed dependencies, anchor representation)
  in the territory doc.
- [ ] Implement the generated-artifact ownership manifest and its
  acceptance lifecycle per the contract: accepted manifests in the tracked
  NON-GENERATED authority root (outside every Apply/Delete/reconstruction/
  rollback path) with frozen path and schema version, MULTI-SCOPE
  operations declaring their full mutation scope set with one candidate
  manifest per touched scope, preflight against every participating LAST
  ACCEPTED manifest plus global ownership-conflict validation, clean-clone
  validation, and fail-closed activation/retirement rules.
- [ ] Implement immutable versioned manifests with ONE atomically replaced
  active-manifest-set record as the single activation authority
  (transaction ID, active scope set, per-scope manifest path + SHA-256,
  prior active-set SHA-256, acceptance operation ID); activation never
  derives from directory enumeration; referenced-manifest corruption
  fails closed.
- [ ] Implement recoverable transactional replacement with interruption
  semantics per the contract: durable recovery snapshot/journal, active-set
  record replaced LAST, crash before that replacement leaves the prior
  active set authoritative and the partial tree failing preflight, next
  Apply refuses mutation, and no command ever builds a manifest from a
  partial tree.
- [ ] Implement the THREE operation routes per the contract: Normal Apply
  (missing or drifted content rejects before mutation); EXPLICIT clean
  reconstruction (named flag, exact SCOPE SET - map-only preserves
  compatible shared scopes, partial absence rejects, exact prior-state
  restoration on rejection, available only from a coherent accepted
  state); and EXPLICIT transaction recovery (journal-driven rollback or
  completion, stale-journal-after-success recognition, fail-closed
  without a valid journal); absence alone never authorizes regeneration.
- [ ] Perform one-time initial manifest enrollment for every current scope
  explicitly - P0 map scopes, representative map scopes, and the shared
  presentation-profile scope - from accepted inputs plus the pinned
  generator revision via isolated reconstruction; the tracked tree is a
  comparison target only and is never hashed into the initial manifests;
  unexplained differences BLOCK enrollment; manifests are transactionally
  published to the working tree (commits stay operator-owned).
- [ ] Rerun the P0 and representative top-level gates after enrollment with
  all participating manifest hashes recorded in their receipts.
- [ ] Implement ownership scopes (map-owned, presentation-profile-owned,
  generator-layer-owned) with consumer references recorded separately;
  reject two owners for one path, never two legitimate consumers.
- [ ] Implement drift validation: reject unowned or drifted generated
  content against the accepted manifest, independent of Git.
- [ ] Add authored-overlay and polish fixtures above the existing layers
  (terrain, roads, building preview, presentation) using the three anchor
  classes (coordinate, feature-anchored via resolver, masks).
- [ ] Prove, with live receipts, byte-identical authored packages AND
  semantic anchor resolution (bindings, transforms within tolerance,
  references, fail-closed on missing/moved anchor features) across full
  regeneration, one-cell incremental rebuild, rejected-apply rollback,
  and clean-machine rebuild.
- [ ] Exit gate: generic isolation accepted on all existing layer classes,
  explicitly including: manifest schema + active-set rules; multi-scope
  transaction support; immutable manifest documents; the single
  active-manifest-set commit record; a partial manifest-activation
  interruption test; a malformed/missing referenced-manifest fails-closed
  test; a recovery rollback test; a recovery completion test; a
  stale-journal-after-success test; initial enrollment of all current map
  and shared scopes; P0 and representative top-level recertification.

## Slice 1 - Freeze territory and playable-scenario contracts

May run in parallel with Slice 0A; both gates block Slice 2.

- [ ] Create the `kazan_territory_v1` source, compiler, and validation
  profile identities; freeze coordinate envelope, CRS, grid, cell
  dimensions, sample spacing, vertical origin, and target cell range.
- [ ] Freeze the smaller `kazan_playable_v1` coordinate envelope inside it
  as a ProjectWorld spatial profile referencing the territory grid.
- [ ] Identify every required raster tile and vector snapshot. Read-only
  discovery and bounded downloads into a DISPOSABLE verification cache are
  allowed in this slice so scale estimates use verified bytes and ALIS
  SHA-256 values; accepted source admission, canonical compilation, and
  Unreal mutation remain prohibited until both Slice 0A and Slice 1 gates
  pass.
- [ ] Produce a dry-run scale receipt with ESTIMATED CEILINGS, not measured
  baselines: assumptions and scaling formula, representative evidence used
  as the starting point, estimate ranges with confidence margins, and the
  hard maximum allowed for proceeding (source bytes/tiles, cells/samples,
  features per class, canonical bytes, package/actor counts, cook time,
  installed bytes, memory/frame-time/streaming ceilings). Measured
  baselines belong to Slice 4.
- [ ] Shrink or split the envelope BEFORE implementation if estimates exceed
  the frozen ceilings; executable thresholds go into validation profiles,
  stable decisions into the territory doc.
- [ ] Read-only legacy inventory (no migration): every City17 map, sublevel,
  Data Layer, Level Blueprint; WorldSettings/GameMode/PlayerStarts/nav
  config; hard and soft references; feature activation dependencies;
  map-owned vs gameplay actors; exclusive and referenced content sizes;
  licenses/provenance of reusable art; hero locations needing coordinate
  alignment. Record as a ProjectWorld docs inventory record.
- [ ] Exit gate: exact envelopes, estimated ceilings, and legacy inventory
  accepted before any source or Unreal expansion.

## Slice 2 - Scale source ingestion and canonical compilation

- [ ] Versioned raster manifest; verify each original tile independently;
  deterministic mosaic/resample; reject gaps, unresolved nodata, and
  inconsistent overlap.
- [ ] Clip the pinned OSM snapshot to the exact envelope with relation
  completion, provider IDs, and per-source provenance preserved.
- [ ] Compile terrain, water, land cover, vegetation areas, foliage points,
  roads, and building footprints across the extent.
- [ ] Freeze representative quality cells: dense urban, riverbank, suburban,
  sparse edge, cross-cell boundary.
- [ ] Prove water and road topology across cell boundaries; prove
  source-growth and one-cell incremental rebuild at the larger envelope.
- [ ] No automatic building-provider conflation or alternate data sources in
  this milestone.
- [ ] Exit gate: two isolated full compiles with equal D0-D2 evidence;
  bounded change rebuilds only its proven scope; provenance and rejection
  reports complete.

## Slice 3 - Realize geography in independently admitted layers

Create the `kazan_territory_v1` E2E validation profile at slice start;
extend its expectations as each layer is accepted. Reuses the existing E2E
framework; profile/schema and validator extensions are expected work.

Per-layer admission (contract SOT: territory doc "Proof split"): every new
OR MATERIALLY CHANGED generator, artifact layout, anchoring contract, or
ownership scope passes, BEFORE territory-scale use:
minimal synthetic implementation -> authored overlay and polish fixtures
above it -> full/incremental/rejected/clean regeneration matrix ->
layer accepted -> territory-scale realization allowed.

- [ ] 3A Terrain + water: complete Landscape first; water passes per-layer
  admission; water realization decision frozen in a ProjectWorld decision
  doc with a supported non-Experimental fallback (polygon/river-line
  behavior, elevation, shoreline intersection, cross-cell ownership,
  material/collision policy, navigation policy); deterministic clean
  rebuild; continuous cross-cell water.
- [ ] 3B Roads: select realized road classes; widths, bridge/tunnel
  fallback, intersections, terrain conformance; collision and navigation
  proven only inside `kazan_playable_v1`; no city-wide final-road claim.
- [ ] 3C Vegetation: passes per-layer admission; derived only from canonical
  records; deterministic seeds from stable feature/cell identity;
  road/water/overlay exclusion masks; no per-tree actors at territory
  scale; frozen density, instance, package, and runtime budgets;
  ISM/HISM/PCG chosen from measurements with a supported fallback.
- [ ] 3D Building massing: passes per-layer admission; footprint holes and
  multipolygons; height basis and fallback; simple bounded massing only;
  deterministic TOPOLOGY CLASSIFICATION instead of blanket overlap
  rejection - distinguish building, building part, duplicate, and
  contained footprints; merge or associate supported relationships per a
  documented rule; reject only malformed or unresolved conflicts with
  reported provider identities and reasons; no duplicate rendered or
  collision ownership; gameplay collision/navigation only inside the
  playable envelope; no interiors, no final art.
- [ ] Exit gate: the TERRITORY matrix per the contract's stage-scoped
  definition - complete for every generated layer enabled in
  `kazan_territory_v1`, with modular assembly recorded as "not enabled"
  (pending, never N/A); anchor-class coverage and recorded N/A pairs per
  contract - plus clean rebuild, unchanged Apply, incremental rebuild,
  failure rollback, layer counts, seam checks, package audit, and
  required captures, all with live receipts.

## Slice 4 - Measured traversal baseline, then optimization

Fixed cameras are not traversal evidence. Extend the spatial validation
profile with a deterministic traversal sequence: pinned start/end and
ordered checkpoints, fixed movement speed, loaded-cell observations,
streaming latency and hitch sampling, peak memory, p95 frame time,
navigation path success, package and cook attribution.

- [ ] Record the MEASURED packaged traversal baseline with current settings;
  this replaces the Slice 1 estimated ceilings as the factual reference.
- [ ] Change ONE concern at a time (World Partition cells/loading, HLOD,
  Nanite applicability, instancing, PCG, Data Layers); retain a change
  only when the same traversal receipt proves a net benefit without
  breaking regeneration, package size, or fallback behavior.
- [ ] Coordinate package attribution with
  [package size investigation](../02_backlog/content/package_size_investigation.md).
- [ ] Exit gate: accepted packaged traversal receipt; every retained
  optimization has before/after evidence.

## Slice 5 - Integrate one bounded playable scenario

Ownership rule: ProjectWorld owns spatial acceptance only (map identity,
playable bounds, spawn/route anchors, collision, navigation, streaming
roles, capture/traversal paths, budgets). The scenario's objective graph,
state transitions, item requirements, success/failure, and feature
activation live in ProjectSinglePlay or existing feature data. The
protected overlay owns spatial anchors and hero actors, never gameplay
logic. Baked local content only - no launcher delivery, hot-mounted region
packs, or new IoStore mounting path in this milestone.

- [ ] Register the generated map through the existing local world/menu
  route; new game starts on the correct map and game mode.
- [ ] Spawn inside `kazan_playable_v1` on generated ground; verify
  collision, navigation, interaction, and streaming through the scenario.
- [ ] Author the minimum [00_focus](00_focus.md) Track B loop with existing
  systems only (spawn -> inspect/interact -> obtain or choose ->
  vitals/inventory consequence -> clear success/failure -> clean restart);
  no dialogue content, combat, crafting, multiplayer, or Mind work.
- [ ] Store spatial anchors in the protected overlay; store scenario logic
  in ProjectSinglePlay or existing feature data.
- [ ] Verify any save/load contract the playable build already promises;
  prove regeneration preserves all scenario anchors unchanged (byte AND
  anchor-resolution proofs per the layered regeneration contract).
- [ ] Extend the Presentation Gate with scenario cameras and traversal
  identity without moving scenario logic into ProjectWorld.
- [ ] Exit gate: scenario runs menu-to-terminal-outcome in Development and
  packaged Shipping with no manual editor preparation.

## Slice 6 - Decide and integrate legacy content by class

Use the Slice 1 inventory; decisions are per class, recorded in a
ProjectWorld docs decision record.

- [ ] Geography-bound roads, trees, buildings: regenerate or retire.
- [ ] Hero locations and gameplay landmarks: migrate as coordinate-owned
  protected overlays.
- [ ] Reusable art kits: feed the procedural building route per its
  [decision record](../../Plugins/World/ProjectBuildingAssembly/docs/decision_record.md)
  and the territory doc "Building geometry authority split"; kit intake
  only per licensing audit and a measured prototype; author the plugin
  README and regeneration evidence in the same slice.
- [ ] Modular building assembly passes per-layer admission (synthetic
  implementation, fixtures, full regeneration matrix, manifest ownership)
  BEFORE its first territory or playable use, extending the coverage
  matrix with its layer and fixtures; each assembled feature explicitly
  replaces or suppresses its blockout - no competing building geometry.
- [ ] Conflicts, unknown licensing, hidden map dependencies: exclude.
- [ ] Retire City17 only after: the playable scenario is accepted, every
  retained reference has moved, repository-wide reference checks find no
  required old-map dependency, and build, tests, and package pass without
  it. One pass, no compatibility map, alias, or dual authority.

## Slice 7 - Final playable and promotion release gate

- [ ] P0, representative, and `kazan_territory_v1` profiles pass from the
  final revision; a clean bootstrap/build route reproduces the generated
  maps; the FINAL coverage matrix is complete for every generated layer
  present in the released package.
- [ ] Packaged game boots through the normal menu; the micro-scenario can
  be started, completed, failed, and restarted.
- [ ] Runtime and rendering budgets pass; IoStore contains required maps
  and no provider payloads.
- [ ] Package size and archive split stay under current transport
  thresholds; release hashes/signatures and third-party notices generated.
- [ ] Comparison and playable footage come from that exact accepted
  package; footage metadata records the release hash so promotion cannot
  drift from the downloadable build.

## Scope guards

- Slice 0A generic isolation and per-layer admission gate all
  territory-scale generation; the complete coverage matrix is the Slice 3
  exit gate.
- The playable envelope, not the whole territory, receives gameplay-grade
  acceptance in this milestone.
- Human place names remain aliases; coordinates and generated IDs own
  identity.
- No required paid, hosted, Marketplace, or Experimental dependency.
- Persistent contracts, budgets, and decisions land in the referenced docs;
  this file owns only execution steps.
