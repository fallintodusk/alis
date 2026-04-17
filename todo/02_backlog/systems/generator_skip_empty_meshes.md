# Generator: Skip Default Mesh for Meshless Objects

## Problem

When an object definition JSON has no `meshes` field (trigger-only objects like SniperZone, GasCloud), the generator still creates a default empty mesh entry in the UAsset. This shows as an empty "body" mesh in the definition editor and triggers thumbnail warnings ("no supported mesh found").

## Expected

No `meshes` array in JSON = no mesh entries in the generated UAsset.

## Files

- `Plugins/Editor/ProjectDefinitionGenerator/Source/ProjectDefinitionGenerator/Private/DefinitionGeneratorSubsystem.cpp` -- asset creation
- `Plugins/Editor/ProjectDefinitionGenerator/Source/ProjectDefinitionGenerator/Private/DefinitionJsonParser.cpp` -- already handles missing meshes gracefully
- Thumbnail system: suppress "no mesh" warnings for meshless definitions

## References

- SniperZone.json, GasCloud.json -- trigger-only objects with no meshes
