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

The wrapper holds the project-wide [generated-content lock](../generated_content/README.md),
or joins a live owner that delegated it, refuses while an Unreal process for this project
runs, snapshots exact prior output and manifest state, journals the transaction, launches
one hidden launcher-engine commandlet, authenticates its receipt, and restores after
rejection, crash, or timeout. A restore never deletes live content first: it copies the
snapshot to staging, checks it, moves the live roots aside, moves the copy in, and checks
again. If that restore fails, for example because another process holds an output file
open, the journal and its snapshot are both kept and the next run of any mode restores
them before anything else. The host releases the lock as its last step.

Every run that holds the lock writes `host.receipt.json` to `Current` when accepted or
`Rejected` when rejected: its operation id, mode, status, the commandlet status and error,
and digests of the output and manifest content it leaves on disk. The accepted receipt also
lists `retained_orphans`, the referenced orphans a cleanup kept.

A caller that must record a run before it mutates passes `-OperationId <32 hex>` to
`run_material_generation.ps1`; an id that already names an operation folder, the pending
journal, or the retained rollback bundle is refused before any snapshot. Each accepted
`Regenerate` retains the state it replaced as the one `RollbackPrevious` bundle, bound to
its operation id and to content digests: under `Saved/Validation/<domain folder>/` for
production, and in the test root for test runs. Only the host restores it:

```powershell
scripts/ue/material/run_material_generation.ps1 `
  -Domain Surface `
  -Mode RestorePrevious `
  -RestoreOperationId <operation id>
```

`RestorePrevious` runs no commandlet. When the pending journal belongs to that operation,
the journal restore is the restore. Otherwise it restores the bundle only when the
bundle's `replaced_by_operation_id` equals the requested id and its digests verify, inside
its own journaled transaction, and keeps the bundle. A bundle bound to another operation
restores nothing.

For surface tests, `-PatternTestRoot <tmp/texture/generation/...>` makes the surface
test mount consume a pattern that a ProjectTexture test run regenerated there.

Scratch lives under `tmp/material/generation/`. Production evidence retains only
Current, Previous, and one RollbackPrevious bundle.
