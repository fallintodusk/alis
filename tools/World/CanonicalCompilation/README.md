# Canonical World Compilation

Owns the deterministic transformation from accepted provider-preserving
receipts into provider-neutral ALIS terrain and feature cells. It does not
acquire provider data or create Unreal assets.

## Flow

```text
accepted source receipt + compilation profile + authored overlay
    -> provider adapter
    -> canonical metric terrain and ALIS feature identities
    -> compiler-cell ownership and portable artifact descriptors
    -> deterministic validation and evidence reports
```

## Layout

| Path | Responsibility |
|---|---|
| `contracts/` | Canonical features, terrain, cells, coverage, reports, and results |
| `app/adapters.py` | Provider record to canonical semantic mapping |
| `app/spatial.py` | Quantization, fixture transform, grid and cell identity |
| `app/projection.py` | Pinned OGR projection and production geometry admission |
| `app/membership.py` | Exact geometry-to-cell membership and fixture differential seam |
| `app/terrain.py` | Pinned GDAL alignment plus core-and-halo cell extraction |
| `app/raster_dependencies.py` | Exact bilinear source pixels and cell-local component dependencies |
| `app/features.py` | Semantic ownership, cell representations, and overlays |
| `app/buildings.py` | Logical Building association, coverage, and effective massing volumes |
| `app/water.py` | Profile-owned water admission, grouping, width, and surface Z |
| `app/water_geometry.py` | Canonical-grid polygon sampling and visible ribbon subtraction |
| `app/incremental.py` | Impact derivation, profile growth, and accepted-base merge |
| `app/lineage.py` | Reuse compatibility, source lineage, and overlay delta derivation |
| `app/validation.py` | Structural, provenance, topology, and boundary gates |
| `app/artifacts.py` | Cell artifact storage, accepted-base reuse, and atomic promotion |
| `app/reports.py` | Provenance, attribution, rejection, validation, and diff reports |
| `app/pipeline.py` | Profile/run contracts and full or incremental orchestration |
| `app/source.py` | Public accepted-receipt consumer boundary |
| `app/control_network.py` | Geodetic control qualification and error gates |
| `app/territory_admission.py` | Cross-contract territory admission and owner confinement |
| `app/promotion.py` | Persistent canonical authority promotion and reuse |
| `tests/` | Determinism, incremental, overlay, coordinate, and failure checks |

## Commands

```powershell
python -S tools/World/CanonicalCompilation/bootstrap.py run --profile Plugins/World/ProjectWorldTestData/Data/Profiles/CanonicalCompilation/synthetic_two_cell.compile.json --dry-run
python -S tools/World/CanonicalCompilation/bootstrap.py run --profile Plugins/World/ProjectWorldTestData/Data/Profiles/CanonicalCompilation/synthetic_two_cell.compile.json
python -S tools/World/CanonicalCompilation/bootstrap.py run --profile Plugins/World/ProjectWorldTestData/Data/Profiles/CanonicalCompilation/synthetic_representative_v1.compile.json
python -S tools/World/CanonicalCompilation/bootstrap.py run --profile Plugins/World/ProjectWorldTestData/Data/Profiles/CanonicalCompilation/synthetic_landscape_water_twin.compile.json
python -S tools/World/CanonicalCompilation/bootstrap.py run --profile Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_p0.compile.json
python -S tools/World/CanonicalCompilation/bootstrap.py run --profile Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_representative_v1.compile.json
python -S tools/World/CanonicalCompilation/bootstrap.py controls --profile Plugins/World/ProjectWorldData/Data/Controls/kazan_territory_v1.control.json
python -S tools/World/CanonicalCompilation/bootstrap.py admit --profile Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/kazan_territory_v1.compile.json
python -S tools/World/CanonicalCompilation/bootstrap.py promote --profile <compiler-profile> --result <accepted-compile-result>
python -S tools/World/CanonicalCompilation/bootstrap.py authority --profile <compiler-profile>
python -S tools/World/CanonicalCompilation/bootstrap.py materialize --profile <compiler-profile>
python -m unittest discover tools/World/CanonicalCompilation/tests
```

P0 and representative compiler profiles are independent runtime SOTs. They
may share an immutable provider snapshot and grid, but they keep separate
profile identities, source receipts, outputs, and acceptance routes.

