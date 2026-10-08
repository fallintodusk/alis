# Make Public World Release Projection Isolated and Recoverable

Status: DONE - frozen unsigned R5 accepted; public build, payload/dependency audit and both map loads verified
Priority: Required before 3.0.0 release inputs are generated
Created: 2026-09-14 12:15 Europe/Moscow
Updated: 2026-10-08 Europe/Moscow

## Goal

Generate the release-only public Kazan and Manhattan projections without replacing any bytes in the
operator's working tree. An interrupted or concurrent release run must be recoverable by discarding
only its isolated, release-owned checkout. Do not reopen or change the accepted 2.0.0 release.

## Outcome

The frozen unsigned 3.0.0 route accepted from
`a4167f43b91403d69da0ad011aa85d319c1453e6` with exit 0.
Both public World realizations and pinned runtime profiles passed inside the
isolated checkout. Composition, licensing, install/reinstall, clean public
Editor build, dependency closure, both map loads and final unsigned assembly
passed. The main source/index remained unchanged and the isolated checkout
was removed. P4 is satisfied. No signing or publication occurred.

The ledger is
`tmp/release/r5/a4167f43b91403d69da0ad011aa85d319c1453e6/closure.json`.
The [release router](../../00_current/00_release_3.0.0.md#current-state-and-next-steps) owns the
operator walkthrough/review and R6 boundary. Absolute performance is
inconclusive with explicit residual-risk acceptance; this is not performance PASS.

## Implementation and historical evidence

The live-tree failure mode is verified. Release input preparation now creates a
detached worktree at the frozen source commit. The existing child World
transaction and projection snapshot operate inside that disposable checkout;
the live generated tree is not a transaction participant. Focused helper,
projection, and release-entrypoint tests pass. The implementation is in commit
`bc5b3c0926a0583503419a5876a1655510a119f2`. The first unsigned release
run stopped in the Kazan playable-tour Candidate prerequisite before release
input preparation. The CSV producer is identified and its project-wide fix passed
one exact packaged Development repro on 2026-10-05. Its committed version then
passed the full player Candidate, but the isolated projection stopped at an
obsolete helper parameter. Its committed fix and the host-load cap passed the
frozen Candidate at `09b7b6532`. The next projection attempt refused an omitted
pinned runtime profile. Commit `76ed07763` forwarded a runtime only after changing
both public profiles to `none`; that invalidated the intended public map contract.
Commit `fa645902cf7d1c7572d6117b651583eabf178c7f` restores the pinned IDs. A real isolated Kazan run
then exposed an unconditional vegetation-instancing acceptance check even though
the public profile deliberately omits vegetation. The narrow runtime-owner fix
passed a focused isolated Kazan and Manhattan projection with both pinned runtime
hashes and byte-for-byte restoration. This was an uncommitted overlay at `76ed07763`,
not a frozen release. The direct instancing-policy guard was committed at
`f45a9c01dee51cbc2d868f0236c207579adb1480`. The Manhattan same-package performance harness
and evidence envelope are committed through `c3c006cf12757bf0040ba7e047d1f9476f138e30`.
Independent focused R2 returned PASS at that clean HEAD. The frozen unsigned route at
`f4cce29f3b41840790355c18eb5c5fd72cd8a806` stopped in Manhattan performance before the public
projection. The [release router](../../00_current/00_release_3.0.0.md) owns that failure and the focused correction;
this task's P4 remains unverified.

| Step | Runs | Verified by | Starts when |
|---|---|---|---|
| R1 independent review | fresh non-author `architect` reviewer | PATCH findings applied in code and this plan | done |
| P0 focused fixtures | implementer | committed checkout, corrupt LFS, concurrent roots, kill recovery, cleanup mismatch, source refusal | focused tests green; red-before-code was not recorded |
| P1 isolated projection | implementer with `world-engineering` | exact commit route and no live World mutation | focused two-city overlay PASS; frozen release proof pending |
| P2 release integration | implementer | one unsigned 3.0.0 dry preparation from one frozen commit | Candidate accepted; corrected public projection and frozen rerun remain |
| P3 docs and diff review | implementer, independent reviewer | SOT updates, ASCII/link checks, corrected R2 PASS | done for focused scope; R5 proof pending |

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
2. Use the existing isolated-source workspace owner to create one detached linked worktree under `tmp/release/projection/<run-id>/repo` at that commit.
   Guard the resolved path under the project `tmp/` root before any removal.
3. Fetch the required LFS objects for that commit, materialize them in the isolated worktree, and
   fail closed if a required canonical bundle or World asset is absent, an LFS pointer, or has a
   failed object check.
4. Invoke the existing release-input composition from the isolated repository root. The existing
   child transaction and public-source verification run unchanged inside that disposable tree.
5. Promote only the accepted release-input directory back to the caller's requested `InputRoot`.
   The promoted source-identity receipt records `SourceCommit`, projection profiles, and result.
6. On success or caught failure, remove the exact linked worktree with `git worktree remove --force`
   after proving it is the recorded detached checkout. On interruption, the next run
   removes only stale release-owned worktrees whose run receipt, registered worktree path, and HEAD
   all agree. A mismatch fails closed for operator inspection.

The live checkout may be dirty; it is read only. The frozen commit, not live bytes, is the release
input. Testing the complete route therefore happens only after the intended source is committed.

### Premise / KISS gate

- **Owner:** release orchestration owns its disposable checkout; World retains one normal mutation
  owner inside it.
- **Added:** exact source-commit input, a commit-only path in the existing
  isolated-worktree owner, LFS materialization checks, and an owner receipt for
  safe stale-checkout cleanup.
- **Removed:** live-tree participation in the projection snapshot/delete/restore,
  broad release scratch deletion, and any need to extend the World journal for
  a release-only outer transaction.
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

1. The operator's working-tree file bytes and staged entry set remain unchanged
   by projection, including on interruption. Git may refresh index metadata.
2. Every accepted output names one resolved source commit; no live or mixed source state contributes.
3. Canonical and generated LFS inputs are real objects, not pointer files, before Unreal runs.
4. Concurrent runs use different isolated roots and cannot mutate each other's generated content.
5. Before final directory promotion, a failed or killed run leaves `InputRoot`
   absent. After promotion, a killed run may leave the complete accepted input tree.
6. Cleanup removes only an exact, receipt-owned path under `tmp/release/projection/`; identity
   disagreement fails closed.
7. Successful public World manifests, developer payload, public source, and map-load proof retain
   their current contracts.
8. The normal World lock, journal, manifest-last promotion, and recovery owners are not widened or
   duplicated.
9. Accepted 2.0.0 artifacts and historical verification remain byte-identical.
10. No commit, push, tag publication, or release publication occurs in this task.

## Implementation tasks

- [x] **P0 - focused controls:** add disposable fixtures for source-commit resolution, LFS
      pointer refusal, two concurrent projection roots, forced child-process termination before
      promotion, stale-worktree identity mismatch, and exact cleanup. Prove each test discriminates
      with a known-bad live-tree or mismatched-receipt control. Permanent controls are green and
      independent R2 accepted them; historical pre-fix red output was not retained.
- [x] **P1 - isolate the owner:** extend `isolated_source_workspace.ps1` for guarded
      detached-worktree create/verify/remove. Keep `public_world_projection.ps1` focused on World
      projection inside the repository it is given. Do not add recovery operations to the World
      journal.
- [x] **P2 - route one frozen identity:** require `SourceCommit` through the release entrypoint and
      `prepare_release_inputs.ps1`; run public projection and composition inside that checkout; copy
      or move only accepted outputs to the caller-owned `InputRoot`.
- [x] **P3 - make cleanup observable:** write an ASCII JSON owner receipt before checkout creation.
      The next run removes only dead-owner checkouts whose path, Git registration,
      HEAD, detached state, and receipt agree. An ambiguous entry remains for inspection.
      No release phase journal is needed because no generated authority leaves
      the disposable checkout before promotion.
- [x] **P4 - retain child guarantees:** keep the existing realization manifests, map identities,
      public profile checks, payload audit, clean public checkout, and map-load receipt.
- [x] **P5 - docs and review:** update the package README, packaging guide, and canonical World
      realization docs only after the route is proven; run independent diff review.

## Proof traceability

| Invariant | Acceptance surface | Execution envelope | Cheapest proof | Final proof | Stop condition |
|---|---|---|---|---|---|
| 1 | live worktree files and staged entries | forced kill | before/after status, file digests, and `ls-files --stage` | killed projection leaves those values identical | any live or staged-content delta |
| 2 | preparation receipt and outputs | exact detached commit | unresolved/mismatched commit refusal | unsigned dry preparation records one commit everywhere | mixed identity |
| 3 | canonical and content LFS paths | isolated checkout | synthetic pointer refusal | required-path LFS object check before Unreal | pointer or missing object |
| 4 | per-run checkout | two simultaneous fixtures | distinct roots and no shared writes | overlapping projection processes both isolate | shared mutable path |
| 5 | `InputRoot` promotion | process kill before promotion and after directory rename | absent destination before rename; complete accepted tree after rename | focused interruption fixtures plus frozen-commit dry | partial or unaccepted input tree |
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

- An independent review of the corrected diff is `PASS` after the R1 findings are applied.
- Invariants 1-10 pass with known-bad controls, and a forced interruption leaves working-tree
  tracked bytes and staged entries unchanged. Git may refresh index metadata.
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
- **2026-10-04 independent R1:** PATCH. Reuse the existing workspace owner; verify cleanup
  registration, detached HEAD, owner process, and path containment; verify LFS object IDs;
  protect prior inputs on source refusal; remove unsafe broad scratch cleanup; bind
  player/Linux release work to a clean frozen HEAD. The production diff addresses
  these findings. `isolated_source_workspace.ps1` is over 700 lines because the
  committed checkout shares its lifecycle and cleanup invariant with overlay
  workspaces. If it grows, the next cohesive seam is candidate-overlay snapshot
  construction, not a second worktree owner.
- **2026-10-04 independent R2:** PATCH found omitted LFS inputs in the
  developer payload. The corrected route selects ProjectObject and
  ProjectExperienceData artifacts from their accepted manifests in the exact
  checkout, fetches only those paths, and checks their manifest digests before
  Unreal. Independent re-review returned PASS. Helper, projection, and
  release-entrypoint tests exited 0. At this review, the unsigned 3.0.0 build,
  payload audit, and map loads awaited a frozen committed source identity.
- **2026-10-04 frozen-commit attempt:** HEAD was clean at
  `bc5b3c0926a0583503419a5876a1655510a119f2`. The three focused package tests,
  path governance, and dry release routing passed. The single
  `make release 3.0.0 RELEASE_SIGN=0 SOURCE_COMMIT=bc5b3c0926a0583503419a5876a1655510a119f2`
  run built and cooked the Development Candidate, then its first packaged Kazan
  playable-tour child exited 3. `Saved/Validation/WorldRealization/playable-tour/`
  `fb5fe2ad46124e1c8e74474108447dec/development/run-01/game.log` records an
  Unreal `CSVProfiler` assertion at `CsvProfiler.cpp:1623` after the product-route
  receipt was accepted. A focused repeat using the matching staged Development
  files and the same product-route/performance flags also exited 3 with the same
  assertion; its log is `tmp/world/playable_tour/csv_repro_20261004/game.log`.
  The Candidate owner removed the failed archive and restored the prior package.
  Release input preparation did not start; no map-load or public
  developer proof was produced. The before/after `git ls-files --stage` digest,
  clean status, HEAD, and registered worktree list matched; no release projection
  checkout or promoted 3.0.0 input tree remains. The Candidate failure belongs to
  R5 in `todo/00_current/00_release_3.0.0.md`; do not bypass its gate or mark P4 done.
- **2026-10-05 focused unblock:** the installed UE navigation source and a live
  debugger identified a numbered Recast actor name in the `NavTasks` CSV stat.
  The project config disables that category and its two sibling dynamic-name
  categories. A fresh packaged Development Kazan route exited 0 with accepted
  product and performance receipts and nonempty CSV and exact sample files.
  Independent review returned PASS. The full release route and isolated public
  projection remain unverified until this fix is committed and R5 reruns from
  the new clean source identity.
- **2026-10-05 frozen rerun:** clean commit
  `a61715d31004df95723b5acdd105192c9f6f7516` passed the player Candidate
  (three Development runs, Shipping, water stability). The isolated projection
  then refused a stale `IncludePresentation` argument that its generated-content
  helper no longer accepts. No release inputs or workspace were promoted. A
  focused test now validates wrapper calls against the helper signatures; it
  failed before the local two-call correction and passes after it. Adjacent
  generated-content transaction and release-entrypoint tests pass. P4 is still
  open until the correction is committed and the full route passes.
- **2026-10-05 next frozen rerun:** commit
  `09b7b6532c15947fd894106a2d26eb91ab02175b` accepted the complete Kazan
  Candidate, including Shipping and water. Public projection then failed at the
  World realizer because the wrapper omitted the runtime profile pinned by the
  public Kazan realization profile. The focused regression failed before the
  correction and passes after it; the wrapper derives and checks each city's
  runtime file before snapshot mutation. The release-entrypoint and nine World
  transaction tests pass. Public map loads and combined release remain unverified
  until the operator commits this fix and the frozen release route succeeds.
- **2026-10-05 precommit boundary proof:** a disposable overlay at that source commit accepted
  public Kazan and Manhattan with engine exit 0 and byte-for-byte private restoration. Release
  preparation now materializes committed Unreal LFS packages, producer inputs, and isolated
  Editor modules before projection. A later precommit package pass reached and passed the public
  source mirror through a fixed Windows/WSL linked-worktree Git handoff; the developer composer
  then refused an untracked, public-manifest-authenticated Manhattan package. Its focused fix
  allows only that digest-verified projected class; all 19 composer tests pass. Evidence and
  remaining frozen-release gate are recorded in `00_release_3.0.0.md`. No live private World
  content, Git index, or commit was changed by the diagnostics.
- **2026-10-05 external review PATCH and runtime diagnosis:** the earlier R2 accepted public
  `runtime_profile_id: none`, which skipped partition, route, and product-spawn realization.
  Commit `76ed07763` contains that incorrect profile change despite its commit message.
  Restoring the original pinned IDs caused a real isolated Kazan projection to reject at
  `runtime-acceptance`: `player_start=1 nanite=1 instancing=0 hlod=1`. The public profile has no
  vegetation layer, but the territory runtime acceptance check required a vegetation instance
  unconditionally. The working-tree correction makes that probe required only when vegetation
  is selected or present, while full territory validation still requires a true probe. Rejected
  receipt and log are under `tmp/release/diagnostics/pinned_runtime_28dde9f94ae84ddd88b615fac0e78f92/`.
  The corrected isolated overlay at `76ed07763` then accepted both cities with the original
  runtime IDs and SHA-256 hashes, one runtime partition and the correct route each, 14 public
  authority files, and byte-for-byte snapshot restoration. Its evidence is under
  `tmp/release/diagnostics/pinned_runtime_34e9a71f17fd4d7f85cebc29858835b6/`.
  The detached worktree was removed and main generated content remained clean. Independent
  corrected R2 returned PASS for this precommit scope. The operator commit and frozen release
  remain; clean public checkout, map loads, and packaged player were not part of this overlay.
