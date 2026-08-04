# 04 End-to-End Validation

Status: complete. The project-owned P0 route passes from an initially absent
environment through D0-D3, exact IoStore inspection, and distribution checks.

## Responsibility

Prove the synthetic two-cell contract first, then validate it with the minimal
pinned Kazan snapshot across source ingestion, canonical compilation, and
Unreal realization. This file owns cross-layer tests and metrics, not
implementation logic.

Synthetic ingestion, canonical compilation, and D0-D2 validation are
engine-independent. The separately owned
[engine-upgrade gate](../../tools/engine_version_update_sot.md) has passed.

## P0 Acceptance Contract

The synthetic fixture contains non-zero terrain slope and curvature across two
target cells, one diagonal road crossing their boundary, four valid buildings
including one boundary-intersecting building, one authored terrain correction
near the boundary, one authored building override, and one deliberately
invalid feature.

| Measure | Required threshold |
|---|---:|
| Full synthetic compile | 60 seconds or less |
| One-cell synthetic rebuild | 30 seconds or less |
| Canonical JSON plus reports | 64 MiB or less |
| UE synthetic import | 180 seconds or less |
| Generated UE actors and assets | 64 or fewer, with every declared valid feature realized |
| Boundary mismatches, duplicates, and owned-output orphans | 0 |
| Shared quantized height and weight-mask edge mismatches | 0 |
| Invalid fixture feature | Rejected once with no generated artifact |
| Authored terrain and building overrides | Preserved after changed input and partial regeneration |

These are guardrails for the bounded fixture, not regional performance
targets. Performance thresholds are frozen when implementation begins. A
later change requires explicit operator approval, a recorded reason, and a
fresh baseline run. Licensing, provenance, boundary correctness,
invalid-feature rejection, and authored-content protection permit no
exception.

The real-data P0 admission gate is separate from those output budgets:

| Size category | Gate |
|---|---:|
| Immutable provider payloads plus required verification metadata | 1 GiB or less |
| Ignored immutable cache | Measure separately |
| Disposable provider-normalized cache | Measure separately |
| Canonical output and reports | Measure separately |
| Generated Unreal source assets | Measure separately |
| Cooked installed prototype build | Measure separately |

Required determinism levels:

- D0: semantically equivalent features, identity, topology, and provenance.
- D1: byte-identical canonical manifests and structured reports.
- D2: byte-identical declared engine-independent artifacts under the pinned
  toolchain and platform contract.
- D3: semantically equivalent Unreal assets, actors, ownership, placement,
  references, and validation results. `.uasset` byte identity is not required.

## Required Scenarios

- [x] Before UE import, require the separate engine-upgrade build, packaging,
  plugin compatibility, and core-regression gate to pass.
- [x] Run the synthetic fixture through all layers before admitting real Kazan
  inputs.
- [x] After the synthetic proof passes, run the pinned minimal Kazan terrain,
  road, and building snapshot through all layers with no manual asset edits.
- [x] Delete every generated output and rebuild twice; require the declared
  D0, D1, and D2 comparisons to pass.
- [x] Import twice and compare D3 Unreal semantics without assuming
  `.uasset` byte identity.
- [x] Import an affected Landscape subregion without a positional crack or
  duplicate component, then prove the authored correction layer survives a
  generated-base reimport.
- [x] Regenerate only one cell and prove there are no boundary seams,
  duplicates, stale references, or orphaned generated assets.
- [x] Rebuild the eastern cell plus its required halo and prove byte-identical
  shared height and weight-mask edges, compatible border-band hashes, and an
  unchanged western core hash.
- [x] Extend source coverage and require the staged impact set to join existing
  neighbors exactly; reject an incompatible grid, algorithm, or edge before
  any accepted output is replaced.
- [x] Prove the diagonal road keeps one identity and boundary coordinate, and
  the crossing building creates one authoritative feature without a duplicate
  gameplay actor.
- [x] Change one source snapshot and prove the authored override survives its
  documented rebase path.
- [x] Inject corrupt input, missing provenance, invalid license metadata,
  invalid geometry, and an interrupted compile; require fail-closed results
  with stable diagnostics.
- [x] Generate attribution, notices, and any required source or alteration
  offer from the same provenance graph as the artifacts.
- [x] Rebuild in CI or a second pinned environment and compare the declared
  deterministic levels.
- [x] From a clean machine, use one top-level noninteractive command to
  bootstrap the pinned tools, plan, fetch, verify, decode, compile, validate
  D0-D2, invoke the Unreal commandlet, and validate D3. P0 source acquisition
  is anonymous and must not introduce provider credentials.
- [x] Prove that the clean-machine path needs no manual GIS or Unreal Editor
  work, hand-written intermediate data, or agent judgement as a pass
  condition; require a machine-readable final result and a successful rerun
  after generated outputs are deleted.
- [x] Inspect publishable roots and any cooked outputs to prove that no raw
  PBF, DEM tile, provider-normalized snapshot or cache, provider credential,
  or provider download metadata can be shipped to players.

## Results to Record

- [x] Source acquisition, verification, decoding, and snapshot time; record
  network payload, immutable cache, normalized cache, canonical output, and
  generated Unreal source asset and cooked installed build sizes separately.
- [x] Full and one-cell compile time.
- [x] Canonical output and report size.
- [x] UE import time and generated actor/asset counts.
- [x] Boundary mismatch, duplicate, rejection, and orphan counts.
- [x] Authored-override survival, impact-set promotion, shared-edge hashes,
  untouched-core hashes, and D0-D3 results.

Cook amplification, runtime memory, frame time, draw calls, streaming latency,
HLOD, navigation, replication, and representative-region quality belong to a
later measured region gate.

## Exit Gate

All implementation and clean-bootstrap gates pass through the command and
receipt contracts owned by `tools/World/EndToEndValidation/`.
