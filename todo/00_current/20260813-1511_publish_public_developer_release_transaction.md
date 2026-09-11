# Publish Public Developer Release Transaction

Status: READY - R7 PASS; freeze tracked source before final Candidate
Priority: Release 2.0.0 critical
Created: 2026-08-13 15:11 Europe/Moscow
Active gate: P7 - final source freeze and replacement Candidate

Freeze rule: this R7 correction is the planned last tracked edit inside
`v2.0.0`. After freeze, exact hashes, receipts, and PASS results live only in
ignored/generated owner evidence and `release_manifest.json`. A concrete defect
may reopen tracked source, but then the affected final identity gates restart;
bookkeeping alone may not do so.

## Goal

Publish one coherent ALIS 2.0.0 release with:

1. the accepted packaged game archive for players; and
2. an exact text-only public source tag plus one matching signed developer
   payload containing approved Unreal binary projections.

The developer route stays simple:

```text
clone exact v2.0.0 tag
-> download matching developer payload
-> run checkout-local installer
-> configure local launcher UE 5.8
-> build and open generated Kazan and Manhattan
```

No second installer, package manager, asset database, public project fork, or
regenerate-everything-on-first-open workflow.

## Authority register

- **D1 [version superseded by D10]:** use this existing release-publication
  task; do not create a parallel release todo.
- **D2:** binaries that cannot live in public Git use the developer payload.
- **D3:** restricted proprietary ProjectObject and other third-party source
  assets do not ship.
- **D4:** ship accepted ALIS-generated projections when their complete
  provenance permits distribution. Generation by ALIS is not, by itself, a
  license for embedded or derived third-party bytes.
- **D5:** keep installation KISS through the existing trusted checkout-local
  installer; raw unzip is not a second authority.
- **D6:** consume the accepted Shipping Candidate; rebuild only if the release
  transaction changes evidence-bound source or required bytes.
- **D7:** make the root README a concise role router for players, developers,
  World builders, architecture reviewers, contributors, and legal/security.
- **D8:** public Git is text-only. Every approved `.uasset` and `.umap`, small
  or large, uses the signed developer payload.
- **D9:** authoritative public-safe JSON, schemas, code, docs, config, and tools
  live in Git. The payload contains generated Unreal binaries, not duplicate
  source JSON.
- **D10:** release identity is `2.0.0` / `v2.0.0`.
- **D11:** include public-safe source by default, but reject operator identity,
  personal contact/path, machine-local path, credential, secret, or private
  endpoint leakage. Legitimate project, place, and character names are not KYC.
- **D12:** active public World authority contains no HLOD outputs or
  participation.
- **D13:** final developer acceptance uses an exact local filtered commit/tag,
  a fresh clone, the matching signed payload, and the real installer/build/boot
  route. The private checkout cannot substitute.
- **D14:** public developer acceptance opens generated Kazan Territory and
  Manhattan Showcase. Old City 17 and its proprietary dependencies are out of
  scope.
- **D15:** this materially reopened legacy task keeps its original Git creation
  timestamp in its filename. Do not bulk-rename history.
- **D16:** remove unsupported "architecture reference" self-ranking from the
  public front door. Name only verifiable patterns and route to their owners.
- **D17:** a third-party plugin or asset may be redistributed only when its
  exact terms allow it. Free availability or adding a copied license/notice
  does not grant redistribution rights. Engine-bundled Epic dependencies are
  obtained with Unreal rather than republished; legacy Marketplace-derived
  packages stay excluded until replaced or regenerated from rights-cleared
  inputs with complete provenance.
- **D18:** cinematic engineering is closed for this release. The approved
  twelve-shot raw library is sufficient for the external human montage; the
  montage does not gate packaging. Durable cinematic truth stays in stable
  cinematic owners, and the ALIS skill is routing only.
- **D19:** the root `LICENSE` owns first-party component assignments. Do not
  scatter redundant first-party `NOTICE` files through ordinary tool and
  automation roots. Retain local notices only for genuine boundary exceptions,
  third-party/original terms, or an attribution obligation of distributed
  material; release-specific generated notice bundles remain evidence.
- **D20:** ALIS currently has one GitHub maintainer. Official publication must
  not depend on a ceremonial second collaborator. Keep the protected-PR and
  required-`verify` rules, but configure the approval count for the single-owner
  repository to zero before the release PR. Untrusted users may propose a PR;
  only an authorized repository account may merge or publish it. Official
  release mode validates `fallintodusk/alis` as its target; the reusable mirror
  utility may still create a dry run or user-owned fork without conferring ALIS
  publication authority.
- **D21:** one release-suite owner must prepare and verify the complete 2.0.0
  transaction. Extend the existing package/release scripts at their shared
  boundary rather than teaching one narrow payload composer about the player
  archive or teaching the signer domain semantics.
