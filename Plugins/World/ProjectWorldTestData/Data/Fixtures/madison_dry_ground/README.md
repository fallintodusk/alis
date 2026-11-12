# Madison dry-ground replay

Owns the captured Manhattan Ground reference and authored-correction inputs for
the [focused replay tests](../../../../../../tools/World/CanonicalCompilation/tests/test_madison_ground.py).
This is a component and consumer fixture, not a selected geographic model.

## Input provenance

- `terrain.json` retains the complete original cell values, halo, roles and
  immutable Water-fit samples from `canonical/terrain/cell_x1_y0.json` in the
  Manhattan bundle with SHA-256
  `84b0b6be76427baf31b0087d91e5f88c66cf9237a776acbf59cb92587e032067`.
- `water.json` retains all three Water features owned by that cell, including
  their complete mapped footprints, membership and point-fallback uncertainty.
- `profile.json` retains the compiler grid, target selection and surface policy.
  It supplies stage parameters; this fixture does not replay source ingestion or
  a fresh complete compilation. JSON schema references are relocated locally.
- `reference.csv` retains every provider-grid node over the nine incident quads.
  Its parent processed grid has SHA-256
  `4c53d780e17160a3f22c2b371c201e4c04db342c1135576a0b03001df9c8861c`;
  the NYC bare-earth raster has SHA-256
  `180f5e33895bbd0cd4099e28920c62e43cfed2e8c83b432de191e5bb41137f1b`.
  The retained processing used point-bilinear provider sampling in
  EPSG:32618 + EPSG:3855. These raster-model samples are not LAS observations,
  surveyed error bounds, present-day truth or a new source qualification.
- `overlay.json` owns the bounded authored-fixture adjustment. Its height delta
  is the median of the four captured Ground-minus-base residuals, rounded to the
  original height quantum. It is a source-guided game-world model, not a claim
  that every DSM departure is corruption. Provider attribution is the City of
  New York, AppGeo and Quantum Spatial; NOAA distributes the
  [2017 bare-earth DEM](https://noaa-nos-coastal-lidar-pds.s3.amazonaws.com/dem/NYC_topobathy_BE_DEM_2017_9307/index.html).

## Replay boundary

Use the [pinned Python environment](../../../../../../tools/World/ExecutionEnvironment/README.md)
with `tools/` on `PYTHONPATH`:

```powershell
python -B -m unittest World.CanonicalCompilation.tests.test_madison_ground
python -B -m World.CanonicalCompilation.tests.madison_ground_replay tmp/world/madison_ground/replay
```

The output directory must be fresh. No scratch file is an input. The replay uses
the existing surface finalizer on a two-by-two sample window with all three Water
features. Original metric coordinates and lattice spacing remain intact. The
window has no external halo because its output is only those four samples; it is
not a canonical cell or a proof that whole-cell replay succeeds. The typed model
retains every surrounding sample over all nine incident quads and the original
engine frame. Its original grid stays separate from the corrected faces so the
native stale-field control remains effective.

The tests require source-reference improvement, complete positive-area triangles, the exact edited samples and
incident quads, unchanged external context/weights/Water-fit data, deterministic
reconstruction, and Water/provenance refusal. Reference statistics sample the
retained provider stations; they do not certify a continuous surveyed field or
visual acceptance. The neighborhood maximum departure remains outside the local
authored controls. A negative observer control shows why the unchanged 49.3%
maximum is insufficient: six incident faces steepen, including 2.4% -> 18.4%
and 2.4% -> 15.1%. The worst crease including unchanged adjacent faces increases
15.3 -> 25.0 degrees. Source agreement does not establish better terrain shape.

Re-running the complete base finalizer separately refuses `water_conditioning_policy_required`
on an unrelated point-fallback Water body. That refusal must preserve the entire
input. The local window does not qualify that Water, remove it, supply missing
quality limits, publish a canonical candidate, or close regional acceptance.

The separate [authored dry Terrain operation](../../../../../../tools/World/CanonicalCompilation/terrain_refinement.md#authored-dry-terrain)
admits a height-only input through the existing authenticated child publisher
without re-fitting unchanged Water. It requires complete Water-disjoint incident
quads and preserves original cell/feature bytes. `test_authored_terrain.py` uses
this capture for mechanical artifact controls; a real profile authority and the
native canonical-loader test remain distinct checks. Retaining inherited Water
does not qualify that low-confidence feature for additional conditioning.

`before.json` and `after.json` contain the complete diagnostic geometry.
`after.json` can exercise the existing typed native consumer through its
authenticated external fixture environment variables described in the
[Mesh Terrain verification owner](../../../../ProjectWorldMeshTerrain/README.md).
That test proves saved StaticMesh CPU-field fidelity only; it does not prove
GPU appearance, collision, gameplay, Water realization or production admission.

```powershell
$env:PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE = (Resolve-Path 'tmp/world/madison_ground/replay/after.json').Path
$env:PROJECT_WORLD_COMPOSITE_TERRAIN_SHA256 = (Get-FileHash $env:PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE -Algorithm SHA256).Hash.ToLowerInvariant()
$env:PROJECT_WORLD_COMPOSITE_TERRAIN_OUTPUT = Join-Path (Get-Location) 'tmp/world/madison_ground/native'
scripts/ue/test/unit/run_cpp_tests_safe.ps1 -Mode Dev -TestFilter 'Project.World.Realization.MeshTerrain.CompositeConsumer' -LogDirectory tmp/world/madison_ground/native-log -TimeoutSeconds 300
```
