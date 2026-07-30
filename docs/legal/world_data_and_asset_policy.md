# World Data and Unreal Asset Licensing Policy

Status: canonical world-data, generated-artifact, and asset-release policy.

This document is the stable ALIS source of truth for:

- application of the root license assignments to world artifacts;
- acceptable geospatial and third-party inputs;
- public, private, and packaged distribution boundaries;
- publication of Unreal `.uasset` files;
- reproducible source acquisition and generation; and
- provenance and release gates.

The root [LICENSE](../../LICENSE) is the sole ALIS component-license
assignment. This document owns world-specific provenance and distribution
rules and does not redefine those assignments.

## 1. Core Rule: Use a Provenance and License Graph

Do not infer one artifact license from one pipeline layer. Record every
copyrightable or database-bearing input and every transformation relationship.

```text
Source datasets + authored overlays + reusable assets + procedural rules
                              |
                              v
                  compiler and tool versions
                              |
                              v
        canonical database + generated artifact + notices
```

Using a tool normally does not place the tool's software license on its
output. The output is instead governed by its inputs, embedded materials, and
any tool-specific terms. The distributable result is the intersection of those
obligations, not a new blanket ALIS license.

## 2. World-Specific Controls

The component compatibility analysis lives in the
[Component License Policy](component_license_policy.md). World releases add
these controls:

1. A generated artifact may preserve database, attribution, content, and
   provider-contract obligations simultaneously.
2. A maintainer can grant an exception only for material whose rights the
   maintainer controls.
3. A `.uasset` can embed material that ALIS has no right to redistribute.

No world artifact is approved for public or commercial release merely because
it fits a root license category.

## 3. World Artifact License Matrix

The root `LICENSE` owns ALIS license names. This matrix adds the world-data
result and upstream obligations:

| Material | Required rule or terms | Reason |
|---|---|---|
| Rights-cleared first-party public art and independently generated Produced Works | Root public-asset rule | Commercial reuse with attribution, when every input permits it |
| Synthetic non-code test fixtures | Root synthetic-fixture rule | Unrestricted test and CI reuse without contaminating production provenance |
| OSM-derived data or an Overture theme/release identified as ODbL | ODbL-1.0 | Preserves the exact upstream database share-alike obligation |
| Other Overture themes and source layers | Exact release, theme, and source terms | Overture data is not distributed under one blanket license |
| Independent ALIS-authored database with no ODbL input | CDLA-Permissive-2.0 | Data-specific permissive terms; do not use if mixed into an ODbL Derivative Database |
| Copernicus or other provider-derived raster/terrain | Provider terms plus required notices | Provider obligations survive format conversion and slicing |
| Public Blueprint logic | Root UE-facing rule | Treat program behavior as software |
| Private hero content | Root private-content rule | Use when public source redistribution is not intended |
| Fab, Marketplace, Megascans, MetaHuman, or other licensed source content | The provider's terms only | ALIS cannot sublicense third-party rights |

Do not apply the root public-asset rule to a generated artifact until the
provenance graph proves that ALIS can grant it.

## 4. Can ALIS Publish `.uasset` Files?

Yes, but only for the classes marked public below. `.uasset` is a container,
not a license. Its embedded or referenced content determines the boundary.

| `.uasset` class | Public source on GitHub? | Required treatment |
|---|---|---|
| Entirely ALIS-authored art asset, with no copied Epic or restricted third-party content | Yes, legally possible | Apply the root public-asset rule and publish a provenance sidecar |
| ALIS-authored Blueprint or other program logic | Yes, legally possible | Apply the root UE-facing rule and offer the preferred editable source |
| ALIS asset that only references a separately obtained dependency | Conditional | Publish no dependency payload; record its exact source and license |
| Generated mesh or terrain from approved open data | Conditional | Satisfy every input license, attribution, database, and Produced Work obligation |
| Asset made from CC0 or CC BY input | Usually yes | Retain attribution and modification notices where required |
| Asset containing OSM or ODbL-classified Overture feature tables or recoverable geometry | Conditional | Treat conservatively as an ODbL Derivative Database, not merely an ALIS asset |
| Asset containing a non-extractable ODbL-derived rendered mesh | Conditional | May be a Produced Work; attribute it and offer the underlying Derivative Database or alteration method |
| Fab Standard, Marketplace, Megascans, or MetaHuman source content | No | Keep source private; distribute only as the applicable agreement permits |
| Unreal Starter Content, Examples, Engine Code, or copied Engine material | No public source by default | Follow the Unreal Engine EULA and Epic Content License Agreement |
| Mixed or undocumented asset | No | Quarantine until every embedded dependency is identified |

