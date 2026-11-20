# Generated definition authority refresh and check workflow

**Status:** IMPLEMENTED - reviewer safety findings fixed and verified; awaiting operator review. The implementation is in the working tree and is not committed or staged.
**Scope:** Generated-definition authority workflow only: `scripts/git/mirror/refresh_developer_asset_authority.{ps1,py}`, the generated-authority error and source-root classification in `scripts/git/mirror/prepare_public_source.py` plus its caller `scripts/git/mirror/mirror_to_github.sh`, thin `make` targets in `Makefile`, one narrowly scoped incremental-skip correction in `Plugins/Editor/ProjectDefinitionGenerator/Source/ProjectDefinitionGenerator/Private/DefinitionGeneratorSubsystem.cpp`, the new mirror test and its registration in `.github/workflows/source-checks.yml` and `scripts/ue/package/run_release_preflight.ps1`, the current `ProjectExperienceData` authority manifest, and the mirror/generator/experience documentation. The ID-change contract is documentation and tests only; no new migration code or automatic asset deletion. No other generation C++, schema, payload-composition, or mirror-policy change.
**Stable documentation owner:** [scripts/git/mirror/README.md](../../scripts/git/mirror/README.md) for the refresh, check, cleanup, and error procedure; [Plugins/Editor/ProjectDefinitionGenerator/README.md](../../Plugins/Editor/ProjectDefinitionGenerator/README.md) for the commandlet invocation; [Plugins/Resources/ProjectExperienceData/README.md](../../Plugins/Resources/ProjectExperienceData/README.md) for the experience publication pointer.

## Contents

