# Realize Kazan Territory Roads - Slice 3B

**Status:** DONE - producer-local fingerprint v2 and final L3 accepted
**Roadmap source:**
[Kazan territory roadmap](../02_backlog/world/world_generate_kazan_territory_roadmap.md#slice-3---realize-geography-in-independently-admitted-layers)

## Goal

Realize recognizable Kazan roads end to end through the accepted generated
layer architecture: accepted canonical road authority -> typed road generator
-> declared dependency halo -> deterministic cell-owned Unreal artifacts ->
existing manifests and transaction -> territory realization.

## Read first

- [Generation SOT](../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition SOT](../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [Architecture overview](../../Plugins/World/ProjectWorld/docs/architecture_overview.md)
- [World test layers](../../docs/testing/world_pipeline_layers.md)
- [Realization commands](../../scripts/ue/world/README.md)
- [End-to-end validation](../../tools/World/EndToEndValidation/README.md)

## Accepted starting authority

- Canonical authority remains `kazan_territory_v1:86821a99...`; changing it is
  out of scope and requires an operator stop.
- Slice 3A durable active set: `1c812c00...`.
- Slice 3A enrollment: `enroll-20260820T162432Z`.
- Slice 3A final audit:
  `Saved/Validation/WorldAuthority/slice3-final-audit.json`.

## Architecture boundary

```text
owning black box: road layer generator
public interface / contract: typed generator registration + realization
  profile layer + dependency halo + owned artifact inventory
expected components CHANGED: road generator and registration, road profile,
  synthetic road fixture, focused tests, territory realization profile,
  territory validation expectations, generated road packages and manifests
expected components UNTOUCHED: canonical authority, terrain generator, water
  generator, transaction engine, manifest schema, persistence/rollback owner,
  authored overlays, runtime state, P0 and representative map semantics
```

Editing an expected-untouched owner is architectural RED unless evidence first
shows the accepted extension point cannot satisfy a required invariant.

## Required invariants

1. Road classes, widths, bridge/tunnel fallback, intersections, and terrain
   conformance come from accepted road semantics or the bounded road profile.
2. Cross-cell roads are continuous and dependency halo propagation is explicit.
3. Every generated fragment has deterministic cell ownership and one render
   and collision owner.
4. Road assets authenticate player-blocking collision configuration. A real
   territory trace remains part of playable-Kazan runtime acceptance.
5. Whole-layer, exact-cell incremental, no-op, rejected rollback, and clean
   regeneration preserve unrelated terrain, water, presentation, and authored
   bytes.
6. Territory output remains zero-HLOD and uses the accepted transaction,
   manifests, active-set-last commit, and audit path.

## Execution

- [x] Inventory current canonical road records and the bounded representative
  road adapter without changing authority.
- [x] Add the smallest typed road generator through the existing registry and
  profile extension point.
- [x] Add dependency halo propagation only where cross-cell road continuity
  requires it; zero-halo terrain and water stay exact.
- [x] Prove road geometry, terrain conformance, seams, ownership, collision,
  locality, no-op, rollback, and clean regeneration on the smallest fixture.
- [x] Enable roads in `kazan_territory_v1`, extend exact Matrix layer
  expectations, and realize the product territory. Profile, expectations,
  accepted Matrix, and the seven-scope product tree are complete; the final
  accepted enrollment receipt is still pending.
- [x] Rerun the coherent final Check, selected Matrix, authorized L3
  enrollment, and read-only authority/LFS audit after the R2 corrections are
  green.

## Current evidence and gate stop

- Editor build: accepted after the road generator and lifecycle changes.
- Exact UE tests: `Project.World.Realization.Layers.ProfileContract` and
  `Project.World.Realization.Layers.DirtyClosure` accepted.
- Canonical road fixture: one cross-cell primary road, two exact fragments,
  and an asserted shared boundary point.
- Reversible 3-core lifecycle with clean reconstruction: accepted at
  `Saved/Validation/WorldRealization/3-core-lifecycle/`
  `7b0cba34f97046748a7ad82d13b4dc9d/summary.json`.
- Synthetic measured topology: 2 road actors, 2 Nanite meshes, 30 triangles,
  and 4 authenticated artifacts.
- Kazan expected topology from accepted canonical authority: 4,394 selected
  road features, 161 occupied cells, 227,820 triangles, and 322 artifacts.
- Fresh common Check `check-20260820T171814Z`: accepted.
- Initial Matrix `run-20260820T170535Z` rejected only clean reconstruction;
  its outer transaction restored the pre-run generated tree.
- Replacement territory Matrix `run-20260820T172141Z`: accepted with 161 road
  actors, 227,820 triangles, and 322 authenticated road artifacts.
- Pre-enrollment refresh audit
  `Saved/Validation/WorldAuthority/road-enrollment-preflight-audit.json`:
  accepted with six current scopes, 558 intact artifacts, and zero unowned.
- Enrollment `enroll-20260820T174933Z` produced a coherent seven-scope tree but
  its receipt rejected after commit because the validator incorrectly treated
  regenerated scopes of the same target map as unrelated. Read-only audit
  `Saved/Validation/WorldAuthority/road-enrollment-post-reject-audit.json` is
  accepted with 880 intact artifacts and zero unowned.
- R2 restored strict sibling-scope manifest equality. Enrollment may add the
  requested layer scopes but may not replace a pre-existing map, terrain,
  water, presentation, P0, or representative scope.
- That validator edit invalidates the broad common-contract fingerprint, so
  the retry `enroll-20260820T175302Z` refused before mutation. Per the approved
  expensive-gate budget, another Check plus replacement Matrix requires an
  explicit scope decision.
- Matrix work retention is automatic: each new run keeps only the current and
  one previous run until enrollment no longer needs them.
- R2 removed the duplicate Python enrollment snapshot. The PowerShell
  generated-authority transaction remains the sole rollback owner; Matrix
  retains its separate observational backup.
- Final common Check `check-20260820T180239Z`: accepted.
- Final territory Matrix `run-20260820T180601Z`: accepted.
- Final enrollment `enroll-20260820T182137Z`: accepted with active set
  `eb123f140589d0788878f24b4c4b41fe0223b066cd4835c81f7ea3ffe5f9c74a`.
- Final read-only authority audit
  `Saved/Validation/WorldAuthority/road-slice-final-audit.json`: seven scopes,
  880 intact artifacts, current generator fingerprint, and zero unowned.
- `git lfs fsck`: accepted. All Matrix, enrollment rollback, and manual road
  refresh work under `tmp/world/` was removed after the final audit.
- Independent R2 review reopened closure: remove the duplicate Python rollback
  owner, restore strict sibling-scope enrollment equality, and prove that road
  admission does not advance terrain/water/presentation or unrelated maps.
- R2 removed aggregate realization-profile hash invalidation from per-layer
  no-op equality and prevents road-only self-saved actor changes from invoking
  Unreal's broad dirty-package save. Focused Python and Pester regressions are
  green.
- R2 common Check `check-20260820T184538Z` and replacement territory Matrix
  `run-20260820T184900Z` are accepted. The restored-tree pre-enrollment audit
  rejects only generator currency: the remaining global source fingerprint
  marks all seven scopes stale. L3 was not invoked because doing so would
  either fail before mutation or advance unrelated scope manifests, violating
  the correction being proved.
- Existing physical collision traces cover the older representative runtime
  route, not the new territory road meshes. Until the playable runtime gate,
  3B claims collision configuration/authentication only.
- Fingerprint v2 focused proofs: 76/76 world lifecycle tests and 82/82 E2E
  Python tests accepted. Explicit producer sets prove road implementation
  changes move only the road producer; true shared primitives move every
  affected producer.
- Final common Check `check-20260821T081654Z`: accepted.
- Final territory Matrix `run-20260821T082034Z`: accepted. Its real-road-cell
  locality leg selected `grid_413718bc833994e5:x-6:y1`; roads alone were
  dirty, terrain/water stayed clean with exact artifact records, and all
  content-identical geometry mutation counts were zero.
- One-time metadata-only fingerprint migration:
  `Saved/Validation/WorldAuthority/fingerprint-v2-migration.json`. All seven
  stable payload hashes were retained and the active set advanced from
  `eb123f14...` to `6c773355...` without artifact regeneration.
- Final L3 `enroll-20260821T084416Z`: accepted and retained active set
  `6c773355...`, proving strict existing-scope equality.
- Final authority audit
  `Saved/Validation/WorldAuthority/road-slice3b-final-audit-v2.json`: seven
  scopes, 880/880 byte-identical artifacts, current producer fingerprints,
  zero unowned files, and no journal. `git lfs fsck` accepted.
- The one-time migration temp owner and its transaction directories were
  removed after acceptance.

## Stop conditions

- Canonical-authority change.
- Required propagation into an accepted unrelated owner.
- Change to an accepted invariant, contract, or operator-control boundary.
- Irreversible local data destruction outside the accepted transaction.
- Commit or push; operator-owned.
