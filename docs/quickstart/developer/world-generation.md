# World Generation Quick Start

ALIS World work is a pipeline, not one editor action:

```text
provider data -> canonical compilation -> Unreal realization -> acceptance
```

Before changing behavior, use the
[ProjectWorld generation router](../../../Plugins/World/ProjectWorld/docs/territory_generation.md).
It routes each concern to the owner of authored inputs, generated outputs,
persistence, or regeneration.

## World tool prerequisite

Install a Windows x86-64 CPython host compatible with the version declared by
the [World execution environment](../../../tools/World/ExecutionEnvironment/README.md).
The bootstrap installs the locked packages, but not Python itself.

From the repository root, resolve and check that host:

```powershell
$WorldPython = & .\tools\World\ExecutionEnvironment\resolve_python_host.ps1
& $WorldPython -S tools/World/ExecutionEnvironment/bootstrap.py --check
```

## First generation proof

For a small engine-independent fixture, ingest the synthetic two-cell world
and dry-run its compilation:

```powershell
& $WorldPython -S tools/World/SourceIngestion/bootstrap.py run --profile Plugins/World/ProjectWorldTestData/Data/Profiles/SourceIngestion/synthetic_two_cell.source.json
& $WorldPython -S tools/World/CanonicalCompilation/bootstrap.py run --profile Plugins/World/ProjectWorldTestData/Data/Profiles/CanonicalCompilation/synthetic_two_cell.compile.json --dry-run
```

These commands keep disposable work under `tmp/world/`. Do not begin with a
production promotion or release gate.

After your first World edit, classify the minimum required proof without
building or mutating generated content:

```powershell
& $WorldPython -S tools/World/EndToEndValidation/bootstrap.py plan --base HEAD
```

## Continue with the owning stage

| Need | Owner |
|---|---|
| Acquire and verify provider data | [Source Ingestion](../../../tools/World/SourceIngestion/README.md) |
| Produce canonical world data | [Canonical Compilation](../../../tools/World/CanonicalCompilation/README.md) |
| Validate or apply canonical data in Unreal | [Canonical World Realization](../../../scripts/ue/world/README.md) |
| Choose proof depth | [World Pipeline Test Layers](../../testing/world_pipeline_layers.md) |
| Run cross-layer acceptance | [World End-to-End Validation](../../../tools/World/EndToEndValidation/README.md) |
| Navigate all World tools | [World Tools](../../../tools/World/README.md) |
