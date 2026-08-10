# ProjectWorldData

Data/content-only owner for the Kazan reconstruction.

## Ownership

- `Data/` contains authoritative Kazan JSON and source/control provenance.
- `Data/Manifests/` contains durable accepted generated-artifact authority.
- `/ProjectWorldData/Generated/` contains disposable derived Unreal packages.
- `/ProjectWorldData/Authored/` contains protected authored overlays.

All schemas, generators, serialization, replication support, runtime services,
and validation logic remain in ProjectWorld. This plugin has no `Source`
module and must not fork a generator or builder.

The detailed regeneration and geospatial contracts live in
[territory_generation.md](../ProjectWorld/docs/territory_generation.md).
