# Restore World Generation Locality and Identity Correctness

**Status:** DONE 2026-10-04 - independent R1 and R2 PASS; S0-S7 and the production authority
audit accepted. The exact historical raw-source replay remains a separate backlog task.
**Priority:** release-required (D3), completed after
`20261002-1602_world_fix_capture_readiness.md` and before 3.0.0 R5.
**Scope:** World producer identity and its mandatory verify net, one layer producer contract with
one leaf module per built-in producer (open/closed), representation leaks in generic World code, the dirty closure, test-staleness
routing, and dead generation paths. Upstream tool identities were audited and stay (Decision 7).
**Stable documentation owner:** Impact-Driven Development in
[World pipeline layers](../../../docs/testing/world_pipeline_layers.md) (identity and locality rule),
[Canonical World Realization](../../../scripts/ue/world/README.md) (fingerprints, manifests, audit),
[Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md) (producer
contract), [ProjectWorld pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md) (33, 36).
**Origin:** reviewer brief supplied by the operator (D2). Absorbs the prototype
`20261002-1628_world_narrow_map_fingerprint_scope.md`, removed as a duplicate.

## Contents

- Progress and handoff (final closure and historical execution record)
- Goal / Authority register / Non-goals / Read first
- Verified evidence: owners, edges, findings F1-F22, Q1 analysis
- Current architecture / Problem and root cause
- Decision 1-9 / Verify table / Developer workflow / KISS gate / Alternatives / Required invariants
- Mutation matrix / Build-locality note / Proof traceability
- Implementation slices S0-S7 with the S5 operations / Cleanup plan / Verification plan
- Documentation plan / Rollout and rollback / Completion criteria / Review record

## Progress and handoff

### Final closure - 2026-10-04

- Reviewer findings 1-3 are corrected: Matrix authenticates saved/reloaded Mesh
  Terrain, Map, and Building Massing projections after the builder, limits
  clean path reallocation to external actor artifacts in map/terrain scopes,
  preserves both rename endpoints in planning, and skips raw-cache scanning
  and budgets in canonical-authority mode. Focused negative controls reject
  saved collision drift at unchanged inputs and counts. The terrain audit
  calls the same producer projection as the compiled-output sabotage verify.
- Common Check `check-20261004T080456Z` and canonical-authority Kazan Matrix
  `run-20261004T081540Z` accepted on the corrected code. The Matrix passed
  unchanged Apply, road locality, rejected overlay, clean reconstruction,
  saved projections, and exact restoration of all 3096 generated artifacts
  (pre/post tree SHA `dfc6df6e3d39307d63d4d69473bbf7a00166d241e35935fe12eef59afe818b66`).
  An earlier R2 Matrix failed on the second Apply because the isolated first
  leg replaced actor packages while a later Unreal process used stale Asset
  Registry entries. The realization runner now passes `-NoAssetRegistryCache`;
  a protected exact isolation probe passed a full rebuild, both projections,
  a zero-write second Apply, and byte-identical restoration before the final
  Matrix.
- Reviewer finding 4 is closed at saved output: the recovery test persists a
  stale building actor, detects it through the real lifecycle, replaces it,
  reloads, and proves no writes on the following Apply. A new verify test
  proves retired actors remain observable in the building projection. All
  122 changed pre-S5 Kazan building payloads were recovered from pre-S5 Git LFS
  objects and checked against the old manifest, then substituted under an
  outer recovery marker. The historical saved projection contains 61 v1 and
  110 v2 actors; the current projection has 171 v2 actors. All 342 actor/mesh
  records have equal meaningful placement, geometry, material, collision, and
  tag fields after excluding the producer-version tag and allocation IDs.
  Evidence: `tmp/world/locality/reviewer_r2/historical_building_diff.json` and
  `Saved/Validation/WorldRealization/reviewer-r2/historical-buildings/`. The
  original generated tree was restored byte for byte. There is no retained
  pre-S5 snapshot; the reviewer's preserved-snapshot premise was inaccurate.
- The final post-S5 production Apply probe accepted on Kazan and Manhattan:
  zero actor/mesh writes, zero dirty units, unchanged World and ProjectMaterial
  identities, accepted authority audit and Material Validate, and clean marker
  removal. Evidence: `Saved/Validation/WorldRealization/reviewer-r2/post-s5-noop/`.
- The complete review history starts at commit
  `5a3f820c3b9267620e5ea804f26ec8c2c2ef9b60` and includes the committed
  CanonicalCompilation cleanup in `2b282a4873f695b2f38d082ef2f258745d7002b3`. That cleanup makes
  `algorithm_version` required; all four admitted production compile profiles
  already declare the same v2 value, and CanonicalCompilation tests accepted.
  The reviewed World candidate started at commit `2b282a4873f695b2f38d082ef2f258745d7002b3`,
  so its Matrix selected `canonical_authority`. The final receipt claims no
  historical source replay.
  The complete path set is recorded in
  `tmp/world/locality/reviewer_r2/slice_history.json`: review base
  `5a3f820c3b9267620e5ea804f26ec8c2c2ef9b60`, candidate base
  `2b282a4873f695b2f38d082ef2f258745d7002b3`, 312 committed paths,
  1936 uncommitted paths, and 2219 unique paths. The exact pinned raw PBF
  remains unavailable.
- The exact-source clean consumer checkout is accepted at
  `tmp/world/locality/reviewer_r2/clean_consumer_workspace.json`. Its overlay
  authenticates all 1936 changed paths, required LFS payloads, canonical
  bundles, descriptors, and verify baselines without copying old Saved/tmp
  proof. In the first isolated checkout, `plan --base HEAD` chose
  `canonical_authority`, `plan --base HEAD^` chose `full`, the editor built
  successfully, and the exact retired-building projection test passed. The
  final authenticated checkout differs from that tested checkout only in this
  task's handoff text. Its source identity is
  `4798f197eb2ae7e64ec71e71ed899f58916906079f760a2bf501bd9dafc03138`.
- Final common Check `check-20261004T094153Z` and canonical-authority Kazan
  Matrix `run-20261004T095238Z` accepted after the isolated-source helper
  correction. The Matrix restored all 3096 ProjectWorldData artifacts with
  equal pre/post tree SHA
  `dfc6df6e3d39307d63d4d69473bbf7a00166d241e35935fe12eef59afe818b66`.
  Direct post-Matrix audit
  `tmp/world/locality/reviewer_r2/final_post_matrix_audit.json` accepted all
  14 current scopes, fingerprints, ownership, and artifact bytes. No recovery
  marker remains. `git diff --check` and changed-document ASCII checks passed.
- The reviewed candidate identity is
  `aa6246e751a69363745a0b672ed14bc3efd2bfc3a22dbba903aa48409431b073`
  over 1936 changed paths. The SHA-256 of
  `tmp/world/locality/reviewer_r2/candidate_manifest.json` is
  `d6dc677e7597f85ac6f5c1357541809104a249e5447c82bd3641505966da9118`.
  Every path and file hash matches commit `ecd14a610e8628f4b538520662cb8b3e9b4bfa52`.
  Closure-only task and release-router edits follow that reviewed commit.
  Independent R2 PASS accepted the four corrected findings and the production
  repair; packaging/runtime proof remains the 3.0.0 R5 release-candidate gate.

### S5 closure handoff - 2026-10-04 (historical)

- Common Check `check-20261003T232200Z` and canonical-authority Kazan Matrix
  `run-20261003T233233Z` accepted on the final World code. The Matrix restored
  the exact generated tree. The old active set and all 3096 artifacts then
  matched the pre-run audit, with only 14 expected stale fingerprints.
- S5 exposed a production-only inconsistency: 61 Kazan building actor packages
  carried `ProjectWorld.BuildingMassing.v1` even though the active manifest and
  profile said v2. The focused `Project.World.Realization.Buildings.PersistentLayer`
  test covers replacement before garbage collection. Unreal's `Requested`
  spawn name is used only when a retired actor reserves the required name;
  otherwise the stable required name is retained. Two cold twin probes match
  the recorded pipeline projection. The S5 operation table below was narrowed
  to 10 metadata-only scopes and four realized scopes (terrain and buildings
  for each map). The old O2-only-terrain statements below are historical.
- S5 completed under one lock, outer snapshot, and recovery marker. Its final
  World audit, Material Validate, and marker removal accepted. Direct follow-up
  audit `tmp/world/locality/s5_migration/post_s5_audit.json` accepts all 14
  current scopes and 3096 intact owned artifacts. Read-only Kazan and Manhattan
  Validate receipts in `Saved/Validation/WorldRealization/s5-locality/` accept
  with no dirty layers. The v1 building actor package scan found zero.
- Manifest diff: 61 Kazan building actor paths replaced and 61 Kazan building
  artifacts changed at stable paths; 210 base terrain artifacts changed in each
  map, while 315 Kazan and 321 Manhattan compiled-section paths were reallocated.
  Every other scope's artifact paths and digests stayed equal. The direct terrain
  projections of both maps stayed equal. The 10 O4 manifests contain metadata
  refreshes only. ProjectMaterial generated surfaces and its manifest refreshed.
- Final static checks after S5: `git diff --check`, 506 JSON schemas,
  1210-file naming check, engine-path check, and changed-document ASCII check
  accepted. The exact editor build, focused building test, 43 manifest Pester
  tests, common Check, and Matrix accepted before S5. A local diff review of
  the changed code/docs and a per-scope manifest comparison found no further
  in-scope correction. Independent R2 remains. The exact pinned raw PBF is still unavailable for a
  full historical source replay; it is tracked in the separate mirror backlog.
- `plan --base HEAD` on the S5 tree selects `canonical_authority` for Kazan
  and identifies ProjectWorldData as the L3 candidate owner. It also flags
  the packaged-runtime boundary for L4; the final package proof belongs to
  the R5 release candidate. A post-S5 no-op editor build had zero actions.
  The candidate remains uncommitted, so a clean-checkout consumer route on
  these exact bytes has not been proved; keep that completion check open.
- The sections labeled `Latest handoff - 2026-10-03` and `Continuation on
  2026-10-03` below are historical execution notes. Use this current handoff
  and the S5 operation table for the current decision.

### Latest handoff - 2026-10-03

- Reviewer PATCH resolution in progress: `EndToEndValidation run` now has
  `--from-canonical-authority --changed-base HEAD` for the uncommitted World
  slice. `plan` reports `canonical_authority` for World-only paths and `full`
  for source/canonical paths. The new route selects Kazan, authenticates and
  materializes the promoted bundle, and runs World Apply, no-op, road locality,
  rejected overlay, clean reconstruction, and exact generated-tree restoration.
  It reports source and fresh compile proofs as absent. It refuses changed
  upstream paths. Focused Python routing and Matrix tests passed 38/38;
  the exact common Check and production Kazan Matrix remain the next gates.
  `ProjectWorldGeneratedActorLifecycle.cpp` no longer preserves an ownerless
  generated actor through Water/Road/Vegetation/generic cell tags. Static
  partition audit rejects that actor explicitly. The HostBoundary negative
  control passed its editor build and exact Unreal test.
  SOTs now explain mode selection and the raw-cache boundary. Continue with
  exact current Check, canonical-authority Kazan Matrix, then S5. A first
  canonical-authority Matrix rejected in the Mesh Partition builder because
  UE loaded stale Asset Registry cache entries for actor packages backed up
  by Matrix. The producer now passes UE's `-NoAssetRegistryCache` option. A
  focused first-leg probe passed Apply, Mesh Partition build, postbuild
  inventory (202710 canonical height samples), receipt copy, and generated-tree
  restoration. The generated authority audit after that probe found 3096
  intact artifacts and only the expected 14 stale fingerprints. An additional
  canonical-mode work-directory creation fix is covered by a focused test.
  If authority validation itself fails, stop and
  diagnose that boundary; do not repin the historical source.
- The replacement common Check `check-20261003T195733Z` accepted all suites on
  the reviewer fixes before the latest validator corrections. The production
  canonical-authority Matrix `run-20261003T200713Z` completed first, no-op,
  road-locality, rejected-overlay, and clean-rebuild Unreal legs. Its final
  validator rejected on a missing observational `reports/metrics.json` in the
  promoted bundle; the generated tree was restored. The validator now derives
  canonical bytes from the authenticated compile-result inventory. The saved
  legs then exposed pre-existing Matrix assumptions: UE Mesh Partition gives
  new external actor package paths on isolated clean reconstruction, the
  builder contributes 315 artifacts after the first/clean child receipt, and
  the Kazan territory product runtime does not run the synthetic road collision
  probes. These boundaries are now represented explicitly: no-op/locality paths
  stay fixed, clean map/terrain scope counts and terrain producer semantics
  stay equal, first/clean terrain receipt count is 211 and final manifest count
  is 526, and runtime policy checks match the territory product route. A
  read-only validation of the saved five legs now passes. Focused negative
  controls for locality path churn, clean semantic drift, and scope count drift
  pass. Run one final exact common Check, then the canonical-authority Matrix
  again before S5; prior receipts are stale after these validator/profile edits.
  The End-to-End validation profile mixes World and upstream fields. A final
  route guard now compares source/compiler references and upstream budgets to
  the reviewed Git base, so its World-only artifact-count correction still
  selects `canonical_authority`, while upstream edits require `full`.
  The final common Check `check-20261003T205955Z` accepted all suites. Matrix
  `run-20261003T211005Z` completed every Unreal leg and restored the tree, but
  final validation rejected `feature_count_mismatch`: the profile's 2026-08-17
  expectation was 67136, while the 2026-09-26 promoted canonical bundle and
  the current World manifest compile hash identify 67131. The World-only
  expectation is now corrected to 67131; source identity is unchanged. A
  read-only diagnostic of the saved five legs now traverses all final validator
  checks using the actual receipts and promoted authority, with synthetic
  equal authored/restoration values only for fields the failed run did not
  persist. Run the exact common Check and Matrix once more before S5.
- Final common Check `check-20261003T213202Z` and canonical-authority Matrix
  `run-20261003T214204Z` accepted. Matrix passed the Kazan D0-D3, attribution,
  distribution, and exact generated-tree restoration checks. The post-Matrix
  audit showed the same active-set SHA and 3096 intact artifacts with only the
  predicted 14 stale fingerprints. S5 started under one outer snapshot and
  recovery marker but refused at O4 before publishing: migration candidates
  inherited a root-relative `$schema` while prospective validation requires
  the staging schema ID. The coordinator restored both authorities and removed
  its marker; direct audit verified the original state byte-for-byte. A new
  Pester case was red on the exact error, then green after a one-line candidate
  fix; all 43 generated-manifest tests pass. A read-only O4 preflight accepted
  all 12 metadata-only candidates. The exact Check/Matrix receipts predate
  that script change. Rerun them before retrying S5.
- S0-S4 production and focused verification are implemented. S6 stable docs are updated. The
  replacement common Check at `Saved/Validation/WorldPipeline/check-20261003T141956Z/result.json`
  accepted all eight suites, including 65 Unreal Realization cases and the twin. A later small
  `record_verify_baseline.ps1` change replaced a fixed `ValidateSet` with an identifier pattern
  so newly registered producers can use the same entrypoint; its invalid-name refusal was
  checked separately. This means the accepted Check does not cover the exact final tree.
- The Kazan Matrix `run-20261003T142945Z` rejected at `kazan_source`, before any production
  authority changed. The pinned source profile requires
  `volga-fed-district-260802.osm.pbf`, 769160200 bytes, SHA-256
  `e2a149c36a3eb3b33bee50e986006519f9a8255eecedda9e6e0b67e93ee471b1`.
  Its Geofabrik URL returns HTTP 404. The local cache and repo/sibling search found no exact
  file. The public archive lists 260801 but not 260802. Do not repin a different date silently:
  that changes canonical geography and invalidates the O2-only-terrain S5 contract. Exact raw
  source recovery remains a separate full-replay task; it is not the World-only S5 gate.
- Source-gate locality finding (operator discussion 2026-10-03): the accepted 2026-08-28 Kazan
  Matrix reports 823461441 source-cache bytes and zero network transfer. The cache is now empty,
  while the pinned profile did not change. `CanonicalCompilation bootstrap.py authority` still
  validates the tracked Kazan bundle (638 outputs, bundle SHA-256 `995fb0fb...`).
  `EndToEndValidation/app/execution.py:_run_profile` nevertheless runs Kazan source ingestion and
  two compiles unconditionally before materializing that promoted bundle for Unreal. Thus the
  old full L2 Matrix coupled realization-only changes to raw provider availability. The
  canonical-authority route above corrects this; keep full source->compile Matrix for changed
  source/canonical owners and release-level reproducibility. Do not run S5 before the new L2
  evidence and enrollment route are verified.
- S5 has not run. The prepared, ignored coordinator is
  `tmp/world/locality/s5_migration/coordinator.ps1`; review it before using `-Run`. The read-only
  pre-S5 audit is `tmp/world/locality/s4_pre_s5_audit.json`: exactly 14 World scopes have stale
  `generator_fingerprint_current`; their other authority checks and 3096 artifacts pass.
  Both production terrain projection probes passed before S5 (Kazan 210 bases/315 sections,
  Manhattan 210 bases/321 sections) under `Saved/Validation/WorldRealization/s5-locality-pre/`.
- S7 independent checks completed: 186/186 World Pester, 492 data schemas, 1210-file naming
  check, engine-path check, and `git diff --check`; no-op build had zero actions, a Water leaf
  edit rebuilt only Water's compile/link actions, and its exact verify passed. Seven leaf
  sabotages rejected expected output fields. The map ISO control rejected four `camera.iso`
  fields. Compiled terrain controls rejected six section `tags` fields and two section
  `collision.0` fields at the same verify identity; Map remained equal in both. The collision
  control is a temporary transformer code mutation, with source and binary restored. The final
  clean twin accepted at
  `Saved/Validation/WorldRealization/verify-twin/2b4c9c5bd6bb4220a6f9febff14b482e/verify.json`.
  Evidence is under `tmp/world/locality/workflow/s7/`.
