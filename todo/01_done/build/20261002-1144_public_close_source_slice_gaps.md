# Close Public Source Slice Gaps

**Status:** DONE - 2026-10-02  
**Scope:** what remained after the public mirror and public developer build slice (`9890f379b`,
`2e8e56af8`, `dd807159c`, `2feb9d764`). That covers the primary-asset scan config and its
validator, a guard for plugin config file names, the GASP console-variable contract, the public
character's GASP dependency, and one operator-local environment fix.  
**Stable documentation owner:** none for this record; each fact lives in the owner named in Outcome

## Outcome

- **T1:** `.claude/settings.local.json` has no `PATH` override; the operator removed it, and the
  file was verified as valid JSON. Agent sessions started after the fix inherit the Windows PATH.
- **T2:** `scripts/ue/check/assets/check_primary_assets.py` treats a type with no `Directories` as
  nothing to check and fails a list it cannot read (six tests, run in public CI).
- **T3:** the Map scan in `Config/DefaultGame.ini` includes Manhattan's directory, so the editor
  registers its map and the chunk-11 rule can apply. R5 step 5 certifies `pakchunk11`.
- **T4:** `scripts/ue/check/governance/validate_plugin_data_staging.py` rejects plugin
  `Config/Default*.ini` other than `Default<PluginName>.ini`; the rule lives in
  [Project Conventions](../../../docs/architecture/conventions.md#unreal-assets-and-data).
- **T5:** `Plugins/Gameplay/ProjectSkeletalCapabilities/Config/Engine.ini` declares the 8 names
  `SandboxCharacter_CMC_ABP` reads, and the test checks them in every build.
- **T6:** the [developer quickstart](../../../docs/quickstart/developer/README.md) tells public
  developers to create the Game Animation Sample project for the release's engine version and to
  move the content ALIS uses into the `MotionMatching` content plugin by hand (D5).

**Still owned by the [3.0.0 release plan](../../00_current/00_release_3.0.0.md):**
- its Pre-candidate items: the payload closure, which adds a machine-readable external-dependency
  declaration for `/MotionMatching`, and the packaged console-variable checks;
- R5.

## Authority register

### Operator decisions

- **D1** Keep one current todo as the source of truth for what remains of this work and all its
  gaps; give a huge and important item its own current todo.
  - Effect: this record owned the slice's remaining items; the character dependency had its own
    task until D6 reduced it to T6.
  - Reason: "we need some SOT for you current work".
  - Date/source: 2026-10-02, operator.
- **D2** Publish as much as possible to the public mirror, excluding only private information,
  secrets, and local overrides; verifiers check this automatically; the projected public tree
  builds.
  - Effect: new checks that need only tracked files run in public CI.
  - Reason: not stated.
  - Date/source: 2026-10-01, operator goal for this slice.
- **D3** Critically evaluate the reviewer's feedback and fix the issues that affect current
  functionality or future scalability; refute the points that are wrong.
  - Effect: valid reviewer findings were applied without a further approval round.
  - Reason: not stated.
  - Date/source: 2026-10-02, operator.
- **D4** "in future I guess we will refactor it in our own char submodule, but not now".
  - Effect: an ALIS-owned character module, which needs no GASP, is the expected later direction;
    this slice does not build it.
  - Reason: not stated.
  - Date/source: 2026-10-02, operator.
- **D5** "gasp will be downloaded with exact release so we use in release some ue version so gasp
  will be use from that ue version".
  - Effect: public developers obtain GASP separately, from the sample released for the engine
    version the release uses.
  - Reason: not stated.
  - Date/source: 2026-10-02, operator.
- **D6** "P0b, compare against the real sample - bro don't need such bothering, if devs will be in
  issue - they tell and we fix , now please be very kiss for achive main goal".
  - Effect: no sample comparison, conversion tooling, probe, or compatibility check; the
    character work is one quickstart note, and install problems get fixed when developers report
    them.
  - Reason: as quoted.
  - Date/source: 2026-10-02, operator.

### Operator gates

- **Q1 [CLOSED by D3]:** Remove the three declared names no ALIS asset reads? - removed (T5).
- **Q2 [CLOSED by D3]:** Check the Data Driven CVar names in every build and gate only the
  engine-owned name on the `/MotionMatching/` mount? - done (T5).
- **Q3 [CLOSED by D3]:** Replace the zero defaults with the sample project's own values? - kept at
  0.
- **Q4 [CLOSED by D3]:** Keep Manhattan in its own chunk 11? - kept; T3 made the rule effective.
- **Q5 [CLOSED by D3]:** Are T3 and the public character work in 3.0.0 scope? - yes; the release plan
  carries both.
- **Q6 [CLOSED by D5]:** Which outcome must the public route provide for the character: (a) GASP
  obtained separately, (b) an ALIS-owned fallback, or (c) no playable body? - (a); (b) later (D4).
- **Q7 [CLOSED by D5]:** Which GASP version? - the sample for the release's engine version.

### Working assumptions

- **A1 [ACTIVE]:** The supported GASP console-variable contract covers only sample assets that ALIS
  references. Source: reviewer guidance forwarded on 2026-10-02, not an operator decision.
- **A2 [RESOLVED by D3]:** Until Q1-Q3 are answered, the committed names, values, and test gate stay.
- **A3 [RESOLVED by D3]:** The chunk-11 rule states intent, so T3 makes it effective instead of
  deleting it.
- **A4 [RESOLVED by D5]:** The reviewer recommended an ALIS-owned public fallback now.

## Non-goals

- The ALIS-owned character module (D4).
- Install tooling, sample comparison, a no-GASP probe, or a compatibility check (D6).
- Removing the `GameFeatureData` scan entry (`Config/DefaultGame.ini:112`): every editor loads
  GameFeatures through `AllToolsets`, and its editor module logs a LoadErrors error on each start
  without that rule (`GameFeaturesEditorModule.cpp:419-434`).
- Per-type merging and multi-line parsing in `check_primary_assets.py`, until a plugin `Game.ini`
  declares scans.
- GASP names read only by sample assets ALIS does not use (A1).

## Verified evidence

- **Validator (T2):** the validator exited 1 at `dd807159c` on `GameFeatureData`, whose entry has an
  empty `Directories=`. The engine accepts spaced and lowercase spellings of that list and imports
  a bare string inside a struct as empty, so the reader follows those rules.
- **Manhattan (T3):** before the fix, the extended Manhattan product-route test failed with the
  editor AssetManager holding no primary asset id for the map. The 2026-09-30 staged build and
  the playable-tour Candidate had no `pakchunk11` (`bGenerateChunks=True`). The public projection
  already scanned the directory (`scripts/git/mirror/prepare_public_source.py:143-153`) and
  replaces the private line, so public output did not change.
- **Plugin config names (T4):** UE 5.8 takes a plugin ini's branch from its file name
  (`ConfigCacheIni.cpp:7276-7299`). Five dead files existed: MotionMatching's `DefaultEngine.ini`
  and the four deleted in `dd807159c`.
- **GASP names (T5):** the sample AnimBP reads exactly the 8 declared names plus
  `a.animnode.offsetrootbone.enable`. The removed three are read only by sample components no ALIS
  actor carries.
- **Public character (T6):**
  - The game mode spawns `ObjectDefinition:Hero` by default (`SinglePlayerGameMode.cpp:25`).
  - Hero's and GrandPa's driver bodies use the GASP AnimBP and mannequin at `/MotionMatching/`
    (`Hero.json:40-41`, `GrandPa.json:10-13`).
  - The payload ships both definitions with `"dependency_payload_policy": "references_only"`.
  - The dependency audit lists no `/MotionMatching/` path today; the release plan's seed-only
    exporter fix owns that.

## Implementation tasks

- [x] **T1 (operator):** removed the `PATH` override.
- [x] **T2:** validator reads `Directories` in any spacing or letter case, reports a type with none
  as nothing to check, and fails an unreadable list.
- [x] **T3:** Manhattan's directory added to the Map scan; the Manhattan product-route test asserts
  the editor registers the map.
- [x] **T4:** plugin config name check, its test, the conventions rule, and the README row.
- [x] **T5:** 8-name contract, a test that checks them in every build, and the architecture
  section.
- [x] **T6:** quickstart note on obtaining the Game Animation Sample and placing its content.

## Test evidence

- **T2:** six fixture tests, each guard proven by a mutant that removes it.
- **T3:**
  - The Manhattan test failed before the config change and passes after.
  - The Kazan product-route test still passes.
  - The validator now counts the Manhattan directory (Map: 870 assets).
- **T4:** the known-bad tree from `2e8e56af8` is rejected with all four files listed (rc 1), and the
  current tree passes.
- **T5:**
  - The console-variable test passes in the full checkout, and deleting an entry fails it.
  - In a clean clone of the `make mirror` candidate, built from scratch in 3m37s, the test passes
    without the sample, and fails when an entry is deleted.
- **Public CI and mirror:** the validators and the suites (4, 34, 6, and 20 tests) pass, and the
  `make mirror` dry run passed (657 files, no dead links).
- **Final tree:** a second dry run after T3 and T6 passed (659 files, no dead links). The closing
  wording changes to the quickstart and the release plan had text, link, and forbidden-pattern
  checks only; per the operator, no further mirror run or clone build.

## Review record

### 2026-10-02 - investigation

- **Trigger:** operator: "we need some SOT for you current work, /investigate-change for what
  remains and all gaps in one todo , if task are huge and important add in current another todo".
- **Root cause:** inventory only.
- **Fix:** this todo and a separate public character task were created.
- **Verification:** validator run; engine source, descriptors, packages, and payload contract read.
- **Authority:** D1 and D2 recorded; Q1-Q5 opened; A1-A3 recorded.

### 2026-10-02 - reviewer final patch and todo review

- **Trigger:** two reviewer rounds forwarded by the operator under D3.
- **Root cause:**
  - The contract declared names for unused sample components.
  - The test skipped entirely without the sample.
  - The validator failed a type that names no directory.
  - Nothing guarded plugin config names.
- **Fix:** T2, T4, and T5 were implemented, and T3's completion moved before packaging. A new
  validator and deleting the `GameFeatureData` entry were rejected.
- **Verification:** observed red and green; public CI suites; `make mirror` dry run; public clone
  build and test.
- **Authority:** D3 added; Q1-Q5 closed; A2 and A3 resolved.

### 2026-10-02 - independent reviews of the final delta

- **Trigger:** the independent review returned PATCH three times, then PASS.
- **Root cause:**
  - The validator failed open, first on spacing inside the list and then on the key's spelling.
  - The release plan asked for check code after its freeze.
- **Fix:**
  - The fail-closed reader.
  - The packaged checks became a Pre-candidate item.
  - Optional items: tests that pin each guard, config-name failures printed last, and the rule
    moved to `conventions.md`.
- **Verification:** each new case failed before its fix and passes after it.
- **Authority:** none changed.

### 2026-10-02 - character decisions and closure

- **Trigger:** operator answers D4, D5, and D6.
- **Root cause:** n/a.
- **Fix:**
  - The separate character task was merged into T6 and removed.
  - T3 and T6 were implemented.
  - This record moved to `todo/01_done/build/`.
- **Verification:** see Test evidence for T3 and T6.
- **Authority:** D4-D6 added; Q6 and Q7 closed by D5; A4 resolved by D5.

### 2026-10-02 - closing review

- **Trigger:** reviewer closure PATCH forwarded under D3.
- **Root cause:**
  - The quickstart implied Fab ships a ready `MotionMatching` plugin.
  - The release plan called `/MotionMatching/` a declared external dependency, although the
    audit has no such declaration.
- **Fix:** the quickstart now says the content move is manual. The payload-closure item owns a
  machine-readable external-dependency declaration with generic audit handling.
- **Verification:** text, link, and forbidden-pattern checks.
- **Authority:** none changed.
