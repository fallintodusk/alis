# ALIS World Fabric Implementation Research

> Status: Imported implementation research draft. Source links were preserved.
> Validate material claims against primary sources and ALIS repository
> assumptions before promoting conclusions into architecture documentation.

**Verdict: ALIS should become a versioned Earth-data compiler and simulation platform. Unreal Engine is the high-fidelity runtime-not the database, not the source of truth, and not the global world format.**

That architecture directly supports ALIS's real-world setting, scientific grounding, continuity, and eventual large-scale expansion.  It also matches the project's goal of open protocols, community-hosted servers, and survival beyond a single company. 

# What proven digital twins teach us

## 1. The renderer must never own the world

The OGC CDB standard describes a **single, versionable representation of Earth** that can publish relevant content to multiple simulators. The important lesson is not to adopt the entire CDB schema; it is to maintain one engine-independent world repository and generate client-specific representations from it. ([Open Geospatial Consortium][1])

This is also the direction of modern digital-twin architecture: capabilities are kept modular and composable instead of being fused into one giant platform. ([Digital Twin Consortium][2])

For ALIS:

* PostgreSQL/PostGIS, JSON schemas, object storage and manifests own the world.
* Unreal owns rendering, local interaction, physics and presentation.
* A web client may inspect the same world.
* Simulation services may consume the same roads and building semantics.
* Community servers may mirror or extend the same published cells.

## 2. Semantic truth and rendered geometry are different products

**3D Tiles** is designed for hierarchical streaming and rendering of massive photogrammetry, buildings, BIM, instanced features and point clouds. It is an excellent distribution format, but it is explicitly a format for delivering renderable content-not a complete world ontology or simulation database. ([Open Geospatial Consortium][3])

**CityGML 3.0** and **CityJSON** solve a different problem: buildings, terrain, roads and city objects as meaningful entities with relationships, classifications, geometry and multiple levels of detail. CityJSON provides a much more developer-friendly JSON encoding of a CityGML subset. ([Open Geospatial Consortium][4])

Therefore:

* `Building DNA` is semantic source data.
* A Nanite mesh is one generated representation.
* A collision mesh is another.
* A navigation representation is another.
* A 3D Tiles building is another.
* A World Partition actor or generated UAsset is another.

Never reverse this direction by trying to recover authoritative building meaning from an Unreal mesh.

## 3. Static geography and live world state must be separated

Digital-twin frameworks such as NGSI-LD and Eclipse Ditto separate entity identity, properties, relationships and changing state from visualization. Ditto additionally distinguishes reported, desired and current state and provides policy-controlled subscriptions and messaging integrations. ([ngsi-ld.org][5])

ALIS should borrow this separation without adopting an entire IoT platform:

* **Static world:** terrain, road topology, building structure, addresses.
* **Scenario state:** war damage, blocked roads, abandoned buildings, contamination.
* **Server state:** containers, doors, inventories, vehicles, construction.
* **Transient state:** players, AI, projectiles, current weather.
* **Historical state:** previous dates or reconstruction scenarios.

The base city should not be regenerated because somebody broke a window.

## 4. Massive worlds use several spatial hierarchies-not one universal grid

CARLA's own large-map workflows divide geometry into tiles, while its OpenDRIVE mode generates road meshes from a separate semantic road definition. Its documentation explicitly notes that large cities push engine limits and that generated meshes are split into portions to contain cost and errors. ([CARLA][6])

ALIS needs at least five distinct spatial partitions:

| Partition                  | Responsibility                                            |
| -------------------------- | --------------------------------------------------------- |
| Source partitions          | How OSM, Overture, DEM, LiDAR and imagery are stored      |
| Compilation cells          | Unit of deterministic regeneration and cache invalidation |
| Render hierarchy           | 3D Tiles, terrain LODs, HLODs and Nanite representations  |
| Unreal World Partition     | Engine-local loading and actor lifetime                   |
| Simulation authority cells | Which server or process owns dynamic state                |

