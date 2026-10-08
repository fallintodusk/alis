# ALIS 3.0.0 Release Plan

**Status:** frozen unsigned R5 machine gates accepted at a4167f43b91403d69da0ad011aa85d319c1453e6; operator walkthrough/review and R6 publication remain
**Active concern:** review the verified unsigned workspace, perform the operator walkthrough, then authorize R6 steps individually; concerns 1 and 2 are done ([Terrain Material v2](../01_done/content/20260923-1620_content_generate_terrain_material_v2.md))
**Scope:** release-level scope, sequencing, and gates for 3.0.0, plus the release-platform contract change in `scripts/ue/package/` (R4)  
**Stable documentation owner:** [Package and Release Guide](../../docs/build/packaging_guide.md) and [package scripts README](../../scripts/ue/package/README.md) for release operations; each concern task names its own owners

This file is the single current product-focus and release router for 3.0.0, aligned with
[VISION.md](../../VISION.md). Each remaining concern phase and R4 in this file follows its own
authorization and gates. Durable facts move to their stable owners before this file closes.
Reviewers start with `## Reviewer brief`.

## Current state and next steps

### Current frozen boundary: unsigned R5 accepted

The operator committed `a4167f43b91403d69da0ad011aa85d319c1453e6`.
One `make release 3.0.0 RELEASE_SIGN=0 SOURCE_COMMIT=<that-full-SHA>
RELEASE_ACCEPT_INCONCLUSIVE_PERFORMANCE=1` completed with exit 0.
The accepted workspace is `tmp/release/v3.0.0/`, with `game/` and `github/`.
Its manifest remains `pending_owner_approval`; it is unsigned and unpublished.

All six Development city routes, Shipping gameplay and water, both isolated
public realizations, source/payload composition, component licensing,
install/reinstall, clean public Editor build, native dependency audit, both map
loads, archive integrity and final release/workspace verification passed.
The private source/index stayed unchanged and the isolated checkout was removed.
The existing pre-freeze Check/Matrix/verifies remain accepted; no replacement
run or production correction was needed during this frozen R5.

Performance is explicitly inconclusive with accepted residual risk: Kazan pooled
Frame p95 20.8098 ms exceeded its 17.059 ms measured budget; Manhattan pooled
16.5936 ms accepted against 16.67 ms. The base target remains 16.67 ms.
The manifest retains the rejected Kazan evidence and waiver decision.
This does not certify absolute performance on this busy host.

The authenticated ledger is
`tmp/release/r5/a4167f43b91403d69da0ad011aa85d319c1453e6/closure.json`.
That folder also retains both public realization receipts and the per-owner
IoStore comparison: Shipping 2,077,142,532 bytes versus retained 2.0.0's
5,102,176,124 bytes. Manhattan appears exactly once in chunk 11.
Projection recovery P4 is satisfied and its task is done.

Next: operator walkthrough/review of this exact unsigned candidate, then R6.
Do not build, cook, regenerate or repeat R5 merely for task bookkeeping.
Do not sign, push, tag, publish or promote without the operation's authorization.
The sections below retain investigation context; this frozen receipt owns the
current execution status.

### Closed investigation: busy-host performance

The unsigned R5 at `914a9b64d430b965a15bd2a0c5c181e2edfa41da` stopped
after Kazan's three native gameplay runs and valid IoStore proof, before
Manhattan, Shipping and public preparation. Its pooled Frame p95 was 24.3549 ms.
The rejected measurement remains under
`Saved/Validation/WorldRealization/playable-tour/227a62ac8a9440ecb2e807db22582b38/`;
the native release ledger is `tmp/release/r5/<that-full-SHA>/result.json`.

One bounded ABBA diagnostic and a subsequent identity-only audit live under
`tmp/release/performance/abba-20261007T184131-5b06b6d9/`. The known-good
reference degraded on this host, and package assignment was confounded with
load. Earlier accepted B runs share the executable, route, runtime profile,
graphics envelope and 249 logged CVars; their deleted Development packages
prevent complete historical runtime-byte equivalence proof. Candidate regression
is not demonstrated; current absolute performance is not PASS. Do not run
Insights, another benchmark campaign or raise the target to debug this incident.