- Remaining: obtain the exact pinned PBF; resolve the Check/Matrix gate budget against the exact
  final tree; run an accepted Kazan Matrix; review and execute S5 under its one-lock recovery
  contract; repeat both audits and Material Validate; finish S7 and independent R2. Only then
  move this and capture-readiness todos to `01_done/`. Do not treat the older continuation and
  remaining-work list below as current state.

State on 2026-10-03. Nothing is staged or committed; commits stay with the operator.
S0's verify net and diagnostic inventory are complete. The table below records completed chunks, and the
continuation section records the current partial slice.

### Continuation on 2026-10-03

- S2 identity implementation and focused exit are green: descriptors and v4 fingerprints,
  v2 request/identity-dirty closure, Mesh Terrain producer tags and receipt v2, and the
  ProjectMaterial ABI reader are in place. The editor build, 492 JSON schemas, 195 World
  Pester cases, 9 Material host Pester cases, 20 planning cases, exact request/closure/tag/
  Material tests, the isolated twin's Map/MeshTerrain/Pipeline verifies, and all five leaf
  verifies plus BaselineRules passed. The read-only ProjectWorldData audit found exactly
  `generator_fingerprint_current` stale across 14 scopes; transaction, manifests,
  artifacts (3096 files), ownership, references, and unowned scans passed. S2 evidence:
  `tmp/world/locality/s2_identity/` and
  `Saved/Validation/WorldRealization/verify-twin/b9f998451e314166b2f0f384aa03be12/verify.json`.
  S3 has started: removed the duplicate script-side tuple validator and its dead direct
  tests (root confinement remains), removed unused Landscape/Foliage dependencies and
  the empty ProjectPCG plugin, and made CanonicalCompilation algorithm_version required
  (90 tests passed). The generic neutrality scan passes after limiting its source scan
  to production code, since fixtures and verify runners must name producers. Continue
  S3 cleanup and verification, then S4 and the single common Check/Matrix gate before S5.
- S2 identity C1/C2 details: the S0 producer descriptors now drive a v4 fingerprint made
  from producer id, output revision, pipeline revision, engine Build.version identity, and sorted
  declared data-input digests. Map and presentation declare no leaf module. Source files, modules,
  scripts, schemas, and descriptor formatting do not enter the identity. Catalog validation refuses
  missing or duplicate declarations and unknown producers. The audit reports a manifest whose
  producer descriptor is absent as stale while still checking artifacts and unowned files.
  `generator_fingerprint.Tests.ps1` now passes 15/15 including malformed descriptor controls,
  profile coverage, and a production source scan. The manifest and audit suites also passed.
- S1 exit: all built-in layers now register through leaf modules, and the shared result type
  carries producer counters in a generic metrics map. Result schema v2 emits sorted per-layer
  metrics. The focused Python Matrix/validation/planning suite passed 61/61; 492 inline schemas
  validated; editor build, UBT target export, `Layers.HostBoundary`, `Buildings.PersistentLayer`,
  `GameplayPlacement.Lifecycle`, and `Vegetation.RetirementPersistence` passed. The isolated twin
  passed after the counter migration. The full TestData lifecycle passed first, unchanged,
  one-cell, rejected rollback, and Delete legs with a v2 result. A read-only Kazan static audit
  accepted with 211 extent actors, 315 terrain role actors, and zero missing cell identities.
  `git diff --check` is clean. The common Check remains an S4 gate because its declared S2/S3
  failures are still present. Next: S2 identity, starting with descriptor revisions and the
  fingerprint contract in `tmp/world/locality/s2_identity_design.md`.
- S1 I13/I30: the producer descriptor now owns terrain runtime acceptance expectations;
  `new_product_terrain_acceptance.ps1` resolves exactly one descriptor by generator pair and writes
  those expectations into the authenticated contract. The product runtime parses the three exact
  fields and refuses missing/unknown values before navigation. The red
  `Neutrality.UnknownTerrainGeneratorRejected` exact Unreal test is green; Pester 3/3, editor build,
  and data schema validation passed. The realization profile JSON schema now checks generic
  identifier/envelope syntax and leaves executable producer tuples to the C++ registry; its Pester
  schema test and 492-file data validation passed. The descriptor lookup was narrowed to
  `Data/Producers` directories (Pester runtime 5.8s to 1.3s). C1 registry and the remaining S1
  chunks are still outstanding.
- S1 C1 partial: added `IProjectWorldLayerProducer` on `IModularFeatures`, declaration/tuple
  validation, duplicate and malformed-registration rejection, generic unit hashing, apply,
  capture, delete, owned-actor lifecycle, and a topological layer pipeline. Profile admission
  accepts a probe registered outside the host allowlist; duplicate producer layers now reject,
  and ready nodes use declaration order. The probe HostBoundary exact test covers two units,
  apply, capture metrics, preservation, stale-cell deletion, and default delete; exact registry
  and profile tests pass. The isolated twin `-Verify Twin -Probe` passed twice with equal map,
  terrain, and pipeline projections and different saved package bytes; its outer transaction
  restored TestData. The built-in producers still use transitional dispatch and the terrain
  adapter now registers through the modular producer interface; the old terrain registry was removed.
  A read-only Kazan static audit accepted 211 cell extent actors and 315 terrain role actors with
  zero missing cell identities. The host no longer tests the adapter's partition count.
  C1 is NOT complete: five built-in leaf modules and the remaining generic ownership paths remain.
- S1 C2: the adapter declares its post-apply builder; generic capture and result writing
  carry the declaration, and `realization_layer_operation.ps1` dispatches declared inventory,
  builder, and final inventory commandlets. The adapter audit exports `builder_artifacts` v2.
  The runner authenticates schema, map, accepted final status, and artifact paths before merging;
  it checks all declarations before starting a builder. Pester passes 20/20, including three-call
  order and unsafe-argument refusal. The exact `Layers.HostBoundary` and `Layers.ProducerRegistry`
  tests pass; the former reads serialized metrics and builder data. The isolated twin comparison
  accepted with equal recorded projections; its final audit reported six builder artifacts.
  Next S1 work: leaf modules, generic ownership completion, and result-v2 metric consumers.
- S1 gameplay leaf: `ProjectWorldGameplayEditor` now owns placement implementation, provider
  hashing, artifacts, verification projection, and focused tests. Its descriptor names that
  module, and the generic host no longer names gameplay in dispatch, hashing, capture, or tuple
  validation. Required data-root, schema-reference, profile-load, and generated-actor seams were
  exported from the host. The editor build, exact gameplay lifecycle, profile contract, and
  gameplay verify tests passed; UBT target export was refreshed, 492 JSON schema files and 12
  focused check-contract tests passed. An isolated twin Apply and saved-output compare accepted
  with S0 baseline projections. Four built-in leaf modules remain.
- S1 road leaf: `ProjectWorldRoadEditor` now owns settings, cell hashing, realization, actor
  artifacts, verification projection, and its exact verify test. The descriptor names that
  module; the host has no road-specific dispatch, hashing, capture, or tuple validation.
  Profile contract, road verify, incremental inventory, and data schema validation passed.
  The F9 material regression now passes: an existing mesh is preserved only when its bound
  material also matches the requested presentation. An isolated twin Apply and saved-output
  comparison accepted with the S0 projections. A road `.cpp` edit rebuilt one compile action
  and only the road module library and DLL links. Three built-in leaf modules remain.
- S1 water leaf: `ProjectWorldWaterEditor` now owns settings, cell hashing, realization, mesh
  building, actor and material artifacts, its verify projection, and focused water tests. The
  generic host no longer names water in apply dispatch, hashing, capture, or tuple validation.
  The mixed native twin test was split: canonical contract parsing remains in the host, while
  Water geometry and persistence tests live in the leaf. The editor build, ProfileContract,
  PersistentWater, WaterCanonicalContract, CanonicalSurfaceMesh, WaterGeometryAndSeams,
  WaterAssetPersistence, Water verify, and IncrementalInventory exact tests passed. UBT target
  export and 492-file JSON validation passed. An isolated twin Apply and saved-output compare
  accepted with the S0 projections. Two built-in leaf modules remain: Vegetation and Building.
- S1 vegetation leaf: `ProjectWorldVegetationEditor` now owns settings, cell hashing, placement,
  exclusions, realization, actor artifacts, its verify projection, and focused tests. The generic
  host no longer names vegetation in apply dispatch, hashing, capture, or tuple validation.
  `ProjectWorldAuthoredOverlay.h` moved to the host public interface for the leaf's data contract.
  The editor build; ProfileContract, PlacementContract, ExclusionContract, AssetContract,
  RetirementPersistence, Vegetation verify, and IncrementalInventory exact tests; UBT target
  export; and 492-file JSON validation passed. An isolated twin Apply and saved-output compare
  accepted with the S0 projections. Building is the remaining built-in leaf module.
- S1 building leaf: `ProjectWorldBuildingEditor` now registers both supported v1 and v2 producer
  tuples and owns settings, hashing, realization, mesh building, artifact capture, verification,
  and focused tests. The generic host's last producer-specific dispatch, hash, capture, tuple
  validation, and fallback were removed; missing producers now fail closed. The editor build;
  ProfileContract, TopologyAdmission, Dimensions, EffectiveVolumes, PersistentLayer,
  InputLocality, BuildingMassing verify, and IncrementalInventory exact tests; UBT target export;
  and 492-file schema validation passed. The existing F9 building material regression failed on
  its saved-mesh assertion before the presentation binding fix and passed afterward; a building
  `.cpp` edit rebuilt one compile action and only the building module library and DLL links.
  An isolated twin Apply and saved-output compare accepted with the S0 projections. C1 still
  needs the audit cleanup and result-v2 consumers before S1 exit.
- S1 C9 (completed at S1 exit): the result writer now emits schema v2 with only generic and temporary P0
  `changes` counters; built-in producer output and work counters are in sorted per-layer `metrics`.
  The Kazan and synthetic Matrix profiles, validation schema, Python and PowerShell consumers use
  those metrics. Focused E2E Python tests passed 61/61; the editor build, `Layers.HostBoundary`,
  and an isolated twin Apply and saved-output comparison passed. The twin result has the expected
  metrics for all six layers. The fixed built-in counters were removed from the shared C++ result;
  the lifecycle and affected producer tests passed. P0 counters stay only until S3.
- S0 red: added `UnknownTerrainGeneratorRejected` in ProjectWorld runtime tests, observed red
  because product terrain acceptance does not reject the undeclared id; added
  `IdentityRefreshClosure` and observed water/roads/gameplay dirty on an identity-only terrain
  refresh (the intended S2 red); added the generic-script neutrality source scan and observed
  28 adapter name hits (intended S1 red). Profile contract has a one-cell halo control already.
- S0 twin: renamed the tracked synthetic twin files and references to
  `synthetic_territory_twin`; added building semantics v2, vegetation, a product runtime
  profile, and updated the synthetic Matrix leg. Canonical twin test 1/1, Matrix test 13/13,
  JSON validation 493 files, and RCW Validate passed. Gameplay baseline was re-recorded
  after the rename; no old twin token remains under Plugins/World, scripts/ue/world or
  tools/World. `ProfileContract` passes 1/1 after removing its obsolete duplicate vegetation
  fixture. `compile_twin_fixture.py` and its import from the locality variant generator run.
- A full TestData lifecycle initially found two Delete bugs. Loaded external actor package
  files needed `ResetLoaders` before deletion; Delete then reached artifact capture, which
  wrongly expected the removed terrain partition. Unreal's World Partition builder uses
  `ResetLoaders` for the same file-handle reason. Both were fixed in their direct owners.
  The existing lifecycle is the regression: first Apply, unchanged Apply, one-cell Apply,
  rejected rollback, and Delete all passed, ending with zero active TestData scopes.
  Receipt: `Saved/Validation/WorldRealization/3-core-lifecycle/
  7c6781f4bc2f4ed994b33c3f02d6dbba/summary.json`. No TestData content remains modified.
- Also fixed the package-locality variant fixture's overlay v2 provenance and its false
  assumption that water width cannot affect hydrologic terrain. The generated variants
  pass and show one direct terrain cell, one water feature cell, and two hydrologic cells.
  M24 Pester and M26 Python guard additions pass.
- S0 exact tests now green: `ProfileContract`, `DirtyClosure`, `IncrementalInventory`,
  `PersistentWater`, GameplayPlacement `Lifecycle`, and realization profile schema Pester.
  `git diff --check` is clean after normalizing two modified Python test files to LF.
- Next: finish the S0 twin Map/MeshTerrain/Pipeline projections, commandlet, wrapper,
  three baselines, probes and sabotages; add the two F9 presentation red cases; then S0
  measurements and S1 onward. Keep Unreal runs serial.
- S0 twin Map, MeshTerrain, and Pipeline baselines now exist. The TestData wrapper restores its
  generated roots after each run. A fresh compare accepted all three rows; the two-run probe
  produced equal projection hashes for all three and different saved package bytes. The pipeline
  baseline covers eight manifest scopes, six embedded layer projections, named artifacts, and
  external-actor counts. The common Check now exports the target graph before its first proof
  hash and runs the twin verify; 12 focused check-contract tests pass. Map cell-size and pipeline
  minimap sabotages both failed on their named fields. A canonical terrain-height sabotage was
  refused by the transaction audit; a code-only compiled-section tag sabotage failed the terrain
  projection on six section tags with unchanged map output. All sabotages were reverted in source;
  rebuild and a normal compare remain before calling this chunk settled. Receipts: `Saved/
  Validation/WorldRealization/verify-twin/{a5148840e7a749898134a471770a97bf,
  b89c96f618ab42bb978a5a36dda28952,5e5bc2c48b0a489fb219d6b4f7d2db63}/verify.json`;
  two-run probe: `tmp/world/realization_verify/record/Twin-20261003-110220/probe/`.
- Next: rebuild restored source and compare; add F9 red cases, measure S0, then implement S1.
- Restored source rebuilt and the normal twin compare accepted. The exact F9 road and building tests
  each failed on the intended saved-mesh material assertion: the first material stayed bound after
  an unchanged-unit Apply with the second material. Added the tracked twin source, compile,
  provider, authored overlay, gameplay, and marker-map inputs to all three verification
  identities; a record run updated all three baselines because their identities changed. Next:
  fresh compare, S0 workflow measurements, then S1. The declared-red set now also contains
  `UnknownTerrainGeneratorRejected`, `IdentityRefreshClosure`, and the two F9 material tests.
- S0 workflow sample, native Windows editor closed: no-op build 1.87 s / 0 actions; a temporary
  comment in `ProjectWorldWaterRealization.cpp` 8.45 s / one compile plus World Editor link;
  a temporary comment in public `ProjectWorldCanonicalBundle.h` 35.77 s / 32 compile/link
  actions, including the terrain adapter. Both edits and their rebuilds were reverted. One
  exact Water verify took 66.7 s wall and passed. Logs are in `tmp/world/locality/workflow/s0/`.
  This supports local leaf modules as the performance change; a Rust/Ruby rewrite of the
  seconds-long twin compiler has no measured basis. Data validation passed for 496 JSON files
  after correcting the generated pipeline baseline's `layer:` fixture prefix. The final pipeline
  baseline was re-recorded with that correction; a post-record fresh compare is still due.
- Terrain verification contract v2 now includes compiled channel texture source bytes,
  texcoord mapping, material parameters, and the stable layout receipt fields. A two-run
  fresh probe found the three Map/MeshTerrain/Pipeline projections equal while package bytes
  differed; JSON schema validation passed for 496 files. Map exposure ISO 100 -> 200 changed
  four `camera.iso` fields. A 1 cm compiled-only Mesh Partition mesh-component move changed
  four section keys while the transaction audit still accepted it. Both temporary sabotages
  were reverted, rebuilt, and a normal twin compare accepted all three baselines at
  `Saved/Validation/WorldRealization/verify-twin/cef3dfd0bfd7439fa05a0ee9b6e54bd3/verify.json`.
  S0 now has all intended twin sabotage controls and the F9 red tests. Remaining S0
  diagnostic work is the affected suite inventory; the S1 producer contract is next.
- S0 diagnostic inventory: `tools/World/tests` ran 8 cases, with only the two declared
  neutrality source scans red; `tools/World/EndToEndValidation/tests` ran 101 cases, with
  only the two declared F7 provenance cases red; `generator_fingerprint.Tests.ps1` ran
  18 cases, with 12 green and six declared S2 identity cases red. The exact UE
  `UnknownTerrainGeneratorRejected`, `IdentityRefreshClosure`, and both F9 material
  cases are red on their intended assertions. All twin sabotages were restored and a
  fresh compare accepted all three baselines. S0 is ready for S1, with the known
  pipeline external-package projection gap recorded for follow-up.
- S1 I23 is underway: `accept` now takes one authenticated territory Matrix run;
  its chain schema requires exactly the `kazan_territory_v1` key, and `plan` and
  the common-check proof closure select that profile. The focused planning and
  acceptance tests have been updated; the full affected suite is still pending.
  The cleanup row's instruction to delete `FProjectWorldPresentationGate` conflicts
  with live code: `presentation.py` launches `-ProjectWorldPresentationGate` for
  the packaged territory gate, and the `ProjectWorld` runtime module installs it.
  Preserve that gate while removing the bounded P0 branches. This is a
  correction to the D5 cleanup list, not a change to release acceptance.
