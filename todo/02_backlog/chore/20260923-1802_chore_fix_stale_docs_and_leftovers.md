# Fix Stale Docs, Routes, And Dead Leftovers Found In The 3.0.0 Investigation

**Status:** REVIEW REQUIRED - investigated 2026-09-23; backlog, not selected; no implementation authorized  
**Scope:** documentation, agent-routing, and governance statements that contradict the repository or carry personal paths; four dead or misleading leftovers found beside them (a config profile, a reflected stub class, a misnamed receipt field, duplicated host-script helpers); and two content folders named for work state  
**Stable documentation owner:** each item names its own owner below; this task owns none of their facts

Agent route: read this file, then only the owner file each item names. Items are independent:
implement and verify them one at a time, in the order of `## Implementation tasks`. Reviewers
start with `## Reviewer brief`.

## Current state and next steps

State on 2026-09-23: investigated; backlog, not selected; operator decisions D1-D7; non-author
architect verdicts recorded for all six oversized files. No code written.

| Step | Runs | Verified by | Starts when |
|---|---|---|---|
| Verdicts for the two Editor files (G-1 table) | complete | two SPLIT verdicts recorded with exact seams | done 2026-09-23 |
| R1 review of the whole task | fresh `architect` subagent, plus web research for the `## Reviewer brief` items | findings applied here | now |
| Items A to L | `implement-approved-change`, one item at a time | the item's line in `## Verification plan` | operator selects this task |

## Contents

- Goal - Authority register - Non-goals
- Verified evidence (groups A routing, B World docs, C resource and config docs, N names,
  G governance, L leftovers) - Refuted
- Current owners - Problem
- Decision (per-item fixes, premise / KISS gate, alternatives)
- Required invariants - Implementation tasks - Verification plan
- Reviewer brief - Completion criteria - Review record

## Goal

Every statement an agent or developer reads about these areas matches the repository, and the dead
leftovers found beside them are removed or explicitly kept, so no one plans work against a false
fact or couples new work to a stub.

## Authority register

Binding `D<n>` entries outrank `## Decision`, the KISS gate, reviewer findings, and agent
preference within their scope. Cite IDs elsewhere; never restate or renumber them.

### Operator decisions

- **D1** One new task covers every outside-scope finding of the 2026-09-23 3.0.0 investigation.
  Operator's words: "outside scope create new todo for all with /investigate-change".
  - Effect: the findings are grouped here, each with its own owner and acceptance. A finding that
    another task already owns is named in prose, not duplicated (`todo/README.md`).
  - Reason: not stated.
  - Date/source: 2026-09-23, this session.
- **D2** Do not couple to existing stubs; keep the right separation-of-concerns naming hierarchy
  and dependency direction; build future-proof, data-driven, fully modular, black-boxed,
  decoupled, and component-driven. Operator's words: "we don't have fully architecture for such
  solutions so no need couple for existing stubs, we need keep the right SOC names hierarchy and
  vector and future proof oriented build with our data driven design fully modular blackboxed
  decoupled component driven".
  - Effect: dead stubs are removed rather than documented, and names say what things are (L-2,
    L-3).
  - Reason: not stated beyond the words above.
  - Date/source: 2026-09-23, this session.
- **D3** Even though Mesh Terrain is Experimental, it is the long-term direction: after the test
  passes, the architecture wraps fully around it, with no legacy support and no legacy mentions.
  Operator's words: "even experimental it future long going goal so we will wrap around it our
  architecture after test fully without any legacy support and mentions".
  - Effect here: the Landscape-specific World doc fixes (B-1 to B-3) are minimal current-truth
    corrections; if the Mesh Terrain cutover rewrites a section first, that item closes as
    superseded. No fix adds a legacy mention.
  - Reason: "future long going goal".
  - Date/source: 2026-09-23, this session.
- **D4** Every finding lands in a task, and where a finding has open questions or possibly better
  paths, the reviewer is asked to investigate carefully, on the web and against ALIS goals and
  standards. Operator's words: "ensure that all finding are in todos and if some has questions or
  maybe better pathes - propose reviewer to investigate it carefully via web and our goals and
  standarts".
  - Effect: `## Reviewer brief`.
  - Date/source: 2026-09-23, this session.
