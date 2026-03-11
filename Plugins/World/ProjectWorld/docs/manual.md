# World Manual

Field guide for touching the world partition maps with minimal editor work once the automated systems are in place.

## Quick Start (≈5 minutes)

1. Launch **AlisEditor** with `-NoSound -NoLoadingScreen -game` flags (or run `make launch-editor`) to keep startup lean.
2. Open **`/Game/Project/Maps/KazanMain/KazanMain`**; confirm World Partition is enabled (World Settings → World Partition checkbox).
3. Use the **Data Layer Outliner** to load only the layers you need (e.g., `DL_City_Core`) and check the streaming cells that should be visible.
4. Bake **Navigation** and **Lighting** only if you touched relevant actors (Build → Build HLOD is automated, skip unless necessary).
5. Save the map; run `utility/test/selective_run_tests.sh --filter ProjectLoading` and `--filter ProjectWorld` (once available) before committing.

## Detailed Guide

### 1. Preparing the Editor

- Enable *Editor Preferences → Loading & Saving → Monitor Content Directories*. This keeps the JSON-based hot reload active for layouts.
- Disable *Live Coding* (Project Settings → Live Coding) during heavy world edits to avoid unnecessary recompiles.
- Use the **Command Line** `-ulog` to place logs under the project directory (`Saved/Logs`) for easier diffing.

### 2. Working with World Partition

- **Streaming Cells**: open the Cell Selection window (Window → World Partition). Toggle only the neighborhoods you are modifying to save memory.
- **HLOD / Foliage**: avoid rebuilding globally; instead, select the HLOD layer and press *Build Selected*. Designers should leave global rebuilds to the nightly CI job.
- **Data Layers**: when adding new layers, respect the naming convention `DL_{Region}_{Purpose}` and document them in `docs/systems/world_partition.md`.

### 3. Post-Edit Validation

Run the following console commands from the editor Output Log:

- `wp.Runtime.ToggleLoading` – ensures runtime streaming is still responsive.
- `stat levels` – verifies the expected cells are loaded.
- `wp.Runtime.DumpState` – outputs the current streaming state (check `Saved/Logs/WorldPartition.log`).

## Hot Reload & Editor Settings

- Ensure *Editor Preferences → General → Miscellaneous → Enable Editor Utility Hot Reload* is checked.
- Confirm `DefaultWorldPartitionSettings.ini` matches the server IP requirements (documented in `docs/systems/world_partition.md`).
- Keep the **Project Settings → Editor → Asset Editor Open Location** set to *Last Docked Tab* so world partition panels stay consistent between sessions.

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| Cells refuse to load in PIE | Data layer not loaded | In World Partition window, right-click data layer → *Load Selected*. |
| Lighting rebuild prompts after every save | Auto-build option enabled | Disable *Editor Preferences → Level Editor → Miscellaneous → Automatically Apply Build Data*. |
| Map fails in automation | World Partition config mismatch | Run `make test-unit-smart --base origin/main`; inspect `Saved/Automation/Reports/index.json` for `ProjectLoading World` failures. |