- S1 I11/I23 continued: the territory validation profile now pins the enrolled
  Kazan runtime profile `kazan_territory_512_1536_v1` (its SHA-256 equals the
  active map manifest) and the packaged three-camera presentation gate. The
  retired P0 and representative validation profiles and their bounded runtime
  fixtures were removed. `ProjectWorldRuntimeProfile` and its schema accept only
  `territory_product`; an exact UE rejection test was red first and is now green.
  `ProjectWorldRuntimeRealization`'s route-only apply, collision probe, and
  budget branches were removed, shrinking 961 to about 530 LOC; a crafted
  retired profile was observed passing `Validate` before a fail-closed guard,
  and `Runtime.ProfileContract` is now green. `Runtime.ProductNavigationDomain`
  and `Runtime.ProductNavigationDomain` pass after the removal. The old
  `CreateOwnedActors` function is used only by legacy tests. Migrate those
  tests to the leaf modules, then remove its section builders after the new
  test hosts exist. This adjusts I11's test-helper order without carrying P0
  into the new producer contract. Exact Python affected suite 81/81 and JSON
  inline schema validation 492/492 passed. No UE build or test ran concurrently.

### Done

| Chunk | What landed | Evidence (`tmp/`, scratch) |
|---|---|---|
| Capture readiness | its own todo, `20261002-1602_world_fix_capture_readiness.md`: implemented, independent R2 PASS; its last proof is the ProjectWorldData audit accepted after S5 | `tmp/world/capture_readiness/` |
| Implementation design and R1 | four design notes, I1-I30, invariant 15; independent R1 PASS | `tmp/world/locality/{s0,s1,s2_identity,s2_recovery}_design.md`, `r1_review.md` |
| S0 tooling (I3, I7, I8, I10, I28) | inert descriptors in `Data/Producers/` of ProjectWorld (pipeline plus 7) and ProjectWorldMeshTerrain, keyed by generator id and version, optional `runtime_acceptance`; `project_world_producer_descriptor.schema.json`; `producer_descriptors.py`, `proof_input_fitness.py`; explicit proof roots in `contract_inputs.py` (plus ProjectCore, ProjectObjectCapabilities, `scripts/ue/material`, `scripts/ue/generated_content`); `plan` selects a verify from the baseline's `verify` field and needs no per-producer list; `scripts/ue/build/export_target_json.ps1` (UBT `-Mode=JsonExport` to gitignored `Binaries/Win64/AlisEditor.json`; measured: no makefile invalidation, next build 0 actions); `api.py` dead import gone; architecture test admits `Producers/` and `TestFixtures/Verify/` | `s0_tooling/` |
| S0 non-UE red tests | 6 descriptor-identity cases in `generator_fingerprint.Tests.ps1` (red; pass against a scratch S2 prototype); `test_world_generation_neutrality.py` (source scan red with 9 hits, including `ProjectWorldVegetationRealizationTests.cpp:52`; module direction green, needs a current export); `test_provenance_refresh.py` (F7 red, both halves) | `s0_red/` |
| S0 in-editor verifies | `Public/ProjectWorldVerify.h`, `Private/Verify/` (mechanics, five projections with static registrars), `Private/Tests/Verify/` (`Project.World.Realization.Verify.{Water,Road,Vegetation,BuildingMassing,Gameplay}` and `...Verify.BaselineRules` for M24 and M26), five baselines in `Plugins/World/ProjectWorld/Data/TestFixtures/Verify/`, `project_world_verify_baseline.schema.json`, `scripts/ue/world/record_verify_baseline.ps1` (cold path, `-Probe`, refuses an unchanged identity). Every sabotage red on the named projection field; two cold runs give identical projections; engine identity equals `GF`'s Build.version hash | `s0_verify/` (sabotage logs `sabotage/red_<name>.log`) |
| S2 recovery (early; production scripts, allowed after R1) | `PROJECT_GENERATED_CONTENT_LOCK_TOKEN` owned by the lock with `Enable-`/`Disable-ProjectGeneratedContentLockDelegation`, old name gone outside `todo/`; same-project Unreal check owned by the lock; outer-recovery marker and `recover_generated_content.ps1` (I17, I26); `generated_outer_snapshot.ps1`; journal-first commit and atomic promotion (I15); Material host delegation, staged restore, lock released last, structured receipt, `-OperationId`, `RestorePrevious` with authentication (`material_host_recovery.ps1`, I16); `cleanup_workspace.ps1` under the lock; suites wired into `run_all.ps1` | `s2_recovery/`; Pester: lock 10/10, outer recovery 22/22, Material host 15/15, outer snapshot 10/10, commit order 4/4; 13 removed-guarantee mutations each red |
| C8 guard (F22) | the production-enrollment guard is `Assert-ProjectWorldProductionEnrollmentAllowed` in `operator_controls.ps1`; six pure tests replace five wrapper runs; one wrapper test still proves the refusal wiring before any lock | `tmp/world/locality/c8_sabotage.py`: removing each of the five guard conditions fails exactly its test |

Authority check after these chunks (`tmp/world/locality/audit/after_s2recovery.out`): active set
`a93f6a01...` with 14 scopes, manifests valid, 3096 artifacts byte-identical, no pending journal.
Only `generator_fingerprint_current` fails, for every scope: the old source-hash formula hashes files
these chunks changed. That is expected and S5 refreshes all of them.

### Expected red set

These fail on purpose until the named slice lands; anything else red is a regression:
`generator_fingerprint.Tests.ps1` (6 new cases, S2); `test_world_generation_neutrality.py` source scan
(S1); `test_provenance_refresh.py` (2, S4). The C++ red tests of the next chunk join this list.

### Historical remaining-work list (superseded by final closure)

Only one agent at a time may build or run Unreal: `iterate.ps1` kills Unreal processes, and a live
editor blocks DLL links. Python and Pester work may run alongside.

1. **S0 C++ red tests** (not started; an agent stopped on the spend limit before writing code): an
   unknown generator id fails closed in profile validation and product terrain acceptance (red where
   today fails open); `Project.World.Realization.Layers.DirtyClosure` gains identity-only and
   presentation-change cases (red today) and a one-cell halo control (green today), per
   `s2_identity_design.md` section 6 and I22; a scripts neutrality test over `scripts/ue/world/*.ps1`
   (`s0_design.md` 7.4, red today). No production change.
2. **S0 twin chunk:** rename `synthetic_landscape_water_twin` to `synthetic_territory_twin` in every
   tracked reference (profiles, fixtures, integration scripts, tests, the generated map name); building
   v2 with `building_semantics`; vegetation; a `territory_product` runtime profile; synthetic-leg Matrix
   expectations (I4); `scripts/ue/world/verify_twin_realization.ps1` behind
   `record_verify_baseline.ps1 -Verify Twin` with the map, terrain (baseline in
   `Plugins/World/ProjectWorldMeshTerrain/Data/TestFixtures/Verify/`), and pipeline baselines (I2, I6);
   every sabotage of those rows red, including I25's code-only compiled sabotage; `checks.py` runs
   `export_target_json.ps1` before its first `common_contract_hash()` and before the
   `world_architecture` suite, and runs the twin verify; update the three twin strings in
   `ProjectWorldGameplayVerifyTest.cpp` and re-record that baseline (identity change only); the Pester
   M24 guard and the Python M26 gate test that `s0_design.md` places in those files.
3. **S0 exit:** the A10 measurements (UBT action lists for a no-op build, one producer `.cpp` edit, one
   `ProjectWorldEditor` public header edit; wall time of one verify, the verify suite, and the common
   checks, diagnostic); run the affected suites and confirm failures equal the expected red set (I27).
4. **S1** (`s1_design.md` chunks, I11-I14, I23, I30): P0/representative route removal with the accept
   route moved to `kazan_territory_v1`; the layer producer contract and registry on `IModularFeatures`;
   generic iteration of the F4 sites; the declared post-apply builder run by `RLO`; schema identifier
   patterns; neutral partition audit and product terrain acceptance from the descriptor's
   `runtime_acceptance`; five leaf modules with their verifies and projections; metrics map and result
   v2 with every consumer; the host-boundary test; descriptors name the leaf modules. Verifies stay
   green with the S0 baselines. Also check two findings from `s1_design.md`: delete-mode terrain
   capture, and the old adapter hash formula in `realization_layer_lifecycle.ps1:302`, `:355`.
5. **S2 identity** (`s2_identity_design.md`, I22): descriptor-based `GF`, request v2 with layer
   fingerprints and prior presentation hashes, Decision 6 dirty plan, adapter reuse tag and removal of
   the adapter `Build.cs` fingerprint, layout receipt v2, ProjectMaterial receipt parser, audit reporting
   a manifest without a descriptor as stale; S0's fingerprint and DirtyClosure tests turn green.
6. **S3** remaining `## Cleanup plan` rows (the P0 row moved to S1).
7. **S4** (Decision 8, I18): provenance refresh in the writer, enrollment, and acceptance;
   `test_provenance_refresh.py` turns green.
8. **Gates** (invariant 15, I27): one `bootstrap.py check` after S4 and one `kazan_territory_v1` Matrix
   on that receipt, each with at most one replacement. The last accepted Kazan Matrix receipts are gone
   (`e2e_realization_evidence_missing`), so this needs a fresh run.
9. **I24 before S5:** the territory contract's rule that a fingerprint cannot advance without a
   rebuild gives way to the identity-refresh rule.
10. **S5** (`#### S5 operations`, I19-I21): a local coordinator under `tmp/world/locality/s5/` built
    from tracked functions; preflight; marker; O1, O4, O2, the layout-receipt step, O3; the checks;
    stop conditions per D7.
11. **S6** `## Documentation plan`, plus router entries for `export_target_json.ps1`
    (`scripts/ue/build/README.md`), `record_verify_baseline.ps1` and the twin verify
    (`scripts/ue/world/README.md`).
12. **S7:** every sabotage rerun red on the final tree (I25); the A10 measurements again; both
    audits; ProjectMaterial `Validate`; independent R2; then both World todos move to `01_done/`.

### Notes for the resuming agent

- Read this todo top to bottom, then the design note for the next chunk; the I-rows govern where a
  note differs.
- Pester suites that call Python need the pinned interpreter
  `tmp/world/execution_environment/python/<hash>/Scripts/python.exe` first on `PATH` (jsonschema);
  `ExecutionEnvironment/bootstrap.py` builds it.
- The local TestData Mesh Terrain maps hold no base actors, and ProjectWorldTestData has no enrolled
  active set (capture task facts 14-15).
- Todo edits go through small asserted scripts (`tmp/world/locality_todo/edit_*.py`): exact anchors,
  one match each, ASCII, table field counts checked afterwards.

## Goal

Clean architecture and fast development (D3): a change regenerates, re-accepts, and tests only the
owners whose output it can actually change; adding or replacing a producer touches only that
producer; every accepted World artifact stays reproducible by the current code; a forgotten revision
bump on output a fixture exercises cannot reach a release, and output no fixture exercises is a
stated limit (Decision 2). A developer or agent knows which box owns a change, runs one documented
local workflow, and sees why anything became dirty. Unexpected cross-owner regeneration stays
ARCHITECTURAL RED.

## Authority register

### Operator decisions

- **D1** "so we need add all todos in priority with proper scope, but don't blow up with many
  todos" (with: use `/architect` and `/investigate-change`).
  - Effect: one current task for this brief, ordered slices inside it, the priority line; the
    fingerprint-scope prototype is absorbed; backlog tasks keep their own owners.
  - Reason: not stated.
  - Date/source: 2026-10-02, operator request.
- **D2** The operator supplied the reviewer's brief for a repository-wide generation-architecture
  locality investigation and asked for it to go through `/architect` and `/investigate-change`.
  - Effect: the brief defines the requested outcome, including independent black boxes,
    representation adapters replaceable without changing generic infrastructure, and owner-local
    declarations composed by generic code (its smell 5 and its "admission of a new producer"
    matrix row); its mechanisms are judged against evidence, not adopted verbatim.
  - Reason: not stated.
  - Date/source: 2026-10-02, operator request.
- **D3** "we need fullly done before release, no need overcomplicate - our goal clean architecture
  and fast dev".
  - Effect: closes Q2. Every slice, including S5, lands before 3.0.0 R5; the release router links
    this task. The goal is clean architecture and fast development.
  - Reason: "our goal clean architecture and fast dev".
  - Date/source: 2026-10-02, operator answer to Q2.
- **D4** "we need think here if it blocks our speed need to think how properly based on our vectors
  and best industrial - rust, linux, ue sources and etc, out of box, KISS".
  - Effect: closes Q1. Owner identity is chosen by whether it blocks development speed, judged
    against industrial practice and the engine's own mechanism, out of the box and KISS.
  - Reason: development speed.
  - Date/source: 2026-10-02, operator answer to Q1.
- **D5** "if it redundant and wrong intermidiate - ofcouser we need clenup all".
  - Effect: closes Q3. Every redundant or wrong intermediate generation path found here is removed,
    the P0/representative route included (`## Cleanup plan`).
  - Reason: not stated.
  - Date/source: 2026-10-02, operator answer to Q3.
- **D6** "bro again our goal clean architecture why you are reasking, I understand that if we don't do
  properly now without any noise we are dead".
  - Effect: the full clean end state is required now, in this task: the layer producer contract
    (Decision 4) and every other decision here stay in scope; no scope-trimming or deferral offers
    and no re-asking of settled points.
  - Reason: "if we don't do properly now without any noise we are dead".
  - Date/source: 2026-10-02, operator reply to the third-review report.
- **D7** `/implement-approved-change` with "PASS. Run it." and "if implementation will be red we
  need focus on achieve goal instead reverting, so no way back CLEAN FAST DECOUPLED ARCHITECTURE".
  - Effect: approves this todo and the capture-readiness task as one autonomous job, capture
    readiness first; red is fixed forward, not reverted; the job stops only for an architectural
    red, a recovery that cannot be completed safely, a persistent authority change outside
    `#### S5 operations`, or evidence that disproves an approved premise.
  - Reason: clean, fast, decoupled architecture.
  - Date/source: 2026-10-02, operator request.

Binding stable rules, cited and not restated: Component Ownership in
[principles.md](../../../docs/architecture/principles.md) (three identities); Impact-Driven
Development and Change Locality in World pipeline layers; the public-repo migration policy
(`CLAUDE.md`).

### Operator gates

- **Q1 [CLOSED by D4]:** Owner compiler fingerprints (ProjectTexture, ProjectMaterial, and the Mesh
  Terrain adapter's `Build.cs` list) were kept conservative on 2026-09-30 (D5 of
  `20260930-2139_content_restore_black_box_independence.md`). Narrow them here too? Narrowing the
  adapter list changes the adapter fingerprint, which re-realizes every terrain cell (pitfall 36).
- **Q2 [CLOSED by D3]:** When does S4, the one combined re-acceptance of the affected manifests in
  both data plugins, run: in the pre-candidate window before the 3.0.0 R5 freeze, or after 3.0.0
  publication?
- **Q3 [CLOSED by D5]:** Remove the retired P0/representative bounded-procedural route (code,
  profiles, End-to-End docs) instead of restoring it?

### Working assumptions

- **A1 [REJECTED by evidence]:** The only durable World change is S4's re-acceptance through the
  supported transaction; no new geography is applied and no manifest is hand-edited. - The adapter
  stamps its identity into terrain base-actor bytes (`ProjectWorldMeshTerrainProducer.cpp:668-698`),
  so changing that identity rewrites those actors once; see A7.
- **A2 [REJECTED by evidence]:** Owner compiler fingerprints and the adapter fingerprint value stay
  unchanged (Q1 default). - Under D4 the adapter's source-hash identity is the speed blocker (F13);
  Texture and Material compiler fingerprints stay unchanged (Decision 1).
- **A3 [ACTIVE]:** All identity-moving changes batch into one combined realization (S5), as the
  done black-box task's single combined refresh did.
- **A4 [RESOLVED by D5]:** Generation code whose only consumers are tests or fixtures is removed
  under the public-repo migration policy (`CLAUDE.md`); a fixture that still needs a current path
  migrates first (building v1: the twin fixture moves to v2).
- **A5 [REJECTED by evidence]:** No C++ build-locality probes: the module graph is already clean (F12); a static
  fitness test guards it. - Clean module direction says nothing about the loop's cost; the common
  checks run seven suites (`E2E/checks.py:12-20`), about six minutes per receipt. See A10.
- **A6 [REJECTED by evidence]:** S5 runs the supported realization for the affected scopes of both
  data plugins with unchanged canonical input; only identity fields change (manifest fingerprints,
  terrain base-actor identity tags) besides D5 removals. No manifest is hand-edited. - A stale
  fingerprint dirties its whole layer and the dirty closure carries that to every dependent (F14),
  so realization would rewrite every layer of both territories; terrain semantic hashes contain the
  adapter identity (F17); one data plugin, ProjectWorldData, holds both territories. See A7.
- **A7 [REJECTED by evidence]:** S5 refreshes identity without regeneration for the 12 scopes whose artifacts do
  not change, using the existing `New-ProjectWorldFingerprintMigrationCandidate`
  (`generated_manifest.ps1:446-460`) published through `Publish-ProjectWorldActiveSet`, driven by an
  uncommitted local script under `tmp/`, as pitfall 33 prescribes for a formula change. Only the two
  terrain scopes are realized. The contract is `### S5 contract`. - The receipt parser edit forces
  an owner-local ProjectMaterial refresh (F18); the presentation scope may retire (`## Cleanup
  plan`) and F9 may move road inputs, so the operation set is derived after S3. See A11.
- **A8 [REJECTED by evidence]:** A verify compares package bytes where S0 shows two fresh runs of the fixture are
  byte-identical, and a producer-computed output digest otherwise. - Producer-computed digests are
  input-shaped for terrain, and fallback field lists miss producer-written state such as map
  exposure (F17). See A12.
- **A9 [ACTIVE]:** Verify fixtures are built from tracked inputs only (the producers' existing test
  bundles and TestData profiles); generated TestData output stays ignored, and only small baseline
  JSON files become tracked.
- **A10 [ACTIVE]:** The workflow is measured, not assumed: S0 and S7 record UBT actions and wall time
  for a no-op build, one producer `.cpp` edit, one owner verify, the full verify suite, and the
  common checks (`### Developer workflow`). No benchmark system is added.