- **D22:** release privacy is a fail-closed machine check for forbidden private
  paths, identities, contacts, endpoints, credentials, secrets, and metadata.
  Do not create a KYC, contributor-identity, or personal evidence ledger. Emit
  one non-personal exact-release result that binds the existing owner reports
  and their unresolved counts.
- **D23:** the operator performs the one final signature only after the release
  suite reports the exact directory ready. Public developers never sign ALIS
  artifacts; they verify the published fingerprint, signature, hashes, tag,
  and payload identity through the existing installer/verifier.
- **D24:** prepare one short, human-reviewable `PRODUCT_TERMS.txt` candidate for
  the packaged game because it contains private game content governed by the
  root license's Product-terms boundary. It grants install/play permission for
  the packaged release and preserves the separate licenses of public/upstream
  components. Installation instructions link it but do not replace it.
- **D25:** the operator accepted the exact Shipping Kazan Candidate represented
  by the owner-local acceptance receipt after the final product walkthrough.
  That PASS authorizes release work against those proven product behaviors; it
  does not approve Product terms, a production signature, or remote writes.
- **D26:** production signing is a consumer trust gate, not a reason to rebuild
  unchanged source or payload bytes. Final developer proof may compose the
  accepted pre-sign build/boot with a fresh consumer-side signature/install
  replay only when the final merged Git tree and installed payload entry hashes
  are exactly identical; any byte difference requires the affected real route
  to run again.
- **F1 [verified]:** `prepare_release.py source-state` and its focused World /
  ProjectCinematic parity regression prove that tracked working-tree byte
  changes move the release source-state digest while Git revision remains a
  separate identity. Therefore this todo edit requires a final source-bound
  replacement transaction; the old accepted package cannot be relabeled.
- **Q1 [OPEN - BLOCKING at final composition]:** operator approval of the exact
  `PRODUCT_TERMS.txt` bytes. The earlier Shipping walkthrough PASS did not
  answer this legal/product-terms gate.
- **Q2 [OPEN - BLOCKING at remote boundary]:** explicit authorization for the
  named GitHub ruleset, branch, PR/merge, tag, release, and asset writes after
  the local release suite is ready.
- **Q3 [OPEN - BLOCKING at signing boundary]:** the operator's one production
  signing action after the actual merged source identity and exact ready
  release directory are known. No agent or public developer signs for ALIS.

## Stable owners

This todo owns execution order, open blockers, and evidence only.

