# Open World Generation Research Audit

Status: evidence and rationale only. Bounded implementation results belong
exclusively to [01](01_source_ingestion.md), [02](02_canonical_world_compilation.md),
[03](03_unreal_world_realization.md), and
[04](04_end_to_end_validation.md).

This audit checks `research_main.md`, `research_agentic.md`,
`research_ue.md`, and `research_impl.md` against repository evidence and
primary sources. It does not own implementation tasks, acceptance status, or
standing architecture.

## Audit Verdict

The research supports these directions:

- Canonical geospatial and semantic data remain outside Unreal.
- Unreal assets are disposable projections, not source data.
- Static geography, authored overrides, generated output, and dynamic state
  have separate ownership.
- Compilation is deterministic by explicit spatial cell and stable feature
  identity.
- Agents may assist authoring and validation but are not shipped runtime
  authority.

The reports are research inputs rather than an approved implementation plan.
They sometimes describe proposed ALIS systems as implemented, mix engine
versions, treat optional infrastructure as foundational, and blur source
admission with canonical compilation. The `01` through `04` files contain the
corrected execution model.

## Research Evidence Coverage

| Report | Evidence state | Safe use |
|---|---|---|
| `research_main.md` | Numbered source links recovered from the source PDF | Strategy and candidate-source discovery |
| `research_agentic.md` | Numbered source links recovered from the source PDF | Automation opportunities, subject to security and maturity checks |
| `research_ue.md` | Numbered source links recovered from the source PDF | UE 5.8 opportunity research, not proof of current project capability |
| `research_impl.md` | Includes direct source references | Compiler concepts, subject to ALIS scope and dependency decisions |

A recovered citation establishes traceability, not correctness. Mutable
product status, licenses, data access, standards, and engine compatibility
must be rechecked at their adoption boundary.

## Repository Evidence

### C-02 - Correct the implemented baseline

- `Alis.uproject`, launcher `UE_PATH`, and the verified source-release root
  select the UE 5.8 line.
- `ProjectWorld` contains manifest and definition-host primitives, but its
  documented loader, tile, query, validator, diff, watcher, and test surfaces
  are not an implemented world compiler.
- `ProjectPCG` identifies its ALIS engine integration as a stub.
- City17 has no tracked canonical world schema, compiler-cell manifest, or
  generated-world data pipeline.
- Existing `blueprint-mcp` and `ue-mcp` surfaces remain precision and coverage
  fallbacks. UE 5.8's official Unreal MCP is preferred for proven native
  discovery and supervised control, but remains optional and cannot own
  generation or acceptance logic.

The first implementation is therefore a new external compiler plus a narrow
Unreal adapter. It is not an extension of a completed ALIS world pipeline.

### C-03 - Preserve the UE version boundary

Source ingestion, canonical schemas, external compilation, and D0-D2 remain
engine-independent.

The approved Unreal realization target is the UE 5.8 baseline. The separate
[engine-upgrade task](../../tools/engine_version_update_sot.md) owns
candidate identity, migration, build, packaging, plugin compatibility, and
core regressions; that gate has passed. World realization now waits for the
canonical fixture.

Unreal MCP and its editor-only toolsets are enabled for supervised use from
the first slice, but are not P0 dependencies. Stable PCG is owned by
`ProjectPCG`; GeoReferencing is owned by `ProjectWorld`. In UE 5.8, the
Experimental PCG Editor Mode ships inside the PCG plugin and defaults on, so
it is already available for a supervised spline, paint, or volume pilot. It
must not become generation authority or an acceptance dependency.

Python, Editor Scripting Utilities, Geometry Scripting, and Interchange are
already available through the active engine dependency graph. Do not add
duplicate project declarations. Geometry Scripting may be evaluated for a
bounded footprint-to-Static-Mesh massing proof; only a proven ALIS consumer
may promote it to an explicit component dependency.

Semantic Search is transitively mounted by `AllToolsets`, but AI search and
indexing remain inactive until the operator approves provider, cost, asset
data transfer, and credential-storage policy. Epic documents that indexing
may send thumbnails, asset paths, classes, and metadata to the provider, and
that provider keys are stored in clear text in per-user editor configuration.

Procedural Vegetation Editor and Mesh Terrain remain disabled experiments.
Current user reports show useful PVE output but a large skeletal-versus-static
performance spread, while multiple UE 5.8 users report standalone and unload
crashes with Mesh Terrain. Water and Landmass may be transitively mounted by
engine plugins, but ALIS does not depend on them for P0. Community reports
prioritize pilots; they are not acceptance evidence.

