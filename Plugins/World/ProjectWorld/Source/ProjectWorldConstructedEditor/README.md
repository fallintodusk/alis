# Structural realization

Realizes source-bound canonical structural fields as persistent meshes and spatial actors.

| Need | Owner |
|---|---|
| Boundaries and internal parts | [Documentation](docs/README.md) |
| Registered producer interface | [Producer](Private/ProjectWorldConstructedLayerProducer.h) |
| Saved source-shaped proof | [Native fixture](Private/Tests/ProjectWorldConstructedSavedOutputTests.cpp) |
| Complete imported structural neighborhood | [Imported fixture](Private/Tests/ProjectWorldConstructedImportedNeighborhoodTests.cpp) |
| Saved output comparison | [Verify fixture](Private/Tests/ProjectWorldConstructedVerifyTests.cpp) |
| Stored role refusal controls | [Role quality](Private/Tests/ProjectWorldConstructedStoredRoleTests.cpp) |
| Local regeneration and retirement | [Lifecycle](Private/Tests/ProjectWorldConstructedLifecycleTests.cpp) |
| Source-only dependency preparation | [Dependency regression](Private/Tests/ProjectWorldConstructedDependencyTests.cpp) |

Prepare a fresh source-shaped canonical input with the
[pinned World Python](../../../../../tools/World/ExecutionEnvironment/README.md),
from the repository root:

```powershell
$env:PYTHONPATH = Join-Path (Get-Location) 'tools'
python -B -m World.CanonicalCompilation.tests.structural_import_fixture --output-root tmp/world/constructed-source/fresh-input
```

The output directory must be new. The command prints the receipt path and SHA256.
Resolve the receipt to an absolute path with `Resolve-Path`; the Editor's working
directory need not be the repository root. Pass it and the printed hash as
`-ProjectWorldConstructedImport=<absolute-receipt>` and
`-ProjectWorldConstructedImportHash=<sha256>` through `-ExtraArgs` to
[the cold runner](../../../../../scripts/ue/test/unit/run_cpp_tests_safe.ps1),
selecting exactly `Project.World.Realization.Constructed.RegisteredSourceOutput`.
That fixture accepts the complete canonical context around the two physical
membership cells. Empty cells remain dependency-visible for removal; they are
not additional physical units. It checks every saved source-face centroid against
complex and Pawn-channel collision, plus save/reload, refusal and no-op behavior.
It does not establish a combined Terrain/Water scene or player traversal.
The ordinary verify, dependency, role-quality and lifecycle fixtures need no
external input or old scratch state.
