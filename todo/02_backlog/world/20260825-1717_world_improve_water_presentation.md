# Improve Generated-World Water Presentation

Status: researched; implementation not selected

## Current evidence

- ProjectWorld currently owns a generator-local, solid opaque blue
  `M_ProjectWorldWater` placeholder through `BuildMaterial()`.
- The water realization profile keeps `material_shading_model`, surface offset,
  and non-Nanite policy in the layer contract; the generated material is part
  of the World water artifact inventory.
- Water cell meshes serialize the material reference. Moving to a reusable
  material changes those mesh package bytes even when geometry is unchanged.
- ProjectMaterial currently implements only its accepted terrain family. It
  does not yet provide the researched water archetype.

The prior artifact count and captures are not current authority. Recount the
active manifest and capture a fresh solid-opaque control before implementation.

## Accepted candidate

If this concern is selected, the retained W1 candidate is one textureless
ProjectMaterial-owned Single Layer Water parent/default instance, consumed by
ProjectWorld through a closed `water.default` binding. ProjectWorldData keeps
only polygons, elevations, behavior, and sourced semantic facts.

The first candidate excludes Epic Water lifecycle, WaterBody actors, WPO,
displacement, foam, flow maps, underwater behavior, buoyancy, Niagara, custom
HLSL, and a second geometry authority. World-space animation must remain
continuous across cell boundaries.

## Migration boundary

- Remove the local graph builder only after the reusable material exists and
  authenticates successfully.
- Separate material identity from water geometry dirty identity so later
  same-path tuning does not rewrite World geometry or advance its manifests.
- Perform the first reference cutover as an explicit material-reference
  migration. Preserve mesh description, topology, bounds, transforms,
  collision, navigation, and actor/map package identity where those packages
  do not store the changed reference.
- Recount every affected mesh and prove rollback before mutation; do not reuse
  historical counts as a gate.

## Remaining acceptance

1. Select Water against the other presentation backlog work.
2. Capture a fresh control and exact active artifact/reference census.
3. Add focused material-generation, binding, inventory, locality, migration,
   and rollback regressions.
4. Compare matched shore, oblique, aerial, and cross-cell views against W1.
5. Prove unchanged canonical geometry, packaged streaming behavior, package
   impact, and the fixed packaged frame budget before promotion.

Start from [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md)
and [ProjectMaterial](../../../Plugins/Resources/ProjectMaterial/README.md).
