# ProjectMaterial generation

This directory owns the host transaction for ProjectMaterial's Editor-only compiler.
The stable material contract is documented by
[ProjectMaterial](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md).

## Commands

Validate recipes without persistent mutation:

```powershell
scripts/ue/material/run_material_generation.ps1 -Mode Validate
```

Regenerate accepted universal material assets:

```powershell
scripts/ue/material/run_material_generation.ps1 -Mode Regenerate
```

`-CleanupOrphans` is an explicit mutation option and is valid only with
`Regenerate`. Normal runs report orphans and retain them.

## Safety boundary

The wrapper is the only supported mutation entry. It uses the same project-global
generated-content lock as ProjectWorld, refuses when another Editor has this project
open, snapshots exact prior output and manifest state, journals the transaction,
launches one hidden launcher-engine commandlet, authenticates its receipt, and restores
after rejection, crash, or timeout.

Scratch lives under `tmp/material/generation/`. Accepted production evidence retains
only `Current`, `Previous`, and one exact `RollbackPrevious` asset/manifest bundle. The
latest rejection is bounded to one directory and is removed on the next accepted run.
Core integration tests use an isolated test mount and remove their synthetic assets and
evidence.

The wrapper never elevates, edits Windows Firewall, or starts a source-built engine.