D18 authorizes the default-off release acceptance flag. Its stable contract is
[release preparation and failure triage](../../scripts/ue/package/README.md#preparation-preflight-and-failure-triage).
The performance correction is limited to release decision propagation and
authentication. A subsequent functional gate found a separate actual-input
driver defect; its focused correction and evidence are recorded below.
Generated World authority, performance calculation and targets are untouched.
Verify focused admission/refusal and assembly fixtures,
then satisfy only the exact planner's unexecuted pre-freeze gates. L4 and packaged
performance belong to the explicitly requested real frozen release, not an
architectural review. The earlier diagnostic downstream tail is accepted only
within its recorded non-authoritative identity; it does not close frozen R5 or
the projection task's P4. No main index/commit/tag, remote write or background
process stop is authorized by this implementation.

Pre-freeze implementation evidence is under
`tmp/release/performance-waiver/20261007/`. The package-owned decision is
propagated through Make, release coordination, the existing two-city runners,
Candidate admission, preparation and workspace recovery. Focused RED proved
default refusal, altered nested Manhattan evidence and recovery mutation before
explicit acceptance. Those regressions are GREEN, including native-shaped
three-run evidence, output refusal without mutation, and a real Windows 3.0.0
wrapper/archive/workspace fixture. Fixture payloads are synthetic, not release
or runtime certification. Independent focused R2 PASS covers both review fixes.

Affected fixtures passed: 33 preparation, 13 workspace, 5 v4 assembly,
3 platform, 3 finalization, 39 Pester and 10 impact-planner cases; release
contracts passed for 93 files and the unsigned entrypoint fixture passed.
Make dry runs prove default-off, explicit-on and invalid-value refusal. Changed
PowerShell parsing, Python parsing, ASCII, portable paths, reusable naming and
diff hygiene passed. The workspace fixture's explicit module loader now loads
the new policy dependency; its initial standalone import failure is corrected.
The existing preparation fixture remains cohesive at 917 lines; its next seam
would be release admission/refusal fixtures if it grows further. The release
coordinator and Kazan runner remain their orchestration owners; their next seams
would be workspace resume and two-city evidence composition. The pre-existing
preparation owner grows by 19 net lines, below the mega-file guardrail.

The first common Check rejected source changes during execution. A premature
replacement was cancelled during Python fixtures, before any Unreal work, to
correct the standalone workspace fixture loader. The replacement started
only after all focused fixtures and production source were stable. Preserve
these attempts as diagnostics; only the accepted current Check can authorize
Matrix. Do not package or benchmark to verify this implementation.

The required uncooked gate first completed Kazan gameplay but rejected an
optional local MCP bridge's abnormal shutdown. Native process-only plugin
isolation proved normal exit; the procedure lives in the
[MCP owner](../../docs/ue_engine/mcp_editor_control.md#optional-local-bridge-unattended-shutdown).
That replacement exposed backward-only Manhattan retreat blocked by a second
wall. The existing driver now chooses and retains a collision-free capsule
escape direction. Exact physics RED/GREEN and focused independent R2 PASS are
under the same evidence root; the existing World pitfall entry 45 owns the fix.
Both current-driver uncooked admissions accepted without source or generated
data mutation: Manhattan `a43d061eea7e4cac9bc55f404e587fd4` and Kazan
`6c08caefb71748b0b62e6dff6c5df2cb`. Both reached four waypoints and proved
streaming and collision slide; neither certifies performance. `plan-escape.json`
preserves the exact base/worktree plan: L1, canonical Kazan Matrix, eight verifies,
no candidate owners, and L4 reserved for the frozen release. The invalidated
common Check and Matrix were replaced once after the driver fix, both PASS:

- Check: `Saved/Validation/WorldPipeline/check-20261007T200625Z/result.json`,
  SHA-256 `df1568bc8d50b2747b3b391c3f7db8a0b0ad615bc01b2a345a0bf6a27e11de40`.
  All eight suites accepted, including 205 lifecycle and 69 native cases.
- Matrix: `Saved/Validation/WorldPipeline/run-20261007T201651Z/result.json`,
  SHA-256 `70b6e2574ccbf42e2e346f23433ee1c7a6327abcdd791109cf54ea8b798dfe7b`.
  Canonical mode, five realization legs and saved-output comparisons accepted.
- Authenticated common contract:
  `9b758006bc57aa92368c1d4eba3cfda0a08b4f05b499cced0592402e59d56ff3`.
  Matrix authenticates the Check receipt. All eight listed verifies passed
  through native and twin verification. ProjectWorldData restored all 3,096
  files with identical before/after tree hash
  `dfc6df6e3d39307d63d4d69473bbf7a00166d241e35935fe12eef59afe818b66`.

`closure.json` in the evidence root records `world_gates_executed=true` for the
pre-freeze scope, with L4 explicitly pending. These were the accepted inputs
to the `6b82046` frozen attempt. Their source binding was superseded by the
accepted current Check and Matrix above. Retain these historical receipts;
do not rerun their gates.
Retain INCONCLUSIVE performance and strict functional/integrity decisions.

### Earlier frozen R5: public dependency closure

The operator committed the centralized license correction at
`4876552bb419630e47158d5a92e0b0bacb31a0de`. One native unsigned
`make release 3.0.0 RELEASE_SIGN=0 SOURCE_COMMIT=<that-full-SHA>` ran and
stopped at the developer dependency audit:

| Boundary | Result at the frozen SHA |
|---|---|
| Kazan packaged gameplay and three-run performance | Accepted; aggregate frame p95 13.4586 ms |
| Manhattan through the same Development package | Accepted; aggregate frame p95 12.6178 ms |
| Shipping gameplay, CVars, water and IoStore | Accepted; 2,077,138,948 bytes; Manhattan map in chunk 11 |
| Isolated public Editor build and both public World projections | Accepted |
| Tagged component-license manifest | Accepted; no first-party fixture NOTICE required |
| Clean developer install, no-op reinstall, Editor build and skill projection | Accepted |
| Public developer dependency audit | Rejected; exact report removed by failed-input cleanup |
| Public map loads and combined unsigned workspace | Not executed |

Evidence root: `tmp/release/r5/<that-full-SHA>/`; `release.log` preserves the
native run. The derived local public commit was
`248ebf2b848caee6af4713ae634d165588599541`. The developer payload identity was
`ff1de390a317cf9bcc5e0e95a917373659dd6fa0d07aa7eca329cfdf06fe1e73`.
No publication or signing ran; the working tree was clean after owned cleanup.

The earlier readiness claim missed the unresolved dependency-closure item
under Pre-candidate items. Keep valid upstream evidence for this frozen SHA;
do not debug with another package/cook/R5.

Focused diagnosis and correction:

- The private checkout audit reports 20 missing packages and 22 unselected
  dependencies. A native synchronous registry gather resolves all missing
  packages and discovers 45 unselected dependencies. This is diagnostic,
  not the failed public checkout's report: public profiles exclude gameplay
  and vegetation, so private tree/bottle findings do not establish public gaps.
- The exporter never started a full registry search. Add native synchronous
  discovery before traversing dependencies. Its regression failed with a
  missing transitive package before the correction.
- Rejection printed only the report path, and enclosing release cleanup removed
  that report. Print each issue's code, package and reason to the retained log.
  Its regression failed before correction and proves diagnostics survive report
  deletion. Both corrections passed the initial eight dependency-audit fixtures.
- Reconstruct only the two public projections through the existing transactional
  wrapper, retain their selected bytes and dependency inventory under the evidence
  root, and restore private generated content exactly. No new checkout, cook,
  S5, production enrollment or runtime-performance iteration is involved.
- The payload selection fixture file reached 719 lines. It remains the owner of
  the shared composer's admission contracts; the next SRP seam, if needed, is
  fixtures grouped by authority family. No helper file is needed for this slice.
- The restored public-only inventory has 2,827 audit roots, 2,829 traversed
  packages and zero missing packages. Exactly one dependency rejects:
  `/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1`. The fixture
  map, vegetation and gameplay objects are absent from this public closure.
- Independent architect R1 accepts explicit `producer_input` selection in the
  existing release contract. The descriptor only declares the input; the contract
  separately selects its exact bytes, native class, first-party configuration
  provenance and preferred C++ source. No registry, NOTICE, new production file,
  World generator change or binary regeneration is needed. Installed engine
  source confirms the producer duplicates a class default object, not an Epic
  content asset, then configures its own material, channels and pipelines.
- Both generated-definition authorities were refreshed with their existing
  generator and retained native inventory. Comparison proved only
  `release_contract_sha256` changed: all sources and binary assets are unchanged.
- All 32 affected payload/dependency cases are green across the initial run
  (29 green) and exact replacement of three installer fixtures. Their fixture
  checkout had hardcoded old owner markers; it now derives descriptors from the
  release contract. The producer-input selection regression was RED before the
  new kind, and hash/owner/package/source/provenance/duplicate/undeclared-input
  refusals and native class mismatch are covered.
- Exact PlanOnly against the frozen base requires L1, one Kazan Matrix in
  `canonical_authority` mode and eight verifies; L3 owners are empty, L4 false.
  Plan: `dependency-diagnostic/plan.json` under the evidence root. No PIE,
  source replay, S5, cook or package is required for this correction.

- The focused native policy proof accepts 2,828 selected packages and 2,829
  traversed packages with zero issues. Its inventory transparently combines the
  original public inventory with a read-only native inventory of the selected
  input. All five focused packages, classes and outgoing edges match the original
  public inventory; the input bytes match both the declaration and frozen LFS
  identity. This proves the uncommitted selection policy, not a clean public
  checkout or frozen R5. Original inventories remain unchanged.
- Independent architect R2 PASS covers the final code, admission/refusal guards,
  generated hash bindings, native provenance and the explicitly bounded proof.
  No further implementation correction is required within that reviewed scope.
- All remaining selected preflight checks passed: 93 World contract files, one
  realization-schema fixture, ten planner cases, three manifest-staging cases,
  twenty public-source projection cases and the linked-worktree mirror fixture.
  Release-contract schema, Python syntax, license, portable-path, reusable-name,
  documentation ASCII and diff hygiene checks passed.
- Common Check: `Saved/Validation/WorldPipeline/check-20261007T121940Z/result.json`,
  SHA-256 `13bbf05c71208c50837867b85383f86ff2e818b13c41404aea6cab8fdac3623c`.
  All eight suites passed, including 203 lifecycle cases, 69 native cases and
  the twin. Five planned verifies passed in native automation; Map, MeshTerrain
  and Pipeline passed in the twin receipt under
  `Saved/Validation/WorldRealization/verify-twin/4b1f1f47d3f249a8a70d74fde90d3d2d/`.
- Kazan Matrix: `Saved/Validation/WorldPipeline/run-20261007T123157Z/result.json`,
  SHA-256 `0de34c0d06379be5480792ce538d7bd3eb07fbea27aa74e00c9a8849182c7f8c`.
  `canonical_authority` mode passed all five legs and equal saved terrain, map
  and building projections. All 3,096 ProjectWorldData files restored exactly;
  before/after SHA-256 is
  `dfc6df6e3d39307d63d4d69473bbf7a00166d241e35935fe12eef59afe818b66`.
  Common proof-input SHA-256 is
  `2e312e60d47aafc38d1aa03561baf27442076fb7d98ce437888b4e57b18b0972`.
- Final PlanOnly (`dependency-diagnostic/plan-final.json`) preserves the exact
  initial requirements: L1 true, Kazan `canonical_authority`, all eight verifies,
  no candidate owner and L4 false. Its `presentation:v1` entry has no standalone
  verify; the saved presentation is covered by the Map projection. The separate
  correction ledger (`dependency-diagnostic/closure.json`) records
  `world_gates_executed=true` and no outstanding required pre-commit gate.
  No source changed after independent R2 or during the World gates.

### Diagnostic unsigned tail after the dependency correction

The operator committed the dependency correction at
`203667beb4c90a5377ded0ae95dd852e884ddb8e`. The diagnostic evidence root is
`tmp/release/diagnostic/efd7d461/`. Scratch Git identities are authorized there;
they are not release authority. No main-checkout staging, commit, tag, remote
write, signing or package/cook was performed during this continuation.

- Native public source validation, developer composition, clean installation,
  no-op reinstall, public Editor build (285 actions), dependency audit and both
  real public map loads passed. The audit accepts 2,828 selected packages,
  2,829 closure packages and 22,641 dependency edges with zero issues.
- The first workspace initialization rejected an unexpected Candidate `debug`
  directory. `package_release.ps1` had placed IoStore inspection receipts and
  listing logs inside the distributable tree. Its existing owner now writes
  these diagnostics under invocation-specific `tmp/package/iostore/` paths.
  No workspace guard, inspection rule or package identity formula changed.
- The permanent regression in the existing preflight fixture executes the
  actual packager inspection block twice. The original owner fails the path
  assertion; the corrected owner passes, preserves the Candidate inventory
  and native package hash, and creates distinct diagnostic paths. All nine
  preflight cases passed; independent architect R1 and focused R2 passed.
- Native inspection of the retained real containers passed: 10,470 entries,
  69 expected object definitions and eight capability references, with no
  missing or forbidden package. Receipts and four listing logs are outside
  the Candidate (`iostore-location-native.path.txt` identifies the receipt).
- The original packaged Candidate and native composite remain immutable.
  Only a scratch projection excludes the five auxiliary diagnostic files.
  Its full-tree hash is
  `63a766c306e8252c1bfc6603b94634c1cdbac2562e1072c66209b0250931d02c`,
  distinct from the original accepted full-tree hash
  `90e7d5ef42d490fe782bae3e5bee2c71d409042d2959d8b309ff766ce9e1f3c1`.
  `player-projection-evidence.json` records this non-authoritative derivation;
  unchanged Windows bytes and executable authenticate runtime-evidence reuse.
- Current scratch public source/tag identity is
  `ff56461cfee3d54766b8a78550106cfff3a2858c`; developer payload identity is
  `b04c5e59bf1abb1800dde9221a0d4fd8a0152dca351d7dd3815fc09f46f29768`.
  Only the package owner, its fixture and README changed in that source.
  Payload bytes, audit seeds and runtime inputs remain identical; the native
  dependency validator revalidated the retained inventory against the current
  identity. `upstream-evidence-reuse.json` records the build/map reuse boundary.
- Native player archive creation and 7-Zip validation, complete current release
  preparation, workspace initialization/adoption, unsigned release verification
  and strict workspace verification all passed. The final diagnostic workspace
  is `workspace-current/`, not the preserved first rejected `workspace/`.
  It uses Windows-only workspace v1/manifest v3, remains pending owner approval,
  and contains concrete 3.0.0 Player and Developer highlights without fallback.
- `plan-after-iostore.json` requires fresh Common Check, one Kazan Matrix in
  `canonical_authority` mode, all eight verifies, no candidate owner and L4.
  Common Check passed all eight suites, including 203 lifecycle and 69 native
  cases. Its receipt is
  `Saved/Validation/WorldPipeline/check-20261007T135743Z/result.json`, SHA-256
  `37f9e84037333ff0d52f6152df6787b41b411888bbdf40c2e8b82af4505c91a9`.
  All eight planned verifies passed; `world-verifies.json` authenticates the
  five native completion markers and three saved-twin projections. The common
  proof-input SHA-256 is
  `bc47936202c75b70175a86e954ff7994bf126e95544188ac03b8d8eca80f97e1`.
  Kazan Matrix passed all five legs in `canonical_authority` mode. Its receipt
  is `Saved/Validation/WorldPipeline/run-20261007T141449Z/result.json`, SHA-256
  `495052f73c8335689d67225c2930ff982c344313e48218773f966b767dc1e818`.
  Saved terrain and World projections match across the first and clean legs.
  All 3,096 ProjectWorldData files restored exactly; before/after SHA-256 is
  `dfc6df6e3d39307d63d4d69473bbf7a00166d241e35935fe12eef59afe818b66`.
  L4 is reserved for the one final frozen unsigned R5; diagnostic
  assembly and retained runtime evidence do not satisfy frozen acceptance.

Next: the operator commits the four changed files once. Obtain the resulting
full SHA, then run one frozen unsigned R5. `closure.json` records the complete
diagnostic tail, satisfied pre-commit requirements, exact current identities,
protected original package and unchanged main Git staging. Its separate L4
entry remains outstanding until frozen certification; no source changed after
focused R2 or during the World gates. Do not rerun PIE/S5 or retry historical
scratch cleanup.
No new production framework or NOTICE is needed. The rejected R5 remains
rejected and this task stays current until final frozen certification passes.

### Frozen R5 result and fixture-boundary correction

The operator committed the highlights/version correction at
`049e06a42e1f121bf9b816a6dc19996e552740e6`. One native unsigned
`make release 3.0.0 RELEASE_SIGN=0 SOURCE_COMMIT=<that-full-SHA>` ran:

| Boundary | Result at the frozen SHA |
|---|---|
| Kazan packaged gameplay and three-run performance | Accepted; frame p95 12.9586 ms |
| Manhattan through the same Development package | Accepted; frame p95 12.4809 ms |
| Shipping gameplay, console-variable and water proofs | Accepted |
| Isolated public Editor build and both World projections | Accepted |
| Filtered public source and developer payload composition | Completed; dry run only |
| Component-license manifest | Refused `Test/test_latest_state.json`: unknown boundary |
| Clean developer install/build, map loads and combined unsigned workspace | Not executed |

Both cities used the 16.67 ms budget with zero host-load allowance. The frozen
player composite is under `Saved/Validation/WorldRealization/playable-tour/`
operation folder `c90c9b3eb53d46dcaaa08b0bdb3583f8`; Manhattan's aggregate is
under `manhattan-showcase/38d4d6a007834309ae3e302dd10785b5/`.
The Shipping game measures 2,077,138,948 bytes versus 5,102,176,124 bytes for
the retained 2.0.0 game projection (including its verification sidecars).
Native IoStore inventory confirms Manhattan's map occurs once in chunk 11.
The per-owner comparison and preserved public projection receipts are in
`tmp/release/r5/049e06a42e1f121bf9b816a6dc19996e552740e6/`.

The failing files are first-party Orchestrator UE-facing fixtures, established
by their introducing commit `edc92c267`. The operator rejected adding
`Test/NOTICE`: ordinary first-party components inherit the root license through
the centralized router. The shared `validate_licensing.py` owner now recognizes
UE fixtures under `Test/` and `Tests/`, plus the existing editor, cinematic and
asset-check integration components. Their three redundant first-party notices
were removed; genuine upstream notices and provenance guards remain intact.
Root license terms are unchanged. No registry, schema or new source file was added.

Regression tests reproduced the unknown fixture boundary and missing integration
classification without local declarations. All 19 manifest and 15 validator
cases now pass, including unknown-owner, original-terms, ownership and mixed-process
refusals. A diagnostic filtered working-tree inventory classified all 2,994 paths
without error; it is not tagged release evidence. Focused release preflight,
licensing, portable-path, ASCII, local-link and diff checks passed.

PlanOnly against the exact operator base above required L1, one Kazan Matrix
in `canonical_authority` mode, eight verifies, no candidate owner and no L4
for this correction. Exact plan: `tmp/release/license_router/plan.json`, seven
production paths; final PlanOnly confirmed the same requirements. Replacement
gates passed without further source changes:

- Check: `Saved/Validation/WorldPipeline/check-20261007T070703Z/result.json`,
  SHA-256 `522ec87805a32a804da720d573e7aecdead8af7c2dc71a31d2377b2a480f716c`.
  All eight suites passed, including 203 lifecycle cases, 69 native cases and
  the twin. All eight planned verifies are covered by native automation and
  `Saved/Validation/WorldRealization/verify-twin/21ae8a543b14434ea8369b2e21a1cde2/verify.json`.
- Matrix: `Saved/Validation/WorldPipeline/run-20261007T071852Z/result.json`,
  SHA-256 `6ebd99cb809ac8a4f00f41bfa0b6a765d233363304c1e6c39c2f48ffabc16413`.
  All five legs and projection comparisons passed. All 3,096 ProjectWorldData
  files restored exactly, with equal before/after hashes.

The current receipt loader authenticated common proof-input SHA-256
`085fc50ac01542cd2f5d605009ea58a5b7b3de97943afed6b461d8da48205443`.
`tmp/release/license_router/closure.json` records `world_gates_executed=true`
for every requirement of this correction's plan and no outstanding patch gate.
The previous notice-based receipts are historical and do not authenticate the
current router. Their ledger, `component-closure.json` in the R5 evidence folder,
is marked superseded. The combined release R5 remains incomplete.
The release runner removed its owned failed projection checkout; the four
historical scratch repositories were untouched. No signing or publication ran.

The router correction was committed and its replacement R5 is recorded above.
Do not use R5 as a debugger, replay raw acquisition, run S5, or retry historical
scratch cleanup. The optional
leading-zero SemVer alignment remains deferred; the release was already frozen.
Earlier sections record evidence for their earlier source inputs.

### Release highlights and explicit preparation identity

The follow-up review correctly found two omitted R4 requirements (V16). D17
removed the per-version contract proposal, not the need for concrete 3.0.0
highlights or explicit preparation identity. The Windows-only policy is committed
at `3aac3847333a588a316e85c70850bdbbc08cc6b4` and remains accepted.

Two regression cases reproduced the placeholder highlights and successful
historical-default preparation. The existing README generator now supplies
bounded Player/Developer 3.0.0 highlights. Direct preparation requires the version
and derives its tag; the coordinator and documented example pass that one value.
No registry, schema, loader, extraction, dependency or new file was added.
The ten-line highlights branch remains with the existing guide generator; it
does not require extracting the pre-existing mega-file. The coordinator only
removes the redundant tag argument; its next SRP seam remains final source checks.

All 48 affected Python fixtures passed, including archived formats, concrete
3.0.0 README output, version omission with no output/Candidate mutation, and
real 2.0.0/3.0.0 wrappers deriving the tag. The unsigned entrypoint fixture,
eight preflight Pester cases, 93 World contract files, the realization-schema
fixture and ten release-planner cases also passed. Syntax, portable paths,
ASCII, document links and diff checks passed; the 2.0.0 README is byte-identical.

PlanOnly is recorded in `tmp/release/assembly_metadata/plan-current.json` against
current HEAD `3aac3847333a588a316e85c70850bdbbc08cc6b4`, and `plan.json`
against original operator base `ac9f8c998acf5d8bdcaa8945afcf604b49dcdb8a`.
Both require L1, one Kazan Matrix using `canonical_authority`, eight verifies,
no candidate owners, and L4. The fresh Check and Matrix passed once:

- `Saved/Validation/WorldPipeline/check-20261006T135049Z/result.json`;
  operation `check:common:20261006T135049Z`, SHA-256
  `e227d0573b5cfe4513b1a058f453d3472544201970aabbc05c5f9833dc927d45`.
  All eight suites passed, including 203 lifecycle cases, 69 native tests and
  the twin. All eight planned verifies passed; the twin receipt is
  `Saved/Validation/WorldRealization/verify-twin/dbead1947d594f1b86df9e9d78e35b21/verify.json`.
- `Saved/Validation/WorldPipeline/run-20261006T140807Z/result.json`;
  operation `validate:kazan_territory_v1:20261006T140807Z`, SHA-256
  `4b52a8c18e2aaf06607afabd1ed96f489937759da896191affcad68959686892`.
  All five legs and both projection runs passed. Exact restoration accepted
  all 3,096 ProjectWorldData files with equal before/after SHA-256
  `dfc6df6e3d39307d63d4d69473bbf7a00166d241e35935fe12eef59afe818b66`.

The current common proof-input hash is
`d73c030eae4a94ce096a2537d5d47e82ea04ab2e256b83e7fd30264bec2fb1a0`;
the authoritative receipt loader accepted it after Matrix. Execution ledger:
`tmp/release/assembly_metadata/closure.json`. It records
`pre_freeze_world_gates_executed=true`, no outstanding pre-freeze gates, and
`world_gates_executed=false` because L4 awaits the frozen unsigned R5.
Earlier sections retain evidence for their earlier source inputs.
No gameplay PIE, S5, package/cook or historical scratch cleanup is part of this
correction. No new independent R2 is required for these bounded text/default
corrections. Stop for the operator commit; the next package/cook operation is
one unsigned R5 from its full SHA. No pre-freeze gate rerun is required merely
for the commit or this evidence-only handoff.

### Committed Windows-only release policy correction

The committed gate handoff at `74e3797e667d71920078dc8758ea78408d4ff398`
missed the still-unimplemented R4 blocker: six SemVer-to-platform checks.
The operator narrowed R4 to one global Windows-only policy (D17). The unused
per-version loader, contracts and schema draft were removed before production
integration. No new source files, dependencies or Linux acceptance work remain.

`release_platforms.py` now owns the policy; both PowerShell coordinators consume
its public CLI. Workspace/manifest tools use recorded formats rather than version
thresholds. The focused 3.0.0 regression failed before the fix with the expected
v4 manifest demand. After the fix, 16 workspace/platform cases, 29 preparation/
finalization cases and the real PowerShell unsigned entrypoint fixtures passed.
Unknown workspace refusal preserves its complete file inventory.

`release.ps1` remains the coordinator for selecting new versus resumed workspaces;
its small policy change is cohesive. Its next possible SRP seam is final public
source validation, which this fix does not extract. `prepare_release.py` shrank
by two lines; no prerequisite mega-file refactor is needed for this fix.

Independent R2 PASS covers the minimal code/docs change and focused consumers.
The real 3.0.0 preparation wrapper passed with no Linux inputs (one additional
case); release preflight Pester passed all eight cases. Syntax, ASCII, diff and
portable-path checks passed.

Exact original-base PlanOnly is captured in
`tmp/release/platform_contract/plan-global.json`: L1 required, one Kazan Matrix
in `canonical_authority` mode, eight verifies, no L3 candidate owners, and L4
required. Current Check/Matrix passed once, with no source changes after R2. L4 remains
reserved for one frozen unsigned R5. Earlier World receipts below prove their
original input set; the following fresh receipts cover these shared script edits:

- `Saved/Validation/WorldPipeline/check-20261006T125435Z/result.json`: eight
  suites accepted, including 203 lifecycle cases, 69 native cases and the
  synthetic twin. Receipt SHA-256
  `cb258ce181a43aeaa0aea1fdcca0d24af87750ec3b88ae89362237f211d171a5`.
- `Saved/Validation/WorldPipeline/run-20261006T130726Z/result.json`: Kazan
  `canonical_authority` Matrix accepted. Receipt SHA-256
  `4969512537d7c4f1939e8336fe56ad727cbb77f2938920707c0f71b1e0867429`.
  All five legs and saved projections passed; exact ProjectWorldData restoration
  passed for 3,096 files with equal before/after hash
  `dfc6df6e3d39307d63d4d69473bbf7a00166d241e35935fe12eef59afe818b66`.
- All eight named verifies passed in the fresh common Check. Twin evidence:
  `Saved/Validation/WorldRealization/verify-twin/8a2a25e90cef44ada8a1f13de17bbd89/verify.json`.

Execution ledger: `tmp/release/platform_contract/focused-global.json`. It records
`pre_freeze_world_gates_executed=true` and no outstanding pre-freeze gate.
`world_gates_executed=false` deliberately retains the planned L4 requirement.
Stop for the operator commit; the next package/cook operation is one unsigned
R5 from that new full SHA. Do not rerun already-accepted pre-freeze gates solely
for a commit or this evidence-only handoff.
No package/cook, gameplay PIE, S5 promotion or historical scratch cleanup is part
of this fix. The next package operation still requires an operator commit.

### Prior routing World gate closure

The review finding was valid: focused routing proof did not satisfy the World
requirements reported for the production planner edit. The implementation is
committed at `9f1c6b61911c243e84abca63fa460a0b2ed70c7b`; no implementation
change was needed to close this finding.

PlanOnly captured the complete slice from operator base
`ac9f8c998acf5d8bdcaa8945afcf604b49dcdb8a` through that commit and the working
tree. Exact plan and execution ledger: `tmp/release/world_gate_closure/plan.json`
and `closure.json`. Required: L1, one `kazan_territory_v1` L2 Matrix in
`canonical_authority` mode, and eight named producer verifies. No L3 candidate
owners, durable identity changes or L4 requirement were selected.

A README-only commit `444ba873608385bee4e83ddc51c148a7e13a0b03` landed while
the gates ran. Final PlanOnly captured that additional documentation path and
identical World requirements; the current common Check identity was revalidated.

ACCEPTED receipts under `Saved/Validation/WorldPipeline/`:

- `check-20261006T110123Z/result.json`: all eight common suites, 203 lifecycle
  cases and 69 native World tests passed; the synthetic twin passed as well.
- `run-20261006T111637Z/result.json`: Kazan Apply, no-op, road locality, authored
  overlay rejection and clean reconstruction passed, including saved projections.
  Exact pre-run generated inventory restored: 3,096 files, equal before/after
  digest `dfc6df6e3d39307d63d4d69473bbf7a00166d241e35935fe12eef59afe818b66`.

All eight selected verifies passed: BuildingMassing, Gameplay, Map, MeshTerrain,
Pipeline, Road, Vegetation and Water. Map/MeshTerrain/Pipeline use the accepted
synthetic twin commandlet; the other five use native automation. The planner's
`unrecorded_verifies=[presentation:v1]` diagnostic remains recorded; it names no
additional executable verify and no presentation baseline or revision changed.

The separate execution ledger records `world_gates_executed=true`; PlanOnly
remains read-only. No raw replay, durable enrollment, gameplay PIE rerun,
package/cook, R5 or refused historical scratch cleanup ran. Existing accepted
two-city gameplay evidence remains valid. No additional R2 is needed for this
execution-only closure. Its handoff was committed at
`74e3797e667d71920078dc8758ea78408d4ff398`. The Windows-only correction above
supplies the current pre-freeze receipts. Packaged release acceptance is still
unverified.

### World planner composition correction

The prior routing fixes are committed at
`ac9f8c998acf5d8bdcaa8945afcf604b49dcdb8a`. The external follow-up correctly
identified that the adapter's JSON-only filter omitted production source,
scripts, tool implementations and generated packages from World requirements.
The RED equivalence table reproduced eight omitted production path cases.

R1 independent PASS: put the release projection beside the ordinary World
planner, filter only known proof/docs surfaces, and delegate retained inputs
unchanged. Keep `.txt` dependencies, production package scripts and mixed
production changes. Full L1/Matrix decisions and Check identity remain intact.

CHANGED: World-owned release planning seam, thin adapter, equivalence/negative
controls and existing Git-fixture producer declarations, owning SOTs/handoff.
UNTOUCHED: ordinary planner decision logic, Check fingerprint inputs, gameplay,
profiles, generated authority, native binaries and the frozen release boundary.

The full production result must equal the direct ordinary planner result;
proof-only changes report no generation plan. Compiler/source L1 requirements
add WorldContracts without relabelling focused checks as full L1/Matrix proof.
GREEN: 10 World tests (14 production variants plus proof/mixed controls), 9 Pester
cases, 26 existing planner cases and 93 profile/schema files; PlanOnly, parse,
ASCII, link, path/name and diff hygiene passed. Independent R2 PASS with no
concrete blocking finding. Evidence under `tmp/release/preflight-routing/composition/`:
`red.log`, `world-router.log`, `planner.log`, `world-contracts.log`, `pester.json`
and `plan.json`. This focused proof initially left the reported World gates
unexecuted. The correction is committed at
`9f1c6b61911c243e84abca63fa460a0b2ed70c7b`; their execution is closed in the
Required World gate closure above. Packaged acceptance remains unverified.
Do not retry refused scratch cleanup.

### Release impact routing correction

The accepted admission slice is committed at
`e41f774445b6e526b737d83825d7474ef67f197a`. Its real two-city PIE and native
gameplay evidence below remains valid: this follow-up changes only impact
selection, contract checks and handoff documentation.

Operator authorized evaluation and valid fixes from the external routing review.
R1 independent design review accepted shared rename-safe Git collection,
additive `-Owner`, World-owned profile/schema selection and explicit unexecuted
Matrix requirements. It required projection selection for consumed Runtime,
Presentation and Authored inputs and validation of unchanged schema consumers.
The blanket public-profile -> private PIE claim is incorrect: private PIE reads
existing maps; the public profile belongs to projection/contract checks.

Owning black boxes: World impact planning/contracts and release orchestration.
CHANGED: shared snapshot API, World release adapter/contract validator, additive
release selection, focused router tests and owning SOTs.
UNTOUCHED: gameplay, maps, profiles, generated authority, native launch arguments,
performance thresholds and frozen release/publication boundaries.

RED: three focused routing cases rejected the missing plan/World adapter seams.
Independent R2 also found toolkit-owned profile schemas fell through. The real
Git compiler-schema route returned no owners, all four toolkit profile/budget
schema cases lost Matrix metadata, and an invalid unreferenced toolkit schema
passed validation. The World owner now includes toolkit schemas and their
existing profile consumers; all six RED assertions are GREEN.

GREEN: 9 Pester cases, 8 World impact/contract cases, 26 existing planner cases,
93 World profile/schema files, public projection fixture, PlanOnly, PowerShell
parse and ASCII/path/name/diff hygiene. Independent routing R2 PASS: no remaining
concrete blocking finding. Evidence under `tmp/release/preflight-routing/20261006/`:
`routing-pester.json`, `pester.json`, `world-router.log`, `world-contracts.log`
and `plan.json`. This slice is committed at
`ac9f8c998acf5d8bdcaa8945afcf604b49dcdb8a`; planner composition and its required
World gates are closed above. The full packaged release is still unverified.

Automatic approval refused manual cleanup of four earlier test Git fixtures in
ignored `tmp/release/preflight-tests/` (reason: blocked by policy). They remain
disposable diagnostics. New routing fixtures clean their recorded repositories
after containment checks; that cleanup passed. Source and index were preserved.

For a necessary routing correction, start with routing, schema/consumer fixtures,
parse and governance hygiene, then satisfy the exact authoritative World plan.
Do not add real PIE, unrelated regeneration, cook or R5 as a debugging loop.

### Uncooked admission slice

Implementation, runtime proof and independent R2 are closed and committed at
`e41f774445b6e526b737d83825d7474ef67f197a`. The remaining commit boundary is
the World gate handoff above.
Do not repeat generation, packaging or real-map probes for documentation
handoff alone. The next frozen unsigned R5 remains final artifact certification.

Operator request: apply the supplied release-debugging review before another R5.
R1 independent design review PASS: reuse the existing product route, performance
collector, playable driver, and residency tracker; add no simulator or native test mode.
The uncooked wrapper accepts gameplay only, including a native p95-only rejection
after independently proving sample validity, zero streaming failures, and full input,
collision, and residency evidence. It never certifies performance.

Owning black box: World product-runtime verification and release test routing.
Contract: one current-source, no-cook command; fresh operation-bound receipts,
native uncooked PIE identity, stable source/modules and generated authority.
CHANGED: runtime receipt envelope metadata, operation argument/description adapters,
uncooked runner and tests, bounded real-input driver overhang recovery,
thin release preflight, owning test/release docs.
UNTOUCHED: CharacterMovement and collision physics, generated maps, runtime profiles, generators,
canonical authority, final performance thresholds, frozen release publication.

| Invariant | Acceptance surface | Envelope | Cheapest proof | Final proof | Stop |
|---|---|---|---|---|---|
| Real center return, descent and forward slide | CharacterMovement and collision on each production map | Current Editor modules, normal RHI, native single-client PIE, menu/loading route | Receipt refusal fixtures | Unflagged Manhattan control, then precise Manhattan and ordinary Kazan | No R5 admission if geometry/control evidence is absent |
| Exact current inputs and no generated mutation | Source/modules, runtime profile and generated path/hash inventory | Current checkout, no cook or regeneration | Identity/mutation sabotage fixture | Equal before/after inventory and bound receipts | Reject changed or unknown input |
| Correctness cannot stand in for package performance | Native cooked identity and outer acceptance scope | Uncooked gameplay versus final package | Envelope and native p95-refusal tests | R5 remains the only final artifact proof | Reject wrong-envelope certification |
| Release fixes select their own checks | First failed owner and dependency closure | Focused scripts in current checkout | Owner-routing fixture and historical guard audit | Independent R2 of runner and evidence | No automatic package/territory replay |

Execution: focused RED fixtures -> implementation -> one incremental Editor build ->
real known-bad control and two-city GREEN -> historical guard audit -> independent R2.
Only then request the operator commit and resume the frozen unsigned release.

The initial `-game` diagnostic at `tmp/world/playable_tour/uncooked/977f4b5bda26468aac06e7923557d9fd/`
asserted in native `FWorldPartitionActorDesc::Serialize`: Mesh Partition's editor
component classes are required to read these uncooked descriptors. Generated
authority stayed byte-identical. This is not the slide RED control. R1 amendment
PASS selects native full-Editor PIE with transient Standalone settings and
canonical map identity after PIE prefix removal; plugin types and saved maps
remain untouched. The following launch diagnostic was stopped after native logs
localized incorrect quoting of `ExecCmds`; it is not gameplay evidence either.

The actual PIE probe `e2f7592a05134fb6b6acc35300b392c3` entered a different
failure before final return: forward/upward input stalled under a generated
building underside on leg 2. A live native capsule sweep identified downward
impact normal, clear backward travel, active input, and zero velocity. R1
amendment PASS approved generic bounded `S+Space` recovery in the test driver;
it changes neither physics nor generated collision. The native regression
first proved the real blocking underside and failed only backward-input
selection/forward release. That RED is an overhang regression, not the
historical cooked zero-slide control. Probe exit was clean after moving PIE
cleanup to the native editor pre-exit delegate. Flat and linked plugins are
included in the loaded-module inventory. Native policy fixtures now run cold
with exact authenticated names, NullRHI, and distinct ignored log directories.

Historical failure-class audit by the independent reviewer:

| First failed property | Cheapest permanent guard | Invalidated by its fix | Reusable input/evidence |
|---|---|---|---|
| LFS object materialization | `test_isolated_source_workspace.ps1`: local LFS, pointer, corrupt committed payload and missing-object fetch arms | Isolated source checkout/materialization proof | Private game and unrelated projection policy |
| Linked-worktree Git handoff | `test_mirror_linked_worktree.ps1` plus `test_developer_payload.py` linked-worktree composer arm | Mirror adapter and public composition | Private gameplay/cooked bytes; WSL composition coverage requires the conditional arm to execute |
| Untracked manifest-projected package | `test_developer_payload.py::test_compose_accepts_only_digest_verified_untracked_projection` | Selected payload/archive and manifest-digest proof | Game correctness and unrelated tracked source checks |
| Pinned runtime forwarding | `test_public_world_projection.ps1`: actual realizer binding and identity drift rejection | Public World runtime projection | Private generated authority and package correctness |
| Vegetation omitted by public profile | Exact `Project.World.Realization.Runtime.TerritoryInstancingPolicy` | Runtime instancing acceptance for selected/omitted vegetation | Canonical geography and unaffected generated layers |
| Manhattan evidence binding | `manhattan_existing_package.Tests.ps1`: map/runtime/executable/path/policy/package mutation and abnormal exit arms | Manhattan acceptance envelope | Same immutable package bytes, while their consumed inputs remain unchanged |
| Final center return produces zero physical slide | Permanent uncooked real-map probe: ordinary Manhattan RED, precise Manhattan and ordinary Kazan GREEN | Game executable and both candidate runtime proofs | Unchanged generated content and previously proved projection owners |

The historical guards exist and have focused proof from their owning slices;
the current change does not replay their packaging routes. Full source identity
and failed preparation rollback still prevent relabelling an old artifact as a
new frozen candidate. Locality governs debugging, not provenance substitution.

Actual geometry RED: `tmp/world/playable_tour/uncooked/e288baf0ad01419b8ff07cc177df5a4b/admission.json`.
The unflagged Manhattan control ran the real production route at 2500 cm,
completed the center streaming cycle and blocked descent, then native forward
slide measured 0 cm and rejected. The wrapper rejected `collision_slide`,
the process closed normally, and source/modules/generated authority remained
unchanged. This is the relevant old-policy discriminator, unlike the earlier
startup, lifecycle and ceiling diagnostics. Native overhang fixture GREEN:
`tmp/world/playable_tour/slide_diagnostic/overhang_green/tests.log`.
Two-city corrected-policy GREEN and independent R2 PASS are recorded below.

The first two-city admission `69f0e7673d754b45ba0b14c012a2f108` accepted Kazan
at 2500 cm with 2017.03 cm slide, then Manhattan's native startup refused
1920x1080 before traversal. Generated authority/source/modules stayed unchanged.
Native `FStartPIEForAutomationCommand::Update` waits for match readiness; setting
the viewport only after it returns true was too late. R1 amendment PASS keeps
the real transient render target at 2560x1440 during every startup/wait poll,
including graphics-application and travel resizes, without changing preferences
or acceptance limits. A focused RED/GREEN contract fixture now reports native
startup errors before missing traversal fields, preserving the first failing
owner. Only the affected Editor envelope and its downstream gameplay evidence
are invalidated; ordinary-policy geometry RED and other owner fixtures remain
valid. The corrected two-city replacement accepted both routes.

Admission closure:

| Proof | Result | Evidence |
|---|---|---|
| Ordinary Manhattan geometry control | RED: actual 2500 cm, blocked descent, 0 cm forward slide | `tmp/world/playable_tour/uncooked/e288baf0ad01419b8ff07cc177df5a4b/admission.json` |
| Corrected two-city uncooked gameplay | GREEN: Kazan 2500 cm / 1858.73 cm slide; Manhattan 500 cm / 1188.71 cm slide | `tmp/world/playable_tour/uncooked/2e03af07933f4fd08b4e6de779761563/admission.json` |
| Selected release preflight | PASS: 32 Pester cases, two exact native tests, release entrypoint, 26 Python cases, syntax/ASCII/path/name/diff checks | `tmp/release/preflight/c1b5a6eeb1de4e07bbc5f8ab4ce03b69/preflight.json` |
| Independent R2 | PASS, no remaining blocking findings; gameplay and diagnostic performance scopes kept distinct | Non-author source, artifact, RED/GREEN and final documentation review on 2026-10-06 |

Both native gameplay children returned an FPS-budget-only refusal under Editor
load. The wrapper independently proved samples, input, streaming and collision;
it accepted gameplay and explicitly did not certify performance. Independent
review authenticated every artifact hash and raw sample count (6325 / 7003),
actual production classes, map/profile/radius bindings, normal process exits,
and unchanged generated authority/source/modules. Final documentation edits
after this probe alter no consumed gameplay or launch input; their hygiene is
checked separately. Existing probe hashes are preserved, not relabelled as a
future frozen candidate. Packaged performance, Shipping and final projection
remain R5 properties.

Current state: concern 1 is done; production Kazan and Manhattan use Mesh Terrain
([record](../01_done/world/20260923-1620_world_migrate_terrain_to_mesh_terrain.md)). Concern 2's
generated terrain material is in production on both cities and its final development delta passed
review, and the task is done. Generated texture and material identities now bind only the public
contracts they consume, so an internal ProjectTexture or ProjectMaterial change no longer reaches
World. The public dependency correction is verified in the current checkout; the next frozen R5
must authenticate the assembled public checkout (see Current state and next steps). Earlier concern-2 packages are development evidence only; R2 below names
what R5 re-proves on the candidate. The committed D16 runner accepted the frozen Kazan Candidate
at `09b7b6532`, including Shipping and water. The release then reached the public World projection
and refused an omitted runtime-profile argument. Commit `76ed07763` forwarded runtime only after
setting both public runtime IDs to `none`, weakening the map contract. The local correction restores
the IDs and fixes runtime acceptance for public profiles without vegetation. A focused isolated
overlay accepted both cities and restored its private snapshot byte-for-byte; it is not a frozen
release. The permanent runtime guard is committed at `f45a9c01dee51cbc2d868f0236c207579adb1480`.
The Manhattan same-package performance harness and evidence envelope are committed through
`c3c006cf12757bf0040ba7e047d1f9476f138e30`; focused R2 passed. The unsigned frozen route at
`f4cce29f3b41840790355c18eb5c5fd72cd8a806` cooked both maps and accepted Kazan's three-run
Development aggregate (Frame p95 13.293 ms), then Manhattan child 1 rejected because the final
`W` input produced zero slide after blocked descent. The first failing receipt is under
`Saved/Validation/WorldRealization/manhattan-showcase/33ab5f2f434e4dbe83d1ed0edd58852c/`.
The same cooked Development content proved that a 5 m final return accepted three Manhattan
children (7.90-9.06 m slide), while applying 5 m globally rejected Kazan. The focused fix selects
5 m only for Manhattan's performance operation and records it in the receipt. The final rebuilt
binary accepted one Manhattan child at 5 m and one Kazan child at its default 25 m, with slide,
streaming, and performance evidence. The retained Development IoStore inventory places Manhattan
in `pakchunk11`. Independent focused R2 passed with no concrete finding. Routing corrections
are committed; the remaining operator commit records the public dependency correction above;
the full release route remains unverified. The scoped runtime correction is now
committed at `e93d7d17c836616af18e23b2bca548119deffbd0`; the new admission slice
passed independent R2 and is committed at `e41f774445b6e526b737d83825d7474ef67f197a`.
No combined 3.0.0 release workspace exists.

Daily Mesh development uses the one normal repository. The separate public World projection task
keeps its isolated frozen-commit worktree because it protects final release-input reproducibility;
it is not the Mesh development environment. Exact source identity remains required for the final R5
candidate, not for every local implementation iteration. The source engine (`UE_SOURCE_PATH`) is on
`5.8.3-release` and passed its smoke check; `UE_SOURCE_PATH` remains its single configured record
(D15).

| # | Step | Runs | Verified by | Starts when |
|---|---|---|---|---|
| 1 | Payload dependency closure owners | each owner in Pre-candidate items | the release audit accepts | owners decide selection or removal |
| 2 | R4 and the public projection safety fix | `implement-approved-change` | R4 verification list; projection tests; `architect` diff review | their own approvals |
| 3 | Source packaging gate: `update_engine.ps1 -CompleteSource -RunId <dev-run> -SourceRoot <UE_SOURCE_PATH>` (D15) | implementer | `SOURCE_COMPLETE` | final packaging is needed and the status file ends with `SMOKE PASS` |
| 4 | R5 candidate, R6 publication | implementer and operator | World L4, operator walkthrough, operator approval of each publication step | concerns done |

## Contents

- Goal - Authority register - Non-goals
- Release concerns - Pre-candidate items - Sequencing
- Verified evidence
- Release gates R0-R6 - Optional Water gate
- R4 release-platform contract (decision, invariants, tasks, verification)
- Documentation plan - Reviewer brief - Completion criteria - Review record

## Goal

ALIS 3.0.0 publishes a signed Windows release. Its headline is a visible advance in the World core:
Kazan and Manhattan terrain comes from the same canonical geography through the substrate the
operator accepts in concern 1, shaded by a production generated terrain material from concern 2,
with no city-specific code or manual setup. Release platforms become explicit release data instead
of a side effect of the version number.

2.0.0 proved that a second city comes out of the same generic pipeline. 3.0.0 is major because it
moves the foundation every generated city's terrain stands on (D1, D12). Packaging,
cleanup, and internal reliability work are supporting evidence, not the headline.

## Authority register

Binding `D<n>` entries outrank agent preference and reviewer findings within their scope. Cite IDs
elsewhere; never restate or renumber them.

### Operator decisions

- **D1** Make the largest architecture moves now, while they are cheap, when a new Epic technology
  is genuinely future-proof and positioned as the next generation with strong performance and
  quality - pick it. Operator's words: "we should do hugestest architecture all moves exact now
  while we can if it really future proove, so if some tech arrived and epic claims it's will be
  next with great performance quality and etc , so we should pick".
  - Effect: 3.0.0 is defined by the terrain-substrate decision and the production terrain
    material.
  - Reason: "while we can".
  - Date/source: 2026-09-23, operator conversation supplied in this session.
- **D2** The next release is 3.0.0 with this scope and order: (A) Mesh Terrain migration first - a
  decisive same-Kazan experiment and, if it wins, production cutover; (B) Terrain Material v2,
  built against the substrate A selects; release integration inside one common 3.0.0 release
  document; Water optional, only if A and B close early. A settles the substrate before B freezes
  its final material contract.
  - Effect: two implementation tasks plus this router; no separate release-contract task.
  - Reason: not stated by the operator; the accepted proposal argued that perfecting a
    classic-Landscape material that may be retired immediately is waste.
  - Date/source: 2026-09-23; the operator accepted the proposed plan ("ok so prepare these todos
    ...").
- **D3** Release integration is a section of this common 3.0.0 document, not its own task. This
  document owns what makes the release major, the exact included and excluded scope, sequencing,
  the optional Water gate, the final comparison against 2.0.0, release and platform constraints,
  and operator acceptance and promotion. A genuinely larger release-system problem found during
  implementation is split into its own task only then.
  - Effect: R4 owns the platform/version decoupling.
  - Reason: the operator proposed it ("so better for C - just common release 3.0.0 doc with
    details?").
  - Date/source: 2026-09-23.
- **D4** Windows is the required 3.0.0 player platform. Linux is not required until it is
  separately qualified. The `>= 2.1.0` version-to-Linux coupling is removed.
  - Effect: R4.
  - Reason: not stated; Linux acceptance is parked on missing hardware (V9).
  - Date/source: 2026-09-23; accepted plan.
- **D5** Do not couple to existing stubs; keep the right separation-of-concerns naming hierarchy
  and dependency direction; build future-proof, data-driven, fully modular, black-boxed,
  decoupled, and component-driven. Operator's words: "we don't have fully architecture for such
  solutions so no need couple for existing stubs, we need keep the right SOC names hierarchy and
  vector and future proof oriented build with our data driven design fully modular blackboxed
  decoupled component driven".
  - Effect in this file: the required platform set has one explicit owner (R4, narrowed by D17). Both
    concern tasks carry the same decision.
  - Date/source: 2026-09-23, this session.
- **D6** Three tasks live in `todo/00_current/`: this router, the Mesh Terrain task, and the
  Terrain Material v2 task.
  - Effect: Material v2 stays current while its final contract waits for D2's substrate gate.
  - Date/source: 2026-09-23, this session ("I guess we need 3 todos in current").
- **D7** Even though Mesh Terrain is Experimental, it is the long-term direction: after the test
  passes, the architecture wraps fully around it, with no legacy support and no legacy mentions.
  Operator's words: "even experimental it future long going goal so we will wrap around it our
  architecture after test fully without any legacy support and mentions".
  - Effect: Experimental maturity alone is not a reason to change 3.0.0's direction. Q1 applies
    only if the test itself fails. The released product and its docs carry no mention of the
    superseded terrain path.
  - Reason: "future long going goal".
  - Date/source: 2026-09-23, this session.
- **D8** Anything that prevents a proper build is fixed now: small fixes go into this release plan
  itself, fixes that belong to a concern go into that concern's task, and wide fixes become a new
  current task linked from this plan. Operator's words: "ofcourse if some prevent proper build we
  need it fix it now, so need fix in relative scope todo or if wide create new in current and add
  in release link but small fixes hsould be in release todo itself".
  - Effect: closes Q2. The public World projection recovery audit is wide, so it moved to
    `todo/00_current/` and is a required concern below; R4 keeps the small release fixes.
  - Date/source: 2026-09-23, this session.
- **D9** City17 is a legacy demo; once City17 is restored inside the fully rebuilt Kazan with
  gameplay, the legacy City17 content is removed outright. Operator's words: "city 17 it's just demo
  legacy, when we will rebuilt terrain and etc and restore city 17 in the full rebuilded Kazan with
  game play we will remove all just plainly".
  - Effect: closes Q3. City17's SM5 default-material fallback is accepted for 3.0.0; its fix path is
    dropped.
  - Date/source: 2026-09-23, this session.
- **D10** Linux is out of scope: no Linux build ships until we gain a server; the Linux task stays
  parked. Operator's words: "linux now it's out of scope and we don't ship any linux build until we
  gain server - now it's parked".
  - Effect: closes Q4; the parked task's 2.1.0 label no longer needs an answer now.
  - Date/source: 2026-09-23, this session.
- **D11** A bigger package is fine if it buys more freedom, quality, and optimization, and the future
  path is proven robust. Operator's words: "if it bigger but we gain more freedom and quality and
  optimization and will be ensured that it future path robust - we fine".
  - Effect: closes Q5. Size is still measured against 2.0.0 and reported (R5).
  - Date/source: 2026-09-23, this session.
- **D12** Switch to Mesh Terrain for real, keep working through problems (workarounds, including
  custom C++ wrappers that override properly), and give up only when something unresolved blocks it
  completely. Operator's words: "yes point will definetly switch to mesh terrain , if test will some
  bad we will try until give up completely if something unresolved hit, but goal is this" and "if
  we falied we will try workaround even for custom wrapper cpp to override properly , if it will be
  very raw from audit and not compatible we could give up and keep some plugin with it".
  - Effect: closes Q1; the Mesh Terrain task owns the workaround loop and its give-up question.
  - Date/source: 2026-09-23, this session.
- **D13** The engine moves to 5.8.3 now: the source engine at the proper tag, the launcher binaries by
  the operator. Operator's words: "definetly need update souce code now to the 5.8.3 (do it with
  proper tag [UE_SOURCE_PATH]) , I'm already updating binaries launcher version to 5.8.3".
  - Effect: the 3.0.0 candidate is built on 5.8.3 (A5); the project adopts it through
    `scripts/ue/update/update_engine.ps1`.
  - Date/source: 2026-09-23, this session.
- **D14** "Release plan" is the right name for this file, because it is the main document of a
  release. Operator's words: "release plan is good name because it like main doc for release".
  - Effect: `00_release_X.Y.Z.md` stays; documenting it in `todo/README.md` is owned by the backlog
    task [Fix stale docs and leftovers](../02_backlog/chore/20260923-1802_chore_fix_stale_docs_and_leftovers.md).
  - Date/source: 2026-09-23, this session.
- **D15** After the source update, the configuration keeps exactly one record of the source-engine
  path; the source packaging gate runs when packaging is needed, not now. Operator's words: "after
  sources will update you could change SOT config, should be only one record for source engine
  path, so we will testing when need packaging not now".
  - Effect: `UE_SOURCE_PATH` in `scripts/config/ue_path.conf` is already the only
    record (no `ue_path.local.conf`, no environment copy), and the in-place update needs no config
    edit. `update_engine.ps1 -CompleteSource` moves from step 1 to R5; source packaging stays frozen
    until then, as the update flow requires.
  - Date/source: 2026-09-23, this session.
- **D16** Allow an automatically calculated, bounded host-load cap for the packaged
  performance comparison so development and release checks can run while the host is
  moderately busy. Operator's words: "our goal to use auto cap , so we could use the host
  aproximately performance calculations even it it unders some load".
  - Effect: the three-run packaged Kazan aggregate uses the measured host-load windows and
    effective threshold owned by the [World runner](../../scripts/ue/world/README.md).
    The native child target stays at 16.67 ms, and the adjusted result is identified as an
    approximate busy-host comparison.
  - Date/source: 2026-10-05, operator conversation.
- **D17** Keep release platform enablement global and Windows-only now; add Linux
  globally when qualified. Use the smaller architecture-reviewed fix without
  expanding the repository with per-version contracts or schema scaffolding.
  - Effect: supersedes R4's derived per-version contract design and A7. The
    existing release-platform module owns policy; existing artifact formats and
    their validation remain intact. No prerequisite extraction or notes framework.
  - Date/source: 2026-10-06, operator correction in this session.
- **D18** Implement the default-off explicit release performance waiver and
  complete its local verification autonomously. Absolute performance on the busy
  host remains INCONCLUSIVE; preserve rejected measurements and accept residual
  risk explicitly without changing the target or claiming performance PASS.
  - Effect: a valid budget rejection may be accepted at the release boundary;
    all non-performance gates remain strict. Architectural/development checks do
    not run packaged benchmarks. Final frozen release and todo closure still
    require their real acceptance evidence and the operator-owned source commit.
  - Date/source: 2026-10-07, operator: "yes performance only need check when we
    do real release, not some arhcitectural check" and "so do fully without my confirmation".

### Operator gates

- **Q1 [CLOSED by D12]:** If the
  same-Kazan evidence does not justify replacing Landscape, what are the 3.0.0 headline and scope? - gates: redefining 3.0.0 and the material task's substrate.
  Default while open: A1.
- **Q2 [CLOSED by D8]:** The public World projection
  recovery audit was deferred "before the first post-2.0 release input generation", and 3.0.0 is
  that generation. Select it into 3.0.0, or defer it again with the risk stated? - gates: R5 step 4,
  whose `make release` runs the projection (V12).
- **Q3 [CLOSED by D9]:** City17's `M_Landscape` exceeds the SM5 sampler limit and
  Shipping renders the default material on affected components. Is that a 3.0.0 blocker? - default
  while open: A3.
- **Q4 [CLOSED by D10]:** The parked Linux task binds Linux support to the version 2.1.0. With
  3.0.0 as the next release, which release should Linux target once its acceptance host exists? -
  default while open: A4.
- **Q5 [CLOSED by D11]:** The operator hopes 3.0.0 is smaller.
  If the accepted substrate or production textures grow the package, which trade is acceptable? -
  default while open: report exact per-owner and whole-package deltas against 2.0.0 and invent no
  threshold.

### Working assumptions

- **A1 [RESOLVED by D12]:** If Q1 rejects Mesh Terrain, 3.0.0 waits for the operator's redefinition, and the
  material task targets classic Landscape through its substrate adapter.
- **A2 [ACTIVE]:** Each concern attributes its deltas against the pre-3.0 tree at its start. The
  final release comparison uses the published 2.0.0 assets (V14).
- **A3 [RESOLVED by D9]:** City17 remains a supported product route; its SM5 defect is fixed by its own
  backlog task before the candidate unless Q3 decides otherwise.
- **A4 [RESOLVED by D10]:** The parked Linux task stays untouched and parked. 3.0.0 claims no Linux support.
  The implemented Linux workspace v2 and manifest v4 route is kept, not required.
- **A5 [ACTIVE]:** The candidate's engine identity is the engine that produced the accepted
  substrate and material evidence. The engine changes only through
  `scripts/ue/update/update_engine.ps1` (operator-owned), and a change invalidates the affected
  evidence.
- **A6 [ACTIVE]:** Promotion means updating site and promotion links after publication, as
  [release compliance](../../docs/legal/release_compliance.md) requires.
- **A7 [SUPERSEDED by D17]:** Per-version platform contracts are unnecessary for
  the current Windows-only product policy.

## Non-goals

- Linux: no Linux build ships until we gain a server (D10); its task stays parked.
- Water presentation unless admitted through the Water gate below.
- itch.io publication, unless separately authorized. The 2.0.0 attempt closed on the account
  allowance, and the publisher remains available.
- Reopening 2.0.0 artifacts or verification.
- A release-system redesign beyond R4 (D3).
- Unrelated backlog. The Manhattan Shipping performance watch item activates only if it reproduces
  in the candidate.

## Release concerns

| Order | Concern | Task | State |
|---|---|---|---|
| 1 | Terrain substrate: switch to Mesh Terrain (D12), proven on a same-Kazan comparison, then cutover | [Mesh Terrain migration](../01_done/world/20260923-1620_world_migrate_terrain_to_mesh_terrain.md) | Done |
| 2 | Generated production terrain material | [Terrain Material v2](../01_done/content/20260923-1620_content_generate_terrain_material_v2.md) | Done; candidate proof per R2 |
| Required | World release projection safety fix | [Projection recovery](../01_done/world/20260914-1215_audit_public_world_release_projection_recovery.md) | Done; frozen unsigned R5 accepted (D8) |
| Required | World generation locality and identity correctness | [Generation locality](../01_done/world/20261002-1711_world_restore_generation_locality.md) | Done; independent R2 PASS before R5 (D8) |
| Integration | Release-platform contract and final candidate | this file, R4-R6 | R4 any time after review; R5-R6 last |
| Optional | Water presentation | [Water presentation](../02_backlog/world/20260825-1717_world_improve_water_presentation.md) | Only through the Water gate |

### Pre-candidate items

- The public World projection safety fix is a required concern (table above, D8).
- City17's SM5 default-material fallback is accepted as legacy; its fix path was dropped (D9).
- Public source build (2026-10-01 development evidence): with City17 projected disabled, a fresh
  isolated projected public checkout built `AlisEditor Development` from scratch with
  `ProjectWorldMeshTerrain` enabled. Mesh Terrain does not block the public build; do not disable
  or investigate it for that. The payload install, no-op reinstall, and Kazan/Manhattan load stay
  in R5 step 4.
- Public character GASP dependency: the public Hero and GrandPa definitions name private
  `/MotionMatching/` assets. Public developers install the Game Animation Sample for the
  release's engine version themselves, as the developer quickstart says; an ALIS-owned character
  module comes later ([decisions D4-D6](../01_done/build/20261002-1144_public_close_source_slice_gaps.md)).
  These reference-only examples are outside the two-map World closure promise.
- Packaged console-variable checks (before R5 step 1, so the frozen Candidate stays fresh). In
  `scripts/ue/world/test/performance/run_kazan_playable_tour.ps1`:
  - `Assert-PlayableTourNormalExit` fails on a `Failed to find console variable` line, which proves
    the packaged Data Driven CVars through the Development child logs.
  - Launcher-engine Shipping writes no log (`Source/Alis.Target.cs`), so the Shipping
    product-route receipt records `FindConsoleVariable` for `a.animnode.offsetrootbone.enable`,
    and `Read-PlayableTourCorrectness` requires it.
  - Both are proven on a development Candidate. If the Candidate moves to the source engine, the
    Shipping log must exist before it is checked.
- Manhattan chunk 11: the Map scan list now includes Manhattan's directory, so the editor
  registers its map and the `ChunkId=11` rule (`Config/DefaultGame.ini:121`) can apply
  ([record](../01_done/build/20261002-1144_public_close_source_slice_gaps.md)). Packages before this
  fix hold no `pakchunk11`; R5 step 5 certifies the chunk.
- Public developer payload dependency closure: the exact public four-layer
  profiles require one native configuration asset omitted from the frozen payload,
  `ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1`. Its focused
  correction passed focused R2 and every planned pre-commit gate, recorded in
  Current state and next steps. The next frozen R5 must certify its assembled
  clean public checkout and map loads. The audit
  stays fail-closed. The historical 45-package report sampled private vegetation,
  gameplay and fixture references; it does not define public payload selection.
  The prediction that synchronous registry discovery would introduce character
  sample dependencies into this World closure was unsupported: the native public
  inventory contains none. Do not turn reference-only object examples into new
  admission roots or broaden this fix to character content.
- [Manhattan Shipping performance watch](../02_backlog/world/20260903-1545_world_investigate_manhattan_shipping_performance.md) - only if it reproduces.
- Later build-blocking findings follow D8: small fixes land in this file, concern-owned fixes in
  their task, and wide fixes in a new current task linked from this table.

### Sequencing

1. Concern 1 Phases 0-3 run first. Concern 2 T0-T2 (census, design, substrate-neutral lifecycle
   changes proven in test roots) may run beside them.
2. Concern 1 switches to Mesh Terrain (D12); a failed invariant starts its workaround loop, not a
   rejection.
3. Concern 1's production cutover and concern 2's production identity migration land in one
   terrain regeneration. A separate material path migration would rewrite every terrain package a
   second time: 428 external-actor packages reference the current terrain material today.
4. R4 is independent of World work and can be implemented once reviewed.
5. The World release projection safety fix (D8) lands before R5 step 4.
6. The World generation-locality task, including its one combined realization of both cities,
   lands before R5 (D8).
7. R5 and R6 run last from one frozen source identity, on engine 5.8.3 (D13).

## Verified evidence

Read at HEAD `81a3b60a4` with a clean tree. Nothing was built, packaged, or executed.

- **V1** The version check `>= 2.1.0` appears six times in four files:
  `scripts/ue/package/release.ps1:168-170` and `prepare_release.ps1:53`
  (`$UseMultiPlatformSchema = [version]$ReleaseVersion -ge [version]"2.1.0"`);
  `release_workspace.py:147-148` (v1 `initialize` refuses `>= (2, 1, 0)`), `:179-180`
  (`initialize-v2` refuses `< (2, 1, 0)`), and `:272-273` (required schema);
  `prepare_release.py:755-756` (manifest v4 for `>= (2, 1, 0)`, else v3).
- **V2** A 3.0.0 release therefore requires Windows and Linux x86-64, workspace v2
  (`game/windows-x86_64/`, `game/linux-x86_64/`, `github/`, `release-workspace.json`), manifest v4
  with matching source identity for both platforms (`prepare_release.py:775-776`), a Linux package
  built from `UE_SOURCE_PATH` (`release.ps1:343-358`), and a Linux acceptance receipt. The receipt
  must come from Ubuntu 22.04 under WSL2 or WSLg with GPU-backed Vulkan
  (`accept_linux_player.py:154-166`, `:322-325`; `prepare_release_v4.py:115-116`, `:144-147`
  rejects `llvmpipe`). The parked Linux task records that this workstation's WSLg exposes only
  `llvmpipe`, so preparation should fail before any workspace exists
  (`docs/build/packaging_guide.md:110-111`).
- **V3** The platform map is closed at two: `release_workspace.py:26`
  `PLATFORM_KEYS = ("windows-x86_64", "linux-x86_64")`, `prepare_release_v4.py:17-18`,
  `prepare_release.py:759-761` ("platform map is incomplete or open-ended"), and
  `sign_release.ps1:15` `ValidateSet`.
- **V4** Later steps already branch on the schema recorded in the workspace, not on the version:
  `sign_release.ps1:422-433`, `publish_itch.ps1:239-243`, `finalize_release.py:143-151`,
  `release.ps1:259`.
- **V5** Tests that encode the threshold directly: `scripts/ue/package/tests/test_release_workspace.py`
  `test_v1_rejects_release_version_that_requires_v2` (`:214-227`), and
  `tests/test_release_entrypoint.ps1:156-159` (expects `*requires workspace schema*`). Tests that
  rely on it through version numbers: `test_release_workspace.py` (v1 at 2.0.99, v2 at 9.8.7),
  `test_prepare_release_v4.py` (five tests at 2.1.0), `test_finalize_release.py`
  (`test_v4_rebind_preserves_each_platform_game_identity`), `test_prepare_release.py` (2.0.0),
  `test_component_manifest_signing.ps1` (v1 at 2.0.0 to 2.0.90, v2 at 9.9.1),
  `test_publish_itch.ps1` (v1 at 1.9.0 and 2.0.0, v2 at 2.1.0). No Makefile target or CI runner is
  known to run `scripts/ue/package/tests` (CI unverified); the README lists manual commands.
- **V6** Docs that state the coupling: `docs/build/packaging_guide.md:88` ("For 2.1.0 and later,
  the same commands own one multi-platform workspace"), `:100-115`; `scripts/ue/package/README.md:48-50`,
  `:106` ("The multi-platform workspace written for 2.1.0 and later is:"), `:129-131` ("The
  requested version selects the required schema generation ..."). The player quick start is
  already Windows-only (`docs/quickstart/player/README.md:3`).
- **V7** Historical exactness depends on: `release_workspace.py` `verify_workspace` v1 branch
  (`:274-277`), `assert_workspace_inventory` (`:107-117`), `verify_release_identity` (`:204-222`),
  `workspace_package_tree_digest` (`:80-104`, which must reproduce the 2.0.0 manifest's
  `package_tree_sha256`), and the v1 branches of `recover_github_projection` (`:314-317`) and
  `adopt_game` (`:333-361`); manifest v3 in `prepare_release.py:750-756`; `sign_release.ps1:428-433`;
  `publish_itch.ps1:240`; `finalize_release.py:148-151`; `refresh_github_release.py`; and the v1
  branches of `release.ps1` (`:217-219`, `:266-268`, `:636-642`).
- **V8** The repository declares no product version: `Config/DefaultGame.ini` has no
  `ProjectVersion`, and there is no VERSION file. The operator types it per run
  (`make release X.Y.Z`, `Makefile:39-47`; `release.ps1:6-8`, `:164` sets `$ReleaseTag`), and it is
  stored only in release outputs.
- **V9** The [parked Linux task](../03_parked/build/20260917-1719_release_add_linux_player_support.md)
  records operator decisions dated 2026-09-17 that bind Linux to 2.1.0 (its D1 and D3), GitHub-only
  distribution (D6), no parallel release authority (D7), and native Ubuntu host acceptance, not WSLg
  (D9). It records no decision for any version after 2.1.0. Its blocker is the unavailable native
  acceptance host.
- **V10** Code and docs disagree on the Linux acceptance route: the code accepts only a WSL2/WSLg
  receipt, while `packaging_guide.md:101-106`, the package README (`:84-86`, `:456-461`), and the
  parked task's D9 require a native host. The generated README states "Linux x86-64 is tested on
  Ubuntu 22.04 under WSL2/WSLg" (`prepare_release_v4.py:176`). This belongs to the parked task; R4
  must not change it by accident.
- **V11** Mirror path defect, verified in code but not executed:
  `scripts/git/mirror/publish_reviewed_release_source.ps1:57-69` looks for
  `tmp/release/vX.Y.Z/release_manifest.json`, while both workspace generations write it under
  `github/` (`release.ps1:178`, `:424`); the mirror test fixture uses the old flat layout
  (`test_publish_reviewed_release_source.ps1:13`, `:101`). A real workspace would likely fail with
  "ReleaseTag requires one pending reviewed release source". R4 owns the fix.
- **V12** World release consumers: `release.ps1:311-336` builds the Kazan playable-tour Candidate
  (`scripts/ue/world/test/performance/run_kazan_playable_tour.ps1`) and requires its machine and
  operator acceptance (`Saved/Validation/WorldRealization/playable-tour/Candidate/operator-acceptance.json`).
  `make release` then runs `prepare_release_inputs.ps1` (`release.ps1:338-339`), which calls
  `Invoke-WithProjectWorldPublicProjection` unless `-PublicAssetRoot` is given
  (`prepare_release_inputs.ps1:86-94`). That projection (`public_world_projection.ps1`) snapshots
  private generated content, realizes the Kazan and Manhattan public profiles with
  `-RequireLandscape`, runs the release action, and restores byte-for-byte; the backlog audit found
  that it holds neither the global content lock nor an interruption journal.
- **V13** Engines: the launcher install is 5.8.1 (CL 56057345) and the source engine
  (`UE_SOURCE_PATH`) is `5.8.1-release`. `package_release.ps1` defaults to the launcher engine
  (`UE_PATH`, `:89-90`); `make package` passes the source engine explicitly, and `release.ps1` uses
  it for the Linux build (`:343-358`), so under D4 the source engine is not a 3.0.0 player input
  unless the Windows Candidate route passes it (confirm at R5). Epic has shipped 5.8.2 (2026-08-25)
  and 5.8.3 (2026-09-22), and states that UE 5.8 is the last planned major UE5 release. Both local
  engines are moving to 5.8.3 (D13): the source engine checkout reached tag `5.8.3-release` on
  2026-09-23, and the operator is updating the launcher.
- **V14** 2.0.0 baseline: the signed release is published at tag `v2.0.0`; the verified game
  projection was 4.752 GiB (the itch publication record, git history before `d7efe85cf`). The
  retained workspace `tmp/release/v2.0.0/` is workspace v1, manifest v3, status `complete`, and is
  disposable (`tmp/`); the published assets are the durable comparison source.
- **V15** File sizes for R4: `release.ps1` 698 lines, `prepare_release.ps1` 242,
  `release_workspace.py` 459, `prepare_release.py` 1030 (above the 1000-line guardrail in
  `AGENTS.md`), `sign_release.ps1` 611. Whether `prepare_release.py` is admitted to
  the mega-file baseline or split is decided in the backlog task
  [Fix stale docs and leftovers](../02_backlog/chore/20260923-1802_chore_fix_stale_docs_and_leftovers.md)
  (its Q1).
- **V16** Release highlights are hard-coded per version: `prepare_release.py:488`
  (`if version == "2.0.0":`) writes the 2.0.0 "WHAT'S NEW" list, and every other version gets "See
  the Git history for changes in this release." (`:506-507`), while the package README says
  "`README.txt` owns the version highlights" (`scripts/ue/package/README.md:133`). A 3.0.0 README
  would ship without highlights. `prepare_release.ps1:19-20` also defaults the version and tag to
  2.0.0. SemVer ordering in `publish_itch.ps1:212` is a legitimate version comparison.
- **V17** The earlier R4 proposal still failed open: a missing Makefile platform variable silently
  became Windows. Its schema-generation proxy could encode only the two existing platform sets and
  left platforms and highlights with different authorities. A new release therefore remained
  reproducible only if the operator remembered an unrecorded invocation choice.
- **V18** GitHub immutable releases lock the release tag and assets after publication and
  automatically create a release attestation containing the tag, commit SHA, and release assets.
  GitHub recommends uploading every asset to a draft before publishing it. After publication,
  `gh release verify <tag>` verifies immutability and `gh release verify-asset <tag> <path>` compares
  a local artifact with the published asset.
  [Immutable releases](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases),
  [release verification](https://docs.github.com/en/code-security/how-tos/secure-your-supply-chain/secure-your-dependencies/verify-release-integrity).
- **V19** Release immutability is a repository/organization setting and applies only to future
  releases. Enabling it is an external GitHub write and remains an operator action, not an R4 code
  action.
  [Preventing release changes](https://docs.github.com/en/code-security/how-tos/secure-your-supply-chain/establish-provenance-and-integrity/prevent-release-changes).

## Release gates

### R0 - review

Each concern passes its R1 and R2 independent reviews
([World pipeline layers](../../docs/testing/world_pipeline_layers.md)). R4 passes an architect review
before implementation.

### R1 - terrain substrate

Concern 1 reaches `PASS` (D12 decided the switch; its workaround loop handles failures).

### R2 - terrain material

Development completion: concern 2 reaches `PASS` in its own task when its generated authority,
owner no-op guarantees, orphan safety, public payload composition and dependency closure, and
independent review pass. That needs no release package; its earlier Development and Shipping
packages are development evidence, not candidate certification.

Release certification: R5 proves the packaged properties on the frozen candidate, with no
separate material package:

- step 2: SM5/SM6 cook, archive/IoStore and package limits, packaged Kazan and Manhattan terrain
  render, frame/memory/hitch for both territories against the
  [World performance contract](../../scripts/ue/world/README.md#packaged-kazan-playable-tour-acceptance)
  (Manhattan has no accepted frame measurement yet), and the Shipping Water proof. The Kazan
  runner now calls Manhattan's existing-package mode on its exact Development package after
  Kazan's three-run aggregate and before Shipping replaces it. Manhattan uses the same package
  and executable bytes, three runs, explicit non-interactive policy, and the shared 16.67 ms
  base p95 aggregator. Focused fixtures pass; real packaged Manhattan acceptance awaits R5;
- step 4: payload composition that includes the v2 material and ProjectTexture authorities;
- step 5: owner-attributed ProjectTexture and ProjectMaterial bytes, plus shader and PSO counts;
- step 6: operator visual acceptance of the terrain material.

### R3 - supported-route blockers

The projection safety implementation is committed, with its frozen-commit proof pending before
R5 step 4 (D8). City17's SM5 fallback is accepted as legacy (D9). No shader error on a
non-legacy route is suppressed. The packaged host allowance follows D16.

### R4 - release-platform contract

See the R4 section below.

### R5 - final candidate (one frozen source identity)

1. Freeze a clean tree at the candidate commit.
2. Run the World L4 release proof for both territories: package, IoStore, packaged render and
   performance, post-audit.
3. Pass the Kazan playable-tour Candidate machine gate, including its console-variable checks
   (Pre-candidate items), and record operator acceptance (V12).
4. With the projection safety fix in place (D8), run
   `make release 3.0.0 RELEASE_SIGN=0 SOURCE_COMMIT=<candidate-commit>`. After that fix it must
   run the public World projection in an isolated checkout of that commit (V12).
   The focused isolation tests pass and the implementation is committed at
   `bc5b3c0926a0583503419a5876a1655510a119f2`. The first unsigned run at
   that commit stopped before input preparation because the Kazan playable-tour
   Candidate child crashed in Unreal's CSV profiler. The project-wide CSV
   category fix was committed at `a61715d31004df95723b5acdd105192c9f6f7516`.
   The second run accepted the player Candidate, then stopped at a stale World
   transaction parameter in the isolated projection. Commit `09b7b6532` includes that fix and
   the D16 host-load cap. Its replacement run accepted the Candidate, then the projection refused
   an omitted pinned runtime profile. The corrected projection and permanent runtime guard are
   committed through `f45a9c01dee51cbc2d868f0236c207579adb1480`. The Manhattan same-package
   harness and evidence envelope passed focused independent R2 at
   `c3c006cf12757bf0040ba7e047d1f9476f138e30`. The first frozen route at
   `f4cce29f3b41840790355c18eb5c5fd72cd8a806` stopped at Manhattan's collision-slide proof
   before Shipping or public projection. That runtime correction was committed and the frozen
   `049e06a42` route re-proved both cities; its assembly failure and current correction are
   recorded in Current state and next steps. Earlier aggregates do not certify a later candidate.
   Do not run the projection separately; that would repeat an
   expensive gate.
   Review the unsigned `tmp/release/v3.0.0/` `game/` and `github/` projections, including the 3.0.0
   highlights in `README.txt` (V16).
5. Record IoStore inventory (`scripts/ue/package/inspect_iostore.ps1`) per owner and for the whole
   game, compared with 2.0.0 (D11). Manhattan's packages must sit in `pakchunk11`.
6. Operator walkthrough of Kazan and Manhattan through menu, ProjectLoading, and PreviewFlight.
   City17 ships as the legacy demo it is (D9).

### R6 - publication (operator-gated at every step)

1. With V11 fixed under R4, `make mirror RTAG=v3.0.0`.
2. `make release 3.0.0` - approve, sign, verify.
3. Upload every asset to the GitHub draft by hand. Confirm release immutability is enabled before
   publication and compare the draft asset digests (`packaging_guide.md`, Recommended Public
   Release Flow).
4. Publish, then run `gh release verify v3.0.0` and `gh release verify-asset v3.0.0 <local-path>`
   for every uploaded asset (V18). Any mismatch stops promotion.
5. Promote only after verification (A6). itch.io remains separately authorized.

Nothing is published, mirrored, signed for release, or promoted without the operator's approval of
that exact step.

## Optional Water gate

Water joins 3.0.0 only when all of these hold:

- R1 and R2 are accepted and no required 3.0.0 blocker remains;
- the Water investigation finds a bounded change that reuses canonical Water authority and
  ProjectMaterial, with no second geometry or material lifecycle;
- it can pass its complete acceptance envelope without weakening the terrain evidence.

Otherwise Water stays the next World release concern.

## R4 - release-platform contract

### Current decision and boundary

D17 narrows this slice to one global platform policy in the existing
`release_platforms.py` owner. Windows is enabled now. Add Linux globally only
after its separate acceptance is qualified; no new Linux work is authorized.
No version number selects a platform or schema. No per-version registry,
contract schema, highlights loader, prerequisite extraction or new dependency.

New workspaces use the policy through the existing public CLI. Existing
workspaces resume and verify their recorded schema, so a later global policy
change cannot reinterpret historical artifacts. v1/v3 and the existing closed
v2/v4 formats retain their identity, inventory, recovery and signing guards.
The archived two-platform fixtures remain tests of the existing format, not
Linux release qualification.

### Tasks and verification

- [X] Reproduce the Windows-only 3.0.0 initialization failure before the fix.
- [X] Remove the unused per-version draft files and directories.
- [X] Add global policy to the existing owner and consume it in both coordinators.
- [X] Remove all six SemVer-to-platform checks, retaining source-freeze and
      publication-order comparisons.
- [X] Verify 3.0.0 initialization, adoption, unsigned resume and identity checks.
- [X] Verify unsupported-schema refusal leaves the workspace unchanged.
- [X] Keep existing Windows and closed two-platform fixture validation green.
- [X] Update the existing package README and release guide.
- [X] Verify the real Windows-only 3.0.0 preparation wrapper with existing fixtures.
- [X] Independent R2 of the minimal diff.
- [X] Capture the platform correction's release/World plan and satisfy its required
      unexecuted pre-freeze gates. Reserve the reported L4 requirement for frozen R5.
- [X] Restore concrete 3.0.0 Player/Developer highlights in the existing generator.
- [X] Require explicit direct preparation version and derive its tag; verify
      omission refuses before output creation or Candidate adoption.
- [X] Satisfy the fresh exact Check/Matrix/verifies plan for these assembly edits.
- [X] Operator committed; one frozen unsigned R5 accepted at `a4167f43b91403d69da0ad011aa85d319c1453e6`.

The focused RED command was the exact
`ReleaseWorkspaceTests.test_windows_3_0_workspace_does_not_require_linux`
case, which failed on the erroneous v4 manifest requirement. Focused green
coverage is recorded in the current state above. Release notes and publication
remain with their existing owners; this platform fix adds no second owner.

## Documentation plan

- **Release operations:** `packaging_guide.md` and `scripts/ue/package/README.md`
  route to the global platform owner and describe recorded-format verification,
  Windows-only scope and immutable-release verification.
- **World and material owners:** each concern task's own documentation plan.
- **Release history:** after publication this file shrinks to the outcome record, following the
  2.0.0 precedent, and still-current facts live in their stable owners.
- Stable docs, code, tests, and configuration never link to this file.

## Reviewer brief

For the review of R4 before implementation, and of the R5 and R6 plan before the candidate. Review
with the `architect` skill and the review contract. D1-D14 bind: a finding that contradicts one names
it and argues with evidence. Judge against ALIS goals and standards: [VISION.md](../../VISION.md)
(signed, verifiable releases), the [Package and Release Guide](../../docs/build/packaging_guide.md),
the [package scripts README](../../scripts/ue/package/README.md),
[release compliance](../../docs/legal/release_compliance.md), the
[component license policy](../../docs/legal/component_license_policy.md),
[canonical.md](../../docs/agents/canonical.md) section 8.6 (the public mirror is text-only), and
`AGENTS.md` (commit, push, and publication boundaries).

Investigate carefully, on the web wherever the fact is external, and cite primary sources:

- **Release policy (R4, D17):** verify one global owner, no SemVer-derived
  platform selection, Windows-only creation and unchanged recorded-format
  validation. Reject speculative scaffolding or a prerequisite refactor.
- **GitHub publication:** current GitHub guidance on immutable releases, asset digests, and artifact
  attestations for the manual upload step; verify `gh release verify` and `verify-asset` are
  sufficient after the operator publishes without widening agent push authority.
- **Linux (D10):** confirm R4 does not weaken the parked Linux route; the native-host acceptance gap
  (V10) stays with its owner.
- **Size (D11):** how to present the 2.0.0 comparison when the published assets are the only durable
  baseline.
- **Better paths to weigh, not assume:** any smaller R4 shape.

Return `PASS`, `PATCH`, `BLOCKER`, or `UNVERIFIED`; state each finding as evidence, consequence,
and smallest fix, with sources linked.

## Completion criteria

- Concerns 1 and 2 reached `PASS` (D12).
- R4 is implemented and verified, and the projection safety fix is done (D8).
- The R5 candidate passed every step from one frozen source identity, with exact size and
  performance evidence against 2.0.0.
- The operator approved the candidate, and publication and promotion completed through R6.
- Durable facts live in their stable owners, and this file is reduced to the outcome record.

## Review record

### 2026-09-23 - initial investigation

- **Trigger:** the operator asked for three current tasks for ALIS 3.0.0 - this router, the Mesh
  Terrain migration, and Terrain Material v2 - rechecked against the live repository.
- **Root cause:** no 3.0.0 router existed, and release code derives Linux from the version number.
- **Fix:** this router and R4.
- **Verification:** read-only census of the release scripts, tests, docs, and the parked Linux
  task; no command changed state.
- **Authority:** D1-D6 recorded; Q1-Q5 opened; A1-A6 active.

### 2026-09-23 - independent architect review (PATCH)

- **Trigger:** the operator asked for `/architect` review; a fresh non-author reviewer returned
  `PATCH` and confirmed V1, V3, V4, V5, V9, V12, and V14.
- **Root cause:** the first draft left the platform record and its entrypoint unspecified, missed
  the version-keyed highlights branch, ran the public World projection twice in R5, and called an
  already-true refusal a red test.
- **Fix:** the schema generation is the platform record with a Makefile entrypoint (R4 Decision);
  invariant 3 is narrowed to selection policy; highlights, the stale 2.0.0 default, and the verified
  mirror path defect join R4 (V11, V16); R5 runs the projection once through `make release`; the
  Linux refusal is a preservation guard with a sabotage control; V13 now states which engine packages
  what. The reviewer's claims were rechecked in the scripts before being applied.
- **Naming:** this file follows the release-router precedent (`00_release_1.0.0.md`,
  `00_release_2.0.0.md`), which `todo/README.md` does not document; the other new-task names follow
  its `YYYYMMDD-HHMM_topic_verb_noun.md` rule. Documenting the exception is owned by the backlog task
  [Fix stale docs and leftovers](../02_backlog/chore/20260923-1802_chore_fix_stale_docs_and_leftovers.md).
- **Authority:** no register change.

### 2026-09-23 - operator direction and reviewer brief

- **Trigger:** "even experimental it future long going goal so we will wrap around it our
  architecture after test fully without any legacy support and mentions", and "ensure that all
  finding are in todos and if some has questions or maybe better pathes - propose reviewer to
  investigate it carefully via web and our goals and standarts".
- **Fix:** Q1's status notes D7; R4 serves both generations' highlights from release
  data; V15 names the owner of the `prepare_release.py` baseline decision; `## Reviewer brief` added.
- **Authority:** D7 added.

### 2026-09-23 - operator answers to the open gates

- **Trigger:** the operator's plain-words answers to Q1-Q5 and the naming question.
- **Fix:** the World release projection safety fix moved to `todo/00_current/` as a required concern
  (D8); the City17 SM5 fix path was dropped and its task file removed (D9); sequencing, R1, R3, R5, the reviewer brief, and the
  completion criteria follow the answers. The source engine checkout moved to `5.8.3-release`.
- **Authority:** D8-D14 added; Q1 closed by D12, Q2 by D8, Q3 by D9, Q4 by D10, Q5 by D11; A1
  resolved by D12, A3 by D9, A4 by D10.

### 2026-09-23 - investigation closure after R1

- **Trigger:** the operator requested a complete, review-ready picture before implementation.
- **Finding:** the Makefile-input proposal still failed open and was not durable release data
  (V17). GitHub now provides immutable-release attestations and exact local-asset verification
  commands (V18-V19). The public World projection also mutates live private content and is now
  separately planned as an isolated frozen-commit worktree.
- **Fix:** R4 now uses one closed tracked contract for platform set and highlights, with no default;
  R5 supplies the exact candidate commit to the isolated projection; R6 verifies the immutable
  release and every uploaded asset before promotion.
- **Verification:** repository census plus primary GitHub, Git, Git LFS, and Windows documentation;
  no build, package, code change, external write, index change, commit, or publication.
- **Authority:** A7 added. D1-D14 unchanged. This authoring pass is not the required independent R1.

### 2026-09-25 - same-repository MeshDevelopment execution

- **Trigger:** the operator accepted the first-class adapter as normal ALIS code and removed
  detached comparison worktrees from daily Mesh development.
- **Finding:** the ordinary packaged MeshDevelopment product route passes, but the performance tour
  deterministically meets an unclassified collision surface about 4.7-4.9 m above its start. The
  current package contains both production Landscape maps and the development Mesh map, so its
  total size is not a terrain-representation comparison.
- **Fix:** the release sequence now routes first to collision provenance and the remaining concern
  1 Phase 3 gates. Production profiles and Phase 4 remain untouched. The public projection's
  isolated frozen-commit worktree remains with its separate final-release reproducibility owner.
- **Verification:** see concern 1 for the accepted package/product receipt, rejected diagnostic
  performance runs, exact open gates, and the removed failed forward/up workaround.
- **Authority:** D1-D15 unchanged. No release, publication, commit, or production-profile change.

### 2026-09-30 - concern 1 done, concern 2 final delta R2

- **Trigger:** concern 2's final delta R2.
- **Finding:** this router still described concern 1 as PATCH and linked its task at its old path.
  Concern 2's packages predate its final source and generated authority. The public developer
  payload contract still declares the deleted V1 material authority, so R5 step 4 cannot compose
  the payload; per D8 that fix belongs to concern 2.
- **Fix:** status, current state, step table, concern table, and R2's candidate proof list.
- **Authority:** D1-D15 unchanged. No release, publication, or commit.

### 2026-10-01 - concern 2 done

- **Trigger:** concern 2 closed after T11 and the black-box independence sweep passed their
  independent review.
- **Fix:** status, current state, step table, and concern table; the payload closure item now names
  45 owned rejections and the audit exporter's registry-gather defect as its first fix.
- **Authority:** D1-D15 unchanged. No release, publication, or commit.

### 2026-10-02 - public source build slice

- **Trigger:** review of the City17 public projection and Motion Matching console-variable change.
- **Finding:** R3 called the projection safety fix done, while the concern table and the projection
  task call it pending; the code agrees with the task (no isolated worktree, and
  `public_world_projection.ps1:135` still removes live-tree paths).
- **Fix:** R3 now says pending; Pre-candidate items record the public source build evidence; R5
  step 3 names the packaged missing-console-variable check.
- **Authority:** D1-D15 unchanged. No release, publication, or commit.

### 2026-10-02 - public source slice final patch

- **Trigger:** reviewer final PATCH on the public source slice, applied where valid at the
  operator's request.
- **Finding:** R5 step 3 allowed an accepted gap for a Shipping registration the product needs; R5
  step 4 described the isolated projection as current behavior; the public character's private
  GASP dependency had no owner before R5 step 4.
- **Fix:** the console-variable checks, including the mandatory non-log Shipping proof, became a
  Pre-candidate item, and R5 step 3 only runs them. R5 step 4 states the isolated projection as
  required after the safety fix. Pre-candidate items link the public character task and
  Manhattan chunk 11, and R5 step 5 certifies `pakchunk11`.
- **Authority:** D1-D15 unchanged. No release, publication, or commit.

### 2026-10-02 - World generation locality linked

- **Trigger:** the operator required the World generation-locality task to be fully done before
  release.
- **Change:** one Required row in Release concerns and Sequencing step 6, under D8 (wide fixes
  become a linked current task).
- **Authority:** D1-D15 unchanged.

### 2026-10-04 - frozen release route blocked by Candidate CSV crash

- **Attempt:** one unsigned `make release 3.0.0` run from clean commit
  `bc5b3c0926a0583503419a5876a1655510a119f2`. The Development Candidate
  built, cooked, staged, and passed archive verification. Its first packaged
  Kazan playable-tour child accepted the product route, then exited 3 on an
  Unreal `CSVProfiler` assertion at `CsvProfiler.cpp:1623`. Evidence:
  `Saved/Validation/WorldRealization/playable-tour/`
  `fb5fe2ad46124e1c8e74474108447dec/development/run-01/game.log`.
- **Focused repeat:** the matching `Saved/StagedBuilds/Windows/` Development
  files run with the same product-route/performance flags exited 3 on the same
  assertion. The product-route receipt accepted; the CSV performance receipt
  was not produced. Log: `tmp/world/playable_tour/csv_repro_20261004/game.log`.
- **Boundary:** Candidate acceptance failed before release input preparation;
  Kazan/Manhattan public projection, developer build, and map loads were not run.
  The script restored the prior Candidate. HEAD, tracked status, staged entries,
  and registered worktrees remained unchanged; no 3.0.0 input tree was promoted.
  Keep R5 step 3 and the projection concern open. At this point the malformed
  CSV stat producer was unidentified. The repeat's minidump showed the
  `FName` stat raw ID `0x48999b23002b9801`, which exceeds the engine's 51-bit
  CSV stat mask; the stack alone did not establish whether ALIS or Unreal
  supplied it. The next entry records the producer and fix.
- **Authority:** D1-D15 unchanged. No signing, publication, or remote write.

### 2026-10-05 - CSV producer identified and focused package fixed

- **Diagnosis:** live debugging of the exact packaged Development child traced
  the failing CSV stat to UE 5.8.3's `NavTasks` category and a numbered Recast
  navigation actor name. The stable [World pitfall](../../Plugins/World/ProjectWorld/docs/pitfalls.md)
  owns the root cause and config contract.
- **Fix and focused proof:** `Config/DefaultEngine.ini` disables the three UE
  navigation categories that emit dynamic actor-named CSV stats. A fresh
  Development Kazan package built, cooked, staged, and passed IoStore checks;
  its packaged product and performance route exited 0, both receipts accepted,
  and rich CSV plus exact frame samples were produced. Evidence:
  `tmp/world/playable_tour/csv_config_fix_20261005/`. Independent boundary and
  fix review returned PASS. The native performance budgets were unchanged.
- **Next at this point:** commit the CSV fix, then run R5 step 3 and the
  unsigned 3.0.0 release route from the new clean source commit. This focused
  run did not close the three-child Candidate, public projection, or P4 proof.

### 2026-10-05 - player Candidate accepted; frozen projection integration stopped

- **Attempt:** from clean commit `a61715d31004df95723b5acdd105192c9f6f7516`,
  `make release 3.0.0 RELEASE_SIGN=0 SOURCE_COMMIT=<that-commit>` accepted the
  Kazan playable-tour Candidate: three Development runs, Shipping, and water
  stability. The accepted composite receipt is at
  `Saved/Validation/WorldRealization/playable-tour/`
  `ecf67532e77d47c284e9eaeac66b41a3/composite.json`.
- **First failing boundary:** the detached release checkout reached the public
  World projection, then `Invoke-WithProjectWorldPublicProjection` failed when
  its `Get-ProjectWorldGeneratedPaths` call passed removed parameter
  `IncludePresentation`. The isolated checkout and failed work root were cleaned;
  no `tmp/release/inputs/v3.0.0` or `tmp/release/v3.0.0` was promoted. The live
  tracked tree and staged entries remained unchanged during the failed route.
- **Focused correction:** the public projection wrapper no longer passes the
  obsolete argument to generated-content enumeration or removal. Its test now
  checks both calls against the live helper signatures: observed RED before the
  correction, GREEN after. The generated-content transaction's nine tests and
  the release-entrypoint test pass. The exact frozen route still requires a new
  operator commit; do not infer public payload or map-load acceptance from
  these focused checks.

### 2026-10-05 - busy-host performance policy and local Candidate

- **Frozen-route failure:** the unsigned route from clean `735c9726e50629ab5c3bc49d7a2c329731aa156f`
  stopped at the three-run Kazan Development aggregate before public projection. Its pooled
  Frame p95 was 17.507 ms against the fixed 16.67 ms target. The three children were
  16.112, 17.306, and 18.805 ms; the prior accepted Candidate used the same executable
  hash. Evidence: `Saved/Validation/WorldRealization/playable-tour/`
  `53855523e17c4d4b8da7f07d81966cc7/development/performance-aggregate.json`.
  No release input tree was promoted.
- **Decision and implementation:** D16 replaces the idle-host wait with six game-stopped
  load windows around the three packaged runs. The [World runner](../../scripts/ue/world/README.md)
  owns the bounded calculation. A focused aggregate test was RED before the change and all
  10 cases passed after it, including partial load, maximum allowance, invalid evidence,
  and slow-frame refusal.
- **Local product proof:** the uncommitted source state produced an accepted Kazan Candidate
  with three Development runs, Shipping, and Water. Its pooled Frame p95 was 16.632 ms;
  the lowest host window was 19.7%, so the effective threshold remained 16.67 ms in this
  live run. The accepted composite is `Saved/Validation/WorldRealization/playable-tour/`
  `dcf81c63b1704052b97791cbff4d695a/composite.json`. This proves the new measurement
  path and unadjusted decision on a real package; the adjusted branch is proven by the
  focused regression tests, not by this particular product run.
- **Remaining:** commit this focused change, then rerun the unsigned 3.0.0 route from that
  clean source commit. The public projection and combined release remain unverified.

### 2026-10-05 - frozen Candidate accepted; public projection profile gap

- **Attempt:** clean commit `09b7b6532c15947fd894106a2d26eb91ab02175b` first failed the
  Kazan Development aggregate at pooled Frame p95 19.499 ms versus its adjusted 17.337 ms cap.
  The same staged build passed one exact packaged control at 16.499 ms, and one replacement
  `make release 3.0.0 RELEASE_SIGN=0 SOURCE_COMMIT=<that-commit>` accepted the full Candidate.
  Its pooled Development Frame p95 was 16.424 ms, below the unadjusted 16.67 ms target;
  Shipping and water also passed. Composite receipt:
  `Saved/Validation/WorldRealization/playable-tour/60611dbfe041400680a041b53c6e7a84/composite.json`.
- **Projection failure:** the detached checkout reached public Kazan realization, which refused
  the missing `-RuntimeProfile`: its realization profile pins
  `kazan_territory_512_1536_v1`, but the wrapper passed `none`. No public input tree or combined
  release workspace was promoted. The realizer's refusal and the release rollback are correct.
- **Focused correction:** the package projection derives each city runtime file from its pinned
  public realization profile, validates the file identity before snapshot mutation, and forwards
  it to the realizer. The projection regression was RED before the change, then GREEN. The release
  entrypoint test and nine generated-content transaction tests pass. The next frozen route needs
  an operator commit; public payload, map loads, and combined release remain unverified.

### 2026-10-05 - precommit package boundary diagnosis

- **Real public projection:** an isolated overlay of the uncommitted package fix at source commit
  `09b7b6532c15947fd894106a2d26eb91ab02175b` accepted both Kazan and Manhattan with engine
  exit 0 and restored private generated content. Receipts and logs are under
  `tmp/release/diagnostics/precommit_projection_50f5240c09c64f568b573aa9aa218ce2/`.
  The earlier engine exit 1 was caused by 5,002 Git LFS pointer packages scanned by Unreal;
  preparation now verifies the committed `Content/` and `Plugins/` LFS payloads and builds the
  isolated Editor before realization. That diagnostic used runtime `none` in both public profiles;
  it is not evidence for the intended pinned-runtime map contract.
- **Mirror boundary:** the expanded precommit run accepted both cities but WSL Git could not read
  a Windows linked-worktree `.git` pointer. The Windows wrapper now hands its verified Git
  directory to the Bash source operations and developer composer without rewriting Git metadata.
  A synthetic linked-worktree regression passes, and a real source-only official dry mirror
  passed at `tmp/release/diagnostics/mirror_linked_probe_e1e91c3b68424e769ef4bc7f6c876aa0/`.
- **Payload boundary:** the next expanded run accepted both cities and the public source mirror,
  then rejected an untracked generated Manhattan package in the disposable checkout. The
  composer now admits only explicit public-manifest-projected World packages after SHA-256
  verification when `--allow-dirty` is used; every other untracked payload remains rejected.
  The focused regression was red before the fix, green after, and all 19 developer-payload tests
  pass. The failing candidate and reports remain under
  `tmp/release/diagnostics/precommit_projection_4fb2cc05d3c94cb3b07837e85ef0c3f4/`.
- **Verification discipline:** repeated full precommit worktrees were unnecessarily costly.
  Package and general troubleshooting now route to the first failing owner and focused check.
  Do not replay two World generations for another composer or mirror diagnosis. The final
  combined unsigned release, installed developer checkout, and public map loads remain unverified;
  they require the operator's next commit and one frozen `make release 3.0.0 RELEASE_SIGN=0` run.
- **Focused brick closure:** a real composer fixture archived an untracked map selected by a
  projected manifest, then rejected a changed digest and an unrelated untracked map. Its linked
  worktree variant invoked the real composer CLI inside WSL with the source Git handoff and passed.
  The 20-test developer-payload suite passed before that WSL extension; the revised exact fixture
  passed afterward, and the other 19 tests were unchanged. Manifest staging and dependency
  audit passed 9 tests; public source selection passed 20. Public projection, isolated-source
  recovery, release entrypoint, and Windows/WSL handoff focused PowerShell checks passed.
  Release-workspace assembly passed 26 focused tests. Changed
  PowerShell files parse, Bash syntax checks, public realization JSON parses, and `git diff --check`
  pass. The public Editor build and real two-map load require assembled release inputs and remain
  the final frozen-commit proof. No package build was run before review.
- **Independent precommit R2 (superseded):** PASS on that diff and focused evidence. The reviewer
  examined the linked-worktree WSL composer fixture, runtime forwarding, LFS/Editor preparation,
  release ownership, and test limits; no remaining concrete defect was found. The assembled public
  developer Editor build, installation in that checkout, and both map loads remain final-only
  checks. An external reviewer subsequently found that this review had accepted the invalid
  `none` premise; the corrected runtime contract needs its own proof and re-review before commit.

### 2026-10-05 - public runtime contract correction

- The external reviewer found that commit `76ed07763` changed both public runtime IDs to `none`.
  That allowed map-load proof to pass without the pinned partition, route, and spawn. The local
  correction restores the original Kazan and Manhattan IDs.
- The first real pinned Kazan projection rejected at runtime acceptance because the reduced
  public profile omits vegetation while the C++ territory policy required an instance
  unconditionally. The runtime owner now conditions that probe on the selected layer or a
  present vegetation actor. Full product validation still requires a true probe. A corrected
  isolated overlay at `76ed07763` accepted both cities with their pinned runtime identities,
  partition, route, and manifest enrollment, then restored the private tree byte-for-byte.
  Evidence is under `tmp/release/diagnostics/pinned_runtime_34e9a71f17fd4d7f85cebc29858835b6/`.
  Independent corrected R2 returned PASS for the precommit scope. The overlay is precommit
  evidence; clean public checkout, map loads, packaged player, and R5 still require the eventual
  clean committed source.
