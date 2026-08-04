# 02 Canonical World Compilation

Status: complete; synthetic and pinned Kazan compilation, D0-D2 determinism,
impact-only regeneration, provenance, industrial GIS, and boundary gates pass.

## Responsibility

Own engine-independent schemas, stable feature identity, coordinate
normalization, compiler-cell ownership, authored overlays, deterministic
compilation, and canonical reports. Do not acquire provider data and do not
create Unreal assets.

## Contract

```text
source ledger + provider-normalized snapshots + authored overlays
    -> external deterministic compiler
    -> canonical ALIS JSON features
    -> two adjacent cell manifests and validation reports
```

The external compiler is a separate-process ALIS component. Provider-specific
types stop at its input adapters; the core consumes only the source-ledger,
provider-feature, and raster-layer contracts owned by step 1. Canonical JSON
is the durable contract; generated meshes and `.uasset` files are projections.

## Automation Contract

Every operation has a deterministic, noninteractive command used identically
by developers, agents, and CI. The project-owned top-level route composes the
operations without manual GIS edits or hidden agent judgement. Commands use
explicit input and output roots, stable exit codes, structured JSON results,
operation IDs, idempotent behavior, and input and output hashes. Mutating
operations support a dry run, partial acquisition or processing is resumable
where applicable, and CI mode never prompts.

Exact command names belong to the implementation surface rather than this
plan. MCP servers and skills may call the commands, but cannot own another
compiler or be the only execution route.

## Prototype Build Boundary

Raw provider bytes and provider-normalized snapshots are build-time inputs and
are never player payloads. A portable generated-artifact descriptor contains
only:

```text
artifact_id
cell_id
representation
content_hash
byte_size
schema_version
dependencies[]
provenance_result
```

P0 bakes the generated Kazan prototype into the installed game. Stock World
Partition streams its already installed cells. Future content delivery is a
separate task and is not specified here; it may consume the neutral artifact
descriptors without changing the canonical compiler.

## Canonical Terrain Grid

Freeze one versioned terrain-grid contract before real terrain compilation:

```text
terrain_grid_version
canonical_crs, vertical_datum, and stable vertical_origin_m
origin_x and origin_y
sample_spacing_x and sample_spacing_y
height_quantization and nodata_policy
resampling_method
cell_quads_x and cell_quads_y
alignment_cell_bounds
halo_samples
```

Cell bounds derive only from integer grid coordinates and this fixed origin.
Provider tile bounds and independently rounded floating-point boxes cannot
define compiler cells. Build one logical source mosaic, warp it once onto the
aligned canonical grid, then extract core cells and operation-sized halos by
deterministic pixel windows. Never reproject or resample each cell
independently. The logical raster may be virtual or chunked internally without
changing its grid, transform, resampler, or quantization contract. The declared
`halo_samples` is the minimum seam band; each operation declares any larger
required halo.

Adjacent cells share one quantized boundary sample. Each cell records
height-edge hashes and wider border-band hashes for all four sides; generated
weight masks use the same alignment and hash rule. Edge equality proves
position continuity, while the border band detects slope-affecting differences
inside the largest declared processing kernel. For example, a western cell's
east edge must be byte-identical to its eastern neighbor's west edge.

## Incremental Regeneration

A change rebuilds only the source-intersecting cells expanded by the declared
algorithm halo and the owners of affected cross-boundary features. Generation
writes to staging, validates schemas, provenance, grid compatibility, edge and
border-band hashes, and neighbor references, then atomically promotes the
accepted impact set. Untouched core and artifact hashes must remain unchanged.

The accepted base is reusable only when its grid, algorithm, identity,
compiler, execution environment, source semantics, immutable snapshots, and
raster authority remain compatible. Profile growth creates missing cells and
updates neighbor manifests while preserving existing core artifacts. The
compiler derives profile-growth scope and old/new overlay deltas; it uses
source receipts to accept bounded acquisition growth or force a full rebuild,
not to infer a general provider-feature or raster delta. Operator flags only
request additional work. When an accepted source envelope grows, the compiler
recompiles canonical vector inputs for all old and new cells, compares accepted
feature documents, and rewrites only changed feature cells. Compatible terrain
remains impact-only. Every cell records separate terrain and feature lineage.

Reject before promotion when a neighbor disagrees on terrain-grid version,
source contract, vertical datum, quantization, algorithm version, shared edge,
or required border band. Visual blending cannot turn an invalid boundary into
an accepted one.

## Tasks