- [Mirror and developer payload](../../scripts/git/mirror/README.md)
- [Developer asset admission](../../scripts/git/mirror/developer_asset_release.json)
- [Component license policy](../../docs/legal/component_license_policy.md)
- [World data and asset policy](../../docs/legal/world_data_and_asset_policy.md)
- [Release compliance](../../docs/legal/release_compliance.md)
- [Package and signing workflow](../../scripts/ue/package/README.md)
- [World generation and regeneration](../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [World tools](../../tools/World/README.md)
- [Release campaign router](00_release_2.0.0.md)

Use the existing legal classifications and notices. Do not reopen general
license research during implementation; an exact asset with missing or
ambiguous provenance simply blocks admission until its owner resolves it.

## Verified implementation state

### Existing working owners

- `compose_developer_payload.py` already builds a root-relative payload,
  manifest, signatures, and installer copies.
- `install_developer_payload.ps1` already verifies revision/tag, inventory,
  paths, and conflicts before copying.
- ProjectWorldData active manifests and canonical bundles already feed the
  composer; the active set contains zero HLOD artifacts.
- ProjectObject already has one generated-definition admission contract.
- The accepted Shipping game Candidate remains separate release evidence.

### Baseline defects closed

1. The filtered source admits the required public `Config`, `scripts/config`,
   and `tools/World` projections without admitting their private counterparts.
2. Machine-local UE paths, crypto/private configuration, and credentials remain
   excluded; public setup writes only ignored local configuration.
3. Binary admission covers the required ProjectObject, ProjectWorldData,
   ProjectExperienceData, ProjectMaterial, authored map, boot, and menu packages.
4. The release envelope consumes the different owner-manifest shapes without
   replacing their semantic authority.
5. The real filtered project has been installed twice, built, cold-booted, and
   opened from an isolated exact-tag checkout.
6. The six missing ProjectObject JSON/generated pairs were refreshed through
   their owner and admitted without weakening completeness.
7. The public projection disables excluded `InstanceArrayTool`; the promised
   Kazan/Manhattan route is proven absent-safe.
8. The root README and World routing now provide the public role entry points.
9. One release-suite owner prepares and verifies the combined player,
   developer, privacy, dependency, terms, and attribution transaction.
10. Remote draft creation, upload, read-back, and final download verification
    remain intentionally unexecuted until explicit remote-write authorization.

## Hard invariants

1. One version binds source tag, game archive, developer payload, signatures,
   manifests, and release metadata.
2. Public Git contains no Unreal binary, restricted byte, secret, private
   endpoint, personal identity/path/contact, or machine-local configuration.
3. Every payload file is owner-selected, hash-bound, provenance-classified,
   path-contained, and rights-approved.
4. Every active accepted generated output is included or blocks the candidate
   with an exact reason. Historical/test/scratch outputs are not release input.
5. Every non-Engine package required by the promised Kazan/Manhattan route has
   one disposition: source, approved payload, or rejection. A separately
   obtained reference may describe optional functionality only; it cannot be a
   dependency of either acceptance root.
6. `references_only` never smuggles bytes or turns a missing required runtime
   dependency into success.
7. Owner plugins retain semantic, generation, and provenance authority. The
   release layer owns transport and cross-owner completeness only.
8. The existing installer refuses dirty/wrong revision, bad signature, unsafe
   path, and conflict before protected-tree mutation.
9. Active public World, both promised maps, dependency closure, and installed
   payload have zero HLOD participation.
10. Privacy checks extend the existing mirror validator with exact path,
    metadata, and marker controls. No generic PII service, human-name detector,
    or report that echoes sensitive values.
11. Exact clean-clone acceptance is mandatory; private-checkout success is not
    a substitute.
12. The root README routes to stable truth and does not duplicate it or rate
    ALIS's architectural quality.
13. Remote writes require explicit operator authorization, start as a draft,
    and preserve the prior public release until read-back succeeds.
14. Release readiness has one non-personal machine-readable manifest. It
    consumes and hashes owner reports; it does not replace their semantics or
    store contributor identities.
15. Signing is the last local mutation of the ready release directory. The
    operator signs once; public consumers only verify.

## Implementation sequence

### Current evidence and remaining gates (2026-09-11)

This is the review checkpoint, not the final immutable release identity. The
exact values below are retained so the reviewer can audit what was actually
proven before this tracked todo update.

#### Accepted player checkpoint

- Operator acceptance:
  `Saved/Validation/WorldRealization/playable-tour/Candidate/operator-acceptance.json`
  reports `operator_accepted` for private revision
  `cbeb4b46de83446090983febfaba0ef50077e77b`, source state
  `a346cba04eaae187cd51a7532ffe3909f39ea9b481732c3e78cf5dab62b1293c`,
  operation `kazan_playable_tour_089dfef2d71a469295959dae3555d7e3`,
  package tree
  `7608d8f11d9cc6daa5011920220750f64391d7886b80e5e57872f9f13e2743e8`,
  and Shipping executable
  `a975612d3ec768601ee8f1649f18879beb27a2b7f52edd81fcc756ead885b19f`.
- The same operation's Development aggregate accepted 48,860 frames at Frame
  p95 `13.366799801588058 ms`, GPU p95 `9.409099817276001 ms`, and zero
  streaming failures. The three child runs also passed the complete product
  route and package-identity envelope.
- Shipping Water proof accepted canonical feature
  `alis:osm:relation:7493502` in cell `grid_413718bc833994e5:x1:y1`, with pawn
  XY error `19251.128578627195 cm`. Its exact two-frame diagnostic reported
  2,018,266 blue pixels, zero classification flips, and zero mean blue-channel
  drift. The normal FinalColor product screenshot visibly showed the geographic
  blue Water plane.
- Package identity is one shared contract across World, ProjectCinematic,
  player archive, and release composition: relative POSIX paths, ordinal UTF-8
  ordering, and exclusion of declared runtime-written state only. The focused
  cross-owner regression includes the `pakchunk1`/`pakchunk10` ordering control.

#### Optional historical Track V evidence

- `Saved/CinematicRelease/Kazan/Current/receipt.json` reports
  `technically_accepted` for operation
  `project_cinematic_release_capture_3398be4f13924c518c8fb648441d9b40`.
  It binds the same private revision, source state, release operation, Shipping
  executable, and operator-acceptance receipt.
- Retained outputs are `Saved/CinematicRelease/Kazan/Current/KazanRelease_v1.mov`,
  `preview.png`, and `receipt.json`. The master hash is
  `cdca6e30aa152a737f6eb8c7d138d0cdab511470558ca321b40ac10092c9d473`.
  Start/middle/end inspection showed dense city, visible blue Water, and a
  ground-scale end without a visible streaming hole. This is release-binding
  evidence, not a replacement for the approved trailer raw-footage library.
  Per D18 it is retained as optional historical evidence: distribution does not
  rerender, rebind, or consume Track V.

#### Public developer and combined-release checkpoint

- The final successful public dry run is `tmp/release/public-final3-20260911`:
  local branch `release/v2.0.0-source`, local tag `v2.0.0`, commit
  `5ae78ac12ee91ccd31ab3a6e0c8996797d2a3d69`, tree
  `260298fd1477121fefa3c734eb64d5ac3dd1dd5f`, clean status, no remote writes.
  `tmp/release/public-final3-privacy-20260911.json` reports `accepted`, dry run,
  634 projected files, and zero privacy findings.
- The matching unsigned developer payload is
  `tmp/release/developer-final3-20260911/ALIS_DeveloperProject_2.0.0_edc1cca63fee.zip`
  with manifest
  `ALIS_DeveloperProject_2.0.0_edc1cca63fee.developer-payload.json`. It contains
  2,272 verified files, is 137,797,859 bytes, and has payload identity
  `edc1cca63fee40279c28898dc29168184c6a1c70dae71fc247962af03b7c56eb`.
- `tmp/release/pff-final2-20260911` is the isolated exact-tag checkout. The
  checkout-local installer succeeded, its second run was a no-op, ignored local
  configuration selected launcher UE 5.8.1, Git status remained clean, and the
  Editor build completed all 259 actions. No missing plugin/module blocked the
  build; `InstanceArrayTool` remained disabled. Kazan and Manhattan public map
  acceptance remains valid for this checkpoint because the refreshed public
  delta did not change C++, descriptors, build/config/schema, or asset inputs.
- `tmp/release/inputs-final2-20260911/developer-dependency-report.json` reports
  `accepted`, 2,182 closure packages, zero issues, and zero HLOD. Public World
  authority authenticated 11 scopes and 2,178 artifacts.
- `tmp/release/v2.0.0-final-20260911/release_manifest.json` verifies 19 local
  artifacts with zero unresolved items. Its status is correctly
  `pending_owner_approval`; it is not signed or publishable yet.
- Final focused checks passed: 33 mirror Python tests, 13 package Python tests,
  eight package-identity/cinematic Pester tests, release-manifest verification,
  the clean-clone build, and the dependency audit. Independent architecture and
  developer-experience audits both returned PASS for this bounded checkpoint.

#### Why one final refresh is mandatory

- The release source-state contract deliberately includes tracked working-tree
  bytes. This requested todo correction changes those bytes, so the accepted
  `a346...` state and everything bound to it remain prior product evidence but
  cannot be promoted as the final 2.0.0 identity.
- Do not write the replacement source-state digest into this tracked todo: that
  would change the value being authenticated. The final ignored owner receipts
  and `release_manifest.json` own exact replacement identities.

#### Remaining gates, in order

1. [x] R7 confirms this evidence summary, the final-refresh rule, and
   the closeout sequence. No architecture expansion is requested.
2. [ ] Freeze the reviewed tracked source. Re-run focused release checks, build
   the replacement Development/Shipping Candidate, run the complete machine
   gate, and stop for the one exact Shipping operator walkthrough.
3. [ ] On operator PASS, write the replacement owner-local acceptance receipt.
   Do not copy final identities into tracked files or relabel the old receipt.
4. [ ] Recreate the public source candidate and developer payload from that
   frozen state; repeat the exact-tag install/no-op and the smallest real
   build/boot/map/dependency envelope required by D13.
5. [ ] Operator approves the exact short `PRODUCT_TERMS.txt`. Current candidate
   hash `33e46c4a8f0f1135c15f4bf18351a832ddad56c98c3cfbb9fa7682a443697a60`
   is evidence for review, not approval.
6. [ ] After explicit remote-write authorization, configure the single-owner
   ruleset per D20, publish the validated source branch through a protected PR,
   and bind the final tag/payload to the actual merged `main` commit.
7. [ ] Recompose and verify the exact ready directory, then stop for the
   operator's single production signature. Run consumer-side signature,
   fingerprint, tag, hash, installer, and payload verification afterward.
   Apply D26: do not rebuild byte-identical inputs solely because they gained a
   signature.
8. [ ] Create the GitHub draft release, upload and read back every asset, verify
   fresh remote game and developer downloads, then publish without removing the
   prior recoverable release.
9. [ ] Remove only the verified stale scratch roots listed under P8, verify the
   retained evidence set, propagate final routing, and archive this todo.

### Remote and legal investigation (2026-09-11)

- The public mirror owns privacy before upload. It excludes secret/private
  paths, sanitizes known user and machine paths, projects public-only config,
  rejects forbidden text/metadata, binary files, Unreal assets, key material,
  and LFS pointers, and applies a private denylist only from the private source
  operation. The exact local candidate passed this boundary.
- Live GitHub `main` is protected by active ruleset `protection for main`:
  pull request required, one approval required, review threads resolved,
  non-fast-forward and deletion rejected, and required status `verify`.
  The active `Rights affirmation` workflow verifies the four canonical PR-body
  declarations. It does not inspect privacy, payload bytes, Product terms, or
  release assets.
- The ruleset protects only the default branch. It does not protect release
  tags or authenticate uploaded assets. Signed hashes and read-back remain the
  release-asset trust boundary.
- `mirror_to_github.sh --push` targets `main` directly and therefore conflicts
  with the active pull-request rule. Final source publication must push only an
  already-validated public candidate branch, merge it through the protected PR,
  then bind the tag and payload to the actual merged `main` commit.
- The repository's configured hooks path is `.githooks`, whose active
  `pre-push` currently runs Git LFS only. The autonomous-branch refusal shown
  in `docs/ci/push_protection.md` exists only in inactive `.git/hooks` state.
  GitHub `main` protection remains effective, but the local documentation and
  hook claim must not be used as release evidence.
- The generated developer notice already carries exact Kazan and Manhattan
  OpenStreetMap/Copernicus attribution and source/alteration-offer metadata.
  Do not create a second world-attribution database; publish and hash this
  owner-derived notice in the final release set.
- Root contribution terms plus the protected PR rights affirmation govern
  source-submission authority. Per D22, a personal/KYC ledger is rejected as
  duplicate authority and unnecessary private data. The release suite instead
  emits a non-personal result binding the exact commit, component manifest,
  dependency report, package inventories, privacy result, unresolved counts,
  and approving role.
- The player archive itself does not embed Product terms. The combined release
  set carries the exact approved `PRODUCT_TERMS.txt` beside the archive and
  links it from the signed install guide. This keeps the accepted package bytes
  immutable while making install/play terms part of the authenticated download.
- The stale-player issue was closed for the pre-review checkpoint by the exact
  Candidate and archive receipts named above. Because this todo update moves
  tracked source state, the replacement source-bound Candidate must repeat the
  same machine and operator acceptance before final release composition.
- Public mirror tooling can be run by anyone because it is open-source code,
  but it contains no publication credential. A random user may fork or propose
  a PR; they cannot merge to protected `main`, create the official release, or
  upload official assets without repository authorization.
- The single-owner repository makes the current one-approval rule
  operationally unsatisfiable. The KISS correction is approval count zero while
  retaining PR review flow, required `verify`, resolved threads, and
  non-fast-forward/deletion protection. Do not add a collaborator merely to
  manufacture an approval, and do not create an owner bypass that skips
  `verify`.
- The active `.githooks/pre-push` owns Git LFS invocation only. Preserve it.
  The autonomous-push refusal described by `docs/ci/push_protection.md` exists
  only as inactive local `.git/hooks` residue and is not a security boundary.
  Clean that stale claim during durable-doc propagation; retain GitHub ruleset
  protection and agent authorization policy as the real controls.

No architecture or identity question remains open. The remaining work is the
ordered closeout above: final source-bound replacement evidence, exact Product
terms approval, one production signature, and explicitly authorized protected
GitHub publication/read-back.

### P0 - Freeze identity, roots, and owner inventory

- [x] Release version/tag: `2.0.0` / `v2.0.0`.
- [x] Stable creation timestamp applied to this reopened todo.
- [x] Cinematic workflow closed; distribution selected by the release router.
- [x] Freeze public roots:
  - Kazan: `/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory`
  - Manhattan: `/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase`
- [x] Record Old City 17 as excluded from public acceptance.
- [x] Census every owner with active accepted generated output. The admitted
  set is ProjectWorldData, ProjectObject, ProjectExperienceData, and
  ProjectMaterial; each remains selected through its own authority.
- [x] Record the exact machine- and operator-accepted pre-review game Candidate
  identity, package tree, Shipping executable, source state, operation, and
  composite receipt. The tracked todo update requires the P7 replacement bind.
- [x] Refresh the six missing ProjectObject pairs through the existing owner
  workflow; do not invent hashes or weaken completeness.

### P1 - Prove package dependency and rights closure

- [x] Select active outputs from owner manifests, then use Unreal Asset
  Registry/package dependency data on those outputs and both public roots.
- [x] Produce one machine-readable report mapping every non-Engine dependency
  to owner, authority, provenance, distribution class, disposition, and reason.
  A separate-acquisition disposition is valid only outside both promised roots.
- [x] Consume existing license/provenance owners; do not infer rights from file
  names, folders, Git LFS, "free" price, or generator ownership.
- [x] Prove zero active HLOD packages, references, proxies, companions, and
  eligible generated actors.
- [x] Fail closed on missing, unknown, restricted, ambiguous, or unowned
  required dependencies.
- [x] Known-bad controls: remove one required package and introduce one
  restricted embedded dependency; both must reject before payload mutation.

### P2 - Generalize approved binary admission

- [x] Extend only the transport envelope needed to consume owner-local manifest
  shapes; do not centralize owner semantics.
- [x] Keep ProjectObject JSON in source Git and generated packages in payload;
  preserve source/artifact authentication and `references_only` behavior.
- [x] Enroll required ProjectExperienceData projections and all accepted
  ProjectMaterial outputs using their own authorities.
- [x] Include every active ProjectWorldData generated package and its required
  canonical bundles/notices.
- [x] Individually classify authored first-party packages found by closure.
- [x] Exclude all restricted third-party plugin/content bytes. Include an Epic
  or Marketplace-derived package only when the existing legal owner records an
  express redistribution grant for that exact component.
- [x] Preserve one binary transport: signed developer payload only.

### P3 - Make filtered source self-hosting

- [x] Replace coarse excludes with the smallest public-safe source policy that
  admits required JSON, schemas, code, docs, config projections, scripts, and
  `tools/World` while retaining explicit private/restricted exclusions.
- [x] Reuse and minimally extend mirror forbidden-content validation across
  normalized paths, text, project/plugin descriptors, release metadata,
  archive entries, notices, and relevant Unreal metadata.
- [x] Add known-bad operator path, identity, endpoint, credential, and metadata
  controls plus accepted city/character-name controls.
- [x] Keep `ue_path.conf`, `*.local.conf`, `DefaultCrypto.ini`, backups,
  credentials, deployment details, and private infrastructure out.
- [x] Make public UE setup write machine values only to ignored local state.
- [x] Disable excluded `InstanceArrayTool` in the public project projection and
  prove both promised roots build, boot, and open without it. If either root
  requires it, fail the candidate. Separate acquisition may be documented only
  for optional functionality outside public acceptance.
- [x] Verify every command named by public quick starts exists in the filtered
  candidate.

### P4 - Preserve one trusted install route

- [x] Keep `install_developer_payload.bat` / `.ps1` as the only installer.
- [x] Preserve tag/revision, signature, archive path, clean-tree, conflict, and
  authenticated-inventory refusal behavior.
- [x] If remote limits require parts, split transport deterministically while
  retaining one logical signed payload and one install command.

### P5 - Prove the real developer experience

- [x] Compose exact filtered source and matching payload under
  `tmp/release/developer/<operation-id>/`.
- [x] Materialize the source candidate as an exact local `v2.0.0` commit/tag,
  then clone that exact object into a second isolated `tmp/` checkout.
- [x] Install the exact pre-sign payload only through the trusted checkout-local
  installer; do not use raw extraction or the private working tree. The final
  consumer replay remains gated on the operator's production signature.
- [x] Configure the launcher-installed UE 5.8 through the documented ignored
  local override; the tag must contain no operator path.
- [x] Generate project files if required and build `AlisEditor Win64
  Development` through repository wrappers, never a source-engine build.
- [x] Cold-boot the actual filtered `Alis.uproject` unattended.
- [x] Open both generated Kazan Territory and Manhattan Showcase in the same
  clean Editor envelope; Old City 17 is not acceptance.
- [x] Run applicable schema/JSON, owner-manifest, canonical/generation,
  package-reference, and missing-asset validation.
- [x] Run one production-shaped World validation/no-op and one existing
  synthetic regeneration test from the clean checkout.
- [x] Run the normal agent setup in the clean checkout, prove it recreates the
  ignored `.agents/skills` projection, run
  `scripts/agents/link_codex_skills.ps1 -Verify`, and prove Claude and Codex
  discover the same tracked project skill set.
- [x] Prove both roots have no forbidden or unexplained missing dependency and
  zero HLOD participation.
- [x] Recompose semantically identically and prove installer no-op behavior.
- [x] Record source/tag, payload, engine, commands, logs, and pre-sign receipt
  identity in the current evidence block and owner reports.
- [ ] After the operator signs, record and consumer-verify the production
  signature/fingerprint. Prove the final merged Git tree and installed payload
  entry hashes equal the built pre-sign inputs; rebuild only if they differ.

### P6 - Clean the public front door after P5

- [x] Make root `README.md` one concise role router: Project, Player,
  Developer, World Builder, Architecture Reviewer, Contributor, Legal/Security.
- [x] State prototype/blockout status and verifiable patterns only; remove the
  "architecture reference" and overlapping repository/start-here prose.
- [x] Add the missing `Plugins/World/README.md` shallow category router.
- [x] Keep security and support routing in the existing GitHub advisory,
  legal, build, and troubleshooting owners; no duplicate root `SECURITY.md` or
  `SUPPORT.md` is required.
- [x] Validate every root role link and quick-start command against the filtered
  candidate.
- [x] Reconcile remaining current todos: archive completed work, park unrelated
  work, and dissolve any newly discovered duplicate authority.
- [x] Remove root `testResults.xml` after its consumer census proved it is
  residue. Do not cosmetically delete owner-generated
  `Alis_sanitized.uproject`.

### P7 - Stage one recoverable publication transaction

- [x] Add one KISS release-suite prepare/verify owner under `scripts/ue/package`
  that consumes the existing source projection, accepted fresh player
  Candidate, developer payload manifest, effective component manifest,
  dependency report, privacy result, generated attribution notice, and approved
  Product terms.
- [x] Emit one non-personal `release_manifest.json` binding exact source
  revision/tag, every artifact/report hash, unresolved count, and readiness
  state. Fail closed on a missing input, identity mismatch, nonzero unresolved
  count, privacy failure, stale player acceptance, or unsigned-ready directory
  mutation.
- [x] Prepare a concise `PRODUCT_TERMS.txt` candidate for operator/legal review.
  Include the approved bytes beside the immutable player archive in the signed
  release set and final release manifest; link them from install guidance
  without duplicating their terms.
- [x] Compose and verify the pre-review local 19-artifact bundle under
  `tmp/release/v2.0.0-final-20260911`; retain its
  `pending_owner_approval` state as evidence, not final authority.
- [ ] Freeze tracked source and create the replacement accepted game Candidate,
  operator receipt, and player archive. Track V is not a distribution input.
- [ ] Obtain explicit operator approval of the exact `PRODUCT_TERMS.txt` bytes.
- [ ] Recreate the public candidate, developer payload, privacy/dependency
  reports, and exact-tag clean checkout from the frozen state. Repeat the
  install/no-op and required pre-sign build/boot/map validation.
- [x] In official-release mode, fail unless the remote repository identity is
  `fallintodusk/alis`. Do not prevent local dry runs or user-owned forks.
- [x] HARD STOP for explicit operator authorization before remote tag, release,
  or asset writes.
- [ ] Under that authorization, set the single-owner approval count to zero
  without weakening required `verify`, publish only the validated release
  branch, and merge it through the protected PR route.
- [ ] Bind the final local source tag, developer payload, notices, reports, and
  ready release directory to the actual merged `main` commit. Re-run release
  verification and require zero unresolved items.
- [ ] After the suite reports ready, stop for the operator's one production
  signature. Then run the existing verifier and prove the public fingerprint,
  signature, hashes, tag, and payload identity from consumer-side commands.
- [ ] Create a draft release, upload exact assets, and read back every name,
  size, and hash.
- [ ] Publish only after local and remote validation; preserve the prior release
  until the new source and both downloads are independently usable.
- [ ] Verify the remote game download and a fresh remote source/payload install.

### P8 - Propagate durable truth and close

- [x] Update mirror/install, package, legal/compliance, owner, and release
  routers only where implementation changed standing truth.
- [ ] Remove only these verified stale scratch roots after resolving the local
  recursive-delete tool refusal through an approved safe path; do not bypass
  the refusal through another shell or hidden helper:
  - `tmp/release/player-final-20260911`
  - `tmp/release/player-final2-20260911`
  - `tmp/release/public-final-20260911`
  - `tmp/release/public-final2-20260911`
  - `tmp/release/developer-final-20260911`
  - `tmp/release/developer-final2-20260911`
  - `tmp/release/inputs-final-20260911`
  - `tmp/pff`
  - `tmp/cinematic/release-track-v/frame-audit`
- [ ] Preserve the latest review inputs until the final transaction supersedes
  them: `public-final3-20260911`, `developer-final3-20260911`,
  `pff-final2-20260911`, `inputs-final2-20260911`, and
  `v2.0.0-final-20260911`; also preserve the current/previous packaged
  Candidates. `Saved/CinematicRelease/Kazan/Current` may remain under its own
  historical evidence policy but is not a distribution dependency.
- [ ] After successful remote read-back, remove superseded local final inputs
  only through the same bounded cleanup owner; retain the release evidence
  required by stable package/cinematic policy.
- [ ] After successful publication, record the published state in
  `00_release_2.0.0.md` and archive this todo as post-v2.0 housekeeping. Those
  tracked edits occur after the immutable `v2.0.0` tag and do not trigger a
  refresh of that released tag; no second execution todo or duplicate release
  summary.

## Verification contract

### Required red evidence

- [x] Baseline filtered source lacked a required UE resolver/bootstrap file;
  the real clean-clone build replaced the synthetic assumption.
- [x] Baseline payload omitted required ProjectExperienceData and ProjectMaterial
  projections.
- [x] Baseline synthetic installer fixture could not prove build/boot/map open;
  the exact-tag UE 5.8 checkout now owns that evidence.
- [x] Baseline ProjectObject public authority rejected six unrecorded active
  source/generated pairs.
- [x] Baseline public descriptor enabled excluded `InstanceArrayTool`; the
  projection now disables it.
- [x] Accepted public World authority selects zero HLOD artifacts.
- [x] Removed required package and restricted embedded dependency controls both
  reject before candidate promotion.

### Focused automated evidence

- [x] `python -m unittest scripts/git/mirror/tests/test_developer_payload.py`
- [x] Mirror include/exclude and forbidden-content controls cover every newly
  exposed path class without rejecting legitimate domain names.
- [x] Every owner manifest validates under its own schema and authenticates its
  source, output, provenance, and distribution class.
- [x] Dependency closure rejects missing, unknown, restricted, unowned, and
  HLOD inputs.
- [x] Installer negative controls prove no protected-tree mutation.
- [x] Publication failure tests and official-remote guards use local/disposable
  controls and dry runs; they never write GitHub.
- [x] Documentation/link checks run against the filtered candidate.

### Real acceptance

- [x] Exact filtered tag and payload contain all documented public entry points
  and no forbidden/private identity or restricted byte.
- [ ] Fresh final-tag checkout verifies and installs the signed payload through
  the consumer route. Its Git tree and payload entry hashes exactly equal the
  pre-sign checkout that built and cold-booted with launcher UE 5.8, or the
  affected build/boot/map gates are rerun.
- [x] Generated Kazan and Manhattan open and their bounded generation/
  validation entry points run without missing restricted dependencies.
- [x] Owner outputs, dependency closure, maps, and installed payload contain
  zero HLOD participation.
- [x] Every reachable non-Engine dependency and every active accepted owner
  output is accounted for.
- [x] Local game archive matches the machine-accepted Candidate package tree;
  the exact pre-review Candidate launched through Development and Shipping
  product routes and received operator PASS.
- [ ] The replacement source-bound Candidate repeats the complete machine gate,
  receives exact Shipping operator PASS, and emits its owner-local receipt.
- [ ] The final signed exact-tag checkout repeats the trusted install/no-op and
  signature/fingerprint verification without private inputs. It reuses the
  pre-sign build/boot/map evidence only under D26's exact identity proof.
- [ ] Draft and final remote inventories match signed local hashes.
- [x] Each README role reaches a valid first action in the filtered candidate.

## Rollback and publication boundary

- Local candidates live only under their exact `tmp/` operation directory.
- Composition, closure, install, build, boot, and clone failures do not mutate
  the public mirror or prior release.
- Installer refusal leaves the protected checkout unchanged.
- Remote work starts only after explicit operator authorization and remains a
  draft until read-back validation succeeds.
- Do not reuse an immutable public version/tag after any published byte changes.

## Completion

- [ ] P0-P8 pass with machine-readable evidence.
- [x] Public Git is text-only and self-hosting for the promised developer route.
- [ ] Signed payload contains every approved required Unreal binary and no
  restricted source bytes.
- [x] Real clean-clone Kazan/Manhattan acceptance passes before final signing.
- [ ] Exact Product terms are operator-approved and hash-bound.
- [ ] Final player acceptance binds the reviewed frozen source state.
- [x] Public front door and stable owners reflect final behavior without todo
  dependencies or duplicate truth.
- [ ] Operator authorizes remote mutation; remote read-back and downloads pass.
- [ ] Prior release remains recoverable through final verification.
- [ ] Final architecture review returns PASS; archive this todo.

## Review record

- **R0 - 2026-09-08:** two-artifact architecture accepted; incomplete public
  bootstrap, owner admission, closure, and real clean-clone proof found.
- **R1 - 2026-09-08:** operator fixed 2.0.0 identity, text-only Git, one signed
  binary payload, generated-owner expansion, and trusted installer direction.
- **R2 - 2026-09-08:** operator fixed privacy scope, zero-HLOD public World,
  Kazan/Manhattan acceptance, and exact fresh-clone proof.
- **R3 - 2026-09-09:** stable timestamp and role-based public-front-door
  decisions added.
- **R4 - 2026-09-09:** reviewer accepted core architecture and required todo
  compression plus sequential routing. Compression applied; cinematic premise
  superseded by D18 after the proven raw-footage workflow closed that concern.
- **R5 - 2026-09-09:** duplicate mirror-policy authority dissolved after its
  unique clean-clone skill-bootstrap proof moved to P5. `InstanceArrayTool` is
  forbidden as a required dependency of either public acceptance root.
- **R6 - 2026-09-10:** live repository, mirror, hook, release-script, privacy,
  attribution, and player-archive audit resolved the remote/legal questions.
  Single-owner branch protection keeps PR plus `verify` with zero mandatory
  approvals; the release suite owns one non-personal readiness manifest;
  operator-only final signing and a concise Product-terms candidate remain the
  two bounded human gates.
- **R7 - PASS - 2026-09-11:** Track V removed from distribution per D18; it
  remains optional historical evidence. This is the planned final in-tag
  tracked correction. Post-publication router/archive edits are explicitly
  post-v2.0 housekeeping outside the immutable release tag.