They can share parent-child relationships and identifiers, but they **must not be forced to use identical cell sizes**. A traffic simulation district, a 3D Tiles node and a World Partition cell have different optimal boundaries.

# The references ALIS should actually study

## Cesium for Unreal and 3D Tiles: adopt the streaming boundary

Cesium for Unreal is Apache 2.0, works with a full-scale WGS84 globe, supports private-network and non-Cesium content, and integrates streamed terrain, imagery, buildings and photogrammetry with Unreal interaction and physics. It is the strongest ready-made geospatial/runtime boundary available for Unreal. ([GitHub][7])

**Borrow:**

* WGS84/ECEF georeferencing.
* Hierarchical streaming.
* Origin and precision management.
* Open 3D Tiles interfaces.
* Far-field terrain and city representation.

**Do not copy:**

* A mandatory dependency on Cesium ion.
* The assumption that streamed visual data is authoritative simulation data.
* A pipeline where ALIS cannot regenerate or self-host its tiles.

3D Tiles 1.1 should be the stable target now. On **July 21, 2026**, 3D Tiles 2.0 is still a proposed work item, with public comments open until July 23, 2026. Its proposed scope includes glTF 2.1, time-dynamic content, voxels, vector tiles and Gaussian splats. Keep the exporter isolated so ALIS can adopt 2.0 later without changing the canonical world. ([Open Geospatial Consortium][8])

## OGC CDB: copy the architecture, not the full standard

CDB's strongest idea is a versionable Earth repository from which different simulator clients retrieve suitable representations. This is almost exactly the strategic role ALIS needs. ([Open Geospatial Consortium][1])

However, CDB carries simulation-industry structure and historical baggage that ALIS does not need. Treat it as an architectural reference:

* Common world repository.
* Consistent feature identities.
* Versioned publishing.
* Correlation between clients.
* On-demand derived representations.

Do not implement full CDB compliance during the prototype.

## CityJSON and 3DCityDB: borrow the semantic building model

3DCityDB v5 is a maintained open-source PostgreSQL/PostGIS system for semantic city models. It supports CityGML 3.0, CityJSON import/export, multiple levels of detail, textures, terrain, prototypical objects and historical termination timestamps. ([docs.3dcitydb.org][9])

For ALIS, 3DCityDB is best used as:

* A reference schema.
* An import/export sidecar.
* A validation target for external city datasets.
* A source of tested semantic-city concepts.

It should **not automatically become the primary ALIS database**. Game-specific requirements-damage, inventories, construction states, modular interiors, procedural rules and replication-would eventually fight its city-model schema.

## Overture Maps and GERS: use as the global identity seed

Overture provides open global map releases, modular schemas and GERS identifiers that are intended to remain stable across releases. Its release artifacts include registries, changelogs and bridge files that track feature changes. ([Overture Maps][10])

Recommended policy:

1. Use a GERS ID when the real-world object has one.
2. Preserve the source OSM or authority ID.
3. Assign an ALIS namespaced UUID for entities without stable external identity.
4. Never derive permanent identity from coordinates or mesh names.
5. Keep identity when geometry or attributes change.

This solves a hard future problem: associating historical data, gameplay state, community corrections and alternative representations with the same real-world building or road.

## OpenDRIVE, CARLA and SUMO: separate road meaning, mesh and traffic

ASAM OpenDRIVE 1.9.0 was released on May 19, 2026. It describes road geometry, lanes, junctions, markings, objects and signals in an exchangeable simulation format. ([Asam][11])

CARLA demonstrates that an OpenDRIVE road definition can generate temporary geometry and traffic controls. It also exposes the danger: malformed or incomplete road semantics produce malformed generated roads, and special cases such as slopes, sidewalks and junctions require explicit handling. ([CARLA][6])

SUMO is a maintained open-source, multimodal traffic simulator for large networks, supporting vehicles, public transport and pedestrians. It can import OpenStreetMap data and be controlled externally. ([Eclipse Foundation][12])

ALIS should therefore maintain:

