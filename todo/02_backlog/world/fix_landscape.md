# Qualify authored Landscape work

Status: BACKLOG - target map, actors and content owner must be qualified before
implementation. No generated-world terrain work is selected by this task.

## Scope qualification

The retained requests mention `Test_2`, `Persistent`, `StartLocation` and several
landmarks, but do not establish one exact map package or owner. These names are
historical clues, not a confirmed target. Separately authored Landscape is valid;
do not infer that this task is obsolete or that it targets City17.

- [ ] Identify the exact authored map package, affected actors and owning content
  plugin from the selected case. Confirm its existing Landscape backend.
- [ ] Select one retained issue and record its failing behavior and acceptance
  evidence before implementation. Re-scope a request that belongs to another
  owner instead of treating this list as a combined generation campaign.

The [generated versus authored backend boundary](../../../Plugins/World/ProjectWorld/docs/world_partition.md#generated-terrain-representation)
remains authoritative. This task cannot introduce a generated Landscape producer,
fallback or migration, replace Mesh Terrain, or change canonical Terrain/Water.

## Historical work candidates

The requests below remain unqualified until the scope checks above pass. Tool
research is a historical proposal, not authorization for a new terrain backend.

### Setup & Generation

- [ ] Move Actor Landscape from Persistent to StartLocation.
- [ ] Research terrain generation plugin with 3D house prototype support.

### Terrain Design & Materials

- [ ] Align terrain with shop buildings.
- [ ] Replace landscape layer, do export/import, sort landscape.
- [ ] Research ground/soil/meadow/rock packs in Test_2 for replacement.
- [ ] Change asphalt tiling, adjust gap and terrain elevation.

### Splines & Roads

- [ ] Fix road material rotation near Khrushchev building, dormitory, store. Research feeding spline directions into material.
- [ ] Research road creation tools for quick road/intersection generation.
