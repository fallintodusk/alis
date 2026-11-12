# Building realization

Owns the registered building projection: unselected massing and admitted modular assemblies.

| Concern | Owner |
|---|---|
| Classification and selected post-classification emission | [Massing contract](Private/ProjectWorldBuildingMeshBuilder.h) and [builder](Private/ProjectWorldBuildingMeshBuilder.cpp) |
| Qualified prototype rectangle, complete physical support and remaining massing | [Assembly projection](Private/ProjectWorldBuildingAssembly.h) and [implementation](Private/ProjectWorldBuildingAssembly.cpp) |
| Production massing lifecycle | [Realization](Private/ProjectWorldBuildingRealization.cpp) |
| Selection admission, per-cell inputs, shared kit assets and persistent assemblies | [Selection contract](Private/ProjectWorldBuildingSelection.h), [qualification](Private/ProjectWorldBuildingSelection.cpp) and [persistence](Private/ProjectWorldBuildingSelectionPersistence.cpp) |
| Accepted geometry authority split | [World contract](../../docs/territory_contract.md#building-geometry-authority-split) |

The editor adapter constructs inputs for the separate
[assembly component](../../../ProjectBuildingAssembly/README.md). Assembly never
imports World internals. The existing buildings producer owns selection and
suppression together; concrete production profiles require separate admission.
Selection belongs with that classifier: excluding input before overlap admission
can change unrelated buildings. If this builder needs further growth, separate
classification from emission while retaining that ordering.

Set the optional buildings setting `assembly_source` to
`BuildingAssembly/<selection>.json`, relative to the selected WorldData `Data/`
directory. The [selection schema](../../Data/Schemas/project_world_building_selection.schema.json)
defines a version-one document with at most five `feature_id` / `kit_source`
records. Kit paths are repository-relative original Assembly `Data/Kits/` JSON.
Only TestData may instead consume confined `tmp/world/inputs/` documents. All
selected inputs must qualify before the complete replacement union is emitted;
an empty configured selection still checks its planned input identity. Omitting
the setting retains the massing-only route.
