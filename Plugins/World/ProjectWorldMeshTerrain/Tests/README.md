# Canonical region observations

Owns preparation for `Project.World.Realization.MeshTerrain.CanonicalRegion`.
This optional native observation reads a fully authenticated canonical result,
selects one complete original cell (or the explicit two-cell combined case), and realizes its actual Terrain with ordinary
Water in a disposable scene through the existing native provider/channel builder.
It does not modify the selected canonical authority.

Use the [pinned World Python environment](../../../../tools/World/ExecutionEnvironment/README.md).
The [preparer](prepare_region.py) accepts explicit compiler and Water profiles
and canonical XY. Without a supplied result, it uses the public canonical
materialization API. `--canonical-result` requires `--result-sha256` and provides
an explicit immutable consumer input; it does not establish current selection.

```powershell
python -B Plugins/World/ProjectWorldMeshTerrain/Tests/prepare_region.py tmp/world/region/input `
  --compiler-profile <compiler-profile> --water-profile <realization-profile> `
  --xy <canonical-x> <canonical-y>

$env:ALIS_NO_PERSISTENT_EDITOR = '1'
$env:PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE = (Resolve-Path 'tmp/world/region/input/input.json').Path
$env:PROJECT_WORLD_COMPOSITE_TERRAIN_SHA256 = (Get-FileHash $env:PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE -Algorithm SHA256).Hash.ToLowerInvariant()
$env:PROJECT_WORLD_COMPOSITE_TERRAIN_OUTPUT = Join-Path (Get-Location) 'tmp/world/region/native'
$settings = Join-Path (Get-Location) 'tmp/world/region/editor.ini'
scripts/ue/test/unit/run_cpp_tests_safe.ps1 -Mode Gate -Map '/Engine/Maps/Entry' `
  -TestFilter '^Project.World.Realization.MeshTerrain.CanonicalRegion$' `
  -ExpectedTestNames 'Project.World.Realization.MeshTerrain.CanonicalRegion' `
  -UseTestExitOnly -TimeoutSeconds 600 -LogDirectory tmp/world/region/log `
  -ExtraArgs "-ProjectSkipFrontEnd -SCCProvider=None -nop4 -RenderOffscreen -EditorPerProjectUserSettingsINI=$settings"
```

Input and output directories must be fresh. The observation excludes basin
gameplay, storage experiments and implicit typed-geometry fallback. Its native
controls compare every selected canonical triangle, including unchanged grids,
before and after save/reload. `UnrefinedSurfaceFidelity` checks the all-field
comparator with missing, raised and overlapping faces and empty-input controls.
It also rejects extra faces outside the selected field and degenerate faces;
matching expected faces alone must not hide additional generated geometry.
The installed native automation selector otherwise matches substrings. Anchors
select this one exact test and avoid its Selection/View siblings; `Gate` permits
that native exact-selector syntax through the wrapper's source-literal guard.
The existing `CanonicalAuthoredDry` route keeps its stricter authored-refinement
and distinguishing-query requirements and mechanical transformer scope.

The bounded [Pier25 preparer](prepare_region_pier.py) selects the admitted
structure's two original membership cells. It authenticates the complete candidate
and immutable base before producing an input with `cell_ids`, per-cell query
witnesses and `constructed_feature_id`. All original Terrain artifacts and selected
ordinary Water features must remain unchanged. It reuses the exact affine-field
certificate, native polygon clipping and entire-cap contact oracle to measure
deck/Ground penetration, both complete shore caps and clearance over the actual
overlapping Water polygons. Its captured model is explicitly inferred game-world
access geometry, not an observed or surveyed deck.

For a composed qualified Ground/bank/Pier child, use
[prepare_region_shore.py](prepare_region_shore.py) with `--canonical-result`,
`--result-sha256` and `--water-profile`. It checks final artifact bindings,
complete deck clearance and both full-width contacts, including the profile's
Water presentation offset. It observes both complete membership cells through
the same `CanonicalRegion` route. The unchanged-Terrain preparer retains its
separate captured-model guard.

```powershell
python -B Plugins/World/ProjectWorldMeshTerrain/Tests/prepare_region_pier.py tmp/world/pier/input `
  --canonical-result <admitted-candidate-result> --result-sha256 <candidate-sha256> `
  --base-result <immutable-base-result> --base-sha256 <base-sha256> `
  --water-profile <realization-profile>
