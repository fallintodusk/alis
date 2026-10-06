# Generated-content lock and outer recovery

This directory owns the one lock that every generator of persistent Unreal content
serializes on: ProjectWorld realization, transaction recovery, and the authority audit;
the ProjectMaterial and ProjectTexture host; End-to-End validation; and World workspace
cleanup. It also owns the outer-recovery marker, which covers an operation that changes
more than one owner under one lock.

## Lock and delegation

- `generated_content_mutation_lock.ps1` holds `tmp/world/world_realization/content_mutation.lock`
  open as an OS file handle and stamps a random owner token into it. The OS releases the
  handle when its process exits. The file is a permanent endpoint; cleanup never deletes it.
- An owner that runs other owners' entry points as child processes calls
  `Enable-ProjectGeneratedContentLockDelegation -Lock <handle>` and, before it releases,
  `Disable-ProjectGeneratedContentLockDelegation -Prior <value>`. Enable exports the token in
  `PROJECT_GENERATED_CONTENT_LOCK_TOKEN`. A child that finds the variable joins the live
  owner when the token matches, and fails closed when it does not or when no owner holds
  the lock. The name belongs to this owner; EndToEndValidation's `execution.py` holds the
  only other copy, and its lock test keeps the two equal.
- `-RequireOwnership` refuses a delegated token. Work that must never run inside another
  operation, such as workspace cleanup or an outer coordinator, passes it.
- `Assert-ProjectGeneratedContentNoProjectUnrealProcess` is the one same-project Unreal
  process check; the material host and recovery reuse it. A commandlet started by a
  participant holds no lock handle, so this check covers it.

## Outer-recovery marker

`tmp/world/world_realization/outer_recovery.json`, with its staging file
`outer_recovery.json.tmp`, is the durable journal of an outer operation. Its schema and
write protocol live in `generated_content_outer_recovery.ps1`.

- The live owner writes it after the World outer snapshot and before any owner changes.
  It records the operation id and phase, the World snapshot under
  `tmp/world/world_realization/outer_recovery/<operation id>/`, whose records World owns, the
  ProjectMaterial operation id bound before that operation starts, and both owners'
  identities before the operation.
- While the marker or its staging file exists, every non-delegated acquisition fails
  closed, whether or not any process holds the lock. That covers PowerShell and Python
  owners and workspace cleanup. Delegated children of a live owner still proceed.
- Writes create with `File.Move` onto an absent name and update with `File.Replace`. Stale
  staging is deleted only while the committed marker exists; otherwise it is promoted
  first. A failed marker write is terminal for its run: the owner keeps the lock and the
  snapshots and stops.
- Removing the marker commits the operation, or its recovery.

## Recovery

```powershell
.\scripts\ue\generated_content\recover_generated_content.ps1
```

The only resolver of a pending marker. It opens the lock with no sharing, which fails
while any participant, an orphaned delegated child included, still holds a handle, and
refuses while a project Unreal process runs. It then validates the marker, the World
records, and the input copy, and takes the lock as its owner with delegation. It runs
World transaction recovery when a journal is pending, restores the World outer snapshot,
and restores ProjectMaterial through the host's `RestorePrevious` mode for the bound
operation. It removes the marker only when both owners match their recorded identities.
Every step is idempotent, so a rerun is safe.

| Exit | Meaning | State |
|---|---|---|
| 0 | resolved, or nothing was pending | marker gone |
| 5 | refused: a participant holds the lock, a project Unreal process runs, a delegated token is set, or another operation took the lock | unchanged; retry once the participant exits |
| 6 | the marker, its World records, or its input copy is invalid or unconfined | unchanged; investigate before anything else |
| 7 | an owner restore or the identity check failed | marker and snapshots kept; fix the cause and rerun on the same commit |

## Tests

`test/` runs from `scripts/ue/world/test/run_all.ps1`, so the common checks run it. Every
case uses its own fake project root; none takes this checkout's lock or starts Unreal.
