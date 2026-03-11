# Build and Release

Public router for compiling, validating, and packaging ALIS.

This router focuses on the build and release paths that are meaningful in the public source tree.

## Start Here

- Detailed workflow: [workflow.md](workflow.md)
- Public release packaging: [packaging_guide.md](packaging_guide.md)
- Architecture router: [../architecture/README.md](../architecture/README.md)
- Testing router: [../testing/README.md](../testing/README.md)
- Scripts router: [../../scripts/README.md](../../scripts/README.md)

## Common Tasks

### Build the editor

Use:

```powershell
.\scripts\ue\build\build.bat AlisEditor Win64 Development
```

See: [workflow.md](workflow.md)

### Rebuild one module

Use:

```powershell
.\scripts\ue\build\rebuild_module_safe.ps1 -ModuleName ProjectMenuMain
```

See: [workflow.md#fast-iteration-techniques](workflow.md#fast-iteration-techniques)

### Run fast validation checks

Use:

```powershell
.\scripts\ue\check\validate_uht.bat
.\scripts\ue\check\validate_blueprints.bat
.\scripts\ue\check\validate_assets.bat
```

Related docs:
- [../../scripts/ue/check/README.md](../../scripts/ue/check/README.md)
- [../testing/README.md](../testing/README.md)

### Package a public release

Use:

```powershell
.\scripts\ue\package\package_release.ps1 -EngineRoot <ue-path> -CreateReleaseArchive
.\scripts\ue\package\sign_release.ps1 -ReleaseDir <release_dir>
.\scripts\ue\package\verify_release.ps1 -ReleaseDir <release_dir>
```

Canonical guide:
- [packaging_guide.md](packaging_guide.md)
- [../../scripts/ue/package/README.md](../../scripts/ue/package/README.md)

### Build client or server targets

Examples:

```powershell
.\scripts\ue\build\build.bat AlisClient Win64 Shipping
.\scripts\ue\build\build.bat AlisServer Win64 Development
```

Related docs:
- [workflow.md](workflow.md)
- [../gameplay/multiplayer/architecture.md](../gameplay/multiplayer/architecture.md)

## Important Public-Scope Note

This repository intentionally excludes Unreal content payloads and private machine configuration from the public mirror.

That means:
- the code and automation are public
- the release flow is public
- some full runtime or packaging scenarios still require the complete internal asset-bearing checkout

## Related References

- Source engine build notes: [../ue_engine/build.md](../ue_engine/build.md)
- Scripts router: [../../scripts/README.md](../../scripts/README.md)
- Root docs router: [../README.md](../README.md)
