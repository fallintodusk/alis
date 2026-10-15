# Improve Generated Building Presentation

Status: material scope implemented and adopted; final packaged acceptance is
coordinated by [World Surface Presentation](../../00_current/20261008-1543_world_improve_surface_materials.md).
This retained research record is not a parallel implementation task.

## Implemented scope

- D48's delegated visual judgment accepts Concrete Macro192 for default building
  massing in Kazan and Manhattan. ProjectMaterial owns its authenticated recipe,
  shared triplanar aggregate family and semantic binding; ProjectTexture owns the
  shared generated Signals/Normal bundle and cache.
- The existing reference-only transaction updates 312 Building mesh material
  slots without mesh builds, actor writes or geometry changes. Cold reload/replay
  retains source/render/Nanite/collision identities and writes nothing.
- Both-city saved-binding PIE views and motion retain fine relief and broad
  variation. These are simple concrete masses, not facades, windows, district
  styles, landmarks or additional sourced building semantics.
- D43's separate winding correction is an accepted geometry baseline. Material
  changes do not repeat that rollout or canonical compilation.

## Remaining boundary

Exact final packaged city material cost and gameplay collision, including the
retained Manhattan fallback sample, remain with the current presentation concern
and release plan. No separate building-material implementation remains here.

Stable owners: [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md),
[material generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md)
and [pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md).
