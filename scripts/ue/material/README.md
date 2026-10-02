# ProjectMaterial surface generation

This directory owns the host transaction used by ProjectMaterial's Editor-only
surface compiler and, through `scripts/ue/texture/run_pattern_generation.ps1`, by
ProjectTexture's pattern compiler. The durable contracts are documented by
[ProjectMaterial surface generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md)
and [ProjectTexture pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md).

## Commands

```powershell
scripts/ue/material/run_surface_generation.ps1 `
  -Mode Validate `
  -LayoutReceipt <mesh-terrain-layout-receipt.json>

scripts/ue/material/run_surface_generation.ps1 `
  -Mode Regenerate `
  -LayoutReceipt <mesh-terrain-layout-receipt.json>
```

`-CleanupOrphans` is an explicit regeneration-only option. Normal runs report
orphans and retain them. Cleanup is refused while any transaction can still restore
content: an entry under `tmp/world/world_realization/transactions/` (a World realization
transaction's snapshot, kept there until it commits or rolls back), a `snapshot` or
`outer-snapshot` folder elsewhere under `tmp/world/` (World test harnesses), a release
projection's `tmp/release/work/*/world-projection-rollback`, or the other generation domain's
pending journal. Restoring such a snapshot can bring back references to an orphan that the Asset
Registry cannot see. The public World projection holds no content lock of its own; a
projection that starts during a cleanup is outside this check.

The wrapper holds the project-wide generated-content lock, refuses a second Editor
for this project, snapshots exact prior output and manifest state, journals the
transaction, launches one hidden launcher-engine commandlet, authenticates its
receipt, and restores after rejection, crash, or timeout. If that restore itself fails,
for example because another process holds an output file open, the journal and its
snapshot are both kept and the next run of either mode restores them before anything
else. The host receipt lists `retained_orphans`, the referenced orphans a cleanup kept.

For surface tests, `-PatternTestRoot <tmp/texture/generation/...>` makes the surface
test mount consume a pattern that a ProjectTexture test run regenerated there.

Scratch lives under `tmp/material/generation/`. Production evidence retains only
Current, Previous, and one RollbackPrevious bundle.