Epic states that developers own rights in their Products other than Licensed
Technology and identifies qualifying self-developed asset files as Non-Engine
Products. An asset that references ordinary Unreal classes is therefore not
automatically Epic-owned. It still must not copy Engine Code, Starter Content,
or restricted source content into the public package.

### 4.1 No "Combined Epic + ALIS License"

ALIS cannot relicense Epic or third-party content. Use layered notices:

```text
ALIS-owned portion
    -> approved ALIS content license

Unreal Engine requirement and Epic-owned technology
    -> Epic's separate Unreal Engine EULA

Third-party data or content
    -> original provider license and notices
```

For an ALIS-owned public asset package, provide a notice similar to:

```text
Copyright the identified ALIS contributors.
ALIS-owned material is licensed only as identified in this package.
Unreal Engine is licensed separately by Epic Games.
No Epic, Fab, Marketplace, Megascans, or MetaHuman source content is
redistributed unless explicitly identified and permitted.
Third-party materials remain under the terms listed in THIRD_PARTY.yml.
```

Do not state or imply that Epic approved, sponsors, or relicensed the asset.

### 4.2 Public Asset Manifest

Every public binary asset needs a text manifest entry. The manifest is
canonical because GitHub cannot inspect a binary license reliably.

```json
{
  "path": "Content/Generated/World/Example.uasset",
  "sha256": "<content-hash>",
  "engine_version": "5.7",
  "creation_class": "alis_owned_art",
  "alis_license": "CC-BY-4.0",
  "contains_program_logic": false,
  "epic_content_in_source": false,
  "source_ledger_ids": ["source-example"],
  "distribution_class": "public_asset"
}
```

Use `alis_license: null` when ALIS has no right to apply its own license.

## 5. Data Suitability

### 5.1 Preferred Inputs

| Source class | Intended use | Policy |
|---|---|---|
| Overture Base, Buildings, Divisions, and Transportation when the pinned release identifies them as ODbL | Roads, footprints, boundaries, base features | Approved with ODbL isolation, attribution, and source-release pinning |
| OpenStreetMap extracts | Roads, buildings, POIs | Approved with ODbL obligations |
| Copernicus GLO-30/GLO-90 | Regional terrain baseline | Approved with exact provider notices; remember it is a DSM |
| OpenTopography datasets | DTM, LiDAR, and terrain improvements | Dataset-by-dataset approval; acknowledge the source and service |
| Owned surveys and photography | Hero terrain and landmarks | Approved when releases and ownership are recorded |
| Local government data under CC0, CC BY 4.0, ODbL, or equivalent open terms | Regional corrections | Approved only after the exact dataset terms are recorded |

### 5.2 Conditional Inputs

- Overture Places and Addresses: theme and source licenses differ.
- Municipal GIS portals: "open portal" does not establish a license.
- CityJSON, CityGML, BIM, and LiDAR: the format is open, but each dataset has
  its own terms.
- Cesium ion and other hosted tiles: client software licensing does not grant
  export or redistribution rights to hosted content.
- Fab assets marked CC BY: follow the exact listing license and attribution,
  not the Fab Standard License summary.
- Academic or non-commercial datasets: local research only unless commercial
  production and redistribution are expressly allowed.

### 5.3 Forbidden Inputs

- Google Maps, Google Earth, or Street View used to trace, digitize, model,
  reconstruct, or extract terrain, roads, buildings, or imagery.
- Scraped photographs, map tiles, or 3D models without explicit permission.
- Data with no identifiable license.
- NoDerivatives data that must be transformed by the compiler.
- NonCommercial data intended for a commercial or publicly redistributable
  ALIS build.