```

Pass the resulting `input.json` through the same exact `CanonicalRegion` command
above. This projection is only for a new disposable World: it must never be applied
to an established city, where omitted cells could be removed. The registered
constructed-surface producer realizes both fragments without copying another
scene's actor packages. Its explicit existing `BasicShapeMaterial` fixture binding
is the same as `RegisteredSourceOutput`; accepted Terrain and ordinary Water
materials remain intact. This is a physical access fixture, not road art acceptance.

Terrain-only physics and combined access physics are separate proofs. During fresh
reload Terrain probes, only registered-owner-authenticated structures in this exact
scratch scene temporarily lose collision. Scoped cleanup restores every original
mode and profile after isolation; refused preflight performs no collision writes.
The component inventory is recorded. The restored combined scene
then requires every saved structural face's world ray to hit its own fragment;
disabled-fragment controls cannot borrow Ground as structural support. The final
capture uses restored components. Neither phase proves Hero traversal, navigation,
streaming, full-world Water equivalence or geographic acceptance.
Replay authenticates every live external actor's exact saved package, then refreshes
native actor discovery from that bounded authenticated file inventory before loading
the saved map. This follows the existing VerifyWorld reload method; explicitly
loading every regional actor does not prove runtime streaming.

`Project.World.Realization.MeshTerrain.CanonicalRegionSelection` checks the original
one-cell contract, explicit two-cell selection and duplicate/missing-primary/domain
refusals without building a regional scene. It also exercises numeric/array/null
query-identity refusal, duplicate-observed Water bindings and actual native
component collision profile/mode restoration. This exact test can use the safe
runner's `-NoRHI` envelope; it makes no render or regional geometry claim. Its pure
input controls also run before the runtime view. The small
[preparation controls](test_prepare_region_pier.py) prove snapshot refusal preserves
inputs, retain bilinear-query versus physical-triangle semantics and reject an
actual full-width Water intrusion. Native section, package and fragment censuses
remain mandatory after preparation.

`PhysicsProbePackageIsolation` exercises the actual foreign-box collision
calibration in an external-actor world. The probe must remain transient and must
not create a new external actor package. Saved replay authenticates every owned
package; it never filters missing package files out of its census.

Replay snapshots package names before releasing the original world. Editor
cleanup may already unload those packages, so replay resolves the remaining
loaded names again rather than dereferencing old package pointers. All original
instances must still be released. The fresh World Partition map explicitly loads
its saved actors before checking their exact identities, section fields and
physics; that residency operation is not a streaming test.

Regional appearance requires the native section channel texture/table, an actual
component material derived from the accepted definition material, and a complete
shader resource for the observing world's platform. The capture uses the engine's
synchronous material-readiness operation. A complete material UObject is not a
shader-readiness certificate. Injecting mesh data into a transformer alone proves
neither provider/channel generation nor accepted material appearance.

Regional images select the `nanite` visual variant. Other co-resident test
variants have visibility and shadow casting suppressed for the capture scope,
then restored. The receipt binds the selected variant and positive component
census; multiple sections of that same variant are allowed. Whole-field and
collision comparisons still cover all three configured variants. Rendering
coincident Nanite and fallback test meshes together is not a platform view.

Images use controlled diagnostic lighting and manual exposure. The receipt binds
the actual light intensity, exposure brackets, base-color/normal capture sources
and image hashes. A depth image has an empty-geometry negative control. Neither
nonempty depth nor a complete shader proves a useful final-color image: inspect
the retained images for dark, clipped or otherwise misleading presentation. These
synchronous diagnostic captures do not replace the actual World lighting,
stable-frame capture or player-eye acceptance route.

The native child retains the parent's explicit `InstalledNoZenLocalFallback`
selection only with a validated scratch-local `LocalDataCachePath`. Without
that override it retains the existing native cache defaults. Cache selection
does not alter the geometry or source authority.

Unrefined public Terrain queries use bilinear interpolation. Physical grid
triangles are piecewise affine. The preparer keeps API-query witnesses separate
from physical collision witnesses instead of treating one as the other.

Water uses the registered producer and the profile's settings, with the existing
accepted presentation binding. Saved package identities, actual GPU vertices and
indices, material and nonphysical ownership are observed before and after reload.
Water assets unload with the original scene; missing Water cannot pass an empty
census. Actual contact analysis must use those rendered Water triangles and
Terrain collision, not a solid-Water ray test.

The reduced cell set changes Water's CDT preparation domain. Receipts label that
scope explicitly. They do not prove full-world Water equivalence, source-quality
admission, visual plausibility, Hero traversal, streaming or either city's
acceptance. Those properties require their actual consuming boundaries.

## Saved regional runtime views

`Project.World.Realization.MeshTerrain.CanonicalRegionView` observes an existing
saved regional scene in PIE without rebuilding it. The
[view preparer](prepare_region_view.py) pins the complete saved package inventory,
original canonical fixture, presentation and camera plan. The original fixture
must match the saved section receipt; the native canonical reader authenticates
its full field before selecting the cell.

For the two-cell case the same observer consumes every saved selected-variant
`native_sections` identity and both declared structural fragments. It rejects
missing, duplicated or unowned fragments and requires their actual visible Nanite
geometry, fixture material and blocking physical role. Hiding an actual fragment
is a negative control. Source packages remain pinned through shutdown.

```powershell
python -B Plugins/World/ProjectWorldMeshTerrain/Tests/prepare_region_view.py tmp/world/region_view `
  --scene <saved-native-observation-root> --original-fixture <original-input-json> `
  --presentation-profile <presentation-profile> --vantage-plan <vantage-plan>
