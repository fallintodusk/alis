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
