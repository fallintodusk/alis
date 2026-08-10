# Generate Kazan Territory and First Playable Scenario

Status: active. Build the accepted `kazan_territory_v1` geography, then its
first scenario and packaged prototype. The whole product territory is
generated and player-traversable; the scenario profile selects content
density, route, AI/navigation, and acceptance coverage only. Geographic
extent and playability authority live in the territory SOT linked below.

Profile authority split (one geographic SOT, no duplicates):

- `kazan_territory_v1` - ProjectWorldData-owned source, compiler, runtime,
  presentation, validation, control, and budget profiles consumed by generic
  tools through explicit paths.
- `kazan_scenario_v1` - spatial/runtime scenario profile referencing the
  territory grid; never a second source/compiler or playable-area authority.
- Scenario profile/data - ProjectSinglePlay or the owning Feature plugin,
  referencing spatial anchors in the territory.

Sequencing: Slice 1 source/control admission, contracts, estimation, and
read-only inventory may run with Slice 0A. Discovery and bounded
verification-cache downloads may run during Slice 1; promotion into the
accepted source ledger requires Slice 1's own admission checks and hashes,
but does not depend on authored-layer realization. No accepted canonical
territory compilation or Unreal territory mutation begins until Slice 0A and
Slice 1 both pass. From Slice 2 on, a slice starts only when the previous
slice's exit gate is accepted.

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

- [x] Freeze authored storage roots and the manual-polish physical
  definition (content root, Data Layer / Level Instance policy,
  external-actor ownership, allowed dependencies, anchor representation)
  in the territory doc.
- [x] Generated-artifact ownership manifest and acceptance lifecycle
  implemented and LIVE-PROVEN 2026-08-07: accepted manifests in the tracked
  NON-GENERATED authority root with frozen path and schema version,
  MULTI-SCOPE operations declaring their full mutation scope set with one
  candidate manifest per touched scope, preflight against every
  participating LAST ACCEPTED manifest plus global ownership-conflict
  validation, and fail-closed activation/retirement rules. Live evidence:
  four sequential clean reconstructions republished all five scopes
  (four maps at generation 2, shared presentation at generation 5 with
  all four maps carried as consumers) under one shared generator
  fingerprint.
- [x] Implement immutable versioned manifests with ONE atomically replaced
  active-manifest-set record as the single activation authority
  (transaction ID, active scope set, per-scope manifest path + SHA-256,
  prior active-set SHA-256, acceptance operation ID); activation never
  derives from directory enumeration; referenced-manifest corruption
  fails closed.
- [x] Recoverable transactional replacement with interruption semantics
  implemented: durable recovery snapshot/journal written before mutation,
  active-set record replaced LAST, crash before that replacement leaves the
  prior active set authoritative and the partial tree failing preflight,
  next Apply refuses mutation, and no command ever builds a manifest from a
  partial tree. Interruption, malformed-manifest, rollback, completion, and
  stale-journal scenarios are covered by the Pester suite; the live
  enrollment transactions journaled and settled cleanly (post-enrollment
  audit reports no pending journal).
- [x] The THREE operation routes implemented and exercised: Normal Apply
  (drifted content rejects before mutation - confirmed live when the
  post-E2E tree refused re-Apply); EXPLICIT clean reconstruction (named
  flag, exact SCOPE SET, absent-or-exactly-matching precondition, shared
  presentation scope preserved across map-only legs - the route used for
  the 2026-08-07 enrollment); and EXPLICIT transaction recovery
  (journal-driven rollback or completion, stale-journal-after-success
  recognition, fail-closed without a valid journal). Absence alone never
  authorizes regeneration; only the named flag does.
- [x] Initial manifest enrollment performed for every current scope
  explicitly - both P0 map scopes, both representative map scopes, and the
  shared presentation-profile scope - from accepted inputs plus the pinned
  generator revision via isolated reconstruction; manifests are
  transactionally published to the working tree (commits stay
  operator-owned).
- [x] P0 and representative top-level gates run 2026-08-07 (both accepted:
  run-20260807T140959Z, run-20260807T141745Z), then enrollment from the
  final accepted tree as the LAST generated-tree mutation per contract
  ordering; manifest lifecycle evidence recorded in the run receipts and
  in the durable audit receipt.