- **A11 [ACTIVE]:** S5 runs separate operations, each with its own permitted changes and evidence,
  on a scope set derived after S0 settles F9 and S3 settles the presentation scope
  (`#### S5 operations`). No manifest is hand-edited.
- **A12 [REJECTED by evidence]:** A verify observes the produced output after save and reload: package bytes where
  S0 proves two fresh runs byte-identical, otherwise an output projection of every producer-owned
  object (exported properties plus mesh, attribute, and collision digests) minus a documented
  volatile-field list. Input-derived hashes and aggregate counts never stand in for output. - A
  generic export also captures engine-owned defaults and transient state the producer never sets,
  so producer-irrelevant changes would fail verifies and train people to ignore them. See A13.
- **A13 [ACTIVE]:** A verify observes the produced output after save and reload: package bytes where
  S0 proves two fresh runs byte-identical, otherwise the producer's own projection of exactly the
  generated state it owns, built from shared mechanics (stable ordering and hashing; mesh,
  attribute, collision, and transform digests; tags; object paths; simple property values).
  Input-derived hashes and aggregate counts never stand in for output; the sabotages in
  `### Verify table` prove each projection covers what it claims.

## Non-goals

- ProjectTexture and ProjectMaterial identities: proven GREEN by rows X1-X11 of the done black-box
  task, and cheap to recompute (Decision 1).
- A revision model for SourceIngestion, CanonicalCompilation, or ExecutionEnvironment (Decision 7).
- A build system, dependency engine, cache framework, service, or dependency inference by scanning
  implementation source; any producer framework beyond the existing terrain contract's shape.
- Per-producer source lists for test selection (Decision 2, `### Developer workflow`).
- UBT timing or relink probes (A5).
- Backlog-owned work: the `UProjectWorldManifest` stub (chore L-2), object-definition host
  compatibility L001/L004 (architecture backlog), the ProjectDefinitionGenerator parsing authority.
- Capture readiness and the Visual Verification census/planner (the capture task).
- Release payload closure, City17's authored Landscape, and any change to canonical geography.

## Read first

[principles.md](../../../docs/architecture/principles.md) Component Ownership; World pipeline layers
(Impact-Driven Development, Change Locality, Proof Traceability);
`.claude/skills/world-engineering/SKILL.md`; Canonical World Realization;
[ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md); Territory
contract; [World tools](../../../tools/World/README.md);
[ProjectWorldMeshTerrain](../../../Plugins/World/ProjectWorldMeshTerrain/README.md); the done tasks
`20260930-2139_content_restore_black_box_independence.md` (D4-D13, mutation matrix X1-X11) and
`20260923-1620_world_migrate_terrain_to_mesh_terrain.md` (D3, D5).

## Verified evidence

Paths: `PWE` = `Plugins/World/ProjectWorld/Source/ProjectWorldEditor`, `GF` =
`scripts/ue/world/generator_fingerprint.ps1`, `RLO` = `scripts/ue/world/realization_layer_operation.ps1`,
`MTP` = `Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp`,
`E2E` = `tools/World/EndToEndValidation/app`. Code reading unless a command is named. Eight
read-only investigation passes; the load-bearing claims were rechecked directly.

### Owners

| Owner | Authoritative inputs | Outputs | Action identity today | Public contract | Consumers |
|---|---|---|---|---|---|
| ExecutionEnvironment | runtime pins, locks | pinned tools, geospatial helpers | `identity_sha256` incl. own code (`ExecutionEnvironment/app/identity.py:19-49`) | locks, toolchain receipt | ingestion, compilation |
| SourceIngestion | provider data, profile | accepted receipts | `run_inputs_hash` incl. `implementation_sha256` (`SourceIngestion/app/run_identity.py:39-67`) | accepted receipts | CanonicalCompilation |
| CanonicalCompilation | receipts, compile profile | canonical cells | `inputs_hash`; `compiler_contract_sha256` = algorithm version, namespace, own and EE code (`CanonicalCompilation/app/lineage.py:42-52`) | per-cell `content_hash` | World realization |
| World realization pipeline (ProjectWorldEditor + `scripts/ue/world`) | canonical bundle, realization profile | manifests, active set | dirty plan, layer contract hash | `normalized_layer_contract_sha256` | every producer, audit |
| `map:v1` | bundle, runtime and presentation profiles, overlays | map package and 14-16 external actors per map | `GF` source-file hashes | map path | runtime |
| `presentation:v1` | presentation profile | no artifacts; consumer links from both maps | `GF` source-file hashes | none consumed beyond the links | maps |
| `project_mesh_terrain:v1` (ProjectWorldMeshTerrain) | canonical final surface, material object path, MPD asset | base actors, partition, compiled sections, layout receipt | adapter `Build.cs` source hash in actor tags + `GF` | layout id/version/sha, terrain role | runtime, ProjectMaterial |
| water, road, vegetation, building v1/v2, gameplay | canonical cells, layer contract, data assets | cell actors and packages | `GF` source-file hashes + per-unit `canonical_inputs` | none consumed | runtime |
| ProjectTexture, ProjectMaterial | recipes | patterns, surfaces | `Build.cs` compiler fingerprint | output contract, object path | adapter, presentation receipt |
| ObjectDefinition | definition JSON | definition assets | `FileHash` | `DefinitionStructureHash` | gameplay placement |

### Edges

| Edge | What is consumed | Class | Verdict |
|---|---|---|---|
| SourceIngestion -> CanonicalCompilation | accepted receipts and `run_result.json` bytes | input | GREEN |
| SourceIngestion -> CanonicalCompilation | `run_inputs_hash` as freshness gate, whole-run key, promotion gate (`source.py:106`, `pipeline.py:168`, `promotion.py:102`); not in per-layer reuse (`lineage.py:151-167`) | owner-chain identity | GREEN under D4 (F6) |
| ExecutionEnvironment -> CanonicalCompilation | EE code in `compiler_contract_sha256` (`pipeline.py:162-173`, `lineage.py:46`); EE code produces output (`geospatial.py:12-40`, `toolchain.py:100-128`) | input | GREEN (F6) |
| CanonicalCompilation -> World layers | per-cell `content_hash` (`ProjectWorldCanonicalBundle.cpp:483-507`; terrain units `PWE/Private/ProjectWorldLayerInventory.cpp:273-276`) | input | GREEN |
| CanonicalCompilation -> World manifests | `compile_result_sha256` (`realize_canonical_world.ps1:788-794`) | provenance | GREEN at the writer; RISK at enrollment (F7) |
| Realization profile -> layers; dependency layer -> dependent | contract hashes (`LayerInventory.cpp:418-428`) | public contract | GREEN |
| Dependency layer dirtiness -> dependent | `*` from a stale fingerprint propagated by `DependsOn` (`ProjectWorldRealizationProfile.cpp:674-714`) | none consumed | RED (F14) |
| Producer code -> its manifests | `generator_fingerprint` from hand-kept file lists (`GF`) | action identity | RED both ways (F1, F2) |
| Adapter code -> terrain artifact bytes | `ProjectWorld.MeshTerrain.AdapterCompiler` tag on base actors | provenance in output | RED (F13) |
| Adapter receipt -> ProjectMaterial | `layout_sha256` (consumed); full receipt payload re-derived in `ProjectMaterialTerrainLayoutContract.cpp:83-110`, `:168-191` | contract; duplicate | GREEN; DUPLICATE (F3) |
| Presentation profile -> road and building meshes | road and building materials in the meshes (`ProjectWorldRoadRealization.cpp:534`, `ProjectWorldBuildingRealization.cpp:476`), absent from every unit hash, contract hash, and compared field | input | RED (F9) |
| ObjectDefinition -> gameplay units | `DefinitionStructureHash` plus `DefinitionContentHash` (`ObjectSpawnServiceImpl.cpp:56`) | input (conservative) | GREEN under D4 (F6) |
| `GF` -> planner, E2E contract inputs, adapter `Build.cs` | the same source sets, re-derived | duplicate | DUPLICATE (F3) |
| Generic code -> each producer | per-id branches in about 12 generic surfaces | open/closed | RED (F4) |
| Adapter internals -> generic audit and acceptance | private tags, generator-id literal | leak | RED (F5) |
| Producing code -> test staleness | planner and common-check roots | proof routing | RED (F15) |

### Findings

- **F1 Omitted producing code (correctness).** A stale fingerprint marks a whole layer dirty
  (`RLO:263-270`) and the audit accepts only current fingerprints
  (`audit_generated_authority.ps1:159-177`), so an omitted producing file lets the audit accept
  manifests the current code would not reproduce. Omitted today: for every layer,
  `ProjectWorldLayerInventory.cpp`, `ProjectWorldRealizationProfile.cpp` (contract hash `:446-472`,
  dirty plan `:603-729`), `ProjectWorldRealizationService.cpp` (`SaveGeneratedWorld` `:116-161`,
  `WriteResult` `:720-770`), and `RLO`'s producing functions; for the map, the service's map create,
  save, and apply; for terrain, `ProjectWorldTerrainRuntimeRole.*`,
  `ProjectWorldPresentationMaterialBinding.*`, `RLO:439-531`, and the section-selection region of
  `ProjectWorldMeshTerrainAuditCommandlet.cpp`; for water, `ProjectWorldGeneratedGeometry.*` and
  `ProjectWorldGeneratedActorLifecycle.cpp:66-72`; for vegetation,
  `ProjectWorldWaterContractParsing.*`; for gameplay, `ProjectWorldActor.*`,
  `ObjectDefinitionHostHelpers.*`, `InteractableActor.*`, `ObjectCapabilityPropertyResolver.*`; for
  water, road, and vegetation, their capture code in `LayerInventory.cpp:494-709`.
  `generator_fingerprint.Tests.ps1:31-41`, `:213-238` assert the wrong expectations.
- **F2 Non-producing code in the lists (speed).** `$shared`: `ProjectWorldEditor.Build.cs`, the
  realize commandlet, `SchemaReference`, `DataRoots`, `world_data_roots.ps1`, `operator_controls.ps1`,
  three schemas, and the lock, journal, and recovery regions of the transaction scripts. Map:
  `ProjectWorldEvidenceCapture.cpp/.h` (`GF:65-66`). Terrain: both registries and all of
  `LayerInventory` and `RealizationService` (`GF:97-99`, contradicting the `GF:24-26` comment), so
  adding any producer's branch moves terrain and, through F14, every dependent layer. Layers:
  `GeneratedActorLifecycle`, `PresentationProfile` for road and building, the test-only region of
  `GeneratedGeometry` (`:78-605`), schemas.
- **F3 Duplicate authorities.** Producer source sets in `GF`, `planning.py:28-39`,
  `contract_inputs.py:8-16`, and the adapter `Build.cs:42-58` (which also hashes raw bytes, without
  the line-ending normalization of pitfall 33, and lists `ProjectWorldTerrainRuntimeRole.*`, which
  `GF` lacks). Layer tuples and settings validated three times, already disagreeing: Water
  `surface_offset_m` must equal 0.25 in `RLO:54` but may be in (0, 1] in
  `ProjectWorldRealizationGeneratorRegistry.cpp:100-104` and the schema; gameplay `placement_source`
  has three different patterns (`RLO:161`, schema `:361`,
  `ProjectWorldRealizationGeneratorRegistry.cpp:223-224`). Mesh Terrain receipt payload built by the
  adapter and re-derived by ProjectMaterial (`ProjectMaterialTerrainLayoutContract.cpp:168-191`),
  although ProjectMaterial's recipe binds only `layout_sha256`
  (`ProjectMaterialSurfaceRecipe.cpp:453`).
- **F4 Open/closed.** Only terrain has a producer contract (`ProjectWorldTerrainProducerRegistry.h:11-38`:
  id, version, tuple, `ValidateSettings`, `Apply`, `Delete`, `CaptureArtifacts`, registered by the
  adapter module). The other five producers are wired by hand into about 12 generic surfaces:
  admission (`ProjectWorldRealizationGeneratorRegistry.cpp:232-247`), validation dispatch (`:249-282`),
  selector allowlist (`LayerInventory.cpp:245-247`), unit input hashing (`:252-298`), capture and the
  mandatory-water check (`:444-726`), the apply chain (`ProjectWorldRealizationService.cpp:535-581`)
  and terrain lookup by layer id (`:472-476`, `:621-626`), stale-actor reconciliation
  (`ProjectWorldGeneratedActorLifecycle.cpp:66-93`), the static partition audit tag list
  (`ProjectWorldStaticPartitionAudit.cpp:29-36`, `:128-136`), fixed result counters
  (`ProjectWorldRealizationService.h:125-158`, `.cpp:860-909`), the schema (`:59-369`, `:395-404`),
  `RLO:19-168` and `:439-531`, and `GF:57-181`.
- **F5 Representation leaks.** `ProjectWorldStaticPartitionAudit.cpp:26-28` hard-codes the adapter's
  private tags and requires one partition (live via `audit_runtime_partition.ps1:65`).
  `ProjectWorldProductTerrainAcceptance.cpp:474`, `:538` (runtime module) branch on
  `"project_mesh_terrain:v1"`; any other id passes its navigation checks: fail open.
  `ProjectWorldRealizationProfileTests.cpp:72`, `:116-118` pass only with the adapter loaded.
- **F6 Upstream identities (refuted as speed blockers).** SourceIngestion's implementation digest is
  its own rerun key and CanonicalCompilation's freshness gate, whole-run key, and promotion gate, but
  not part of per-layer reuse. CanonicalCompilation's own digest and ExecutionEnvironment's are in
  `compiler_contract_sha256`; ExecutionEnvironment code produces output. Cost of a code edit: Kazan
  ingestion re-decode 150-200 s without re-download (`acquisition.py:68-69`), full compile 108 s
  versus 45 s incremental (`Saved/Validation/WorldPipeline/run-20260828T101909Z/result.json`).
  Nothing downstream moves: World units key on per-cell `content_hash`, the active canonical
  authority is not checked against code identity (`promotion.py:243-256`), and the matrix tolerates
  provenance drift (`E2E/validation.py:110-125`). Gameplay keys on `DefinitionStructureHash` plus a
  line-ending-normalized content MD5 (`DefinitionGeneratorSubsystem.cpp:424-432`, `:710-727`); the
  structure hash alone misses capability data (`:729-778`), and both values are stamped into spawned
  actors (`ObjectSpawnUtility.cpp:507-513`). A whitespace edit re-places only that definition's
  objects.
- **F7 Acceptance contradiction (UNVERIFIED at runtime).** The writer carries manifests forward on a
  provenance-only compile change (`generated_layer_manifest.Tests.ps1:534-552`), keeping the old
  `compile_result_sha256`, but `verify_manifest_provenance` compares it per scope
  (`E2E/enrollment.py:157`, `E2E/acceptance.py:175-180`, `:335-341`); layer scopes record
  `runtime_profile_sha256` `none` (`RLO:13-15`) while maps carry the real hash. The last accepted
  enrollment predates commit `d29ce7297` that introduced `none`.
- **F8 Dead or wrong intermediate paths.** `## Cleanup plan`.
- **F9 Road and building material identity (RED, verified in R1).** Both materials are bound from
  the presentation profile (`ProjectWorldRealizationService.cpp:553-554`, `:567-569`) and appear in
  no unit hash (`ProjectWorldRoadRealization.cpp:381-421`, `ProjectWorldBuildingRealization.cpp:266-330`),
  no layer contract hash (`ProjectWorldRealizationProfile.cpp:452-466`), and no field `RLO` compares
  (`:256-330`); every active scope records the current `presentation_profile_sha256`. Fix: I22.
- **F10 The gap is latent.** Since the last manifest acceptance (`a23a2a39d`, 2026-09-30) no
  commit touches an F1 file except one additive `ProjectSha256` helper (+15/-0).
- **F11 Backlog cross-check.** The chore task's G-1 instruction to add a moved `RealizationService`
  file to `$catalogPaths` encodes F1's wrong premise; S6 corrects it. L001 sits in the gameplay set.
- **F12 Module direction is clean.** No generic World module depends on MeshPartition, the adapter,
  Water, or PCG; `ProjectWorldEditor.Build.cs` lists unused `Landscape` and `Foliage`;
  `ProjectPCG` is an empty module with an unused PCG dependency.
- **F13 Source-hash identity blocks speed.** Any edit to a listed file, including a comment or a
  refactor with identical output, stales every manifest of that producer in both territories; the
  next realization dirties the whole layer (`RLO:263-270`) and the pre-package audit fails until
  re-acceptance. The adapter also writes its source hash into every terrain base actor
  (`MTP:668-698`), so each adapter edit rewrites all of them. Completing the lists (F1) would add
  files to every layer producer and make this slower.
- **F14 Dirty closure has no cutoff for producer identity.** A stale fingerprint adds `*` for its
  layer (`RLO:263-270`), and `BuildDirtyPlan` copies a dependency's dirty set to every dependent
  (`ProjectWorldRealizationProfile.cpp:674-714`). Dependents consume canonical cells and the
  dependency's contract hash only (`LayerInventory.cpp:418-428`; gameplay touches only its own tagged
  actors, `ProjectWorldGameplayPlacement.cpp:334`, `:460-480`). In Kazan, water, roads, vegetation,
  buildings, and gameplay depend on terrain (`kazan_territory_v1.realization.json:44-137`), so any
  terrain identity change re-realizes every layer of both territories.
- **F15 Proof routing misses producing code.** `plan` selects no gate for the adapter, its MPD,
  ProjectObject spawning, ProjectMaterial's surface manifest, or an unknown path (read-only
  `plan_for_paths` run; `planning.py:28-39` returns `False` by default). The common-check receipt
  that R5's World L4 proof requires (`E2E/acceptance.py:153-162`) goes stale only on
  `contract_inputs.py:8-16` roots, which exclude the adapter plugin and cross-plugin producing code,
  so a receipt older than an adapter edit is still accepted. No hook or CI runs `plan`.
