# World Partition Architecture

Comprehensive guide for World Partition setup, data-driven manifests, and in-editor workflows.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [World Manifest Design](#world-manifest-design)
3. [Generation Strategy](#generation-strategy)
4. [Loading Flow](#loading-flow)
5. [Editor Workflow](#editor-workflow)
6. [Validation & Testing](#validation--testing)
7. [Troubleshooting](#troubleshooting)
8. [Outstanding Work](#outstanding-work)

---

## Architecture Overview

**Goals:**
- Single World Partition root map that streams regions on demand
- Data-driven manifests (ProjectData plugin) describe worlds, experiences, and content packs
- Automation-friendly: manifests can be generated/validated without hand-editing `.umap` files

**Key Components:**
- **World Partition Map**: Editor-authored root map with streaming cells
- **World Manifests** (`UProjectWorldManifest`): DataAsset describing regions and metadata
- **Region Descriptors** (`FProjectWorldRegionDescriptor`): Individual streaming regions
- **Loading Subsystem** (`ProjectLoadingSubsystem`): Orchestrates world streaming

---

## World Manifest Design

World manifests (`UProjectWorldManifest`) carry:

- **WorldRoot**: Soft reference to the World Partition map
- **Regions**: Array of `FProjectWorldRegionDescriptor` entries
  - Region ID (string)
  - Display name (localized)
  - Bounds (world space coordinates)
  - Partition grid reference
  - Optional asset references
- **FeatureDependencies**: Cross-plugin features required for the world to load
- **PartitionMetadata**: Future hook for auto-generated JSON/DataAsset with cell-to-region mapping

**Experiences** (`UProjectExperienceManifest`) point at gameplay mode/pawn overrides and reference required systems.

**Content packs** (`UProjectContentPackManifest`) describe optional IOStore/PAK payloads.

---

## Generation Strategy

### Automated (Future)

1. Author the World Partition map in editor (keep map minimal—streamable cells supply real content)
2. Run forthcoming automation (tracked in `todo/create_world.md`) to parse World Partition metadata and emit region descriptors
3. Commit the generated manifest assets (DataAsset instances under `/Game/Project/Data/Worlds/`)
4. Use validation helpers (`ProjectManifestValidator`) in CI to gate malformed manifests

### Manual (Current)

Until the automation is written, manifests can be created manually via the `ProjectData` Blueprint utilities:

```cpp
LogWorldManifestSummary(Manifest)
MakeRegionDescriptor("Kazan_Outskirts", DisplayName, Bounds, EProjectWorldLayerType::Landscape)
```

---

## Loading Flow

1. **Boot**: `ProjectBoot` loads the minimal map and spawns UI
2. **Selection**: UI surfaces `ProjectWorldManifest` entries
3. **Load Request**: `FProjectLoadRequest` references `ProjectWorld` + `ProjectExperience` ids
4. **Execution**: `ProjectLoadingSubsystem` resolves manifests and streams layers/feature packs

**See Also:**
- [Loading Pipeline](loading_pipeline.md) - Detailed phase execution
- [docs/agents/build.md](../agents/build.md) - Build commands for testing

---

## Editor Workflow

### Quick Start (≈5 minutes)

1. **Launch Editor** with `-NoSound -NoLoadingScreen -game` flags (or run `make launch-editor`)
2. **Open Map**: `/Game/Project/Maps/KazanMain/KazanMain`
3. **Verify World Partition**: World Settings → World Partition checkbox enabled
4. **Load Data Layers**: Use Data Layer Outliner to load only needed layers (e.g., `DL_City_Core`)
5. **Check Streaming Cells**: Verify correct cells are visible
6. **Bake (if needed)**: Build → Navigation/Lighting only if you touched relevant actors (skip HLOD unless necessary)
7. **Save & Test**: Run `utility/test/selective_run_tests.sh --filter ProjectLoading`

### Preparing the Editor

**Editor Preferences:**
- Enable *Loading & Saving → Monitor Content Directories* (for JSON hot reload)
- Disable *Live Coding* during heavy world edits (avoid unnecessary recompiles)
- Enable *General → Miscellaneous → Enable Editor Utility Hot Reload*

**Project Settings:**
- Set *Editor → Asset Editor Open Location* to *Last Docked Tab* (keeps World Partition panels consistent)

**Command Line:**
- Use `-ulog` to place logs under `Saved/Logs/` for easier diffing

### Working with World Partition

**Streaming Cells:**
- Open Cell Selection window: **Window → World Partition**
- Toggle only the neighborhoods you're modifying (saves memory)
- Use `stat levels` console command to verify loaded cells

**HLOD / Foliage:**
- **Avoid global rebuilds** - select HLOD layer and press *Build Selected*
- Leave global rebuilds to nightly CI job

**Data Layers:**
- **Naming Convention**: `DL_{Region}_{Purpose}` (e.g., `DL_City_Core`, `DL_Kazan_Landscape`)
- Document new layers in this file
- Load/unload via Data Layer Outliner (right-click → Load Selected)

### Post-Edit Validation

Run these console commands from the editor Output Log:

```
wp.Runtime.ToggleLoading   // Ensures runtime streaming is responsive
stat levels                // Verifies expected cells are loaded
wp.Runtime.DumpState       // Outputs streaming state to Saved/Logs/WorldPartition.log
```

---

## Validation & Testing

### Automated Tests

```bash
# Run loading subsystem tests
utility/test/selective_run_tests.sh --filter ProjectLoading

# Run world-specific tests (once available)
utility/test/selective_run_tests.sh --filter ProjectWorld
```

### Manual Checks

**Before committing map changes:**
1. ✅ World Partition enabled in World Settings
2. ✅ Data layers named correctly (`DL_{Region}_{Purpose}`)
3. ✅ Streaming cells load correctly in PIE
4. ✅ Console validation commands pass (`wp.Runtime.DumpState` shows no errors)
5. ✅ Automated tests pass (ProjectLoading filter)

**Asset Manager Integration:**
```bash
# Verify world manifests are discoverable
# Check Saved/Logs/LogAssetManager for WorldManifest primary asset type
```

---

## Troubleshooting

### Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| "No Loaded Region(s)" after UE 5.7.2 migration | Editor streaming settings reset | *Editor Preferences → World Partition*: uncheck "Enable Loading in Editor"; *Project Settings → Level Instance*: check "Enable Streaming"; restart editor |
| Cells refuse to load in PIE | Data layer not loaded | World Partition window → right-click data layer → *Load Selected* |
| Lighting rebuild prompts after every save | Auto-build option enabled | Disable *Editor Preferences → Level Editor → Miscellaneous → Automatically Apply Build Data* |
| Map fails in automation | World Partition config mismatch | Run `make test-unit-smart --base origin/main`; inspect `Saved/Automation/Reports/index.json` for `ProjectLoading World` failures |
| World manifest not found in Asset Manager | Asset not scanned or wrong directory | Check manifest is under `/Game/Project/Data/Worlds/` and Primary Asset Type = `WorldManifest` |
| Region streaming hangs | Circular dependency in FeatureDependencies | Review manifest FeatureDependencies, ensure no circular references |

### Debug Commands

```
# World Partition state dump
wp.Runtime.DumpState

# Show loaded levels
stat levels

# Toggle runtime loading
wp.Runtime.ToggleLoading

# Asset Manager diagnostics
AssetManager.DumpAssetRegistry WorldManifest
```

### Log Files

- **World Partition**: `Saved/Logs/WorldPartition.log`
- **Asset Manager**: `Saved/Logs/LogAssetManager.log`
- **Loading Subsystem**: `Saved/Logs/LogProjectLoading.log`

---

## Outstanding Work

**Automation:**
- Implement manifest auto-generation using World Partition runtime descriptors
- Wire telemetry storage for region load times
- Define analytics schema for session/world/region transitions

**Testing:**
- Create `ProjectWorld` test module for World Partition validation
- Add automated tests for region streaming
- CI integration for map validation

**Documentation:**
- Document all Data Layer conventions
- Create region descriptor templates
- Add World Partition best practices guide

**Tracked in:** `todo/create_world.md`

---

## See Also

- [Loading Pipeline](loading_pipeline.md) - Phase execution and subsystem integration
- [Asset Manager Configuration](../guides/asset_manager.md) - Primary asset type setup
- [Experience Manual](../manual/experience_manual.md) - Player experience configuration
- [Architecture Principles](../architecture/core_principles.md) - Data-driven design patterns