```text
Road source data
    v
ALIS canonical road graph
    +-- OpenDRIVE export
    +-- Unreal road/sidewalk mesh compiler
    +-- navigation graph
    +-- SUMO traffic network
    +-- low-detail map representation
```

The road mesh is expendable. The road graph is not.

## OSM2World: use as a procedural-generation benchmark

OSM2World already understands more than 250 OSM tags, supports building roofs, road markings, street objects, materials, different LODs and glTF/GLB export. It runs as a CLI, library or self-hosted service and is fully open source. ([OSM2World][13])

It is valuable in two ways:

* Immediate generation of prototype cities.
* A mature reference for turning imperfect OSM semantics into deterministic 3D rules.

Do not deeply couple ALIS to its internal Java architecture. Wrap it behind an adapter, compare its output against ALIS compilers, and port only rules where ALIS needs tighter gameplay control.

## TerriaJS: use a web world inspector beside Unreal

TerriaJS powers large public geospatial explorers and digital-twin deployments, can federate tens of thousands of layers, supports 2D/3D data and can be hosted largely as a static browser application. ([GitHub][14])

A browser inspector is not optional for a project this complex. It gives developers and contributors a fast way to inspect:

* Source layers.
* Cell boundaries.
* Data provenance.
* Generation errors.
* Road topology.
* Building DNA.
* Diff between two world versions.
* Server and scenario overlays.

Opening Unreal Editor merely to check whether a building ID or footprint is wrong is a catastrophic workflow at global scale.

## OpenUSD: use between tools, not as the global database

OpenUSD is a scalable system for composing and streaming layered scene descriptions between graphics applications. It remains actively maintained, with version 26.05 released in April 2026. ([GitHub][15])

Use it for:

* Blender and DCC interchange.
* Layered authored modules.
* Variant sets.
* Non-destructive asset composition.
* Reusable facade, roof and interior component libraries.

Do not use USD as the authoritative geographic database, road graph or simulation-state protocol.

# Recommended architecture: **ALIS World Fabric**

```text
 +---------------------------------------------------------------+
 | Public and community data                                    |
 | Overture - OSM - DEM - LiDAR - imagery - city models - BIM   |
 +------------------------------+--------------------------------+
                                |
                 ingest, snapshot, license, validate
                                |
 +------------------------------v--------------------------------+
 | Source Registry                                               |
 | STAC catalog + immutable objects in MinIO                     |
 | Original CRS - timestamp - license - quality - source hashes  |
 +------------------------------+--------------------------------+
                                |
                   GDAL - PROJ - PDAL - validators
                                |
 +------------------------------v--------------------------------+
 | Canonical World Model                                         |
 | PostGIS geometry + versioned JSON/JSONB DNA                   |
 | Stable IDs - relationships - provenance - confidence          |
 +------------------------------+--------------------------------+
                                |
                deterministic, incremental cell compiler
                                |
 +------------------------------v--------------------------------+
 | Published World Cells                                         |
 | Semantic: CityJSON - OpenDRIVE - GeoParquet                   |
 | Raster: COG        Point cloud: COPC                          |
 | Render: glTF - 3D Tiles - terrain tiles                       |
 | Runtime: UE actors - PCG inputs - UAssets - HLODs             |
 +------------------------------+--------------------------------+
                                |
           signed manifests + content hashes + HTTP range access
                                |
 +---------------+--------------v--------------+-----------------+
 | Unreal client | Browser world inspector     | Simulation      |
 | World Part.   | CesiumJS/TerriaJS           | Rust/SUMO/etc.  |
 +---------------+-----------------------------+-----------------+

 Dynamic state and events are separate overlays, never baked into base geography.
```

# Layer responsibilities

## 1. Source Registry

Use **STAC** as the catalog for source datasets, imagery, DEMs, LiDAR, derived products and their provenance. STAC became an OGC Community Standard in 2025 and was designed around a minimal core with extensible metadata for many geospatial asset types. Overture itself publishes a STAC catalog for its releases. ([Open Geospatial Consortium][16])

Every imported source should record:

