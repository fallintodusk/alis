# Canonical World Realization

`realize_canonical_world.ps1` is the supported operator-controlled UE entry point
for ProjectWorld canonical validation, application, and owned-output deletion.

It accepts one exact Canonical Compilation `compile_result.json`, resolves the
configured launcher engine, and writes machine-readable evidence under
`Saved/Validation/WorldRealization/<invocation-sha256>/`. The invocation
identity covers the compile receipt, presentation, authored-overlay and
optional runtime and realization profiles, operator dirty additions, target
map, realization budgets, and Landscape requirement so distinct runs cannot
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
The four executable v1 pairs are complete contracts, not ID-only dispatch:
Landscape owns terrain canonical cells, water owns water-only canonical cell
semantics, roads own terrain-dependent road fragments, and vegetation owns
terrain-draped instances outside its road, water, and authored-mask dependencies.
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
ProjectWorldTestData owns fixtures; ProjectWorldData owns production Kazan. Add
`-RequireLandscape` when validating a real Landscape envelope; incompatible
canonical dimensions fail rather than resample.

Modes:

- `Validate` verifies the full receipt and runs the real GeoReferencing probe
  for EPSG inputs without saving a map.
- `Apply` creates or regenerates owned actors and assets. Compatible grids use
  one stock Landscape; incompatible fixtures retain exact procedural terrain.
- `Delete` removes owned feature, presentation, and GeoReferencing actors,
  clears generated Landscape layers, and preserves the `Authored Corrections`
  layer. It does not load or require the former presentation profile.

`-MaxRoads` and `-MaxBuildings` bound source feature identities, not generated
mesh fragments. P0 defaults to one road and four buildings so a real provider
snapshot cannot accidentally expand the prototype content budget.

Each result records verified inputs, the grid-owned vertical origin, coordinate
error, actor/section/component counts, changed Landscape components, protected
authored-layer identity, authored-overlay hash, per-anchor resolutions,
resolved/refused/placed/mask counts, maximum drift, presentation profile
identity, generated source bytes, and an actual-world semantic fingerprint. Repeating an unchanged Apply must
keep the fingerprint stable and update zero Landscape components.

Runtime evidence additionally records the exact profile hash, route identity,
one collision probe per route-fragment cell, Recast path length, generated
source bytes, allocated procedural mesh buffer bytes, exact streaming-role
counts, a mesh-section draw-call upper bound, and executable Nanite, instancing,
and HLOD policy results. The frozen frame-time value is a target, not a
`-NullRHI` measurement. A result is rejected when any structural policy or
budget fails.

`audit_runtime_partition.ps1` is the read-only static gate for an existing
generated map. Pass the bounded runtime-profile paths, selected profile ID, and
an evidence path under `Saved/Validation/WorldRealization`. It measures actor
bounds, runtime-cell assignments, actor reference bundles, external-package
weight, canonical-cell identity, Landscape proxy ownership, Data Layers,
loading-range coverage, and HLOD without calling realization Apply or saving
the map.

Every run removes its previous result before launching Unreal, requires a
newly emitted accepted result, and propagates any nonzero Unreal process exit.
Apply and Delete also snapshot only the target map, its World Partition
external actor/object roots, every participating layer root, and generated
presentation material. Acceptance
discards that temporary snapshot; any rejected result, engine failure, or
missing receipt restores the exact prior files. An initially absent target is
restored to absence. A failed restoration preserves the recovery snapshot and
reports its absolute path. If a nonzero engine exit rolls back a child result
marked `accepted`, the wrapper removes that stale receipt. The focused
transaction regression lives under `test/`.

For the complete source-to-cooked-package P0 gate, use the single command
owned by [`tools/World/EndToEndValidation/`](../../../tools/World/EndToEndValidation/README.md).

## Workspace cleanup

World tools keep reusable source/cache material and one rollback snapshot, but
completed comparisons and package copies must not accumulate indefinitely.
Preview and apply the bounded owner cleanup with:

```powershell
.\scripts\ue\world\cleanup_workspace.ps1
.\scripts\ue\world\cleanup_workspace.ps1 -Apply
```

The script fails closed while Unreal/build processes or a durable transaction
journal exists. It retains `final_p0`, current canonical materialization,
source runs/cache, the latest L3 run, and the pinned execution environment.
Each applied cleanup writes a small receipt under
`Saved/Validation/WorldCleanup/`.

## Operator mutation controls

Apply, Delete, reconstruction, and enrollment first print the exact operation,
map, replaced scopes, protected Authored root and Landscape correction layer,
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
or the active set. Producer fingerprints use explicit small source sets:
shared byte-producing primitives participate in every affected producer, while
terrain, water, road, vegetation, presentation, and map implementation files
participate only in their owners. Vegetation also fingerprints its admitted
mesh assets and exclusion-geometry implementation. Pure catalog, tuple
validation, profile-schema, and dispatch additions are excluded because they
cannot change an existing producer's bytes; their executable behavior remains
covered by profile and end-to-end validation. Direct placement and lifecycle
dependencies are listed in each producer that consumes them. A stale layer
fingerprint injects whole-layer dirty work before manifest publication, so
metadata cannot advance over artifacts skipped by the current producer. The
auditor and
test sources are excluded because editing them cannot change generated bytes
or manifest documents.

## Fast lifecycle tests

```powershell
.\scripts\ue\world\test\run_all.ps1
```

This runs the manifest, transaction, recovery, and operator-control Pester
tests. The end-to-end shared Check runs this entrypoint once before its Matrix
legs; individual matrices do not repeat it.

## 3-CORE isolated L1 lifecycle

After producing the accepted two-cell Landscape/water compile result, run the
real wrapper-to-commandlet seam with:

```powershell
.\scripts\ue\world\test\integration\realization_layer_lifecycle.ps1 `
  -CompileResult <accepted-synthetic-landscape-water-compile-result.json>
```

It holds the project content lock, snapshots the TestData map, layer and shared
presentation roots, and uses a transient manifest authority. It proves first
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
also authenticates Landscape proxy count, water-cell actors, georeference
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
2. Remove them. Remove the shared presentation root ONCE, before the first
   map; afterwards it matches its freshly published manifest and the
   remaining maps pass their drift check.
3. Run `-Mode Apply -Reconstruct -Map <package>` per map, from that gate
   run's accepted compile result. Each touched scope republishes at
   generation+1.

`-Reconstruct` is the named flag that authorizes regeneration from absence;
absence alone never does. `-EnrollManifests` is a different route - it
waives only the "no accepted manifest yet" refusal for a brand-new scope,
never the drift precondition, and the two flags are mutually exclusive.

`recover_generated_transaction.ps1 -WorldDataPlugin <owner>` completes or
rolls back an interrupted transaction from its journal, and is the only
supported response to a pending journal.

Acceptance ordering is owned by the territory contract
([layered regeneration contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#layered-regeneration-contract-scale-out-precondition)):
top-level gates run BEFORE enrollment, enrollment is the last generated-tree
mutation, and the post-enrollment audit gates the read-only package. The
locked package/gate tail and its evidence-chain receipt are one command,
`bootstrap.py accept`, owned by the end-to-end validation component.
