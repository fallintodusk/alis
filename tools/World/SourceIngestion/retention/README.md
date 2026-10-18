# Raw snapshot retention

Owns durable exact-byte provider copies and disposable acquisition-cache recovery.

| Path | Responsibility |
|---|---|
| [api.py](api.py) | Retention, recovery and storage preflight |
| [app/](app/) | Verified copy and recovery implementation |
| [contracts/](contracts/) | Machine-local storage configuration |

## Storage and recovery

Copy the [project configuration example](../../../../scripts/config/world_source_storage.json.example)
to its `.local.json` sibling and select
two durable, disjoint archive roots outside the checkout. Use separate physical
storage failure domains and include the replica in the existing backup process.
Machine paths stay ignored. A second directory alone is not proof of backup
durability or independent hardware.

Both stores use the acquisition owner's SHA-256 layout and verifier. Before a
provider operation can be accepted, every selected raw payload must match the
pinned byte size and hashes in both stores. Synthetic JSON fixtures need no raw
archive. Configuration is preflighted before acquisition. Missing storage or a
failed replica prevents a new accepted ingestion marker; completed verified
copies remain reusable. A failed retry never overwrites a prior result marker.

On a cache miss, acquisition checks the primary and replica before contacting
the pinned URL. A corrupt copy cannot satisfy recovery. Unknown I/O or access
failures propagate; they are not reclassified as absence. Each copy is locked,
written to a sibling staging file, flushed, verified, atomically replaced and
verified again. Recovery and retention emit structured events. Repeated retention
verifies existing copies and repairs damaged ones from authenticated input.
The disposable cache must be disjoint from both stores. Source file names cannot
escape their resolved SHA-256 directory; absolute, parent and drive-relative
paths refuse before payload writes.

This store does not select new geography, replace source pins, certify historical
replays, or modify canonical authority. Missing historical bytes remain missing.
Profile rights and source hashes retain their existing authority. Raw provider
bytes never become public Git payload merely because they were retained here.
