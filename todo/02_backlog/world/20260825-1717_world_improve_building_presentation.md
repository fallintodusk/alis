# Improve Generated Building Presentation

Status: researched; implementation not selected

## Current evidence

- Kazan currently realizes `project_building_massing:v2` as cell-owned Nanite
  StaticMeshes while its presentation profile selects Unreal's debug
  `VertexColorMaterial`.
- The active v2 producer already consumes logical buildings and effective
  volumes, including admitted `building:part` geometry. The earlier v1
  part-admission limitation is superseded and must not be carried forward.
- `ProjectWorldBuildingRealization::Apply()` can leave a geometry-current mesh
  untouched before material reassignment. A future material-only change needs
  its own assignment/locality proof rather than forcing geometry dirty.
- ProjectMaterial currently implements only its accepted terrain family; it
  does not yet provide a reusable building material.

The old package census and visual captures are not current authority. Recount
the active manifests and capture a fresh control before implementation.

## Accepted candidate

The smallest retained candidate is one universal, textureless Default-Lit
building-massing material owned by ProjectMaterial and selected through a
ProjectWorld semantic binding. It may improve wall/roof readability, but it
must not invent facade, district, landmark, roof-shape, or height semantics.

Keep canonical footprints, effective volumes, heights, generated geometry,
cell ownership, Nanite, collision, actor identity, and manifest semantics
unchanged. Kazan is the first fixture, never a reusable-code branch.

## Remaining acceptance

1. Select this concern against the other presentation backlog work.
2. Recount active Building artifacts and capture matched player, oblique, and
   aerial control views.
3. Add focused tests proving material assignment on both geometry mutation and
   geometry no-op paths without rebuilding unchanged mesh descriptions.
4. Generate and authenticate the reusable material through ProjectMaterial;
   ProjectWorld owns only binding, validation, and assignment.
5. Prove visual improvement, unchanged geometry/authority, package-size impact,
   and the fixed packaged frame budget before promotion.

Start from [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md)
and [ProjectMaterial](../../../Plugins/Resources/ProjectMaterial/README.md).
