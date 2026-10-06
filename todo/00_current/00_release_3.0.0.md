# ALIS 3.0.0 Release Plan

**Status:** CURRENT RELEASE FOCUS - routing fixes committed; planner composition verified with independent R2 PASS; operator commit precedes one unsigned R5
**Active concern:** none; concerns 1 and 2 are done ([Terrain Material v2](../01_done/content/20260923-1620_content_generate_terrain_material_v2.md))  
**Scope:** release-level scope, sequencing, and gates for 3.0.0, plus the release-platform contract change in `scripts/ue/package/` (R4)  
**Stable documentation owner:** [Package and Release Guide](../../docs/build/packaging_guide.md) and [package scripts README](../../scripts/ue/package/README.md) for release operations; each concern task names its own owners

This file is the single current product-focus and release router for 3.0.0, aligned with
[VISION.md](../../VISION.md). Each remaining concern phase and R4 in this file follows its own
authorization and gates. Durable facts move to their stable owners before this file closes.
Reviewers start with `## Reviewer brief`.

## Current state and next steps

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
and `plan.json`. Planning reports unexecuted Matrix/verify requirements; no real
PIE, Matrix, generation, build, package or R5 ran for this correction.
Stop for the operator commit, then perform the single frozen unsigned R5.
Packaged acceptance remains unverified. Do not retry refused scratch cleanup.

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
`ac9f8c998acf5d8bdcaa8945afcf604b49dcdb8a`; the next commit boundary is the
planner composition correction above. The full packaged release is still unverified.

Automatic approval refused manual cleanup of four earlier test Git fixtures in
ignored `tmp/release/preflight-tests/` (reason: blocked by policy). They remain
disposable diagnostics. New routing fixtures clean their recorded repositories
after containment checks; that cleanup passed. Source and index were preserved.

For any necessary routing correction, run only routing,
schema/consumer fixtures, parse and governance hygiene; no real PIE, Matrix,
regeneration, cook or R5 for this slice.

### Uncooked admission slice

Implementation, runtime proof and independent R2 are closed and committed at
`e41f774445b6e526b737d83825d7474ef67f197a`. The remaining commit boundary is
the routing correction above.
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
World. The payload can be composed again, but its dependency closure still rejects World content
(see Pre-candidate items). Earlier concern-2 packages are development evidence only; R2 below names
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
in `pakchunk11`. Independent focused R2 passed with no concrete finding. The next operator commit
must include the routing correction and its handoff;
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
  - Effect in this file: the required platform set becomes explicit release data (R4). Both
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
- **A7 [ACTIVE]:** A release contract is reviewed source data for a future release, not a global
  product-version file. It records only facts needed to reproduce that release preparation.

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
| Required | World release projection safety fix | [Projection recovery](20260914-1215_audit_public_world_release_projection_recovery.md) | Corrected two-city overlay PASS; frozen release pending (D8) |
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
  The payload-closure item below owns declaring this dependency for the audit.
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
- Public developer payload dependency closure (blocks R5 step 4): the audit approves all 3,103
  selected packages but rejects 45 dependencies of current World content that no payload
  authority selects; none is reachable from a generated surface or pattern. Each owner either
  selects its packages with verified authority and redistribution rights or removes the
  reference (D8); the audit stays fail-closed:
  - ProjectObject (22): AmurCork and Hornbeam tree meshes with their layer instances and
    textures (Manhattan vegetation layer); the water-bottle family (Kazan gameplay layer).
  - ProjectMaterial (15): the object material-layer system (`MaterialLayer/*`, 11 packages),
    `Function/MF_TextureColor`, `MF_WorldAlignedTexture`, `MF_WorldUvTiling`, and
    `Base/M_ObjectUv_Compact`, used by those objects.
  - ProjectTexture (5): `Base/T_*` textures used by those layers.
  - ProjectElement (1): `HumanMade/Paint/MLI_Paint_Mate_Pure_Grey`.
  - ProjectWorldMeshTerrain (1): `Terrain/MPD_ProjectTerrain_Shared_v1`, referenced by the
    terrain layer actors.
  - ProjectWorldData (1): the fixture marker map `Authored/Fixtures/L_ProjectWorldMarker`.
  First fix: `scripts/ue/check/assets/export_public_dependency_inventory.py` scans only the seed
  files and never gathers the registry, so the audit reported 18 of these as missing and traversed
  none of their dependencies (it counted 42); a read-only full-registry run gives 45 with none
  missing.
  External dependency: once the registry is gathered, Hero's and GrandPa's `/MotionMatching/`
  references will be rejected. `classify_dependency` (`scripts/git/mirror/audit_developer_payload.py:172`)
  knows only approved payload, public source, local engine, and rejected. This item therefore
  owns three changes:
  - a machine-readable external-dependency declaration in `developer_asset_release.json`: the
    `/MotionMatching` mount, Epic's Game Animation Sample from Fab, and a version matching the
    release's engine;
  - generic audit handling of that declaration, with no mount name in code;
  - an audit that still rejects any undeclared mount, and a payload that carries no GASP bytes.
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
   before Shipping or public projection. The focused runtime correction changes the game executable,
   so the next frozen route must rebuild the package and re-prove both cities from the operator's
   new full commit SHA. Do not reuse the earlier Kazan aggregate as candidate acceptance.
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