- **F16 No tracked regenerate-and-compare baseline.** TestData generated content and manifests are
  ignored (`.gitignore:143-146`; TestData `README.md:13-16`). Existing digest checks compare within
  one run (`scripts/ue/world/test/integration/realization_layer_lifecycle.ps1:572-574`). The only
  TestData realization profile, `synthetic_landscape_water_twin`, covers terrain, water, road,
  building v1, and gameplay; vegetation and building v2 are exercised only by in-code UE tests
  (`ProjectWorldVegetationRealizationTests.cpp:28-32`, `ProjectWorldBuildingMassingTests.cpp:89-97`).
- **F17 Terrain semantic identity is input-shaped.** Base-actor `semantic_sha256` hashes cell,
  input, material path, and adapter identity (`MTP:850-853`); compiled sections hash their package
  name (`MTP:879-882`). A geometry change at the same identity is invisible to them. Vegetation's
  semantic hash covers instance ids, mesh index, and transforms
  (`ProjectWorldVegetationPlacement.cpp:367-381`); whether water, road, building, and gameplay
  semantic hashes are output-shaped is UNVERIFIED. The Mesh Terrain audit commandlet reads
  compiled-section vertex positions and collision counts but emits no digest
  (`ProjectWorldMeshTerrainAuditCommandlet.cpp:242-254`, `:377-457`). Field-list fallbacks miss
  producer-written state: the map writes post-process exposure
  (`ProjectWorldPresentationRealization.cpp:67-78`), which class, name, and transform comparisons
  cannot see; the adapter's transformer only tags compiled sections
  (`ProjectWorldMeshTerrainTransformer.cpp:6-14`), so compiled output can change while every base
  mesh stays equal, and section counts or height deviation cannot see topology, attribute, or
  collision changes.
- **F18 ProjectMaterial identity reads all its editor code.** `ProjectMaterialEditor.Build.cs:39-50`
  fingerprints every non-test editor `.cpp` and `.h`; that fingerprint enters each surface's
  semantic identity (`ProjectMaterialSurfaceRecipe.cpp:454-470`), and `Validate` rejects stale
  surfaces (`ProjectMaterialSurfaceGenerationService.cpp:280`, `:511`, `:571`). The receipt parser
  edit of Decision 3 therefore needs one owner-local Material refresh. Its public paths and
  `layout_sha256` stay, and no World field reads material bytes (done black-box task, edge W2).
  Material generation reads the layout receipt passed to it (`run_material_generation.ps1:18`,
  `:322-327`), so the refresh runs after terrain writes its new receipt.
- **F19 Proof inputs beyond module sources.** The common-check roots already cover World tools,
  World, package, check, and test scripts, ProjectWorld sources and schemas, TestData data and
  authored content, `.uplugin` files, the project file, and two config files
  (`contract_inputs.py:8-23`). They do not cover adapter sources, ProjectObject, or anything under
  `Data/Producers/`. `Build.cs` files are executable C# rules; UBT's `-Mode=JsonExport` writes each
  module's resolved public and private dependencies (`Engine/Source/Programs/UnrealBuildTool/Modes/JsonExportMode.cs`,
  `Configuration/UEBuildModule.cs:1553-1555`). World plugin `Data/` never ships: no World `Build.cs`
  declares `RuntimeDependencies`, which the staging audit requires (`package_release.ps1:258-262`).
- **F20 Producers are not physical boundaries.** `ProjectWorld.uplugin` has two modules,
  `ProjectWorld` and `ProjectWorldEditor`. All five built-in layer producers live in
  `ProjectWorldEditor/Private`: Water 6 files and 1,182 lines, Road 2 and 658, Vegetation 5 and
  1,216, Building 6 and 1,593, Gameplay 2 and 655. Each includes only shared pipeline headers
  (`ProjectWorldCanonicalBundle.h`, `ProjectWorldGeneratedGeometry.h`,
  `ProjectWorldRealizationProfile.h`, `ProjectWorldRealizationService.h`, and for some
  `ProjectWorldAuthoredOverlay.h`, `ProjectWorldDataRoots.h`, `ProjectWorldSchemaReference.h`),
  never another producer's. Vegetation's only cross-producer coupling is data: it reads the road
  and water layer settings of its declared dependencies (`ProjectWorldVegetationExclusions.cpp:225-237`).
  With every producer in one module, a path cannot name its producer without a list, and every
  producer edit relinks the shared editor module.
- **F21 One lock, one delegation path, one non-participant.** Every persistent-content generator
  serializes on one OS-held lock, `Enter-ProjectGeneratedContentMutationLock`
  (`generated_content_mutation_lock.ps1:5-82`). A parent may hold it and let a child enter with a
  verified delegated token: the child must present the live owner's token while the lock is held,
  and a wrong token or a token with no live owner fails closed (`:22-64`). World passes the
  variable `ALIS_WORLD_CONTENT_LOCK_TOKEN` (`generated_manifest.ps1:60-67`), and its Pester suite
  covers delegation, a wrong token, refusal while held, and a token without an owner
  (`generated_manifest.Tests.ps1:751-790`). The ProjectMaterial host acquires the lock with no
  delegation variable (`run_material_generation.ps1:269-271`), so under a parent-held lock it is
  refused. The old variable name appears in eight files: `generated_manifest.ps1`,
  `generated_manifest.Tests.ps1`, `authored_overlay_persistence.ps1`,
  `realization_layer_lifecycle.ps1`, `runtime_profile_locality.ps1`,
  `run_kazan_runtime_profile_tournament.ps1`, `tools/World/EndToEndValidation/app/execution.py:33`,
  and `scripts/git/mirror/tests/test_developer_payload.py:303`. The lock is an open file handle
  (`generated_content_mutation_lock.ps1:66-81`), so the OS releases it when its process exits. No
  durable state spans both owners: World refuses Apply while its own journal exists and only
  `recover_generated_transaction.ps1` resolves it (`realize_canonical_world.ps1:364-367`), but that
  journal is one per manifest root and O2's own transaction needs it; ProjectMaterial restores only
  its own journal at its next run (`run_material_generation.ps1:280-284`, `:437-447`).
  Recovery integrity:
  - `cleanup_workspace.ps1 -Apply` deletes `tmp/world/world_realization/transactions` and the lock
    file (`:104-111`) after checking only Unreal processes and journals under `Plugins/World`
    (`:193-208`); it never takes the lock, so it could delete a snapshot an unresolved recovery
    needs.
  - Every other deletion route removes only its own operation's paths: World transactions their
    journal's snapshot (`generated_manifest.ps1:836`, `:872`), the realization wrapper its own
    result and superseded artifacts (`realize_canonical_world.ps1:292`, `:755-781`), the Material
    host its own operation root and only when no journal remains (`run_material_generation.ps1:459-475`),
    and End-to-End its own confined scopes and maps (`execution.py:245-298`).
  - The Material host moves its pre-change snapshot to `Saved/Validation/<folder>/RollbackPrevious`
    only after a successful Regenerate, deletes it on the next one (`run_material_generation.ps1:400-416`),
    and has no route that restores from it (modes `Validate` and `Regenerate`, `:11`).
  - A delegated child holds its read handle, opened with read-write sharing, until it exits
    (`generated_content_mutation_lock.ps1:25-29`, `:63`), so a later normal acquisition can coexist
    with an orphaned child; an open with no sharing cannot. A commandlet grandchild holds no handle;
    the Material host already refuses while any Unreal process for this project runs
    (`Assert-NoSameProjectEditor`, `run_material_generation.ps1:82-93`).
- **F22 A guard test drove a real realization (found in implementation, fixed).** The C8 case
  "ALLOWS durable enrollment when the operator authorized that exact operation" ran the realize
  wrapper on the durable ProjectWorldData manifest root with real profiles, so it took the real lock,
  snapshotted and journaled real content, and launched the realize commandlet; on 2026-10-02 it was
  stopped only by the commandlet rejecting the fake compile fixture (`contract-field $schema`), and
  the audit afterwards showed authority intact. The guard is now a pure function with direct tests,
  and the four other "ALLOWS" wrapper runs, which failed on placeholder profiles before reaching the
  guard, are replaced by those tests.

Baseline commands (read-only): `audit_generated_authority.ps1 -WorldDataPlugin ProjectWorldData`
accepted, `generator_fingerprint_current` OK, 14 scopes (Kazan: map, 6 layers; Manhattan: map, 5
layers; one presentation scope), 3096 artifacts; the existing fingerprint Pester suite passes 11/11
(it checks list membership with placeholder files).

### Q1 analysis (D4)

| Practice | Producer key | Propagation | Safety net |
|---|---|---|---|
| Cargo (Rust) | compiler identity, profile, features, source hashes | dependents rebuild on fingerprint change; rustc incremental cuts off on unchanged query results | none needed beyond determinism |
| Bazel | digests of declared inputs and tools | early cutoff: identical output digests stop downstream work | remote cache, reproducibility checks |
| Linux Kbuild | exact headers and per-`CONFIG` symbols each object used (compiler dep files, `fixdep`) | per object | none |
| UE derived data (installed 5.8) | function name + version + input hashes; "Functions have a version which is used as a proxy for their code. Any code changes that affect the behavior of a function must have a corresponding change in the function version." (`DerivedDataBuildFunction.h:31-33`, `:48-49`) | content-addressed values | `-VerifyDDC` / `-DDC-Verify` regenerate and compare (`DerivedDataCache.cpp:224-225`, `:389`; `DerivedDataBackends.cpp:128-136`) |

Cargo and Bazel can key on source because rebuilding a crate or action is cheap and automatic. Kbuild
is precise because the compiler reports what it read; ALIS producers share one editor module, so no
tool can report a producer's read set. ALIS World producers are expensive, durable, and operator
accepted, which is UE's derived-data situation, with one difference: a DDC value is disposable,
while ALIS generated authority is durable and release-gated. ALIS therefore takes UE's version plus
input hashes and makes the regenerate-and-compare verify mandatory before release (Decision 2).
ProjectTexture, ProjectMaterial, and the upstream tools recompute in seconds to minutes with a
content cutoff after them (F6), so their conservative identities do not block speed.

## Current architecture and source of truth

Producer identity is a hand-maintained central switch in `GF`: `$shared` plus one file list per
producer, with `PROJECTWORLD_PRODUCER_BEGIN/END` regions for shared files. Three components copy
those sets. The adapter has its own `Build.cs` source hash, written into terrain base-actor tags.
Only terrain is reached through a producer contract; the other producers are branches in generic
files, validated three times. The dirty closure propagates any dirtiness along `depends_on`. Proof
routing (`plan`, the common-check staleness roots) knows only the ProjectWorld plugin and scripts.

## Problem and root cause

Producer identity hashes implementation source files as a proxy for output. File lists cannot be
both complete and local while generic files hold per-producer code, so they drifted both ways (F1,
F2), each copy drifted differently (F3), and even a perfect list would invalidate whole territories
on refactors and comments (F13), amplified by a dirty closure without cutoff (F14). The per-producer
code sits in generic files because only terrain has a contract (F4). Nothing regenerates and
compares output against a stored baseline (F16), and proof routing cannot see code outside
ProjectWorld (F15).

## Decision

1. **World producer identity follows UE's derived-data model.** A producer's fingerprint is its id,
   its declared output revision, the shared pipeline revision, the digests of its declared data
   inputs (MPD, tree meshes), and the engine build identity (`GF:239-263`). It hashes no source
   file. An output change bumps the revision in the same change; a refactor, log, or comment changes
   nothing. `GF`'s file lists and region markers and the adapter's `Build.cs` fingerprint go.
   ProjectTexture and ProjectMaterial keep their fingerprints (D4).
2. **Mandatory verify net.** Every surviving producer and the shared pipeline have one verify,
   `Project.World.Realization.Verify.<Producer>`, which realizes its fixture into a transient package
   root, saves, reloads, and compares the output per A13 with a tracked baseline
   (`### Verify table`). Each row's sabotages prove the verify executes the changed producer rather
   than an existing artifact or a cached result. The limit is stated, not hidden: output no fixture
   exercises is not covered, as with UE's `-VerifyDDC`.
   - **Verification identity**, stored in the baseline: producer id and output revision, pipeline
     revision, fixture input digests (tracked fixture files, plus the canonical `content_hash` per
     cell for fixtures compiled at test time), engine identity, and the comparison-contract
     version. Production identity (Decision 1) never reads baselines, fixtures, or the comparison
     contract.
   - **Baseline rules:**

     | Situation | Behaviour |
     |---|---|
     | Same verification identity, different output | fail; record mode refuses to overwrite |
     | Fixture or comparison contract changed | re-record that baseline in a reviewed diff; no production revision bump |
     | Intended producer-output change | bump the producer revision and re-record in the same diff |
     | Ordinary runs, common checks | read-only toward tracked baselines |

     The failure message names all three causes: a fixture or comparison change (bump its version
     and re-record), a producer output change (bump the revision and re-record), nondeterminism (fix
     it). Record mode is a separate script entry; no gate passes it.
   - **Enforcement reuses existing machinery.** The verifies sit under `Project.World.Realization`,
     which the common checks' `unreal_realization` suite runs (`E2E/checks.py`), and R5's World L4
     proof requires a current common-check receipt (`E2E/acceptance.py:153-162`; release router R5
     step 2).
   - **Proof-input set** in its existing owner, `contract_inputs.py`: the current roots stay (F19),
     plus the resolved first-party dependency closure of every producing module from UBT
     `-Mode=JsonExport` run through a project script, every descriptor under `Data/Producers/`
     and every baseline under `Data/TestFixtures/Verify/`, every declared data input, and the verifier and its fixtures. The fitness
     check fails closed when the export is missing or older than any `Build.cs`, when a producing
     module is absent from it, or when the closure reaches a first-party module outside the set; it
     never parses `Build.cs` text. Compilation, generation identity, and proof keep separate keys.
   - The authority audit still fails closed on a stale revision, data input, or engine identity.
