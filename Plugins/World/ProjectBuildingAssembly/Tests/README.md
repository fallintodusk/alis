# Building assembly verification

Use the [pinned World Python environment](../../../../tools/World/ExecutionEnvironment/README.md)
for schema checks. A GCC/Clang-compatible C++20 host compiler must be on `PATH`;
`CXX` can name another compatible executable. The host compiler is a development
check, not runtime authority.

```powershell
python -B Plugins/World/ProjectBuildingAssembly/Tests/run.py
```

This compiles the production layout core and checks the captured rectangular
dimensions, deterministic reconstruction, all four part families, volume bounds,
story/slab contacts, a changed kit and six refusal cases. Compilation and execution
normally take seconds. No Unreal process or scratch input is required.

## Source-bound native case

The retained case uses Manhattan building `alis:osm:way:250440683`, from the
accepted Manhattan source bundle
`84b0b6be76427baf31b0087d91e5f88c66cf9237a776acbf59cb92587e032067`.
Its source is OpenStreetMap via Geofabrik's 2026-08-31 release. The canonical
effective volume and its source-derived height remain source data; the wall,
frame, floor and roof pieces are original procedural test geometry. Source-data
obligations remain governed by the [World asset policy](../../../../docs/legal/world_data_and_asset_policy.md).
The portable test retains the fitted dimensions and a conservative support span;
the native test independently derives full footprint support from the actual
canonical triangle field. Neither assumes the center query proves foundations.

Prepare a fresh case from the existing admitted authority, then use the safe
serialized native lane:

```powershell
python -B Plugins/World/ProjectBuildingAssembly/Tests/prepare_case.py tmp/world/building_assembly/proof
$env:ALIS_NO_PERSISTENT_EDITOR = '1'
$env:PROJECT_BUILDING_ASSEMBLY_CASE = (Resolve-Path 'tmp/world/building_assembly/proof/case.json').Path
$env:PROJECT_BUILDING_ASSEMBLY_CASE_SHA256 = (Get-FileHash $env:PROJECT_BUILDING_ASSEMBLY_CASE -Algorithm SHA256).Hash.ToLowerInvariant()
$settings = Join-Path (Get-Location) 'tmp/world/building_assembly/editor.ini'
scripts/ue/test/unit/run_cpp_tests_safe.ps1 -Mode Dev -Map '/Engine/Maps/Entry' -TestFilter 'Project.World.BuildingAssembly.CanonicalPrototype' -UseTestExitOnly -ExpectedTestNames 'Project.World.BuildingAssembly.CanonicalPrototype' -TimeoutSeconds 600 -LogDirectory tmp/world/building_assembly/log -ExtraArgs "-ProjectSkipFrontEnd -EditorPerProjectUserSettingsINI=$settings"
```

The test authenticates the complete canonical base and classifies all candidate
buildings in the selected cell. It checks unchanged unrelated emissions,
selected-only suppression, kit-local regeneration, actual no-op validation,
component/instance drift, unowned actor refusal, authored protection, Water width,
strict kit types, physical support, save/reload and actual rendering. Independent
compiled signed-volume and normal checks reject inward-facing module shells;
position fidelity and double-sided physics alone cannot prove outward rendering.

It creates only a disposable nonpartitioned scene. This does not qualify a
production layer, packaged building generation, city streaming, detailed facades,
interiors, or general Terrain grading. The [case contract](case.schema.json)
belongs to the [preparation command](prepare_case.py); the [host test runner](run.py)
owns its compile/test invocation.

`Project.World.BuildingAssembly.SavedRender` separately authenticates a retained
map and compares presentation materials without canonical or building regeneration.
Its descriptor supplies the exact map path, package name, SHA-256 and fresh scratch
output through `PROJECT_BUILDING_ASSEMBLY_VIEW_CASE` and
`PROJECT_BUILDING_ASSEMBLY_VIEW_SHA256`. It preserves the source map and verifies a
separate saved/reloaded preview copy. This is a presentation test, not another
geography admission. Camera/light component transforms are measured explicitly;
native root defaults can otherwise compose with the requested spawn transform.

## Registered World lifecycle

The [small neighborhood fixture](../../ProjectWorldTestData/Data/Fixtures/building_neighborhood/README.md)
owns the permanent source-shaped inputs. Prepare a fresh two-cell canonical and
confined test profile, then exercise the existing World wrapper:

```powershell
python -B Plugins/World/ProjectBuildingAssembly/Tests/prepare_lifecycle.py tmp/world/building_assembly/lifecycle_case
Plugins/World/ProjectBuildingAssembly/Tests/run_lifecycle.ps1 -CaseRoot tmp/world/building_assembly/lifecycle_case
```

`test_lifecycle_inputs.ps1` checks six runner input-admission paths without Unreal:
tracked/outside kit and selection destinations, aliased inputs and valid scratch
paths. It proves refusal before locking/native work and unchanged input bytes.

The [preparer](prepare_lifecycle.py) uses the existing source/canonical compiler;
selection and kit documents live below the reserved `tmp/world/inputs/` namespace.
The [lifecycle test](run_lifecycle.ps1) uses normal profile/producer/manifest
generation, one shared content lock and the existing outer snapshot restoration.
It starts with one building and verifies no-op, kit-only change, failed apply
after an actual actor save, three selected buildings sharing four meshes, cold
reconstruction and selection removal. Per-run receipts retain actual dirty units,
asset inventories and timings. This is TestData integration, not city admission.

After a cold-boundary failure, `-ResumeReconstruction` reuses accepted upstream
receipts and unchanged canonical/profile identities. After a removal failure,
`-ResumeRemovalSnapshot <case>/lifecycle/admitted-cold-state-<id>` restores only
the saved case outputs after checking their scope and current active authority.
Both paths retain outer restoration and replay the failing boundary through the
normal wrapper. They do not enroll a new authority or recompile the source.

`Project.World.BuildingAssembly.SelectionSettings` checks the optional producer
setting and refusal contract. `Project.World.BuildingAssembly.SelectionInputs`
loads a fresh prepared case from `PROJECT_BUILDING_SELECTION_CASE`, proving
selected building-height/support identity, complete batch suppression, authored
refusal and stale planned-input refusal even for an empty configured selection.
Run each exact test through the safe serialized lane with `-NoRHI` and
`-Map '/Engine/Maps/Entry'`. These tests create their own scenes; loading the
project's default city first adds unrelated asset compilation on a cold cache.
`Project.World.MeshTerrain.EvidenceDestinations` checks shared evidence roots,
canonical-input and scratch-input protection, including direct result writes.