Epic labels Unreal MCP, PCG Editor Mode, Procedural Vegetation Editor, and
Mesh Terrain Experimental and warns that relevant APIs or data formats may
change. Time and marketplace compatibility are planning signals, not ALIS
acceptance evidence.

Primary evidence:
[UE 5.8 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes),
[PCG Editor Mode](https://dev.epicgames.com/documentation/unreal-engine/pcg-editor-mode-in-unreal-engine),
[Mesh Terrain](https://dev.epicgames.com/documentation/unreal-engine/mesh-terrain-in-unreal-engine),
and [Unreal MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor).
Community signals:
[PVE performance discussion](https://www.reddit.com/r/unrealengine/comments/1uw7m8d/has_anyone_tested_the_performance_impact_of_the/)
and [Mesh Terrain crash reports](https://forums.unrealengine.com/t/can-anyone-actually-test-the-new-5-8-mesh-terrain-please/2729927).

### C-04 - Define identity, coordinates, and separate spatial grids

Source partitions, compiler cells, generated artifacts, World Partition cells,
simulation cells, and delivery bundles are different identities. A mapping
between two grids does not make them the same grid.

Canonical compilation must eventually define:

- provider feature and release identity;
- ALIS feature, revision, artifact, and tombstone identity;
- source CRS and vertical datum;
- canonical CRS, axis order, units, precision, and quantization;
- source-to-canonical and canonical-to-Unreal transforms;
- deterministic boundary ownership and neighbor references.

The exact contracts and their implementation status belong to
[02 Canonical World Compilation](02_canonical_world_compilation.md), not this
audit.

## First-Slice Rationale

The contract proof is intentionally smaller than a Kazan content-production
slice:

```text
two adjacent cells
    synthetic terrain
    one boundary-crossing road
    small building massing
    one authored override
    one invalid feature
```

This is enough to expose provenance, identity, boundary, determinism,
regeneration, and authored-ownership failures. The same contract then runs on
minimal pinned Kazan terrain, road, and building inputs. Water, vegetation,
foliage, rendering profiles, regional quality, and scale are later work.

Acceptance values and D0 through D3 meanings are owned by
[04 End-to-End Validation](04_end_to_end_validation.md). Keeping them there
prevents this evidence file from becoming a parallel test plan.

## Ingestion and Compiler Boundary

The transformation authority is singular:

```text
01 ingestion
    download and verify provider bytes
    decode provider formats
    preserve provider IDs, schema, CRS, datum, precision, and terms
    emit provider-normalized snapshots

02 compiler
    transform into canonical CRS and units
    assign ALIS identity and compiler-cell ownership
    conflate, repair, and validate semantics
    emit canonical ALIS JSON
```

Ingestion must not assign ALIS identity, choose compiler-cell ownership,
conflate providers, or perform canonical CRS conversion.

## Source Discovery Evidence

Discovery evidence proves that a candidate currently has relevant Kazan
coverage. It is not an admitted build input and does not replace an immutable
snapshot.

### OpenStreetMap Kazan probe

Endpoint:
`https://overpass-api.de/api/interpreter`

Observed at UTC:
`2026-07-31T06:46:59Z`

Normalized UTF-8 response SHA-256:
`911e3dd815f66b80ff67fba137b33da3746d981df14ce83fff9f89c2ec23f333`

Query:

```text
[out:json][timeout:120];
nwr["building"](55.78,49.08,55.81,49.13)->.buildings;
nwr["highway"](55.78,49.08,55.81,49.13)->.highways;
nwr["waterway"](55.78,49.08,55.81,49.13)->.waterways;
nwr["natural"="water"](55.78,49.08,55.81,49.13)->.water;
nwr["natural"="tree"](55.78,49.08,55.81,49.13)->.trees;
.buildings out count;
.highways out count;
.waterways out count;
.water out count;
.trees out count;
```

Observed totals:

| Selection | Count |
|---|---:|
| Buildings | 2,044 |
| Highway-tagged elements | 5,139 |
| Waterways | 13 |
| Water areas | 11 |
| Mapped trees | 462 |

The response reports OSM base timestamp `2026-07-31T06:45:06Z`. These counts
are mutable discovery evidence only. P0 admission requires a dated immutable
OSM snapshot or exact redistribution-safe extract with an identifier and
hash. OSM data is ODbL; distribution requires attribution and license
disclosure. See the
[OSM copyright and attribution page](https://www.openstreetmap.org/copyright).

The adopted real-data P0 route is a dated Geofabrik Volga Federal District
PBF, not live Overpass. Geofabrik exposes timestamped PBFs, a raw archive, and
provider MD5 files. Admission records that provider checksum and computes an
ALIS SHA-256 over the retained payload before deterministic Kazan and
Tatarstan clipping. The mutable `latest` alias is never a snapshot identity.
The exact dated release and hashes are pinned in the
[source profile](../../../../tools/World/SourceIngestion/profiles/kazan_p0.source.json).
See the [official Volga extract](https://download.geofabrik.de/russia/volga-fed-district.html).

### Copernicus DEM GLO-30

The official collection page identifies GLO-30 as a global 30 m DSM with a
free license and required notices for original and modified outputs. The
implemented P0 route uses the public AWS Open Data COG distribution of the
2021 release, so every developer can anonymously acquire the pinned N55 E049
tile without an account. The exact URL, bytes, hash, horizontal CRS, vertical
datum, and notices are pinned in the
[source profile](../../../../tools/World/SourceIngestion/profiles/kazan_p0.source.json).
See the
[Copernicus DEM collection](https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM).

GLO-30 is a DSM containing building, infrastructure, and vegetation effects.
It is sufficient for the regional baseline and scale proof, but it is not
accepted as precise road grading, foundations, tunnels, embankments, or
high-detail playable terrain without correction layers.

### Deferred candidates

- Copernicus LCFM is the preferred later land-cover adapter candidate.
  WorldCover 2021 v200 remains the reproducible 10 m CC BY 4.0 fallback
  candidate; both remain outside P0 and require an exact release, hash, and
  acknowledgement before admission.
- OSM water features remain representative-region inputs rather than P0
  compiler requirements.
- Microsoft Global ML Building Footprints and Overture Buildings remain
  comparison-only adapters. They may emit coverage-gap candidates and quality
  reports, but no automatic canonical merge is permitted until conflation and
  provider-specific provenance obligations are proven.
- Paid, NonCommercial, NoDerivatives, unknown-license, Google-derived, or
  scraped reconstruction inputs remain excluded by the
  [world-data policy](../../../../docs/legal/world_data_and_asset_policy.md).

## Other Research Corrections

- HLOD, Nanite, instancing, GPU PCG, collision, navigation, and streaming
  policies require representative-region measurements. No global rule is
  justified by the reports.
- PostGIS, object storage, remote workers, browser twins, 3D Tiles delivery,
  and repository federation are adoption options, not first-slice
  foundations.
- Third-party Unreal plugins and hosted geospatial services require separate
  code-license, service-term, compatibility, maintenance, and removal-path
  review. Free download does not establish suitability.
- Visual geometry does not automatically provide gameplay collision,
  navigation, interaction, query, or replication authority.
- Provenance, attribution, source or alteration offers, and distribution
  decisions must derive from the same source graph as generated artifacts.
  Stable policy lives in
  [World Data and Unreal Asset Policy](../../../../docs/legal/world_data_and_asset_policy.md)
  and
  [Component License Policy](../../../../docs/legal/component_license_policy.md).

## Out-of-Scope Ownership

Native content delivery is independent and does not block local source,
compiler, or Unreal realization work. Delivery architecture and current
implementation behavior are owned by
[Content Publishing](../../../../docs/loading/content_publishing.md) and the
[Boot Chain](../../../../docs/loading/boot_chain.md).

General script and unattended-automation structure is owned by
[Scripts Architecture](../../../../scripts/docs/architecture.md) and
[Agent Canonical Rules](../../../../docs/agents/canonical.md). This world audit
does not own Delivery or Automation task queues.

## Deferred Sequence

The engine-independent synthetic fixture is compiled first. After the
separate engine upgrade passes, Unreal realization imports that same fixture.
After the complete synthetic and minimal Kazan two-cell gates pass:

1. Expand to Kazan city canonical coverage and a representative game-ready
   region with water, vegetation, foliage, and measured rendering profiles.
2. Compile the complete Tatarstan terrain, road, and building-massing envelope
   with aggregate coverage evidence plus dense urban, suburban, smaller-town,
   rural, sparse, and boundary representative cells.
3. Evaluate optional UE 5.8 Experimental authoring surfaces separately and
   measure cook amplification, runtime memory, frame time, draw calls,
   streaming latency, collision, navigation, and regional data quality.
4. Attempt all-Russia generation only as a batch-scale and heterogeneous-data
   stress test with explicit urban, rural, mountainous, northern, and sparse
   evidence; do not imply nationwide game-ready content.
5. Add databases, object storage, remote workers, far-field streaming, or
   federation only when measured needs justify them.

Until the active acceptance contract passes, the research guides bounded
experiments but is not production architecture.
