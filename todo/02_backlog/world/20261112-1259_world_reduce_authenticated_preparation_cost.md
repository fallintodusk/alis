# Reduce authenticated canonical preparation cost

Status: BACKLOG - optional performance work; no Terrain acceptance prerequisite
unless a measured cost prevents required verification.

## Owner and trigger

CanonicalCompilation refinement owns candidate preparation. Resume when a real
iteration demonstrates that its authenticated base reads or validation dominate
the required local work. Do not change source qualification, geography, World
transactions or native realization to improve this measurement.

## Retained evidence

The dry Terrain case measured 45.49 s for 631 authenticated reads/validations,
0.744 s geometry and 0.987 s copying 694,507,646 bytes. Four nodes/nine quads
changed while 636 existing outputs remained unchanged. This demonstrates local
geometry, not sparse total preparation. Total preparation was 55.10 s;
cold/no-op took 54.22/55.04 s and selection/materialization 213.10/102.78 s.
Evidence is retained under
`tmp/world/terrain_case_d34/`; remeasure the current implementation before design.
The separate repeated Water-footprint work already has a focused fix and does
not justify another optimization campaign.

The joint Ground/bank/Pier candidate measured 255.22 s preparation and 416.20 s
deterministic authority preparation. Its bank construction alone took 1.74 s
(35 quads, 456 triangles); the joint route also reconstructs the full 210-cell
Ground operation. Do not attribute the entire cost to the bank or claim sparse
total execution. Reuse the pinned result and attribute the current owner costs
before optimizing: `tmp/world/terrain_execute_d44/{joint-input/publication-report.json,authority-report.json}`.
Fresh public candidate-input admission measured 789.90 s while the shared Check
ran concurrently; this is an observed concurrent cost, not a sequential benchmark.
Its evidence is `tmp/world/terrain_execute_d44/selected-inputs-cost.json`.

Selected target `refinement-20261114T035531Z` demonstrates repeated caller cost:
public admission, adapter admission and transition preparation precede the same
native selection; its failure-control leg authenticates and prepares that
selection again. The adapter began at 04:11 UTC, initial transition preparation
at 04:25, base native work at 04:42, and the selected native Apply at 05:27.
Native Apply itself measured 546.31 s. These observed stage times include
different safety work and are not an isolated benchmark. Retain the existing
request, transport and logs under `tmp/world/end_to_end_validation/` and
`Saved/Validation/WorldPipeline/` for attribution. Diagnose the existing
canonical admission/transition owner before adding caches or changing guards.

The saved-artifact viewport replay also exposes the fresh-publish admission seam:
`refinement/app/activation.py::_publish` calls
`authority.py::validate_current_surface_selection`, which extracts base/child
archives and calls `_replay_result` / `prepare_surface_refinement`. It reconstructs
the full Ground/Water operation, shore and imports instead of using the existing
materialized result. One live publish-process observation had already consumed
301.28 s user CPU, 8.72 s kernel CPU, 2,689,838,383 read bytes and 1,394,805,311
written bytes. These are combined admission observations, not a geometry-only,
copy-only or end-to-end benchmark. Raw process-family evidence is
`tmp/world/terrain_execute_d44/viewport-diagnostic-r6/publish-process-observation.json`.
Reverse CAS skips fresh geometry replay but still authenticates both archives;
outer restoration repeats frozen validation before, during and after reversal.
Measure those boundaries separately. Do not remove fresh-admission or recovery
guards merely to reuse an old materialization.

The saved presentation replay in `tmp/world/terrain_execute_d44/presentation-comparison-r7/`
reached its strict selected audit at 09:04 UTC after an approximately 08:46 UTC
start; native cold capture finished at 09:05 UTC. It made no new physical model.
The wrapper and process logs are in `presentation-r7-inputs/`. Attribute this
observation cost before repeatedly using fresh selection publication for visual
comparisons; a future read-only observation seam must preserve authenticated
saved-byte provenance and recovery rather than bypassing fresh-admission guards.

## Work and acceptance

- Attribute current cold/no-op costs on one pinned real case before choosing a
  change; distinguish reads, validation, geometry, copying and native execution.
- Change only the measured owner through the existing preparation contract.
  Reuse authenticated unchanged results without another authority or scheduler.
- Prove exact affected dependency closure, reconstruction/no-op equivalence,
  stale/corrupt-input refusal and preservation of accepted state on refusal.
- Retain small source-faithful regressions and report total execution cost and
  changed/unchanged outputs. A geometry-only timing is not total iteration cost.

Stable owner: [combined preparation](../../../tools/World/CanonicalCompilation/terrain_refinement.md#combined-feature-preparation)
and [refinement structure](../../../tools/World/CanonicalCompilation/refinement/docs/architecture/structure.md).
