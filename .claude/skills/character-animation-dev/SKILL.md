---
name: character-animation-dev
description: "Implements and debugs ALIS modular character animation and parity behavior: Hero.json data-driven mesh/capability wiring, DefinitionCharacter runtime setup, SkeletalAssembly lifecycle, Motion Matching bridge behavior, Mutable customization defaults, leader-pose and local-body animation flow, character parity automation, and character debug capture analysis. Use for tasks mentioning BP_Hero, DefinitionCharacter, Hero.json, SkeletalAssembly, MotionMatching, MutableCustomization, LocalFirstPerson, DriverBody, WorldBody, LocalBody, character parity, project.character.switch, or project.character.capture."
metadata:
  author: alis-team
  version: "1.0"
---

# Character Animation Developer

## Mission

Implement and debug modular character animation changes quickly, with low regression risk, while keeping one source of truth for parity and investigation flow.

## Core Rule

Before any character animation logic, parity, or runtime wiring change, check:
- `docs/testing/character_parity.md`

Then load only task-specific files from `references/local_routes.md`.

Use memory-style refs for context hygiene:
- cite relative paths first
- open only files needed for the current task

Do not create duplicate wrappers around the existing parity flow unless the current test surface is proven insufficient.
The canonical automated entrypoint is:

```powershell
.\scripts\ue\test\character\capture_parity.ps1 -TimeoutSeconds 180
```

Optional runner policy:
- if your loader supports experimental `allowed-tools`, restrict to read/search/git/script execution needed for this skill
- example: `Read`, `Bash(git:*)`, `Bash(rg:*)`, `Bash(powershell:*)`
- if not supported, omit it and keep default tool policy

---

## Fast Start Protocol

1. Classify the task quickly:
- authored data issue
- runtime wiring issue
- animation propagation issue
- parity regression
- CDO/defaults comparison
- test gap or tooling gap

2. Load minimal context:
- always: `docs/testing/character_parity.md`
- data tasks: add `Hero.json`, `layer_contract.md`, and `ObjectDefinition.h`
- runtime tasks: add `DefinitionCharacter.cpp`, `ObjectSpawnUtility.cpp`, assembly/capability docs, and the specific capability cpp/h files
- legacy comparison tasks: add BP_Hero CDO dump or MCP CDO inspection
- parity tasks: add `CharacterParityTest.cpp` and `CharacterDebugCaptureComponent.cpp`

3. Define acceptance before editing:
- exact runtime invariant
- exact test or capture command
- exact artifact or log signal that proves the fix

4. Debug using evidence first:
- run the existing parity capture
- inspect the latest JSONs in `Saved/Validation/CharacterDebug/`
- inspect `Saved/Logs/Alis.log`
- compare legacy vs modular before changing code

5. For visible-mesh animation failures, inspect this chain explicitly:
- DriverBody: source AnimBP and movement data
- WorldBody parent mesh: empty or populated, animClass, leader pose
- BodyCustomization child: actual generated mesh, animClass, parent
- LocalBody and LocalBodyCustomization
- Head and HeadCustomization

Do not assume animation failure means Motion Matching failed. The bridge can work while visible meshes still have no anim propagation.

---

## One SOT

`docs/testing/character_parity.md` is the SOT for:
- canonical debug command
- source-of-truth doc routing
- parity capture workflow
- CDO fallback path
- runtime fields to compare
- known non-bugs

If this skill and that doc disagree, update the skill to match the doc or fix the doc first. Do not create a second SOT.

---

## Design Invariants

### Data and Wiring
- `Hero.json` is the modular hero authored source of truth.
- `meshes[]` and `capabilities[]` remain data-driven.
- character behavior should be rebuilt in C++ plus JSON, not by adding new project Blueprints.
- third-party asset paths may be hardcoded where necessary.

