# Compile the Kazan Territory - Slice 2

Status: Done. R1, R2, the coherent L2 boundary, and the corrected canonical
promotion passed on 2026-08-13. Slice 0 and Slice 1 remain closed at base
`3daa8e56d`.

## Read first

- [Architecture and observability](../../../Plugins/World/ProjectWorld/docs/architecture_overview.md)
- [Territory contract and routes](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Canonical compilation commands](../../../tools/World/CanonicalCompilation/README.md)
- [Source ingestion commands](../../../tools/World/SourceIngestion/README.md)
- [World pipeline test layers](../../../docs/testing/world_pipeline_layers.md)
- [Historical evidence and later-slice outline](../../02_backlog/world/world_generate_kazan_territory_roadmap.md)

Do not import the historical roadmap unless investigating an old decision.
Use the linked component README for command syntax; this todo owns only the
current execution order and exit state.

## Frozen invariant routes

| Boundary | Authority | Slice 2 treatment |
|---|---|---|
| Territory, grid, GeoReferencing, and error budget | [Kazan v1 contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#territory-envelope-v1-operator-decision-2026-08-10) plus the compiler profile linked there | Consume unchanged. |
| Source, licence, coverage, and controls | ProjectWorldData source/control profiles linked by the compiler profile | Verify and consume unchanged. |
| Canonical output, persistence, and budgets | [Canonical Compilation](../../../tools/World/CanonicalCompilation/README.md) plus the ProjectWorldData budget profile | Extend within the accepted contract. |
| Current implementation scope | This todo sections 2B-2C | Compile the admitted geography layers; no alternate provider or automatic building conflation. |
| Exit evidence | This todo section 2D | Equal D0-D2 full compiles, bounded incremental scope, complete seam/provenance/rejection evidence. |
| Unreal | [Delivery stage 3](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#3-generated-unreal-geography) | Not entered in Slice 2. |

If implementation would change a stable authority, stop for an operator
decision and update that SOT first. Measurements may tighten a ceiling but
cannot silently redefine acceptance.

## Reviewer checkpoints

### R1 - Pre-code design review

Prepare the Slice 2 design against the frozen contracts, then give the
reviewer this file, the architecture overview, the relevant owner docs, and
the diff from `3daa8e56d`. Review only identities, mutable inputs, lineage,
seam rules, retry/rollback, concurrency, persistence, and hard budgets. R1
closes when the design has no unresolved production-path blocker; it does not
require Matrix, Unreal, or package evidence.

#### R1 proposal - multi-raster and vector authority

Status: accepted by the external reviewer on 2026-08-12. The design extends
the existing v1 fixture path; it does not add another activation authority or
change the frozen grid.

Observed production inputs:

| Fact | E048 | E049 |
|---|---|---|
| Original identity | `copernicus_glo30_2021_n55_e048` plus full pinned SHA-256 | `copernicus_glo30_2021_n55_e049` plus full pinned SHA-256 |
| Raster contract | EPSG:4326, Float32, `AREA_OR_POINT=Point`, EGM2008 | Same |
| Size | 2400 x 3600 | 2400 x 3600 |
| Pixel step | 0.0004166666667 x -0.0002777777778 degrees | Same |
| Pixel footprint | 47.9997916667..48.9997916667 E | 48.9997916667..49.9997916667 E |

The measured footprints meet at one edge with no positive-area overlap or
gap. The admitted geographic source envelope is
`[48.9838, 55.73314, 49.22434, 55.86052]`. Its independently derived required
envelope is
`[48.9838100991, 55.7331493663, 49.2243252989, 55.8605141517]`, from the
technical bounds plus the 30 m terrain halo and 350 m source margin.

##### 1. Identities

| Identity | Exact basis | Consumer |
|---|---|---|
| Original snapshot | Source ID, provider/dataset/release, full verified payload hashes, bytes, CRS/datum, accuracy, licence | Source ledger |
| Normalized component ID | Original snapshot ID/hash, requested clip, realized pixel footprint/transform/size, semantic normalization contract version, datum and accuracy | Mosaic manifest |
| Component sample semantic hash | Canonical row-major Float32 samples plus exact transform, size, and nodata semantics | Mosaic authority and cell terrain dependency |
| Component artifact hash | Exact normalized COG bytes | Source result and provenance evidence only |
| Mosaic contract ID | Contract version, geographic clip authority, component ordering, grid-compatibility rules, overlap/nodata policy, output format, and no-resample rule | Every terrain cell and mosaic manifest |
| Mosaic authority ID | Mosaic contract ID plus ordered `(component_id, sample_semantic_sha256)` pairs | Source result, provenance report, promotion evidence |
| Cell terrain dependency | Mosaic contract ID plus only the ordered `(component_id, sample_semantic_sha256)` pairs touched by that cell's exact bilinear sample kernels | Cell terrain lineage and dirty selection |

Component order is ascending `(west, south, east, north, source_id,
snapshot_id)`, which is E048 then E049 here. The semantic sample stream is a
UTF-8 canonical-JSON header for transform/size/nodata followed by LF and
row-major IEEE-754 little-endian Float32 values; negative zero normalizes to
positive zero and non-finite values reject. Artifact SHA-256 remains separate
so semantic and physical determinism are independently auditable.

The full mosaic authority ID is deliberately not a stable field in every cell.
Doing that would make an E048-only content change invalidate E049-only cells.
Cells reference the composite layer through `mosaic_contract_id` and their
exact component set; run/report authority carries the full mosaic ID.

##### 2. Component verification and deterministic mosaic

For each original, independently and before combination:

1. Verify pinned byte size, SHA-256, and provider MD5/ETag evidence.
2. Read metadata with the pinned GDAL toolchain and require one Float32 band,
   EPSG:4326, no rotation, `AREA_OR_POINT=Point`, admitted footprint, raster
   class, EGM2008 datum/metres, and the pinned accuracy/confidence.
3. Require every component to have equal resolution/alignment, datum, unit,
   raster class, accuracy/confidence, and compatible licence obligations.
4. Clip the component to the pixel-aligned intersection of its footprint and
   the accepted geographic envelope. Record requested and realized bounds.
5. Compute the canonical sample semantic hash and reject nodata, NaN, or
   infinite values in covered samples.

Build one temporary VRT from the ordered component COGs with pinned
`gdalbuildvrt -strict -resolution same -addalpha`. Scan the actual VRT alpha
band and require value 255 for every mosaic-grid pixel intersecting the
accepted geographic envelope. Alpha 0 proves a coverage gap even if GDAL would
otherwise expose a finite zero elevation. After coverage acceptance, copy only
the elevation band without resampling to one composite COG using fixed
single-threaded creation options. Add
`gdalbuildvrt.exe` and its SHA-256 to the existing toolchain lock; it is already
in the pinned GDAL 3.11.5 distribution. The VRT prevents the per-input edge
behavior of warping tiles independently. Bilinear reprojection happens once,
later, from the composite COG to the fixed EPSG:32639 canonical grid with
`-et 0`.

Behavior authority: [GDAL VRT mosaic](https://gdal.org/en/stable/programs/gdalbuildvrt.html)
and [GDAL multi-input warp](https://gdal.org/en/stable/programs/gdalwarp.html#multiple-input-files).

Zero-area shared edges are valid. Positive-area overlap is valid only when the
aligned Float32 samples are byte-equal; otherwise reject. There is no
last-input-wins ambiguity. Coverage comes only from the constructed VRT alpha
mask, never from finite elevation values. The compiler separately rechecks
that every requested core plus halo sample is finite.

Two isolated source runs are equivalent only when the component manifests,
mosaic authority ID, component/composite sample semantic hashes, composite COG
SHA-256, raster metadata, coverage/validity counts, OSM feature shards, and
accepted source result are equal. Same component ID with different semantic
samples is a semantic determinism conflict. Same component ID and semantic
hash with different COG bytes is an artifact determinism conflict. Timing and
temporary paths are not identity inputs.

##### 3. Canonical terrain provenance

Territory output uses a versioned multi-raster lineage contract while current
single-raster accepted fixtures remain readable. The territory cell manifest
separates:

- terrain lineage: compiler/grid contract, mosaic contract ID, exact raster
  component dependencies, datum/accuracy, source area, and terrain overlay;
- feature lineage: vector decoder contract, OSM snapshot, source area, and
  feature overlay. DEM changes cannot invalidate feature artifacts;
- run provenance: current source-result hash, full mosaic authority ID and
  manifest hash, all component records, toolchain, and implementation identity.

The compiler derives a cell's component set from every canonical sample in its
core plus declared terrain halo. It inverse-transforms the sample with the
same exact transform authority used by the warp. Territory compilation pins
GDAL `XSCALE=1.0` and `YSCALE=1.0`; under that frozen unit scale it expands a
sample to the four source pixels used by bilinear interpolation and maps them through the
mosaic component rectangles. A seam cell may therefore name both E048 and
E049; an E049-only cell cannot silently depend on E048. Without the frozen
unit warp scale, GDAL may widen a downsampling kernel, so a generic four-pixel
claim is not accepted.

`reports/provenance.json` and a schema-valid copy of the exact mosaic manifest
carry the global authority and component inventory. Cell manifests carry the
cell-local subset. Promotion bundles those JSON documents and hashes; it does
not bundle raw provider or normalized raster payloads. This makes the accepted
canonical bundle sufficient to audit and materialize without the source cache.

##### 4. OSM selection and identity

The source-membership authority is the accepted geographic envelope above.
The canonical-selection authority is the fixed EPSG:32639 technical bounds
and 210-cell selection. The geographic envelope is intentionally slightly
larger so crossing features and resampling have admitted context.

Run `osmium tags-filter` without `-R` over the full pinned Geofabrik PBF so
admitted objects and their references form the candidate source. Decode its
complete geometries through the explicit OGR `points`, `lines`, and
`multipolygons` layers. Apply admitted tag membership and exact
closed-envelope `Intersects` selection with the pinned OGR/GEOS authority and
retain each matching provider ID. Recursively
close those exact IDs against the full snapshot with `osmium getid -r`, run
`osmium check-refs -r` on the selected closure, and export complete geometry.
This
finds a segment that crosses the envelope even when both endpoint nodes are
outside it. It must not geometrically cut source members at the source
boundary; canonical projection/intersection later clips representation to
selected cells.

The Geofabrik parent extract is not globally reference-complete at unrelated
outer boundaries, so global `check-refs` is not a valid admission gate. The
exact selected closure is the fail-closed proof. Behavior authority:
[recursive ID closure](https://docs.osmcode.org/osmium/latest/osmium-getid.html),
[reference validation](https://docs.osmcode.org/osmium/latest/osmium-check-refs.html),
and [complete geometry export](https://docs.osmcode.org/osmium/latest/osmium-export.html).

Provider identity remains original OSM `type/id` plus version, timestamp and
changeset provenance. Shard order and canonical ownership are deterministic;
duplicate provider IDs with conflicting properties or equal-rank geometries
reject. Scope growth creates a new accepted source result. With the same OSM
snapshot, the compiler compares the complete current feature set and rewrites
only changed/new cell documents. A changed OSM snapshot rebuilds all feature
artifacts but does not rebuild terrain when its raster dependencies are equal.
Any later bounded pre-extract is only an optimization and must prove the same
selected provider-ID/geometry hashes as this full-snapshot membership path.

##### 5. Persistence and authority state

```text
verified content-addressed provider cache
  -> source staging/<pid>
  -> accepted source run_result.json (candidate evidence only)
  -> compiler staging/<pid>
  -> accepted compile_result.json (candidate evidence only)
  -> deterministic JSON-only canonical ZIP
  -> ProjectWorldData/Data/Canonical/kazan_territory_v1/active.json
  -> ignored materialized accepted base
```

Source candidates stay under
`tmp/world/source_ingestion/runs/<profile>/<inputs_hash>/`; compiler candidates
stay under `tmp/world/canonical_compilation/runs/<profile>/<inputs_hash>/`.
Only the content-addressed ZIP and small active index under ProjectWorldData
are durable canonical authority. There is no active mosaic pointer.

Retry of the same identity verifies and adopts byte-equal accepted output.
Different semantics or bytes at the same applicable identity are classified
as the semantic/artifact determinism conflicts above and never overwrite. A
new immutable bundle may exist without becoming active; only the validated
atomic replacement of `active.json` commits authority.

##### 6. Failure, retry, and concurrency

| State | Lock | Failure/retry rule |
|---|---|---|
| Provider download | Payload SHA-256 | Resume `.partial`; verify, then atomic replace |
| Source normalization/mosaic | Source run-input hash, outside final output root | Unique staging; validate all outputs; accepted result last; remove/adopt staging on retry |
| Canonical compile | Compiler run-input hash | Unique staging; validate full output; compile result last; old accepted base is read-only |
| Canonical promotion | Profile/owner | Validate staged bundle and candidate active record; active index commits last |
| Materialization | Bundle SHA-256 | Extract to staging, validate, atomic replace; identical existing result is reused |

An interrupted source or compile leaves no accepted result. An interrupted
promotion may leave an immutable unreferenced bundle, which retry verifies and
adopts; the prior active index remains valid. No cleanup may delete the old
active bundle or accepted base during candidate work.

##### 7. Incremental dirty scope

The dirty unit is one canonical cell per layer. Terrain and features are
selected independently.

| Change | Required work |
|---|---|
| E048 bytes change; same footprint/grid/datum/contract | Rebuild terrain cells whose stored/current bilinear dependency set includes E048; seam cells include the declared terrain halo; reuse E049-only terrain and every feature artifact |
| E049 bytes change | Symmetric rule |
| Additive component/source-area growth | Build new cells and any old cells whose exact component dependency set changes; compare current vector documents for old cells |
| Component removal, coverage gap, datum/grid/mosaic-contract change | Reject incomplete authority; otherwise full terrain rebuild after explicit compatible contract change |
| OSM snapshot changes | Rebuild all feature artifacts; reuse terrain |
| Authored overlay changes | Use the existing precise terrain bounds/provider-ID impact rules |
| Grid, quantization, algorithm, or compiler semantic contract changes | Full rebuild |

Source growth and content change are distinct receipt states. Growth changes
the admitted area/component inventory and target selection; content change
keeps geometry/contract stable but changes a component hash. The incremental
proof perturbs E048 in a multi-raster fixture and requires the derived dirty
set exactly, byte-identical E049-only cells, halo inclusion at the seam, and
canonical terrain/feature/coverage content equal to a clean current compile.
Incremental-only run/diff evidence may differ from the clean run by design.

##### 8. Hard budgets and exit receipts

| Gate | Authority | Enforcement |
|---|---|---|
| Source bytes | Accepted plan: 823,461,438 bytes | Preflight/admission, hard max 1 GiB; downloaded bytes and hashes then verify exactly |
| Cells | Compiler selection: 210 | Preflight/admission and measured compile, hard max 210 |
| Features | Compiler metrics | Post-compile/promotion, hard max 100,000 |
| Canonical bytes | Deterministic JSON inventory | Post-compile/promotion, hard max 2 GiB |
| Full compile time | Observational metrics | Post-compile/promotion, hard max 900 seconds; excluded from D1/bundle identity |

Required source evidence: plan/coverage, source ledger, per-component
verification records, mosaic manifest and validity/seam receipt, OSM extraction
contract plus reference check, output inventory, and accepted source result.

Required compiler evidence: run contract, per-cell lineage/dependency map,
coverage and semantic hash, seam/topology checks, provenance, rejections,
incremental diff, metrics, and complete output inventory. Two isolated full
compiles must have equal D0 semantic hash, equal D1 deterministic JSON hashes
excluding observational metrics/final receipt, and equal D2 declared artifact
hashes. Their normalized promotion bundles must also be byte-equal. No Matrix
belongs to R1. At Slice 2 exit, run only existing L2 Matrix profiles selected
by `plan`; there is no territory Matrix, L3 enrollment, or L4 packaging in
Slice 2.

Implementation order after R1 acceptance:

```text
verify E048 and E049 independently
  -> write component records inside one staged source run
  -> build and validate one deterministic mosaic authority
  -> build relation-complete OSM selection
  -> commit accepted source result
  -> compile and prove 210 cells
  -> promote one canonical bundle
  -> materialize and authenticate the accepted base
```

### R2 - Integrated exit review

After 2B-2C and focused L0/L1 proofs, give the reviewer the updated checklist,
implementation diff, and exact compile/promotion/materialization evidence.
Review the complete lifecycle once. Fix defect classes before the one final
affected L2 boundary; do not alternate reviewer comments with Matrix runs.

After acceptance, move this file unchanged except for final evidence to
`todo/01_done/world/`, then create the Slice 3 active file. Do not pre-create
future active slice files.

## Execution

### 2A - Boundary preflight

- [x] Operator accepted the reviewed Slice 0/1 closure. Base `3daa8e56d`
  isolates it; the later Mermaid UX edit may remain uncommitted through R1.
- [x] Planner from base `3daa8e56d`: 11 documentation/todo paths, L0 only;
  no Matrix, L3 candidate, or L4 route selected before Slice 2 code.
- [x] Confirm this slice performs no generated-world enrollment. The one-command
  re-enrollment hardening is a Slice 3 precondition after real territory Matrix
  inputs exist; do not invent that evidence contract here.
- [x] R1 accepted: identities, mutable inputs, lineage, retry/rollback,
  concurrency, and budgets are frozen above. Do not reopen without a concrete
  implementation contradiction.

### 2B - Source authority

- [x] Extend the raster manifest to both admitted tiles; verify each original
  independently and reject gaps, unresolved nodata, inconsistent overlap, or
  mixed vertical datums.
- [x] Build the deterministic mosaic/resample authority. Derive its identity
  from ordered component identities/hashes, datum, accuracy/confidence, and
  the frozen mosaic contract; retain component lineage. Reject rotation,
  signed-resolution disagreement, and non-integer pixel-phase offsets.
- [x] Differentially prove the derived bilinear source-pixel dependencies
  against actual pinned-GDAL samples: E048-only, E049-only, both seam sides, a
  seam-crossing cell halo, and an edge/corner. Perturb one E048 source pixel;
  require every affected cell and no unrelated E049-only cell.
- [x] Select whole admitted OSM geometries against the geographic source
  envelope with reference closure and provenance intact; canonical compilation
  then intersects the exact EPSG:32639 technical cell envelope.

### 2C - Canonical territory

- [x] Compile every selected cell for terrain, water, land cover, vegetation,
  roads, and building footprints within the hard Slice 1 budgets.
- [x] Freeze quality cells for dense urban, riverbank, suburban, sparse edge,
  and a cross-cell boundary.
- [x] Prove road/water seam topology, stable provider identity, full reports,
  source growth, and a one-cell incremental rebuild. Water evidence separates
  exact linear endpoints from shared quantized polygon boundary segments.
- [x] Keep complete feature-population semantics in run authority and stable
  source/generator semantics in cell lineage. Prove full -> local feature
  change -> no-op reuse -> second local feature change.
- [x] Promote only an accepted deterministic bundle through the existing
  canonical authority command; materialization must reproduce it without the
  provider cache or current compiler implementation.

### R2 evidence - 2026-08-12, final proof 2026-08-13

| Proof | Accepted evidence |
|---|---|
| Source | Inputs `93062ed11643...`; 2 aligned raster components; 265,880 covered pixels, 0 uncovered; 87,750 OSM features; 201.9 s |
| Mosaic | Contract `f4de4b5e808e...`; authority `580826983379...`; exact realized grid 578 x 460 |
| Full compile | Inputs `0f6f7631882a...`; shared grid `grid_413718bc833994e5`; 210 cells; 67,169 features; 169,605,090 canonical bytes; isolated runs 63.8 s and 64.4 s |
| Seam/quality | 391 terrain neighbor comparisons; 5,879 multi-cell features; 3,789 road seams; 28 linear-water seams; 175 polygon-water seams; 0 mismatches; all 5 frozen quality roles accepted |
| D0 | Both isolated full compiles: `e101ca2ea9264fff4a739195fa3cbe7ad39185a8d8787d66ad06983e5d801f54` |
| D1 | All 637 deterministic JSON output hashes equal |
| D2 | All 420 declared terrain/feature artifact hashes equal |
| Bundle equality | Both isolated normalized promotion bundles are byte-equal: SHA-256 `2d2eed8005a89f75f59c1b73a27174d24b47e4d3fcb6f71ca0e45bb9c3a28811` |
| Incremental | Production chain: one terrain cell rebuilt and 209 reused, then no-op from that accepted result rebuilt 0 and reused 210; D0 unchanged and 0 features processed. Focused feature chain proves two successive local semantic changes. |
| Tests | Source Ingestion 33/33 and Execution Environment 9/9 on 2026-08-12; corrected chained-incremental regression 1/1 in 1.008 s, then Canonical Compilation 48/48 in 19.989 s on 2026-08-13 |
| Persistent authority | `kazan_territory_v1:f4a4f6fa...`; deterministic ZIP SHA-256 `2d2eed80...`, 10,184,678 bytes, 637 outputs |
| Materialization | Authority validated and materialized to the bundle-SHA accepted-base path; all outputs authenticated without provider or compiler rerun |

No Check, Matrix, generated-world enrollment, Unreal build, or packaging was
run before R2. Those gates remain ordered by section 2D.

### 2D - Coherent boundary

- [x] System review after focused tests: audit the whole lifecycle classes,
  not one reviewer finding at a time (provenance, ownership, atomicity,
  concurrency, persistence, and recovery).
- [x] Run `plan` again, then one fresh common Check and only the Matrix runs it
  selects. Do not run L3 generated-world enrollment or L4 packaging.
- [x] Exit gate: two isolated full compiles have equal D0-D2 evidence; bounded
  change rebuilds only proven scope; provenance, rejection, budget, coverage,
  and seam evidence are complete; accepted authority materializes cleanly.

### L2 evidence - 2026-08-13

| Proof | Accepted evidence |
|---|---|
| Static plan | Base `3daa8e56d`; P0 and representative selected; no L3 owner and no L4 route |
| Common Check | `check-20260813T075357Z`; 7/7 suites; contract `e1d9a9aeecb1...`; 177.8 s |
| P0 Matrix | `run-20260813T075709Z`; profile `4118539ea42b...`; Kazan 1,074 features; D0 `94d8259911b9...`; generated tree restored |
| Representative Matrix | `run-20260813T080254Z`; profile `22d5d2be483a...`; route `cross_cell_footway`; 158.21 m navigation; 2 collision and 2 orientation probes; D3 `4f00e6c32a30...`; generated tree restored |
| OSM regression | Direct large-PBF OGR layer reading had silently omitted `way/91641075`; full-snapshot `osmium tags-filter` reference closure before exact OGR intersection restored it. Focused crossing proof 1/1 and Source Ingestion 33/33 passed before the fresh Check. |

- [x] Re-run the accepted territory source and two isolated full compiles with
  the corrected membership contract; require complete D0-D2 and budget/seam
  evidence, then promote and materialize one deterministic canonical bundle.
- [x] Prove the corrected active bundle contains `way/91641075`, validates
  without provider/tool recomputation, and is the only active authority before
  archiving Slice 2.

## Iteration cadence

```text
many edits -> exact L0 tests
real cross-component seam -> L1 once
implementation stable -> system review -> one affected L2 boundary
accepted canonical bytes -> canonical promotion once
Unreal/package work -> later slice only
```