- **D5** "Release plan" is the right name for release routers, because each is the main document of
  a release. Operator's words: "release plan is good name because it like main doc for release".
  - Effect: closes Q2. A-2 documents the `00_release_X.Y.Z.md` exception in `todo/README.md`.
  - Date/source: 2026-09-23, this session.
- **D6** Run the `architect` skill on the files that crossed 1000 lines, to learn whether each shows
  real design or architecture smells (then split) or is fine for its entity (then admit it with a
  comment saying why). Operator's words: "we need run /architect skill to understand if it will
  trigger proper sowtware desing and architecture smells or properly comment that this fine for
  such entities".
  - Effect: closes Q1. G-1 follows the per-file verdicts recorded under "G-1 architect verdicts".
  - Date/source: 2026-09-23, this session.
- **D7** City17 is a legacy demo; once City17 is restored inside the fully rebuilt Kazan with
  gameplay, the legacy City17 content is removed outright. Operator's words: "city 17 it's just demo
  legacy, when we will rebuilt terrain and etc and restore city 17 in the full rebuilded Kazan with
  game play we will remove all just plainly".
  - Effect: in N-1, assets used only by City17 are not moved; they leave with City17. Only assets
    that other content also uses move to concern-named homes now.
  - Date/source: 2026-09-23, this session.

### Operator gates

- **Q1 [CLOSED by D6]:** The mega-file baseline "MUST NOT grow", yet three first-party
  `.cpp` files now exceed 1000 lines outside the table, and one `.py` file exceeds it outside the
  check (G-1). Admit them to the baseline, each with a matching refactor task as `AGENTS.md`
  requires, or split them before the table is updated? - gates: G-1.
- **Q2 [CLOSED by D5]:** Should `todo/README.md` document the `00_release_X.Y.Z.md`
  release-router exception, or should release routers follow the standard task name? - default
  while open: A2.

### Working assumptions

- **A1 [ACTIVE]:** This task sits in `todo/02_backlog/chore/`: it is actionable but not selected,
  and the current focus is 3.0.0. The operator may move it.
- **A2 [RESOLVED by D5]:** Documenting the release-router exception is smaller than renaming: three releases
  already use `00_release_X.Y.Z.md` in `00_current/`, sorted first, and move to `01_done/build/`.
