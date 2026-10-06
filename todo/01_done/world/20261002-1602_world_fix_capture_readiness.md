# Fix Capture Readiness for Mesh Terrain

**Status:** DONE 2026-10-04 - independent R2 PASS. The post-S5 ProjectWorldData
audit (`tmp/world/locality/s5_migration/post_s5_audit.json`) accepted all 14
current scopes and 3096 intact artifacts; the generation-locality task also
received independent R2 PASS.
**Priority:** completed before the generation-locality task (D2); small, not a release gate, and
independent of the release-required `20261002-1711_world_restore_generation_locality.md`
(invariant 4).
**Scope:** Slice A, readiness of `ProjectWorld.CaptureEvidence` (ProjectWorldEditor) and the Mesh
Terrain adapter's Editor-side report of its drawn terrain (ProjectWorldMeshTerrainEditor); Slice
B, the Visual Verification census and vantage planner on Mesh Terrain worlds.
**Stable documentation owner:** the capture stage of
[Visual Verification](../../../tools/World/VisualVerification/README.md),
[ProjectWorld pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md), and the generated
terrain representation in [World Partition](../../../Plugins/World/ProjectWorld/docs/world_partition.md).
**Origin:** P1 of `20260930-2139_content_restore_black_box_independence.md` (in `01_done/content/`);
created as the prototype `20261001-1003_world_fix_capture_readiness.md`.

## Contents

- Goal / Authority register / Non-goals
- Verified evidence
- Current architecture and source of truth / Problem and root cause
- Decision (KISS gate, alternatives) / Required invariants / Proof traceability
- Implementation tasks / Test-first and verification plan
- Documentation plan / Rollout and rollback / Completion criteria / Review record

## Goal

On a territory with Mesh Terrain, `ProjectWorld.CaptureEvidence` captures only after the terrain is
drawn in the Editor world it renders. If the terrain is never drawn within the existing timeout, the
receipt is rejected and says why. Worlds without Mesh Terrain keep today's behaviour. The capture
stays operator evidence, never an acceptance gate.

Slice B: the documented route works on current worlds. The census and the vantage planner find
generated terrain on a Mesh Terrain territory, so an operator can plan and capture Kazan or
Manhattan without a hand-made vantage plan.

## Authority register

### Operator decisions

- **D1** Track this in `00_current` as a prototype, and run `/investigate-change` before
  implementing.
  - Effect: no implementation until this investigated todo is approved.
  - Reason: not stated.
  - Date/source: 2026-10-01, operator request.
- **D2** "so we need add all todos in priority with proper scope, but don't blow up with many
  todos" (with: use `/architect` and `/investigate-change`).
  - Effect: Slice B joins this task because it has the same owner and outcome; the priority line;
    no separate todo for the census and planner.
  - Reason: not stated.
  - Date/source: 2026-10-02, operator request.
- **D3** "bro again our goal clean architecture why you are reasking, I understand that if we don't do
  properly now without any noise we are dead".
  - Effect: closes Q1. The generation-locality task deletes the producer source lists, so
    `ProjectWorldEvidenceCapture.*` leaves `map:v1` there with no separate change here; A2 holds
    for this task.
  - Reason: "if we don't do properly now without any noise we are dead".
  - Date/source: 2026-10-02, operator reply to the third-review report.
- **D4** `/implement-approved-change` with "PASS. Run it." and "if implementation will be red we
  need focus on achieve goal instead reverting, so no way back CLEAN FAST DECOUPLED ARCHITECTURE".
  - Effect: approves this todo and the generation-locality task as one autonomous job, this task
    first; red is fixed forward, not reverted.
  - Reason: clean, fast, decoupled architecture.
  - Date/source: 2026-10-02, operator request.

### Operator gates

- **Q1 [CLOSED by D3]:** Should `ProjectWorldEvidenceCapture.cpp/.h` leave the `map:v1`
  producer source list in `scripts/ue/world/generator_fingerprint.ps1`? They produce no map bytes,
  but removing them changes the `map:v1` fingerprint once, so every map manifest in both data
  plugins needs a refresh through the World wrapper, a durable authority change. Only then can the
  receipt gain a structured readiness block. - default while open: A2.

### Working assumptions

- **A1 [ACTIVE]:** The change is read-only for generated World content: no World Apply,
  regeneration, cook, package, or manifest change. The prototype's boundary came from the origin
  task's D3, which was scoped to that task.
- **A2 [ACTIVE]:** Readiness metrics go into the receipt `message` and the editor log;
  `ProjectWorldEvidenceCapture.cpp/.h` stay unchanged.
- **A3 [REJECTED by evidence]:** The shared definition's preview variant has no transformer
  pipeline, so a preview section draws its dynamic preview mesh for the whole session and never
  swaps to static meshes. If the green run shows static meshes on preview sections, readiness must
  also cover that swap: renewed review. - The swap never leaves terrain undrawn: static mesh
  components start hidden (`MeshPartitionPreviewSection.cpp:118`), and only after every section mesh
  has compiled does `OnStaticMeshBuild` empty the preview mesh and show them in the same call
  (`:394-421`). Static mesh compiles are tracked by asset compilation, which already resets
  readiness. Readiness is representation-neutral (invariant 9).