3. **One owner per value.** Each producer owns one descriptor in its owning plugin,
   `Data/Producers/<producer>.json`: id, version, output revision, data inputs, and the modules
   that implement it. The shared pipeline
   revision is `Plugins/World/ProjectWorld/Data/Producers/realization_pipeline.json`. `GF` discovers
   descriptors under `Plugins/*/*/Data/Producers/` and is the only code that computes a fingerprint.
   C++ holds no revision: the realization request passes each layer's fingerprint, and the adapter
   stamps and compares that value in its reuse tag, which replaces the `AdapterCompiler` and
   `Engine` tags (the fingerprint covers both). The producer registry is the single validator of
   tuples and settings; the profile schema keeps the layer envelope, `RLO`'s copy goes, and a test
   validates every tracked realization profile through the registry. ProjectMaterial validates only
   the layout ABI it consumes and records the receipt digest as provenance; that edit moves its
   conservative compiler fingerprint (F18), so S5 includes one owner-local Material refresh. The
   planner and E2E contract inputs stop re-deriving source sets. The shared mutation lock owns its
   one delegation variable, `PROJECT_GENERATED_CONTENT_LOCK_TOKEN`; the World wrapper and the
   ProjectMaterial host both pass it, and every reference to `ALIS_WORLD_CONTENT_LOCK_TOKEN` in the
   eight files of F21 is renamed in the same change, with no alias. The same lock owner,
   `scripts/ue/generated_content/`, owns the outer-recovery marker, a small journal beside its lock
   file in the location the lock and World snapshots already use (F21):
   - written atomically; it holds the operation id, its phase, the confined path of the World outer
     snapshot, the Material state to restore (O3's own transaction while it runs, then the
     `RollbackPrevious` bundle whose `replaced_by_operation_id` equals O3's operation id), and the
     pre-S5 identities (ProjectWorldData active-set hash and audit result, ProjectMaterial manifest
     hash and `Validate` result);
   - while it exists, every non-delegated acquisition fails closed; a malformed marker, or one
     naming a path outside its confined roots, fails closed too;
   - its recovery entry opens the lock with no sharing, so it is refused while any participant,
     including an orphaned delegated child, still holds a handle, and it reuses the existing check
     that refuses while any Unreal process for this project runs; no PID is recorded;
   - recovery runs World's own `recover_generated_transaction.ps1` if O2 left a journal, restores
     the World outer snapshot, restores Material through its own journal or through a new
     owner-local `RestorePrevious` mode of the Material host, which today retains that bundle
     without any route to restore it, and removes the marker only after both owners match their
     pre-S5 identities;
   - `cleanup_workspace.ps1 -Apply` takes the lock before deleting anything, so a live S5 or an
     unresolved marker refuses it, and no longer deletes the lock file, which is a permanent
     coordination endpoint.
4. **One layer producer contract (F4).** The terrain contract becomes the contract for every layer
   producer, extended only by what the current producers use: layer kind, dirty granularity,
   permitted dependencies, unit input hashing with an optional dependency unit mapping, one apply
   context (world, bundle, profile, layer, overlays, final dirty units, presentation materials), a
   default `Delete` (the generic owned-tag sweep), artifact capture, owned-actor identification for
   lifecycle and the static audit, a per-layer metrics map, and a declared post-apply builder that
   `RLO` runs generically (terrain's MeshPartition builder). Generic code iterates the registry in topological
   order. Each built-in layer producer moves into its own Editor module in the ProjectWorld plugin,
   `ProjectWorldWaterEditor`, `ProjectWorldRoadEditor`, `ProjectWorldVegetationEditor`,
   `ProjectWorldBuildingEditor`, and `ProjectWorldGameplayEditor`, as the adapter already is (F20).
   Each leaf depends on `ProjectWorldEditor`'s public contract plus its own UBT needs, owns its
   implementation, registration and unregistration in its module startup and shutdown, descriptor,
   baseline, and focused tests, and never depends on another producer's module. Vegetation keeps
   reading road and water layer settings as declared dependency data. `ProjectWorldEditor` keeps
   generic orchestration, the map and presentation scopes, and the public headers the leaves use.
5. **Generic code names no adapter internals.** The partition audit takes owned tags from registered
   producers and terrain ownership from the neutral role; product terrain acceptance rejects unknown
   generator ids and reads producer expectations from data.
6. **Dependents are dirtied only by what they consume.** A change to a producer's identity (its
   revision, the formula, its engine identity) dirties that producer's layer; `RLO` passes such
   layers separately from computed and operator units, and `BuildDirtyPlan` adds them after the
   closure. A dependent is dirtied by the inputs and contracts it consumes: canonical units with
   their halo, dependency contract hashes, and any dependency output it declares in its unit input
   identity. No current dependent consumes another layer's output (F14); one that does must
   declare it, and then that producer's output change dirties it.
7. **Upstream identities stay (F6).** SourceIngestion, CanonicalCompilation, and
   ExecutionEnvironment keep their implementation digests as their owners' action identity, which
   Component Ownership allows; they cost seconds to minutes, and the cutoff at per-cell
   `content_hash` already stops World work. `algorithm_version` stays a profile semantic and is not
   promoted to a code revision. Gameplay keeps its definition key. Only F7 remains upstream.
8. **Provenance-only changes refresh provenance** (F7): enrollment and acceptance compare
   `compile_result_sha256` as provenance, refreshed without regeneration.
9. **Clean up all dead and wrong intermediate paths** (D5, `## Cleanup plan`).

### Verify table

Each verify realizes into a transient package root, saves, reloads, and compares with
`Data/TestFixtures/Verify/<producer>.verify.json` in the owning plugin (A13, Decision 2), kept
apart from `Data/Producers/` so production identity cannot discover it. Every sabotage must fail; a
refactor with unchanged baselines must pass.

| Producer | Fixture input (tracked) | Owned output in its projection | Sabotages that must fail | On a revision bump |
|---|---|---|---|---|
| `map:v1` | TestData twin runtime, presentation, and overlay profiles | generated actor and component identities and transforms, partition and navigation settings, lighting, post-process exposure | change one runtime-partition value; change the fixed exposure the presentation code writes | re-record; both maps; map L1 per territory |
| `presentation:v1` | presentation profile | none: zero artifacts | - | retired in S3 if nothing reads it beyond the maps' links; otherwise its manifest projection with a dropped-link sabotage |
| `project_mesh_terrain:v1` | the adapter's test cells and the twin terrain cells | base meshes (positions, triangles, both weight layers), partition, layout receipt, and compiled sections from the MeshPartition builder (render geometry and attributes, collision geometry, tags, section set) | offset one base-mesh height; change compiled output with base meshes and the verification identity unchanged (drop the role tag in the transformer; have the transformer strip a section's collision or offset a section mesh component) | re-record; terrain scopes only (Decision 6) |
| `project_water_mesh:v1` | `PersistentWater` bundle on the twin profile (`ProjectWorldWaterRealizationTests.cpp:34-85`) | water actors, meshes, materials | change how `surface_offset_m` is applied | re-record; water scopes |
| `project_road_mesh:v1` | road bundles of `CrossCellRoadIdentity` and `Geometry.RoadTerrainDrape` | road actors, meshes, bound material (F9) | change the drape offset; bind another material | re-record; road scopes |
| `project_vegetation_instances:v1` | vegetation test layers (`ProjectWorldVegetationRealizationTests.cpp:28-32`) | instanced foliage actors and instance data | jitter one instance | re-record; vegetation scopes |
| `project_building_massing:v2` | building bundles (`ProjectWorldBuildingMassingTests.cpp:89-97`, `:314`) and the twin moved to v2 | building actors, meshes, bound material (F9) | change one massing height rule; bind another material | re-record; building scopes |
| `project_gameplay_placement:v1` | `MakeBundle()` (`ProjectWorldGameplayPlacementTests.cpp:78`) | placement actors and their host state | change placement yaw handling | re-record; gameplay scopes |
| realization pipeline | the twin profile through the realization service | every layer's output through the shared save, inventory, and capture path | change a shared save policy | re-record all; every scope stale |

Compiled terrain sections need the WorldPartition MeshPartition builder, so the terrain and pipeline
rows run as a script verify on the twin when S0 shows they cannot run in process (check
`Project.World.Realization.CommandletBoundary`); that script joins the common checks and reads the
same baselines.

### Developer workflow

The documented inner loop (A10). Acceptance is structural; wall time is recorded as diagnostic.

| Action | Normal loop | Never |
|---|---|---|
| Edit one producer's implementation | build compiles only its leaf module; `plan` selects its verify; its focused tests | other producers' modules or verifies, generated writes, stale scopes |
| Edit shared generic World code, including the map | build compiles `ProjectWorldEditor` and the leaves that include a changed public header; `plan` selects every verify and says why | production writes during iteration |
| Change a fixture, diagnostic, or comparison | its focused checks; re-record per Decision 2 | production identity change |
| Change one canonical unit | that unit and its dependency closure | whole layers |
| Add a producer | its leaf module, descriptor, registration, profile, tests, baseline, and the plugin's module entry | any generic dispatch edit |
| Finish a coherent slice | the common checks once; reuse the receipt while its proof inputs are current | common checks after every edit |

`plan` maps a path under a module that exactly one descriptor names to that producer's verify; any
other path in the proof-input set selects every verify. For each selection it prints the path and
the rule that selected it. It needs no per-producer list.

Target daily experience, the bar for S7:

```text
Water code changed.
  1 module rebuilt (ProjectWorldWaterEditor).
  1 producer verify passed (project_water_mesh:v1).
  0 generated scopes stale.
  Nothing else required.

Generic realization code changed.
  ProjectWorldEditor rebuilt; leaves rebuilt only for the changed public header.
  Every producer verify ran: shared output path changed (plan rule: generic module).
  0 production writes.
```

### Premise / KISS gate

The realization pipeline keeps ownership of identity and the audit stays as it is: the manifest
field and the fail-closed check are unchanged, only the formula changes. The verify net reuses the
common checks and R5's L4 proof. The producer contract is the existing terrain contract, extended.
Added: one small descriptor and one baseline per producer, one verify per producer, shared
projection mechanics for the verifies, five leaf producer modules, one proof-input fitness check,
and one owner-local Material refresh. Removed: the file lists and region parser in `GF`, the adapter's compiler fingerprint
and its 15-file list, three source-set copies, two validator copies, the receipt re-derivation in
ProjectMaterial, about 12 generic per-producer branch sites, and the dead routes. Given up: detection
of an output change that no fixture exercises, as in UE; the verify table and its sabotages bound
what is covered.

### Alternatives considered

- **Correct and complete the file lists, owner-declared:** fixes F1 and F3 but keeps F13 and makes
  it worse.
- **Byte-identical early cutoff on source-hash keys (Bazel style):** still re-runs whole layers on
  both territories after every edit; terrain cannot cut off while its hash is in the bytes.
- **Automatic read-set discovery (Kbuild style):** no tool reports a producer's read set inside one
  editor module.
- **C++ reads the descriptor for the adapter's revision:** the fingerprint also needs the pipeline
  revision, data digests, and engine identity, so C++ would re-implement `GF`'s formula.
- **Revision constants in C++ parsed by PowerShell, or code generation:** two languages or a
  generator for one integer.
- **Explicit revisions upstream (reviewer):** F6 shows no speed gain and a new forgotten-bump risk.
- **Per-producer path lists in `plan`:** another source list; leaf modules give the same precision
  with nothing to maintain.
- **Built-in producers as subdirectories of `ProjectWorldEditor`:** no physical boundary, a list for
  `plan`, and registration from generic startup.
- **A generic snapshot of every exported property:** captures engine-owned and transient fields,
  churns baselines, and gives failures no meaning (A12).
- **Gate on S7 wall time not exceeding S0's:** file cache, DDC, background load, and link caches
  make one sample noisy; structural counts are exact.
- **Parsing `Build.cs` text for the dependency closure:** C# rules can build dependencies in ways a
  parser reads as none; UBT's resolved export cannot.
- **Skip the Material refresh by keeping `adapter_compiler_sha256` in the receipt:** leaves a field
  that no longer means what it says and the receipt duplicate in ProjectMaterial.
- **Narrow Texture and Material fingerprints too:** no speed gain worth the churn (D4).

## Required invariants

1. A World producer's fingerprint depends only on its id, its revision, the pipeline revision, its
   declared data-input digests, and the engine build identity; no source-file edit moves it.
2. Each value has one owner: descriptors own id, version, revision, and data inputs; `GF` alone
   computes fingerprints; the registry alone validates tuples and settings; no C++ revision
   constant; nothing re-derives source sets.
3. Every surviving producer and the pipeline have a verify that observes saved and reloaded output
   (A13), fails under every sabotage in `### Verify table`, and passes on a refactor.
4. The verify net is mandatory: it runs in the common checks that R5's L4 proof requires, and the
   proof-input set covers the existing roots, the producing modules' resolved first-party
   dependency closure, descriptors, baselines, data inputs, and the verifier, guarded by a
   fail-closed fitness check.
5. Adding a producer adds its own leaf module (or plugin), descriptor, baseline, profiles, tests,
   and the plugin's module entry, and registers from its own module startup; no generic surface
   listed in F4 changes. A test-only producer registered inside a test proves the host boundary.
6. Generic ProjectWorld code and modules name no adapter tag, class, module, CVar, or generator id
   outside the registry contract; unknown producers fail closed.
7. A producer identity change dirties only its own layer; a dependent is dirtied only by inputs and
   contracts it consumes.
8. No artifact byte carries provenance; the terrain reuse tag carries the producer fingerprint.
9. Provenance-only changes refresh provenance and never invalidate production outputs.
10. No dead generation path, compatibility branch, or test-only producer remains in production code
    (D5).
11. S5 changes exactly what `#### S5 operations` allows.
12. Baselines follow Decision 2's rules: production identity never reads them, ordinary runs never
    write them, and record mode refuses an unchanged verification identity.
13. The `### Developer workflow` loops hold structurally: a producer's private `.cpp` edit compiles
    only its leaf module and selects only its verify, with zero generated writes and zero stale
    scopes; a no-op build runs no compile actions; the common checks run once per slice. Wall time
    is recorded and diagnostic.
14. S5 is one concurrency envelope: one live lock from before the snapshots to the release, every
    World and ProjectMaterial step under its delegated token, no unrelated persistent-content
    generator in between, and release only after full success or successful restoration of both.
    A durable marker covers what the lock cannot outlive: while it exists no persistent mutation
    or cleanup starts, whether or not any process holds the lock, and recovery waits until no
    participant holds a handle and no project Unreal process runs.
15. Before S5 changes any authority, the territory acceptance route accepts the S0-S4 change on
    today's authority: one current common-check receipt and one `kazan_territory_v1`
    canonical-authority Matrix on it, which exercises result v2 and the producer contract
    against real output. The common Check covers the synthetic twin. This World-only slice
    does not require historical raw-source replay; the Matrix receipt must say so.

## Mutation matrix

| # | Mutation | Owner recomputes | Consumers invalidated | Scopes changed | Gate | Today | Target |
|---|---|---|---|---|---|---|---|
| M1 | docs-only | none | none | none | doc checks | GREEN | GREEN |
| M2 | test-only | none | none | none | that test | RED | GREEN |
| M3 | evidence or diagnostic | none | none | none | focused | RED | GREEN |
| M4 | validation-only | none | none | none | validation tests | RED | GREEN |
| M5 | admit a new producer | new producer | none | new scopes | its verify; no generic edit | RED (F4) | GREEN |
| M6 | producer refactor, same output | none | none | none | verifies pass | RED (F13) | GREEN |
| M7 | producer output change | that producer (revision bump) | none | its scopes | its verify re-recorded, targeted L2 | partial (F1) | GREEN |
| M8 | provenance only | none | none | provenance refresh | E2E | RISK (F7) | GREEN |
| M9 | public contract (material path, layer contract) | owner | declared consumers | consumer scopes | consumer L1 | GREEN | GREEN |
| M10 | one canonical cell | affected units | dependent units | those units | L1 | GREEN | GREEN |
| M11 | one layer's profile settings | that layer, dependents | dependents | those scopes | L1 | GREEN | GREEN |
| M12 | runtime profile only | map | none | map | L1 | GREEN | GREEN |
| M13 | adapter refactor, same output | none | none | none | verifies pass | RED (F13) | GREEN |
| M14 | engine build identity | every producer | all | all | baselines re-recorded, L2/L3 | GREEN | GREEN |
| M15 | shared pipeline output change | every producer (pipeline revision) | none beyond | all scopes | pipeline verify | MISSING (F1) | GREEN |
| M16 | manifest tooling, non-writer | none | none | none | Pester | RED | GREEN |
| M17 | `ProjectWorldEditor.Build.cs` edit | none | none | none | build | RED | GREEN |
| M18 | definition JSON whitespace | that definition's placements | none | those units | gameplay L1 | conservative | unchanged (F6) |
| M19 | SourceIngestion implementation only | ingestion, compilation (minutes) | none: content cutoff | none | tool tests | GREEN under D4 | unchanged (F6) |
| M20 | output change without revision bump | - | - | - | that verify fails | undetected | GREEN (caught) |
| M21 | terrain revision bump | terrain | none | terrain scopes only | terrain verify, L2 | RED (F14) | GREEN |
| M22 | producing module gains a first-party dependency outside the roots | - | - | - | roots fitness test fails | undetected (F15) | GREEN (caught) |
| M23 | adapter-only edit before R5 | none | none | none | common checks stale; verifies rerun | undetected (F15) | GREEN |
| M24 | fixture or comparison-contract change | none | none | none | that verify re-recorded | - | GREEN (no production identity) |
| M25 | descriptor, baseline, verifier, or `RLO` change | per descriptor | none | per descriptor | common-check receipt stale | undetected (F19) | GREEN |
| M26 | same verification identity re-recorded | - | - | - | record mode refuses | possible | GREEN (refused) |
| M27 | map post-process or compiled-section-only change without a bump | - | - | - | map or terrain verify fails | undetected (F17) | GREEN (caught) |
| M28 | one built-in producer's private `.cpp` edit | none | none | none | that leaf module builds; its verify only | RED (F20) | GREEN |

## Build-locality note

Module direction is clean (F12) and producers become leaf modules (Decision 4), so a producer's
private `.cpp` edit compiles and links only its leaf, and a `ProjectWorldEditor` public header edit
fans out to the leaves that include it, which is the expected and explained cost. S0 and S7 record
UBT's compile and link actions for a no-op build, one leaf `.cpp` edit, and one generic public
header edit; the action lists are the acceptance (invariant 13). Wall time for those builds, one
owner verify, the full verify suite, and the common checks is recorded as diagnostic (A10).

## Proof traceability

| Invariant | Acceptance surface | Envelope | Cheapest proof | Final proof | Stop condition |
|---|---|---|---|---|---|
| 1, 2 | producer fingerprints, descriptors | Pester, static | a source edit moves nothing; a revision bump moves only its producer; no revision literal in C++ | same, green | blocks merge |
| 3 | saved and reloaded output | UE automation, twin script | each verify red under every sabotage in its row, green after a refactor | every verify green with S0 baselines, and every sabotage rerun red on the S7 tree (I25) | blocks merge |
| 4 | common-check staleness, `plan` | Python | M22, M23, M25 as tests: an adapter, ProjectObject, MPD, descriptor, baseline, verifier, and `RLO` change each stale the receipt | the fitness check fails closed on a missing or old export | blocks merge |
| 5 | realization host boundary | UE automation, Pester | a test-only producer through validation, dirty planning, apply, capture, a no-op rerun, and deletion in process; descriptor discovery with a test descriptor and post-apply builder dispatch from a synthetic realization result in Pester | same, green, with no generic edit | blocks merge |
| 6 | generic source and gates | static, UE automation | neutrality test; unknown-id test | green | blocks merge |
| 7 | dirty plan | UE automation | `Layers.DirtyClosure` cases: a terrain identity refresh and a presentation change leave dependents clean (roads and buildings whole for the latter); one terrain cell change keeps its halo propagation (control) | same, green | blocks merge |
| 8 | terrain base-actor bytes | adapter test | base identity uses the passed fingerprint | S5: base projections and partition paths equal; base identity tags exactly Authoring, Cell, Input, Material, Producer; compiled-section count per variant and projections equal (I20) | ARCHITECTURAL RED |
| 9 | E2E enrollment and acceptance | Python | F7 fixture: provenance-only change with runtime profile and layers | green | blocks merge |
| 10 | source tree | static | cleanup checklist | final diff review | blocks merge |
| 11 | ProjectWorldData and ProjectMaterial authority | audits, projections | the operation set before S5 equals the prediction | every `#### S5 operations` check; both audits accepted | blocks release (D3) |
| 12 | baselines | Pester, UE automation | M24 and M26 as tests | same, green | blocks merge |
| 13 | developer loop | UBT action lists, `plan` output, audit | S0 action lists and selections | S7: leaf edit compiles one module and selects one verify, no-op compiles nothing, zero writes and stale scopes; the contributor route run from a clean checkout | blocks S7 approval |
| 14 | generated-content lock, marker, cleanup | Pester, S5 log | the S2 delegation, recovery, cleanup, and orphaned-child cases | S5 log shows one acquisition, the marker written after the World snapshot, and its removal before the one release | blocks S5 |
| 15 | territory acceptance route | `bootstrap.py check`, `kazan_territory_v1` Matrix | the affected suites at each S0-S3 exit (diagnostic; failures equal the recorded red set) | one check after S4 and one Matrix on that receipt before S5, each with at most one replacement (I27) | blocks S5 |

## Implementation slices

Each slice records its locality block before coding; editing or regenerating an UNTOUCHED owner is
ARCHITECTURAL RED and returns here. Every slice after S0 keeps every verify green with the S0
baselines unless it bumps a revision on purpose; none does before S5.

