# Realize Kazan Territory Vegetation - Slice 3C

**Status:** DONE - accepted and visually verified
**Roadmap source:**
[Kazan territory roadmap](../../02_backlog/world/world_generate_kazan_territory_roadmap.md#slice-3---realize-geography-in-independently-admitted-layers)

## Goal

Realize deterministic, territory-scale Kazan vegetation through the accepted
generated-layer architecture without per-tree actors or regeneration impact
on terrain, water, roads, presentation, authored overlays, or unrelated maps.

## Read first

- [Generation SOT](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition SOT](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [World test layers](../../../docs/testing/world_pipeline_layers.md)
- [Realization commands](../../../scripts/ue/world/README.md)
- [End-to-end validation](../../../tools/World/EndToEndValidation/README.md)

## Architecture boundary

```text
owning black box: vegetation generated-layer producer
public contract: realization profile layer + canonical cell inputs + stable
  seed identity + cell-owned instance artifacts + existing manifest publisher
expected components CHANGED: vegetation producer, its explicit fingerprint
  source set, canonical vegetation metadata adapter, generated-layer registry
  seam, shared inventory dispatch, synthetic fixture/profile expectations,
  Kazan realization profile
expected components UNTOUCHED: canonical authority, terrain/water/road
  producers, map/presentation producers, transaction/rollback owner, authored
  overlays, runtime save state, P0 and representative map semantics
```

Unexpected propagation into an untouched owner is architecture RED and stops
implementation until the owning contract is corrected.

## Execution

- [x] Inventory accepted canonical land-cover, vegetation-area, and foliage
  point semantics plus current material/mesh assets and exclusion inputs.
- [x] Evaluate partitioned PCG: the reusable module and forest pack contain no
  admitted graph or biome content, so 3C uses direct cell-owned HISM output
  without adding a second placement authority or dependency.
- [x] Freeze the smallest vegetation generator tuple and deterministic seed,
  cell ownership, density, exclusion, instance, and package contracts.
- [x] Pass per-layer admission on a minimal synthetic fixture: full, no-op,
  exact-cell, rejection/rollback, clean reconstruction, and sibling equality.
- [x] Enable the accepted layer in `kazan_territory_v1` within frozen package,
  memory, and runtime budgets.
- [x] Run the selected end-of-slice Check, Kazan Matrix, authorized L3, final
  authority audit, LFS check, and owner temp cleanup to accepted completion.
- [x] Correct road, water, and authored-mask exclusions plus exact cell input
  identity; prove synthetic exclusion and persistent empty-cell retirement.
- [x] Replace the stale 3C acceptance, enroll the corrected vegetation
  authority, recapture visual evidence, audit, and clean owner temp files.

## Accepted evidence

- Common Check: `Saved/Validation/WorldPipeline/check-20260821T154750Z/result.json`.
- Kazan Matrix: `Saved/Validation/WorldPipeline/run-20260821T155116Z/result.json`.
  It proves 146 cell actors, 279 HISM components, 7,490 instances from 7,528
  candidates, exact exclusions (road 7, water 30, mask 1), six realization
  legs, declared road-to-vegetation dirtiness, and byte-for-byte rollback.
- Producer migration proof: the Matrix marks vegetation whole-layer dirty and
  changes all 146 vegetation package digests before L3 publication.
- L3 enrollment: `Saved/Validation/WorldEnrollment/enroll-20260821T161107Z/result.json`.
- Strict final audit:
  `Saved/Validation/WorldAuthority/vegetation-slice3c-review-final-audit-v3.json`.
  Active set `fc9a9d345b2f2ec7283397179b17f695c8569a8dedf413028f8368f005219f4e`
  owns 1,026 current, byte-intact artifacts with no unowned files or journal.
- Authenticated nine-view capture:
  `Saved/Validation/WorldVisualEvidence/vegetation-slice3c-review-final-v3/capture.json`.
- Live MCP alignment and focused screenshots:
  `Saved/Validation/WorldVisualEvidence/vegetation-slice3c-review-final-v3/mcp/receipt.json`.
  The reloaded vegetation and road actors for `x-6:y1` share the exact
  `[-722900,-97900,0]` cell origin; 146 vegetation actors are tagged.
- Focused regressions passed: manifest 9/9, fingerprint 4/4, exact Unreal
  placement/exclusion/persistence tests, authenticated capture, and LFS fsck.

## Stop conditions

- Canonical-authority or source-provider change.
- Per-tree actor architecture at territory scale.
- Required edit to an accepted terrain, water, road, persistence, or rollback
  owner instead of the declared vegetation extension point.
- Unmeasured dependency/asset addition or a frozen budget change.
- Commit or push; operator-owned.
