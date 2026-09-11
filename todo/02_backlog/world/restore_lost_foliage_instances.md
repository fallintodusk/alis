# Restore Lost Foliage Instances in City17 Persistent WP

Status: restoration applied to working tree, awaiting commit. Symptom: trees and bushes missing in painted areas of `City17_Persistent_WP` (originally reported in [../02_backlog/world/fix_foliage.md](../02_backlog/world/fix_foliage.md): "trees, bushes, and TV near the dumpster disappeared").

## Where foliage lives

Foliage in this map is stored as `InstancedFoliageActor` external actors under [Plugins/World/City17/Content/__ExternalActors__/Maps/City17_Persistent_WP/](../../Plugins/World/City17/Content/__ExternalActors__/Maps/City17_Persistent_WP/), one HISM-bearing actor per WP cell. The five cells confirmed as foliage carriers (cross-referenced via [Plugins/Resources/ProjectObject/Content/Nature/ProceduralFoliageSpawner.uasset](../../Plugins/Resources/ProjectObject/Content/Nature/ProceduralFoliageSpawner.uasset)):

| Cell | File |
|------|------|
| `0/61` | `S2IYDQVZ7DI5MN82WQ12K5.uasset` |
| `0/SZ` | `EZN5SQ9SNGXLI8Z51WV33V.uasset` |
| `8/3H` | `3H4SHPE11AWPF3XJ38K40B.uasset` |
| `9/4D` | `Z61S7GXLKNH5DBBA60QNE0.uasset` |
| `A/JP` | `BI7R146V4UY4HRCAAH8QOU.uasset` |

There may be more foliage cells in the map; this five-cell set is the verified baseline from the RT-disable batch in `0b4f34121`.

## What broke

LFS pointer sizes on each commit that touched these cells, with the loss event highlighted:

| Cell | Peak (`6d48f1929`, Jan 10) | After loss (`a05c45a9b`, Jan 11) | Pre-restore HEAD | Pre-restore vs Peak |
|------|---:|---:|---:|---:|
| `0/61` | 259,966 | 249,157 | 246,392 | **94%** |
| `0/SZ` | **274,055** | 223,922 | 216,737 | **79%** |
| `8/3H` | 11,233 | n/a | 9,481 | **84%** |
| `9/4D` | **608,319** | 458,801 | 456,758 | **75%** |
| `A/JP` | 19,123 | n/a | 17,253 | **90%** |

Loss commit `a05c45a9b` (Archer-cpu, 2026-01-11 10:24, single parent `6d48f1929`) is titled "The OID values and sizes have been updated for several foliage actor files" and rewrote LFS pointers on 7 foliage cells to smaller blobs. Drops are proportional in a way that's characteristic of HISM instance loss (large cells -18% / -25%, small cells -4%). At ~80 bytes per HISM instance, the `0/SZ` + `9/4D` deltas alone account for roughly 2,500 lost trees.

Earlier mass event: `18098fcbf` (2025-12-11, "Add multiple new UAsset files to City17 Persistent WP") — single-parent commit with 3,364 files changed and 7,248 deletions vs 2,850 additions, looks like an editor-side WP grid resave during the UE 5.5→5.7 transition. Same failure mode as the KazanMain incident in [../01_done/world/foliage_recovery.md](../01_done/world/foliage_recovery.md).

## Why "OID update" = "instance loss"

A `.uasset` under LFS is a 137-byte text pointer on disk:

```
version https://git-lfs.github.com/spec/v1
oid sha256:<hash>
size <bytes>
```

Editing the `oid` / `size` lines redirects the file at the same path to a **different binary blob**. A smaller blob for an `InstancedFoliageActor` means fewer HISM instances. So a commit titled "updated OID and sizes" with shrinking sizes is, mechanically, **a deletion of foliage data described as a metadata change**.

`a05c45a9b` has one parent (not a merge), so a conflict-resolve scenario is ruled out. Most likely mechanism: a WP cell was not Loaded in the editor, `Save All` then persisted an empty `InstancedFoliageActor` for it, the resulting `git diff` showed only `oid` / `size` churn (which is technically all LFS records for binary files), and the author committed with that exact wording — not realising it was overwriting the prior day's painting.

LFS itself is not at fault. `merge=lfs` is declared in [.gitattributes](../../.gitattributes) but no `merge.lfs.driver` is registered in any git config scope, so git falls back to its built-in binary merge — which produces a **visible** conflict, not silent data loss. `git lfs fsck` clean. Every loss in this map's history traces back to editor-side save behaviour, not to git mechanics.

## Restoration applied

All five peak LFS blobs from `6d48f1929` were already cached locally under `.git/lfs/objects/` — no remote fetch required. The five WP cell files were overwritten in the working tree with bytes from those blobs, then re-saved through the editor with all five cells `Load Region`'d. UE preserved the content byte-for-byte (sizes match peak exactly). Verified visually in the map.

Working-tree state ready to commit:

```
git checkout -b restore-foliage-jan10
git add Plugins/World/City17/Content/__ExternalActors__/Maps/City17_Persistent_WP/{0/61,0/SZ,8/3H,9/4D,A/JP}/*.uasset
git add todo/00_current/restore_lost_foliage_instances.md
git commit -m "restore(city17): recover foliage instances lost in a05c45a9b"
```

A dedicated branch is recommended because the peak content predates 4 months of unrelated edits to these cells — the recovery should land via PR with team review, not as a silent overwrite on a personal branch.

## Prevention

1. **WP discipline.** Always `Load Region` on a cell before painting or before any `Save All`. Saving an unloaded cell overwrites it with an empty `InstancedFoliageActor`. This is the proximate cause of every foliage loss event in this map's history.
2. **No manual pointer edits.** Never commit "update OID and sizes" changes. To revert a foliage cell, use `git checkout <commit> -- <file>` so history shows the source.
3. **LFS file locking.** Update [.gitattributes](../../.gitattributes) to `*.uasset filter=lfs merge=binary diff=lfs -text lockable`. Adopt `git lfs lock` on `City17_Persistent_WP.umap` and foliage cells before editing, `git lfs unlock` after commit. Per [../01_done/world/foliage_recovery.md](../01_done/world/foliage_recovery.md).

## References

- Peak: `6d48f1929` (2026-01-10) — "redrawing the folio onto the map, scattering garbage".
- Loss: `a05c45a9b` (2026-01-11, Archer-cpu) — "The OID values and sizes have been updated for several foliage actor files".
- Earlier resave: `18098fcbf` (2025-12-11) — "Add multiple new UAsset files to City17 Persistent WP".
- Symptom report: [../02_backlog/world/fix_foliage.md](../02_backlog/world/fix_foliage.md) — close once restore commits land.
- Sibling: [improve_foliage_lumen_quality.md](improve_foliage_lumen_quality.md) — visual quality recovery after RT/HISM disable.
- Precedent: [../01_done/world/foliage_recovery.md](../01_done/world/foliage_recovery.md) — UE 5.5→5.7 LFS merge incident.
- Related: [../01_done/world/fix_wpo_foliage.md](../01_done/world/fix_wpo_foliage.md).