$view = Get-Content tmp/world/region_view/launch.json -Raw | ConvertFrom-Json
$viewInput = (Resolve-Path $view.input).Path
$viewPlugin = (Resolve-Path $view.plugin).Path
$env:ALIS_NO_PERSISTENT_EDITOR = '1'
scripts/ue/test/unit/run_cpp_tests_safe.ps1 -Mode Dev -Map $view.map `
  -TestFilter 'Project.World.Realization.MeshTerrain.CanonicalRegionView' `
  -ExpectedTestNames 'Project.World.Realization.MeshTerrain.CanonicalRegionView' `
  -UseTestExitOnly -TimeoutSeconds 240 -LogDirectory tmp/world/region_view/logs `
  -ExtraArgs "-ProjectSkipFrontEnd -RenderOffscreen -PLUGIN=`"$viewPlugin`" -ProjectWorldRegionalView=`"$viewInput`" -ProjectWorldRegionalViewHash=$($view.input_sha256)"
```

The [native observer](../Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainRegionalViewTests.cpp)
requires actual compiled sections selected by native platform policy, drawn
Nanite components, accepted material/channel resources and ordinary Water.
It waits for three distinct ready frames and uses the existing
[WorldEditor presentation/capture bridge](../../ProjectWorld/Source/ProjectWorldEditor/Public/Tests/ProjectWorldRegionalEvidence.h).
Sun and exposure values are read back from the applied PIE actors. Material
completion uses the existing native readiness operation before shader inspection.

Controls refuse an editor-world census, hidden compiled visuals, wrong variant
requirements, substituted material, missing visible Water and stale profile/plan
bytes. Cleanup cannot pass with PIE still active. End-PIE must preserve every
consumed saved package, original fixture and definition. This is an appearance
observation; it does not prove Hero traversal, navigation, streaming, full-world
Water equivalence or geographic admission.

The [preparation controls](test_prepare_region_view.py) run without Unreal:

```powershell
python -B -m unittest discover -s Plugins/World/ProjectWorldMeshTerrain/Tests -p test_prepare_region_view.py
```

## Gameplay applicability

`CanonicalRegion` explicitly excludes basin gameplay. `CanonicalRegionView`
observes the saved scene using a hidden default pawn, not the configured Hero.
Their field, collision and appearance receipts do not establish character
movement, navigation or streaming.

View and `CanonicalRegionGameplay` register only when their input or hash option
is declared. Ordinary regression discovery does not manufacture saved-scene inputs;
declared malformed/missing pins still fail. Exact candidate observers remain
separate required evidence at their consuming boundary.

The existing [basin gameplay scene](../Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainBasinScene.cpp)
samples first-cell refined triangles and constructs basin-specific routes.
The [constructed-pawn fixture](../../ProjectWorld/Source/ProjectWorldEditor/Private/Tests/ProjectWorldConstructedPawnPIETest.cpp)
creates a separate StaticMesh scene from its grouped input. Neither accepts an
arbitrary saved regional Terrain/Water/deck scene as a gameplay input.
Their pawn, movement and streaming helpers remain reusable, but passing those
fixtures cannot certify movement on a different saved candidate. The owning
[test-layer contract](../../../../docs/testing/world_pipeline_layers.md)
requires proof on the actual representation and admitted playable route.

The [regional gameplay adapter](../Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainRegionalGameplayTests.cpp)
consumes that same saved scene directly. It shares the
[authenticated scene loader and runtime census](../Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainRegionalScene.h)
with the view test. It does not create replacement Terrain, Water or structural
geometry. The normal configured GameMode must possess the expected definition-driven
Hero through the existing [pawn helper](../../ProjectWorld/Source/ProjectWorldEditor/Public/Tests/ProjectWorldConstructedRuntimePawn.h);
generic characters and capsules cannot satisfy this route.

Use [prepare_region_gameplay.py](prepare_region_gameplay.py) with an existing
saved-view input and explicit source-axis intervals. Consecutive source legs must
share their exact original endpoint; the preparer never invents a connector. It
pins the saved scene and source-route selection and performs a stationed support
preflight. This preflight is not a complete corridor certificate or a walking proof.
The actual capsule dimensions and continuous contacts are verified in native PIE.

[prepare_region_shore_gameplay.py](prepare_region_shore_gameplay.py) additionally
derives two bounded Ground/Upper contact approaches from the admitted ribbon's
actual cap edges. These routes are explicitly inferred diagnostics, distinct
from the unchanged mapped-axis selections. Its close-view camera plan uses the
same saved scene. The [contact controls](test_prepare_region_shore_gameplay.py)
check the incident-face direction and refuse missing or ambiguous full-width caps.

```powershell
$prepared = python -B Plugins/World/ProjectWorldMeshTerrain/Tests/prepare_region_gameplay.py `
  tmp/world/region_gameplay/input --scene-input <saved-view-input> --routes <source-axis-selection> |
  ConvertFrom-Json
$input = (Resolve-Path $prepared.input).Path
$plugin = (Resolve-Path $prepared.plugin).Path
$env:ALIS_NO_PERSISTENT_EDITOR = '1'
# Set the explicit configured GameMode, pawn class and definition expected by the pawn helper.
scripts/ue/test/unit/run_cpp_tests_safe.ps1 -Mode Gate -Map $prepared.map `
  -TestFilter '^Project.World.Realization.MeshTerrain.CanonicalRegionGameplay$' `
  -ExpectedTestNames 'Project.World.Realization.MeshTerrain.CanonicalRegionGameplay' `
  -UseTestExitOnly -TimeoutSeconds 900 -LogDirectory tmp/world/region_gameplay/logs `
  -ExtraArgs "-ProjectSkipFrontEnd -SCCProvider=None -nop4 -RenderOffscreen -PLUGIN=`"$plugin`" -ProjectWorldRegionalGameplay=`"$input`" -ProjectWorldRegionalGameplayHash=$($prepared.sha256)"
