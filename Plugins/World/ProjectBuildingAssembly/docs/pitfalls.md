# Building assembly pitfalls

## Missing or removed assemblies in an accepted layer

- Symptom: cold reconstruction omits assemblies when inputs are unchanged, or
  deselection leaves an unused kit package in the exact layer population.
- Root cause: input dirtiness was mistaken for output existence; retired actors
  still held package loaders, and their last shared kit remained owned on disk.
- Fix: reconstruct absent expected actors independently of input dirtiness;
  destroy retired actors through the editor, release loaders, and retire only
  their referenced owned kit packages that no admitted selection still uses.
- Owner: [selection persistence](../../ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingSelectionPersistence.cpp).
- Regression: cold reconstruction and complete deselection in the
  [registered lifecycle](../Tests/README.md#registered-world-lifecycle), including
  exact population checks and outer restoration.

## Saving an external actor before its map has a persistent name

- Symptom: initial generation or cold reconstruction refuses a partially loaded
  actor package under a temporary map name.
- Root cause: an external actor package was saved before the map owner assigned
  the final map package and rehomed its external actors.
- Fix: defer that actor save while the map is temporary. The existing map save
  owns initial persistence; later updates self-save the real external package.
- Owner: [selection persistence](../../ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingSelectionPersistence.cpp).
- Regression: the [registered lifecycle](../Tests/README.md#registered-world-lifecycle)
  exercises both first admission and cold reconstruction.

## Inward-facing generated boxes

- Symptom: saved modules pass position comparisons but their exterior faces
  disappear or light incorrectly.
- Root cause: the Static Mesh build uses UE's clockwise face convention. A
  conventional counterclockwise box and double-sided collision can hide the
  orientation error from geometric and trace checks.
- Fix: emit clockwise triangles, matching outward normals and hard box edges;
  retain one-sided collision.
- Owner: [native realization](../Source/ProjectBuildingAssemblyEditor/Private/ProjectBuildingAssemblyRealization.cpp).
- Regression: the [native case](../Tests/README.md#source-bound-native-case)
  independently measures signed compiled volume and normal orientation after
  save/reload, then captures all four part families.

## Directional-light root defaults in diagnostic captures

- Symptom: a requested diagonal light becomes nearly vertical and leaves walls
  dark, despite valid meshes and two different opaque materials.
- Root cause: the native directional-light root has its own default rotation,
  which composes with the spawn transform.
- Fix: explicitly set actor rotation after spawn and record the actual light
  direction; do not compensate with exposure or mesh changes.
- Owner and regression: `Project.World.BuildingAssembly.SavedRender`, reached
  through the [native verification route](../Tests/README.md#source-bound-native-case).

## Package collection during map replacement

- Symptom: a presentation test crashes while unloading a package after opening
  a new map.
- Root cause: map replacement can collect the old package, invalidating a raw
  package pointer retained across that operation.
- Fix: resolve the package again after replacement and unload only if retained;
  independently prove old actors are gone before reloading the saved candidate.
- Owner and regression: `Project.World.BuildingAssembly.SavedRender` on the
  [saved native preview](../Tests/README.md#source-bound-native-case).