- [x] Implement ownership scopes (map-owned, presentation-profile-owned,
  generator-layer-owned) with consumer references recorded separately;
  reject two owners for one path, never two legitimate consumers.
- [x] Implement drift validation: reject unowned or drifted generated
  content against the accepted manifest, independent of Git.
- [ ] Add authored-overlay and polish fixtures above the existing layers
  (terrain, roads, building preview, presentation) using the three anchor
  classes (coordinate, feature-anchored via resolver, masks).
- [ ] Prove, with live receipts, byte-identical authored packages AND
  semantic anchor resolution (bindings, transforms within tolerance,
  references, fail-closed on missing/moved anchor features) across full
  regeneration, one-cell incremental rebuild, rejected-apply rollback,
  and clean-machine rebuild.
- [x] Road collision ORIENTATION fixed (live user report 2026-08-06) and
  the runtime acceptance is now orientation-sensitive. Root cause: the
  route navigation volume was an axis-aligned hull of the route diagonal,
  so a diagonal road showed a visibly mis-oriented box; it is now yawed
  onto the route direction and sized route-local (yaw recorded in the
  receipt). The probe was upgraded from one centreline presence trace to
  a 7-point road-local frame per fragment cell (centre, +/- tangent, +/-
  inside half-width must hit the lifted road band with an upward normal;
  +/- beyond half-width must NOT), which is what actually discriminates
  orientation, width, and facing. Receipt carries
  `runtime_route_collision_orientation_probed` +
  `runtime_collision_orientation_probe_count`; E2E requires both across
  all three legs. Sabotage-verified (tangent-collapsed rails ->
  "Road collision covers the declared ribbon along and across the
  tangent." fails specifically; toggle restored, green after restore).
  This precedes the pre-enrollment representative recertification.
- [x] All generated-tree mutation routes through the ONE project-global
  content lock: the E2E driver owns the lock for its full backup ->
  realize -> restore lifecycle (write-exclusive with a stamped owner
  token), child wrappers verify delegated ownership against the LIVE
  lock (token match + write-access held-probe; stale or mismatched
  claims fail closed, never self-acquire). Regressions in both
  directions: Python owner lifecycle + double-own refusal; PowerShell
  delegation accepted only against a live matching owner, plus stale
  and mismatch rejections.
- [x] Authority documents are LF-only so a CLEAN CLONE still verifies.
  Found 2026-08-07: `ConvertTo-Json` emits CRLF on Windows and both
  manifest writers persisted it, while the repo declares `eol: lf` for
  those paths - so git rewrote them on checkout and every recorded
  `manifest_sha256` would mismatch on a fresh clone, failing the authority
  closed for a non-content reason invisible on the authoring machine.
  Both writers now normalize; regression asserts no CR byte; clean-clone
  byte stability verified against `git hash-object --path` for the active
  set and all referenced manifests. Superseded generations keep their old
  bytes (immutable, unreferenced, unhashed).
- [x] Acceptance-chain evidence is machine-readable and serialized:
  `bootstrap.py accept --p0-run <id> --representative-run <id>` owns the
  project-global content lock across audit -> package -> gate -> audit, so
  the package cannot read a tree that changed mid-cook and changed back
  (a race that no before/after comparison can detect), and emits
  a schema-validated, atomically written `acceptance_chain.json`. It
  PROVES rather than trusts its inputs: each supplied run must carry the
  profile identity it was passed as; both `result.json` hashes are
  recorded; each run FREEZES the SHA-256 of its own realization receipts
  and acceptance verifies those bytes before reading any provenance field
  (a path in `result.json` is not evidence - a child mutated afterwards
  would otherwise launder itself in while the parent hash still
  validated); and identity is compared as a TUPLE per map package,
  selected by the manifest's own `map_package`, never as a union of
  hashes seen anywhere - a union would accept a P0 map carrying another
  leg's perfectly legitimate compile hash. Ambiguous map packages fail
  closed. Sentinel `none` fields are absences, not claims, and the shared
  presentation scope derives its provenance from the presentation input
  its consumer maps were gated with, which must be unanimous.
  Package and IoStore inspection are the SAME helper the E2E gate uses, so
  acceptance cannot prove less than the gate it concludes. A REJECTED
  receipt with partial evidence is written on every failure path. It
  refuses unless both audits report the SAME active-set SHA. The E2E
  driver's own lock window was widened to cover its package, IoStore
  inspection, and gate for the same reason.
- [x] Executable durable authority audit command landed:
  `scripts/ue/world/audit_generated_authority.ps1` (both locks; ONE
  receipt with active-set integrity, per-scope manifest + artifact
  verification, global ownership, unowned scan of the generated roots,
  consumer resolution, and generator-fingerprint currency) and
  the territory doc names it as the mandatory post-enrollment gate
  blocking the read-only package. Fingerprint currency FAILS CLOSED, and
  the fingerprint was first scoped to inputs that can actually change
  generated bytes or manifests - the read-only auditor and test sources
  are excluded by construction, because a gate that fires on provably
  inert edits is one operators learn to bypass. Receipts proven for
  accepted, drift+unowned rejection, pending-journal rejection, stale
  fingerprint rejection (live: all five scopes flagged after the
  fingerprint definition changed), and external-object scan coverage.
- [x] Frozen sequence executed end to end 2026-08-07 with deterministic
  repo-relative generator fingerprints (all five scopes share
  `76a267ae...` across four separate invocations):
  P0 gate accepted (run-20260807T140959Z) -> representative gate accepted
  (run-20260807T141745Z) -> pre-enrollment audit REJECTED as expected
  (90 artifact problems, 61 unowned files - the drift the orientation fix
  created) -> enrollment by clean reconstruction (maps at generation 2,
  shared presentation at generation 5 with all four maps as consumers)
  -> durable audit ACCEPTED (active set `cd851f7b...`, 90 artifacts
  byte-identical, zero unowned) -> read-only package + packaged
  Presentation Gate ACCEPTED on that exact tree (Shipping
  `82efcd64...`, three viewpoints at p95 10.3-11.0 ms against the
  33.34 ms budget) -> post-package audit re-run reports the IDENTICAL
  active-set SHA and artifact set, proving the package step mutated
  nothing. Receipts: `Saved/Validation/WorldAuthority/`.
- [x] Bind retirement recovery to the EXACT prior authority recorded in
  the journal: retired_scopes is an UNCONDITIONAL required field (empty
  only for non-delete operations), the journal carries a frozen
  operation field (apply/delete/reconstruct/enroll), delete refuses
  before mutation when the prior active entry cannot be resolved, and
  missing/empty retirement evidence fails closed (red regressions:
  omitted field + empty delete evidence, both preserving snapshot and
  journal); ordinal-sorted deterministic generator fingerprint with the
  two-root regression.
- [ ] Operator control surface (KISS, decided 2026-08-07 - the system must
  be controllable, not just correct). Three small things, no transaction
  browser: (a) a preview before any mutating run that prints what will be
  replaced, what is protected, and what is untouched, then asks yes/no,
  with a flag for non-interactive automation; (b) a readable history view
  of each scope's generation, acceptance time, and originating run, from
  the immutable manifests and archive that already exist; (c) the audit's
  refusals already name the exact scope and file - keep that property.
- [ ] Wire the anchor resolver into realization: spawn the generated-owned
  anchor actor per overlay (soft reference to the authored Level Instance,
  manifest-owned, regenerated), record resolved/refused counts and drift
  in the realization receipt, and require them in the E2E gate. The
  resolver, schema, and fail-closed semantics are already implemented and
  tested (`Project.World.Authored.Anchor.*`).
- [ ] Exit gate: generic isolation accepted on all existing layer classes,
  explicitly including: manifest schema + active-set rules; multi-scope
  transaction support; immutable manifest documents; the single
  active-manifest-set commit record; a partial manifest-activation
  interruption test; a malformed/missing referenced-manifest fails-closed
  test; a recovery rollback test; a recovery completion test; a
  stale-journal-after-success test; initial enrollment of all current map
  and shared scopes; P0 and representative top-level recertification.

## Slice 1 - Freeze territory and scenario contracts

May run in parallel with Slice 0A; both gates block Slice 2.

- [x] Territory extent, real-world centre, georeference/lattice split,
  geodetic error-budget mechanism, and territory-wide traversal are accepted
  in the territory SOT. The rejected 36-cell production profiles were removed;
  do not restore, compile, or realize them.
- [ ] Create accepted `kazan_territory_v1` source/compiler profiles under
  `ProjectWorldData/Data/Profiles/` from the territory SOT; generic tools
  consume their explicit repository-relative paths. Project and verify every
  source-bbox margin, then run the planning dry run before admission.
- [ ] Add the named Spasskaya Tower control point plus at least two
  non-collinear territory controls with horizontal/vertical provenance and
  source-accuracy qualification. Implement the territory SOT's separate
  numeric, projection, control-network, source-uncertainty, horizontal-class,
  and vertical gates; emit worst-case/RMS receipts and add transform,
  distance, unknown-accuracy, oversized-error, and Z-mode sabotage tests.
  Production records must qualify XY control evidence independently from
  terrain/absolute-Z evidence and set tolerances that cover the explicit
  source + drift/resolver sums without hiding unknown terms.
- [x] Freeze the ProjectWorld/ProjectWorldData ownership boundary, descriptor
  direction, and production roots in the stable plugin/data SOTs.
- [x] Create the data/content-only `ProjectWorldData` plugin and its validated
  `Data/`, `Data/Manifests/`, `/ProjectWorldData/Generated/`, and
  `/ProjectWorldData/Authored/` layout. The compiler names the owner and
  generic C++/PowerShell derives every root from its UE `.uplugin`; no second
  descriptor, reusable logic, or custom generator lives in the data plugin.
- [x] Harden the generalized production boundary after external review:
  keep the strict manifest root documentation-free; generate owner-relative
  links to ProjectWorld-owned schemas; prove empty ProjectWorldData enrollment;
  pin both final audits to the required-map owner; require real filesystem
  confinement and matching source/compiler ID/path/owner pairs for non-fixture
  profiles. Regression proves a valid ProjectWorld authority cannot be borrowed.
- [x] Close the portable-schema enforcement gap after external review: shared
  C++ resolution checks the actual document-relative target for authored,
  presentation, and runtime profiles; their JSON Schemas admit the sanctioned
  owner-relative shapes; manifest documents from ProjectWorld,
  ProjectWorldData, and the validation sandbox pass full JSON-Schema validation.
  Wrong-type optional XY/Z provenance fields now fail closed.
- [x] Split anchor evidence by axis. Source admission now freezes declared
  accuracy, each canonical terrain cell carries its actual vertical source
  identity/datum/confidence plus sampling/quantization residual, and resolver
  v3 receipts expose horizontal and vertical identities and explicit fail-closed
  sums. `surface_snap` gets Z evidence from the sampled cell; `absolute` uses an
  explicit vertical provenance reference.
- [x] Freeze the extensible generation-layer dependency graph and dirty-impact
  contract in the territory SOT before the first production layer.
- [ ] Admit and hash Copernicus GLO-30 N55/E048 beside the already admitted
  N55/E049 tile. The 210-cell technical envelope plus source margin crosses
  west of longitude 49.0; fail closed until both products and the expanded
  extraction receipt prove full coverage. Geofabrik needs no new snapshot.
- [ ] Replace the invalidated 36-cell budget with a planning dry-run receipt
  and new machine-readable source, compile, Unreal, cook, package, memory,
  and traversal ceilings before accepted compilation.
- [ ] Create the E2E profile after the first accepted compile supplies its
  per-leg feature counts, incremental bounds, map package, and synthetic twin.
  Import ceilings from the replacement budget; do not invent expectations or
  derive success thresholds from the first realization.
- [ ] Read-only legacy inventory (no migration): every City17 map, sublevel,
  Data Layer, Level Blueprint; WorldSettings/GameMode/PlayerStarts/nav
  config; hard and soft references; feature activation dependencies;
  map-owned vs gameplay actors; exclusive and referenced content sizes;
  licenses/provenance of reusable art; hero locations needing coordinate
  alignment. Record as a ProjectWorld docs inventory record.
- [ ] Exit gate: exact envelopes, estimated ceilings, and legacy inventory
  accepted before any source or Unreal expansion.

## Slice 2 - Scale source ingestion and canonical compilation

- [ ] PRECONDITION (maintenance hardening, not a correctness blocker):
  promote enrollment from four hand-ordered commands into ONE command.
  The route is documented in
  [scripts/ue/world/README.md](../../scripts/ue/world/README.md), but at
  210 cells make an ordering mistake expensive. Do this
  before the first territory-scale enrollment, not after.
- [ ] Versioned raster manifest; verify each original tile independently;
  deterministic mosaic/resample; reject gaps, unresolved nodata, inconsistent
  overlap, or mixed vertical datums. Give the accepted mosaic one composite
  identity derived from the ordered tile identities/hashes, common datum, and
  worst qualified input accuracy/confidence plus the frozen mosaic/resampling
  contract; retain component lineage. Canonical cells reference that composite
  authority, not raw tile IDs, so a valid N55/E048-to-E049 boundary passes the
  seam rule.
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

Create the `kazan_territory_v1` E2E validation profile at slice start. Its
thresholds come FROM the already-frozen budget
under `Plugins/World/ProjectWorldData/Data/Profiles/`,
never from measurements taken after the first realization - that ordering
is the whole point of freezing them in Slice 1. Per-leg expectations
(`expected_features`, `incremental_bounds`, `map_package`, and the
synthetic twin) are filled from the first territory compile, which is the
only part that legitimately could not exist earlier. A provisional ceiling
may be re-frozen from Slice 3/4 measurements, but only as an explicit
recorded decision; a silent breach is a gate failure. Reuses the existing
E2E framework; profile/schema and validator extensions are expected work.

Per-layer admission (contract SOT: territory doc "Proof split"): every new
OR MATERIALLY CHANGED generator, artifact layout, anchoring contract, or
ownership scope passes, BEFORE territory-scale use:
minimal synthetic implementation -> authored overlay and polish fixtures
above it -> full/incremental/rejected/clean regeneration matrix ->
layer accepted -> territory-scale realization allowed.

- [ ] 3-PRE Implement only the extensible-layer core with terrain: JSON schema
  and loader; stable layer/generator/version IDs; ownership/exclusion kinds;
  dependency-DAG validation and topological planning; owned manifests;
  whole-layer and exact-cell/source-tile dirty selectors; input-hash discovery;
  manual dirty-set union; transitive cell propagation. Test add/remove,
  generator replacement, display-only reorder, cycles, unknown generators,
  root overlap, cell precision, unchanged-scope equality, and rollback.
- [ ] 3B-EXT Add dependency halo propagation with the first roads/water
  consumer that requires neighboring cells; keep zero-halo layers exact.
- [ ] 3E-EXT Add stable-object dirty selection with generated gameplay
  placement, its first real consumer. Do not prebuild unused selector modes.
- [ ] 3A Terrain + water: replace the representative always-loaded Landscape
  endpoint with the World Partition SOT's one-logical-Landscape baseline:
  210 canonical-cell components and baseline proxies under the single
  `kazan_main` terrain partition. Make `terrain_partitions` list-shaped but
  implement only that entry; prove seamless edges, exact cell ownership,
  incremental replacement, and clean topology-version regeneration. Allow
  proxy bundling only through Slice 4 evidence; split into additional logical
  partitions only when the documented engine/performance escape condition is
  actually reached. Water passes per-layer admission; water realization
  decision frozen in a ProjectWorld decision
  doc with a supported non-Experimental fallback (polygon/river-line
  behavior, elevation, shoreline intersection, cross-cell ownership,
  material/collision policy, navigation policy); deterministic clean
  rebuild; continuous cross-cell water.
- [ ] 3B Roads: select realized road classes; widths, bridge/tunnel
  fallback, intersections, terrain conformance, and player collision across
  the product territory. AI navigation remains scenario-owned.
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
  collision ownership; player collision across the product territory; AI
  navigation remains scenario-owned; no interiors, no final art.
- [ ] 3E Generated gameplay placement: designer-authored JSON can create
  dynamic-object definitions and initial placements in a generated layer.
  Runtime state is saved/replicated separately and never written into the
  generation source. Prove object-level dirty regeneration by stable ID.
- [ ] Exit gate: the TERRITORY matrix per the contract's stage-scoped
  definition - complete for every generated layer enabled in
  `kazan_territory_v1`, with modular assembly recorded as "not enabled"
  (pending, never N/A); anchor-class coverage and recorded N/A pairs per
  contract - plus clean rebuild, unchanged Apply, incremental rebuild,
  failure rollback, layer counts, seam checks, package audit, and
  required captures, all with live receipts.

## Slice 4 - Automated partition design and runtime proof

Implement the machine-readable design gate defined in the World Partition
SOT. Fixed cameras and manual walkthroughs are diagnostic, not authority.

- [ ] Audit actor bounds/reference bundles, per-cell weight, Landscape proxy
  ownership, Data Layers, HLOD coverage, and source-speed loading coverage.
- [ ] Apply and read back each accepted candidate runtime profile; build/cook
  through supported commandlets and reject descriptor or package drift.
- [ ] Start from one 2D grid at `256 m` cells / `768 m` loading range; compare
  the bounded `128/768`, `256/768`, and `512/1536` candidates plus declared
  Landscape proxy bundles and HLOD variants, changing one concern at a time.
- [ ] Run deterministic dense-centre, diagonal, perimeter, backtrack, and
  future-vehicle stress routes with cell, latency, hitch, memory, CPU/GPU,
  package, and cook evidence.
- [ ] Change ONE concern at a time (World Partition cells/loading, HLOD,
  Nanite applicability, instancing, PCG, Data Layers); retain a change
  only when the same automated receipt proves a net benefit without
  breaking regeneration, package size, or fallback behavior.
- [ ] Coordinate package attribution with
  [package size investigation](../02_backlog/content/package_size_investigation.md).
- [ ] Apply hard correctness/budget gates first; among passing candidates,
  select lowest p99 hitch, then peak memory, then activation churn. Persist
  the winner in the runtime profile and receipt and update the World Partition
  SOT without another operator round-trip. Stop for operator decision only if
  no candidate passes or the solution changes a frozen constraint.
- [ ] Prove RTX 4070 High 1440p/60 and RTX 3060-class Medium 1080p/60 plus its
  explicit 30 FPS fallback. Epic/Cinematic remain optional non-gating tiers.
- [ ] Exit gate: accepted packaged traversal receipt; every retained
  optimization has before/after evidence and the selected profile is the
  stable documented default.

## Slice 5 - Integrate the first scenario

Ownership rule: ProjectWorld owns spatial acceptance only (map identity,
territory bounds, spawn/route anchors, collision, navigation roles, streaming
roles, capture/traversal paths, budgets). The scenario's objective graph,
state transitions, item requirements, success/failure, and feature
activation live in ProjectSinglePlay or existing feature data. The
protected overlay owns spatial anchors and hero actors, never gameplay
logic. Baked local content only - no launcher delivery, hot-mounted region
packs, or new IoStore mounting path in this milestone.

- [ ] Register the generated map through the existing experience route and
  add one explicit second menu entry after its load/package gate passes. The
  widget owns the logical experience ID, never the map package path.
- [ ] Spawn inside the territory on generated ground; verify
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

## Slice 6 - Integrate and retire legacy content by class

Execute the accepted
[legacy transition decision](../../Plugins/World/ProjectWorld/docs/legacy_world_transition.md)
against the Slice 1 inventory. City17 remains a supported bug-fix-only menu
experience until every retirement condition in that SOT passes.

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
- The whole product territory receives generated ground, player collision,
  and streaming/traversal acceptance. Scenario content density and AI
  navigation remain bounded by scenario needs.
- Human place names remain aliases; coordinates and generated IDs own
  identity.
- No required paid, hosted, Marketplace, or Experimental dependency.
- Persistent contracts, budgets, and decisions land in the referenced docs;
  this file owns only execution steps.
