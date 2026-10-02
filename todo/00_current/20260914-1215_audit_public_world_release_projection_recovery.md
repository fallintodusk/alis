# Make Public World Release Projection Isolated and Recoverable

Status: CURRENT - investigation complete; independent R1 review required before implementation
Priority: Required before 3.0.0 release inputs are generated
Created: 2026-09-14 12:15 Europe/Moscow
Updated: 2026-09-23 Europe/Moscow

## Goal

Generate the release-only public Kazan and Manhattan projections without replacing any bytes in the
operator's working tree. An interrupted or concurrent release run must be recoverable by discarding
only its isolated, release-owned checkout. Do not reopen or change the accepted 2.0.0 release.

## Current state and next steps

The live-tree failure mode is verified. The smallest safe design is now selected: run the projection
inside a detached worktree at the frozen release source commit. The existing child World transaction
continues to protect mutations inside that disposable checkout; the live generated tree is never a
transaction participant. No production code has been changed.

| Step | Runs | Verified by | Starts when |
|---|---|---|---|
| R1 independent review | fresh non-author `architect` reviewer | findings applied here and verdict recorded | now |
| P0 red fixtures | `implement-approved-change` | interruption, concurrency, LFS, and source-identity tests fail for the intended reason | operator approval |
| P1 isolated projection | implementer with `world-engineering` | invariants 1-10 and focused package tests | P0 red |
| P2 release integration | implementer | one unsigned 3.0.0 dry preparation from one frozen commit | P1 green |
| P3 docs and diff review | implementer, independent reviewer | ASCII/link checks and `architect` diff verdict | P2 green |

## Authority register

### Operator decisions

- **D1** Anything that prevents a proper build is fixed now: small fixes stay in the release plan,
  concern-owned fixes stay with that concern, and wide fixes become a linked current task.
  - Effect: this task is current and required by the 3.0.0 release plan because
    `prepare_release_inputs.ps1` invokes this projection.
  - Date/source: 2026-09-23, operator session.

### Working assumptions

- **A1 [ACTIVE]:** Release-input preparation runs from an exact frozen commit, not uncommitted
  working-tree state. This is already the 3.0.0 R5 contract. The implementation must refuse an
  absent or unresolved source commit before creating a worktree.
- **A2 [ACTIVE]:** Network access to fetch missing Git LFS objects is allowed during the local
  release-preparation phase. Publishing remains separately operator-gated.

## Non-goals

- No World architecture redesign or second generated-content journal.
- No change to canonical World authority, generator semantics, public profiles, or accepted maps.
- No change to the accepted 2.0.0 bytes or signing sequence.
- No WSL release enablement; native Windows remains the accepted release route.
- No automatic commit, push, release creation, or publication.

## Verified evidence

- **E1 - outer live-tree mutation:**
  `scripts/ue/package/public_world_projection.ps1:128-180` snapshots, recursively removes, rebuilds,
  and restores paths under the live `Plugins/World/ProjectWorldData/Content`. Restoration exists
  only in `finally`.
- **E2 - interruption gap:** Process termination, host failure, or power loss does not run
  PowerShell `finally`. The outer projection owns neither the project-global generated-content lock
  nor a durable recovery journal. Its byte digest detects a bad completed restore but cannot recover
  after an interrupted process.
- **E3 - child transactions are narrower:** Each call to
  `scripts/ue/world/realize_canonical_world.ps1` owns its normal per-realization transaction. That
  journal does not own the outer pre-run private snapshot or the two-world public projection.
- **E4 - tests miss the failure domain:**
  `scripts/ue/package/tests/test_public_world_projection.ps1` proves digest sensitivity, package-root
  routing, and failed-work cleanup. It does not force process interruption or concurrent generation.
- **E5 - clean checkout is too late:** `prepare_release_inputs.ps1:87-94` performs the projection
  against the live root. Its isolated public checkout is created only at `:103-106`, after public
  source composition.
- **E6 - all required projection inputs are versioned:** The live census found 14 tracked canonical
  files, 18 tracked profile files, and 2,501 tracked ProjectWorldData content files, with no
  untracked files in those roots. Nine canonical files and 2,500 content files are Git LFS objects.
  Therefore a detached checkout at an exact commit can contain the complete input set.