- **A4 [ACTIVE]:** Readiness observes the preview build and never forces it; capture latency is not
  a requirement.

## Non-goals

- A game-world (PIE) capture, ProjectTexture pattern pixels in Editor captures, or the runtime role
  `ProjectWorld.Terrain.v1` as the capture subject.
- Forcing MeshPartition's preview build (synchronous mode, its CVar, or a wrapper argument).
- Per-vantage subject expectations in vantage plans; any `verify_capture.py` change.
- Reclassifying `ProjectWorldEvidenceCapture.*` in the generator fingerprint (Q1); the current task
  `20261002-1711_world_restore_generation_locality.md` owns that problem.
- Census checks that duplicate generated-authority facts (artifact counts per territory): the
  authority audit already verifies every generated artifact byte for byte.
- Compiled sections, the producer contract, the terrain transformer, and generated content.
- Release gates: no 3.0.0 gate uses this route; R2 step 6 is a packaged property.

## Verified evidence

Engine citations are the installed UE 5.8 source: MeshPartition files under
`Engine/Plugins/Experimental/MeshPartition/Source/`, Core under `Engine/Source/Runtime/Core/`, the
rest under `Engine/Source/Runtime/Engine/`.

### Verified facts

1. **Readiness today.** `ProjectWorldEditorModule.cpp:190-216` loads the World Partition editor
   bounds through a loader adapter and starts a ticker; `:91-106` with
   `ProjectWorldEvidenceReadiness.h:12-25` capture after three distinct frames with zero
   `FAssetCompilingManager` remaining assets, or reject after 180 s. The command sets the receipt
   `status` and `message` (`:84-134`); `ProjectWorldEvidenceCapture.cpp` renders and writes the
   receipt. Under `-unattended` the editor exits after the receipt.
2. **The Editor never holds the role-tagged terrain.** Kazan has 1168 external actor packages; 315
   are `CompiledSection` actors, the only carriers of `ProjectWorld.Terrain.v1`
   (`ProjectWorldMeshTerrainTransformer.cpp:12`, configured only in the three compiled variants,
   `ProjectWorldMeshTerrainProducer.cpp:150-174`). `ACompiledSection::IsRuntimeOnly()` is true
   (`MeshPartitionCompiledSection.h:181`), so the descriptor is not editor-relevant
   (`WorldPartitionActorDesc.cpp:1056-1061`) and loader adapters skip it
   (`WorldPartitionActorLoaderInterface.cpp:123-126`). Neither the known-bad run (871 actors) nor
   the known-good P1 runs (872 actors, all 1170 descriptors requested) hold one.
3. **The Editor draws transient preview sections.** `APreviewSection` is `Transient`
   (`MeshPartitionPreviewSection.h:33-39`). The Editor-world subsystem calls `Update()` every tick
   (`MeshPartitionEditorWorldSubsystem.cpp:44-56`); `Update()` launches UE::Tasks preview builds
   for pending changes (`MeshPartitionEditorComponent.cpp:333-375`, `:466`). Only when all of a
   build's tasks are complete does `FinalizePreviewSectionBuilds` spawn a preview section, give it
   the built mesh, link each base modifier to it, and set it temporarily hidden when preview
   visibility is off or the section is interactive (`:1699-1856`, `UpdateLinks` `:1443-1464`).
   With no modifiers loaded, all preview sections are destroyed (`:1249-1258`).
4. **Asset compilation does not track it.** MeshPartition registers no `IAssetCompilingManager`;
   the tracked managers are listed at `AssetCompilingManager.cpp:543-550` (shader, distance-field
   and mesh-card managers register elsewhere).
5. **The race, in existing logs** (`tmp/material/blackbox/`, transient).
   `MeshPartitionMeshBuilder.cpp:414` logs `MegaMeshBuild DDC Put` when a finished build stores its
   mesh; an Editor session runs only preview builds.

   | Run | Bounds loaded | Preview build stored | Capture accepted | Terrain |
   |---|---|---|---|---|
   | wrapper only (`capture_readiness/capture.log`) | frame 0, 06:54:37.728 | frame 15, 06:54:42.122 | frame 14, 06:54:41.968 | absent |
   | P1 baseline, changed, restored, run1 baseline (`p1/**/editor.log`) | driver load at startup | frames 67-71 | frames 344-956, at least 88 s later | drawn |

   Every run logged one `Put` and no `Hit`, so each capture races a full build of about 4-5 s.
6. **Public engine API is enough.** `UModifierComponent::IsBase()` and `GetPreviewSection()`
   (`MeshPartitionModifierComponent.h:479`, `:322`);
   `UMeshPartitionEditorComponent::ForAllCurrentModifiers` and `IsAnyPreviewSectionBuildActive()`
   (`MeshPartitionEditorComponent.h:258`, `:389`); `AMeshPartition::GetMeshPartitionComponent()`
   (`MeshPartition.h:48`). The adapter already depends on `MeshPartition` and
   `MeshPartitionEditor` (`ProjectWorldMeshTerrainEditor.Build.cs`).