### Runtime Ownership
- `DefinitionCharacter` owns capsule, camera, movement, GAS, vitals, and input.
- `ObjectSpawnUtility` owns mesh/component creation from definition data.
- `SkeletalAssembly` owns lifecycle ordering and deferred activation.
- `ProjectSkeletalCapabilities` owns adapters such as MutableCustomization, MotionMatching, and LocalFirstPerson.

### Animation Flow
- DriverBody is the motion source.
- WorldBody should follow DriverBody through the intended retarget or pose-sharing path.
- Local body should use the project local-body anim instance for clipping fixes and local-only overrides.
- Empty parent meshes with generated child customization meshes are valid, but visible animation must still propagate correctly to the rendered child.

### Debugging
- use parity JSON and logs before speculation
- compare legacy and modular captures from the same RunId
- use BP_Hero CDO dump as the default legacy baseline when needed

---

## Implementation Routes

### Route A: Authored Data Change
Read:
- `docs/testing/character_parity.md`
- `Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json`
- `Plugins/Resources/ProjectObject/docs/layer_contract.md`
- `Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h`

Touch:
- `Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json`
- generator-owned outputs only if the normal pipeline updates them

Validate:
- run parity capture
- inspect modular mesh roles, animClass values, and capability activation

### Route B: Runtime Wiring Change
Read:
- `docs/testing/character_parity.md`
- `Plugins/Gameplay/ProjectCharacter/Source/ProjectCharacter/Private/DefinitionCharacter.cpp`
- `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp`
- assembly and capability docs from `references/local_routes.md`

Touch:
- `ProjectCharacter`
- `ProjectObject`
- `ProjectSkeletalAssembly`
- `ProjectSkeletalCapabilities`

Validate:
- build
- run parity capture
- compare legacy/modular JSONs

### Route C: Motion Matching or Mutable Adapter Change
Read:
- `Plugins/Gameplay/ProjectSkeletalCapabilities/docs/architecture.md`
- specific capability headers and cpp files
- any relevant TODO from `references/local_routes.md`

Guardrails:
- keep fixes in C++ and JSON
- preserve assembly lifecycle ordering
- avoid hidden tick hacks when an existing lifecycle hook is sufficient
- log enough state to prove bridge install and runtime activation

### Route D: Pure Investigation
Read:
- `docs/testing/character_parity.md`
- latest JSON captures under `Saved/Validation/CharacterDebug/`
- `Saved/Logs/Alis.log`
- `Saved/Inspection/BP_Hero_CDO_2026-04-03.json`

Deliver:
- exact failure point
- exact files used to prove it
- exact next code path to modify

---

## Test Tiers

### Tier 1 (fast, required)
```powershell
scripts/ue/standalone/build.ps1
.\scripts\ue\test\character\capture_parity.ps1 -TimeoutSeconds 180
```

### Tier 2 (when targeted automation is needed)
```powershell
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.Character.ParityCaptureTest" -Game -Map "/City17/Maps/City17_Persistent_WP.City17_Persistent_WP"
```

### Tier 3 (manual follow-up only if parity artifacts are insufficient)
- use `project.character.switch`
- use `project.character.capture`
- inspect `Saved/Logs/Alis.log`

Do not jump to PIE/manual testing first when the parity test can already produce the needed evidence.

---

## References

- Local routing and file map: `references/local_routes.md`
- UE official links: `references/ue_links.md`

---

## Output Contract Per Task

After implementation or investigation, always provide:
1. the concrete behavior delta or root cause
2. the exact files touched or inspected
3. the exact test/capture commands run
4. the exact artifact or log evidence used
5. residual risks or known gaps

---

## Anti-Patterns

- Creating a second SOT for character parity when `docs/testing/character_parity.md` already exists.
- Creating redundant wrapper scripts around `capture_parity.ps1` without a proven gap.
- Assuming DriverBody animation success means visible meshes animate correctly.
- Assuming visible T-pose means Motion Matching failed.
- Moving orchestration into the wrong plugin instead of the owning adapter or assembly layer.
- Fixing runtime behavior by adding project Blueprints when C++ plus JSON is the chosen path.
- Unicode in docs/comments.
