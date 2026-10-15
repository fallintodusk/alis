# World Execution Environment

Owns the reproducible runtime shared by Source Ingestion and Canonical
Compilation. It contains no provider, grid, feature, or Unreal policy.

## Layout

| Path | Responsibility |
|---|---|
| `requirements.lock.txt` | Exact Python distributions, hashes, platform, and licences |
| `toolchain.lock.json` | Exact GDAL, PROJ, Osmium binaries, dependencies, hashes, and terms |
| `contracts/` | Toolchain lock and installation-receipt schemas |
| `app/` | Python-environment and native-tool implementation |
| `api.py` | Public runtime surface consumed by downstream World components |
| `tests/` | Runtime, lock, installation, and native-capability checks |
| `bootstrap.py` | Standard-library clean-machine entry point |

## Commands

```powershell
python -S tools/World/ExecutionEnvironment/bootstrap.py --check
python -S tools/World/ExecutionEnvironment/bootstrap.py
python -m unittest discover tools/World/ExecutionEnvironment/tests
```

The bootstrap validates Windows x86-64 and the pinned CPython version, then
accepts an ignored environment only after runtime verification and final
receipt creation. Failed or interrupted installations are not reusable.
Windows PowerShell coordinators use `resolve_python_host.ps1` so an unrelated
active virtual environment cannot replace the pinned bootstrap host.

Native tools are content-addressed and verified before use. External GDAL
plugins are disabled; the built-in GTiff and COG capabilities remain enabled.
The pinned GDAL application set includes the rasterizer used for exact
canonical-grid water membership; its executable hash is part of the lock.
Downloaded runtimes and receipts stay under ignored
`tmp/world/execution_environment/` storage and are not ALIS redistributable
payloads.

## Native geometry boundary

`api.partition_triangles` and `api.validate_triangle_cover` use the GEOS C API
already pinned in the toolchain lock. The adapter verifies the installation,
owns a reentrant context and releases geometries and its DLL search-directory
handle. It exposes polygon partition, constrained triangulation and geometric
coverage validation, without Water, terrain, feature or quality policy. Native
failure is explicit; there is no unconstrained triangulation fallback or second
dependency installation.

Native tools and capability probes use disconnected standard input. Editor
processes can inherit an invalid Windows stdin handle; forwarding it to a child
process can fail before any native operation. The exact detached-handle
regression is `tests/test_native_process.py`.
