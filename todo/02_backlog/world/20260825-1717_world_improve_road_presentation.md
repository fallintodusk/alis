# Improve Generated Road Presentation

Status: material scope implemented and adopted; final packaged acceptance is
coordinated by [World Surface Presentation](../../00_current/20261008-1543_world_improve_surface_materials.md).
This retained research record is not a parallel implementation task.

## Implemented scope

AsphaltMacro is visually accepted for default asphalt in both cities. Reusable
aggregate recipes generate shared Signals/Normal outputs through ProjectTexture;
ProjectMaterial owns shading, metric projection and appearance. ProjectWorld
consumes the authenticated semantic binding. D39's material-reference transaction
and cold replay preserve geometry without mesh builds. Earlier textureless and
soil-based road samples remain rejected/historical controls, not selected defaults.

Road topology, class widths, terrain conformity and junction correctness belong
to [Road and Water Alignment](../../00_current/20261008-1543_world_investigate_road_water_alignment.md).
D45's separate geometry rollout is an accepted baseline and must not be repeated
for appearance changes. Exact final packaged city cost remains open; isolated
selected-resource timings do not certify the fixed frame budget.

Stable owners: [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md)
and [material generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md).
