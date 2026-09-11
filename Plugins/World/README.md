# World Plugins

World plugins own reusable generation and realization, concrete public World
data, synthetic fixtures, and optional World integrations.

| Goal | Owner |
|---|---|
| Inspect the legacy Old City 17 plugin | [City17](City17/README.md) |
| Inspect the menu World plugin | [MainMenuWorld](MainMenuWorld/README.md) |
| Route optional procedural integrations | [PCG](PCG/README.md) |
| Understand or extend reusable World realization | [ProjectWorld](ProjectWorld/README.md) |
| Inspect concrete Kazan and Manhattan source or generated authority | [ProjectWorldData](ProjectWorldData/README.md) |
| Run deterministic synthetic World fixtures | [ProjectWorldTestData](ProjectWorldTestData/README.md) |
| Extend building assembly | [ProjectBuildingAssembly](ProjectBuildingAssembly/README.md) |

Engine-independent acquisition, canonical compilation, validation, and visual
verification tools are routed separately from [tools/World](../../tools/World/README.md).

## First World Tool Run

After installing the matching developer payload and configuring Unreal Engine,
bootstrap the pinned World toolchain once:

```powershell
python -S tools/World/SourceIngestion/bootstrap.py bootstrap-tools
```

The command creates the pinned Python environment, then downloads and verifies
the ignored, content-addressed geospatial runtime. Subsequent generation
commands reuse it. Continue at the
[World tools router](../../tools/World/README.md) for source ingestion,
canonical compilation, realization, and verification.