### Implementation design

Four read-only design passes checked S0, S1, S2 identity, and S2 recovery against the code. Their
working notes are `tmp/world/locality/s0_design.md`, `s1_design.md`, `s2_identity_design.md`, and
`s2_recovery_design.md` (scratch, not authority). The code forced these adjustments, each within the
approved intent:

| # | Slice | Adjustment | Evidence |
|---|---|---|---|
| I1 | S0 | Every verify compares an output projection; no row compares package bytes | every save of a new package draws a random `PersistentGuid` |
| I2 | S0 | Map, terrain, and pipeline run as one script verify on the twin with three baselines; the other rows run in the editor against a test-only mount under `tmp/` | map creation and save are private to the realization service and need an on-disk compile result; the MeshPartition builder runs only out of process; producers skip saving actors on `/Temp` maps |
| I3 | S0 | Descriptors and their schema land in S0 as inert data, so the verification identity reads revisions from the start; `GF` reads them from S2 | baselines recorded in S0 then survive S2 unchanged |
| I4 | S0 | The twin becomes `synthetic_territory_twin`: building v2 with `building_semantics` in its compile profile, vegetation, a `territory_product` runtime profile, and updated synthetic-leg Matrix expectations | no twin runtime profile exists; the twin declares `none` |
| I5 | S0 | Terrain uses the twin's two cells; road uses tracked road bundles run through the road producer instead of the P0-route tests | the adapter has no test cells; the cited road tests run the P0 route and lack road class and cell membership |
| I6 | S0 | The terrain projection excludes identity tags and receipt provenance (`adapter_compiler_sha256`, `receipt_payload`, `receipt_sha256`) | otherwise S1 and S2 turn it red with no output change |
| I7 | S0 | The tool architecture test admits `Data/Producers/` and `Data/TestFixtures/Verify/` beside `Data/Schemas/` | `test_world_tool_architecture.py:15-28` |
| I8 | S0 | Proof roots stay explicit; the fitness check fails when UBT's resolved closure reaches a first-party module outside them; ProjectObject roots are declared for gameplay | an auto-added closure can never fail; gameplay reaches ProjectObject's spawn service through a service locator UBT cannot see |
| I9 | S0 | No committed declared-red list: S0's red tests are observed and recorded, turn green in S1 and S2, and the first accepted common-check receipt follows S4 | |
| I10 | S0 | `tools/World/CanonicalCompilation/api.py` drops its import of the module deleted in `c08b79afe` | E2E enrollment and acceptance cannot import today |
| I11 | S1 | The P0/representative route is removed before S1 instead of in S3, so the contract never carries it | it still writes the fixed counters and uses the `ProjectWorld.Cell=` lifecycle fallback |
| I12 | S1 | `ProjectWorldWaterContractParsing` stays generic (canonical loader); water moves four files | it parses canonical water contracts that other layers read |
| I13 | S1 | The terrain descriptor carries a `runtime_acceptance` block, which product terrain acceptance reads through its authenticated contract | Decision 5's producer expectations had no data owner |
| I14 | S1 | Source-hash fingerprint values move when S1 moves files; only the formula stays until S2 | `GF` hashes files that S1 moves |
| I15 | S2 | Atomic writes use `File.Replace`/`File.Move`; World transactions delete the journal before the snapshot; a World-owned outer snapshot pair covers the manifest root | `Move-Item -Force` deletes, then moves, on PowerShell 5.1; the manifest root lies outside the snapshot root |
| I16 | S2 | The ProjectMaterial host restores by copy, stage, rename, and check before any delete; releases the lock last; writes a structured receipt with content digests on every run; O3 takes a caller `-OperationId` recorded in the marker first; `RestorePrevious` accepts a bundle only with that `replaced_by_operation_id` and matching digests | the host deletes live surfaces before checking that the snapshot exists and releases the lock before its cleanup; a stale `Validate` yields no structured identity |
| I17 | S2 | Delegation goes through `Enable-`/`Disable-ProjectGeneratedContentLockDelegation`; the marker is `tmp/world/world_realization/outer_recovery.json`; the recovery entry is `scripts/ue/generated_content/recover_generated_content.ps1`; the same-project Unreal check moves to the lock owner; `run_all.ps1` and the proof roots cover `scripts/ue/generated_content` | no gate ran the lock's tests; the proof roots omitted its owner |
| I18 | S4 | The manifest writer's provenance refresh joins S4 | Decision 8 needs the writer as well as enrollment and acceptance |
| I19 | S5 | Order O1, O4, O2, the layout-receipt step, then O3 | with the new formula every scope is stale, so O2 first would rebuild every layer; realization writes no layout receipt |
| I20 | S5 | O2 keeps base-actor and partition paths; compiled sections keep their count per variant and their projection, while their package paths may change | every past full base rewrite replaced all compiled sections (Kazan 315, Manhattan 321) |
| I21 | S5 | The preflight refuses while a public World projection runs; that script's own lock belongs to `20260914-1215_audit_public_world_release_projection_recovery.md` | the projection writes ProjectWorldData without the shared lock |

F9 stays a probe. If it is red, its fix must keep road-only changes out of dependents (Decision 6);
widening O2 to vegetation or buildings is not an option, and if no such fix exists the work stops here
(D7). R1 confirmed F9 red; I22 is that fix.

R1 findings absorbed:

| # | Slice | Adjustment | Evidence |
|---|---|---|---|
| I22 | S2 | F9: the v2 request carries each base layer's prior `presentation_profile_sha256`; a producer that declares a presentation binding (road, building) joins the whole-layer, after-closure set when it differs from the current profile's; terrain keeps its `Material=` reuse tag. Unit hashes, halo propagation, semantic hashes, and baselines stay; O2 keeps terrain only | R1 finding 1; every active scope records the current presentation hash |
| I23 | S1 | With the P0 removal (I11), `accept`, the common-hash profile ids (`contract_inputs.py:22`), the End-to-End README, and the territory contract move to the `kazan_territory_v1` territory Matrix run, with a test | `acceptance.py:363-413` and `cli.py:33-34` require p0 and representative runs that realization rejects |
| I24 | S6, before S5 | The territory contract's rule that a fingerprint cannot advance without a rebuild (`:790-800`) is replaced by a link to the identity-refresh rule before S5; pitfall 33 agrees | O4 refreshes fingerprints without rebuilding |
| I25 | S0, S7 | The terrain verify keeps both compiled sabotages; the second is code-only (the transformer strips a section's collision or offsets a section mesh component) and must fail on a section projection field with base meshes and the verification identity unchanged; every sabotage reruns red on the S7 tree | the S0 notes kept only the tag sabotage; the adapter sets no modifier build setting, so editing the MPD would fail on identity instead; verifies move in S1 |
| I26 | S2 | Marker updates delete the staging file only while the marker exists and otherwise promote it; any marker write failure is terminal for the run; a Pester case covers a lost replacement | `ReplaceFileW` can leave only the staging file (`ERROR_UNABLE_TO_MOVE_REPLACEMENT`) |
| I27 | all | Gates: S0-S3 exits run affected suites as diagnostics; one `bootstrap.py check` after S4 and one `kazan_territory_v1` canonical-authority Matrix before S5, each with at most one replacement (invariant 15). Use the full Matrix only when source or canonical ownership changes. | `canonical.md` section 7 budget; the pinned source cache is unavailable, while the promoted canonical authority validates |
| I28 | S0, S1 | Descriptors are keyed by generator id and version and allow one optional `runtime_acceptance` block that never enters identity | the S0 schema rejected I13's block |
| I29 | S0 | Record and probe runs force the cold editor path and confirm the record line in the log | `run_cpp_tests_safe.ps1:52-77` drops `-ExtraArgs` on a warm editor |
| I30 | S1 | The generic profile schema checks tuple values by identifier pattern; the registry alone validates their values | enums in the schema are a second validator and force a generic edit per new mode |

 Capture readiness's `.uplugin` entry already stales both terrain scopes under the old formula;
O2 re-realizes them anyway.

### S0 - verify net and guards on today's code

```text
owning black box:            World generated identity and its proof
public interface / contract: verify baselines, fixtures, proof-input set
expected components CHANGED: tests, the projection mechanics, fixtures and their consumers,
                             baselines, inert descriptors and their schema, the record, verify, and
                             UBT export scripts, contract_inputs.py, planning.py, checks.py, the tool
                             architecture test, CanonicalCompilation api.py (I10)
expected components UNTOUCHED: production code, production manifests
```

- [ ] Measure the workflow (A10).
- [ ] Determinism probe per producer: two fresh fixture runs after save and reload; settle each
      row's owned output projection (A13).
- [ ] The projection mechanics and the verifies per `### Verify table`, calling today's entry
      points; record baselines with their verification identity on today's code (F10: this is the
      accepted behaviour); prove each red under every sabotage in its row.
- [x] Baseline rules: record mode as a separate script entry; tests for M24 and M26.
- [ ] Twin fixture moves to building v2 and is renamed (Cleanup plan); vegetation coverage added.
- [x] Proof-input set in `contract_inputs.py` (Decision 2), used by `plan` and the staleness hash;
      the UBT export script and the fail-closed fitness check; tests for M22, M23, and M25.
- [ ] Fingerprint tests (a source edit moves nothing; a revision bump moves only its producer), the
      static neutrality and module-direction test, the unknown-id test, and the F14 dirty-closure
      case: observe red.
- [ ] F7 and F9 cases: observe red or refute; a refuted case is recorded and dropped.
- [ ] Gate: S2 does not start until every verify row has its settled output, its sabotages red, and
      its baseline recorded; source fingerprints stay until then.

### S1 - layer producer contract and neutrality (Decisions 4, 5)

```text
owning black box:            ProjectWorldEditor generic pipeline; each producer; the adapter
public interface / contract: layer producer contract and registry; neutral terrain role
expected components CHANGED: GeneratorRegistry, LayerInventory, RealizationService(.h), lifecycle,
                             StaticPartitionAudit, ProductTerrainAcceptance, profile schema, RLO,
                             public contract headers of ProjectWorldEditor, five new leaf modules
                             and `ProjectWorld.uplugin`, result consumers (layered_validation.py)
                             plus the P0/representative route removal (I11) with the accept route
                             move and its End-to-End files (I23), moved producer tests,
                             ProjectWorldEditor.Build.cs, realization result consumers, adapter
                             registration through the contract
expected components UNTOUCHED: producer output (verifies green), generated artifacts, the
                             fingerprint formula (I14)
```

- [ ] Promote the headers the leaves need to `ProjectWorldEditor/Public`; move each producer into its
      leaf module with its registration, descriptor reference, and focused tests.
- [ ] The verifies switch from today's entry points to the registry path with unchanged baselines.
- [ ] The host-boundary test of invariant 5 with a test-only producer.

### S2 - revision identity and single owners (Decisions 1, 3, 6)

```text
owning black box:            World realization pipeline, each producer, the adapter
public interface / contract: generator_fingerprint field and audit, unchanged; descriptors
expected components CHANGED: GF, descriptors, realization request, adapter reuse tag and Build.cs,
                             adapter receipt and its TestFixtures copy, ProjectMaterial receipt
                             parser, dirty plan, planning.py, fingerprint tests, pitfall 33/36 tests,
                             the shared lock (delegation variable, outer-recovery marker, its
                             recovery entry), every reference to the old variable name,
                             cleanup_workspace.ps1, the Material host's RestorePrevious mode and
                             receipt (I16), World transaction commit order and outer snapshot
                             pair (I15), road and building presentation-binding declarations and
                             the presentation hash in the request (I22)
expected components UNTOUCHED: ProjectTexture, canonical tools, geography; ProjectMaterial
                             generated assets until S5's Material refresh (F18)
```

- [x] Lock delegation and outer recovery (F21) with permanent Pester coverage:
      - parent holds the lock: a World child with the live token enters; a ProjectMaterial host
        child with the live token enters; an unrelated mutation without the token is refused;
      - a missing or wrong token fails closed;
      - a simulated failed outer restoration followed by coordinator exit leaves the marker and its
        snapshots; a new persistent mutation is then refused although no process holds the lock;
      - with an unresolved marker, `cleanup_workspace.ps1 -Apply` is refused and the snapshots stay
        unchanged; with a live outer lock it is refused too;
      - a malformed marker, or one naming a path outside its roots, fails closed;
      - the parent exits while a delegated child still runs: recovery is refused; after the child
        exits, recovery restores both owners;
      - after recovery the marker and its snapshots are gone, and cleanup and a normal mutation
        proceed.

### S3 - cleanup (D5, `## Cleanup plan`)

```text
owning black box:            World realization pipeline, producers, World tools
public interface / contract: unchanged for every surviving producer
expected components CHANGED: dead routes, profiles, fixtures, scripts, docs that describe them
expected components UNTOUCHED: every surviving producer's output (verifies green)
```

### S4 - provenance acceptance (Decision 8)

```text
owning black box:            End-to-End enrollment and acceptance
public interface / contract: manifest input_identity, accepted receipts
expected components CHANGED: E2E enrollment.py, acceptance.py, the manifest writer's provenance
                             refresh (I18), their tests
expected components UNTOUCHED: canonical tools, World producers, manifests
```

### S5 - one combined migration before R5 (D3, A11)

```text
owning black box:            generated World authority (ProjectWorldData); ProjectMaterial surfaces
public interface / contract: supported transaction; realization wrapper; Material generation
expected components CHANGED: per #### S5 operations
expected components UNTOUCHED: everything #### S5 operations does not allow
```

- [x] Derive the operation set from the surviving scopes after S3 and F9 and review it before any
      write: each scope in exactly one operation, with its predicted audit result.
- [x] Before any write: the stable identity-refresh rule has replaced the territory contract's
      rule that a fingerprint cannot advance without a rebuild (I24), and invariant 15 holds.
- [x] Before: audit stale set equals the prediction; every other audit check passes; verifies green
      against S0 baselines; output projections of the terrain scopes recorded for both
      territories. The existing Kazan actor with a v1 building tag is direct evidence that
      building scopes need realization despite v2 manifest metadata; do not include them in O4.
- [x] Acquire the shared mutation lock once, before either snapshot, and export its token through
      `PROJECT_GENERATED_CONTENT_LOCK_TOKEN` for every World and ProjectMaterial call, in process
      and in child processes; no step acquires the lock on its own.
- [x] Take the World outer snapshot through `New-ProjectWorldGeneratedSnapshot`
      (`generated_content_transaction.ps1:122`) for the manifest root and every affected generated
      root, in the marker's confined root, as `public_world_projection.ps1:80` does for its run.
      ProjectMaterial needs no earlier snapshot: O1 and O2 do not touch it, so O3's own host
      transaction captures its pre-S5 state and retains it as `RollbackPrevious` on success.
- [x] Write the marker (Decision 3), then update its phase and Material state atomically at each
      step; the coordinator runs every World and Material host synchronously.
- [x] Retain O1's presentation scope after S3's reader check; run O4, O2, the
      layout-receipt step, then O3 (I19), O3 through ProjectMaterial's host
      transaction (`run_material_generation.ps1`), then every check; the local coordinator stays
      under `tmp/`.
- [x] A failure anywhere in O1-O4 restores both snapshots while the lock is still held; the audit
      and `Validate` then match their pre-S5 results, and no mixed identity state remains.
- [x] Remove the marker and release the lock only after O1-O4, the final audit, and `Validate` pass,
      or after both restorations succeed and match the pre-S5 results. After a failed restoration
      the coordinator does not release the lock while it lives; it keeps the marker and snapshots
      and stops for the operator. Once it exits the OS drops the lock, and the marker refuses every
      later mutation and every cleanup until the recovery entry resolves it.

#### S5 operations

| Op | Scopes or owner | Permitted changes | Must stay equal | Evidence for keeping the rest |
|---|---|---|---|---|
| O1 scope retirement | `presentation:v1`, only if S3 removes it | unlink its consumer list, then retire it through the transaction | every other manifest and artifact | S3's reader check; audit after |
| O2 producer realization | both terrain and both building scopes | terrain: base-actor packages (identity tag, re-save), layout receipt identity field, compiled-section packages if MeshPartition rebuilds them, manifest fingerprint, generation, operation, time, artifact digests, `semantic_sha256`; buildings: replace old v1-tagged actors with v2 actors and regenerate affected mesh and external-actor packages plus manifests | terrain base-actor and partition path sets, compiled-section count per variant (paths may change, I20); terrain `canonical_inputs`, `dependency_inputs`, material tags, A13 output projection, and MPD bytes; building canonical inputs, v2 layer contract, and meaningful saved actor/mesh output; every other layer outside the dirty plan | before and after terrain projections; historical and repaired 342-record saved building projections equal after excluding version tags and allocation IDs; exact building v2 inventory and authority audit; the dirty plan lists terrain and buildings only |
| O3 Material refresh | ProjectMaterial surfaces, with the new receipt from O2 | compiler fingerprint, semantic identities, surface package bytes, accepted surface manifest | public object paths, `layout_sha256`, pattern bindings; every World manifest | ProjectMaterial `Validate` accepted; the World audit unchanged by O3 |
| O4 identity-formula refresh | the 10 surviving scopes outside O1 and O2 | `generator_fingerprint`, `generation`, `accepted_operation_id`, `accepted_at_utc`; active-set record | every other manifest field; every artifact path and digest; no package written | `New-ProjectWorldFingerprintMigrationCandidate` only clones metadata, so the proof is outside it: verifies green against baselines recorded on the accepted code (F10), an audit showing only `generator_fingerprint_current` stale, and current realized actor/asset identity for these scopes |
| D5 removals | files in `## Cleanup plan` | deletion | all other tracked files outside this task's diff | `git status` against the list |

Any other change is ARCHITECTURAL RED: stop and return here.

### S6 - documentation (`## Documentation plan`)

### S7 - final fitness suite, the audit, independent R2

## Cleanup plan