World profiles and fixture inputs belong to their production or test data
plugin, not this generic tool. Pass their repository-relative JSON path to
`--profile`. Each compiler
profile also carries an explicit repository-relative `source_profile` path and
explicit overlay path; the declared IDs must match the files they resolve to.
Coverage records `world_data_plugin`, which generic Unreal and script adapters
use to derive content/data roots from the plugin's UE descriptor.

Territory profiles additionally reference their control and budget documents.
`admit` resolves all four contracts under one owner, proves matching IDs,
CRS/datum/transform, derived cell bounds, source halo/coverage, engine origin,
control receipt, and hard source/cell ceilings. `promote` invokes the same gate
and then enforces hard measured compile ceilings; it cannot bypass admission.

Incremental compilation requires an accepted base. Terrain and feature
compatibility are evaluated independently. Multi-raster terrain cells store
only the component semantic identities touched by their core plus halo;
changed components rebuild those cells while unrelated components and all
features remain reusable. OSM snapshot/content change rebuilds or compares
features without invalidating compatible terrain. Optional terrain bounds use
the canonical CRS, and explicit provider feature IDs are authoritative precise
change scope:

```powershell
python -S tools/World/CanonicalCompilation/bootstrap.py run `
  --profile synthetic_two_cell `
  --base-result <accepted-compile-result> `
  --terrain-change-bounds 600,600,800,800
