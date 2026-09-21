# Build and Release

Public router for compiling, validating, and packaging ALIS.

This router focuses on the build and release paths that are meaningful in the public source tree.

## Start Here

- Detailed workflow: [workflow.md](workflow.md)
- Public release packaging: [packaging_guide.md](packaging_guide.md)
- Architecture router: [../architecture/README.md](../architecture/README.md)
- Testing router: [../testing/README.md](../testing/README.md)
- Scripts router: [../../scripts/README.md](../../scripts/README.md)

## Public Developer Prerequisites

- Windows 10 or 11
- Git and PowerShell 5.1 or newer
- Epic Games Launcher installation of Unreal Engine 5.8
- Visual Studio 2022 with Desktop development with C++, Game development with
  C++, an MSVC v143 toolset, and a Windows 10 or 11 SDK
- The `v2.0.0` public source checkout and matching signed developer payload

The payload installer never needs the private release-signing key. It verifies
the published signature through the repository public key and fingerprint.

## Common Tasks

### Build the editor

Use:

```powershell
Copy-Item .\scripts\config\ue_path.conf.example .\scripts\config\ue_path.local.conf
# Edit ue_path.local.conf and set UE_PATH to the launcher UE 5.8 directory.
.\scripts\ue\standalone\build.ps1
```

See: [workflow.md](workflow.md)

### Open the generated public worlds

Kazan is the default Editor map. Open either accepted public root directly:

```powershell
.\scripts\ue\run\run_editor.bat /ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory
.\scripts\ue\run\run_editor.bat /ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase
```

Old City 17 remains visible as legacy product context, but its proprietary
content is deliberately not part of the public developer route.

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
.\scripts\ue\package\package_release_source.bat
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

## Public Source and Developer Payload

The public Git tag contains source, JSON, schemas, documentation, and release
automation. The matching signed developer payload contains the approved Unreal
assets required to build and open the public Kazan and Manhattan routes.
Restricted third-party content and private machine configuration are excluded.

Install the payload through the checkout-local installer documented in the
[Developer Quick Start](../quickstart/developer/README.md). The installer
verifies the public signature, tag, revision, inventory, and file hashes before
copying any asset. Developers need the published public key, not the private
release-signing key.

## Related References

- Source engine build notes: [../ue_engine/build.md](../ue_engine/build.md)
- Scripts router: [../../scripts/README.md](../../scripts/README.md)
- Root docs router: [../README.md](../README.md)
