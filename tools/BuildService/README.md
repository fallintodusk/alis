# Build Service

Rust workspace for change detection, dependency ordering, Unreal builds,
artifact packaging, manifest updates, and publication.

## Quick Start

Use the wrappers so the Windows build dependencies are configured consistently.

```powershell
.\scripts\build.ps1 check
.\scripts\build.ps1 test --workspace
.\scripts\build.ps1 build --release
```

Git Bash uses the equivalent `./scripts/build.sh` entry point.

## Documentation

Continue at the [documentation router](docs/README.md). Crate manifests and
source remain authoritative for executable dependencies and interfaces. Script
ownership is routed from [scripts](scripts/README.md).