7. **What the capture draws.** `USceneCaptureComponent` uses Game show flags
   (`SceneCaptureComponent.cpp:169`), so a primitive needs `IsVisible()` and an unhidden owner
   (`PrimitiveSceneProxy.cpp:1568-1570`); in an Editor world `ShouldRender()` drops actors hidden
   in the Editor, including temporarily hidden ones (`SceneComponent.cpp:3485-3518`).
8. **Fingerprints bound the change.** `scripts/ue/world/generator_fingerprint.ps1` hashes, for
   `project_mesh_terrain:v1`, `ProjectWorldTerrainProducerRegistry.h/.cpp`, the adapter's
   producer, layout receipt, build pipeline, Build.cs, partition and transformer files, and the
   shared definition asset (`:94-113`); for `map:v1` it hashes `ProjectWorldEvidenceCapture.cpp/.h`
   (`:65-66`). The adapter's compiler fingerprint (`FingerprintRelativePaths` in
   `ProjectWorldMeshTerrainEditor.Build.cs`) covers the producer files as well, and a changed
   adapter fingerprint invalidates realization (pitfall 36).
   `audit_generated_authority.ps1:159-177` fails closed on a stale fingerprint as the final
   pre-package proof. `ProjectWorldEditorModule.cpp`, `ProjectWorldEvidenceReadiness.h`, tests,
   `ProjectWorldMeshTerrainEditorModule.cpp`, and new files are in no list.
9. **Baseline audit, 2026-10-02:** `audit_generated_authority.ps1 -WorldDataPlugin
   ProjectWorldData` accepted, `generator_fingerprint_current` OK, 14 scopes, 3096 artifacts
   (`tmp/world/capture_readiness/investigation/authority_audit_ProjectWorldData.json`).
10. **Consumers and controls.** `ProjectWorldEvidenceCapture.*` are used only by the console command.
    The wrapper writes its log beside the receipt and prints the receipt `message` on rejection
    (`capture_visual_evidence.ps1:83`, `:140`). `verify_capture.py` reads named receipt fields
    only. `/ProjectWorldTestData/Generated/P0/L_ProjectWorldSynthetic` holds no terrain partition;
    `MeshTerrainTwinV2` and Kazan do. `IModularFeatures` ships in Core
    (`Features/IModularFeatures.h:66`, `:124`, `:133`); the repository does not use it yet.
    ProjectWorld code and World scripts name MeshPartition once, the builder argument in
    `realization_layer_operation.ps1:498`.
14. **MeshPartition's editor headers need Engine internals (found in implementation).**
    `MeshPartitionPreviewSection.h:11` and `MeshPartitionCompiledSection.h:7` (included by
    `MeshPartitionEditorComponent.h:8`) include `MaterialCache/MaterialCacheVirtualTexture.h` from
    `Engine/Source/Runtime/Engine/Internal`, which UBT adds only to engine-scope modules
    (`UEBuildModule.cs:737`); the first build of the adapter file failed with C1083. Any module
    that inspects preview sections therefore needs that include path in its `Build.cs`. The
    adapter's `ProjectWorldMeshTerrainEditor.Build.cs` is in its own compiler fingerprint and in
    `GF`'s terrain list, and so is `ProjectWorldMeshTerrain.uplugin` (`GF:99`).
15. **The local TestData Mesh Terrain maps hold no base actors (found in implementation).**
    `MeshTerrainTwinV2` and `MeshTerrainStreamingTwin` dumps list compiled sections and the
    partition but no base actor (Kazan: 210 base actors, 315 compiled sections), and no tracked
    file produces `MeshTerrainTwinV2`. An Editor world without base actors never draws terrain:
    TwinV2 was rejected at 180 s with its partition counted as one undrawn unit. Manhattan, the second accepted
    territory, replaces TwinV2 as the second Mesh Terrain control.
11. **Census and planner still expect Landscape.** `plan_vantages.py:43-46` takes terrain bounds only
    from `LandscapeStreamingProxy` rows and exits `FATAL: no landscape proxies` otherwise;
    `census.py:61-100` expects 210 Landscape proxies, 145 water actors, and one logical Landscape.
    Neither file changed after `bf65d3d87` (2026-08-20), although the done Mesh Terrain task's
    checklist marks the census updated. The wrapper requires a plan from `plan_vantages.py`
    (`capture_visual_evidence.ps1:27`), so the documented route cannot plan a Mesh Terrain
    territory; this task's Kazan evidence uses a hand-made plan.
12. **The descriptor dump carries the runtime role.** `wp.Editor.DumpActorDescs` writes every
    descriptor in Verbose mode (`WorldPartition.cpp:198`, `:2212`), which includes `Tags`
    (`WorldPartitionActorDesc.cpp:633-640`). Kazan's compiled sections carry
    `ProjectWorld.Terrain.v1` in their saved actor data (fact 2), so their descriptors name the
    producer-neutral terrain role without loading any actor.