- Provider credentials, API keys, signed URLs, or account-bound downloads.

## 6. Tool Integration Policy

### 6.1 Permissive Candidates

The following are suitable candidates when their exact version, binary bundle,
dependencies, and notices are recorded:

- GDAL core: MIT; optional drivers and binary bundles may differ.
- PROJ: MIT.
- PDAL: BSD.
- DuckDB: MIT.
- Cesium for Unreal: Apache-2.0; Cesium ion is separate.
- 3DCityDB: Apache-2.0.
- COLMAP: BSD; its dependencies remain separately licensed.
- OpenUSD and TerriaJS: Apache-2.0.

### 6.2 Separate-Process Candidates

Keep tools with incompatible or uncertain dependency graphs as separate
executables or services unless a dependency review approves tighter
integration:

- PostGIS: GPLv2; ordinary database use does not license the client.
- SUMO: EPL-2.0 with a GPL secondary option.
- OpenMVS: AGPL; prefer CLI isolation and do not copy its code into UE modules.

OSM2World is currently MIT-licensed and can be evaluated as a permissive
dependency. Pin the reviewed commit because older releases and documentation
used different terms.

### 6.3 Proprietary Accelerators

The canonical rebuild path must work without proprietary authoring tools or
hosted services. Such tools may accelerate local work only when their output
can be rebuilt and validated by the canonical open pipeline.

The final Unreal projection and cook necessarily use Unreal under Epic's
terms. Here, "canonical rebuild" means that engine-independent source
snapshots, normalized data, manifests, and notices do not depend on Houdini,
Cesium ion, or another optional proprietary accelerator.

Houdini Engine for Unreal currently offers free commercial plugin licenses,
but the software remains proprietary and performs license checks. Batch,
custom HAPI, farm, Indie, and full commercial uses have different eligibility,
machine-count, and license requirements. Houdini may be an optional authoring
accelerator; it must not be required to compile canonical ALIS world data or
run CI.

### 6.4 Deferred Infrastructure

MinIO AIStor Free currently permits proprietary single-node use, including
commercial production, but prohibits modification and redistribution. Its
distributed and high-availability features require other licensing. The
former AGPL MinIO repository was archived on 2026-04-25.

This corrects the previous evaluation-only claim, but it does not make AIStor
an open, decentralized ALIS foundation. The first slice should use local
files. Select object storage only when scale requires it and after a current
license and operations audit.

## 7. Distribution Boundary

The existing public mirror is text-only and excludes binaries and Git LFS
pointers. Publishing approved `.uasset` files therefore requires a separate
public asset repository, a dedicated GitHub release, or an explicit future
change to the mirror policy.

| Artifact | Main source mirror | Public asset/data release | Private developer cache | Packaged game |
|---|---|---|---|---|
| Code, schemas, docs, ledgers, notices | Yes | Optional | Yes | As needed |
| Synthetic text fixtures | Yes | Optional | Yes | No |
| Small redistributable real-data fixture | Conditional | Preferred | Yes | Optional |
| Raw global DEM, LiDAR, imagery, or Overture release | No | Only if approved and useful | Yes | No |
| ODbL canonical database or alteration file | No by default | Yes, under ODbL | Yes | Offer separately when required |
| Entirely ALIS-owned `.uasset` | No under current mirror rule | Yes, with manifest approval | Yes | Yes |
| Restricted third-party source `.uasset` | No | No | Licensed collaborators only | Cooked and inseparable when permitted |
| Cooked pak/IoStore content | No | Release channel only | Yes | Yes |
| API credentials or signed access material | Never | Never | Secret storage only | Never |

Fab Standard assets may be shared privately with collaborators working on the
project, but may not be redistributed publicly on a standalone basis. Public
GitHub is not a private collaborator repository.

## 8. Reproducible Developer Workflow

Developers must be able to rebuild approved artifacts without ALIS
redistributing every raw source:

1. Select an approved entry from the source ledger.
2. Fetch the pinned release from the official provider into an ignored cache.
3. Complete any required account or license acceptance without automation
   bypasses.