```

Use `--feature-change-id provider/id` for a known precise vector delta. Without
an explicit vector scope, changed current feature semantics are compiled and
compared against the accepted base, and only changed cell documents are
rewritten. Structural grid/compiler/source-contract changes still fail closed
to the affected layer's full rebuild. Unselected outputs are receipt-verified
and byte-reused.
Terrain and feature lineages also carry separate profile-contract hashes.
Raster sampling changes invalidate terrain; water semantics invalidate
features without coupling unrelated profile fields to both layers.
An incremental feature run always recomputes the complete target-relevant
water prepass. It merges that small result with the requested non-water delta
and semantic-diffs cell documents, so polygon/ribbon dependencies remain exact
without recompiling the complete feature estate.
Compilation writes to staging, validates the complete output, and atomically
promotes `compile_result.json` last.

Production terrain uses one pinned GDAL warped VRT over the profile's fixed
canonical alignment-cell domain and reads only selected pixel windows. Growing
the target-cell set inside that domain cannot change its transform context.
Production feature projection, validity, and exact cell intersection use pinned
OGR/GEOS. Ownership considers only cells the geometry actually intersects. The
fixture-only affine and small road-clipping primitive are guarded by
differential tests against that industrial authority.

Every terrain and feature artifact carries its compiler, execution environment,
source snapshot/result/area, raster, and overlay lineage. Feature cells carry
the stable source/generator contract identity; the complete current feature-set
semantic hash is run-level authority. A local feature update therefore does
not rewrite unrelated cell lineage, and its accepted result remains reusable
as the next incremental base. Reused cells retain their original lineage;
coverage and provenance reports expose the effective union without rewriting
the source-ledger SOT.

Each terrain cell additionally freezes the raster snapshot used for Z, its
vertical datum, source accuracy/confidence, and the half-height-quantization
sampling residual. Surface-anchor resolution consumes this cell-local record,
not feature/control provenance and not a realized Landscape actor.

P0 remains bounded to its original single-raster source. Territory profiles
use an admitted multi-raster mosaic plus cell-local actual pixel dependencies,
so one component change selects only cells whose core or halo samples touch
that component. A provider snapshot or contract change still follows the
declared layer invalidation rules rather than a filename or tile-wide guess.

Compilation profiles are the exact grid SOT. `grid_id` derives from CRS,
datum, stable vertical origin, horizontal origin, spacing, quantization, cell
dimensions, operation halos, and grid version. The vertical origin is an
explicit height-encoding and placement baseline; it is never recomputed from
the minimum height in a compilation. Compiler cell identity is independent of
acquisition bounds, place names, provider partitions, and Unreal World
Partition settings.

P0 admits one vector authority. Duplicate canonical identities reject rather
than being silently conflated. Roads, buildings, water, land cover, vegetation
areas, and explicit foliage points receive provider-neutral identities and cell
membership. A cross-cell feature belongs to the lexicographically smallest
intersecting integer `(cell_x, cell_y)` coordinate, so vertex density cannot
move ownership. Alternate sources require explicit adapters and policies
without changing the canonical grid. Linear road and water fragments must
share exact clipped endpoints. Polygonal water is stored once under its owner;
adjacent cells reference that authority and must derive the same exact
quantized segment on their common cell edge.

Canonical Building compilation terminates provider-specific 3D semantics.
Explicit valid building-relation membership wins; otherwise a part is admitted
only when exactly one outline contains it. Ambiguous and orphan parts are
diagnosed rather than guessed. A logical Building keeps its outline as semantic
identity and owns a deterministic ordered `effective_volumes` set. Each volume
contains provider-neutral geometry plus resolved `min_height_m` and `height_m`.
Complete valid part coverage selects the part volumes without also extruding the
outline. Missing parts select one qualified outline volume. Incomplete coverage
uses that same outline-only fallback when its vertical range is valid; otherwise
the logical massing is rejected. Production containment first uses a spatial
index to bound candidates and then applies exact `ST_Covers`; the index is an
optimization, not an alternate geometric authority.

Water policy is data-owned by the compiler profile. It makes exact decisions
for class, modifier, source ID, surface behavior, group membership, width, and
surface function; reusable compiler code does not infer from names or
proximity. A flowing polygon and its ordered source centerlines form one
authority before Z fitting and cell clipping. The polygon owns visible XY,
and only uncovered tails can remain as ribbons. Width resolves before geometry
suppression. Polygon authority is expanded by half the resolved width plus a
profile clearance using the pinned round-cap/round-join buffer contract; the
stored final ribbon footprint records zero polygon overlap and owns cell
membership. A cell touched only by the footprint references the same
feature-level centerline, width, and surface function; widening or narrowing
dirties the union of old and new surface cells. Width uses a valid
metric source value first, then a profile value whose accuracy is explicitly
`heuristic_visual_not_surveyed`.

Water classification and width resolution run before target pruning. Final
polygon/ribbon surfaces establish relevance, so a centerline outside the grid
can still own an edge-cell surface. Only that footprint-only case may use the
authenticated terrain halo for its Z fit; insufficient halo fails closed.
Polygon-suppressed axes keep an explicit hidden marker through this prepass and
cannot reappear as untyped raw lines.

Kazan v1 uses the operator-approved median of canonical terrain samples for
standing water. Flowing water uses 30 m axis samples, a five-sample rolling median with
shrinking endpoint windows, and a downstream non-increasing L1 isotonic fit.
Quantiles use linear R7 interpolation; an L1 tie chooses the lower elevation.
XY knots and Z are quantized by the grid contract. Production polygon
membership is rasterized onto those exact grid centers by the authenticated
GDAL toolchain; overlapping sample ownership rejects. A polygon with no grid
sample uses `PointOnSurface` only after clipping to the canonical terrain
envelope and records low-confidence derived accuracy. These are deterministic
visual geographic approximations, not surveyed water levels.

Metrics and the final result are observational and excluded from D1. The
coverage semantic hash is D0; canonical JSON and deterministic reports are
D1; declared terrain and feature artifacts are D2. The default output under
ignored `tmp/world/canonical_compilation/` is candidate or fixture storage,
not production authority. A production run must be explicitly promoted under
the owning data plugin's `Data/Canonical/` root after complete validation.
Promotion writes one deterministic content-addressed ZIP plus a small active
index. It first verifies every accepted output, then packs the deterministic
compiler outputs and normalized receipt; observational timing metrics remain
outside D1 and the bundle. Raw provider payloads are forbidden. `authority`
verifies every packed byte and current compiler-profile identity; `materialize`
authenticates the bundled admission/control/source-plan receipts and current
human-edited input hashes without rerunning old evidence through newer tools.
`materialize` restores an ignored accepted-base tree without recompiling. Cell identities,
per-cell hashes, lineage, and dirty selection stay inside the bundle, so
storage packing does not change regeneration or world semantics. Canonical ZIP
files are persistent Git LFS artifacts; the public text mirror excludes them.