```

The [support oracle](../Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainRegionalSupport.h)
reads the authenticated saved physical fields. Terrain remains required under a
separate structure; the highest admitted support is compared with the actual
ordinary Water field, including varying elevations. Missing or contradictory
support and submerged route candidates refuse. A mapped source footpath alone
does not establish dry support or authorize grading; its role may require separate
structural context.

The Hero receives ordinary CMC movement input in both directions. Every positive
frame requires a declared owning floor, a matching live world ray and known dry
support. The actual CMC floor impact must coincide with admitted top support.
Feet use the existing native floor-distance/capsule-slope envelope over the
owning layer's faces within the capsule footprint; buried Terrain cannot enlarge
a structural deck's tolerance. One contact normal alone is insufficient at a
triangle junction. An actual unsupported Hero position must fall
and refuse the same checker before the supported route is tested. Route-start
repositioning is diagnostic; it does not establish city spawn placement or travel.
PIE shutdown must leave every pinned scene input unchanged.

`Project.World.Realization.MeshTerrain.CanonicalRegionSupport` checks the pure
support, varying Water, missing-layer, captured capsule-junction and flat-deck
over steep buried Ground cases using
the safe runner's exact anchored `-NoRHI` envelope. Ground validation and sampling
use the exact final face census from the authenticated selected cell grids and
refined quads. Missing or additional saved faces refuse. Structures retain their
independent 4,096-face bound. `Project.World.Realization.MeshTerrain.RegionalGroundCapacity`
covers two complete cells with a legal dense quad, successful large-field sampling,
missing-face refusal and unchanged structural refusal. The
[offline route controls](test_prepare_region_gameplay.py) run without Unreal:

```powershell
python -B -m unittest discover -s Plugins/World/ProjectWorldMeshTerrain/Tests -p test_prepare_region_gameplay.py
```

Passing a declared route proves that route's direct CMC traversal on that saved
candidate. It does not prove the complete source way, input-device integration,
animation parity, navigation, streaming, full-world Water equivalence or geographic
admission. Those consumers retain their own acceptance paths.

### Walking an actual saved product scene

The [saved-scene adapter](../Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainSavedRegionScene.h)
reuses this checker against the selected product map. Regional packages remain
authenticated reference expectations; their disposable-map guards are unchanged.
A separately pinned proof binds the same canonical result and current World,
map, Terrain and optional structure manifests. Every actual floor must belong
to one of those saved native owners, with exact PIE package normalization.
An actual tagged foreign cube must ground the Hero yet fail the same ownership
check before supported walking starts. Existing per-frame height, capsule,
Water and bidirectional movement checks remain required.

The [selected consumer host](../../../../scripts/ue/world/selected_refinement_gameplay.ps1)
prepares the proof and invokes the existing exact test with paired
`ProjectWorldSavedRegionGameplay` path/hash arguments. It uses a content-only
reference mount and restores diagnostic pawn environment on every exit.
The acceptance plan can request both cities' saved routes through its owning
[consumer envelope](../../../../tools/World/EndToEndValidation/README.md).
Regional walking, the ordinary city tour and these saved-scene routes are
distinct proofs; none substitutes for another scene or grants geographic adoption.