- [Goal](#goal)
- [Authority register](#authority-register)
- [Non-goals](#non-goals)
- [Verified evidence](#verified-evidence)
- [Current architecture and source of truth](#current-architecture-and-source-of-truth)
- [Problem and root cause](#problem-and-root-cause)
- [Decision](#decision)
- [Required invariants](#required-invariants)
- [Implementation tasks](#implementation-tasks)
- [Test-first and verification plan](#test-first-and-verification-plan)
- [Documentation plan](#documentation-plan)
- [Rollout and rollback](#rollout-and-rollback)
- [Completion criteria](#completion-criteria)
- [Review record](#review-record)

## Goal

Turn the current one-off failure into a repeatable, owner-complete workflow: after a generated-definition JSON is edited, added, deleted, or renamed (including a filename-only rename with an unchanged ID), one maintainer command regenerates every affected asset, removes generated orphans, refreshes every declared authority manifest, and verifies the result. A filename-only rename preserves the generated asset's package identity and updates its recorded source path instead of letting cleanup delete it. An ID change is a manual identity migration, not an ordinary regeneration: the routine command is expected to stop at the previous source-backed asset until references are audited and that asset is deliberately retired. An editor-free check reports stale or structurally inconsistent authority before commit or publish. The current stale experience authority is refreshed so `make mirror` can proceed.

## Authority register

### Operator decisions

- **D1 [SUPERSEDED IN PART by D2 - no-implementation clause]:** Investigate the proper workflow to check and regenerate all generated definitions, and write the implementation todo in `todo/00_current/`; do not implement it in this phase.
  - Effect: the scope covers every declared generated-definition owner and the current stale experience authority; a ProjectObject-only fix is insufficient. The workflow must expose one maintainer entry point rather than a multi-command sequence.
  - Reason: the reported `make mirror` failure and the need for a single check/regenerate workflow.
  - Date/source: operator request and `/investigate-change`, 2026-11-20.
- **D2:** Implement the approved todo; keep the work in the working tree and do not stage, commit, push, or publish.
  - Effect: the implementation phase is authorized; the plan's Q1/A1 and Q2/A4 defaults apply; no external publication.
  - Reason: operator invoked implement-approved-change.
  - Date/source: operator request, 2026-11-20.

### Operator gates

- **Q1 [OPEN - NON-BLOCKING]:** Should the mirror's generated-authority gate remain strict (recommended), or gain a default-off/opt-out liveness mode as raised earlier? - default while open: A1.
- **Q2 [OPEN - NON-BLOCKING]:** Should the editor-free check also be wired into `.githooks/pre-commit` or `validate_all.bat` in this change? - default while open: A4.

### Working assumptions

- **A1 [ACTIVE]:** Keep the strict mirror gate; add no default-off bypass. The projection rewrites the manifest's source checksum, and the check is cheap, so the correct fix is earlier detection, not publishing stale or false authority.
- **A2 [REJECTED by review evidence]:** Extend the existing `refresh_developer_asset_authority.ps1`/`.py`; do not add a parallel script or a `make` alias.
  - Rejection: the Makefile is the operator-facing command surface (`make mirror`, `make check`); a thin alias that delegates to the existing scripts is not a parallel implementation. The replacement is A5.
- **A3 [ACTIVE]:** The full regenerate flow is explicit. The default manifest refresh uses the existing generated assets and does not launch the definition commandlet.
- **A4 [ACTIVE]:** Automatic pre-commit/`validate_all` enforcement is deferred; expose `make check-generated-json` as the supported command and keep the mirror gate as the final backstop.
- **A5 [ACTIVE]:** Add thin `make generate-json` and `make check-generated-json` targets that delegate to the existing scripts; no second implementation. The editor-free check runs natively on WSL. The editor-dependent regeneration uses the Windows PowerShell/editor path, with WSL delegating through `powershell.exe` when the repository is Windows-accessible; native Windows remains the accepted release route.
- **A6 [SUPERSEDED IN PART by A9 and A11 - filename-only rename and ID-change handling]:** `-Regenerate` runs `GenerateDefinitions -all -cleanup`. Deletion and rename are handled by the generator's generated-asset-only cleanup, not by filesystem-wide deletion. `-Regenerate` rejects `-Owner`; `-Owner` applies only to the default manifest refresh and `-Check`.
- **A7 [ACTIVE]:** The editor-free check aligns with the payload composer's generated-definition structural rules: exactly one collection per asset class, matching schema path, unique package, package root, package-to-artifact path, root containment, the three recorded hashes, contract metadata, and source-root completeness.
- **A8 [ACTIVE]:** The mirror error compares the filtered source with the matching source snapshot (the exact revision for a clean projection, the worktree for a `--force` projection) when available and distinguishes a stale local authority from a public-projection/sanitizer change; the gate remains fail-closed.
- **A9 [ACTIVE]:** The generator's incremental skip compares the recorded `SourceJsonPath` in addition to the JSON hash and generator version. A same-ID filename-only rename refreshes the existing asset's metadata in place and preserves its package identity before cleanup. This is the one permitted generator C++ change.
- **A10 [ACTIVE]:** A refresh uses a newly created Asset Registry inventory path (per-run `ALIS_GENERATED_ASSET_INVENTORY_OUTPUT`, or the fixed file removed before export) and requires the file to exist before any manifest write. An editor exit code of 0 without a fresh export fails the workflow.
- **A11 [ACTIVE]:** An ID change is a manual identity migration, not an ordinary regeneration. The routine one-command flow is expected to stop at the authority refresh while the previous source-backed generated asset remains, because its recorded source path still exists and its source hash is stale. The operator audits references, deliberately retires the previous generated asset, and reruns the command. No automatic ID-change asset deletion or generic migration engine is added.

## Non-goals

- Disabling, default-offing, or bypassing the mirror's generated-authority gate (unless Q1 resolves otherwise).
- Changing the authority manifest schema, the release contract format, or any world, recipe, or producer-input authority.
- Changing generator C++ beyond the one incremental-skip condition in A9, or writing the authority manifest from the editor generator. The refresh script remains the manifest owner.
- Changing `compose_developer_payload.py`, `install_developer_payload.ps1`, or the mirror's rebind gate semantics.
- Filesystem-wide deletion or indiscriminate Content deletion. Cleanup is only the generator's generated-asset cleanup for the requested type.
- Automatic ID-change asset deletion, generic ID-migration tooling, or treating an ID change as a routine one-command regeneration (A11).
- A native-WSL editor implementation. The WSL `generate-json` branch delegates to the Windows PowerShell/editor path; `check-generated-json` is editor-free and runs natively.
- Wiring the check into the pre-commit hook or `validate_all.bat` in this change (Q2/A4).
- A new CI workflow. Register the new test in the existing public-source workflow and release preflight only.
- Regenerating unrelated assets or editing any source JSON or `.uasset` for the current data fix.
- Unrelated cleanup, formatting, or refactoring.

## Verified evidence

### Verified facts

- The reported failure is `[FAIL] Public filtering changed generated source semantics: Plugins/Resources/ProjectExperienceData/Data/Experiences/kazan_territory.json`, from `rebind_generated_definition_sources` in [prepare_public_source.py](../../scripts/git/mirror/prepare_public_source.py). The md5 comparison is at line 226 and the error at line 227. The projection runs before the mirror commit and push; the temp mirror tree is removed by the trap.
- Commit `b26ec8c6274c851fe9e518c98b56eff6d1ae91a3` (2026-11-19) added `"AllowEnvironmentalTimeControls": true` to both experience JSONs and changed both `.uasset` files. `Plugins/Resources/ProjectExperienceData/Data/Manifests/public_generated_experiences.json` was last written by `203667beb4c90a5377ded0ae95dd852e884ddb8e` (2026-10-07) and was not part of `b26ec8c62`.
- Current local hash comparison:

| Asset | Normalized source md5 (actual / manifest) | Normalized source sha256 (actual / manifest) | Artifact sha256 (actual / manifest) |
|---|---|---|---|
| KazanTerritory | `36998c65ec2c10bfed7c2dad210ef9dd` / `318ee938aae48604aa1d1b7daa6a120f` | `f7401e7076d1d97e0ce1e0c1358e17c8aec5d0753e24f8d3aac40fd40d916de8` / `d5eadf8d17fb46081a0ba3c75be4d4c7e0684c5bbd1a4aeefeb1cbe8a2c81fcd` | `73c25da4aa9b8d1cc5d46124978ec77a23c1ebd4072dd0a369baa0c2f0f824a2` / `d8716bd4dbb9c420dd7e4e389bda1e863791838eaf132c0543f9bf9ef976dd94` |
| ManhattanShowcase | `ff032ca35a02ca533dbcdfd40cfa87fd` / `85a0f587710f0055e132213c4b6087e1` | `8051f3d0bd190a4292e6506ffafbb630be2e637c127b39af44a3e6a3b523efb6` / `63c94441827e3a2abc4c84b7ccd401e3778e793706f78fafb2965069bfc64c37` | `599ca69f0d56e7cbfbb67564d976422defdabaedbb35f89a6f8f12a6b1e16c80` / `27af3657945c322082663c0453e807b23dadaf2d973bd5c9a5e7765462391e99` |

- Both `.uasset` files already embed the current source md5 (`SourceJsonHash`); the committed rebuild matches the current JSON. Only the tracked manifest records are stale.
- `ProjectObject` has 80 generated assets and zero stale hash fields. Its recorded source set equals the actual `*.json` set under its declared source roots (80). The experience authority records 2 assets and the actual set is 2.
- The contract's `release_contract_sha256` in both manifests is `0036840c753d8b6dbf8c54c006ed97534e0c47e658f2eb95505fe1a3ac491504`, which matches the current contract file.
- The current `Saved/Inspection/generated_asset_inventory.json` is stale (dated Sep 30) and records the old experience hashes; a fresh Asset Registry export is required for the refresh.
- `scripts/git/mirror/refresh_developer_asset_authority.ps1` hardcodes `--owner ProjectObject` at line 35. `scripts/git/mirror/refresh_developer_asset_authority.py` defaults `--owner` to `ProjectObject` at line 141 and requires `--inventory`.
- `GenerateDefinitionsCommandlet.cpp` requires either `-all` or `-type=<TypeName>`; the usage message is at lines 83-84. The generator README command at around line 139, `-run=GenerateDefinitions -nopause`, supplies neither and therefore fails.
- Orphan cleanup is separate from generation. `GenerateDefinitionsCommandlet.cpp` parses `-cleanup` at line 30 and calls `CleanupOrphanedAssets` only when it is set (lines 114-117), per type, including in the `-all` loop at line 67.
- `CleanupOrphanedAssets` in `DefinitionGeneratorSubsystem.cpp` is scoped to generated assets: it collects JSON paths under the type source directory, scans the type's `GeneratedContentPath` for assets with the generated flag set and a `SourceJsonPath`, deletes only those whose source path is absent, and removes empty generated directories. It does not delete arbitrary Content assets.
- `refresh_developer_asset_authority.py` refuses an inventory record whose source or artifact path is missing: `Generated pair is incomplete` in `build`. Without `-cleanup`, a deleted JSON leaves an orphaned generated asset in the inventory and the one-command flow fails at this check. For a same-ID filename-only rename, `-cleanup` alone is worse: the generation step skips the asset, its recorded `SourceJsonPath` remains the old name, and cleanup deletes the still-valid asset.
- The incremental-skip branch in `DefinitionGeneratorSubsystem.cpp` (lines 385-389) compares only the stored JSON hash and generator version. `SourceJsonPath` is written at line 453 but is not read or compared before the skip. The existing asset is found by `Result.AssetId`; the expected package path is `GeneratedContentPath / RelativeDir / AssetId`. A same-directory filename-only rename therefore keeps the same expected package path, so no relocation occurs, and the skip returns before the source-path metadata is refreshed.
- The minimal C++ correction is to read the existing asset's `SourcePathPropertyName` via reflection and require it to equal the current `RelativePath` in the skip condition. A path-only rename then updates the existing asset's metadata in place, preserves its package identity, and prevents cleanup from treating it as an orphan. The file is 909 lines; the change is a few lines in the existing skip branch, within the 700-1000 small-addition band. The file remains the right home because the change belongs to the existing per-asset incremental decision; the next SRP seam would be extracting that decision and generation block from the subsystem, which is not justified for this condition.
- The refresh PowerShell script reads the fixed `Saved/Inspection/generated_asset_inventory.json` at line 23 and checks only the editor process exit code. The exporter defaults to that fixed path but supports `ALIS_GENERATED_ASSET_INVENTORY_OUTPUT` for an alternate output. If the Python export fails while the editor exits 0, the old inventory remains and is reused; the reviewer's stale-inventory finding is valid.
- The tracked Makefile path is `Makefile` (uppercase; `git ls-files` reports `Makefile`). The Windows working tree resolves `makefile` as the same file, but case-sensitive Linux and public CI require the tracked spelling; the earlier TODO link `../../makefile` would not resolve there.
- Public GitHub CI has no Unreal editor and no generated `.uasset` payload. The new Python unit test must use synthetic JSON and `.uasset`-shaped byte fixtures in a temporary root. The C++ generator rename test and the editor end-to-end acceptance test are internal editor tests, not public GitHub CI tests.
- `FDefinitionAssetManager::FindExistingAsset` matches by asset name/ID under the generated content path. An ID change that keeps the JSON filename means the new ID is not found, so `CreateNewAsset` creates a new asset at `GeneratedContentPath/<NewId>`; the previous asset at `GeneratedContentPath/<OldId>` remains. `CreateNewAsset` does not search by source path.
- `CleanupOrphanedAssets` only deletes generated assets whose recorded `SourceJsonPath` is absent. The previous ID asset's source path is the same still-existing JSON, so it survives cleanup.
- The inventory exporter includes both assets. The previous asset's `SourceJsonHash` is the pre-ID-change hash; `refresh_developer_asset_authority.build` compares the current source md5 to each inventory record and raises `Asset Registry source hash is stale: <previous package>` before it writes the manifest. The routine `make generate-json` therefore fails after creating the new asset; it does not complete the identity change.
- The previous asset is source-backed and path-valid; removing it is a deliberate identity-migration step after a reference audit, not a cleanup action. The reviewer's ID-change sequence is confirmed.
- `prepare_public_source.py` normalizes and writes the source JSON, compares the normalized md5 to the manifest, then rewrites the manifest's `source_sha256`. The md5 comparison is the guard on that write.
- `compose_developer_payload.py` verifies the normalized source sha256, the normalized source md5, and the artifact sha256 before composing a payload (the checks around lines 236-240).
- `compose_developer_payload.py::collect_generated_definition_authority` additionally enforces: exactly one collection per asset class; source and artifact under the declared roots; unique package name; package under the package root; no `..`; artifact path derived from the package name; schema path equal to the collection's; and the recorded source set equal to the actual source-root set.
- `mirror_to_github.sh` lines 768-773 state that coherence between source identity and the asset payload is enforced by the release flow, not by routine mirror pushes. The generated-definition gate is stricter than that comment; this is the policy tension in Q1.
- No editor-free generated-authority check exists today:
  - `.githooks/pre-commit` checks Windows paths and two data cross-reference suites; it has no authority-freshness block.
  - `scripts/ue/check/validate_all.bat` has no generated-authority check.
  - `scripts/ue/check/governance/generate_component_manifest.py` verifies `source_sha256` at release-input preparation, but it requires a tag and a clean tagged HEAD (`resolve_tagged_head`), so it is not a local pre-commit check.
- There is no `make` target for authority refresh or check. `make generate` generates Visual Studio project files, not generated definition assets. The Makefile detects WSL at lines 18-34 and already has Windows/WSL branches for `mirror`.
- The `mirror` target's `force` modifier is synthesized by the Makefile at lines 87-92. `--force` sets `ALLOW_DIRTY=1`, bypasses the clean-tree check, and projects the current worktree; the source revision defaults to `HEAD`, satisfying the script's force validation. A plain `make mirror MIRROR_DRY_RUN=1` still refuses an uncommitted worktree because the clean check runs before the projection.
- The new test would not run automatically. `.github/workflows/source-checks.yml` discovers `scripts/git/tests`, governance tests, and assets tests, then explicitly runs `scripts/git/mirror/tests/test_public_source_projection.py`. `scripts/ue/package/run_release_preflight.ps1` explicitly lists the `Mirror` and `Payload` test modules. A new `scripts/git/mirror/tests/test_refresh_asset_authority.py` is outside all discovery roots.
- The public projection runs the sanitizer before the generated-authority comparison. `mirror_to_github.sh` sanitizes the filtered tree and then calls `prepare_public_source.py`. `sanitize_public_text.py` includes `.json` in `TEXT_SUFFIXES` and applies machine-local path rules to non-code suffixes, so sanitization can change JSON bytes before the manifest comparison.
- The generated authority is small: the 80 ProjectObject assets total about 0.6 MB and the 2 experience assets about 0.0 MB. A full local hash check is cheap. The 9.5 GB under `ProjectObject/Content` is unrelated non-generated content and is not hashed by the check.
- The source repository was clean when the incident was investigated. Later todo-only revisions change no source or manifest. The reported failure was not caused by local modifications; the mirror's clean-tree check runs before the projection and the user's run reached the projection.

### Evidence boundary

The exact hashes, clean-tree state, and the November commit are local HEAD evidence from the operator's repository. The public mirror is a filtered snapshot and can lag; the reviewer's public snapshot lacking the November JSON is expected and is not evidence against the local incident. The local repository is authoritative for this todo, and the public mirror will reflect these facts only after a successful mirror run.

### Inferences

- The fixed default refresh with a fresh inventory will rewrite only the two experience entries; ProjectObject should remain byte-identical because its hashes and contract are current.
- `-Regenerate` with `-all -cleanup` should be an incremental no-op for the current assets because their embedded source hash already matches the JSON and no orphan exists; this remains unverified until the commandlet runs.
- A fresh editor export is the only sanctioned evidence that the `.uasset` was generated from the current JSON; the editor-free check can verify recorded hashes and structure but not regenerate that provenance.
- The sanitizer can alter a generated definition JSON in principle; whether it does so for a specific public projection must be determined from the failed comparison, not assumed.
- The same-ID filename-only rename fix is a metadata refresh, not a package relocation: the package name remains `GeneratedContentPath/<RelativeDir>/<AssetId>`, so gameplay references and the package identity are preserved. An ID change is a manual identity migration: the new asset is created, the previous source-backed asset remains, and the routine command is expected to stop at the stale previous hash until that asset is audited and deliberately retired (A11).
- The current experience assets' recorded source paths match their current JSON paths, so the `SourceJsonPath` skip addition should leave them byte-identical. This remains unverified until the editor test runs.
- A per-run inventory path removes the stale-file reuse risk without changing the refresh's evidence semantics.

### Assumptions / unverified areas

- The operator's editor installation can run `UnrealEditor-Cmd.exe` and the `GenerateDefinitions` commandlet. The November assets were generated, but this session did not launch the editor.
- The operator accepts keeping the strict mirror gate (A1/Q1).
- The Makefile is the preferred operator-facing command surface (A5); the scripts remain the implementation owners.
- The Makefile's WSL `generate-json` branch can reach a Windows-accessible repository and the installed Windows editor through `powershell.exe`. A WSL-filesystem-only checkout is not the release acceptance route.
- The check's source-root completeness check will pass for both owners after the fresh refresh; the current on-disk comparison already shows matching sets.
- The deletion and filename-only rename acceptance test can use a disposable linked worktree under `tmp/` and the installed editor. If the editor is unavailable, that evidence remains blocked and must be reported, not inferred.
- The focused C++ generator rename test can isolate a temporary source directory and generated content path in the editor test harness. If the subsystem cannot be isolated without project content, the disposable-worktree acceptance test remains the required evidence and the limitation is recorded.
- The new Python test uses only synthetic fixtures and does not require an editor, a real `.uasset`, or the internal generated manifest. This is required for public GitHub CI.
- The ID-change acceptance test can change an identity in a disposable worktree, observe the expected failure, audit/migrate references, deliberately delete the previous generated asset, rerun, and observe success. It is an internal editor test; public CI cannot supply the previous `.uasset`.

### Refuted

- "The mirror changed or corrupted the JSON." The working-tree JSON normalized md5 matches the `.uasset` and differs from the manifest record; the mirror did not modify the source repository.
- "The `.uasset` is stale." Both assets embed the current source md5.
- "The documented refresh command refreshes every owner." It hardcodes `ProjectObject`.
- "The bare `GenerateDefinitions` command works." The commandlet requires `-all` or `-type=<TypeName>`.
- "The mirror checks binary files." The projection opens the source JSON and the manifest; the `.uasset` is excluded from the source mirror and is not opened by the projection.
- "The current mirror message is always caused by filtering." It is correct when the sanitizer changes the JSON, but it is wrong for a stale authority. The current incident is the latter; the revised error must distinguish them.
- "A native-WSL editor regeneration is required to support WSL." The editor step is Windows; the WSL target delegates to the Windows PowerShell/editor path, while the editor-free check runs natively. Native Windows remains the release acceptance route.
- "`-cleanup` alone handles every rename." It handles a deleted source, but a same-ID filename-only rename is skipped before `SourceJsonPath` updates, so cleanup deletes the still-valid asset. The incremental-skip condition must include the recorded source path.
- "The fixed inventory path is safe to reuse." It is not: an editor exit code of 0 with a failed Python export leaves the old inventory in place, and the refresh would accept stale evidence.
- "The lowercase `makefile` link is portable." The tracked path is `Makefile`; case-sensitive Linux and public CI resolve only the tracked spelling.
- "`make generate-json` completes an ID change." It does not: the new asset is created, the previous source-backed asset remains, its `SourceJsonHash` is stale, and the authority refresh rejects it. The expected failure is the start of the manual migration.
- "`-cleanup` removes the previous ID asset." It does not: cleanup only deletes generated assets whose source path is absent, and the previous asset's source path is the still-existing JSON.
- "An ID change is an ordinary regeneration." The identity changed; the previous asset and any gameplay references require a deliberate migration, not routine regeneration.

## Current architecture and source of truth

Text-first authoring and the derived binary flow:

```text
Data/**/*.json
  -> ProjectDefinitionGenerator (editor commandlet or editor menu)
  -> generated .uasset with SourceJsonHash
  -> Asset Registry export (Saved/Inspection/generated_asset_inventory.json)
  -> refresh script -> public_generated_*.json (owner authority manifest)
  -> mirror filter -> sanitize -> projection (normalize, rebind source_sha256, md5 gate) -> public source
  -> compose_developer_payload (verify manifest hashes and structure) -> developer payload
  -> install_developer_payload (pin source revision/tag)
```

Ownership:

- [ProjectDefinitionGenerator](../../Plugins/Editor/ProjectDefinitionGenerator/README.md) owns JSON-to-DataAsset generation, orphan cleanup for generated assets, and the `SourceJsonHash`, `SourceJsonPath`, `bGenerated`, and `GeneratorVersion` metadata.
- [developer_asset_release.json](../../scripts/git/mirror/developer_asset_release.json) owns the declared generated-definition owners, collections, source roots, artifact roots, schema references, and manifest paths.
- [refresh_developer_asset_authority.py](../../scripts/git/mirror/refresh_developer_asset_authority.py) owns the inventory-evidence build, the authority check, and the authority manifest write. The PowerShell wrapper resolves the engine and runs the Asset Registry export.
- [prepare_public_source.py](../../scripts/git/mirror/prepare_public_source.py) owns the public projection of the manifest: line-ending normalization, `source_sha256` rebinding, the md5 guard, and the published manifest copy.
- [compose_developer_payload.py](../../scripts/git/mirror/compose_developer_payload.py) owns payload composition and the final hash and structural verification of sources and artifacts.
- [install_developer_payload.ps1](../../scripts/git/mirror/install_developer_payload.ps1) owns verification of the installed payload against the recorded public source revision/tag.
- [Makefile](../../Makefile) owns the operator-facing command names; the scripts remain the implementation owners.
- [scripts/git/mirror/README.md](../../scripts/git/mirror/README.md) owns the maintainer refresh, check, cleanup, and publication procedure. The experience and generator docs should link to it rather than restate it.

The `.uasset` files are not part of the public source mirror; the manifest is. The manifest is therefore the only source-side binding between the public JSON and the separately delivered developer payload.

## Problem and root cause

The mirror gate fired because the tracked experience authority was stale. The JSON and the regenerated `.uasset` agreed, but `public_generated_experiences.json` still recorded the pre-change hashes.

The direct workflow causes are:

1. The documented refresh entry point cannot refresh the experience authority. Both the PowerShell wrapper and the Python default are hardcoded to `ProjectObject`; the experience authority was added later and never wired into the all-owner path.
2. There is no editor-free freshness check at authoring or commit time. The first feedback is the mirror projection, after the source change is already committed. `b26ec8c62` changed the JSON and assets without the manifest, and no local gate caught it.
3. The generator README's commandlet example omits the required `-all` or `-type=<TypeName>`, so the documented generation step does not run as written.
4. The mirror error cannot distinguish a stale local authority from a public projection or sanitizer change. It blames filtering unconditionally and gives no owner or remedy. For the current incident the filter was not the cause; for a future sanitizer change, regeneration would be the wrong remedy.
5. `-Regenerate` without the commandlet's `-cleanup` leaves an orphaned generated asset for a deleted JSON, and the refresh refuses the missing source pair. With `-cleanup`, deletion works. A same-ID filename-only rename is still broken: the incremental skip compares only hash and version, so the existing asset keeps its old `SourceJsonPath`; cleanup then treats the still-valid asset as an orphan and deletes it. The C++ skip condition must include `SourceJsonPath` (A9). An ID change is a separate manual identity migration: the new ID creates a new asset, the previous source-backed asset remains because its source path exists, and its stale `SourceJsonHash` makes the authority refresh fail. The routine `make generate-json` therefore does not complete an ID change; the operator must audit references, deliberately retire the previous asset, and rerun (A11).
6. The editor-free check, as originally planned, does not enforce the payload composer's structural rules. A manifest with a duplicate package, wrong class or schema mapping, or package-to-artifact path mismatch could report current and fail only later during composition.
7. The Makefile has no simple authority command. The operator asked for a KISS command and already uses `make mirror`; the only refresh entry point is a long PowerShell path.
8. The new test would never run in CI or the release preflight because both runners list mirror tests explicitly rather than discovering the full mirror test directory.
9. The planned clean mirror dry run cannot execute on the uncommitted implementation worktree. The mirror's clean check runs before the projection and is not bypassed by a dry run.
10. The refresh can reuse a stale Asset Registry inventory. The PowerShell script reads the fixed `Saved/Inspection/generated_asset_inventory.json` and checks only the editor process exit code. If the Python export fails while the editor exits 0, the old inventory remains and is accepted as fresh evidence (A10).
11. The TODO and its planned documentation used lowercase `makefile`, while the tracked path is `Makefile`. The Windows working tree resolves both, but case-sensitive Linux and public CI do not; the relative link and documentation references must use the tracked spelling.

The gate itself is correct for the published tree: the projection rewrites the manifest's `source_sha256`, so accepting a changed source while retaining the old asset binding would publish a false provenance claim. The defect is the missing owner-complete refresh, the same-ID filename-only rename handling, the stale-inventory reuse, the missing structural checks, the ambiguous diagnosis, and the late detection. It is not the gate.

## Decision

Adopt the existing refresh scripts as the implementation owners and expose them through the established Make surface.

Developer-facing contract:

| Command | Behavior |
|---|---|
| `make check-generated-json` | Read-only check of all generated-definition authorities; no editor; no writes. |
| `make generate-json` | Regenerate all registered definitions with orphan cleanup, export the Asset Registry inventory, refresh all owners, and verify. |
| `make mirror` | Unchanged strict publication behavior. |

Detailed design:

1. Python (`refresh_developer_asset_authority.py`):
   - Discover every `generated_definition_manifest` authority from the contract. `--owner` is an optional filter; the default is all declared generated-definition owners.
   - Add `--check`: read-only, editor-free, no inventory. It verifies the contract hash, owner, manifest id, exactly one collection per asset class, the collection schema path, source and artifact root containment, unique package names, package root and `..` safety, the package-derived artifact path, the three recorded hashes, and source-root completeness. It reports every failure, not only the first.
   - Keep the refresh evidence-based: an inventory whose `source_json_hash` differs from the current source is still rejected.
   - Build every requested manifest in memory, write them, then run the same check over the written result. Exit nonzero on any verification failure.
   - Preserve the existing JSON formatting and manifest schema.
2. PowerShell (`refresh_developer_asset_authority.ps1`):
   - `-Check`: run the Python check before resolving the engine or touching the inventory. No editor, no writes.
   - `-Regenerate`: run `UnrealEditor-Cmd.exe <project> -run=GenerateDefinitions -all -cleanup -unattended -nop4 -NoSound -NullRHI`, capture a dedicated log, fail on a nonzero exit, then export the inventory, refresh all owners, and verify. The generator cleanup is limited to generated assets under the type's generated content path; the operator reviews the resulting asset deletions before commit.
   - Default (no switch): export the inventory and refresh all owners from the existing generated assets. Keep `-Owner` for a targeted manifest refresh or check.
   - Use a newly created inventory path for every export: set `ALIS_GENERATED_ASSET_INVENTORY_OUTPUT` to a per-run file under `Saved/Inspection` (or remove the fixed file before export), and require the file exists before invoking the Python refresh. A missing or stale export fails before any manifest write.
   - Reject `-Check` with `-Regenerate` and reject `-Owner` with `-Regenerate`. A regenerate all-types command must not leave a non-selected owner stale.
3. Generator (`DefinitionGeneratorSubsystem.cpp`):
   - In the existing-asset skip branch, read the recorded `SourceJsonPath` via reflection and require it to equal the current `RelativePath` in addition to the JSON hash and generator version (A9).
   - If the path differs, do not skip; update the existing asset's `SourceJsonPath`, hash, and version in place. A same-directory filename-only rename keeps the same `GeneratedContentPath / RelativeDir / AssetId`, so its package identity and gameplay references are preserved; no relocation or delete is needed.
   - Do not change `FindExistingAsset`, the cleanup predicate, or any other generator behavior. Cleanup remains generated-asset-only. An ID rename is a separate content change and is not inferred by path cleanup.
4. Makefile:
   - Add `generate-json` and `check-generated-json` after the existing `generate` and check targets. Both are thin delegates; no duplicate logic.
   - `check-generated-json`: on Windows call the PS1 `-Check`; on WSL call the Python `--check` natively.
   - `generate-json`: on Windows call the PS1 `-Regenerate`; on WSL invoke the same PS1 through `powershell.exe` with `wslpath -w` and fail clearly if the interop or a Windows-accessible checkout is unavailable. Native Windows remains the release acceptance route.
   - Add the targets to `.PHONY` and the `help` text. Use the tracked `Makefile` spelling; no target-name change.
5. Mirror projection (`prepare_public_source.py` and its caller):
   - Keep the md5 guard, fail-fast behavior, and error for the first stale asset.
   - Pass the source repository root and, for a clean projection, the source revision from `mirror_to_github.sh` to the projection. For a `--force` projection, compare the worktree source that produced the filtered tree. On a filtered-source mismatch, compare the matching source snapshot with the manifest:
     - The source snapshot differs from the manifest: the local authority is stale. Name the owner, path, expected/actual md5, and recommend `make generate-json` (or the PS1 `-Regenerate`).
     - The source snapshot matches the manifest but the filtered copy differs: the public projection or sanitizer changed the source. Do not regenerate; name the owner, path, expected/actual md5, and point at the sanitizer/projection. The gate still fails.
     - The source snapshot is unavailable: report both possibilities without claiming which occurred.
   - The classification never weakens the gate; it only selects the remedy text.
6. Test registration:
   - Add the new `scripts/git/mirror/tests/test_refresh_asset_authority.py` to `.github/workflows/source-checks.yml` after the existing mirror projection test.
   - Add the same test to the `Mirror` group in `scripts/ue/package/run_release_preflight.ps1`.
   - The Python test uses synthetic fixtures only. The C++ same-ID rename test and the editor end-to-end acceptance test are internal editor tests, not public GitHub CI tests.
7. Current data:
   - Run the fixed default manifest refresh with a fresh Asset Registry export; do not regenerate the already-current binaries. It must rewrite only the two experience entries' hash fields. The `.uasset` and source JSON files must not change.
8. Documentation:
   - Update the mirror README, generator README, experience README, and Make help as described in the documentation plan. Use `Makefile` for the tracked path.

### ID-change migration contract

The routine `make generate-json` covers in-place edits, additions, deletions, and same-ID filename-only renames. An ID change is a manual identity migration, not an ordinary regeneration (A11):

1. Change the ID in the JSON. The generator creates a new asset at the new identity. The previous generated asset remains because its recorded `SourceJsonPath` still points to the existing JSON.
2. Run `make generate-json`. Expect it to stop at the authority refresh with `Asset Registry source hash is stale: <previous package>`. The new asset was created, but the previous source-backed asset blocks the manifest refresh. This is the defined expected failure.
3. Audit all references to the previous identity: gameplay data and Blueprints, object/dialogue/audio references, configuration, UI, and any other asset references. Migrate or remove them deliberately.
4. Deliberately retire or remove the previous generated asset through the editor or a reviewed asset operation. Do not rely on `-cleanup`; it only deletes generated assets whose source path is absent. Do not add an automatic ID-migration engine.
5. Rerun `make generate-json`. Verify the previous identity is absent from the inventory and manifest, the new identity is present, and `make check-generated-json` passes.

The expected failure is part of the contract, not a routine success path. The routine one-command workflow does not promise to complete an ID change.

Direct script commands remain available for targeted work:

```powershell
.\scripts\git\mirror\refresh_developer_asset_authority.ps1 -Regenerate
.\scripts\git\mirror\refresh_developer_asset_authority.ps1 -Check
```

The default refresh remains manifest-only. The full regenerate flow is explicit per A3. The strict mirror gate remains per A1. Automatic commit-time enforcement is deferred per A4/Q2. The Make targets are thin aliases per A5, the regenerate/owner rules are A6, the checker contract is A7, the mirror classification is A8, the generator skip correction is A9, the fresh-inventory guarantee is A10, and the ID-change migration contract is A11.

### Premise / KISS gate

- **Owner:** the existing refresh scripts already own the Asset Registry evidence and the authority manifest write. The Makefile is the operator-facing alias surface, not a second implementation.
- **Parts removed:** the manual second `--owner ProjectExperienceData` call; the separate generate, cleanup, refresh, and check sequence; the ambiguous mirror failure; the untested delete/rename path; the stale fixed inventory reuse.
- **Parts added:** two thin Make aliases, one optional filter, one check mode, one regenerate switch with cleanup, one owner loop, one `SourceJsonPath` skip condition, a per-run inventory, structural verification, a source-root classification, and a manual ID-change contract. No service, process, port, protocol, callback, or new lifecycle. An ID change deliberately uses existing editor asset operations after a reference audit; no migration subsystem is added.
- **Capability knowingly given up:** automatic pre-commit enforcement (Q2/A4) and a native-WSL editor implementation. The editor-free check runs on WSL; the editor step delegates to Windows.
- **Existing machinery retained against a concrete requirement:** the editor Asset Registry export is the only evidence that the `.uasset` was generated from the current JSON. The generator cleanup remains the safe deletion mechanism, but it is not sufficient for a same-ID filename-only rename; the source-path skip condition complements it. The previous ID asset is removed through the existing editor asset operation after the audit, not by an automatic migration engine. The check does not replace either.

### Alternatives considered

- Add a new `sync_definitions.ps1` plus a Make target: rejected because it duplicates the existing wrapper and command surface. Thin Make aliases (A5) are accepted instead.
- Make the mirror gate optional or default-off: rejected because the projection writes the manifest; a bypass would publish either a false binding or a stale authority. This is Q1/A1.
- Auto-refresh inside the mirror: rejected because the mirror cannot regenerate the `.uasset` or verify the Asset Registry evidence, and it would mutate published authority without review. It also cannot distinguish a stale authority from a sanitizer change.
- Have the generator write the manifest: viable long-term, but larger (editor code plus cross-owner aggregation) and not the smallest change. The refresh script is the current manifest owner.
- Make `-Regenerate` the default: rejected because it launches the editor, may write assets, and may delete generated orphans; the default must remain a manifest refresh.
- Check only the source md5, as the mirror does: rejected because source sha256, artifact sha256, structural mappings, and source-root completeness are cheap to verify and a malformed manifest could otherwise report current.
- Add a separate `-Cleanup` switch instead of running cleanup in `-Regenerate`: rejected because the one-command recovery for a deleted JSON would still leave the orphan and fail at the refresh.
- Rely on `-cleanup` alone for a same-ID filename-only rename: rejected because the incremental skip returns before `SourceJsonPath` updates, so cleanup deletes the still-valid asset. The minimal skip condition is A9.
- Delete and recreate the asset on every path change: rejected because it needlessly changes package identity and risks gameplay references; a same-directory filename-only rename should update metadata in place.
- Infer an ID rename in path cleanup: rejected because an ID rename can leave the source path present; it is a separate content change and may require gameplay reference updates.
- Automatically delete the previous ID asset: rejected because it is source-backed and may still be referenced; deleting it without a reference audit risks broken gameplay references. The previous asset is retired deliberately after the audit (A11).
- Build a generic ID-migration engine or an ID-change command: rejected as a separate subsystem with no demonstrated need. The manual audit, deliberate asset retirement, and rerun are the smallest safe contract.
- Treat an ID change as a routine `make generate-json` success: rejected because the previous source-backed asset remains with a stale `SourceJsonHash`; the authority refresh fails by design. The expected failure is documented; ignoring it would publish or accept mixed identities.
- Reuse the fixed `Saved/Inspection/generated_asset_inventory.json`: rejected because a failed export with editor exit code 0 leaves the old file and stale evidence. A per-run output is A10.
- Require a final all-owner check after `-Regenerate -Owner`: rejected as more complex than rejecting the combination (A6).
- Wire the check into the pre-commit hook now: deferred per Q2/A4 to avoid index/working-tree coupling and to keep this change within the refresh owner.
- A native-WSL editor implementation: rejected because the editor is Windows and the project's release acceptance is native Windows; the WSL branch delegates through `powershell.exe` and the check runs natively.

## Required invariants

1. The mirror's generated-authority gate remains strict and fail-closed; no bypass is introduced.
2. The refresh remains evidence-based and rejects an inventory whose recorded source hash differs from the current source.
3. `-Check` writes nothing, requires no inventory, and never launches the editor.
4. The default refresh covers every `generated_definition_manifest` authority declared in the contract. Adding an owner to the contract requires no code change.
5. `-Owner` restricts the default refresh and check to one declared generated-definition owner; an unknown or non-generated owner fails. `-Regenerate` and `-Owner` are mutually exclusive.
6. The refresh builds all requested outputs before writing, then verifies the written result. A verification failure exits nonzero.
7. The check verifies all three recorded hashes, the contract sha, the owner, the manifest id, exactly one collection per asset class, the collection schema path, source and artifact root containment, unique package names, package root and `..` safety, the package-derived artifact path, and source-root completeness. It reports every failure.
8. `-Regenerate` runs `GenerateDefinitions -all -cleanup`, with the A9 skip condition. The three rename outcomes are distinct:
   - A deleted JSON: cleanup deletes the orphaned generated asset; the refreshed manifest drops the old record; the final check passes.
   - A same-ID filename-only rename: the skip condition detects the changed `SourceJsonPath`; the existing asset's metadata updates in place; its package identity and gameplay references are preserved; cleanup does not delete it; the manifest record updates the source path; the final check passes.
   - An ID change is a manual identity migration (A11): the generator creates the new asset, the previous source-backed asset remains, and `make generate-json` is expected to fail at the authority refresh with the previous package's stale `SourceJsonHash`. After the reference audit and deliberate retirement of the previous asset, the rerun succeeds. The routine one-command workflow is not claimed to complete an ID change.
9. Cleanup is limited to generated assets under the type's generated content path whose `SourceJsonPath` is absent. No filesystem-wide or arbitrary Content deletion is introduced.
10. The current data change affects only the two experience asset entries' hash fields. Source JSON and `.uasset` files remain unchanged.
11. The PowerShell wrapper remains Windows PowerShell 5.1 compatible and resolves paths from the repository and engine configuration. No machine-local path is written.
12. The Make targets are thin delegates. `check-generated-json` is editor-free on Windows and WSL; `generate-json` uses the Windows PowerShell/editor path, with WSL delegation and a clear failure when interop is unavailable; native Windows is the release acceptance route.
13. The new Python test is registered in the existing public-source workflow and release preflight. The C++ same-ID rename test and the editor acceptance test are internal editor tests. No new CI workflow is added.
14. The mirror classification distinguishes a stale authority from a public-projection or sanitizer change when the matching source snapshot is available (revision for a clean projection, worktree for `--force`); the mismatch always fails.
15. No manifest schema, release contract, world authority, recipe authority, producer input, compose, or installer behavior changes.
16. The check does not claim to prove Asset Registry provenance; the refresh's inventory export remains that evidence.
17. The refresh uses a newly created Asset Registry inventory and requires it to exist before any manifest write. A missing, stale, or failed export cannot be accepted, even when the editor process exits 0.
18. The generator C++ change is limited to the A9 incremental-skip condition. `FindExistingAsset`, the cleanup predicate, package naming, and all other generator behavior remain unchanged. The C++ file is 909 lines, within the small-addition band.
19. An ID change is a manual identity migration, not a routine one-command success. The expected authority-refresh failure, the asset-reference audit, the deliberate retirement of the previous source-backed asset, and the rerun success are the acceptance contract. No automatic ID-change deletion or generic migration engine is added.

## Implementation tasks

- [x] Add the failing tests first:
  - [x] New `scripts/git/mirror/tests/test_refresh_asset_authority.py` covering matching and stale md5, stale source sha256, stale artifact sha256, missing source, missing artifact, unmanifested source, stale contract hash, all-owner refresh, single-owner refresh, unknown owner, stale inventory, non-definition authorities ignored, duplicate package, wrong asset class or schema mapping, package-to-artifact path mismatch, and path escape. Use synthetic JSON and `.uasset`-shaped byte fixtures in a temporary root only; no real `.uasset` or internal manifest. Include a no-write assertion for the check.
  - [x] Add mirror error tests to `scripts/git/mirror/tests/test_public_source_projection.py`: a stale clean source revision reports the refresh remedy; a filtered copy that differs while the clean source revision matches the manifest reports the projection/sanitizer remedy; the same pair on a `--force` worktree reports the matching worktree verdict; the gate still raises in every case.
  - [x] Add a focused C++ generator test for a same-ID filename-only rename with unchanged contents and ID: generate the asset, rename the JSON, generate with `-cleanup`, and assert the asset is not deleted, its package name is unchanged, its `SourceJsonPath` is updated, and cleanup does not remove it. If the subsystem cannot be isolated without project content, use the disposable-worktree acceptance test and record the limitation. This is an internal editor test, not public CI.
  - [x] Extend the acceptance test to distinguish: (a) a deleted JSON is removed by cleanup; (b) a same-ID filename-only rename preserves package identity and updates `SourceJsonPath`; (c) an ID change follows the manual migration contract: the first run creates the new asset and fails on the previous source-backed asset's stale hash; the reference audit is recorded; the previous asset is deliberately removed; the rerun succeeds and the old identity is absent. Add a focused internal automation test for this sequence if the editor test harness supports the temporary asset/removal operation.
  - [x] Add a PowerShell fixture test with a fake editor that exits 0 without writing the inventory. Assert the refresh fails before any manifest write and that a stale inventory is not accepted.
  - [x] Run the new tests and record the expected failures before implementation.
- [x] Add the generator skip correction in `DefinitionGeneratorSubsystem.cpp`:
  - [x] In the existing-asset skip branch, read `SourceJsonPath` via reflection and require it to equal the current `RelativePath` in addition to the hash and generator version.
  - [x] A path-only rename must update the existing asset's metadata in place and preserve its package identity; do not change `FindExistingAsset`, cleanup, package naming, or other generator behavior.
- [x] Extend `scripts/git/mirror/refresh_developer_asset_authority.py`:
  - [x] Add contract-driven owner discovery and optional `--owner` filtering, defaulting to all generated-definition authorities.
  - [x] Add `--check` with no inventory and no writes, including the composer-aligned structural rules and source-root completeness.
  - [x] Add the refresh-all loop, build-before-write, and post-write verification.
  - [x] Validate CLI combinations and report `[FAIL]`/`[OK]` with owner, path, and expected/actual values.
- [x] Extend `scripts/git/mirror/refresh_developer_asset_authority.ps1`:
  - [x] Add `-Check`, `-Regenerate`, and `-Owner`; reject `-Check` with `-Regenerate` and `-Owner` with `-Regenerate`.
  - [x] Run `-Check` before editor resolution and inventory work.
  - [x] Run `GenerateDefinitions -all -cleanup` for `-Regenerate`, with a dedicated log and exit-code check.
  - [x] Use a per-run inventory path via `ALIS_GENERATED_ASSET_INVENTORY_OUTPUT` (or remove the fixed file before export); require the file exists before the Python refresh; fail before any manifest write if it is missing.
  - [x] Export the inventory once, refresh the requested owners, and run the final check.
- [x] Add the Makefile targets:
  - [x] `generate-json` and `check-generated-json`, with Windows and WSL branches, `.PHONY` entries, and `help` text.
  - [x] `check-generated-json` runs the editor-free Python check natively on WSL.
  - [x] `generate-json` runs the PS1 on Windows and via `powershell.exe` plus `wslpath -w` on WSL, with a clear failure when interop or a Windows-accessible checkout is unavailable.
- [x] Add the mirror source-root classification:
  - [x] Pass `--source-root` always and `--source-revision` only for a clean projection from `mirror_to_github.sh` to `prepare_public_source.py`; for `--force`, compare the worktree source that produced the filtered tree.
  - [x] Compare the filtered source to the matching source snapshot; select the stale-authority or projection/sanitizer remedy; keep the gate fail-closed.
  - [x] Replace the misleading generated-authority error with the owner, path, expected/actual md5, and the classified remedy.
- [x] Register the new test:
  - [x] Add the explicit `python -m unittest scripts/git/mirror/tests/test_refresh_asset_authority.py` line to `.github/workflows/source-checks.yml`.
  - [x] Add the test to the `Mirror` group in `scripts/ue/package/run_release_preflight.ps1`.
  - [x] Register the C++ generator rename test in the internal editor test suite, not public CI.
- [x] Refresh the current stale experience authority with the fixed default manifest refresh. Do not regenerate the current binaries. Verify the exact hash values in this todo and that no source JSON or `.uasset` changes.
- [x] Update the documentation:
  - [x] `scripts/git/mirror/README.md`: the Make commands, the one-command `generate-json` flow, the editor-free `check-generated-json`, all-owner default, `-Owner` scope, `-cleanup` semantics, the source-root error classification, and the retained mirror gate.
  - [x] `Plugins/Editor/ProjectDefinitionGenerator/README.md`: fix the commandlet example to include `-all` or `-type=<TypeName>`; add the recorded source path to the incremental-generation triggers; document `-cleanup` and the same-ID filename-only rename behavior; link to the mirror refresh procedure.
  - [x] `Plugins/Resources/ProjectExperienceData/README.md`: add a publication pointer to the mirror refresh procedure.
  - [x] `Makefile` help and any existing command list in the mirror README: add the two Make commands and use the tracked `Makefile` spelling.
- [x] Isolate the C++ safety test: unique temporary type/ID/source/content, collision refusal, own-only cleanup, and no production `Data/Experiences` or `Content/Experiences` mutation.
- [x] Count commandlet generation failures before cleanup and skip cleanup on failure or a missing source directory; cover invalid JSON and missing-source regressions.
- [x] Make the PowerShell inventory fixture self-contained with a minimal `ue_path.conf` containing only `UE_PATH`.
- [x] Document the cleanup safety in the mirror and generator READMEs.
- [x] Run the verification matrix and review the final diff. Do not stage or commit; the operator owns the Git index.

## Test-first and verification plan

### Red evidence

- **New Python tests:** run `python scripts/git/mirror/tests/test_refresh_asset_authority.py -v`. Before implementation the check, all-owner, and structural paths do not exist, so the new cases fail for the missing capability, not for a malformed fixture. After implementation each case must exercise the real behavior. The fixtures are synthetic; public CI has no `.uasset` payload.
- **Mirror error tests:** run `python scripts/git/mirror/tests/test_public_source_projection.py -v`. The new cases fail against the current message and the missing source-root distinction.
- **Same-ID filename-only rename:** before the C++ skip fix, generate a temporary definition asset, rename its JSON without changing the contents or ID, run generation with `-cleanup`, and observe the existing asset skipped with its old `SourceJsonPath`, then deleted by cleanup, and the final check fail. After the fix, observe the package name unchanged, `SourceJsonPath` updated, the asset retained, and the check pass.
- **Deletion and ID change:** run the deletion case without `-cleanup` and observe the orphan; with `-cleanup`, observe the old asset and manifest record removed. For an ID change, run `make generate-json` and observe the expected nonzero result: the new asset was created, the previous source-backed asset remains with its old `SourceJsonPath`, and the authority refresh fails with its stale `SourceJsonHash`. Then perform the reference audit, deliberately remove the previous asset, rerun, and observe success. Do not treat the first run as a routine success or expect path cleanup to remove the previous asset.
- **Inventory export failure:** with a stale inventory present, run the PS1 with a fake editor that exits 0 without writing. Before the fix, observe the stale inventory accepted; after the fix, observe the PS1 fail before any manifest write.
- **Real red fixture:** after `--check` exists but before the manifest refresh, run:

  ```powershell
  python scripts/git/mirror/refresh_developer_asset_authority.py --repo-root . --check
  ```

  It must exit 1 and name both `ProjectExperienceData` sources. This is the reproduced defect; the mirror failure is the same condition discovered later.
- **Make targets:** before the `Makefile` change, `make generate-json` and `make check-generated-json` fail as unknown targets; after, both run.
- **Tracked path case:** before the documentation correction, the `../../makefile` link is absent on a case-sensitive checkout; after, the `../../Makefile` link resolves. Verify with the existing relative-link check or a case-sensitive path probe.
- **Test registration:** before, the new test is absent from `.github/workflows/source-checks.yml` and the preflight `Mirror` list; after, both list it. The red is the missing runner entry, verified by inspection and by running the new test through the same commands.
- **Dirty preview:** a plain `make mirror MIRROR_DRY_RUN=1` fails on the uncommitted implementation worktree with the clean-tree refusal. The implementation preview must use the supported dirty-mode command below. The ordinary clean dry run is reserved for the operator after commit.
- **Initial symptom:** the operator's `make mirror` output named `kazan_territory.json` and stopped before commit/push. Record it as the trigger, not as proof that the filter changed the file.

### Green evidence

- The unit test suites pass:

  ```powershell
  python scripts/git/mirror/tests/test_refresh_asset_authority.py -v
  python scripts/git/mirror/tests/test_public_source_projection.py -v
  ```

- The editor-free check passes after the refresh:

  ```powershell
  make check-generated-json
  ```

  Expected: `[OK]` and exit 0. The command must not launch the editor or create or modify the inventory. On WSL, the same target runs the Python check natively.
- The current data diff contains only the six hash values in `public_generated_experiences.json`: `source_json_hash_md5`, `source_sha256`, and `artifact_sha256` for each of the two assets. The values must equal the current-file values in the table above. No `.uasset` or source JSON diff.
- The default refresh with a fresh inventory processes both owners; the ProjectObject manifest content must be byte-identical. The final check inside the refresh passes.
- The `make generate-json` path runs the commandlet with `-all -cleanup` and captures `Saved/Logs/DefinitionGeneration.log`:

  ```powershell
  make generate-json
  ```

  The log must show `-all`, the registered types, the cleanup count, and zero failures. The experience assets should remain byte-identical because the incremental source hash and source path match; any unexpected asset rewrite or deletion is reviewed and, if legitimate, included with the corresponding manifest refresh.
- The C++ same-ID filename-only rename test passes: the package name is unchanged, `SourceJsonPath` is updated, the asset is not deleted by cleanup, and `make check-generated-json` passes.
- The deletion acceptance test passes in the disposable linked worktree: the old asset and manifest entry are gone, and the check passes.
- The ID-change acceptance test observes the manual contract: the first `make generate-json` exits nonzero with the previous package's stale source hash; the previous asset remains; the reference audit is recorded; after deliberate removal of the previous generated asset, the rerun succeeds; the previous identity is absent from the inventory/manifest, the new identity is present, and `make check-generated-json` passes. The test does not assert automatic removal or a routine one-command success.
- The fresh-inventory failure test passes: a fake editor exit 0 with no new inventory makes the PS1 fail before any manifest write. The per-run inventory path is used; the fixed stale file cannot be picked up.
- The mirror dirty preview runs without a push and gets past the generated-authority gate:

  ```powershell
  make mirror force MIRROR_DRY_RUN=1 MIRROR_EPHEMERAL_PREVIEW=1
  ```

  This is the supported local dirty-tree preview. No push occurs. The exact evidence is the absence of the generated-authority failure and the run reaching the next stage or completing. A later unrelated gate failure is reported separately.
- The ordinary clean dry run is deferred to the operator after the tooling, tests, docs, and refreshed manifest are committed:

  ```powershell
  make mirror MIRROR_DRY_RUN=1
  ```

  No push occurs. This is the reviewed-baseline check, not the implementation worktree check.
- The test registration is verified:
  - `.github/workflows/source-checks.yml` contains the explicit new Python test command.
  - `scripts/ue/package/run_release_preflight.ps1` lists the new Python test in the `Mirror` group.
  - The C++ generator rename test is registered in the internal editor suite, not public CI.
  - The new Python test runs from the same command lines as the existing mirror tests and uses synthetic fixtures only.
- Documentation checks: no non-ASCII characters in the changed docs and this todo; `git diff --check` reports no whitespace errors; the relative links in the changed docs resolve to existing files.
- The final diff contains only the intended tooling, tests, Makefile, documentation, and the two experience manifest entries. `git status` shows no unrelated paths and no staged state created by the implementation.

### Implementation evidence (2026-11-20)

- **Red**
  - `python scripts/git/mirror/tests/test_refresh_asset_authority.py -v` before implementation: 19 errors because `check` and `refresh` did not exist.
  - `python scripts/git/mirror/refresh_developer_asset_authority.py --repo-root . --check` on the pre-refresh repository: exit 1 with six failures naming both experience sources and the expected/actual hashes.
  - C++ red: the pre-fix `DefinitionGeneratorSubsystem.cpp` was restored and rebuilt; `run_single.ps1` ran the same-ID rename test and failed with `Expected ... source path ... zz_defgen_rename_test_renamed.json, but it was zz_defgen_rename_test.json` and `Cleanup must not delete the renamed source-backed asset` (log: `tmp/definition_generator_rename_test_red.log`).
  - The projection classifier tests were added with the implementation and were not run against the old function signature; the old signature would reject the source-root arguments. The original mirror failure is the recorded red for the public projection.
  - The fresh-inventory PowerShell test is a current guard; the old fixed-path behavior was not rerun as a separate red, but the code path is exactly the reviewed fixed-path reuse.

- **Green**
  - `python scripts/git/mirror/tests/test_refresh_asset_authority.py -v`: 20 tests, all passed.
  - `python scripts/git/mirror/tests/test_public_source_projection.py -v`: 24 tests, all passed.
  - `test_developer_payload.py`: 22 passed; `test_developer_dependency_audit.py`: 10 passed; `test_stage_public_world_manifests.py`: 3 passed.
  - C++ safety automation test (`ProjectIntegrationTests.DefinitionGenerator.Safety.SameIdRenameAndCleanupIsolation`): 1 discovered, 1 passed (green: `tmp/definition_generator_safety_test_green5.log`). It uses a unique temporary type, ID, source directory and content path; refuses collisions; cleans only its own asset and temp directories; and does not touch `Data/Experiences` or `Content/Experiences`.
  - Pre-fix commandlet cleanup red: restoring the old cleanup-before-count ordering failed the safety test with `Invalid JSON must not let cleanup delete the existing asset` and `Missing source directory must not let cleanup delete the existing asset`; green after restoring the fix.
  - `test_refresh_inventory_freshness.ps1`: passed with a minimal fixture `ue_path.conf` containing only `UE_PATH`; it no longer copies developer configuration. A fake editor exit 0 without a new inventory was refused, and the stale fixed inventory was not reused.
  - `-Regenerate -Owner` and `-Check -Regenerate` exit 1 with the expected refusal messages; `-Check -Owner ProjectObject` passes.
  - `make generate-json`: exit 0; `make check-generated-json`: exit 0.
  - Current experience manifest diff is exactly the six expected hash fields. The source JSON and `.uasset` files are unchanged; the ProjectObject manifest content is unchanged (the Git stat entry has no content diff).
  - Same-ID filename-only rename acceptance: `ZzAuthorityRenameTest.uasset` retained the same package, its recorded `SourceJsonPath` moved from `zz_authority_rename_test.json` to `zz_authority_rename_test_renamed.json`, cleanup did not delete it, and the manifest source path updated.
  - ID-change acceptance: the first `make generate-json` exited 2 with `Asset Registry source hash is stale: /ProjectExperienceData/Experiences/ZzAuthorityRenameTest`; the new `ZzAuthorityIdentityTest.uasset` existed and the previous asset remained. After the reference audit (temporary definition, no external references) and deliberate removal of the previous asset, the rerun exited 0; the new identity was present, the old identity absent, and the check passed.
  - Deletion acceptance: a temporary `ZzAuthorityDeleteTest` definition generated a third asset and manifest record; deleting its JSON and rerunning `make generate-json` removed the asset via `-cleanup`, returned the manifest to two assets, exited 0, and the check passed.
  - `make mirror force MIRROR_DRY_RUN=1 MIRROR_EPHEMERAL_PREVIEW=1`: exit 0 after removing the TODO's local branch/commit evidence that the private pattern rejected; no push; summary `files_changed=152 push=dry-run`. The first attempt failed only on that TODO private-pattern match. The ordinary clean `make mirror MIRROR_DRY_RUN=1` is deferred to the operator after commit because the worktree is intentionally dirty.
  - Documentation: changed docs and tests are ASCII; the changed relative link paths resolve; `git diff --check` passes. The PowerShell runners parse, and the public-source workflow lists the new Python test.
  - Cleanup safety is documented in the mirror and generator READMEs: cleanup is skipped on any generation failure or missing source directory.
  - Temporary fixtures (`ZzAuthorityRenameTest`, `ZzAuthorityIdentityTest`, the temporary JSON, and the fixed-inventory fixture) were removed; no temporary generated authority remains.

## Documentation plan

- **Authoritative stable owner:** [scripts/git/mirror/README.md](../../scripts/git/mirror/README.md). It already owns the developer release and authority refresh procedure; update it in place.
- **Router / TOC update:** `scripts/README.md` already routes to `git/mirror/README.md`; no router change. The generator and experience READMEs link to the mirror procedure rather than restating it. The Make help is the command router.
- **Content to add or change:**
  - The mirror README: add the refresh and check scripts to the file list; document `make generate-json` and `make check-generated-json`; document the direct PS1 equivalents; state the all-owner default and the `-Owner` scope; explain `-cleanup` for deletions, the same-ID filename-only rename behavior, and the generated-asset-only safety boundary; explain the ID-change manual migration and its expected authority-refresh failure, reference audit, deliberate previous-asset removal, and rerun; explain the fresh per-run inventory guarantee and the source-root error classification for stale authority versus a sanitizer/projection change; state that the mirror gate remains and that a stale authority fails before push.
  - The generator README: correct the commandlet example to `-all` (and document `-type=<TypeName>`, `-force`, and `-cleanup`); add the recorded source path to the incremental-generation triggers; document the same-ID filename-only rename behavior and the package-identity preservation; document an ID change as a manual identity migration whose first routine run creates the new asset and leaves the previous source-backed asset for a reference audit and deliberate removal; link to the mirror refresh procedure.
  - The experience README: add a short publication pointer under "Adding an experience" to the mirror refresh procedure, and state that changing `ExperienceName` is an identity migration, not an ordinary regeneration, with reference audit and deliberate removal.
  - The `Makefile` help: list `generate-json` and `check-generated-json` with one-line descriptions, using the tracked `Makefile` path.
- **Duplication avoided:** the generator and experience docs link to the mirror README for the refresh command; they do not copy the flags, hash rules, owner list, or cleanup rules. No stable doc links to this todo.

## Rollout and rollback

This is local maintainer tooling, one narrowly scoped editor-generator correction, test registration, and two tracked authority records; there is no deployment, migration, or shared-environment change. The operator reviews and commits the tooling, tests, `Makefile`, docs, generator change, and refreshed manifest together. The implementation uses the dirty `force` preview; the ordinary clean mirror dry run runs after the operator's commit. `git revert` restores the previous tooling, generator behavior, and manifest; the stale authority would then fail the mirror again, which is the correct fail-closed state. No source JSON or `.uasset` is modified by the current data fix.

## Completion criteria

- The routine one-command workflow covers an in-place JSON edit, a newly added definition, a deleted definition, and a same-ID filename-only rename: generation, cleanup, inventory export, manifest refresh, and check.
- The same-ID filename-only rename preserves the generated asset's package identity, updates its `SourceJsonPath`, and is not deleted by cleanup. The check passes.
- An ID change follows the manual identity-migration contract (A11), not the routine one-command success path: the first `make generate-json` creates the new asset and fails at the previous source-backed asset's stale hash; the reference audit is recorded; the previous asset is deliberately removed; the rerun succeeds; the previous identity is absent, the new identity is present, and the check passes. No automatic ID-change deletion or migration engine is added.
- A partial-owner operation cannot report false success: `-Regenerate` with `-Owner` is rejected; the regenerate flow refreshes and checks every declared owner.
- A structurally malformed manifest (duplicate package, wrong class or schema mapping, package-to-artifact mismatch, path escape) fails the check rather than passing until payload composition.
- `make check-generated-json` fails on the pre-fix repository with both experience sources and passes after the manifest refresh. The final check reports no stale authority.
- The current `public_generated_experiences.json` contains the six current hash values from the verified-evidence table. No source JSON or `.uasset` changes.
- `make generate-json` runs `GenerateDefinitions -all -cleanup`, refreshes all owners, and ends with a passing check. `make check-generated-json` needs no editor, no inventory, and no writes.
- A fake editor exit 0 without a fresh inventory fails before any manifest write. The stale fixed inventory is never accepted.
- The new Python test is registered in the public-source workflow and release preflight and uses synthetic fixtures. The C++ same-ID rename test is in the internal editor suite. The Python and mirror unit tests pass.
- The `Makefile` references and relative link use the tracked uppercase path and resolve on a case-sensitive checkout.
- The dirty implementation preview `make mirror force MIRROR_DRY_RUN=1 MIRROR_EPHEMERAL_PREVIEW=1` gets past the generated-authority gate without a push. The ordinary `make mirror MIRROR_DRY_RUN=1` is verified by the operator after the clean commit.
- The final diff contains no unrelated changes and no staged or committed work by the implementation agent.
- Q1 and Q2 remain non-blocking under A1 and A4. If the operator resolves either differently, the affected mirror-gate or hook task is re-scoped before implementation continues.
- Implementation evidence and the two verification deviations (linked-worktree replacement and deferred clean mirror dry run) are recorded in the test-first section and the implementation review round.
- The C++ safety test cannot delete pre-existing content, the commandlet skips cleanup on failed or unavailable generation, and the PowerShell inventory fixture is independent of private developer configuration.

## Review record

### 2026-11-20 - initial investigation

- **Trigger:** the operator's `make mirror` failed with `[FAIL] Public filtering changed generated source semantics` on `kazan_territory.json`, and the operator asked for a proper check/regenerate workflow and an implementation todo.
- **Root cause:** the tracked experience authority was stale after commit `b26ec8c62`; the documented refresh path hardcodes `ProjectObject`, so it cannot refresh `ProjectExperienceData`; no editor-free check catches the staleness before the mirror projection, and the mirror error misattributes the cause.
- **Fix:** proposed in `## Decision`; no code or data was changed in this investigation.
- **Verification:** read-only inspection of history, code, manifests, hashes, the current inventory, the pre-commit hook, validation scripts, and the `make` targets. No tests or editor commandlets were run. The working tree was clean; the todo-only revisions add or revise only this document.
- **Authority:** D1 added; Q1 and Q2 opened; A1-A4 recorded.

### 2026-11-20 - reviewer PATCH response

- **Trigger:** reviewer PATCH with required findings F1 (delete/rename cleanup), F2 (Make command), F3 (`-Owner` with `-Regenerate`), F4 (checker contract), F5 (dirty dry run), F6 (test registration), plus the mirror sanitizer diagnostic correction.
- **Root cause of the gaps:** the original plan covered the edit case but not deletion/rename; the Make surface was rejected without accounting for the operator's `make` workflow; partial-owner regeneration could report false success; the checker omitted composer structural rules; the green test used a clean-only mirror command on a dirty worktree; and the new test had no runner entry. The proposed mirror error also assumed every mismatch was stale authority, ignoring the sanitizer that runs before the projection.
- **Fix:** F1 accepted: `-Regenerate` uses `-all -cleanup` and a delete/rename acceptance test. F2 accepted in substance: thin `make generate-json` and `make check-generated-json` targets; the WSL requirement is narrowed because the editor-dependent regeneration cannot be native WSL, so WSL delegates to the Windows PowerShell/editor path while the editor-free check runs natively. F3 accepted: reject `-Owner` with `-Regenerate`. F4 accepted: the check follows the composer's generated-definition structural rules, with negative tests. F5 accepted: the implementation preview uses `make mirror force MIRROR_DRY_RUN=1 MIRROR_EPHEMERAL_PREVIEW=1`; the ordinary clean dry run is reserved for the operator after commit. F6 accepted: register the new test in the public-source workflow and the release preflight. Diagnostic correction accepted: the mirror compares the filtered source with the matching source snapshot (clean revision or `--force` worktree) and classifies stale authority versus projection/sanitizer change, without weakening the gate.
- **Refuted or narrowed:** F2's implication that the editor-dependent regeneration can be native WSL - the project's release acceptance is native Windows and the editor is Windows; the WSL target delegates rather than adding a second editor path. The public-snapshot caveat is a review limitation, not a defect: the exact hashes and clean-tree state are local HEAD evidence; the public mirror is filtered and may lag. The current mirror message is not always wrong - it is correct for a sanitizer change and wrong for a stale authority; the revised error classifies.
- **Verification:** re-read the commandlet cleanup, refresh build, payload composer, mirror clean/force/dry-run flow, sanitizer order, Makefile targets and force modifier, and the two test runners. No production code, data, or tests were changed; only this todo was revised. The working tree remains clean apart from this todo.
- **Authority:** D1 unchanged; Q1 and Q2 unchanged; A2 rejected by review evidence; A5-A8 added.

### 2026-11-20 - reviewer PATCH round 2

- **Trigger:** reviewer PATCH with F1 (same-ID filename-only rename can delete a valid asset), F2 (Makefile path case), F3 (fresh Asset Registry evidence not enforced), plus non-blocking notes on filename-vs-ID rename and synthetic public CI fixtures.
- **Root cause:** the incremental skip compared only the JSON hash and generator version, not the recorded `SourceJsonPath`. A same-directory filename-only rename kept the same package path, so it was skipped; its old source path remained; cleanup then deleted the still-valid asset. Separately, the refresh PowerShell read a fixed inventory path and checked only the editor exit code, so a failed export with exit 0 could reuse stale evidence. The TODO also used lowercase `makefile` for the tracked uppercase `Makefile`.
- **Fix:** F1 accepted as the one permitted generator C++ exception: read `SourceJsonPath` in the skip condition and refresh the existing asset's metadata in place (A9). F2 accepted: use the tracked `Makefile` spelling in the document, link, and help references. F3 accepted: use a per-run inventory path (or remove the fixed file) and require the new file before any manifest write, with a fake-editor failure test (A10). The acceptance tests now distinguish a deleted JSON, a same-ID filename-only rename, and an ID rename; the public Python test uses synthetic fixtures only; the C++/editor test is internal.
- **Refuted or narrowed:** no new reviewer claim is incorrect. F1 is confirmed in the skip branch and cleanup; F2 is confirmed by `git ls-files`; F3 is confirmed by the fixed PS1 path and exit-code-only check. The non-blocking notes are mirrored: filename-only rename preserves package identity, ID rename is a separate content change, and public CI has no `.uasset` payload. The previous F2 WSL narrowing remains.
- **Verification:** re-read the incremental skip and cleanup code, `SourceJsonPath` metadata write, the refresh PS1 inventory handling, the exporter output override, the tracked `Makefile` path, and the public CI test runners. No production code, data, or tests were changed; only this todo was revised.
- **Authority:** D1 unchanged; Q1 and Q2 unchanged; A6 superseded in part by A9; A9 and A10 added.

### 2026-11-20 - reviewer PATCH round 3

- **Trigger:** reviewer PATCH on the ID-change acceptance contract after the previous findings were resolved. The reviewer observed that the completion criteria still implied the routine `make generate-json` completes an ID change.
- **Root cause:** the generator finds an existing asset by ID under the generated content path. Changing the ID while keeping the JSON filename means the new ID is not found; the generator creates a new asset and leaves the previous asset in place. The previous asset's `SourceJsonPath` still exists, so cleanup keeps it. The inventory includes the previous asset with its old `SourceJsonHash`; the authority refresh rejects it as stale. The command therefore fails after creating the new asset, rather than completing the identity change.
- **Fix:** A11 added. An ID change is a manual identity migration, not a routine regeneration. The contract is: change the ID; run `make generate-json` and expect the authority refresh to fail on the previous package's stale hash; audit all references; deliberately retire/remove the previous generated asset through the editor; rerun and verify the old identity is absent and the new identity is present. No automatic deletion or generic ID-migration engine is added. The completion criteria, acceptance tests, and documentation now state the expected failure and manual migration explicitly.
- **Refuted or narrowed:** no reviewer point is incorrect this round. The reviewer's ID-change sequence is confirmed in `DefinitionAssetManager::FindExistingAsset`/`CreateNewAsset`, the path-based cleanup, the inventory exporter, and the refresh `build` stale-hash check. The previous TODO implication that the routine command completes an ID change is explicitly refuted by the code and removed from the completion criteria.
- **Verification:** re-read `DefinitionAssetManager.cpp` (`FindExistingAsset`, `CreateNewAsset`), `DefinitionGeneratorSubsystem.cpp` (skip, cleanup), `export_generated_asset_inventory.py`, and `refresh_developer_asset_authority.py` (`Asset Registry source hash is stale`). No production code, data, or tests were changed; only this todo was revised.
- **Authority:** D1, Q1, and Q2 unchanged; A11 added; A9 and A10 unchanged.

### 2026-11-20 - implementation

- **Trigger:** operator invoked implement-approved-change for the revised todo.
- **Implemented:** C++ incremental-skip `SourceJsonPath` check; editor-free Python check and all-owner refresh with structural/freshness verification; PowerShell `-Check`/`-Regenerate`/`-Owner` rules, `-all -cleanup`, per-run inventory, and no-editor check path; thin Make targets; mirror source-snapshot diagnosis; new Python, PowerShell, and internal C++ tests; current experience manifest; mirror/generator/experience docs and Make help.
- **Verification:** the implementation evidence section records the red/green commands, pass counts, same-ID rename, ID-change failure/migration, current manifest diff, and mirror dirty preview. The pre-fix C++ source was rebuilt and the new test failed for the expected source-path/cleanup reasons, then passed after restoring the fix.
- **Deviations / limits:** the linked-worktree acceptance was replaced by the internal C++ automation test plus a cleaned temporary fixture in the main worktree; the clean `make mirror MIRROR_DRY_RUN=1` remains the operator's post-commit step; the TODO's local branch/commit evidence was removed after the private mirror pattern rejected it, and the final dirty preview passed. The projection classifier and PS1 freshness tests were added with the implementation; their old-code reds were code-path evidence rather than separate pre-fix runs.
- **Authority:** D2 added; D1's no-implementation clause superseded in part; Q1/A1 and Q2/A4 defaults apply; A2 rejected; A5-A11 active. No commit, stage, push, or publication was performed.

### 2026-11-20 - reviewer PATCH round 4 (test and failure safety)

- **Trigger:** reviewer PATCH for three localized safety findings: the C++ test used fixed production paths and unconditional deletion; the PowerShell fixture copied the developer's `ue_path.conf`; and the commandlet ran orphan cleanup before counting generation failures.
- **Root cause:** the first C++ regression test proved the rename behavior but was not isolated from authored content. The inventory fixture depended on a private config file. `GenerateType` cleaned before knowing whether generation had failed or the source directory was available, so an invalid renamed JSON or missing source could delete a previously valid generated asset.
- **Fix:** the C++ safety test now registers a unique temporary definition type based on the real experience type info, uses unique temporary source/content directories and an ID, refuses collisions, cleans only assets and directories it created, and unregisters the test type. The commandlet counts results and validates the source directory before cleanup; cleanup runs only after a clean generation. The PowerShell fixture writes a minimal `ue_path.conf` with only `UE_PATH`. The mirror and generator READMEs document the cleanup safety.
- **Refuted or narrowed:** no reviewer point is incorrect. All three findings reproduced in the local code. The earlier test's fixed-path risk was real; the fixed test now leaves `Data/Experiences` and `Content/Experiences` untouched. The commandlet red was reproduced by restoring the old cleanup-before-count ordering: the safety test failed on invalid JSON and missing-source asset deletion, then passed after restoring the fix.
- **Verification:** the isolated C++ safety test passes (green `tmp/definition_generator_safety_test_green5.log`); the pre-fix red log is `tmp/definition_generator_safety_test_red.log`; the PowerShell inventory test passes with the minimal config; `make generate-json` and `make check-generated-json` pass after the commandlet change; no temporary safety-test or authority fixtures remain.
- **Authority:** D2 unchanged; Q1/A1 and Q2/A4 unchanged. No new operator decision was required for the localized safety corrections.