### Problem

The required platform set is a hidden policy derived from semantic version (V1). Any version at or
above 2.1.0 silently requires Linux, so 3.0.0 cannot be prepared on this workstation (V2) and
contradicts D4.

### Decision

Each releasable version has one tracked, schema-validated contract under
`scripts/ue/package/releases/<version>.json`. It contains exactly:

- contract/schema version;
- matching semantic release version;
- a closed ordered platform set using the existing platform keys;
- the ordered plain-text highlights written to `README.txt`.

There is no platform default. `make release <version>` resolves the matching tracked contract and
fails before creating or resuming a workspace if the file is absent, invalid, names another version,
contains an unknown/duplicate/open platform set, or disagrees with a recorded workspace. The 3.0.0
contract names only `windows-x86_64`; the 2.0.0 contract reproduces its existing highlights and
Windows-only bytes. A future Linux release must add or review its own contract after the parked
Linux acceptance task is complete.

The platform set selects the existing generation: Windows only means workspace v1/manifest v3;
exactly Windows plus Linux means workspace v2/manifest v4. The workspace schema remains the derived
on-disk record used by later signing/finalization tools, but is no longer the policy authority.
`verify_workspace` checks the recorded generation against the release contract rather than deriving
one from SemVer. No version comparison selects platforms or schema.

Both generators read highlights through one small release-contract loader. The 2.0.0 text stays
byte-identical, `prepare_release.ps1` loses its stale 2.0.0 default, and the v4 generator stops
owning a different hard-coded "WHAT'S NEW" block. Its WSL claim remains owned by the parked Linux
task (V10). The mirror path defect (V11) is fixed with a red test built on the real `github/` layout.

### Premise / KISS gate

- **Owner:** the release workspace and manifest tools (`release.ps1`, `prepare_release.ps1`,
  `release_workspace.py`, `prepare_release.py`) already own release preparation. The tracked
  contract becomes their one per-release input; workspace schema remains a derived runtime record.
- **Added:** one tiny closed contract schema and one data file per supported release. Platforms and
  highlights need one recorded owner already, so this is not a second authority.
- **Removed:** the six version comparisons and the tests that assert them, the hard-coded 2.0.0
  highlights branch, and the stale 2.0.0 default.
- **Capability given up:** implicit release preparation from an unrecorded default or a version
  threshold. A reviewed contract is required before any release workspace is created.

### Alternatives considered

- **Raise the threshold** (for example to 4.0.0) - rejected: still semver-derived hidden policy
  (D5).
- **Workspace v2 with a Windows-only platform map** - rejected as larger: it opens the closed
  two-platform map (V3) and touches the parked Linux machinery.
- **Makefile platform variable plus schema-generation proxy** - rejected after investigation (V17):
  a default fails open, the invocation is not durable release data, and highlights still need a
  separate per-version owner.
- **Add platform fields to historical workspace schemas** - rejected: it changes v1/v3 contracts
  and historical bytes when the existing schema generation can remain a derived record.

### Invariants

1. The 2.0.0 workspace verifies exactly, including the reproduced `package_tree_sha256` (V7) and
   the byte-identical 2.0.0 highlights.