13. **The census water check does not transfer to Mesh Terrain.** `census.py:131-143` counts a
    Water actor as buried when its minimum Z lies below the minimum Z of the terrain descriptor
    containing its centre, which assumed one Landscape proxy per canonical cell. Mesh Terrain
    compiled sections use a producer-owned section bound that does not follow the canonical or
    runtime grid (`world_partition.md`, Generated terrain representation), so their bounds say
    nothing about the terrain under a Water actor. CanonicalCompilation owns the vertical relation:
    terrain under Water is fitted to the Water height minus `water_clearance_m`, and an authored
    height that breaks it fails closed (`terrain_surface.py:498-551`). Water actors carry the
    neutral role `ProjectWorld.Water.v1` (`ProjectWorldWaterRealization.cpp:42`).

### Inferences

- At the bad run's capture frame no preview section existed: its build stored its mesh one frame
  later, and preview sections from before the loader ran were destroyed with no modifiers loaded.
  The good runs' one extra actor and visible primitive fit one preview section, but the logged
  class histogram stops at 12 classes.
- Not material to readiness (A3): the shared definition's name table has no
  `PreviewSectionBuildVariant`, so the default variant with a null pipeline probably applies, and
  `LaunchTransformers` returns without a context (`MeshPartitionEditorComponent.cpp:613-618`).

### Assumptions / unverified areas

- Render latency after a preview section appears (proxy creation, Nanite streaming): not examined;
  the existing three-frame wait follows subject readiness.
- An interactive editor session running the command: preview sections normally already exist; not
  exercised.
- Confirmed in implementation: runtime-only compiled-section descriptors appear in the dump with
  their tags (Kazan 315, Manhattan 321).

### Refuted

- **Subject = `ProjectWorld.Terrain.v1` actors with live render state** (external review
  2026-10-02): those actors are runtime-only and absent from Editor worlds in good and bad runs
  alike (facts 2, 5), so every Kazan capture would time out.
- **Extending `FProjectWorldTerrainProducerContract` for a readiness callback** (this todo's
  previous version): its registry files are in the terrain generator fingerprint (fact 8), so the
  edit stales every Mesh Terrain manifest and fails the pre-package audit until a World Apply.
- **Per-vantage "at least one subject primitive in the frustum"**: weaker than whole-subject
  coverage and needs camera math; the route loads the full bounds.
- **Render state created as the deciding signal**: the gap is before any preview section exists.
- **Forcing the preview build may dirty packages**: forcing only adds `Build::Wait` and an
  immediate finalize to the same path (`MeshPartitionEditorComponent.cpp:474-478`); unused anyway
  (A4).

## Current architecture and source of truth

- The capture contract is the capture stage of Visual Verification. `ProjectWorldEditorModule.cpp`
  owns the console command and its readiness wait; `ProjectWorldEvidenceCapture.*` own rendering
  and the receipt; `scripts/ue/world/capture_visual_evidence.ps1` owns the envelope and map
  binding; `verify_capture.py` authenticates the files independently.
- Terrain representation and the MeshPartition boundary are owned by World Partition, generated
  terrain representation: ProjectWorldMeshTerrain alone depends on MeshPartition.
- Generated identity is owned by `generator_fingerprint.ps1` and the adapter's compiler fingerprint;
  the authority audit enforces it before packaging.

```text
owning black box:            World capture route (ProjectWorldEditor console command)
public interface / contract: evidence-subject modular feature (new, ProjectWorldEditor Public)
expected components CHANGED: capture command + readiness evaluator + its test; new interface header;
                             adapter: one new Editor module ProjectWorldMeshTerrainEvidence (subject,
                             registration, one test) and its ProjectWorldMeshTerrain.uplugin entry
                             (fact 14); Slice B: census.py, plan_vantages.py, a shared descriptor
                             reader, one new Python test; Visual Verification README, pitfalls,
                             World Partition doc, ProjectWorldMeshTerrain README
expected components UNTOUCHED: ProjectWorldEvidenceCapture.*, producer registry, every fingerprinted
                             adapter source and Build.cs, wrapper, verify_capture.py, generated
                             content, manifests
```

## Problem and root cause

Loading the full bounds makes MeshPartition queue an asynchronous preview build of the terrain.
Nothing the readiness wait counts tracks that build, so three frames with zero asset compilations
pass on frame 14 while the build finishes on frame 15 and the preview section, the only terrain an
Editor world draws, is spawned after the capture. The receipt is accepted because no check looks at
the subject. Sessions that waited at least 88 s after the build drew the terrain.

## Decision

- **Seam.** ProjectWorldEditor declares an evidence-subject interface in a new public header, as an
  `IModularFeature` with one const query: given the world, report a subject name, whether it is
  present, and counts of expected units, drawn units, and pending builds. ProjectWorldEditor alone
  turns counts into a decision. Nothing in it names MeshPartition.
