# Open World Generation First Slice

Status: active execution plan; blocked only on the four unchecked World Gate W0
decisions below.

## Goal

Prove the external compiler and narrow Unreal adapter across two adjacent
cells. The fixture must expose boundary behavior, provenance enforcement,
authored ownership, deterministic regeneration, and useful Unreal output
before the project expands to a representative region.

## Scope Constraints

- Use the current UE 5.7 project baseline.
- Keep canonical data and compilation engine-independent; keep the Unreal
  adapter narrow and replaceable.
- Use versioned local fixtures and outputs. Do not add PostGIS, object storage,
  remote workers, browser inspection, federation, or native content delivery.
- Do not make a global HLOD decision or depend on UE 5.8-only capabilities.
- Agent-assisted coding and validation are allowed. Unattended editor mutation
  waits until deterministic commands, validation, locking, and rollback exist.

## Decision Tasks

- [x] Pin the first slice to the repository's UE 5.7 baseline.
- [ ] Record the external compiler versus Unreal adapter ownership contract in
  the owning stable architecture docs.
- [ ] Record stable feature identity, CRS, axis order, units, vertical datum,
  compiler-cell identity, and World Partition mapping.
- [ ] Approve the source-ledger, attribution, license, and forbidden-source
  policy.
- [ ] Approve D0-D3 definitions, select required D1/D2 outputs, and pin the
  compiler/toolchain environment.

## Build Tasks

- [ ] Create one immutable fixture spanning two adjacent cells with terrain, a
  boundary-crossing road and feature, buildings, one authored override, and
  one deliberately invalid feature.
- [ ] Define the source-ledger, canonical-feature, compiler-cell manifest, and
  authored-overlay schemas.
- [ ] Implement a CLI compiler that canonicalizes inputs, assigns ownership,
  produces two cells, and never writes into authored-content roots.
- [ ] Implement structural validation for schema, stable identity, CRS, units,
  quantization, and cell addresses.
- [ ] Implement provenance, license, attribution, and forbidden-source
  validation.
- [ ] Implement geometry, topology, ownership, and boundary-consistency
  validation.
- [ ] Emit canonical manifests plus structured diff, rejection, provenance,
  attribution, timing, and size reports.
- [ ] Implement the narrow UE 5.7 import adapter without making Unreal assets
  canonical data.
- [ ] Materialize only controlled terrain, road, and building massing with
  stable source identity for both cells.
- [ ] Preserve the authored override across a changed input snapshot and
  prevent generated output from overwriting it.
- [ ] Reject the invalid feature with a stable reason and block any cell whose
  required provenance or license metadata is absent.

## Verification Tasks

- [ ] Delete outputs and rebuild twice; require D0, D1, and the selected D2
  comparisons to pass under the pinned environment.
- [ ] Regenerate only one cell; prove boundary ownership produces no seams,
  duplicates, stale references, or orphaned generated assets.
- [ ] Rebuild in CI or a second pinned environment and compare D0-D2 results;
  import twice and compare D3 semantics without assuming `.uasset` byte
  identity.

## Acceptance Metrics

Record thresholds before implementation for deterministic compile time,
partial rebuild time, canonical output size, UE import time, generated
actor/asset counts, boundary mismatches, invalid-feature rejection,
authored-override survival, and D0-D3 validation.

Cook amplification, runtime memory, frame time, draw calls, streaming latency,
HLOD, navigation performance, replication counts, and representative-region
data-quality budgets belong to World Gate W3, not this artificial fixture.

The slice passes only when all tasks are checked, every threshold is met or an
explicitly approved exception is recorded in stable documentation, and clean
regeneration requires no manual asset repair.
