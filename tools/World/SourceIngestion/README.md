# World Source Ingestion

Owns provider admission, acquisition, verification, and provider-preserving
normalization. It does not assign ALIS feature identities, define compiler
cells, or create Unreal assets.

## Flow

```text
source profile
    -> plan and network budget
    -> content-addressed acquisition
    -> hash, rights, and coverage verification
    -> provider-preserving normalized snapshots
    -> accepted source receipt
```

## Layout

| Path | Responsibility |
|---|---|
| `contracts/` | Source profile, ledger, provider snapshot, raster, and result schemas |
| `api.py` | Public accepted-source surface consumed by Canonical Compilation |
| `app/contracts.py` | JSON contracts, hashes, atomic writes, and locks |
| `app/profiles.py` | Area identity, profile validation, and planning |
| `app/acquisition.py` | Download, cache, payload verification, and source ledger |
| `app/adapters.py` | Provider-specific normalization only |
| `app/raster_mosaic.py` | Component normalization, semantic identity, VRT coverage, and composite COG |
| `app/osm_selection.py` | Complete-geometry membership, selected-ID closure, and reference proof |
| `app/tool_io.py` | Portable external-tool JSON and artifact metadata |
| `app/run_identity.py` | Public profile resolution and exact run fingerprint |
| `app/cli.py` | Operation orchestration and structured results |
| `tests/` | Source unit and provider-tool integration checks |

## Commands

```powershell
python -S tools/World/SourceIngestion/bootstrap.py bootstrap-tools
python -S tools/World/SourceIngestion/bootstrap.py run --profile Plugins/World/ProjectWorldTestData/Data/Profiles/SourceIngestion/synthetic_two_cell.source.json
python -S tools/World/SourceIngestion/bootstrap.py run --profile Plugins/World/ProjectWorldTestData/Data/Profiles/SourceIngestion/synthetic_representative_v1.source.json
python -S tools/World/SourceIngestion/bootstrap.py run --profile Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_p0.source.json
python -S tools/World/SourceIngestion/bootstrap.py run --profile Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_representative_v1.source.json
python -S tools/World/SourceIngestion/bootstrap.py plan --profile Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_territory_v1.source.json
python -S tools/World/SourceIngestion/bootstrap.py verify --profile Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_territory_v1.source.json
python -S tools/World/SourceIngestion/bootstrap.py run --profile Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_territory_v1.source.json
python -m unittest discover tools/World/SourceIngestion/tests
```

`bootstrap.py` is the public clean-machine route. `run.py` is the internal
environment entry point.

Synthetic profiles and provider fixtures are test data owned by
`ProjectWorldTestData`. Kazan profiles are production data owned by
`ProjectWorldData`. Both are passed by explicit repository-relative path; this
tool owns their contracts and interpretation, not their instance data.

## Spatial Identity

The selected source profile is the area-of-interest SOT. Numeric `bbox`,
`crs`, and `axis_order` define exact coverage. Human `profile_id`, `area_id`,
and `label` values are routing names and never determine spatial identity.

Every source document receives a generated `area_fingerprint`. Human renames
preserve it; coordinate, CRS, or axis-order changes create a new fingerprint.
Provider feature IDs and precision remain intact for downstream compilation.
Every admitted source also records axis-specific accuracy and confidence;
unknown is stored as `null`, never converted to zero. Terrain-producing
sources require qualified vertical accuracy, which is copied through the
source ledger and raster contract so canonical cells can freeze the actual Z
evidence they sampled.
The provider adapter normalizes admitted roads, buildings, water, land cover,
vegetation areas, and explicit foliage points into stable generic classes. The
source profile owns the exact provider tag filters; these classes do not imply
an Unreal representation.

Building admission preserves both `building=*` outlines and
`building:part=*` records. The existing OSM extraction remains the sole parser
and expands its reference closure to the `type=building` relations needed to
retain explicit outline/part roles. Relation membership is normalized provider
evidence; association and fallback decisions belong to Canonical Compilation.
The concrete source profile owns its boundary selector and projected CRS. The
generic adapter contains no territory, administrative-area, or projection
constant.

Generated inputs, normalized snapshots, and receipts remain ignored under
`tmp/world/source_ingestion/`. An accepted result is written last as the
commit marker. Explicit roots are escape-checked, and every file output
records a relative path, SHA-256, and byte size.

Production profiles may declare a projected coverage contract. Planning uses
the pinned GDAL/OGR authority to inverse-project the technical envelope plus
its raster halo and source margin, samples every projected edge, and rejects a
geographic bbox shortfall. The admitted raster rectangles must cover that
complete bbox as a union; merely intersecting it is insufficient.

Multi-raster profiles require an explicit mosaic contract. Each component is
verified and normalized independently to a pixel-aligned, no-resample COG.
Admission requires zero rotation, equal signed pixel sizes, and component
origins separated from the reference origin by integer pixel offsets within a
tight tolerance. Equal absolute resolution alone is insufficient and a
misaligned component is never silently resampled.
The ordered VRT alpha band proves geometric coverage; finite elevation values
do not substitute for that proof. Component semantic hashes, component COG
hashes, the mosaic contract, and the composite authority remain distinct.

Production OSM membership starts with `osmium tags-filter` over the full
pinned provider snapshot without `-R`, so matching objects enter OGR together
with their references. Explicit OGR point, line, and multipolygon layers then
apply the exact closed-bbox intersection and produce provider IDs;
`osmium getid -r` creates their final reference closure, and
`osmium check-refs -r` proves that selected closure. Never run the OGR layers
directly against a large unfiltered PBF: per-layer accumulation can return an
incomplete result even when `ogr2ogr` exits successfully. The provider parent
extract is not required to be globally reference-complete outside the
admitted selection.

Every admitted Copernicus source freezes the exact terms identity and hash,
retrieval date, source/modified/liability notices, downstream obligations, and
decisions for cache, canonical data, Unreal assets, packaged commercial use,
and raw public-repository payloads. The plan and ledger preserve these fields.
