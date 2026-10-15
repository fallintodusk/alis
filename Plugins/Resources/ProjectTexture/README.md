# ProjectTexture

Reusable texture resources and structural surface-pattern authority.

## Documentation

- [Contracts, architecture and diagrams](docs/README.md)

## Ownership

ProjectTexture owns patterns that can be reused by multiple material families: macro
breakup, soil/stone structure, detail-normal structure, grain, pores, cracks, and
scratches. It does not own surface meaning, tint, roughness, wetness, metric scale, or
consumer-specific blend policy; those belong to ProjectMaterial or the canonical
domain owner.

Closed JSON recipes select a reviewed algorithm and bounded structural parameters.
They never serialize arbitrary Unreal expression graphs. Ordinary appearance changes
therefore do not create another texture or function asset.

## Runtime boundary

`ProjectTexture` is the small Runtime resource module. `ProjectTextureEditor` owns
recipe validation, deterministic identity, compiler fingerprinting, and accepted
manifest serialization. Packaged targets do not parse recipes or load Editor compiler
code.

The first implementation is the `project_terrain_structure` runtime DAG; see
[pattern generation](docs/pattern_generation.md). Its stable exported slot is:

```text
Data/Patterns/Terrain/project_terrain_structure.pattern.json
-> /ProjectTexture/Patterns/Terrain/DA_ProjectTerrainStructureCatalog.RT_ProjectTerrainStructure
```

The catalog and render-target descriptor are persistent generated contracts; their
pixels and runtime cache entries are disposable. The recipe and accepted manifest
remain the authority. Generated resource cost scales with unique reusable structure,
not world population or consumer count. Explicit asynchronous bundle producers reuse
the same owner admission, stable slots and cache; their contract is documented in
[opt-in asynchronous bundle producers](docs/pattern_generation.md#opt-in-asynchronous-bundle-producers).
Known asynchronous lifecycle failures and their regressions are recorded in
[ProjectTexture pitfalls](docs/pitfalls.md).
The default has no runtime Texture Graph dependency. Experimental native integration
remains a private opt-in fixture, with no production enrollment. Custom virtual-texture
producers, disk caches and per-city/per-cell outputs are outside this contract.