```text
source_id
source_version
retrieved_at
valid_time
original_crs
license
attribution
spatial_extent
content_hash
quality/confidence
processing_history
```

Mirror important input data into ALIS-controlled storage. Do not make runtime generation depend on a public API remaining available. This is not theoretical: Copernicus announced on July 17, 2026 that access rules for its GLO-30 DEM view service will change on July 28, 2026. ([Copernicus Data Space Ecosystem][17])

## 2. Canonical World Model

Use:

* **PostGIS** for mutable, indexed semantic objects and spatial relationships.
* **JSONB** for versioned ALIS-specific DNA and extensions.
* **GeoParquet** for immutable snapshots, bulk interchange and analytics.
* **MinIO** for raw and generated binary objects.

GeoParquet is well suited to compressed, partitioned, read-heavy vector data, but its own specification warns that it is not the right backing store for write-heavy interaction. That is why it complements PostGIS rather than replaces it. ([GitHub][18])

**JSON-first must not become JSON-only.**

A valid ALIS entity should have:

* A human-readable JSON representation.
* A formal JSON Schema version.
* Typed geometry in PostGIS or GeoParquet.
* Stable identifiers and relationships.
* No giant encoded meshes or heightmaps inside JSON.

## 3. Deterministic World Compiler

The compiler should be a collection of stateless workers, not an Unreal Editor script collection.

Inputs:

```text
source snapshot hashes
world schema version
generator version
generation profile
cell address
deterministic seed
manual override set
```

Outputs:

```text
semantic cell
render cell
collision/navigation data
UE import manifest
validation report
content hashes
required attribution
```

GDAL and PDAL remain heavily maintained open-source foundations: GDAL 3.13.1 was released in June 2026, and PDAL 2.10.2 in June 2026. ([GDAL][19])

Use DuckDB Spatial for local analysis, tests and build-time transformations-not as the authoritative world service. DuckDB 1.5 added a core geometry type, but its spatial behavior is still evolving, including explicit axis-order migration. Pin versions and test CRS behavior. ([DuckDB][20])

## 4. Purpose-specific published formats

Use one format for one responsibility:

| Concern                     | Format                                 |
| --------------------------- | -------------------------------------- |
| Asset and dataset catalog   | STAC                                   |
| Raster, imagery, DEM        | COG                                    |
| Vector snapshots            | GeoParquet                             |
| Point clouds and LiDAR      | COPC                                   |
| Urban semantic exchange     | CityJSON                               |
| Road semantic exchange      | OpenDRIVE                              |
| Massive render streaming    | 3D Tiles                               |
| Individual runtime geometry | glTF/GLB                               |
| DCC composition             | OpenUSD                                |
| Unreal cache                | UAssets, World Partition actors, HLODs |

COG and COPC support range-based retrieval, allowing a consumer to request relevant raster blocks or point-cloud octree chunks instead of downloading entire datasets. ([Open Geospatial Consortium][21])

## 5. Unreal Runtime Adapter

Unreal should receive a cell manifest and decide which representation is required:

### Global/far field

* Cesium terrain or generated mesh terrain.
* 3D Tiles buildings and photogrammetry.
* Non-interactive HLOD geometry.
* Simplified vegetation and roads.

### Playable district

* World Partition actors.
* PCG vegetation and props.
* Generated road and sidewalk meshes.
* Building shells with collision.
* Mass-based background entities.

### Immediate player area

* Interactable building modules.
* Doors, containers and destructible elements.
* Detailed interiors.
* Navigation, physics and gameplay state.

This creates a realistic **detail cone** instead of pretending every square kilometre of Earth can have hero-quality gameplay assets simultaneously.

Generated UAssets must be treated like compiler output:

```text
Never hand-edit generated assets.
Never store unique world meaning only in generated assets.
Always be able to delete and rebuild them.
```

Authored art remains safe by living in reusable module libraries and override layers that the compiler references.

# ALIS Building DNA

Do not try to invent a universal biological schema covering every possible structure immediately. That becomes an unmaintainable ontology project.

Start with a practical CityJSON-inspired profile:

