# Canonical surface refinement

Owns post-canonical construction and qualified child selection over an
immutable authenticated base.

| Concern | Owner |
|---|---|
| Current architecture and semantic contract | [Documentation](docs/README.md) |
| Public operations | [API](api.py) |
| Authored dry height correction over accepted final grids | [Contract](../terrain_refinement.md#authored-dry-terrain) |
| Pure raster Ground construction | [Ground](ground.py) |
| Unpublished inner-to-provider model composition | [Ground composite](ground_composite.py) |
| Unpublished incoming-strip continuation into one east neighbor | [Ground continuation](ground_continuation.py) |
| Unpublished original-width path floor and masked source controls | [Path floor](ground_path_floor.py) |
| Unpublished joint inferred basin/reference-surface model | [Identified basin](identified_basin.py) |
| Closed joint basin input and candidate admission | [Basin admission](basin_admission.py) |
| Finite Ground sampling and shared-edge closure | [Resolution](ground_resolution.py) |
| Pure platform Road construction | [Constructed surface](constructed_surface.py) |
| Source-bound declared structural field import | [Constructed import](constructed_import.py) |
| Construction, publication and selection | [Implementation](app/README.md) |
| Owned request, result and selection schemas | [Contracts](contracts/README.md) |

## Focused contract tests

Use the [pinned execution environment](../../ExecutionEnvironment/README.md) Python,
with repository `tools/` on `PYTHONPATH`:

```powershell
$env:PYTHONPATH = Join-Path (Get-Location) 'tools'
python -B -m unittest World.CanonicalCompilation.tests.test_basin_admission World.CanonicalCompilation.tests.test_structural_import World.CanonicalCompilation.tests.test_surface_refinement
python -B -m unittest World.CanonicalCompilation.tests.test_authored_terrain World.CanonicalCompilation.tests.test_madison_ground
python -B -m unittest World.CanonicalCompilation.tests.test_pier25_connection
```

The [captured Pier25 case](../tests/pier25_connection_fixture.py) supplies a small
current-canonical physical field and an explicitly inferred deck. Its
[fast regression](../tests/test_pier25_connection.py) checks complete Ground
coverage, entire shore-contact edges and existing role/grade rules with negative
controls. It requires no provider download, full compilation or old scratch input;
source admission, combined Water contacts and native traversal remain separate.

The mixed-owner positive fixture has two selected basin cells and one unselected
regular neighbor containing the structural control. It checks complete cell
membership, retained terrain bytes, final Terrain/Water bindings, the full shared
edge, order-independent publication and frozen replay. Existing interaction and
later-operation failures must still refuse without publishing. These controls do
not establish dependent scenario composition. The mechanical
structure does not qualify real pier geography. Inputs are permanent; outputs
are freshly prepared under repository `tmp/`.