- **E7 - Git worktree fit:** Git documents `git worktree add --detach <path> <commit-ish>` as a
  throwaway checkout for testing without disturbing current development. A linked worktree shares
  repository objects but has its own HEAD and index.
  [Git worktree documentation](https://git-scm.com/docs/git-worktree).
- **E8 - Git LFS fit and guard:** Git LFS normally materializes LFS content during checkout; an
  explicit `git lfs fetch` plus `git lfs checkout` handles a missing local object. `checkout` does
  not download and does not overwrite modified files, so the release route must fetch first and
  verify required paths are not pointer stubs.
  [Git LFS manual](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs.adoc),
  [Git LFS checkout](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-checkout.adoc).
- **E9 - Windows APIs do not solve the tree transaction:** `ReplaceFile` replaces one file and its
  write-through flag is unsupported. `MoveFileEx` can move a directory on one volume, but
  replacement fails when the destination is an existing directory; cross-volume moves degrade to
  copy/delete. Neither API provides an atomic replacement of this multi-directory generated tree.
  [ReplaceFile](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-replacefilea),
  [MoveFileEx](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-movefileexa).
- **E10 - similar harness has the same outer pattern:** The runtime-profile tournament takes the
  global content lock and uses snapshot plus `finally`, but has no durable outer interruption
  journal. Reusing that pattern would prevent concurrency while retaining the crash gap.

## Current architecture and source of truth

```text
frozen private source commit
  -> public_world_projection.ps1 mutates live private ProjectWorldData today
  -> mirror_to_github.ps1 composes public source and developer payload
  -> clean public checkout verifies build, payload, and map loads
  -> prepared release inputs are promoted
```

Canonical bundles, profiles, and generated assets remain owned by ProjectWorldData. Normal generated
mutation remains owned by `generated_content_transaction.ps1`, `generated_manifest.ps1`, and
`recover_generated_transaction.ps1`. This task changes only the release orchestration boundary.

## Capability gap

The release route can restore the live tree after a caught failure, but it cannot guarantee recovery
after process death and can interleave with another generated-content writer. A release operation
must not borrow the operator's working tree as temporary public authority.

## Decision

### Target shape

1. R5 supplies one exact `SourceCommit` to release-input preparation. Resolve it with
   `git rev-parse --verify <commit>^{commit}` and record it in the preparation receipt.
2. Create one detached linked worktree under `tmp/release/projection/<run-id>/repo` at that commit.
   Guard the resolved path under the project `tmp/` root before any removal.
3. Fetch the required LFS objects for that commit, materialize them in the isolated worktree, and
   fail closed if a required canonical bundle or World asset is absent, an LFS pointer, or has a
   failed object check.
4. Invoke the existing release-input composition from the isolated repository root. The existing
   child transaction and public-source verification run unchanged inside that disposable tree.
5. Promote only the accepted release-input directory back to the caller's requested `InputRoot`.
   Every report records `SourceCommit`, isolated root, projection profiles, and result.
6. On success or caught failure, remove the exact linked worktree with normal `git worktree remove`
   after proving it is the recorded detached checkout. On interruption, the next run reports and
   removes only stale release-owned worktrees whose run receipt, registered worktree path, and HEAD
   all agree. A mismatch fails closed for operator inspection.

The live checkout may be dirty; it is read only. The frozen commit, not live bytes, is the release
input. Testing the complete route therefore happens only after the intended source is committed.

### Premise / KISS gate

- **Owner:** release orchestration owns its disposable checkout; World retains one normal mutation
  owner inside it.
- **Added:** exact source-commit input, one isolated-worktree helper, LFS materialization checks, and
  a small run receipt for safe stale-checkout cleanup.
- **Removed:** live snapshot/delete/restore, its digest as a recovery mechanism, and any need to
  extend the World journal for a release-only outer transaction.
- **Capability given up:** preparing official release inputs from uncommitted local changes. The
  frozen-source release contract already forbids relying on that state.

### Alternatives considered

- **Lock plus `finally`:** rejected by E2 and E10; it serializes writers but is not interruption
  recovery.
- **Extend the World journal around both profiles:** viable but larger and coupled. The current
  journal grammar owns normal manifest-backed operations, not a release-only private-to-public tree
  swap. Isolation removes the protected-state mutation entirely.
- **Copy the full repository:** rejected because it duplicates LFS bytes and Git identity while a
  linked worktree provides an exact independent checkout.
- **Windows directory rename swap:** rejected by E9; it still needs recovery state and cannot
  atomically replace multiple existing directories.

## Required invariants

1. The operator's working tree and index are never written by projection, including on interruption.
2. Every accepted output names one resolved source commit; no live or mixed source state contributes.
3. Canonical and generated LFS inputs are real objects, not pointer files, before Unreal runs.
4. Concurrent runs use different isolated roots and cannot mutate each other's generated content.
5. A failed or killed run promotes no release inputs.
6. Cleanup removes only an exact, receipt-owned path under `tmp/release/projection/`; identity
   disagreement fails closed.
7. Successful public World manifests, developer payload, public source, and map-load proof retain
   their current contracts.
8. The normal World lock, journal, manifest-last promotion, and recovery owners are not widened or
   duplicated.
9. Accepted 2.0.0 artifacts and historical verification remain byte-identical.
10. No commit, push, tag publication, or release publication occurs in this task.

## Implementation tasks

- [ ] **P0 - focused red evidence:** add disposable fixtures for source-commit resolution, LFS
      pointer refusal, two concurrent projection roots, forced child-process termination before
      promotion, stale-worktree identity mismatch, and exact cleanup. Prove each test discriminates
      with a known-bad live-tree or mismatched-receipt control.
- [ ] **P1 - isolate the owner:** add a small sibling helper under `scripts/ue/package/` for guarded
      detached-worktree create/verify/remove. Keep `public_world_projection.ps1` focused on World
      projection inside the repository it is given. Do not add recovery operations to the World
      journal.
- [ ] **P2 - route one frozen identity:** require `SourceCommit` through the release entrypoint and
      `prepare_release_inputs.ps1`; run public projection and composition inside that checkout; copy
      or move only accepted outputs to the caller-owned `InputRoot`.
- [ ] **P3 - make cleanup observable:** write an ASCII JSON run receipt before checkout creation,
      advance phases atomically, preserve failed evidence, and make the next run list stale entries.
      Automatic cleanup is allowed only after path, HEAD, detached state, and receipt agree.
- [ ] **P4 - retain child guarantees:** keep the existing realization manifests, map identities,
      public profile checks, payload audit, clean public checkout, and map-load receipt.
- [ ] **P5 - docs and review:** update the package README, packaging guide, and canonical World
      realization docs only after the route is proven; run independent diff review.

## Proof traceability

| Invariant | Acceptance surface | Execution envelope | Cheapest proof | Final proof | Stop condition |
|---|---|---|---|---|---|
| 1 | live worktree bytes and index | forced kill | before/after Git status and byte digest | killed full projection leaves both identical | any live delta |
| 2 | preparation receipt and outputs | exact detached commit | unresolved/mismatched commit refusal | unsigned dry preparation records one commit everywhere | mixed identity |
| 3 | canonical and content LFS paths | isolated checkout | synthetic pointer refusal | required-path LFS object check before Unreal | pointer or missing object |
| 4 | per-run checkout | two simultaneous fixtures | distinct roots and no shared writes | overlapping projection processes both isolate | shared mutable path |
| 5 | `InputRoot` promotion | forced kill at each phase | absent destination after kill | full interruption matrix | partial promoted input |
| 6 | cleanup helper | mismatched receipts and paths | refusal test with known-bad control | stale real worktree recovery drill | broad or ambiguous removal |
| 7 | public outputs | current package tests | existing focused tests green | public build, payload audit, two map loads | contract drift |
| 8 | World transaction owners | source dependency audit | no new outer journal operation | independent architecture review | duplicated authority |
| 9 | historical release | retained 2.0.0 workspace/assets | historical unit tests | published/local artifact verification | byte or manifest drift |
| 10 | Git/SaaS state | entire task | status/log inspection | operator performs later publication separately | agent external write |

## Documentation plan

- `scripts/ue/package/README.md`: exact source-commit input, isolated projection lifecycle, stale
  checkout inspection, and recovery command.
- `docs/build/packaging_guide.md`: official preparation starts from the frozen commit and never uses
  live working-tree bytes.
- `scripts/ue/world/README.md`: one routing sentence that release projection uses normal World
  transactions inside an isolated checkout; no duplicate recovery procedure.
- Stable docs, code, tests, and configuration never link to this todo.

## Reviewer brief

Review with the `architect` skill and the review contract before implementation. Verify the selected
isolation boundary against the release scripts, Git LFS behaviour, Windows cleanup semantics, and the
World transaction owner. In particular, challenge:

- whether every private input needed by materialization is tracked at the frozen commit;
- whether linked-worktree LFS configuration and object validation are sufficient on Windows;
- whether the output-promotion boundary can remain outside the isolated tree without mixed identity;
- whether stale-checkout cleanup proves exact ownership before any recursive removal;
- whether a smaller design can meet invariants 1-10 without adding a second journal.

Return `PASS`, `PATCH`, `BLOCKER`, or `UNVERIFIED`. Each required finding states evidence,
consequence, and the smallest safe fix. This investigation pass is not the required independent R1
because it authored this revision.

## Completion criteria

- An independent R1 verdict is `PASS` after any findings are applied.
- Invariants 1-10 pass with known-bad controls, and a forced interruption leaves the live tree and
  index unchanged.
- One unsigned 3.0.0 dry preparation succeeds from one frozen commit, with public build, payload,
  and map-load checks green.
- Durable release and World docs state the accepted route; no live-tree projection path remains.

## Review record

- **2026-09-14:** created as a post-2.0.0 reliability audit; deferred from 2.0.0.
- **2026-09-23:** selected for 3.0.0 under D1 and moved to `todo/00_current/`.
- **2026-09-23 investigation closure:** verified the live-tree interruption gap, late clean-checkout
  boundary, tracked/LFS input census, existing journal scope, and Windows replacement limits.
  Selected a detached worktree at the frozen source commit as the smallest architecture-aligned
  fix. No production code, engine checkout, Git index, commit, or external state was changed.
