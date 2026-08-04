# 03 Unreal World Realization

Status: complete; synthetic and pinned Kazan realization, deterministic
regeneration, ownership, GeoReferencing, and Landscape gates pass.

## Responsibility

Own the narrow adapter for the target UE 5.8 baseline that validates canonical
ALIS JSON and realizes engine-native content. Do not read raw provider data,
redefine canonical identity, or make generated Unreal assets authoritative.

## Engine Boundary

The
[engine-upgrade task](../../tools/engine_version_update_sot.md) owns
candidate identity, migration, build, packaging, plugin, and core-regression
work. This file owns no engine-migration checklist. That gate has passed;
canonical input readiness now controls when world realization begins.

The identical compiler fixture becomes the post-upgrade compatibility proof.
P0 has no required Experimental or optional 5.8 authoring dependency.
Opportunity and maturity evidence remains in
[audit C-03](research_audit.md#c-03---preserve-the-ue-version-boundary).

Official Unreal MCP and its editor-only toolsets may be exercised from the
first slice for supervised inspection, PCG experimentation, and adapter
debugging. They remain optional control surfaces: commandlets, canonical JSON,
and automation tests own reproducibility and acceptance.

## Contract

```text
canonical cell JSON and manifest
    -> ProjectWorld validation and narrow import adapter
    -> ProjectPCG and stock UE realization
    -> disposable World Partition actors and assets
```

The durable import and validation authority is a C++ commandlet plus
automation tests, invoked through the same noninteractive top-level route as
the compiler. MCP may wrap that service for inspection or debugging, but is
not a P0 dependency and cannot contain a separate import algorithm.

## Landscape Projection

Use one logical World Partition Landscape for the baked Kazan prototype, not
one Landscape actor per compiler cell. Before real import, freeze the full
intended Kazan prototype envelope and a stock UE component layout that
satisfies Landscape heightmap and component constraints; compiler cells,
Landscape components, and World Partition cells remain separately mapped
identities.

Project the canonical grid into UE-valid tiled 16-bit heightmaps without
engine resampling. Initial import creates the Landscape with Edit Layers;
validated Subregion or component-aligned imports update only affected terrain.
Use `Generated Base`, `Generated Roads`, and protected `Authored Corrections`
layers. Regeneration may replace generated layers but must preserve authored
corrections. Landscape Patch and Experimental authoring features are not P0
dependencies.

## Tasks

- [x] Record the JSON-to-UE adapter contract in the owning stable ProjectWorld
  and ProjectPCG documentation.
- [x] Implement the commandlet and automation-test entry points with explicit
  inputs, structured results, stable exit behavior, and no manual editor
  interaction.
- [x] Enable only the stock UE plugins required by the proven implementation.
- [x] Validate schema version, cell identity, hashes, provenance result, and
  coordinate metadata before any editor mutation. Exact coverage and
  provenance versions, coverage-to-manifest identity, terrain bounds/spacing,
  and manifest-to-feature ownership fail closed at the same boundary.
- [x] Map canonical coordinates into the selected UE origin and World
  Partition layout through GeoReferencing, with canonical terrain row zero at
  the northern edge.
- [x] After the next editor restart, prove ProjectWorld and GeoReferencing
  load, then run projected -> Unreal -> projected on the canonical fixture.
  Fail above the canonical quantization tolerance and include the measured
  round-trip error in the commandlet's machine-readable result.
- [x] Validate the Kazan envelope against stock Landscape dimensions and
  component limits, then materialize bounded terrain, one cross-cell road, and
  small building massing in that layout.
- [x] Import UE-valid tiled 16-bit heightmaps into one logical World Partition
  Landscape and update only validated component-aligned subregions.
- [x] Use ALIS-authored or procedurally generated placeholder geometry and
  materials; do not require Starter Content, Fab, or Marketplace assets.
- [x] Keep world-specific recipes outside reusable ProjectPCG services.
- [x] Preserve stable source identity on realized actors and assets.
- [x] Keep generated roots separate from protected authored content and
  remove stale owned outputs without touching non-owned assets.
- [x] Preserve the protected authored Landscape layer when generated base or
  road layers are reimported.
- [x] Ensure no public `.uasset` embeds Epic, Fab, Marketplace, or unapproved
  third-party source content.
- [x] Log import inputs, decisions, created/updated/removed artifacts,
  validation failures, duration, and output sizes.
- [x] Support clean deletion and semantic regeneration without manual repair.

## Exit Gate

Both canonical cells import into the target UE 5.8 baseline with correct
placement, no boundary seam or duplicate, protected authored content intact,
and semantically repeatable generated content.

Result: passed. The Landscape-compatible `31 x 31` canonical grid was
re-certified through all 58 World tool tests, a pinned full build, 13 stable
deterministic documents, and an eastern incremental run that processed one
terrain cell while reusing the west. Unreal build and all nine realization
tests pass. The accepted Kazan result verifies 14 receipt outputs, uses the
grid-owned `0.0 m` vertical origin, reports zero placement error across nine
GeoReferencing probes, and realizes `alis:osm:way:1151612452` as one identity,
two cell fragments, and one shared boundary point. Reapplying produces the
same semantic fingerprint, updates zero unchanged Landscape components, and
preserves the authored correction layer. A focused regression lowers a terrain
minimum without moving the Landscape or an untagged authored actor.