- **Adapter.** A new Editor module of the adapter plugin, `ProjectWorldMeshTerrainEvidence`,
  implements the query and registers it in its own startup, unregistering on shutdown. It is the
  only module that includes MeshPartition's editor preview headers and so the only one that adds
  the Engine internal include path (fact 14); evidence code stays out of the producing modules.
  For each Mesh Terrain partition in the world: expected = its loaded base modifiers, counted
  before the editor component registers them, and one undrawn unit for a partition without any;
  drawn = those whose preview section is valid and not hidden in the Editor; pending = active
  preview builds. No partition means not present.
- **Readiness.** A pure function beside `FProjectWorldEvidenceReadiness` turns the reports into one
  decision: subjects are drawn when every present subject has expected above zero, drawn equal to
  expected, and zero pending builds; reports that are not present are ignored, and no present
  subject means drawn, so today's behaviour holds. `Advance` takes that decision; a frame with an
  undrawn subject resets the count exactly like a pending compilation. Subjects are inspected only
  on frames with zero remaining compilations.
- **Diagnostics.** The command logs subject counts when they change and appends the final counts to
  the receipt `message` for accepted and timed-out runs (A2).
- **Unchanged:** the 180 s timeout, the wrapper, `verify_capture.py`, receipt keys, and
  `ProjectWorldEvidenceCapture.*`.
- **Slice B.** `plan_vantages.py` and `census.py` parse the descriptor `Tags` field and identify
  terrain by `ProjectWorld.Terrain.v1`, never by class name. The planner takes terrain bounds from
  those descriptors. The census drops its Landscape checks and hard-coded territory counts and keeps
  structural checks: tagged terrain present and spatially loaded, territory bounds and relief, the
  lighting set, and water found by `ProjectWorld.Water.v1` present and spatially loaded. It drops the
  water-below-host-cell check (fact 13). Both stay Python with no Unreal dependency.

### Premise / KISS gate

MeshPartition's editor component owns the preview build and the drawn representation; the adapter
owns all knowledge of MeshPartition; the capture command owns when to capture. The design adds one
interface header, one implementation file, two registration calls, and one readiness parameter,
and removes nothing. The three-frame compile wait stays: pitfall 16's failure class (render state
settling after load) still applies. Given up: the shortest capture time (no forced build, A4) and a
structured receipt block (Q1).

### Alternatives considered

- **Producer contract callback:** stales terrain manifests and blocks packaging (Refuted).
- **A function registry mirroring the producer registry:** same behaviour, more code than the
  engine's own registry.
