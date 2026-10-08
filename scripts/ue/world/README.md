# Canonical World Realization

`realize_canonical_world.ps1` is the supported operator-controlled UE entry point
for ProjectWorld canonical validation, application, and owned-output deletion.
Release-only public projection runs this same World owner inside a detached
checkout; [release preparation](../package/README.md#prepare_release_inputsps1)
owns that checkout and its cleanup.

## Release impact routing

`test/release_preflight_plan.py` adapts the existing World planner for the
[release preflight](../package/README.md#preparation-preflight-and-failure-triage).
World owns these selections; release preparation composes its other owners and
additive operator hints. Planning is read-only and uses the shared resolved-base,
NUL-delimited, rename-safe Git collector, including untracked inputs.

| Changed World input | Focused release checks |
|---|---|
| Source, module descriptor, generated content, or runtime probe implementation | Uncooked gameplay and receipt/policy fixtures |
| Runtime JSON | Contract validation, projection fixtures, and uncooked gameplay |
| Realization profile, Presentation, or Authored JSON | Contract validation and projection fixtures |
| Other profiles or plugin schemas | Contract validation; realization/runtime/presentation/authored schemas also select projection fixtures |
| World toolkit contract schemas | Contract validation and existing World planner requirements |
| Production World scripts and toolkit implementations | Planner-selected contract checks and unexecuted World gate requirements |
| Documentation only | Documentation hygiene; no gameplay probe |

`test/validate_release_contracts.py` reuses the inline-schema validator on all
World profiles, Runtime, Presentation and Authored documents, and validates World
plugin and toolkit schema syntax. It checks existing consumers even when only
their schema changed.
The realization schema regression and release impact fixtures accompany it.

Private PIE reads existing generated maps and Runtime JSON. It cannot prove a
changed public realization profile. Public projection consumes both private and
public realization profiles, Presentation, Authored and pinned Runtime inputs;
its focused fixtures guard those bindings before final frozen projection proof.

All changed paths go to the World owner's
[release impact planning](../../../tools/World/EndToEndValidation/README.md#release-impact-planning).
Production source, scripts, tools, generated packages/manifests, data and schemas
retain its Matrix, verify, promotion and package requirements. They are reported
as unexecuted requirements
in `world_plan`; `world_gates_executed` is false. This adapter runs no Matrix,
generation or promotion and cannot replace the owning World acceptance gates.
Release fixture edits do not select a generation Matrix merely because they are
shared proof inputs. Planner-selected L1 inputs add focused contract checks;
these do not certify the planner's full L1/Matrix requirements. Final
assembled/cooked proof remains with release preparation.

## Canonical realization contract

It accepts one exact Canonical Compilation `compile_result.json`, resolves the
configured launcher engine, and writes machine-readable evidence under
`Saved/Validation/WorldRealization/<invocation-sha256>/`. The invocation
identity covers the compile receipt, presentation, authored-overlay and
optional runtime and realization profiles, operator dirty additions, target
map, realization budgets, and representation-specific requirement so distinct runs cannot
overwrite each other's evidence.

Every run requires an explicit generated map. Validate and Apply also require
an explicit presentation profile; no reusable command or helper defaults to a
concrete data plugin. Coverage names the world-data plugin, the wrapper derives
all package/data/manifest roots from its UE descriptor, and profile paths must
stay under that plugin's `Data/` root.
Validate and Apply also require `-AuthoredOverlayProfile <path>` under that
root. It declares coordinate anchors, feature anchors, masks, and protected
authored packages; all anchors resolve before any map mutation.
Use `-RuntimeProfile <path>` only when that generated map must realize and
prove the profile-pinned gameplay route. The runtime document owns route
identity, collision/navigation extent, explicit optimization policies, and
structural budgets; omitting it keeps synthetic and non-playable fixtures free
of world-specific runtime behavior. Runtime partition identity belongs to the
map manifest only. Generated layer manifests record `runtime_profile_sha256`
as `none`, and a runtime-only change must leave every layer artifact and active
manifest entry unchanged.
Use `-RealizationProfile <path>` for the layered territory route. The profile
selects typed generator ID/version pairs and owns the layer DAG, roots, and
dirty granularity. Apply derives computed dirty units from the accepted layer
manifests. `-DirtyUnit <layer_id>=<unit_id>` may add operator work but cannot
remove computed work or dependency closure. The commandlet emits exact
per-layer inventories; the wrapper rejects missing, extra, overlapping, or
out-of-owner artifacts before publishing anything.
The registered generator pairs are complete contracts, not ID-only dispatch.
Mesh Terrain owns compiled terrain sections derived from the final canonical surface, Water owns
water-only canonical cell semantics, Roads own terrain-dependent road fragments,
Vegetation owns terrain-draped instances outside its road, water, and authored-mask
dependencies, Buildings v1 own outline massing, Buildings v2 own canonical
effective-volume massing, and Gameplay owns typed object placements.
Operator cell IDs are checked against the current domain,
computed removals may reference accepted-prior cells, and dependency halos are
clipped to real cells. Future gameplay/source-tile/object-ID generators remain
unregistered until they provide their typed domain and realization together.
Automation orchestrators may provide `-EvidencePath <path>` under
`Saved/Validation/WorldRealization/`; both the wrapper and commandlet reject
other roots. Orchestrators therefore do not need to reproduce the script's
invocation identity algorithm without weakening evidence containment.

```powershell
.\scripts\ue\world\realize_canonical_world.ps1 `
  -CompileResult <path-to-compile_result.json> `
  -Map /<world-data-plugin>/Generated/<map-package> `
  -PresentationProfile <path-to-presentation.json> `
  -AuthoredOverlayProfile <path-to-authored-overlay.json> `
  -RealizationProfile <path-to-realization.json> `
  -Mode Validate
```

Use `-Mode Apply` only for the generated root owned by the compile coverage.
ProjectWorldTestData owns synthetic fixtures; ProjectWorldData owns concrete
territories. Kazan and Manhattan use their production map identities and the
registered `project_mesh_terrain:v1` adapter. Incompatible canonical dimensions
or surface contracts fail before mutation.

Modes:

- `Validate` verifies the full receipt and runs the real GeoReferencing probe
  for EPSG inputs without saving a map.
- `Apply` creates or regenerates owned actors and assets through the profile's
  selected terrain producer. Mesh Terrain profiles invoke the adapter builder
  only for Apply.
- `Delete` removes owned feature, presentation, and GeoReferencing actors,
  removes Mesh Terrain authoring and compiled artifacts, and leaves protected
  authored roots untouched. It does not load or require the former presentation
  profile.

`-MaxRoads` and `-MaxBuildings` bound source feature identities, not generated
mesh fragments. Supported territory profiles set the intended feature budget.

Each result records verified inputs, the grid-owned vertical origin, coordinate
error, actor and terrain-section counts, authored-overlay hash, per-anchor resolutions,
resolved/refused/placed/mask counts, maximum drift, presentation profile
identity, generated source bytes, and an actual-world semantic fingerprint. Repeating an unchanged Apply must
keep the fingerprint stable and update zero generated actors.

Runtime evidence additionally records the exact profile hash, route identity,
one collision probe per route-fragment cell, Recast path length, generated
source bytes, allocated procedural mesh buffer bytes, exact streaming-role
counts, a mesh-section draw-call upper bound, and executable Nanite, instancing,
and HLOD policy results. The frozen frame-time value is a target, not a
`-NullRHI` measurement. A result is rejected when any structural policy or
budget fails.
The instancing probe is required when the selected realization layers include
vegetation or vegetation actors are present. A profile that omits vegetation
records the probe as false without failing for an absent layer; full product
validation still requires an accepted instancing probe.

`audit_runtime_partition.ps1` is the read-only static gate for an existing
generated map. Pass the bounded runtime-profile paths, selected profile ID, and
an evidence path under `Saved/Validation/WorldRealization`. It measures actor
bounds, runtime-cell assignments, actor reference bundles, external-package
weight, canonical-cell identity, Mesh Terrain partition/authoring/compiled ownership, Data Layers,
loading-range coverage, and HLOD without calling realization Apply or saving
the map.

For a production terrain identity migration, run
`test/integration/verify_terrain_projection.ps1` before and after Apply with
the same map, materialized canonical compile result, and expected base/section
counts. It loads the persisted map and writes every base mesh and compiled
section's output projection into the audit receipt. Compare
`terrain_projection_sha256` and the records exactly; the projection omits the
producer identity tag and package path churn. Check package paths separately
against the migration contract. This probe saves no map.

Every run removes its previous result before launching Unreal, requires a
newly emitted accepted result, and propagates any nonzero Unreal process exit.
Apply and Delete also snapshot only the target map, its World Partition
external actor/object roots, and every participating layer root. Acceptance
discards that temporary snapshot; any rejected result, engine failure, or
missing receipt restores the exact prior files. An initially absent target is
restored to absence. A failed restoration preserves the recovery snapshot and
reports its absolute path. If a nonzero engine exit rolls back a child result
marked `accepted`, the wrapper removes that stale receipt. The focused
transaction regression lives under `test/`.

For the complete source-to-cooked-package territory gate, use the single command
owned by [`tools/World/EndToEndValidation/`](../../../tools/World/EndToEndValidation/README.md).

## Workspace cleanup

World tools keep reusable source/cache material and one rollback snapshot, but
completed comparisons and package copies must not accumulate indefinitely.
Preview and apply the bounded owner cleanup with:

```powershell
.\scripts\ue\world\cleanup_workspace.ps1
.\scripts\ue\world\cleanup_workspace.ps1 -Apply
```

`-Apply` plans and deletes as the owner of the
[generated-content lock](../generated_content/README.md), so it refuses while any
generator holds the lock, while it runs delegated inside another operation, and
while an outer-recovery marker is pending; the dry run reports a pending marker.
It also fails closed while Unreal/build processes or a durable transaction
journal exists. It never deletes the lock file, current canonical
materialization, source runs/cache, the latest L3 run, or the pinned execution
environment. Each applied cleanup writes a small receipt under
`Saved/Validation/WorldCleanup/`.

## Packaged fixed-view evidence

For one deterministic packaged-runtime visual check without foreground focus,
use `test/capture_packaged_fixed_view.ps1`. It enters the requested experience
through the normal menu and ProjectLoading route, waits for World Partition,
moves the possessed player to an explicit view, finds the expected subject,
and emits one screenshot plus a machine-readable component receipt. The
process runs offscreen with a hidden host window. The caller must supply every
map, view, subject, executable, and output identity; the script has no
product-specific defaults.

## Uncooked gameplay admission

```powershell
.\scripts\ue\world\test\integration\run_uncooked_playable_tour.ps1
```

This command builds the current Editor target incrementally once, runs the exact
native center-return policy and overhang-recovery tests, then probes both production cities in native
single-client PIE. It uses each packaged operation's `-DescribeOperation` adapter
and shared product-route arguments, so map, runtime, interaction, and return
policies are owned by those operations. `-City` scopes a diagnostic run;
shared runtime changes require the default two-city admission.

Full Editor PIE is required for the generated Mesh Partition maps: their uncooked
actor descriptors contain editor component classes. `UnrealEditor -game` does not
load those authoring modules. The native launch test starts from the configured
MainMenu with transient Standalone play settings, then the existing menu/loading
route enters production gameplay. It neither saves preferences nor regenerates
World content.

The test keeps the actual transient render target at 2560x1440 during startup
and map travel, before the native gate samples it. Product graphics application
can resize a PIE viewport; waiting for match readiness to set it is too late.
Native startup errors are reported before missing downstream traversal fields.

The existing real-input driver uses a capsule sweep to detect a blocking
underside during obstacle ascent. It releases forward input and backs upward
until clear, choosing an open lateral direction if the backward capsule sweep
is blocked. It keeps an open escape direction, bounded by a ten-second retreat
and the existing ascent/leg/tour limits. It never moves the pawn directly or
changes collision responses.
The exact native `Project.World.PlayableTour.OverhangRecovery` fixture proves
the physical underside, input transition, clearance, and refusal cleanup.

Receipts under `tmp/world/playable_tour/uncooked/<run-id>/` bind current effective
source, launched executable and modules, actual PIE/uncooked identity, map,
runtime hash, and selected return policy. Exact production data-plugin path/hash
digests must remain equal before/after, including refusal paths. The wrapper
requires real input, pause, ascent/descent, the same center cell's unload/reload,
blocked descent, and forward collision slide. Native FPS measurements remain
diagnostic; only an independently authenticated FPS-only refusal is admissible
after every gameplay and sample-validity check passes. The receipt explicitly
declares that it does not certify performance.

For the historical Manhattan geometry control, add `-City Manhattan
-KnownBadManhattanReturnControl`. This executes the ordinary return policy against
the same real map. Its receipt is diagnostic and never release admission. Count
it as geometry RED only when native movement actually rejects the slide, not
when startup or a metadata check fails.

## Packaged Kazan playable-tour acceptance

The standing Track P gate packages and tests the real Kazan product route with
the production character and collision-aware input-driven preview flight:

```powershell
.\scripts\ue\world\test\performance\run_kazan_playable_tour.ps1
```

One Development package is executed exactly three predetermined times. Every
process owns menu selection through ProjectLoading, possession, real
Space/WASD/Ctrl input, interaction, collision, World Partition unload/reload,
screenshots, and RTX 4070 High 1440p performance. A separate
Shipping package proves cook and product-route correctness. The operation-level
composite under `Saved/Validation/WorldRealization/playable-tour/<run-id>/`
authenticates the focused receipts and artifacts without replacing their owners.
Before replacing the Development package with Shipping, the runner passes the
same package root and executable hashes to the Manhattan existing-package mode
in `run_manhattan_showcase_prototype.ps1`. That mode runs three Development
product-route and playable-tour performance children, explicitly omits gameplay
interaction for the showcase, selects the precise final center return, checks
unchanged package bytes after each child,
and emits a Manhattan receipt under `Saved/Validation/WorldRealization/manhattan-showcase/`.
That receipt records SHA-256 identities for each child's correctness receipt,
product screenshot, performance receipt, sample CSV, diagnostic CSV, playable-tour
screenshot, and process log. Its normal-exit check requires terminal closure and
rejects fatal or assertion markers.
The default Manhattan prototype command still packages its own Development and
Shipping smokes; it is not the release performance route. The existing-package
mode never cooks or promotes a package.
The Development process uses UE's documented `-novsync` flag and the World gate
fails before sampling unless read-back proves VSync and `t.MaxFPS` are zero,
frame smoothing/fixed-rate/fixed-step/benchmark modes are disabled, and dynamic
resolution is disabled. These are process-local acceptance settings; they are
not saved to normal player settings. Fixed-timestep or best-of-run evidence
cannot satisfy the release gate.
Each Development child writes both UE's rich diagnostic CSV and an exact CSV
projection of the C++ performance collector frames. The runner requires every
child's functional, streaming, envelope, package, executable, source, and
minimum-sample evidence, then concatenates all exact frame samples and applies
the nearest-rank Frame p95 threshold. The base 60 FPS target is `16.67 ms`.
The release runner measures three CPU/GPU samples with the game stopped before
and after each of the three Development runs. The lowest of the six window
loads, where each window uses the higher of CPU and NVIDIA GPU utilization,
sets the host allowance: zero through 20% load, then one percentage point per
point of load above 20%, capped at 10%. The effective aggregate threshold is
the base target times this allowance. The receipt includes all six windows,
the selected load, the allowance, the base target, and the effective threshold.
This is an approximate busy-host release comparison, not proof that an idle
player receives 60 FPS. The native in-game threshold remains the base target;
its separate evidence contract is owned by
[World Partition](../../../Plugins/World/ProjectWorld/docs/world_partition.md#automated-design-and-performance-gate).
An unavailable, incomplete, or invalid load measurement fails closed. Child p95 values are
reported, never averaged or selected. A fixed count of three is release
evidence, not the separate runtime-profile tournament.
An individual child whose only rejection is the Frame p95 gate remains part of
the aggregate. UE requests process status 10 for that case, but packaged Windows
may report normal exit 0; the authenticated rejection receipt and normal-exit
log own the performance decision, while every abnormal process exit still fails.

Explicit inconclusive performance acceptance is owned by
[release preparation](../package/README.md#preparation-preflight-and-failure-triage).
The release runners preserve the native measurement and apply that decision
only after its authenticated correctness and envelope checks.

The same Shipping transaction also runs the focused Water proof in the background.
It reuses the packaged playable-tour driver's simulated `APlayerController::InputKey`
events and accepts travel relative to the actual start-to-target distance and declared
arrival radius. After the character reaches the accepted Water viewpoint, one persistent
runtime `SceneCapture2D` and render target write both images from an unchanged top-down
orthographic pose anchored to the canonical Water XY. UE's built-in `SCS_BaseColor`
capture preserves real Water/terrain depth while excluding sky and lighting from the
blue classifier. No debug material, Water-only actor list, or alternate geometry path is
used. The existing `water_temporal_stability.ps1` verifier reads base-color BGRA without
alpha composition and gates that pair with unchanged thresholds. The same Shipping
process then writes one target-centered perspective `SCS_FinalColorLDR` image using
normal product show flags; it has no second numerical verifier and is inspected directly
for player-visible Water presentation. This route uses `RenderOffScreen` and never sends
input to the operator's Windows desktop. Before the cook,
`verify_canonical_feature_viewpoint.ps1` proves that
the configured Unreal XY maps inside the expected Water feature in the exact compile
result authenticated by the active Water manifest. The Shipping receipt binds the
target-relative travel calculation, actual pawn error, persistent capture-session
identity, BaseColor pair, and FinalColor camera contract. The composite hashes all three
images. BaseColor owns automated geometry/occlusion stability; inspection of the exact
FinalColor PNG owns the normal product-appearance claim.

The operator-review package is replaced transactionally at
`Saved/PackageRelease/KazanPlayableTour/Candidate/`; its immediate predecessor is
`PreviousCandidate/`. Failed runs restore the prior Candidate. Runtime package copies
and owner scratch are removed; accepted evidence remains under its run identity.
Automation does not promote Candidate to a final product decision. After the operator
accepts those exact bytes, an owner-local acceptance receipt binds Track V to that
package, executable, source state, runtime profile, and release operation.

Package digests authenticate immutable shipped payload. They exclude only
`Windows/Alis/LocalAppData`, which Orchestrator owns, plus `Windows/Alis/Saved` and
`Windows/Engine/Saved`, which UE owns for runtime-written state beneath a packaged
install. The runner proves that executable, pak, packaged config, and other payload
changes still move the digest; runtime state creation or updates do not masquerade as a
package mutation. Relative package paths are ordered by ordinal UTF-8 bytes before
hashing. World, ProjectCinematic, and release composition use the same contract; run
`scripts/ue/world/test/package_identity.Tests.ps1` after changing any implementation.

Release provenance keeps committed and uncommitted identity separate. `source_revision`
is the exact `git rev-parse HEAD`. `source_state_sha256` is the SHA-256 of the raw binary
tracked-diff stdout relative to that HEAD plus UTF-8 byte-sorted untracked
path/content-hash entries. The shared release identity tool owns those exact bytes. The
World Candidate and ProjectCinematic release binding must emit that dirty-state digest
byte-for-byte identically; a clean revision change is rejected through `source_revision`,
not folded into `source_state_sha256`.

## Operator mutation controls

Apply, Delete, reconstruction, and enrollment first print the exact operation,
map, replaced scopes, protected authored-content root and canonical authored terrain input,
and untouched scopes. An interactive operator must enter `yes`; automation
must opt in explicitly with `-NonInteractive`. Validate is read-only and does
not prompt.

Generation history is read from the immutable active and archived manifests:

```powershell
.\scripts\ue\world\show_generated_history.ps1 -WorldDataPlugin ProjectWorldTestData
```

It reports each scope's state, generation, acceptance time, originating run,
and manifest path. It is a read-only view, not a transaction browser or a
second authority store.

## Durable authority audit

`audit_generated_authority.ps1` verifies the tracked generated tree against
the accepted manifest authority and writes ONE receipt:

```powershell
.\scripts\ue\world\audit_generated_authority.ps1 `
  -WorldDataPlugin <ProjectWorldTestData-or-ProjectWorldData> `
  -EvidencePath <receipt.json>
```

It takes the project-global content lock and the authority lock, then checks
that no transaction journal is pending, the active-manifest-set record and
every referenced manifest validate, no artifact path has two owners, every
recorded artifact is present and byte-identical, no file under the generated
roots is unowned, every consumer reference resolves to an active scope, and
every active manifest carries the CURRENT fingerprint of its owning producer.
Any failure
exits nonzero with the receipt still written.

It is a verifier, never a repair tool: it does not mutate content, manifests,
or the active set. Each producer descriptor under `Data/Producers/` declares
its ID, output revision, modules, and data inputs. The shared pipeline
descriptor declares the pipeline revision. The fingerprint includes those
revisions, the engine build identity, and declared data-input digests; source
text, descriptor formatting, and tests are not identity inputs. A changed
output implementation requires an explicit revision bump and a new verify
baseline. A stale layer fingerprint marks only that layer identity-dirty; it
does not enter the dependent layers' content dirty closure. Its producer must
refresh the layer before publishing a current fingerprint. A metadata-only
formula migration is allowed only under the
[territory identity refresh rule](../../../Plugins/World/ProjectWorld/docs/territory_contract.md).

## Fast lifecycle tests

```powershell
.\scripts\ue\world\test\run_all.ps1
```

This runs the manifest, transaction, recovery, and operator-control Pester
tests, the generated-content lock and outer-recovery tests, and the
ProjectMaterial host recovery tests. The end-to-end shared Check runs this
entrypoint once before its Matrix legs; individual matrices do not repeat it.

## 3-CORE isolated L1 lifecycle

After producing the accepted two-cell terrain/water compile result, run the
real wrapper-to-commandlet seam with:

```powershell
.\scripts\ue\world\test\integration\realization_layer_lifecycle.ps1 `
  -CompileResult <accepted-synthetic-territory-twin-compile-result.json>
```

It holds the project content lock, snapshots the TestData map and layer roots,
and uses a transient manifest authority. It proves first
enrollment, unchanged Apply, one-cell terrain-to-water closure, rejected
out-of-domain rollback, exact layer-manifest no-op, and Delete retirement. A
`Saved/Validation/WorldRealization/3-core-lifecycle/<run-id>/summary.json`
receipt and all child evidence remain for review; the outer snapshot restores
the TestData content tree even on failure. This is L1 and is not part of the
fast Pester suite or the common Check.

Add `-ProveReconstruction -ProvePackageLocality` for the intentional TestData
package-persistence proof. The runner compiles a genuine one-cell terrain
variant and a genuine water-only variant, applies both through the real
wrapper/commandlet path, and hashes the `.umap` plus every terrain proxy before
and after. It requires exactly the changed cell's proxy to move, keeps the map
and unrelated terrain bytes stable, and verifies the corresponding layer
manifest advancement. This mode is TestData-only L1 evidence and is not part
of common Check.

For the generic authored-overlay external-package and rollback proof, run:

```powershell
.\scripts\ue\world\test\integration\authored_overlay_persistence.ps1 `
  -CompileResult <accepted-synthetic-landscape-water-compile-result.json>
```

The runner is confined to `ProjectWorldTestData` and transient manifest
authority. It cold-loads an accepted anchor as a byte-preserving no-op, then
changes only that anchor, rejects the owning operation after the real external
package self-save, and proves exact package/tree restoration, unchanged active
authority, and a green read-only authority audit. The wrapper's
`-TestFailAfterSelfSavedActor` switch is internal to this proof: it refuses
production owners, interactive runs, non-Apply modes, and durable manifest
authority. The runner restores its outer TestData snapshot in `finally`, removes
owner scratch, and retains only the accepted evidence run under `Saved`.

For the production 3A topology proof, pass the explicit ProjectWorldData
compile, presentation, authored-overlay, and realization profiles plus:

```powershell
.\scripts\ue\world\test\integration\realization_layer_lifecycle.ps1 `
  -CompileResult <accepted-kazan-territory-compile-result.json> `
  -PresentationProfile <kazan-presentation-profile.json> `
  -AuthoredOverlayProfile <kazan-authored-overlay-profile.json> `
  -RealizationProfile <kazan-territory-realization-profile.json> `
  -ExpectedCellCount 210 `
  -ExpectedSampleSpacingMeters 30 `
  -MaximumGeoReferenceErrorMeters 0.01 `
  -RequireWater `
  -AllowProductionIsolation `
  -ProveReconstruction
```

Production isolation is an explicit opt-in. The runner holds the project
content lock, snapshots the exact production map, layer, presentation, and
manifest roots, and restores the outer tree even when a child fails. It proves
full Apply, manifest-stable no-op, one-cell dependency closure, rejected
out-of-domain rollback, clean semantic reconstruction, and Delete. The receipt
also authenticates Mesh Terrain partition/authoring/compiled counts, water-cell actors, georeference
error, protected Authored bytes, zero HLOD, and zero scopes after Delete. This
is review evidence only; it does not enroll durable authority or replace the
later Matrix and L3 gates.

For the production-isolated runtime-profile locality proof, run:

```powershell
.\scripts\ue\world\test\integration\runtime_profile_locality.ps1 `
  -CompileResult <accepted-kazan-territory-compile-result.json>
```

It copies current authority into a transient manifest root, migrates producer
fingerprints only inside that isolated authority, switches the map from the
baseline to one bounded candidate and back, and restores the outer generated
tree. This permits testing current producer code before durable metadata is
advanced. It accepts only when all six layer manifest entries and every layer
artifact byte remain identical. Its owner scratch is removed in `finally`.

## Reconstruction and enrollment

Matrix restores the tracked generated content to its pre-run accepted state.
Enrollment of newly gated bytes therefore goes through explicit clean
reconstruction from accepted Matrix compile evidence:

1. Enumerate the owned paths for the scope with
   `Get-ProjectWorldGeneratedPaths` - never list them by hand.
2. Remove them. Keep the shared presentation scope's consumer links under
   the same active-set transaction.
3. Run `-Mode Apply -Reconstruct -Map <package>` per map, from that gate
   run's accepted compile result. Each touched scope republishes at
   generation+1.

`-Reconstruct` is the named flag that authorizes regeneration from absence;
absence alone never does. `-EnrollManifests` is a different route - it
waives only the "no accepted manifest yet" refusal for a brand-new scope,
never the drift precondition, and the two flags are mutually exclusive.

`recover_generated_transaction.ps1 -WorldDataPlugin <owner>` completes or
rolls back an interrupted transaction from its journal, and is the only
supported response to a pending journal. A transaction removes its journal
before its snapshot, so an interruption leaves snapshot debris, never a journal
without its snapshot. A pending outer-recovery marker has its own resolver,
described with the [generated-content lock](../generated_content/README.md).

Acceptance ordering is owned by the territory contract
([layered regeneration contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#layered-regeneration-contract-scale-out-precondition)):
top-level gates run BEFORE enrollment, enrollment is the last generated-tree
mutation, and the post-enrollment audit gates the read-only package. The
locked package/gate tail and its evidence-chain receipt are one command,
`bootstrap.py accept`, owned by the end-to-end validation component.
