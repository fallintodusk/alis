# Complete Definition Actor Structural Sync

Status: investigated; implementation pending

## Current gap

ObjectDefinition capability property changes reapply in place, while mesh or
capability additions and removals change the structure hash and require actor
replacement. Interactive regeneration and passive World Partition loading have
an ObjectDefinition replacement path. Batch PreSave only calls
`ApplyDefinition`; on a structural mismatch it logs and saves the stale actor.

## Investigation boundary

- Reproduce the mismatch through the supported batch/PreSave route with an
  existing definition-backed actor and a capability add or remove.
- Keep replacement orchestration in the editor owner; do not move editor
  factories or replacement policy into the runtime actor.
- Preserve transform, label, folder, definition identity, undo/save behavior,
  and fail visibly if replacement cannot complete.
- Add focused regressions proving capability property updates reapply and
  capability add/remove changes reach the replacement path in every advertised
  synchronization mode.
- Give those regressions an owned editor-world fixture. The current passive
  structure-mismatch test can exit before exercising replacement when its map
  open has not produced an automation world yet.
- Update the placement guide when the batch route has the same structural
  guarantee as interactive and passive synchronization.

Do not add generic factories for definition types that have no current actor
consumer, and do not add asynchronous loading without measured editor hitches.
