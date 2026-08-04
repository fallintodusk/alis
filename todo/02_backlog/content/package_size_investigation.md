# Project-wide Package Composition Investigation

## Problem

Release payload grew from 2.0 GB (2026-03-10 baseline) to 4.4 GB (2026-04-17).
All growth is in chunk 0 (boot/default). Chunk 1 (menu) is empty (1 entry).

The clean open-world P0 gate later measured 3 ProjectWorld IoStore entries at
5,661,893 bytes total, including exactly one 5,656,497-byte Kazan map entry.
Its largest-entry report instead starts with MetaHuman runtime and Mutable
payloads. This is therefore a project-wide dependency-closure concern, not an
open-world P0 blocker. Reproduce the evidence with
`scripts/ue/package/inspect_iostore.ps1`; generated receipts are evidence, not
a maintained source of truth.

## Root Cause: Unmodified GASP Sample ABP

Hero.json uses `SandboxCharacter_CMC_ABP` as DriverBody animClass.
This is the unmodified GASP demo AnimBP, NOT a clean integration ABP.

The ABP hardcodes a ref to `SandboxCharacter_CMC` (the GASP sample Character BP),
which pulls in the entire GASP demo ecosystem via ActorComponents:

```
Hero.json
  +-- animClass: SandboxCharacter_CMC_ABP
        +-- SandboxCharacter_CMC (Character BP) <-- THIS IS THE PROBLEM
        |     +-- AC_TraversalLogic -> LevelPrototyping (teleporters, blocks, textures)
        |     +-- AC_FoleyEvents -> 271 foley audio assets
        |     +-- AC_SmartObjectAnimation -> smart object BPs
        |     +-- AC_VisualOverrideManager
        |     +-- AC_PreCMCTick
        |     +-- GM_Sandbox (GameMode!) -> more sample refs
        |
        +-- CHT_PoseSearchDatabases -> 1404 UEFN_Mannequin animations
        +-- CHT_CMCCharacterAnimations -> state machine data
```

Second chain: Hero.json -> MutableCustomization -> CO_Character -> MutableSample + MetaHumans.
This pulls 363 MutableSample entries + 3 MetaHuman variants (Lean, Hero, GrandPa).

All content is pulled by valid ref chains, not by explicit cook rules.
`bCookAll=False` is set correctly - the cooker only follows refs.

## Chunk 0 Breakdown (3755 entries, 2.8 GB)

| Category | Entries | Notes |
|----------|---------|-------|
| MotionMatching/Characters/UEFN_Mannequin/Animations | 1404 | Full GASP anim set via SandboxCharacter_CMC_ABP |
| MotionMatching/Audio/Foley | 271 | Foley sound waves |
| MotionMatching/Blueprints | 71 | GASP blueprints |
| ProjectObject | 878 | Definitions, textures, materials |
| MutableSample/Character | 221 | Mutable customization objects |
| MutableSample/ExternalAssets | 141 | External refs from Mutable |
| MetaHumans (Lean) | 64 | via MutableSample |
| MetaHumans (Hero) | 62 | via MutableSample |
| LevelPrototyping | 21 | GASP sample level content |
| MetaHumans (GrandPa) | 10 | added recently |
| MetaHumans (Common) | 10 | shared MetaHuman assets |

## Chunk 10 (3542 entries, 1.2 GB) - City17

Looks correct - world-placed objects, map content, GrandPa (NPC).

## Chunk 1 (1 entry, 864 bytes) - Menu World

Only the map file itself. All menu assets fell to chunk 0 via shared refs.

## Investigation Needed

### MotionMatching (biggest: ~1700 entries)
- Does SandboxCharacter_CMC_ABP actually use all 1404 UEFN_Mannequin animations at runtime?
- Could a stripped ABP reference only the needed PoseSearch databases?
- Are foley audio assets (271) all triggered or are some GASP sample leftovers?
- LevelPrototyping content (21) is definitely GASP sample - should not ship

### MutableSample (second: ~363 entries)
- CO_Character pulls all 3 MetaHuman variants (Lean, Hero, GrandPa)
- Are all 3 needed at runtime or only the active hero variant?
- 141 ExternalAssets - what are these? Sample content or runtime-needed?

### MetaHumans (146 entries in chunk 0, 211 in chunk 10)
- Lean (64 in chunk 0): is this variant used?
- Hero (62 in chunk 0): is this the base MetaHuman for the player?
- GrandPa (83 total): NPC in City17 - should all be in chunk 10, not chunk 0

### General Cook Settings
- `bSkipEditorContent=False` in DefaultGame.ini - could set to True
- `DirectoriesToNeverCook` only excludes `/Game/CATEGORIES`
- Could add NeverCook for known sample content paths

## Possible Size Reduction Options (Non-Destructive)

### Option 1: DirectoriesToNeverCook for sample content
Add to DefaultGame.ini under [/Script/UnrealEd.ProjectPackagingSettings]:
```ini
+DirectoriesToNeverCook=(Path="/MotionMatching/Levels")
```
Excludes GASP sample levels. Low risk - these aren't referenced by gameplay.

### Option 2: bSkipEditorContent=True
Set in DefaultGame.ini. Excludes editor-only content from cook.

### Option 3: Audit SandboxCharacter_CMC_ABP refs
The GASP ABP may reference PoseSearch databases that include animations
not needed by ALIS locomotion profiles. Trimming the database refs
would be the highest-impact change but requires understanding which
animations are actually selected at runtime.

### Option 4: Mutable compilation options
Mutable has compile-time options to strip unused customization options.
Check CustomizableObject properties: mesh_compile_type, optimization level.
This could reduce MutableSample payload without removing content.

### Option 5: PakBlacklist
Create Build/Win64/PakBlacklist-<ChunkName>.txt to exclude specific
packages from pak without changing cook rules.

## Recommended Fix (When Ready)

### High impact: Clean ABP (saves ~1500 entries, ~1.5 GB estimated)
Create `ABP_ProjectLocomotion` that references ONLY:
- PoseSearch databases needed for ALIS locomotion profiles
- Animation notify logic (foley, traversal) as needed
- NO ref to SandboxCharacter_CMC (the GASP sample Character BP)
- NO ref to GM_Sandbox, LevelPrototyping, sample SmartObjects

This breaks the ref chain at the root. The PoseSearch databases
and animations they reference would still cook, but the sample
character BP, GameMode, level prototyping, and unused ActorComponents
would be excluded.

Update Hero.json animClass from:
  `/MotionMatching/Blueprints/SandboxCharacter_CMC_ABP.SandboxCharacter_CMC_ABP_C`
to:
  `/<ProjectPlugin>/Animation/ABP_ProjectLocomotion.ABP_ProjectLocomotion_C`

### Medium impact: Audit MutableSample refs (saves ~363 entries)
Check if CO_Character needs all 3 MetaHuman variants at runtime
or if unused variants can be excluded from the Mutable source object.

### Low impact: Quick config wins
- `bSkipEditorContent=True` in DefaultGame.ini
- `+DirectoriesToNeverCook=(Path="/MotionMatching/Levels")` if GASP sample levels exist

## Decision: Keep All For Now

The content is pulled by valid refs and may be needed in future.
Better to understand the full picture before cutting anything.
Revisit when package size becomes a shipping constraint.

## References

- Verified baseline: docs/build/packaging_guide.md
- Hero definition: Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json
- Cook settings: Config/DefaultGame.ini
- Chunk manifests: Saved/Cooked/Windows/Alis/Metadata/ChunkManifest/
