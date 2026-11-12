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
