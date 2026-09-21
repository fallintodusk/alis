# Reduce Shipping Dependency Closure

Status: complete

## Contents

- [Authority register](#authority-register)
- [Goal and boundaries](#goal-and-boundaries)
- [Verified package evidence](#verified-package-evidence)
- [Used, unsupported, and still unknown](#used-unsupported-and-still-unknown)
- [Architecture decision](#architecture-decision)
- [Implementation slices](#implementation-slices)
- [Implementation outcome](#implementation-outcome)
- [Verification](#verification)
- [Documentation impact](#documentation-impact)
- [Completion](#completion)

## Authority register

### Decisions

- D1: This file is the single active task for reducing the current Win64
  Shipping dependency closure. It supersedes the completed status previously
  recorded for the first MetaHuman plugin slice.
- D2: Optimize the player package and its first-party runtime ownership, not
  repository checkout size or the Developer payload.
- D3: Preserve Kazan, Manhattan, City17, ProjectWorld generation, generated
  World data, current supported maps, and current player behavior.
- D4: Preserve the accepted Hero appearance, first-person body, locomotion,
  GrandPa, dialogue, inventory/loot behavior, and player controls.
- D5: Do not edit installed Engine plugins or third-party plugin source. Remove
  a third-party dependency only by replacing the current ALIS-owned consumer
  and then disabling or deleting the now-unreferenced plugin as a whole.
- D6: Cook roots and runtime product roots must be explicit. Do not use a broad
  directory cook rule to hide missing ownership, and do not blacklist content
  that still has a supported runtime owner.
- D7: Measure exact IoStore output after each retained package change. Source
  directory size, repository size, and Asset Registry reference counts are
  supporting evidence, not release-size authority.
- D8: Blueprint syntax is not a package-size category. Delete unused
  first-party Blueprints, keep data-only composition assets where they are the
  cleanest owner, and move only repeated or runtime policy into first-party C++.
- D9: [SUPERSEDED by D11] No new PCG building framework, catalog service,
  dependency registry, or parallel content authority is justified by the
  current evidence.
- D10: Do not sign, mirror, publish, or mutate a remote release in this task.
- D11: InstanceArrayTool replacement and future project-owned PCG/building
  tooling are not part of this package-size cleanup. Preserve that work as one
  separate World backlog task.
- D12: GrandPa owns authored MetaHuman rendering assets, including its body,
  face, five groom assets, their bindings, and the shared Common materials they
  require. This task may remove only independently proven authoring/tracker
  roots and unowned Hero/Lean content; it must not treat the whole MetaHuman
  directory as removable.
- D13: ProjectObject capability references are a global definition-cook
  contract. Resolve each capability through the same `FCapabilityRegistry` used
  by runtime spawning. ProjectObject currently has two live property-assignment
  implementations: ObjectSpawnUtility and InteractableActor definition reapply,
  and their lookup/import behavior differs. Replace that duplication with one
  reflected-property resolver/value contract used by both runtime paths and
  editor bundle discovery. Preserve current semantics only where code/data/tests
  demonstrate an owner. Derive bundle references only from reflected soft
  object/class properties; do not infer asset ownership from arbitrary strings.
  Canonicalize with Unreal soft-path primitives, accept only valid mounted
  content paths, reject native `/Script/` paths, malformed/unmounted values, and
  canonical duplicates. Do not key cook behavior to property names, file
  extensions, capability names, or the eleven values that exposed the defect.
- D15: Capability provider readiness is generic. Enabled provider modules use
  the engine plugin loading lifecycle and register through the shared registry
  kernel; ProjectDefinitionGenerator must not hard-code gameplay module names
  or depend directly on every provider. Definition validation owns source-data
  rejection and must abort before save on an unresolved capability, property,
  or value. Asset-bundle generation derives references from already-valid data;
  it is not a second validation policy.
- D14: Character cleanup requires the same-scene before/after visual proof plus
  runtime component and log evidence. Editor MCP evidence is the fast
  inspection layer; a packaged Shipping run is the final authority.

### Open question

- Q1 - OPEN, NON-BLOCKING: after the cleanup and a fresh accepted release
  candidate, should the result ship as 2.0.1? Package cleanup comes first;
  versioning and publication remain separate operator decisions.

### Rejected assumptions

- A1 - REJECTED: "Convert Blueprints to C++ to reduce package size." Cooked
  assets are retained by dependency roots. The same hard asset reference in a
  C++ class retains the same content closure.
- A2 - REJECTED: "All 1.046 GiB under ProjectObject is dead." Current maps and
  production roots reach most of that content.
- A3 - REJECTED: "The failed MetaHuman Core Tech exclusion proved all 303.83
  MiB is required at runtime." The failure proved that the cooker still tried
  to serialize assets whose class module had been removed. It did not prove
  that Hero or GrandPa use the tracking/solver models in the packaged game.
- A4 - REJECTED: "Zero Asset Registry referencers means safe to delete."
  `ObjectDefinition:Hero` is loaded through a C++ primary-asset identifier and
  is a demonstrated counterexample. Data strings, primary-asset IDs, and code
  soft paths must also be reconciled.
- A5 - REJECTED: "Every JSON-declared runtime asset is already retained by the
  generated definition bundle." Typed definition fields are bundled, but the
  current generic capability-property strings are not.
- A6 - REJECTED: "The MetaHuman directory can be cleaned as one unit." GrandPa
  directly and visibly uses `/Game/MetaHumans/GrandPa` plus shared Common
  content. Only separately traced tracker/solver and unowned Hero/Lean roots
  are candidates.
- A7 - REJECTED: "Prune the MutableSample graph asset by asset." Its compiled
  `CO_Character` bulk is a monolithic vendor-owned product. Replace ALIS's fixed
  Hero consumer, then exclude the whole sample plugin from Game targets while
  remeasuring whether the first-party customization seam and Engine Mutable
  runtime still have supported Game owners.

## Goal and boundaries

### Goal

Produce the smallest behavior-preserving Win64 Shipping player package by:

1. removing editor/authoring model data from Shipping;
2. replacing broad cook roots with current product ownership;
3. deleting first-party assets with no product, data, test, or map owner;
4. replacing the fixed Hero's monolithic Mutable sample dependency with a
   first-party fixed representation only when visual parity is proven.

Stop when each remaining material root has a named current owner and further
reduction would remove or redesign supported behavior.

### In scope

- Win64 Shipping cook policy and exact packaged dependency evidence;
- MetaHuman tracking/solver content included only by authoring paths;
- ProjectObject's broad directory cook root and generated definition closure;
- first-party ProjectObject definitions and Blueprints proven to have no
  product, data, test, or map owner;
- the fixed Hero's Mutable sample closure through a first-party replacement;
- focused tests, package regressions, and stable docs affected by the result.

### Out of scope

- changing World generation, maps, terrain, visuals, or accepted placements;
- changing Motion Matching behavior or replacing the two first-party AnimBPs;
- modifying vendor or installed Engine source/content in place;
- replacing InstanceArrayTool or building the future project-owned PCG and
  building-authoring tooling owned by D11;
- migrating current City17 construction/HISM Blueprints to C++;
- converting every Blueprint merely because it is a Blueprint;
- Developer payload size, Linux release work, itch.io, Launcher, networking,
  signing, mirroring, upload, or release creation.

## Verified package evidence

The current accepted package after the completed MetaHuman Character/Live Link
target restriction is:

| Property | Current evidence |
|---|---:|
| Total game payload | 5,049,481,239 bytes (4.703 GiB) |
| Shipping executable | 186,678,272 bytes |
| IoStore entries | 13,381 |
| `/ProjectWorldData` listed bytes | 272.68 MiB |

The exact listed IoStore roots are:

| Root | Listed size | Current classification |
|---|---:|---|
| `/MutableSample` | 1,781.44 MiB | Used by fixed Hero, but grossly over-broad |
| `/ProjectObject` | 1,046.14 MiB | Mixed current product and broad-cook surplus |
| MetaHuman Animator models | 530.20 MiB | Not in GrandPa's asset dependency closure |
| MetaHuman Core Tech models | 303.83 MiB | Not in GrandPa's asset dependency closure |
| `/ProjectWorldData` | 272.68 MiB | ProjectWorldData persistent data/content; ProjectWorld realizes it; preserve |
| `/Game/MetaHumans` | 180.02 MiB | GrandPa runtime plus dead Hero/Lean closure |
| `/MotionMatching` root | 62.50 MiB | Current Hero/GrandPa/City17 owner |

This identifies the actual gigabytes:

- 1,600.92 MiB is `CO_Character.*.ubulk` inside the 1,781.44 MiB Mutable
  sample root. `Hero.json` currently selects `CO_Character` and `COI_Hero`.
- 834.03 MiB is MetaHuman face tracking/solver model data. The largest files
  are FaceTracker and its regional trackers (530.20 MiB total), HeadPoseTracker
  (113.90 MiB), GenericRigSolver (83.84 MiB), StereoHMCRigSolver (83.78 MiB),
  and FaceDetector (22.30 MiB).
- GrandPa genuinely owns authored MetaHuman rendering content under
  `/Game/MetaHumans`: its definition resolves a 1,725-package transitive graph,
  including 228 `/Game/MetaHumans` packages. A fresh live Asset Registry trace
  found zero of the 20 tracked face-tracker/solver model packages in that
  graph. Preserve the rendering graph; investigate the separate authoring model
  roots independently.
- The 180.02 MiB `/Game/MetaHumans` root is itself mixed: `Common` is 61.93
  MiB, `GrandPa` is 41.77 MiB, `Hero` is 48.81 MiB, and `Lean` is 27.51 MiB.
  GrandPa's graph reaches `Common` and `GrandPa`, not the `Hero` or `Lean`
  roots. Zero-referencer `BP_Lean` directly pulls Lean plus Hero groom assets;
  zero-referencer `RTG_Hero` pulls `IK_Hero` and the old Hero body. The broad
  ProjectObject cook rule currently retains those dead first-party roots.
- A live City17 PIE inspection through the official Unreal MCP route resolved
  GrandPa as a definition-driven `DefinitionCharacter` with the current body
  and face meshes plus five visible, non-hidden, main-pass groom components:
  hair, beard, mustache, eyebrows, and eyelashes. Each component resolved to
  the matching GrandPa groom and binding asset. The current comparison capture
  is under `Saved/Validation/PackageSize/GrandPa/before-pie-lookat.png`; it is
  transient execution evidence, not a durable dependency. The repeatable PIE
  vantage places the player root at `(-2243, 11681, 685)` and looks at
  GrandPa's upper body at `(-2469, 11788, 735)` in City17.
- `/ProjectObject` is forced wholesale by
  `DirectoriesToAlwaysCook=(Path="/ProjectObject")`, added in April 2026 by
  broadening the previous Hero-only rule. A separate Asset Manager rule already
  marks generated ObjectDefinitions as `AlwaysCook`.

The retained first slice remains valid but did not complete this task:

- MetaHuman Character and MetaHuman Live Link are Editor-only project plugins.
- The accepted package saved 50.23 MiB, including the OpenCV runtime DLL.
- Character parity and the World-owned playable-tour gate passed.

## Used, unsupported, and still unknown

### ProjectObject closure

ProjectObject JSON is authoring input, not player runtime payload. The staged
package manifests contain zero ProjectObject JSON files; the repository's
ProjectObject JSON sources total only about 0.2 MiB. Deleting them would destroy
the source of truth and regeneration path without reducing the player package.

Generated `UObjectDefinition` assets are the runtime authority. Their
`UpdateAssetBundleData()` implementation writes a `Default` bundle containing
typed fields: spawn class, meshes, materials, animation classes, groom
bindings, granted effects/ability sets, and the customization section's Mutable
source. The live Asset Registry confirms that GrandPa's generated definition
exposes those typed dependencies.

The bundle is not complete for generic capability properties. Eleven current
property values are asset paths: two audio presets, five dialogue trees, two
Motion Matching bridge classes, Hero's Mutable instance, and Hero's Mutable
source. Asset Registry evidence shows, for example, that GrandPa's generated
definition does not depend on `DLG_GrandPa_Entry`, and Hero's generated
definition does not depend on `COI_Hero`. Current map references and broad cook
roots retain some of these only incidentally.

Close that gap once at the existing definition owner. ProjectObject already
depends on ProjectObjectCapabilities, and runtime spawning already resolves
`FObjectCapabilityEntry::Type` through `FCapabilityRegistry`. Bundle generation
must use that same registry and actual reflected property type, not a second
"path-looking string" heuristic.

The runtime contract is itself duplicated today. ObjectSpawnUtility supports
exact and boolean-prefixed property lookup plus explicit soft-reference arrays;
InteractableActor definition reapply has a separate importer with one-level
nested-struct lookup and different special cases. Both are live. Reconcile the
current data/tests, then extract one cohesive ProjectObject-owned helper used by
both runtime paths and editor bundle discovery; remove the duplicate local
implementation in the same migration. Scalar soft object/class properties and
the accepted semicolon-separated soft-reference array form are the only generic
reference-bearing forms. Canonicalize through Unreal soft-path primitives, add
valid mounted content asset paths to the definition's `Default` bundle, and
reject ordinary strings, numbers, tags, native `/Script/` types,
malformed/unmounted paths, and canonical duplicates. An unknown capability or
declared property is a visible definition validation failure, not an ignored
cook hint. Do not branch on property names, asset extensions, or capability
types, add a new lower-level module, or create a hand-maintained cook list.

The typed mechanism exists but the capability-property gap must close first.
It still does not replace the fresh-cook experiment: only a package made
without the broad directory rule can prove that every required definition
dependency is retained by the current cook.

A live-editor Asset Registry census covered all 3,454 packaged
`/ProjectObject` `.uasset` packages. Starting from current non-test product
referencers (City17, Game, ProjectWorldData, first-party resource plugins) plus
the known dynamic Hero, GrandPa, and loot-profile roots produced:

| Classification | Packages | Direct package bytes |
|---|---:|---:|
| Reachable from current roots | 2,601 | 840.95 MiB |
| Not reached by that graph | 859 | 208.96 MiB |

The 208.96 MiB is a candidate ceiling, not deletion authority. It is dominated
by 110.31 MiB of roadway vehicle variants, 30.21 MiB of nature content, 24.67
MiB of devices, and 14.02 MiB of building content. The product maps already
reference many other vehicle and building assets, so deleting whole categories
would be wrong.

The supported KISS experiment is to remove only the broad
`DirectoriesToAlwaysCook=/ProjectObject` rule while retaining the existing
generated ObjectDefinition and loot-profile Asset Manager rules. A fresh cook
will then reveal the real map + definition dependency closure. If a supported
soft dependency disappears, fix its actual primary-asset/data owner; do not
restore the whole-directory fallback.

### Generated definitions

The public generation manifest contains 77 ObjectDefinitions. Seventeen have
no Asset Registry referencer. Nine of those are still reached by current C++ or
data identifiers: Hero, KeyLuxuryApartment, EmergencyMedkit, EmergencyWater,
FishPaste, BraisedBeans, EmergencyRation, EmergencyPouch, and CompactPryBar.

Eight have neither an Asset Registry referencer nor a production textual ID
consumer and are deletion candidates, not accepted deletions:

- ComplexDoor_Inner_White;
- Plain_Door_1_Shop;
- BreadBig;
- PeasStew;
- StewedBeef;
- DoubleBed_2_Wooden;
- Dresser_Classic;
- Wardrobe_Classic.

Their combined direct ProjectObject dependency closure is about 6.60 MiB.
Before deletion, verify dialogue, loot, save fixtures, maps, and soft paths and
prove generation cleanup removes the derived assets.

### First-party Blueprints

There are 29 Blueprints under `/ProjectObject`.

Ten are current City17 content and must not be deleted blindly:

- BP_BuildingHrushev;
- BP_RainwaterPipe;
- BP_PaletteTrash;
- HruSide_Bp and HruSideDetailed_Bp;
- HruFrontDetailed_Bp and HruBack_Bp;
- BP_Shop_1;
- BP_Naz60K1_Dormitory;
- BP_Piano_1.

Six current Blueprints contain repeated HISM/construction logic:
`BP_PaletteTrash`, `HruSide_Bp`, `HruSideDetailed_Bp`,
`HruFrontDetailed_Bp`, `HruBack_Bp`, and `BP_Naz60K1_Dormitory`.
That is a real future C++/PCG ownership seam, but D11 keeps it outside this
size task. `BP_BuildingHrushev` is the data-only vendor-derived migration
bridge. `BP_RainwaterPipe`, `BP_Shop_1`, and other simple composition assets
are not C++ candidates merely because they are Blueprints. `BP_Piano_1` owns
separate runtime light behavior and must not be folded into the HISM seam.

Nineteen ProjectObject Blueprints have no production Asset Registry referencer:

- BP_GrandPa and BP_Lean;
- BP_AcUnit and BP_Conditioner_OuterOff_1;
- BP_KitchenSet_2 and BP_KitchenSet_1_2;
- BP_Bathtub_4_Set, BP_Single_BedSteel, and Bp_LampWall_1;
- BP_ComplexDoor_Outer_1_1 through BP_ComplexDoor_Outer_1_10.

Their union dependency closure is about 9.72 MiB. `BP_GrandPa` is artificially
kept relevant by `ObjectParentGeneralizationIntegrationTest.cpp` as a generic
spawn-class fixture even though `GrandPa.json` now uses
`DefinitionCharacter`. Replace that test dependency with a test-owned fixture
or current production class before deleting BP_GrandPa. The remaining assets
still require string/soft-path checks before deletion.

`BP_Lean`, `RTG_Hero`, and `IK_Hero` also require explicit removal after the
same reconciliation. They are currently zero-owner ProjectObject assets that
pull the packaged `/Game/MetaHumans/Lean` and `/Game/MetaHumans/Hero` roots.
The package opportunity exposed by those roots is 76.32 MiB; final savings can
only be claimed after the broad cook rule is removed and a fresh cook confirms
their absence.

The two first-party animation Blueprints under
`/ProjectSkeletalCapabilities/MotionMatching` are intentional adapter assets:
`ABP_MotionMatchingBridge` and `ABP_WorldBodyRetarget`. Keep them.

The `/Game/Project` Blueprint inventory is outside the requested plugin-domain
refactor. Three assets are current City17 dependencies; the editor utility is
not cooked. Do not broaden this task to migrate them.

### Mutable Hero

Mutable is genuinely used today. Hero selects a fixed TShirt + Inside Boots
preset and realizes transient body/head meshes. Existing baked MetaHuman assets
do not yet prove the same accepted appearance. Therefore the 1.78 GiB root is
not dead weight, but it is an over-broad implementation of a fixed product.

The project-owned seam is to bake/export the accepted fixed result into ALIS
content and update `Hero.json`. Do not edit or prune MutableSample internals.
The expected cleanup is therefore large: once the fixed replacement owns Hero,
the complete 1,781.44 MiB `/MutableSample` package root should leave the Game
cook rather than being trimmed asset by asset. A fresh IoStore inventory must
measure the actual result and any remaining Engine Mutable code/content
separately.

Removing the current sample payload does not require deleting the source-level
customization design. Once Hero has a fixed representation, disable the
third-party `MutableSample` plugin for Game targets after it has no product
consumer, then remeasure the Game build and receipt. Engine Mutable is currently
a compile-time/runtime dependency of ProjectCharacter and several
ProjectSkeletalCapabilities paths, so excluding it is not assumed to be a
descriptor-only toggle. If no supported Game route still owns those callbacks,
components, or classes and the dependency can be removed cleanly, exclude the
unused Engine Mutable runtime while retaining a repository seam that can be
re-enabled by real product-owned customization work. If another current runtime
owner remains, keep it and record that owner. Do not add conditional framework
or split modules merely to remove a small unmeasured binary cost; decide from
post-Hero dependency, build, runtime, receipt, and IoStore evidence.

### MetaHuman models

Engine source traces the large Animator and Core Tech models to face tracking,
capture, solving, and editor pipeline modules. ALIS source/config has no current
runtime caller for those systems. The Shipping receipt excludes the MetaHuman
Animator plugin but includes MetaHuman Core Tech runtime support for authored
MetaHumans.

The previous Core Tech known-bad cook failed on
`MetaHumanRealtimeSmoothingParams` while the cooker was still requesting
Default/Heavy smoothing assets. That is a cook-policy/module mismatch, not
evidence that the large NNE tracking models are a player feature.

Do not disable MetaHuman rendering support or exclude `/Game/MetaHumans` as a
whole. The first candidate is limited to the 20 independently traced
tracker/solver model packages that are absent from GrandPa's transitive graph.
The second independent candidate is unowned `/Game/MetaHumans/Hero` and
`/Game/MetaHumans/Lean` content after their obsolete first-party referencers
are removed. GrandPa/Common remain until a narrower asset has both no current
owner and a successful package proof.

## Architecture decision

### Cook ownership

```text
supported maps
  -> hard map/external-actor dependencies

generated ObjectDefinitions + loot profiles
  -> existing Asset Manager AlwaysCook rules
  -> their declared soft dependencies

packaging policy
  -> excludes proven editor/authoring model roots
  -> does not force the whole ProjectObject mount
```

Do not add a second manually maintained product catalog unless the experiment
proves that the existing primary-asset rules cannot retain a required dynamic
definition. `UObjectCatalog` already exists for runtime warmup; extending it is
allowed only if a reproduced missing-cook failure demonstrates that it is the
correct single owner.

### Blueprint and C++ ownership

```text
ProjectObject C++
  -> reusable instanced-mesh assembly behavior and runtime policy

ProjectObject data-only assets / placed actor properties
  -> mesh choices, transforms, and per-instance presentation

ProjectPlacementEditor
  -> one-pass migration/bake tooling for current placed actors

City17
  -> actual placements and visual acceptance
```

The first-party C++ type must not hard-code City17 assets or copy vendor code.
Use normal Unreal reflected properties so map/content data owns mesh references.
Hard references are correct for assets that a placed actor always requires;
soft references are used only where deferred/optional loading is a real runtime
contract.

Future InstanceArrayTool/PCG ownership is a separate World backlog concern by
D11. This task carries only the evidence needed to avoid mistaking it for a
package-size win.

## Implementation slices

Retain a slice only after its focused checks and fresh package measurement pass.
If a candidate fails, restore the candidate files, record the real owner, and
continue with the next independent slice. Do not weaken runtime acceptance.

### Slice 0 - Preserve the completed target-boundary win

1. Keep MetaHuman Character and MetaHuman Live Link Editor-only.
2. Keep the governance regression that rejects either plugin becoming available
   to Game targets.
3. Retain the 5,049,481,239-byte accepted package as the comparison baseline.

### Slice 1 - Remove MetaHuman authoring model data from Shipping

1. Preserve the current City17 GrandPa comparison vantage and record the live
   body, face, five groom assets, five binding assets, visibility flags, and
   clean missing-asset/groom log baseline through official Unreal MCP.
2. Run an isolated known-bad cook with `bSkipEditorContent=True` first. Keep it
   only if it removes the tracked model roots without removing current runtime
   MetaHuman assets.
3. If the generic flag does not remove them, add the narrowest project-owned
   cook exclusions for the proven tracker/solver directories. Do not edit Engine
   plugin descriptors or assets.
4. Keep MetaHuman Core Tech runtime modules unless a separate target build and
   receipt prove they are unnecessary. The objective is model-data removal, not
   speculative module surgery.
5. Add a package regression that rejects the exact face tracker/solver roots in
   Shipping and a known-bad control showing the check detects their presence.
6. Package and run Hero, GrandPa, dialogue, City17, Kazan, and Manhattan routes.
   Recreate the GrandPa vantage in the packaged game, capture the same visible
   body/face/groom set, and reject missing asset, groom, binding, shader, or
   animation failures in the runtime log.

Expected opportunity: up to 834.03 MiB of listed model data. The realized
package decides the retained number.

### Slice 2 - Replace the broad ProjectObject cook root

1. Characterize current definition data against both live property-assignment
   paths, then extract one ProjectObject-owned reflected-property resolver/value
   helper. ObjectSpawnUtility, InteractableActor definition reapply, and
   `UObjectDefinition::UpdateAssetBundleData()` must resolve the capability via
   `FCapabilityRegistry`, resolve the same property, and share the accepted
   scalar plus semicolon-array soft-reference semantics. Remove the duplicate
   InteractableActor importer; do not retain divergent legacy behavior without
   a current consumer. Bundle only actual reflected soft object/class
   properties. Cover the 11 current paths, both runtime callers, scalar and
   array forms, every retained lookup form, invalid/non-path strings, native
   `/Script/` paths, unmounted paths, unknown capabilities/properties, and
   canonical duplicates in focused editor tests. Include a differently named
   property and asset type so the regression rejects key/extension special
   cases, and assert that an ordinary string which resembles a content path is
   not bundled.
2. Replace the registry's provider-name list with generic provider
   self-registration through the existing ProjectCore registry kernel. Require
   enabled provider loading phases to be complete before definition validation
   or bundle discovery; missing readiness is a visible failure, not a partial
   registry. Do not add provider plugin dependencies to
   ProjectDefinitionGenerator.
3. Make definition validation resolve every capability/property/value through
   the shared contract and return failure before asset save. Runtime initial
   spawn and definition reapply must also return failure rather than
   log-and-succeed on the same defects. `UpdateAssetBundleData()` consumes the
   resolved contract only to derive bundle references.
4. Remove only `DirectoriesToAlwaysCook=(Path="/ProjectObject")` in an isolated
   candidate. Keep the existing ObjectDefinition and LootProfileDefinition
   Asset Manager `AlwaysCook` rules.
5. Package and verify every generated definition asset, every current loot/data
   identifier, Hero spawn, inventory/drop, current dialogue/object interactions,
   and every supported map.
6. If a required dynamic asset is absent, repair its existing definition,
   primary-asset, or experience ownership. Do not restore the directory-wide
   fallback and do not create a parallel hand list.
7. Repeat the exact IoStore census and record what portion of the 208.96 MiB
   candidate ceiling actually disappears.
8. Add a regression that rejects reintroducing a broad ProjectObject directory
   cook rule while proving generated definitions and all declared asset-path
   capability properties remain packaged.

### Slice 3 - Remove material false owners

1. Replace the stale BP_GrandPa integration-test dependency with a test-owned or
   current production fixture; do not let a generic test retain obsolete product
   content.
2. Reconcile and delete only independently proven Lean-only content. Keep the
   existing `/Game/MetaHumans/Hero`, `RTG_Hero`, and `IK_Hero` assets available
   as candidate inputs to Slice 4 even though they have no current product owner.
3. Reconcile any other post-Slice-2 candidate only when the fresh package proves
   it still retains material closure or false product ownership.
4. Run definition/data validation, the focused spawn-class regression, an Asset
   Registry no-referencer/no-string-reference proof, and fresh package census.

### Slice 4 - Replace the fixed Hero Mutable closure

1. Capture the accepted Hero appearance, component roles, materials, grooms,
   first-person body, animation, and locomotion before editing.
2. Compare the existing baked Hero/IK/retarget assets with a supported
   bake/export of the exact realized Mutable Hero. Build another equivalent
   first-party fixed asset set only if neither existing option can satisfy the
   contract. Choose the smallest representation that passes exact visual,
   first-person, animation, locomotion, and packaged-runtime parity. Do not
   modify MutableSample source/content.
3. Update `Hero.json` to the first-party fixed representation and remove only
   Hero's Mutable customization capability/roles after parity passes.
4. Preserve Motion Matching, retargeting, local/world body visibility, input,
   camera, and definition-driven spawn ownership.
5. Disable MutableSample for the Game target only after no production asset or
   code path references it.
6. Exclude the sample at its plugin/target boundary; do not delete or edit a
   subset of vendor sample assets. Measure the remaining first-party Mutable
   source dependencies, Game receipt, and Engine Mutable package cost after the
   sample root is gone. Remove Engine Mutable from Game only if no supported
   runtime owner remains and the existing code can be cleanly decoupled; future
   customization alone is not a Shipping owner.
7. Add package regressions rejecting `CO_Character` and `/MutableSample` in
   Shipping, with a known-bad control.
8. Only after the fixed representation passes parity, delete existing Hero,
   `RTG_Hero`, or `IK_Hero` assets that the selected result proves obsolete.

Expected opportunity: 1,781.44 MiB listed, dominated by 1,600.92 MiB of
`CO_Character` bulk data. If exact visual parity cannot be achieved, stop for
architecture review rather than accepting a different Hero.

### Slice 5 - Reconcile remaining dead first-party source

This bounded cleanup remains because the operator explicitly requested removal
of confirmed first-party dead weight, not because it is expected to reduce the
package materially. It follows the larger runtime closures and must not delay a
successful Mutable slice or reopen World reconstruction.

1. Re-measure after Slices 2-4 before touching the eight definition and remaining
   zero-production-referencer Blueprint candidates.
2. Reconcile candidates against maps, dialogue, loot, saves/fixtures, code/data
   identifiers, and soft paths. Delete only source JSON/assets proven unowned,
   regenerate, and prove orphan cleanup removes derived outputs. Do not port dead
   assets to C++.
3. If cleanup requires a migration, runtime redesign, or other nontrivial blast
   radius while the asset is absent from Shipping, move that specific work to
   one concise content backlog task rather than expanding this size task.
4. Claim no package saving for source-only deletion. Its accepted value is
   removal of false ownership and maintenance surface.

### Explicit non-task - Motion Matching redesign

The `/MotionMatching` package root contributes 62.50 MiB of listed bytes, but
its full marginal transitive closure is unmeasured and it has current Hero,
GrandPa, and City17 owners. Do not redesign it in this task. Measure its full
transitive package closure first if it becomes material after the larger roots
are removed.

## Implementation outcome

The bounded cleanup is implemented. The realized architecture is:

- enabled capability providers self-register through the ProjectCore class
  provider registry; ProjectDefinitionGenerator has no provider-module list;
- ProjectObject owns one reflected capability-property resolver used by
  generation validation, initial spawn, definition reapply, and asset-bundle
  derivation;
- unresolved capabilities, properties, or imports fail generation before save
  and fail runtime application visibly;
- semicolon-delimited array input remains supported by the shared importer;
- generated ObjectDefinition Asset Registry tags export only validated
  reflected soft object/class references, and the Shipping IoStore audit checks
  that generated projection instead of scanning arbitrary JSON strings;
- the broad ProjectObject directory cook root is gone while the existing
  ObjectDefinition and LootProfileDefinition Asset Manager rules remain;
- Hero now uses a project-owned fixed body/head representation and no Mutable
  capability; MutableSample is disabled for Game targets and rejected by the
  Shipping audit;
- Hero's motion-matching bridge is a reflected soft-class property, and the one
  tracked City17 DefinitionCharacter serialized under the old string field was
  migrated and reloaded without the type-mismatch warning;
- eight unowned ObjectDefinitions and nineteen unowned first-party Blueprints
  were removed; the GrandPa integration fixture is test-owned;
- existing Hero IK and retarget assets remain because the selected fixed Hero
  still uses the current animation boundary.

The final clean package after the City17 actor migration produced:

- 2,038,005,138 bytes total (1.898 GiB), down 59.64 percent from the
  5,049,481,239-byte baseline;
- 1,248,397,392 bytes (1.163 GiB) for the largest file;
- 10,402 listed IoStore entries;
- 69 generated ObjectDefinitions present and 8 reflected capability references
  present;
- zero MetaHuman authoring entries and zero MutableSample entries;
- unchanged ProjectWorld census: 2,425 entries and 285,928,100 listed bytes.

The source-bound package completed successfully, including archive-integrity,
runtime-data staging, and generated-definition IoStore policy checks. Closure
was completed with an offscreen packaged Shipping capture of the exact City17
GrandPa actor and a focused validation of its external-actor asset.

### Acceptance reconciliation

- The packaged fixed-view route loads City17 through ProjectLoading, waits for
  World Partition, finds the exact placed `DefinitionCharacter`, and captures
  GrandPa from a deterministic front view. The image visibly contains the
  expected body, face, hair, beard, mustache, eyebrows, eyelashes, and animated
  standing pose. Its receipt proves those components are registered, visible,
  renderable, and bound to the expected cooked assets.
- The exact GrandPa external-actor asset validates with one asset checked, one
  valid, zero invalid, zero warnings, and zero unable-to-validate. City17's
  descriptor now declares its direct content-plugin dependencies. Clearing
  the serialized components was rejected: the placed definition actor owns
  serialized instance components created from its ObjectDefinition, and the
  runtime does not reconstruct that entire representation from identity at
  BeginPlay.
- Disabled, TransitionGuard, AngleClamp, and ChainIK all fail the same full
  15-phase acceptance on the fixed Hero. Historical pre-clean runs also fail,
  so no default was changed. The remaining correction is owned by one focused
  gameplay backlog task.
- The pre-existing City17 `M_Landscape` SM5 sampler overflow remains owned by
  one focused World/material backlog task. It is not evidence against the
  dependency-closure result and was not suppressed or worked around here.
- The final IoStore lists 1,418 MotionMatching entries totaling 44,362,480
  bytes (42.31 MiB). Of the named sample-gameplay suspects, only
  `SandboxCharacter_CMC_ABP` remains (45,623 bytes); `GM_Sandbox`,
  `LevelPrototyping`, and sample SmartObject entries are absent. Traversal and
  foley entries total 614,843 and 1,947,018 bytes respectively, but the listing
  does not prove that they are exclusively attributable to the AnimBP. No
  material sample-gameplay closure or redesign task is justified by this
  evidence.

## Verification

### Focused evidence per slice

- live Asset Registry dependency and referencer checks;
- repository code/data/string reconciliation for dynamic paths and IDs;
- exact generated-definition and orphan-cleanup validation;
- focused spawn-class and content-ownership regressions;
- focused package-root assertions with relevant known-bad controls;
- incremental editor/game build through project-owned scripts.

### Packaged acceptance

- clean Win64 Shipping package through the supported project route;
- boot and main menu;
- Hero and GrandPa definition-driven spawn and accepted appearance;
- GrandPa body, face, hair, beard, mustache, eyebrows, and eyelashes visible at
  the recorded City17 comparison vantage, with the same component asset and
  binding identities before and after;
- first-person camera/body and current Motion Matching movement;
- inventory pickup, drop, loot-profile, and dialogue object paths;
- Kazan, Manhattan, and City17 walkthroughs;
- no missing asset, module, class, animation, or definition failure attributable
  to a removed candidate in the packaged evidence;
- the packaged screenshot and component receipt prove the retained GrandPa
  meshes, grooms, bindings, materials, and animation boundary; this Shipping
  target does not emit a runtime log;
- the separate pre-existing City17 landscape shader failure remains isolated
  in its World/material backlog owner rather than being hidden here;
- unchanged World performance acceptance at the final candidate;
- fresh package summary and exact IoStore inventory.

### Size acceptance

- every retained slice strictly reduces the fresh package or removes a proven
  architectural dependency with no package-size claim;
- removed roots are absent from exact IoStore listings;
- `/ProjectWorldData` and supported World/map content are not reduced or moved;
- every remaining root above 100 MiB has a named current product owner;
- no arbitrary final-size target is imposed.

## Documentation impact

After implementation is accepted, update only existing owners:

- `docs/build/packaging_guide.md` for the new measured baseline and retained
  cook policy;
- `Plugins/Resources/ProjectObject/README.md` or its existing architecture doc
  for the accepted JSON-to-generated-definition cook ownership;
- `Plugins/Gameplay/ProjectSkeletalCapabilities/docs/architecture.md` and
  `docs/testing/character_parity.md` only if Hero no longer uses Mutable;

Do not add a package-size documentation hierarchy or copy measurements into
root READMEs.

## Completion

This task is complete only when:

- the retained package/cook changes and their regressions are implemented;
- all supported packaged runtime routes pass;
- before/after exact container evidence is recorded during execution;
- materially retained false owners are removed; remaining first-party dead
  source is either removed by the bounded final slice or isolated in one backlog
  task when deletion requires nontrivial migration;
- the fixed Hero Mutable closure is removed with parity or explicitly blocked
  with reproducible visual evidence;
- durable current contracts are migrated to stable owners;
- Q1 remains separate or is answered by the operator;
- this file is moved to the appropriate `todo/01_done/content/` folder.

All completion conditions owned by this task are satisfied. The inherited
first-person correction and City17 landscape material defects are isolated in
their owning backlog domains rather than retained as false blockers here.
