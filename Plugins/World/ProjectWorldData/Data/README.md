# ProjectWorldData

This directory owns production Kazan source, controls, profiles, and accepted
canonical generated JSON. `Canonical/` is populated only by production
promotion; local candidates remain under ignored `tmp/`. `Manifests/` is a
strict durable Unreal-artifact authority root and remains empty until the first
accepted enrollment publishes its active set and immutable scopes.

- `Profiles/` owns source-ingestion, canonical-compilation, validation, and
  pre-realization budget inputs.
- `Controls/` owns human-edited compiler and geodetic control inputs.
- `Authored/`, `Presentation/`, and `Runtime/` own Kazan realization inputs.
- [`Canonical/`](Canonical/README.md) owns accepted generated JSON in
  deterministic Azure Git LFS bundles plus small text indexes. The public
  GitHub mirror excludes the bundle payloads.
- `Manifests/` owns accepted generated Unreal package inventory.