4. Verify the downloaded content hash.
5. Normalize into an immutable source snapshot.
6. Compile canonical features and record all source-ledger IDs.
7. Generate Unreal projections with the pinned compiler and UE version.
8. Generate attribution and third-party notices from the same ledger.
9. Compare semantic manifests; do not require byte-identical `.uasset` files.

CI should use synthetic fixtures or small redistributable snapshots. It must
not depend on personal API keys or volatile public APIs.

## 9. Required Source Ledger

The ledger is a directed graph, not one `license` field on an output.

Node classes:

- source dataset or source asset;
- authored overlay, procedural rule, or reusable content;
- compiler, tool, and dependency version;
- canonical or derivative database;
- generated artifact; and
- release, notice bundle, or source offer.

Every node records stable ID, version, hash, owner, exact license or terms,
commercial-use permission, redistribution permission, required notices,
reviewer, evidence version or hash, and primary evidence.

Every edge records:

- relationship: `derived_from`, `embeds`, `references`, `generated_by`, or
  `packages`;
- source feature IDs or spatial coverage where available;
- whether material remains extractable;
- transformation or alteration method;
- database versus Produced Work classification; and
- obligations inherited by the destination.

Each release node calculates:

- `redistributable`;
- `commercial_use`;
- required output license or provider terms;
- attribution and modification notices;
- source or alteration offer;
- forbidden dependencies; and
- unresolved review items.

Compilation and publication must fail when any required field, edge, license,
notice, source offer, policy result, or content hash is missing.

## 10. Release Gates

A public asset, data release, or packaged build must fail review when:

- any source has an unknown or forbidden policy;
- a public binary lacks a matching manifest and hash;
- required attribution is absent;
- an ODbL Derivative Database or alteration method is not offered when needed;
- Epic/Fab/Marketplace source content appears in a public-source artifact;
- a NonCommercial or NoDerivatives input reaches an incompatible release;
- a service output is treated as owned data without export rights;
- the exact tool and data versions cannot be reproduced; or
- a component assignment, rightsholder, or required permission is unresolved
  for material included in the artifact.

## 11. Primary Sources

- [Unreal Engine EULA](https://www.unrealengine.com/eula/unreal)
- [Epic Content License Agreement](https://www.unrealengine.com/eula/content)
- [Fab Standard License](https://www.fab.com/eula?lang=en)
- [ODbL 1.0](https://opendatacommons.org/licenses/odbl/1-0/)
- [CDLA Permissive 2.0](https://cdla.dev/permissive-2-0/)
- [OSMF Produced Work guideline](https://osmfoundation.org/wiki/Licence/Community_Guidelines/Produced_Work_-_Guideline)
- [Overture attribution and licensing](https://docs.overturemaps.org/attribution/)
- [Copernicus DEM](https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM)
- [OpenTopography terms](https://opentopography.org/usageterms)
- [Google Maps Platform Terms](https://cloud.google.com/maps-platform/terms)
- [GNU FAQ on program output](https://www.gnu.org/licenses/gpl-faq.html#WhatCaseIsOutputGPL)
- [GDAL license](https://gdal.org/en/stable/license.html)
- [PROJ license](https://proj.org/en/stable/about.html#license)
- [PDAL license](https://pdal.io/en/stable/copyright.html)
- [DuckDB license statement](https://duckdb.org/faq)
- [Cesium for Unreal](https://github.com/CesiumGS/cesium-unreal)
- [PostGIS GPL guidance](https://postgis.net/documentation/faq/gpl-license/)
- [SUMO downloads and licensing](https://eclipse.dev/sumo/docs/Downloads.html)
- [OSM2World MIT license](https://github.com/tordanik/OSM2World/blob/master/LICENSE.txt)
- [SideFX Houdini Engine licensing](https://www.sidefx.com/faq/question/how-does-houdini-engine-licensing-work/)
- [SideFX Houdini Indie restrictions](https://www.sidefx.com/faq/indie-new/)
- [MinIO AIStor Free agreement](https://www.min.io/legal/aistor-free-agreement)
- [Archived MinIO AGPL repository](https://github.com/minio/minio)
