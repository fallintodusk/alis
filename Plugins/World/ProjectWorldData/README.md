# ProjectWorldData

Data/content-only owner for the Kazan reconstruction.

## Ownership

- `Data/` contains Kazan source, profiles, controls, and accepted canonical
  generated JSON. Accepted generated data is persistent and never hand-edited.
- `Data/Canonical/` is the production promotion root for immutable canonical
  bundles and their indexes. `active.json` selects the one current bundle,
  which is tracked through Git LFS so a clean checkout can authenticate and
  materialize the exact canonical JSON without provider downloads or a
  compiler rerun. Public deployments fetch the same authenticated bundle from
  the matching public data release; it is not placed in source Git history.
  Compiler candidates under `tmp/` are not authority.
- `Data/Manifests/` contains durable accepted Unreal-artifact authority.
- `/ProjectWorldData/Generated/` contains saved, cooked, manifest-owned Unreal
  packages. They persist until an accepted regeneration replaces them.
- `/ProjectWorldData/Authored/` contains protected authored overlays.

This plugin has no `Source` module. Contracts and logic stay with the component
that interprets them: SourceIngestion owns provider admission,
CanonicalCompilation owns canonical compilation, and ProjectWorld owns Unreal
realization, manifest contracts/lifecycle logic, and runtime integration. The
durable manifest documents produced by that logic stay in this data plugin.
Generic schemas, generators, serializers, and validators are never copied here.

The detailed authority, persistence, regeneration, and geospatial contracts
live in
[territory_generation.md](../ProjectWorld/docs/territory_generation.md).
The project-wide JSON/derived-data rule lives in
[Data Architecture](../../../docs/data/README.md).