- **A3 [ACTIVE]:** Dead stubs with no asset, config, or code consumer are removed in one pass with
  no redirects, per the
  [public repo migration policy](../../../docs/agents/canonical.md#public-repo-migration-policy).
- **A4 [ACTIVE]:** The never-written "Generated Roads" Landscape edit layer is documented as it is,
  not removed here. Removing it changes generated Landscape packages and needs map-scope
  regeneration and L3 enrollment; the Mesh Terrain cutover deletes the edit-layer machinery. If
  Mesh Terrain is rejected, removal becomes its own decision then.
- **A5 [ACTIVE]:** The duplicated host-script helpers (L-4) change only if a consumer is shown to
  hash the Material host JSON files. Otherwise the item closes with no code change.
- **A6 [ACTIVE]:** Under D6, a file admitted as cohesive carries its justification and split
  trigger in the table instead of a refactor task; G-1 updates the `AGENTS.md` baseline wording to
  distinguish admitted rows from debt rows.

## Non-goals

Items owned elsewhere, named here so they are not lost:

- The mirror manifest path defect, the hard-coded 2.0.0 release highlights, and the stale 2.0.0
  default version belong to R4 in the [3.0.0 release router](../../00_current/00_release_3.0.0.md).
- The Linux acceptance route mismatch (WSL-only receipts in code against the native-host decision)
  and the hard-coded "tested ... under WSL2/WSLg" README line belong to the
  [parked Linux task](../../03_parked/build/20260917-1719_release_add_linux_player_support.md).
- The ProjectMaterial manifest CRLF defect and the `Generated` content statements in
  `docs/data/structure.md` belong to the
  [Terrain Material v2 task](../../01_done/content/20260923-1620_content_generate_terrain_material_v2.md).
- Landscape-specific consumers and the edit-layer machinery belong to the
  [Mesh Terrain migration task](../../01_done/world/20260923-1620_world_migrate_terrain_to_mesh_terrain.md).

Also out of scope: rewriting whole documents, behaviour changes beyond L-1 to L-4, and any change to
generated content. A stale local test asset,
`Plugins/World/ProjectWorldTestData/Content/Generated/Presentation/MI_ProjectWorldTerrain_synthetic_representative_v1.uasset`,
is git-ignored transient output with no referencer, not a repository finding; it may be deleted
locally at any time.

## Verified evidence

Read at HEAD `81a3b60a4`; the only working-tree change is the new, untracked task files.

### A - agent routing and task conventions

- **A-1** `.claude/skills/world-engineering/SKILL.md:55` (tracked in Git) lists
  `architecture_overview.md` among typical deeper sources of truth. No such file exists anywhere in
  the repository; the ProjectWorld architecture owner is
  `Plugins/World/ProjectWorld/docs/architecture/README.md`, which routes to `structure.md`.
- **A-2** `todo/README.md:32` requires `YYYYMMDD-HHMM_topic_verb_noun.md` for new tasks, while
  release routers have used `00_release_X.Y.Z.md` for three releases
  (`todo/current/00_release_1.0.0.md`, `todo/00_current/00_release_1.0.0.md`,
  `todo/00_current/00_release_2.0.0.md`, now `todo/01_done/build/00_release_2.0.0.md`, and
  `todo/00_current/00_release_3.0.0.md`). The README does not mention the exception.
- **A-3** `docs/agents/canonical.md:96` routes readers to a machine-local memory file under one
  developer's user-profile directory, outside the repository. Public docs must use portable paths
  and carry no profile paths (global agent instructions). The durable fact already lives in
  `docs/testing/agent_ue_inspection.md:353` ("VIEWMODEL_PROPERTY Reflection Limitation"), which the
  same canonical.md bullet also cites.

### B - World documentation truth

- **B-1** `Plugins/World/ProjectWorld/docs/world_partition.md:340-343` ("Landscape keeps its native
  component and World Partition proxy streaming and builds its Nanite representation") and
  `territory_contract.md:1250-1252` ("Landscape keeps its component/proxy streaming contract and
  builds its Nanite representation") are false: no ProjectWorld code sets `bEnableNanite`, the UE
  5.8 default is `false` (`Engine/Source/Runtime/Landscape/Classes/LandscapeProxy.h:491`), and no
  saved Kazan or Manhattan proxy contains a Nanite component. `world_partition.md:72` (an Epic fact
  about Nanite Landscape memory) and `:387` (a link) stay.
- **B-2** `world_partition.md:266-268` says "a water-only input change dirties no Landscape
  package". The next paragraph of the same file says `-ProvePackageLocality` proves "a Water
  semantic change rewrites exactly its hydrologically affected Landscape proxies", and
  `territory_contract.md:456-461` and `HashTerrainWaterCellInput`
  (`ProjectWorldLayerInventory.cpp:69-138`) make Water part of terrain cell identity.
- **B-3** `world_partition.md:252-256` lists "generated road/deformation passes" among the Landscape
  edit-layer uses. The "Generated Roads" layer is created
  (`ProjectWorldLandscapeRealization.cpp:30`, `:332-338`) and looked up again only on Delete
  (`:911`); nothing writes it, and no road deformation pass exists.
- **B-4** `tools/World/EndToEndValidation/README.md:136` says commandlet realization "runs with
  `-NullRHI`". `scripts/ue/world/execution_envelope.ps1` marks render-required steps
  "-NullRHI FORBIDDEN", and `realize_canonical_world.ps1` runs the realizer with
  `Rendering = 'Required'`. The conclusion of that README sentence (commandlet realization is
  structural evidence, not rendered performance evidence) still holds; its reason is wrong.
- **B-5** `territory_contract.md` "Manifest authority location" lists the allowed root entries
  (`active_set.json`, `scopes/`, `archive/`, `journal.json`) under "unknown entries are rejected",
  while `scripts/ue/world/generated_manifest.ps1` also admits `authority.lock` (the authority lock,
  `Enter-ProjectWorldAuthorityLock`, `:69-83`).

### C - resource and configuration documentation truth

- **C-1** `Plugins/Resources/ProjectObject/README.md:36-41` and `:84-90` show a `Content/Template/`
  folder ("Man-made object templates", "Buildings, furniture, doors"). The real content folders are
  `Animal`, `Environment`, `Human`, `HumanMade`, `LootProfiles`, and `Nature`. The `Source/.../Template/`
  entries (`:44-55`) are correct.
- **C-2** `Plugins/Editor/ProjectDefinitionGenerator/README.md:186-193` shows
  `Data/Objects/Template/door.json -> /ProjectObject/Objects/Template/door.uasset`. The real object
  generator config (`Plugins/Resources/ProjectObject/Data/Schemas/object.schema.json`,
  `x-alis-generator`) uses `SourceSubDir: "../Content"` and `GeneratedContentPath: "/ProjectObject"`:
  each JSON lives under `Content/` and its asset is generated beside it.
- **C-3** `docs/data/structure.md:13` gives `Plugins/Resources/ProjectObject/Data/Objects/Sword.json`
  as the object example; objects live under `Content/`.
- **C-4** `docs/config/render/render.md` (357 lines) is a copy of `Config/DefaultEngine.ini`
  (446 lines) that differs in 133 non-blank lines, including the D3D12 PSO cache, Mutable, and TSR
  settings with their rationale comments. Yet `docs/config/render/README.md` routes to it as the
  "Render Overview", and `lighting-setup-editor.md:22`, `:92` and `lighting-fix-summary.md:188`
  describe it as synchronized with `DefaultEngine.ini`. Configuration owns configurable values;
  the rationale already lives in `DefaultEngine.ini` comments.
- **C-5** `scripts/ue/editor/blueprint/docs/EXAMPLE_SESSION.md:19` and `QUICKSTART.md:15-16`,
  `:37` hard-code another developer's personal checkout path under a user-profile directory. The
  examples break on every other machine, and the repository must not carry profile paths.

### N - names that describe work state instead of concern

- **N-1** `Plugins/Resources/ProjectMaterial/Content/TODO/` holds 27 assets, legacy masters that
  City17 uses (for example `Base_Mat`, `Atlas_Mat`, `Glass/`, `Physic/`), and
  `Plugins/Resources/ProjectTexture/Content/ToDo/` holds 3 (`RMA`, `T_Dummy_BC`, `mask`). The folder
  names record a work state, not what the assets are (D2).

### G - governance

- **G-1** The mega-file baseline in
  [canonical.md section 10](../../../docs/agents/canonical.md#10-mega-file-baseline--file-size-guardrail)
  was regenerated 2026-04-24. Its own check command now reports:
  - baseline rows with changed counts: `MindServiceImpl.cpp` 1739 (table 1738),
    `ProjectInventoryComponent.cpp` 1664 (1663), `InventoryViewModel.cpp` 1657 (1656),
    `OrchestratorCoreModule.cpp` 1594 (1598), `ObjectSpawnUtility.cpp` 1498 (1766),
    `W_InventoryPanel.cpp` 1349 (1339), `LootContainerCapabilityComponent.cpp` 1258 (1256);
  - new files over 1000 lines, absent from the table:
    `Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRealizationService.cpp`
    (1132), `Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent_Mutation.cpp`
    (1043), `Plugins/World/ProjectWorld/Source/ProjectWorld/Private/Presentation/ProjectWorldProductRouteGate.cpp`
    (1015).
  - `AGENTS.md:246` applies the guardrail to `.cpp`, `.h`, `.ps1`, and `.py`, but the check command
    lists only `.cpp` and `.h`. Among first-party `.ps1` and `.py` files, only
    `scripts/ue/package/prepare_release.py` (1030) exceeds 1000 lines.
  - The check also skips `Plugins/Editor`, where two first-party files exceed 1000:
    `ProjectCinematic/.../ProjectCinematicSubsystem.cpp` (1335) and
    `ProjectPlacementEditor/.../Widgets/SProjectObjectBrowser.cpp` (1077). Rust files are outside
    the rule's wording.

#### G-1 architect verdicts (D6; independent review, 2026-09-23)

| File | Lines | Verdict | Action and owner |
|---|---:|---|---|
| `ProjectWorldRealizationService.cpp` | 879 after World locality refactor | RESOLVED | The layer producer split and retired procedural path reduced this file below the guardrail. No pure-move split remains in G-1. Recheck the final line count when G-1 runs. |
| `ProjectInventoryComponent_Mutation.cpp` | 1043 | SPLIT | Here: pure move of equipment handling into `ProjectInventoryComponent_Equipment.cpp` (revoke pair L74-194, plus equip/unequip from `ProjectInventoryComponent.cpp`, whose row shrinks). |
| `ProjectWorldProductRouteGate.cpp` | 1015 | ADMIT | Notes: "Admitted as cohesive: single-scenario packaged product-route driver over shared route state; stateless parts already live in ProjectWorldProductRouteProgress, -Collision, PresentationSampling, RuntimeScreenshotCapture. Split trigger: collision probes move to ProjectWorldProductRouteCollision when terrain identification changes." |
| `prepare_release.py` | 1030 | SPLIT | R4 of the 3.0.0 release plan, as its first behavior-free step: the manifest contract (L709-864) moves to `release_manifest.py`. |
| `ProjectCinematicSubsystem.cpp` | 1335 | SPLIT | Pure move `AssignDirectorClass`, `StampPanelEvents`, `StampHideOriginals`, `StampFocusHighlights`, `EnsureCameraCutTrack`, and their private `ResolveThunk` helper (current L817-1335 plus helper) to `ProjectCinematicSubsystem_SequenceStamping.cpp`. Recording lifecycle/source capture stays in the original file, which falls below 1000 lines. |
| `SProjectObjectBrowser.cpp` | 1077 | SPLIT | Pure move the asset-action surface (current L755-1077: double-click, spawn/transform, and context-menu regeneration/browse/copy actions) to `SProjectObjectBrowser_Actions.cpp`. Construction, discovery, filtering, and thumbnail refresh stay together and fall below 1000 lines. |

### L - dead or misleading leftovers

- **L-1** `Config/DefaultEngine.ini:360` defines a `WaterBodyCollision` collision profile, "Created
  by Water Plugin". The Water plugin is not enabled (`Alis.uproject` has no Water entry), and a
  byte search of every `.uasset` and `.umap` under `Content/` and `Plugins/` finds no reference;
  neither does project config or code.
- **L-2** `UProjectWorldManifest` (a `UPrimaryDataAsset`,
  `Plugins/World/ProjectWorld/Source/ProjectWorld/Public/ProjectWorldManifest.h`, `.cpp`),
  `FProjectWorldRegionDescriptor`, and `EProjectWorldLayerType` (`ProjectManifestTypes.h:13`, `:28`,
  `:50`) have no asset of the class (byte search), no config registration, and no code consumer.
  The only mentions are a comment in `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Services/ILoadingService.h:74`
  and the "World Manifest Design" section of `world_partition.md`, which still describes it,
  including "PartitionMetadata: Future hook".
- **L-3** The ProjectMaterial generation receipt field `authentication_sha256`
  (`ProjectMaterialGenerateCommandlet.cpp:56`, checked at
  `scripts/ue/material/run_material_generation.ps1:288`) is a plain SHA-256 of
  `operation|status|manifest|generated|skipped` with no secret: a consistency hash, not
  authentication.
- **L-4** The Material host wrapper duplicates World helpers with different behaviour:
  `Get-FileSha256OrNone` (`run_material_generation.ps1:55`), `Restore-MaterialSnapshot` (`:116`),
  and `Write-JsonAtomic` (`:153`, `Set-Content -Encoding UTF8`, which writes a BOM under Windows
  PowerShell 5.1) against `Get-ProjectWorldFileSha256` (`generated_manifest.ps1:127`),
  `Write-ProjectWorldJson` (`:462`, LF with no BOM, because "These documents are hashed on disk and
  the recorded hashes ARE the activation authority"), and the World journal functions. Both
  wrappers already share one lock owner, `scripts/ue/generated_content/`.
- **L-5** The Inventory rule "which containers an equipped item grants" exists in three copies that
  already differ for malformed grant data: `GetEffectiveContainers`
  (`ProjectInventoryComponent_Containers.cpp:101-134`) and the drop-revoke check
  (`_Mutation.cpp:99-123`) use item grants whenever any exist, while `Internal_UnequipItem`
  (`ProjectInventoryComponent.cpp:1558-1588`) requires a valid grant ID and otherwise falls back to
  slot grants. Dropping an equipped item has no automated test, and the Inventory pitfalls entry
  "Drop From Equip Slot Leaves Granted Container In UI" still describes one revoke before the spawn,
  while the code now checks before the spawn and commits after it.

### Refuted

- "The engine or Water plugin still needs `WaterBodyCollision`" - no asset, config, or code
  reference exists (L-1). Recheck after any Water plugin adoption.

## Current owners

| Item | Owner of the fact |
|---|---|
| A-1 | `.claude/skills/world-engineering/SKILL.md` |
| A-2 | `todo/README.md` |
| A-3 | `docs/agents/canonical.md` section 3; the fact itself is owned by `docs/testing/agent_ue_inspection.md` |
| B-1 to B-3, L-2 doc | `Plugins/World/ProjectWorld/docs/world_partition.md`, `territory_contract.md` |
| B-4 | `tools/World/EndToEndValidation/README.md` |
| B-5 | `territory_contract.md` "Manifest authority location" |
| C-1 | `Plugins/Resources/ProjectObject/README.md` |
| C-2 | `Plugins/Editor/ProjectDefinitionGenerator/README.md` |
| C-3 | `docs/data/structure.md` |
| C-4 | `Config/DefaultEngine.ini` (values and rationale); `docs/config/render/README.md` (routing) |
| C-5 | `scripts/ue/editor/blueprint/docs/` |
| N-1 | `Plugins/Resources/ProjectMaterial/Content/`, `Plugins/Resources/ProjectTexture/Content/` |
| G-1 | `AGENTS.md` (enforcement) and `docs/agents/canonical.md` section 10 (baseline table) |
| L-1 | `Config/DefaultEngine.ini` |
| L-2 | `Plugins/World/ProjectWorld/Source/ProjectWorld/` |
| L-3, L-4 | `Plugins/Resources/ProjectMaterial/` and `scripts/ue/material/` |

## Problem

Documents and instructions drifted from the code they describe (A, B, C, G), two public docs carry
personal user-profile paths (A-3, C-5), two content folders are named for work state (N), and
three leftovers plus one duplication survive from earlier designs (L). Each drift misleads a planning agent: B-1
misstated the Mesh Terrain comparison baseline, A-1 sends agents to a missing file, and G-1 lets the
file-size guardrail miss files it claims to cover.

## Decision

### Per-item fixes

- **A-1** Replace the missing name with the architecture router, `docs/architecture/README.md`,
  keeping the skill generic.
- **A-2** Add one line to `todo/README.md` Naming for release routers: `00_release_X.Y.Z.md`, the
  main document of a release (D5).
- **A-3** Remove the profile-path reference and keep the repository link to
  `docs/testing/agent_ue_inspection.md`.
- **B-1** State current truth: the generated Landscape does not enable Nanite.
- **B-2** Replace the sentence with the contract's truth: a Water change dirties only the
  hydrologically affected Landscape cells.
- **B-3** State that "Generated Roads" is created without a producer; no road pass exists (A4).
- **B-4** Keep the conclusion and correct the reason: the realizer runs with rendering required and
  is still not packaged-route evidence.
- **B-5** Add `authority.lock` to the layout list.
- **C-1, C-2, C-3** Replace the stale folders and paths with the real ones.
- **C-4** Delete `render.md`; route `docs/config/render/README.md` and the lighting docs to
  `Config/DefaultEngine.ini`. Historical lines in the lighting changelog stay history; only its
  broken link changes.
- **C-5** Replace the personal checkout path with a repository-relative path resolved from the
  project root.
- **N-1** Inventory every referencer of the 30 assets. Assets used only by City17 stay until City17
  is removed (D7). Assets that other content also uses move to concern-named homes (for example by
  material family) in one pass with reference fix-up and resave, with no redirects (A3).
- **G-1** Apply the verdict table: the remaining Inventory split as a pure move; admitted rows marked
  "admitted as cohesive" with their split trigger (A6); then replace canonical.md's check command
  with one that covers every file type and category `AGENTS.md` names, and regenerate the table
  from it:

  ```bash
  git ls-files -- 'Plugins/*.cpp' 'Plugins/*.h' 'Source/*.cpp' 'Source/*.h' \
                  'scripts/*.ps1' 'scripts/*.py' 'tools/*.ps1' 'tools/*.py' \
    | grep -v -E '^Plugins/(Local|InstanceArrayTool|ThirdParty|Test)/|/Tests/|Tests\.cpp$|\.Tests\.ps1$|/tests/|\.gen\.' \
    | xargs wc -l | awk '$1 >= 1000 && $2 != "total"' | sort -rn
  ```

  Keep the moved `WriteResult` file out of every producer fingerprint list (add it to
  `$catalogPaths` in `scripts/ue/world/test/generator_fingerprint.Tests.ps1`); otherwise the active
  layer fingerprints stop matching and whole layers regenerate.
- **L-1** Delete the profile line.
- **L-2** Delete the class, its types, the `world_partition.md` section, and the
  `ILoadingService.h` comment in one pass (A3).
- **L-3** Rename the field to what it is (for example `receipt_sha256`) at both sites.
- **L-4** Prove whether any consumer hashes the Material host JSON. If none, close with no change;
  if one does, move the helpers to one owner under `scripts/ue/generated_content/` (A5).
- **L-5** First add an equip -> drop -> revoke integration test with a known-bad control (skip the
  commit, watch the granted container stay); then collapse the three copies into one private helper
  beside `GetEffectiveContainers`; fix the pitfalls entry. This is a behavior change, kept apart
  from G-1's pure move.

### Premise / KISS gate

Every fix edits the existing owner of the fact; no document or helper is created, except that L-4
may move helpers into the existing shared lock owner, and N-1 moves assets only within their owning
plugins. Deleting beats documenting a stub (D2, A3).
Knowingly given up: the stale `render.md` snapshot and the unused manifest class, which nobody
consumes.

### Alternatives considered

- **Leave the World doc fixes to the Mesh Terrain cutover** - rejected: the statements are false
  today and already misled planning (B-1); D3 closes an item as superseded only if the cutover lands
  first.
- **Keep `render.md` and resynchronize it** - rejected: a second copy of configuration drifts again.
- **Document `UProjectWorldManifest` as a future hook** - rejected by D2 and the public repo
  migration policy.

## Required invariants

1. Each corrected statement matches the cited code or config at the implementation commit.
2. No new document and no second copy of any fact.
3. Current truth only: no "previously" narration and no legacy mentions (D3).
4. Stable docs, code, tests, and configuration never link to a task file.
5. L-1 to L-3 and N-1 keep the editor build, startup, and the cited tests green. A deletion happens
   only after a byte search proves zero references at that commit, and a move leaves no broken
   reference and no redirector.
6. After G-1, the baseline table equals the check command's output exactly.
7. ASCII only, per `AGENTS.md`.
8. No tracked document carries a user-profile or machine-local personal path.

## Implementation tasks

- [ ] A-1, A-3, and A-2.
- [ ] B-1 to B-5 (close any section the Mesh Terrain cutover already rewrote as superseded).
- [ ] C-1 to C-5.
- [ ] N-1: referencer inventory first; move only the assets that non-City17 content uses (D7).
- [ ] L-1: delete the profile, then verify startup and cook.
- [ ] L-2: re-run the byte search, delete in one pass, build, and run the ProjectWorld and loading
      tests that load the module.
- [ ] L-3: rename at both sites and update any test expectation.
- [ ] L-4: prove or refute the hashing consumer, then act per A5.
- [ ] G-1: the remaining Inventory pure-move split, the admitted row, the new check command, the regenerated table. The World row was resolved by the World locality refactor.
- [ ] L-5: test first, then unify the grant rule.
- [ ] Final diff review against this task.

## Verification plan

- **Docs (A, B, C, G):** grep proves each false statement is gone and each replacement matches its
  cited source; relative links resolve; `scripts/ue/check/governance/validate_text_format.bat`
  passes (ASCII and character set).
- **A-3, C-5:** a repository-wide grep finds no user-profile path in tracked docs.
- **N-1:** the referencer inventory before and after shows no broken reference; City17 boots
  through `scripts/ue/test/smoke/boot_test.bat` and cooks with no missing-asset warnings.
- **L-1:** `scripts/ue/test/smoke/boot_test.bat` and a Shipping cook with no collision-profile
  warnings.
- **L-2:** `scripts/ue/build/build.bat AlisEditor Win64 Development`, then exact tests through
  `scripts/ue/test/unit/iterate.ps1 -TestFilter <exact name>` for the ProjectWorld runtime module
  (for example `Project.World.Realization.Runtime.NoHLODPartitionPolicy`) and the loading path.
- **L-3:** `scripts/ue/material/test/run_material_generation.Tests.ps1` and
  `Project.Material.Generation.ProcessOwnership`.
- **G-1:** editor build; `generator_fingerprint.Tests.ps1`; one `-Mode Validate` realization whose
  result file is byte-identical to one written before the move except `duration_seconds`; the
  Inventory equip and unequip tests; the new check command's output equals the table.
- **L-5:** the new drop test fails under its known-bad control, then passes.

## Reviewer brief

Review with the `architect` skill and the review contract. Register entries bind; challenge them
only with evidence. Judge against [VISION.md](../../../VISION.md), `AGENTS.md`,
[canonical.md](../../../docs/agents/canonical.md), and the documentation rules the repository
follows (one owner per fact, current truth only, routing through READMEs). Investigate carefully,
including on the web where a fact is external:

- **C-4:** confirm nothing in `render.md` is missing from `DefaultEngine.ini` comments or other
  docs before deletion, and check Epic's config-hierarchy docs for whether any listed value belongs
  in a platform or scalability ini instead.
- **G-1:** recheck the recorded architect verdicts against the files at implementation time.
- **L-1:** confirm from Epic's Water plugin source or docs that the profile is created only by that
  plugin and that nothing reads it while Water is disabled.
- **L-2:** read the Git history of `ProjectWorldManifest.*` and the ProjectLoading architecture
  docs for a planned consumer before agreeing to delete.
- **L-4:** decide whether a shared host-helper owner is warranted, or whether the duplication is
  harmless.
- **N-1:** recommend concern names consistent with the ProjectMaterial and ProjectObject content
  hierarchies, check which legacy masters are still referenced at all (unreferenced ones may be
  deleted instead of moved), and check Epic's guidance on bulk asset moves without redirectors.
- Any better path than the per-item fixes above, with evidence.

## Completion criteria

`PASS` requires every item to be fixed and verified, or closed as superseded (D3) or as "no change"
(A5), with evidence; G-1 done per the architect verdicts; stable docs stating current truth; and
nothing else changed.

## Review record

### 2026-09-23 - initial investigation

- **Trigger:** "outside scope create new todo for all with /investigate-change", then "ensure that
  all finding are in todos ...".
- **Root cause:** documentation, routing, and governance drift, and leftovers from earlier designs,
  found while investigating ALIS 3.0.0.
- **Fix:** this task.
- **Verification:** read-only. Commands: `git ls-files`, grep and byte searches over docs, code,
  config, `.uasset`, and `.umap`; the canonical.md mega-file check plus a `.ps1`/`.py` variant; a
  `diff` of `render.md` against `DefaultEngine.ini`; a grep for user-profile paths in tracked docs;
  asset counts of the two work-state folders. No build or test ran.
- **Authority:** D1-D4 recorded; Q1-Q2 opened; A1-A5 active.

### 2026-09-23 - G-1 non-author architect completion

- **Trigger:** the handoff left the two Editor mega-files without verdicts.
- **Finding:** `ProjectCinematicSubsystem.cpp` combines recording lifecycle with sequence stamping;
  `SProjectObjectBrowser.cpp` combines browser/filter lifecycle with asset actions. Each has a
  behavior-free same-class translation-unit seam that brings the original below 1000 lines.
- **Fix:** both rows are `SPLIT` with exact method groups and sibling filenames in the G-1 table.
- **Verification:** complete symbol/ownership read of both files and their headers; no source edit,
  build, test, index change, or commit.
- **Authority:** unchanged. These are implementation-time architecture verdicts, not authorization
  to perform the moves while this task remains backlog.