- [x] Record compiler versus Unreal-adapter ownership in the owning stable
  architecture documents.
- [x] Define stable feature IDs, canonical CRS, axis order, units, vertical
  datum, quantization, and explicit mappings from source partitions through
  compiler cells to artifacts. Define the receipt boundary consumed by the
  later Unreal adapter; step 3 owns actual World Partition cell mapping. Derive `grid_id` from
  the frozen terrain-grid contract and identify cells by `grid_id` plus integer
  x/y. Coverage manifests associate ingestion `area_fingerprint` values with
  cell IDs; acquisition-envelope growth must preserve overlapping cell IDs.
- [x] Define and freeze the canonical terrain-grid contract, including fixed
  horizontal and vertical origins, spacing, cell quads, quantization, nodata,
  resampling, and per-operation halo requirements.
- [x] Consume the versioned ingestion contracts and define canonical-feature,
  cell-manifest, coverage-manifest, authored-overlay, rejection, provenance,
  and attribution schemas without duplicating the source-ledger SOT.
- [x] Include `$schema` in every committed JSON fixture and manifest.
- [x] Implement the external CLI and one top-level noninteractive route under
  the automation contract, including machine-readable final results.
- [x] Transform into the canonical CRS and units, assign ALIS identities, and
  normalize and validate provider semantics. P0 has one vector
  authority, so identity collisions reject instead of silently conflating.
- [x] Compile terrain, one boundary-crossing road, and small building massing
  into two adjacent cells without writing into authored roots.
- [x] Build one aligned logical terrain raster and extract core-plus-halo cells
  by integer pixel windows without cell-local reprojection or resampling.
- [x] Assign one deterministic owner to every boundary-crossing feature and
  preserve references from neighboring cells. Split road representations use
  one parent ID and one canonical boundary coordinate; a building has one
  authoritative owner even if render fragments cross a boundary.
- [x] Keep authored corrections as overlays and define rebase behavior when a
  source snapshot changes.
- [x] Implement separate structural, provenance, geometry, topology,
  boundary, and determinism validators.
- [x] Compute four edge and border-band hashes per terrain and weight-mask
  artifact, then implement staged impact-set validation and atomic promotion.
- [x] Reject the deliberately invalid feature with a stable reason and block
  any output whose rights or required provenance are unresolved.
- [x] Emit canonical cell and coverage manifests plus diff, rejection,
  provenance, attribution, timing, and size reports. The same coverage
  contract must scale from the Kazan fixture to the later Tatarstan envelope.
- [x] Emit portable generated-artifact descriptors without platform delivery
  fields and prove that raw provider payloads cannot enter cooked content.
- [x] Prove the D0, D1, and D2 levels owned by
  [04 End-to-End Validation](04_end_to_end_validation.md).

## Exit Gate

Deleting outputs and compiling the same pinned inputs twice produces two valid
canonical cells with the declared deterministic results, clean shared
boundaries, preserved authored overlays, and complete provenance.

Result: passed. The stable implementation and contract router is
[`tools/World/CanonicalCompilation/README.md`](../../../../tools/World/CanonicalCompilation/README.md).
The current ignored receipts are under
`tmp/world/canonical_compilation/runs/<profile_id>/<run_inputs_hash>/run/`.

- Synthetic full compile: 294 ms, 52,471 bytes, two cells, five accepted
  features, and one stable rejection.
- Synthetic eastern terrain change: 316 ms; one terrain cell processed, zero
  vectors processed, and the western artifacts reused byte-for-byte.
- Synthetic profile growth added a third adjacent cell while processing one
  terrain cell and zero unrelated vectors; both existing terrain and feature
  cores remained byte-identical and the new shared edge passed.
- Synthetic accepted-source envelope growth verified all six vector inputs,
  rebuilt the changed old feature cell and new cell, and kept old terrain plus
  the unaffected old feature cell byte-identical.
- Pinned Kazan compile: 3,209 ms, 2,494,903 bytes, two cells, 1,065 accepted
  road/building features, 65 stable geometry rejections, and no provenance or
  boundary failure.
- Pinned Kazan eastern terrain change: one terrain cell processed, zero vectors
  processed, and the accepted western cell reused without boundary drift.
- Two isolated synthetic rebuilds produced equal D0 semantic hashes, 13 equal
  D1 documents, and equal D2 artifact hashes.
- Two isolated Kazan builds on the Landscape-compatible grid produced 13 equal
  deterministic documents; only the observational metrics report differed.
- World architecture, environment, ingestion, and compiler tests: 58 passed,
  0 skipped.
