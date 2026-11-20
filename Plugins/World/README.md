# World Plugins

World plugins own reusable generation and realization, concrete public World
data, synthetic fixtures, and optional World integrations.

| Goal | Owner |
|---|---|
| Inspect the City17 World plugin | [City17](City17/README.md) |
| Inspect the menu World plugin | [MainMenuWorld](MainMenuWorld/README.md) |
| Route optional procedural integrations | [PCG](PCG/README.md) |
| Understand or extend reusable World realization | [ProjectWorld](ProjectWorld/README.md) |
| Maintain the replaceable MeshPartition terrain adapter | [ProjectWorldMeshTerrain](ProjectWorldMeshTerrain/README.md) |
| Inspect concrete Kazan and Manhattan source or generated authority | [ProjectWorldData](ProjectWorldData/README.md) |
| Run deterministic synthetic World fixtures | [ProjectWorldTestData](ProjectWorldTestData/README.md) |
| Extend building assembly | [ProjectBuildingAssembly](ProjectBuildingAssembly/README.md) |
| Maintain environmental time and optional demo controls | [ProjectTime](ProjectTime/README.md) |
| Maintain scene lighting and geographic daylight | [ProjectLight](ProjectLight/README.md) |
| Run engine-independent World workflows | [World tools](../../tools/World/README.md) |

World tooling owns its own setup and command lifecycle; this plugin router does
not duplicate it.