| Item | Evidence | Consumer | Action |
|---|---|---|---|
| P0/representative bounded-procedural route: `CreateOwnedActors` and section builders (`ProjectWorldGeneratedGeometry.cpp:114-600`), non-product branch of `ProjectWorldRuntimeRealization.cpp` (`:439-460`, `:792`, `:843-912`), `bounded_procedural_route` profile kind, `p0`/`representative_v1` validation profiles, End-to-End and World tools wording | realization rejects these profiles (`ProjectWorldRealizationService.cpp:224-231`); maps archived | tests, and the `accept` route that I23 moves to `kazan_territory_v1` | remove (D5); keep `kazan_p0_empty_overlay_v1.json`, the `kazan_representative_v1` presentation profile, and `FProjectWorldPresentationGate`, which is the live packaged territory gate |
| Building massing v1 | production uses v2 | twin fixture, one test | move the fixture to v2, then remove v1 |
| `presentation:v1` scope | zero artifacts; its only use is the maps' `consumer_references` | maps' consumer links | S3 checks the audit, enrollment, and payload: remove through S5's O1 if nothing else reads it; otherwise keep with its verify row |
| `Generated/Presentation` root and its snapshot branches | no writer | tests | remove the root |
| Adoption of a plain tagged `AMeshPartition` (`MTP:515-530`, `:615-627`) | every tagged partition is the project subclass | none | remove |
| `Landscape`, `Foliage` in `ProjectWorldEditor.Build.cs` | no use | none | remove |
| `Remove-ProjectWorldGeneratedHLODArtifacts` | no tracked HLOD layers | UNVERIFIED | remove after one TestData fixture realization shows no HLOD asset |
| `cleanup_workspace.ps1` one-time targets (`:100-103` and the one-time entries of `:128-164`; the live `visual_verification` and `source_ingestion` test scratch targets stay) | targets absent | none | remove |
| `ProjectPCG` empty module, unused PCG dependency | empty startup and shutdown | none | remove |
| Landscape-era fixture name `synthetic_landscape_water_twin` | the fixture is Mesh Terrain | its own files | rename with the S0 fixture changes |
| Duplicate `algorithm_version` default `"alis-world-compiler-1"` (`lineage.py:43`, `pipeline.py:522`) | every tracked compile profile sets the field | tests that omit it, if any | make the field required and delete both defaults |
| `RLO` tuple and settings re-check (`RLO:19-168`) | third validator, already divergent (F3) | `RLO` | remove (Decision 3) |

## Verification plan

- L0: Pester and Python suites; exact UE automation tests through
  `scripts/ue/test/unit/iterate.ps1 -TestFilter <exact name>`; `python -m unittest discover
  tools/World/tests` and each touched tool's tests.
- Workflow: the A10 measurements at S0 and S7.
- Verify suite: every `Project.World.Realization.Verify.*` test by exact name. S0-S3 exits run the
  affected suites individually (diagnostic; their failures must equal the recorded red set); one
  `bootstrap.py check` after S4 and one `kazan_territory_v1` canonical-authority Matrix on that receipt before S5, each
  with at most one replacement (invariant 15, I27).
- Build: `scripts/ue/build/build.bat AlisEditor Win64 Development`.
- Audit: `audit_generated_authority.ps1 -WorldDataPlugin ProjectWorldData` before S5 (stale set
  equals the prediction) and after S5 (accepted), plus the `#### S5 operations` checks; ProjectMaterial
  `Validate` after O3.
- Governance: `validate_no_alis_prefix.bat`, ASCII check on changed docs.

## Documentation plan

- **World pipeline layers, Impact-Driven Development:** replace "World fingerprints should identify
  byte- or manifest-producing behavior" and its list-based locality proof with the revision rule:
  identity is id + revision + pipeline revision + data inputs + engine; an output change bumps the
  revision and re-records the baseline; the verify is the proof and runs in the common checks;
  `plan` and the staleness hash share one set of generation roots.
- **Canonical World Realization:** the fingerprint section states the formula, the descriptors, the
  verify test and its record mode, the identity refresh rule after a formula change, and the
  dirty-closure rule for identity changes.
- **Territory contract:** the layer producer contract and how to add a producer; settings owned by
  the producer; before S5, the rule that a fingerprint cannot advance without a rebuild
  (`territory_contract.md:790-800`) gives way to a link to the identity-refresh rule, and pitfall 33
  agrees (I24).
- **ProjectWorld pitfalls:** rewrite 36 for the fingerprint tag; update 33 if its line-ending case no
  longer applies; add one entry for an output change without a revision bump, caught by verify.
- **End-to-End Validation README and World tools router:** remove the P0/representative route;
  `plan` routing by generation roots.
- **ProjectWorldMeshTerrain README:** the adapter owns its descriptor, settings validation, and
  post-apply builder.
- **ProjectMaterial docs:** the layout receipt digest is provenance; only `layout_sha256` is consumed.
- **ProjectWorld architecture:** `docs/architecture/structure.md` and `docs/architecture/diagrams/main.md`
  replace the terrain-only registry with the layer producer contract, list the five leaf modules,
  and show their dependency on `ProjectWorldEditor`.
- **Contributor route, one place:** the Territory contract's producer section states where a
  producer lives (its leaf module), what it owns, how to run its fixture and verify, when to bump its revision, how
  to re-record its baseline, and what stays untouched; other documents link to it. S7 walks the
  route from a clean checkout with no `tmp/` evidence.
- **World pipeline layers:** the `### Developer workflow` table as the World inner loop.
- **world-engineering skill:** unchanged unless its fingerprint wording contradicts the new rule.
- **Backlog:** S6 corrects G-1 in the chore task.
- Stable docs, code, tests, and configuration never reference this todo.

## Rollout and rollback

S0-S4 land as one reviewed change with S5 before R5 (D3).
- **During S5:** one outer rollback covers both authorities: a failure anywhere in O1-O4 restores the
  pre-S5 World generated packages, manifests, terrain tags, and compiled output from the World
  outer snapshot, and the pre-S5 ProjectMaterial surface packages and manifest from O3's own
  transaction or its `RollbackPrevious` bundle, while the one outer lock is still held (invariant
  14). Each owner's own transaction still rolls back its
  own step. The lock is released only after full success or a successful restoration of both. A
  failed restoration keeps the outer-recovery marker and both snapshots; the OS drops the lock when
  the coordinator exits, and the marker refuses every later persistent mutation and cleanup until
  the recovery entry restores both owners and removes it.
- **After S5:** reverting the change and the S5 authority commit restores the previous formula,
  manifests, tags, and Material surfaces together.

## Completion criteria

- Every finding is fixed, or refuted and recorded; every mutation-matrix row matches its Target in
  a permanent test.
- `#### S5 operations` hold; the ProjectWorldData audit and ProjectMaterial `Validate` accept.
- No source-set list, validator copy, settings copy, per-producer generic branch, or dead generation
  path remains; generic modules carry no representation dependency.
- The S7 workflow measurements meet invariant 13, and the contributor route works from a clean
  checkout.
- `git grep ALIS_WORLD_CONTENT_LOCK_TOKEN` finds nothing outside `todo/`; this is a one-time
  completion check, not a permanent test that would keep the old name in the tree.
- Documentation describes only the resulting system; independent R1 before production code and R2
  on the final diff both PASS.

## Review record

### 2026-10-04 - independent R2 PASS

- The non-author reviewer accepted the saved/reloaded output comparison,
  rename-endpoint routing, raw-cache-independent canonical mode, and saved
  building recovery path after the earlier PATCH. It also accepted the
  historical LFS-based building comparison, protected production no-op,
  complete review history, and isolated consumer checkout.
- Local verification matched the reviewed 1936-path candidate manifest to
  commit `ecd14a610e8628f4b538520662cb8b3e9b4bfa52` with zero path or
  file-hash differences. The Check, Matrix, and post-Matrix audit accepted.
- Historical raw-source replay and packaged runtime remain separately owned
  acceptance work; neither is claimed by this World-only R2.

### 2026-10-04 - reviewer R2 PATCH

- **Accepted:** the clean Matrix previously compared input-derived terrain
  semantics without saved compiled output; its map-scope path exception was
  broad; the planner lost rename sources; canonical mode scanned unused raw
  cache; and the first building test did not cover persisted stale identity or
  the 61 production repairs. All received focused corrections and proof in the
  current handoff. Input-only `Validate` was not treated as an output no-op;
  actual protected Apply passed on both production maps.
- **Accepted historical correction:** the earlier committed
  CanonicalCompilation edits must be included in the full review path set.
  Their required-field cleanup preserves all four admitted production v2
  profiles, but the World-only Matrix does not prove raw-source replay.
- **Refuted premise:** no pre-S5 outer snapshot remained after S5. The exact
  old 122 building payloads were available in HEAD's local LFS store, matched
  the prior manifest, and were substituted under a new recoverable snapshot
  for saved-output comparison. The reviewer had not inspected local receipts
  or binary payloads and could not establish that a snapshot was preserved.
- **Verdict:** PATCH until the clean consumer checkout and independent R2
  verify the final candidate. Packaging remains the separate R5 gate.

### 2026-10-02 - architect review of the brief and investigation

- **Trigger:** operator asked for `/architect` and `/investigate-change` on the reviewer's brief,
  with all todos prioritized and few new files (D1, D2).
- **Architect verdict on the brief: PATCH.** Its goal matches binding rules, but four points
  changed. It framed the work as speed, while F1 shows a correctness gap. Its repo-wide sweep was
  cut to the owners with RED edges: Texture and Material are already GREEN, and the module graph is
  clean, so UBT probes were dropped. Narrowing owner fingerprints revisited a 2026-09-30 decision
  (Q1). It did not address release sequencing (Q2).
- **Absorbed:** the fingerprint-scope prototype. The Visual Verification census and planner went
  to the capture task, which owns that route.
- **Authority:** D1, D2, Q1-Q3, A1-A5 added.

### 2026-10-02 - operator answers

- **Trigger:** operator answered Q1-Q3.
- **Result:** Q1 analysis (D4) replaced the planned list correction with UE's derived-data model
  (Decision 1-2, F13); the cleanup now includes the P0 route and costs no fingerprint churn;
  everything lands before R5 and the release router links this task.
- **Authority:** D3-D5 added; Q1-Q3 closed; A1, A2 rejected by evidence; A4 resolved by D5; A6
  added.

### 2026-10-02 - third external review (PATCH, six findings)

- **Trigger:** operator asked to evaluate the reviewer's PATCH against the code.
- **Accepted:** the verify net is mandatory and routing fails closed (F15; Decision 2 reuses the
  common checks R5 already requires, not new receipts); one revision owner, settled as descriptors
  read only by `GF`, with the fingerprint passed to C++ (Decision 3); a verify row per producer
  (`### Verify table`); an exact S5 contract (`### S5 contract`). F4 is in scope: the earlier
  non-goal contradicted D2's brief, and the evidence shows about 12 generic surfaces and validators
  that already disagree.
- **Partly refuted:** upstream identities. The reviewer is right that Decision 5 of the previous
  revision would have removed CanonicalCompilation's own code identity from its own reuse key with
  no replacement; it is withdrawn. A revision model upstream is not adopted: F6 measures minutes of
  owner-local recompute with a content cutoff before World, and ExecutionEnvironment code produces
  output, so its digest is a real input (Decision 7).
- **Found during the check:** F14 (dirty closure; without its fix S5 would rewrite every layer),
  F16 (no tracked baseline exists), F17 (terrain semantic hashes cannot see geometry), the
  ProjectMaterial receipt re-derivation, and that one data plugin holds both territories. Two
  citations corrected (`ExecutionEnvironment/app/identity.py`; `lineage.py:42-52`).
- **Authority:** A6 rejected by evidence; A7-A9 added. D6 added after the report: F4 stays in scope.

### 2026-10-02 - fourth external review (PATCH, six findings)

- **Trigger:** operator asked to evaluate the reviewer's PATCH against the code.
- **Accepted:** verifies observe saved and reloaded output, with sabotages aimed at the known gaps,
  map exposure and compiled-only terrain changes (F17, A12); the goal states the fixture limit;
  a separate verification identity with baseline rules, which production identity never reads
  (Decision 2); the Material refresh, confirmed in code (F18); the proof-input set keeps today's
  roots and uses UBT's resolved export instead of parsing `Build.cs` (F19); S5 split into separate
  operations on a derived scope set (A11); the precise dirty-closure rule (Decision 6); the workflow
  as an acceptance surface with measurements (A10, invariant 13); the architecture documents and one
  contributor route.
- **Refuted:** none. Two clarifications: World plugin `Data/` never ships, so descriptors and
  baselines stay out of the release payload (F19); the transformer only tags sections, so most
  compiled-geometry changes come from the MPD or the engine, which the fingerprint already covers,
  and the terrain verify still observes compiled output.
- **Authority:** A5, A7, A8 rejected by evidence; A10-A12 added.

### 2026-10-02 - fifth external review (PATCH, four findings)

- **Trigger:** the operator passed the reviewer's PATCH.
- **Accepted:** leaf Editor modules for the five built-in producers, so a producer edit builds one
  module and selects one verify with no list (F20, Decision 4); producer-owned projections on shared
  mechanics instead of a generic property snapshot (A13); structural speed acceptance with wall time
  as diagnostic (invariant 13); one outer rollback over World and ProjectMaterial in S5; baselines
  apart from descriptors.
- **Adjusted:** baselines live in `Data/TestFixtures/Verify/`, the repository's existing tracked
  test-fixture location (the Mesh Terrain adapter's `Data/TestFixtures/`), not a new `Tests/` root.
  A generic snapshot would not have reduced engine-upgrade churn, since engine identity is in the
  verification identity and an upgrade re-records anyway; the gain is fewer producer-irrelevant
  failures and clear messages.
- **Refuted:** none.
- **Authority:** A12 rejected by evidence; A13 added.

### 2026-10-02 - sixth external review (PATCH, one finding)

- **Trigger:** the operator passed the reviewer's PATCH.
- **Accepted:** S5 is one concurrency envelope on the existing lock and delegation (F21, invariant
  14): one acquisition before the snapshots, delegated World and ProjectMaterial steps, restoration
  under the lock, release only after success or restoration, and four permanent Pester cases.
- **Found during the check:** the ProjectMaterial host accepts no delegated token today, and the one
  lock had a World-named delegation variable; the lock now owns one variable that both callers pass.
- **Refuted:** none.
- **Authority:** no change.

### 2026-10-02 - seventh external review (PATCH, two findings)

- **Trigger:** the operator passed the reviewer's PATCH.
- **Accepted:** the rename covers all eight files that use the old variable, with no alias (F21);
  an OS-held lock cannot outlive its process, so a durable outer-recovery marker owned by the
  shared lock refuses every non-delegated mutation until the recovery entry restores both owners
  (Decision 3, S5, invariant 14), with permanent coverage for exit, refusal, and recovery.
- **Adjusted:** the zero-reference check for the old name is a one-time completion grep, not a
  permanent test; a permanent test would itself carry the removed name forever.
- **Refuted:** none.
- **Authority:** no change.

### 2026-10-02 - eighth external review (PATCH, recovery integrity)

- **Trigger:** the operator passed the reviewer's PATCH.
- **Accepted:** `cleanup_workspace.ps1 -Apply` takes the shared lock and keeps the lock file; the
  marker is a small atomic journal with confined paths and pre-S5 identities and fails closed when
  malformed; recovery removes it only after both owners match; the listed permanent cases (F21,
  Decision 3, invariant 14). Of the deletion routes searched, only cleanup touched recovery state.
- **Adjusted:** recovery opens the lock with no sharing and reuses the existing same-project Unreal
  process check instead of recording child PIDs. The OS handle cannot miss a child launched just
  before the parent died and cannot be fooled by PID reuse, and the process check covers a
  commandlet grandchild that holds no handle.
- **Found during the check:** ProjectMaterial retains `RollbackPrevious` but has no route to restore
  it, and the Material outer snapshot cannot be taken by its host before O1; O3's own transaction
  and an owner-local `RestorePrevious` mode replace it.
- **Refuted:** none.
- **Authority:** no change.

### 2026-10-02 - implementation design (four passes, before R1)

- **Trigger:** the operator approved the plan for implementation (D7).
- **Result:** read-only passes over S0, S1, S2 identity, and S2 recovery found the adjustments the
  code forces; `### Implementation design` records them (I1-I21) with their evidence, and the slice
  blocks, S5 order, O2 row, M15, proof row 5, and the cleanup row now match.
- **Not adopted:** a committed declared-red list for S0's red tests (I9); widening O2 to dependents
  if F9 is red (Decision 6 decides instead).
- **Authority:** D7 added.

### 2026-10-02 - independent R1 (PATCH, ten findings)

- **Trigger:** a fresh non-author reviewer checked the plan and the four design notes against the
  code (`tmp/world/locality/r1_review.md`).
- **Accepted:** all ten (I22-I30, invariant 15, proof rows 3, 7, 8, 15, the O2 row, and the
  verification and documentation plans). F9 is red for roads and buildings; its fix keeps
  presentation changes out of dependents, so O2 keeps terrain only.
- **Refuted:** none.
- **Authority:** no change.

### 2026-10-02 - R1 recheck (one finding)

- **Trigger:** the same reviewer rechecked the absorbed findings.
- **Accepted:** the compiled-terrain sabotage is code-only (I25 and the verify table): editing the
  MPD would fail on the verification identity, not on compiled output. The cleanup row, the S1 and
  S2 blocks, and a table break were aligned as well.
- **Authority:** no change.

### 2026-10-03 - pause and handoff

- **Trigger:** the account spend limit stopped a running agent; the operator asked for the full
  picture in this todo so a fresh agent can continue.
- **Result:** `## Progress and handoff` records every finished chunk with its evidence, the expected
  red set, the ordered remaining work, and the resume notes; F22 was found and fixed.
- **Authority:** no change.