2. The v1 and v3 functions in V7 keep their behaviour for historical input.
3. No version-derived platform/schema selection or default platform remains in release code (grep
   proof). SemVer ordering of workspaces (`publish_itch.ps1:212`) is not selection policy and stays.
4. The Linux route stays admissible when explicitly requested and its acceptance evidence exists.
   V10 is not changed here.
5. `make release 3.0.0 RELEASE_SIGN=0` needs no Linux input, toolchain, or receipt.
6. Refusal leaves no workspace: a missing/invalid/mismatched contract, Linux without acceptance, or
   resume against a different platform set fails before mutation.
7. Signing, publishing, finalizing, and verification keep branching on the recorded schema (V4).
8. `prepare_release.py` does not grow past the guardrail. A larger change moves into a sibling
   module (V15).

### Tasks

- [ ] First, behavior-free (architect verdict SPLIT, recorded in the backlog task
      [Fix stale docs and leftovers](../02_backlog/chore/20260923-1802_chore_fix_stale_docs_and_leftovers.md)):
      move the manifest contract (`verify_artifacts`, `verify_release_manifest`, `approve_release`,
      `prepare_release.py:709-864`) into `release_manifest.py`, and update its five importers (two
      import it as `legacy`) in one pass with no re-export shim. The closed release-contract parser
      and highlights loader live in a small sibling module imported by both generations.
- [ ] Red: add closed-schema tests for absent contract, version mismatch, unknown/duplicate/open
      platforms, and unknown fields. Each refusal must leave no workspace.
- [ ] Red: add a test that the tracked 3.0.0 contract selects workspace v1 and manifest v3 and needs
      no Linux input. Add a 2.0.0 golden-byte highlights check.
- [ ] Red: add a refusal test for contract/platform mismatch on resume that proves no workspace is
      changed. It fails today because no release contract exists.
- [ ] Preservation guard: Linux without acceptance evidence already leaves no workspace
      (`prepare_release.ps1:114-121`, cleanup in `:230-233`). Add the test with a known-bad
      control: sabotage the cleanup and watch the test fail before trusting its pass.
- [ ] Red: a mirror test on the real `github/` workspace layout fails on V11 today.
- [ ] Red: a 3.0.0 preparation must produce 3.0.0 highlights from release data; it gets the Git
      history placeholder today (V16).
- [ ] Add the contract schema plus 2.0.0 and 3.0.0 data. Replace the six comparisons (V1) with
      contract selection, move both generators' highlights to the shared loader, drop the 2.0.0
      default, and fix the mirror path.
- [ ] Rewrite the two threshold tests (V5) as platform-set tests; keep every historical v1 and v3
      test green.
- [ ] Update the statements in V6 to describe release-contract selection.
- [ ] Architect review of the diff.

### Verification

- The Python tests under `scripts/ue/package/tests/` (`test_release_workspace.py`,
  `test_prepare_release.py`, `test_prepare_release_v4.py`, `test_finalize_release.py`,
  `test_release_platforms.py`, `test_accept_linux_player.py` for invariant 4) and the script tests
  `test_release_entrypoint.ps1`, `test_component_manifest_signing.ps1`, and `test_publish_itch.ps1`,
  using the commands in the package README; the mirror test
  `scripts/git/mirror/tests/test_publish_reviewed_release_source.ps1` for V11.
- A dry preparation of an unsigned 3.0.0 workspace from a known package, proving invariant 5.
- Verification of the retained 2.0.0 workspace, proving invariant 1. `tmp/release/v2.0.0/` is
  disposable: run this before any `tmp/` cleanup, or verify against the published 2.0.0 assets.

## Documentation plan

- **Release operations:** `packaging_guide.md` and `scripts/ue/package/README.md` describe
  the required per-release contract, closed platform selection, and immutable-release verification
  instead of version thresholds (V6, V18).
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

- **Release contract (R4):** challenge the selected tracked contract as the one owner for platforms
  and highlights; verify workspace schema remains only a derived record and that Linux can resume
  without changing historical schemas.
- **Release highlights as data:** the smallest format and owner that keeps the 2.0.0 bytes
  identical and serves both generations.
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
