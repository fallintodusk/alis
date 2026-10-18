# Improve Generated-World Water Presentation

Status: DONE for the release material-development scope; adopted defaults and
exact post-refinement packaged evidence are closed in [World Surface Presentation](20261008-1543_world_improve_surface_materials.md).
This retained research record is not a parallel implementation task.

## Implemented scope

RiverCalm is visually accepted for calm river water in both cities. ProjectMaterial
owns its recipe and shared native Single Layer Water shader family, reusing Epic's
wave functions and normal resource. It does not invent a ProjectTexture dependency
where the selected physical family needs none. ProjectWorld consumes the accepted
semantic binding instead of building its former opaque-blue local shader.

D39's reference-only transaction, saved-child rollback and cold replay preserve
mesh geometry/build data and separate geometry lineage from material adoption.
Material-only appearance changes do not advance geometry identities or require
World/canonical regeneration. Optional obsolete diagnostic-package cleanup is not
a prerequisite for adopting the accepted material.

World-space animation remains continuous across cells. WaterBody lifecycle,
simulation, WPO/displacement, foam, flow maps, underwater behavior, buoyancy and a
second geometry authority remain outside this release material scope.

## Remaining boundary

Intersections and shoreline conditioning belong to
[Road and Water Alignment](../../00_current/20261008-1543_world_investigate_road_water_alignment.md).
Attractive water does not accept those geometric defects. The exact post-refinement packaged
material-development proof accepts Water visibility, measured pass/frame cost
and streaming. Final source-frozen release certification remains separate; this
does not accept the outstanding geographic defects.

Stable owners: [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md)
and [material generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md).