```text
identity
source references
validity interval
footprint and parcel relationship
ground and roof elevations
storeys and vertical profile
roof grammar
facade grammar and bay layout
openings
structural system
semantic zones
interior connectivity
material rules
reusable module references
generation seed
damage/scenario hooks
confidence per property
manual overrides
schema and generator versions
```

One DNA record should compile to several representations:

```text
LOD0  map footprint / presence
LOD1  building mass
LOD2  roof and facade structure
LOD3  playable shell and selected interiors
LOD4  hero or scientifically detailed reconstruction
```

CityGML and CityJSON already establish semantic objects and multiple levels of detail, while OSM2World proves that rule-based roofs, facades and objects can be generated from common map tags. ([Open Geospatial Consortium][4])

The important biological analogy is:

> DNA describes constraints, relationships and growth rules. It does not store every final triangle.

# Editing and community corrections

Use an immutable overlay chain:

```text
public source snapshot
    v
ALIS normalized base
    v
ALIS factual corrections
    v
historical/scenario layer
    v
server-specific modifications
    v
player-created state
```

Never modify imported public data in place. Never modify generated meshes to correct source data.

A correction should be a small, reviewable patch:

```json
{
  "entity_id": "gers:...",
  "base_version": "2026-06",
  "operations": [
    {
      "op": "replace",
      "path": "/building/roof/type",
      "value": "hipped"
    }
  ],
  "evidence": ["source:municipal-survey-2024"],
  "author": "did-or-pseudonymous-id"
}
```

That model enables moderation, attribution, rollback, forks and community-hosted world variants.

# Critical failure modes

## 1. Treating World Partition as the global world database

World Partition is a downstream Unreal loading mechanism. If it defines global identity, source data and world history, every engine upgrade becomes a database migration.

**Rule:** World Partition consumes published cells.

## 2. One grid for everything

Forcing rendering, simulation, generation and networking into identical cells creates pathological boundaries and scaling problems.

**Rule:** separate grids, common addresses and deterministic mappings.

## 3. Whole-Earth runtime procedural generation

Runtime generation is useful for local adaptation and gameplay detail. It is the wrong place to perform GIS repair, building reconstruction and continental road compilation.

**Rule:** expensive and deterministic work happens offline; runtime work is bounded and local.

## 4. A mandatory commercial tiling pipeline

Cesium's runtimes and standards are open, but turnkey production tiling is less uniform than consumption. Cesium's open 3D Tiles Tools can process and inspect tilesets, while other generators cover specific inputs rather than every ALIS case. Keep a pluggable `IWorldTileExporter` boundary and avoid making Cesium ion mandatory. ([GitHub][22])

## 5. Forking Unreal or adopting CARLA as the platform

CARLA is valuable proof, not the ALIS foundation. Its migration from Unreal 4.26 to Unreal 5.5 was described by the project as a major undertaking. The inference is clear: a long-lived engine fork creates a continuing tax on upgrades, plugins and contributors. ([CARLA Simulator][23])

**Rule:** normal UE plugin architecture unless an engine patch is genuinely unavoidable.

## 6. Deploying a complete digital-twin platform too early

Eclipse Ditto and FIWARE solve real IoT and municipal interoperability problems, but they introduce extra databases, services, brokers, policies and operational complexity. Ditto alone includes multiple microservices and MongoDB. ([GitHub][24])

**Rule:** implement a small ALIS state protocol first. Add NGSI-LD adapters when real external interoperability exists.

## 7. Using AI generation as authoritative reconstruction

Generated geometry can accelerate missing-detail hypotheses, but it must not silently become factual truth.

Every inferred property needs:

* Source or method.
* Confidence.
* Model/tool version.
* Generation seed.
* Ability to regenerate.
* Ability to replace with verified information.

## 8. Expanding before reproducibility works

A pipeline that can generate 1,000 km^2 once but cannot reproduce one changed building safely is already broken.

**Gate:** delete every generated artifact and rebuild the pilot region from immutable sources with equivalent manifests and validated outputs.

