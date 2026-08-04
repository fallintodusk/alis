# 01 Source Ingestion

Status: complete; the isolated Python bootstrap, full test suite, and pinned
Kazan run pass from the accepted hash-locked environment.

## Responsibility

Own provider selection, legal admission, acquisition, content hashing,
immutable snapshots, provider-format decoding, and provider-normalized
snapshots. Preserve provider IDs, schema, CRS, datum, precision, and terms.
Do not convert to the canonical CRS, conflate features, assign ALIS identity
or compiler-cell ownership, or create Unreal assets.

## Real-Data P0 Source Set

| Need | Required source | Admission state | Boundary |
|---|---|---|---|
| Terrain | [Copernicus DEM GLO-30](https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM) | Pinned anonymous AWS Open Data COG distribution; release, bytes, SHA-256, CRS, datum, and notices recorded | DSM baseline; preserve the required original or modified-output notice |
| Roads, buildings, and envelope boundary | [Dated Geofabrik Volga Federal District OSM PBF](https://download.geofabrik.de/russia/volga-fed-district.html) | Pinned dated PBF; provider MD5 and ALIS SHA-256 verified | ODbL source and attribution graph; clip Kazan and Tatarstan without treating provider partitions as compiler cells |

The synthetic fixture has no provider dependency. Real-data P0 admits exactly
these two source classes; live Overpass is discovery-only.
Land cover, imagery, alternate building datasets, water, and vegetation remain
optional adapters and cannot become hidden core prerequisites.

Live probe counts and deferred WorldCover, water, vegetation, and Overture
candidates remain evidence in the [research audit](research_audit.md#source-discovery-evidence);
they are not admitted P0 inputs. Google Maps, Google Earth, Street View,
scraped imagery, unknown-license data, and NonCommercial or NoDerivatives
inputs remain forbidden by the
[world-data policy](../../../../docs/legal/world_data_and_asset_policy.md).

## P0 Input Budget

Real-data P0 may fetch at most 1 GiB of immutable provider payloads and the
provider metadata required to verify them. The admission plan uses one dated
Volga PBF plus the minimum set of GLO-30 1 degree by 1 degree tiles
intersecting the pinned Kazan area of interest. The exact plan selects one tile
and 793,803,271 provider bytes, below the 1 GiB gate.

Measure network payload, ignored immutable cache, disposable normalized cache,
canonical output, generated Unreal source assets, and the cooked installed
build separately. Only the first category is the 1 GiB gate. If the admitted
pair exceeds it, fail planning rather than use live Overpass. A smaller
developer fixture may be derived deterministically
from the pinned Volga PBF only with its parent hash, clipping boundary, tool
version, exact command, and ODbL redistribution obligations recorded. If that
fixture is shared, distribute it under the applicable ODbL terms with the
parent evidence and recipe.

## Inputs and Outputs

```text
official provider release
    -> ignored download cache
    -> verified immutable source snapshot
    -> source ledger plus provider-normalized snapshot
```

This layer outputs provider-preserving records and rasters/vectors. Canonical
ALIS JSON begins in step 2.

Each provider adapter exposes only four operations:

```text
Plan(area, release) -> required files, expected bytes, auth mode, notices
Fetch(plan)         -> immutable cached files
Verify(snapshot)    -> hashes, provider metadata, and policy result
Decode(snapshot)    -> provider-feature or raster-layer records
```

The common record envelope preserves provider, release, source identity,
geometry or raster metadata, CRS or datum, confidence, and provenance. Each
adapter retains its remaining properties in a versioned provider payload; the
compiler does not depend on provider-specific fields.

## Tasks

- [x] Create the synthetic boundary-crossing fixture used before real-data
  admission.
- [x] Define versioned source-ledger, provider-feature, and raster-layer input
  contracts needed by the stable provenance policy. Provider adapters own raw
  field mappings; the compiler core must not expose provider-named types.
- [x] Select the exact Kazan two-cell area of interest without making provider
  partitions or human place names equal spatial identity. Generate its
  fingerprint from the numeric coordinate contract, and pin the versioned
  Tatarstan demonstration boundary from the admitted snapshot.
- [x] Pin the Copernicus acquisition route, authentication mode, release,
  minimum intersecting tile set, official URLs, hashes, CRS, vertical datum,
  terms, and required notice.
- [x] Pin a dated Volga Federal District PBF and provider checksum, compute the
  ALIS SHA-256, then deterministically clip the Kazan and Tatarstan envelopes;
  record identifier, terms, attribution, and clipping parameters.
- [x] Plan the exact real-data download before fetching and fail if the dated
  PBF, required DEM tiles, and verification metadata exceed 1 GiB.
- [x] Support anonymous acquisition for both P0 sources without introducing
  provider credentials into builds, source records, logs, or assets.
- [x] Record an ALIS-use decision for commercial transformation, generated
  artifact publication, source or alteration offers, and reproducible
  developer acquisition.
- [x] Admit and pin an exact `osmium-tool` release as a separate GPLv3
  command-line executable for OSM and an exact GDAL/PROJ command-line bundle
  for raster metadata inspection, clipping, and deterministic conversion.
  Record versions, binary hashes, dependency terms, and notices; do not link
  the tools into Unreal or mirror their payloads by default. Do not add
  DuckDB, PyArrow, Overture, PostGIS, or a hosted GIS dependency to P0.
- [x] Keep original PBF and raster tiles in an ignored immutable cache, verify
  hashes, and make every normalized performance cache disposable and
  reproducible from those bytes.
- [x] Decode provider formats without dropping provider IDs, schema, precision,
  confidence, CRS, datum, or license metadata and without canonical
  transformation.
- [x] Use relation-complete smart extraction for AOI multipolygons and prove a
  boundary-crossing building with inner and outer ways retains its provider
  relation identity.
- [x] Validate profiles and all emitted JSON before promotion, keep artifact
  paths relative to explicit roots, and emit structured failures.
- [x] Address source caches and tool installs by complete content hashes,
  serialize concurrent materialization, disable external GDAL plugins, and
  inventory the executed runtime dependency terms.
- [x] Bootstrap the isolated Python runtime from one version-, hash-, platform-,
  and licence-locked dependency file before importing third-party packages.
- [x] Derive run identity from the source, tool, Python, schema, and
  implementation inputs; isolate operations under that hash and publish the
  accepted result last as their commit marker.
- [x] Put relative, escape-checked COG identity directly in the raster-layer
  contract and hash every file output in the operation result.
- [x] Emit provider features through a shard manifest; P0 uses one shard while
  preserving a stable scale-out contract.
- [x] Reject forbidden, ambiguous, stale, corrupt, or out-of-area inputs with
  structured reasons.
- [x] Contract-test each adapter with pinned synthetic and small real fixtures,
  corrupt bytes, wrong hashes, unsupported releases, missing license metadata,
  out-of-area inputs, and deterministic repeated decode.

## Exit Gate

The synthetic provider fixture and one immutable Kazan snapshot set are
reproducible from recorded provider instructions. Every input has an approved
policy result, hash, coordinate metadata, acquisition-area fingerprint, and
notice. The selected Tatarstan provider feature has an independent fingerprint.
The exact planned real-data download is within 1 GiB, and no raw provider
payload is committed by default.

Result: passed. The stable implementation and contract router is
[`tools/World/SourceIngestion/README.md`](../../../../tools/World/SourceIngestion/README.md). The
exact source pins live in its source profile; shared runtime pins live in the
[World Execution Environment](../../../../tools/World/ExecutionEnvironment/README.md). The
ignored machine-readable run receipt is
`tmp/world/source_ingestion/runs/kazan_p0/<run_inputs_hash>/run/run_result.json`.
