# World slice 3D - Kazan building massing

## Outcome

Realize accepted Kazan building footprints as deterministic, cell-owned blockout
massing with player collision, incremental regeneration, durable authority, and
fresh Editor/MCP visual evidence.

## Stable routes

- [Territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition contract](../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [Building authority split](../../Plugins/World/ProjectBuildingAssembly/docs/decision_record.md)
- [World pipeline layer gates](../../docs/testing/world_pipeline_layers.md)
- [Initiative roadmap](../02_backlog/world/world_generate_kazan_territory_roadmap.md)

## Frozen boundary

Owner: `project_building_massing:v1` in ProjectWorld.

Input/output contract:

```text
accepted canonical building polygons + resolved height + terrain dependency
  -> deterministic cell-local topology admission
  -> clipped static-mesh massing per occupied canonical cell
  -> Nanite + complex-as-simple collision + spatial OFPA actor
  -> existing layer inventory, transaction, manifest, and rollback path
```

Expected changed components:

- realization tuple/profile schema and Kazan/test profiles;
- new ProjectWorld building producer and mesh/admission helper;
- building-focused result inventory, tests, and validation expectations;
- stable ProjectWorld building realization contract;
- generated building artifacts/manifests and acceptance evidence.

Expected untouched components:

- canonical source, compilation authority, and accepted canonical bundle;
- terrain, water, road, and vegetation producer behavior;
- transaction, rollback, enrollment, fingerprint-v2, and runtime-state owners;
- ProjectBuildingAssembly and all facade/interior/roof systems;
- bounded P0/representative preview behavior.

Stop on substantial propagation across an untouched owner or a required change
to canonical authority.

## Characterization baseline

Accepted Kazan authority `86821a9914b758e...` contains:

- 30,196 building features and 30,288 polygon parts;
- 78 holes and zero structurally malformed rings;
- 85 source-derived heights, 13,289 levels-derived heights, and 16,822
  admitted procedural defaults;
- one exact duplicate pair, 10 strict containment pairs, 75 unresolved
  positive-overlap pairs, and one self-intersecting footprint.

The producer must classify these cases deterministically. Exact duplicates use
one stable representative; contained footprints associate with the containing
footprint without duplicate render/collision ownership; partial positive
overlaps and malformed geometry reject only the affected cell fragments with
feature identities and reasons. Zero-area boundary contact is valid.

## Execution

- [x] Add regression-first profile, topology, mesh, locality, removal, no-op,
  collision, OFPA, Nanite, and no-HLOD tests.
- [x] Implement the registered building tuple and deterministic cell input hash.
- [x] Implement holes/multipolygons, topology admission, clipping, extrusion,
  terrain anchoring, persistent mesh assets, and spatial actors.
- [x] Add building inventory/result evidence without moving existing producer
  fingerprints.
- [x] Add the building layer to Kazan and layered test profiles.
- [x] Run focused build/tests until stable.
- [x] Run one fresh Common Check, one final Kazan Matrix, authorized L3
  enrollment, and strict authority/LFS audit.
- [x] Reload through the supported Editor/MCP path and capture multiple real
  territory locations with terrain, water, roads, vegetation, and buildings.
- [x] Re-read changed owners/consumers, archive 3D, and route minimal 3E.

## Acceptance evidence

- Focused build and exact Unreal building/profile/inventory tests: accepted.
- Common Check: `check-20260824T084651Z`.
- Final Kazan Matrix: `run-20260824T085021Z`.
- Durable enrollment: `enroll-20260824T091046Z`, active-set SHA-256
  `69a5fef6530880f282e8d1569f58e996034993ef54076348c7cc605bc628ae1c`.
- Strict authority audit:
  `Saved/Validation/WorldAuthority/audit_3d_buildings_20260824T0913Z.json`;
  9 scopes, 1,368 artifacts, 38,190,992 byte-identical bytes, no drift.
- Live Editor/MCP visual evidence:
  `Saved/Validation/WorldVisual/3d-buildings-20260824T091853Z/capture.json`;
  9 authenticated 1920x1080 views, 171 spatial building cell actors spanning
  the territory, and Map Check at 0 errors / 0 warnings.
- Visual review accepted city-scale massing and representative close views.
  No origin clustering, gross displacement, floating/sunken massing, absurd
  height, or city-scale omission was observed.

## Out of scope

Interiors, facades, roofs, architecture-style inference, modular assembly,
asset-library work, city-wide AI navigation, HLOD, and art polish.