# Recommended permanent stack

## Tier S - foundational

1. **PostgreSQL/PostGIS** - mutable canonical semantic world.
2. **MinIO** - immutable source snapshots and generated artifacts.
3. **STAC** - source catalog, provenance and discovery.
4. **GDAL + PROJ + PDAL** - geospatial normalization and processing.
5. **Overture/GERS + OpenStreetMap** - global base data and stable identity.
6. **CityJSON-derived ALIS schemas** - buildings and city semantics.
7. **OpenDRIVE-derived ALIS road graph** - road and lane semantics.
8. **Cesium for Unreal + 3D Tiles** - global georeferencing and render streaming.
9. **Rust deterministic cell compiler** - orchestration, validation and manifests.
10. **Standard HTTP/S3 distribution with signed manifests** - mirrors and decentralized hosting.

## Tier A - integrate where the use case appears

* **Mapterhorn/OpenTopography** for open terrain sourcing. Mapterhorn is BSD-3 and aggregates many open elevation datasets, but ALIS should mirror selected sources and retain their individual licenses. ([Mapterhorn][25])
* **OSM2World** as a prototype generator and procedural-rule reference.
* **SUMO** for traffic and pedestrian simulation.
* **TerriaJS** for the contributor and operational world inspector.
* **OpenUSD** for Blender/DCC composition.
* **3DCityDB v5** as an interoperability/import-export sidecar.
* **NGSI-LD** as a future adapter for real municipal or sensor twins.

# Build order that avoids rework

## Phase 1 - contracts before visuals

Create these repositories or modules:

```text
alis-world-schema
alis-world-catalog
alis-world-compiler
alis-world-protocol
alis-world-runtime-ue
alis-world-inspector
```

Lock down:

* Entity identity.
* CRS policy.
* Cell addressing.
* Schema versioning.
* Source provenance.
* Artifact manifests.
* Overlay semantics.
* Deterministic seed policy.

## Phase 2 - one complete vertical slice

Use one approximately `10 x 10 km` urban area, with one `1 x 1 km` fully playable district.

Pipeline:

```text
Overture/OSM + DEM
    -> STAC + MinIO snapshot
    -> PostGIS canonical entities
    -> terrain cells
    -> road graph and OpenDRIVE
    -> building DNA and shells
    -> 3D Tiles/glTF
    -> UE World Partition cache
    -> browser inspection
```

Do not add global scale yet.

## Phase 3 - reproducibility gate

The region must support:

```text
clean storage
+ source manifest
+ schema versions
+ generator containers
= equivalent published world
```

Add golden-cell tests for:

* Coastal terrain.
* River crossing.
* Complex junction.
* Apartment block.
* Detached building.
* Industrial structure.
* Sloped terrain.
* Missing or contradictory source data.

## Phase 4 - separate static and dynamic worlds

Add:

* Scenario overlays.
* Destruction state.
* Road closures.
* Server-owned objects.
* Local simulation subscriptions.
* Incremental rebuilds.

Changing one roof should invalidate one building and its affected render cells-not the city.

## Phase 5 - federation

Only after the protocol is stable:

* Signed world manifests.
* Community mirrors.
* Independent cell providers.
* Community corrections.
* Alternative historical/scenario layers.
* Player-run authoritative servers.

# Final architecture decision

The defining ALIS principle should be:

> **The world is a versioned graph of real entities and procedural rules. Meshes, tiles, UAssets and simulations are temporary expressions of that graph.**

That gives ALIS the speed of ready-made geospatial tools, Unreal's visual power, reproducible scientific foundations, community ownership, and a realistic path from one playable district to a world that no single editor, server or company must contain.