- **Force the preview build in the adapter (Epic's minimap recipe):** still needs this seam, mutates
  producer state, and alone proves nothing.
- **Wrapper passes `MegaMesh.Preview.ForceSynchronousBuild`:** a ProjectWorld script would name an
  Epic MeshPartition CVar, with no check that terrain is drawn.
- **Fixed delay or frame count:** describes the race, not a contract.
- **Role-tagged runtime actors, frustum coverage, render state only:** refuted.
- **Game-world capture:** non-goal.

## Required invariants

1. No capture while a present subject has a pending build or an undrawn expected unit; such a frame
   resets the ready count.
2. A world with no present subject captures exactly as today.
3. Fail closed: a present subject never drawn yields a rejected receipt at the existing timeout,
   with the counts in `message`.
4. No generated content or manifest changes and no fingerprinted source or `Build.cs` changes. The
   one fingerprinted path in the diff is the module entry in `ProjectWorldMeshTerrain.uplugin`,
   which fact 14 forces; under today's source-hash formula it stales the two terrain scopes'
   `generator_fingerprint_current` and nothing else. The generation-locality task replaces that
   formula and re-realizes both terrain scopes in its S5 O2, which clears it.
5. ProjectWorldEditor gains no MeshPartition, MeshPartitionEditor, or ProjectWorldMeshTerrain
   dependency, and ProjectWorld code gains no MeshPartition name.
6. The capture only observes: it never starts, forces, or cancels a build, changes no visibility,
   and saves nothing.
7. No persistent strong reference to an Editor actor; inspection is per tick.
8. Receipt keys, `verify_capture.py`, and the wrapper are unchanged; the route stays operator
   evidence.
9. Readiness is representation-neutral: it depends only on modifiers, their preview sections, Editor
   visibility, and active builds, never on whether a preview draws a dynamic or a static mesh.
   Return for review only if a live run shows the counts reporting drawn while terrain is not
   drawn.
10. The census and planner identify terrain and water only by producer-neutral runtime roles in
    descriptor tags; no Landscape or MeshPartition class name, no territory-specific count, and no
    terrain-to-water height check remains in them.

### Proof traceability

| Invariant | Acceptance surface | Execution envelope | Cheapest proof | Final proof | Stop condition |
|---|---|---|---|---|---|
| 1 | count-to-decision function; readiness counter; rendered PNGs | Editor automation; wrapper (`-RenderOffscreen -unattended`) | the subject-decision test and `Project.World.Evidence.ReadinessContract` | Kazan wrapper run: Mesh Terrain subject reported, expected above zero, drawn equal to expected, pending zero, receipt accepted, PNGs show terrain | blocks approval |
| 2 | P0 receipt and log | wrapper | subject-decision test, empty case | P0 synthetic wrapper run accepted, no subject reported | blocks approval |
| 3 | readiness decision | Editor automation | both unit tests: an undrawn subject never captures | the same tests; a live refusal is not required | blocks approval |
| 9 | adapter counts; rendered PNGs | wrapper | none below live | the Kazan and Manhattan final counts agree with terrain in the PNGs (fact 15) | renewed review |
| 10 | census receipt; vantage plan | Python | new census/planner test on descriptor lines with and without the role tag | Kazan dump planned and censused, then captured through the wrapper with that plan | blocks approval |
| 4 | fingerprints and audits | PowerShell | `git diff --name-only` against both fingerprint lists | ProjectWorldData audit: every check OK except `generator_fingerprint_current` for the two terrain scopes; accepted after the locality task's S5 | blocks merge |
| 5 | Build.cs and source | static | diff and grep | final diff review | blocks merge |
| 6, 7, 8 | source | static | diff review | final diff review; `verify_capture.py` tests | blocks merge |

## Implementation tasks

- [x] Add an exact subject-decision test for the pure count-to-decision function, built from plain
      reports with no MeshPartition objects: empty -> drawn; present with expected 0 -> not drawn;
      expected 4, drawn 3, pending 0 -> not drawn; expected 4, drawn 4, pending 1 -> not drawn;
      expected 4, drawn 4, pending 0 -> drawn; a not-present report is ignored; two present
      subjects with one not drawn -> not drawn. Observe it fail under each sabotage: ignoring
      `pending`, ignoring `drawn`, accepting expected 0.
- [x] Extend `Project.World.Evidence.ReadinessContract` with subject cases (undrawn frame resets and
      never captures; three drawn frames capture; no subject behaves as today); observe it fail by
      sabotage (evaluator ignoring the subject flag).
- [x] Add the evidence-subject interface header to `ProjectWorldEditor/Public`.
- [x] Implement the Mesh Terrain subject in the new `ProjectWorldMeshTerrainEvidence` module and
      register it in that module's startup; add one exact adapter test that it is registered.
- [x] Wire the readiness parameter, inspection on zero-compilation frames, and diagnostics into
      `ProjectWorldEditorModule.cpp`.
- [x] Build the Editor; run the exact tests; run the live controls (Kazan, Manhattan, P0; TwinV2
      became a live refusal, fact 15).
- [x] Slice B: add a Python test with descriptor lines that carry `ProjectWorld.Terrain.v1` and
      `ProjectWorld.Water.v1` and lines without them (observe it fail on today's planner and census);
      switch both tools to the role tags and remove the buried-water check and its expectation;
      dump Kazan's descriptors, plan, census, and capture through the wrapper with that plan.
- [x] Run both authority audits and compare the diff against both fingerprint lists.
- [x] Update stable documentation (Documentation plan).
- [x] Review the final diff against this todo and the stable owners.

## Test-first and verification plan

### Red evidence

- **Permanent guards (L0):** the subject-decision test and the extended
  `Project.World.Evidence.ReadinessContract`, each observed failing under its sabotage before the
  wiring lands.
- **Acceptance, known-bad, already observed with the current code:**
  `tmp/material/blackbox/capture_readiness/` holds an accepted receipt and PNGs without terrain; its
  log accepts on frame 14 and stores the preview build on frame 15. This historical run is the
  integration red; it is not re-staged.
- **DIAGNOSTIC / NON-AUTHORITATIVE:** whether a new run logs undrawn counts before the capture, and
  whether the preview build logs a `Put` or a `Hit`. Both depend on cache state and scheduling, so
  correct code may show neither. Record them when present; they gate nothing.
- **Reviewer-checked:** the bad run's oblique PNG shows no terrain; the green Kazan PNGs show it.
- **Slice B guard (L0):** the new census/planner test, observed failing on today's code (the
  planner exits on a dump with no Landscape proxies).

### Green evidence

- `.\scripts\ue\test\unit\iterate.ps1 -TestFilter <exact name>` for the subject-decision test,
  `Project.World.Evidence.ReadinessContract`, and the adapter registration test.
- `scripts/ue/build/build.bat AlisEditor Win64 Development`.
- Wrapper runs with outputs under `tmp/world/capture_readiness/<run>/`:
  - Manhattan (`-WorldDataPlugin ProjectWorldData`), replacing `MeshTerrainTwinV2` (fact 15):
    accepted; the Mesh Terrain subject is reported with expected above zero, drawn equal to
    expected, pending zero.
  - Kazan with the P1 plan (`tmp/material/blackbox/p1/vantages_p1.json`): the same counts, receipt
    accepted, terrain in both PNGs.
  - P0 synthetic: accepted, no subject present.
  - Slice B: `wp.Editor.DumpActorDescs` on Kazan, then `census.py` (no Landscape checks, tagged
    terrain present) and `plan_vantages.py`; the wrapper captures with that plan.
- `python tools/World/VisualVerification/app/verify_capture.py <receipt>` for each run, and
  `python -m unittest discover tools/World/VisualVerification/tests`.
- `audit_generated_authority.ps1` for `ProjectWorldData`: every check OK except
  `generator_fingerprint_current` for the two terrain scopes (invariant 4). `ProjectWorldTestData`
  has no enrolled active set on this machine, so its audit does not apply.
- `.\scripts\ue\check\governance\validate_no_alis_prefix.bat`; ASCII check on changed docs.

## Documentation plan

- **Authoritative stable owner:** `tools/World/VisualVerification/README.md`, capture stage.
  Replace the paragraph starting "That wait does not cover Mesh Terrain section rendering" with the
  current contract: the compile wait plus the drawn-subject wait, where subjects come from, and the
  counts in `message`. Fix the stale "route not yet implemented" (Output locations) and "the sweep
  test" (Scope). Slice B: the census section stops promising "single logical landscape" and states
  that terrain and water are found by their runtime roles in descriptor tags and that the census
  does not judge terrain-to-water heights, which CanonicalCompilation owns.
- **`Plugins/World/ProjectWorld/docs/pitfalls.md`:** new entry 37, "A settled compile queue does not
  prove Mesh Terrain is drawn" (symptom, root cause, fix, files, regression test). Pitfall 16 stays
  true for the compile wait.
- **`Plugins/World/ProjectWorld/docs/world_partition.md`, Generated terrain representation:** compiled
  sections are runtime-only; an Editor world draws transient preview sections built asynchronously
  after the authoring actors load; Editor tools learn whether terrain is drawn from the adapter's
  evidence subject, never by searching for compiled sections or the runtime role.
- **`Plugins/World/ProjectWorldMeshTerrain/README.md`:** add the Editor evidence subject to what the
  adapter owns.
- **Router / TOC update:** none; `world_partition.md` contents unchanged.
- **Duplication avoided:** the README states the contract; the pitfall states the trap and links the
  README; code comments stay local.
- Stable docs, code, comments, tests, and configuration must not reference this todo.

## Rollout and rollback

Editor-only code with no data or manifest change; rollback is reverting the change.

## Completion criteria

- Every Green item above passes, with receipts and logs under `tmp/world/capture_readiness/`.
- The Kazan PNGs show terrain, and the receipt `message` carries the Mesh Terrain subject's final
  counts with drawn equal to expected above zero and pending zero.
- The ProjectWorldData audit as invariant 4 states; the only fingerprinted path in the diff is the
  `.uplugin` module entry; `ProjectWorldEditor.Build.cs` unchanged.
- Slice B: Kazan is planned and censused from its own descriptor dump, and that plan captures
  through the wrapper.
- Documentation updated per the plan; no reference to this todo from stable files.

## Review record

### 2026-10-01 - prototype

- **Trigger:** P1's first baseline and a wrapper-only rerun returned accepted receipts without
  terrain.
- **Authority:** D1 added.

### 2026-10-02 - external review checked against code

- **Trigger:** operator asked to evaluate a reviewer's PATCH against the code.
- **Result:** accepted the drawn-subject direction, no MeshPartition dependency in
  ProjectWorldEditor, a compile-reset counter, diagnostic-only receipt data, and red/green live
  controls; rejected the role-tagged subject, per-vantage frustum, and render state alone
  (`## Verified evidence`, Refuted).
- **Authority:** no change.

### 2026-10-02 - investigation

- **Trigger:** `/investigate-change` on the prototype.
- **Root cause:** the capture's readiness passes before MeshPartition's untracked preview build
  completes (fact 5).
- **Fix:** design in `## Decision`; the producer-contract seam was refuted by the generator
  fingerprint (fact 8).
- **Verification:** read-only authority audit for `ProjectWorldData` accepted.
- **Authority:** Q1 and A1-A4 added.

### 2026-10-02 - second external review (PATCH, three findings)

- **Trigger:** operator asked to evaluate the reviewer's PATCH against the code.
- **Result:** all three findings accepted. Live acceptance no longer requires observing undrawn
  frames or a cache `Put`; both are diagnostic, and the historical run stays the known-bad. The
  dynamic-versus-static condition is dropped (invariant 9). A pure count-to-decision function gets
  its own sabotage-checked test. Q1's problem moved to its own prototype; the origin task names the
  old prototype file only in historical prose, so it is unchanged.
- **Authority:** A3 rejected by evidence (`MeshPartitionPreviewSection.cpp:118`, `:394-421`).

### 2026-10-02 - priorities and scope across current todos

- **Trigger:** operator asked for all todos in priority with proper scope and without many new
  files, through `/architect` and `/investigate-change`.
- **Result:** the generation-locality investigation found that the Visual Verification census and
  planner still require Landscape (facts 11-12); same owner and outcome, so it joined as Slice B.
  The fingerprint-scope prototype was absorbed into the new generation-locality task.
- **Authority:** D2 added.

### 2026-10-02 - third external review (Slice A PASS, Slice B PATCH)

- **Trigger:** operator asked to evaluate the reviewer's PATCH against the code.
- **Result:** Slice A unchanged. Slice B accepted: the water-below-host-cell check assumed one
  Landscape proxy per canonical cell, which Mesh Terrain sections do not follow, and
  CanonicalCompilation already owns the terrain-to-water relation (fact 13); the census now finds
  water by its neutral role and keeps presence and placement only.
- **Authority:** no change.

### 2026-10-02 - implementation

- **Trigger:** the operator approved both World todos as one autonomous job (D4).
- **Built:** the evidence-subject seam, the readiness decision and wiring, the Mesh Terrain subject
  in a new `ProjectWorldMeshTerrainEvidence` module, and Slice B on a shared descriptor reader
  (`actor_descriptors.py`).
- **Found:** facts 14 and 15. Fact 14 moved the subject out of the producing module and makes the
  `.uplugin` entry the one fingerprinted change (invariant 4 amended); fact 15 replaced TwinV2 by
  Manhattan as the second Mesh Terrain control.
- **Red:** each sabotage failed its exact test on the intended assertion: ignoring pending, ignoring
  drawn, and accepting expected 0 (`SubjectDecision`); ignoring the subject flag
  (`ReadinessContract`); dropping the registration (`EvidenceSubjectRegistered`). The Slice B test
  failed 4 of 4 on the previous census and planner (planner exit 2 on role-tagged terrain, Landscape
  checks failing, a Landscape-era dump still planned).
- **Green:** the three exact tests pass; `python -m unittest discover tools/World/VisualVerification/tests`
  ran 16, OK. Live, outputs under `tmp/world/capture_readiness/`:

  | Run | Receipt | Subject counts | Notes |
  |---|---|---|---|
  | Kazan, own plan (`kazan_final`, `r2_fix/kazan`) | accepted, 9 views, authenticated | expected 210, drawn 210, pending 0 | terrain in every PNG; first inspected frame 210, 0, 1 |
  | Kazan, P1 plan (`kazan_p1`) | accepted, 2 views, authenticated | 210, 210, 0 | the known-bad oblique vantage now shows terrain |
  | Manhattan, own plan (`manhattan_final`, `r2_fix/manhattan`) | accepted, 9 views, authenticated | 210, 210, 0 | terrain in the PNGs; frame 9 refused at 210, 0, 0 before any build |
  | P0 synthetic, hand plan (`p0_final`) | accepted, 2 views | none present | captured on frame 3, as before |
  | TwinV2 (`r2_fix/twinv2`) | rejected at 180 s | expected 1, drawn 0 | no base actors (fact 15); one undrawn unit |

  DIAGNOSTIC / NON-AUTHORITATIVE: an instrumented Kazan run logged frame 10 with 210 base
  modifiers loaded and none registered with the editor component, frame 11 with all 210 registered
  and a build active, the preview build `Put` on frame 52, and every unit drawn on frame 60,
  capturing on frame 62. The subject therefore counts loaded modifiers, not registered ones.
- **Slice B:** Kazan dump 1170 descriptors, census PASSED (315 terrain, 145 water, relief 139.8 m),
  9 vantages; Manhattan census PASSED (321 terrain, 115 water, relief 196.5 m). The planner refuses
  P0 and a Landscape-era dump (exit 2).
- **Audit:** ProjectWorldData rejected only on `generator_fingerprint_current` for
  `layer_kazan_territory_v1_terrain` and `layer_manhattan_showcase_v1_terrain`; active set 14
  scopes, manifests valid, 3096 artifacts byte-identical, no unowned file. The diff's only
  fingerprinted path is `ProjectWorldMeshTerrain.uplugin`. `validate_no_alis_prefix.bat` OK;
  `validate_engine_env.py --paths-only` OK; changed docs ASCII.
- **Authority:** D4 added.

### 2026-10-02 - independent R2 (PATCH, two text findings)

- **Trigger:** a fresh non-author reviewer checked the diff and the evidence against this todo.
- **Required, fixed:** the subject's comment said "loaded" while the code counted the editor
  component's registered modifiers; pitfalls 11 and 12 still described the Landscape-era census
  and planner. The fix for the first goes beyond the comment: the subject now counts loaded base
  modifiers aimed at the partition, as the Decision states, so a missing or late editor component
  reads as undrawn.
- **Non-blocking, taken:** a second partition with nothing loaded can no longer hide behind a
  drawn one (one undrawn unit per such partition); the registration sabotage was rerun with its
  editor log kept (`r2_fix/editor_drop_registration.log`); the pending capture no longer stores
  reports it never reads.
- **Non-blocking, noted:** `plan_vantages.py` had mixed line endings and is now LF; the green unit
  runs predating the sabotage restores were repeated on the final build (`r2_fix/run.out`).
- **Verification:** the three exact tests green; `drop_registration` red on
  `The module registers the Mesh Terrain evidence subject exactly once`; Kazan accepted at 210, 210,
  0; TwinV2 rejected at 1, 0, 0.
