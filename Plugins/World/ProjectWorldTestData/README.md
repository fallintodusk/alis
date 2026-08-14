# ProjectWorldTestData

Editor-only owner of deterministic synthetic inputs and their derived Unreal
fixtures. This plugin is enabled only for Editor targets and must not ship.

Ownership:

- `Data/Profiles/` owns synthetic source and compiler profiles.
- `Data/Fixtures/` owns synthetic provider and compiler inputs.
- `Data/Authored/`, `Data/Presentation/`, and `Data/Runtime/` own synthetic
  realization profiles.
- `Content/Authored/` owns protected authored test packages.
- `Content/Generated/`, matching external-package trees, and `Data/Manifests/`
  are ignored transient outputs. L1/L2 generates them inside the locked test
  transaction and restores their prior presence or absence. A clean checkout
  requires none of them.

`ProjectWorld` owns schemas, validation, realization, and manifest lifecycle
logic. `ProjectWorldData` owns Kazan production data and generated packages.
Only `Content/Authored/`, profiles, controls, and source/compiler fixtures are
tracked here; TestData has no L3 durable authority.