[1]: https://www.ogc.org/standards/cdb/?utm_source=chatgpt.com "CDB Standard - Synthetic Environment Data Model & Structure"
[2]: https://www.digitaltwinconsortium.org/press-room/04-24-24/?utm_source=chatgpt.com "Introducing the Composability Framework V1.1"
[3]: https://www.ogc.org/standards/3dtiles/?utm_source=chatgpt.com "3D Tiles Standard - Streaming Massive 3D Geospatial Data"
[4]: https://www.ogc.org/standards/citygml/?utm_source=chatgpt.com "CityGML Standard - 3D Urban Data Model & Exchange Format"
[5]: https://ngsi-ld.org/?utm_source=chatgpt.com "Official Website for NGSI-LD"
[6]: https://carla.readthedocs.io/en/0.9.10/adv_opendrive/?utm_source=chatgpt.com "OpenDRIVE standalone mode - CARLA Simulator"
[7]: https://github.com/CesiumGS/cesium-unreal?utm_source=chatgpt.com "GitHub - CesiumGS/cesium-unreal: Bringing the 3D geospatial ecosystem to Unreal Engine - GitHub"
[8]: https://www.ogc.org/requests/ogc-seeks-public-comment-on-proposed-3d-tiles-2-0-community-standard-work-item/?utm_source=chatgpt.com "OGC Seeks Public Comment on Proposed 3D Tiles 2.0 Standard"
[9]: https://docs.3dcitydb.org/1.1/3dcitydb/?utm_source=chatgpt.com "3D City Database - 3D City Database v5 documentation"
[10]: https://docs.overturemaps.org/gers/?utm_source=chatgpt.com "What is GERS? | Overture Documentation"
[11]: https://www.asam.net/standards/detail/opendrive/?utm_source=chatgpt.com "ASAM OpenDRIVE(R)"
[12]: https://eclipse.dev/sumo/index.html?utm_source=chatgpt.com "Eclipse SUMO - Simulation of Urban MObility"
[13]: https://osm2world.org/?utm_source=chatgpt.com "OSM2World"
[14]: https://github.com/TerriaJS/terriajs?utm_source=chatgpt.com "GitHub - TerriaJS/terriajs: A library for building rich, web-based geospatial 2D & 3D data platforms. - GitHub"
[15]: https://github.com/PixarAnimationStudios/OpenUSD?utm_source=chatgpt.com "GitHub - PixarAnimationStudios/OpenUSD: Universal Scene Description - GitHub"
[16]: https://www.ogc.org/announcement/ogc-announces-publication-of-the-spatiotemporal-asset-catalog-community-standards/?utm_source=chatgpt.com "SpatioTemporal Asset Catalog Community Standard Published"
[17]: https://dataspace.copernicus.eu/news/2026-7-17-copernicus-dem-30m-view-service-license-acceptance?utm_source=chatgpt.com "Copernicus DEM 30m View Service - License Acceptance | Copernicus Data Space Ecosystem"
[18]: https://github.com/opengeospatial/geoparquet?utm_source=chatgpt.com "GitHub - opengeospatial/geoparquet: Specification for storing geospatial vector data (point, line, polygon) in Parquet - GitHub"
[19]: https://gdal.org/en/stable/index.html?utm_source=chatgpt.com "GDAL - GDAL documentation"
[20]: https://duckdb.org/2026/03/09/announcing-duckdb-150?utm_source=chatgpt.com "Announcing DuckDB 1.5.0 - DuckDB"
[21]: https://www.ogc.org/standards/ogc-cloud-optimized-geotiff/?utm_source=chatgpt.com "Cloud Optimized GeoTIFF Standard - Efficient Web Raster Data"
[22]: https://github.com/CesiumGS/3d-tiles-tools?utm_source=chatgpt.com "GitHub - CesiumGS/3d-tiles-tools - GitHub"
[23]: https://carla.org/2024/12/19/release-0.10.0/?utm_source=chatgpt.com "CARLA 0.10.0 Release with Unreal Engine 5.5! - CARLA Simulator"
[24]: https://github.com/eclipse-ditto/ditto?utm_source=chatgpt.com "GitHub - eclipse-ditto/ditto: Eclipse Ditto(TM): Digital Twin framework of Eclipse IoT - main repository - GitHub"
[25]: https://mapterhorn.com/?utm_source=chatgpt.com "Mapterhorn"

