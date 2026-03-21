# Agent UE Inspection Guide

**Main router for autonomous UE runtime inspection**

---

## Table of Contents

| Section | What It Covers |
|---------|---------------|
| [Quick Start](#quick-start) | Simplest paths for common tasks |
| [Automated Analysis](#automated-analysis) | Agent-friendly JSON parsing tools |
| [UI Inspection Example](#ui-inspection-example) | Widget tree dump workflow |
| [Flow Patterns](#flow-patterns) | Layout, Subsystem, Event patterns |
| [JSON Schema](#json-schema) | Widget tree output format |
| [Troubleshooting](#troubleshooting) | Common issues and fixes |
| [File Locations](#file-locations) | Paths to scripts, configs, outputs |
| [UE Built-in Debug Tools](#ue-built-in-debug-tools) | Widget Reflector, Slate Insights, Automation Driver |

**Related:**
- [AGENTS.md](../../AGENTS.md) - Project commands and PowerShell patterns
- [UI Guide](../ui/guide.md) - UI architecture
- [Integration Tests](integration_tests.md) - Cross-plugin testing

---

## Quick Start

### Agent Access (Recommended)

**Read existing dump files - no UE execution needed:**
```
Saved/Dumps/*.json
```

Dumps persist between sessions. If stale, ask user to regenerate in PIE.

### Manual UI Verification

1. Run PIE (Play in Editor)
2. Open UI (e.g., press `I` for inventory)
3. Console: `UI.Debug.DumpTree out=Dumps/Inventory.json format=json`
4. Read `Saved/Dumps/Inventory.json`

### CI/Headless (Automation)

> See [AGENTS.md](../../AGENTS.md#windows-powershell-commands-primary-environment) for PowerShell patterns.

```powershell
.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "..." -Game -Map "/Path/To/Map"
```

**Inventory Layout Test (single resolution):**
```powershell
.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
```
Output: `Saved/Dumps/Inventory.json`

**Inventory + nearby loot layout test:**
```powershell
.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryNearbyLoot.DumpTree" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
```
Output: `Saved/Dumps/InventoryNearbyLoot.json`

**Inventory ordering contract test (hands + pockets + backpack):**
```powershell
.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryHands.PocketOrdering" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
```
Validates the current descriptor-driven layout contract defined in `Plugins/Features/ProjectInventory/docs/design_vision.md`.

**UI framework regression gate (required for framework refactors):**
```powershell
.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
```
Includes:
- Grid hit detector edge and bounds checks
- Grid drag/drop footprint validation
- Grid visual state color mapping
- Widget binder required-widget validation
- Menu settings presenter reuse checks
- Inventory pocket/backpack ordering contract checks

**Multi-Resolution Test (Tier 1 - catches 80% of layout bugs):**
```powershell
.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryHands.MultiResolution" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
```
Outputs (4 files):
- `Saved/Dumps/Inventory_720p.json` (1280x720)
- `Saved/Dumps/Inventory_1080p.json` (1920x1080)
- `Saved/Dumps/Inventory_1440p.json` (2560x1440)
- `Saved/Dumps/Inventory_Ultrawide.json` (3440x1440)

> **Important:** The `-Map` parameter is required for world-dependent tests.

**Script Parameters:**

| Parameter | Description | Default |
|-----------|-------------|---------|
| `-TestFilter` | Test name pattern | `"Project.*"` |
| `-Map` | Map to load (required for world-dependent tests) | none |
| `-Game` | Run in standalone game mode (creates real world) | false |
| `-NoRHI` | Headless mode, no GPU (unit tests only) | false |
| `-TimeoutSeconds` | Max execution time | 120 |

**Timing Expectations:**

| Test Type | Expected | Warn Threshold |
|-----------|----------|----------------|
| Unit tests (NullRHI) | 5-15 sec | 30 sec |
| Layout inspection | 20-30 sec | 60 sec |
| Full integration | 30-60 sec | 90 sec |

---

## Automated Analysis

**Agent-friendly tools for parsing JSON dumps without manual inspection.**

### Layout Report Tool

Parse widget tree JSON and check invariants automatically:

```powershell
python tools/agentic/ui/layout_report.py Saved/Dumps/Inventory.json
python tools/agentic/ui/layout_report.py --format=json Saved/Dumps/Inventory.json
python tools/agentic/ui/layout_report.py --severity=high Saved/Dumps/Inventory.json
```

**Parameters:**

| Parameter | Description | Default |
|-----------|-------------|---------|
| `file` | Path to JSON dump | `Saved/Dumps/Inventory.json` |
| `--format` | Output format: `text` or `json` | `text` |
| `--severity` | Filter: `all`, `high`, `medium`, `low` | `all` |
| `--max-issues` | Max issues shown (text mode) | `40` |

**Output:**
```
Viewport: 1920x1080 dpi=1.0
Widgets: 47  Issues: 2
- ZERO_ARRANGED_SIZE: InventoryGridCell_16 (W_InventoryGridCell) slot=Canvas  Offset=(102,152)
- HIT_TEST_INVISIBLE: ButtonOverlay (SButton) slot=Overlay  Padding=(0,0,0,0)
```

**Exit codes:** `0` = no issues, `1` = issues found, `2` = file/parse error

**Python:** Uses system Python (`python`, `python3`, `py`). Falls back to UE bundled Python if not in PATH.

### Full Workflow Script

Generate dump + analyze in one command:

```powershell
.\scripts\ue\test\ui\check_inventory_layout.ps1
.\scripts\ue\test\ui\check_inventory_layout.ps1 -Scenario NearbyLoot
.\scripts\ue\test\ui\check_inventory_layout.ps1 -SkipDump              # Analyze existing dump only
.\scripts\ue\test\ui\check_inventory_layout.ps1 -DumpPath "path.json"  # Custom dump path
```

**Parameters:**

| Parameter | Description | Default |
|-----------|-------------|---------|
| `-Scenario` | `Hands`, `NearbyLoot`, or `Naked` dump preset | `Hands` |
| `-SkipDump` | Skip UE test, analyze existing dump | `$false` |
| `-DumpPath` | Custom JSON dump path | `Saved/Dumps/Inventory.json` |
| `-TimeoutSeconds` | UE test timeout | `90` |

**Exit codes:** `0` = pass, `1` = issues found, `2` = file/config error, `124` = timeout

This script:
1. Runs `DumpTree` test in standalone game mode (unless `-SkipDump`)
2. Parses `Saved/Dumps/Inventory.json`
3. Reports issues with exit code

### Multi-Resolution Analysis

After running the `MultiResolution` test, analyze all dumps to find resolution-specific issues:

```powershell
# Analyze all resolutions
python tools/agentic/ui/layout_report.py Saved/Dumps/Inventory_720p.json
python tools/agentic/ui/layout_report.py Saved/Dumps/Inventory_1080p.json
python tools/agentic/ui/layout_report.py Saved/Dumps/Inventory_1440p.json
python tools/agentic/ui/layout_report.py Saved/Dumps/Inventory_Ultrawide.json
```

**Agent workflow for resolution comparison:**
1. Run `MultiResolution` test to generate all dumps
2. Analyze each dump with `layout_report.py`
3. Compare issue counts across resolutions
4. Issues appearing only at specific resolutions indicate responsive layout problems

**Common resolution-specific issues:**
| Issue | Likely Cause |
|-------|-------------|
| Issues only at 720p | Minimum size constraints violated |
| Issues only at Ultrawide | Aspect ratio handling missing |
| Issues at all resolutions | Fundamental layout bug |

### Invariant Checks

The report tool validates these invariants (from [Common Issues](#common-issues)):

| Check | Condition | Severity |
|-------|-----------|----------|
| `ZERO_ARRANGED_SIZE` | `visible=true`, `arrangedSize=0`, `desiredSize>0` | High |
| `HIT_TEST_INVISIBLE` | `hitTestable=true`, `visible=false` | High |
| `EMPTY_CONTAINER` | Named grid host has 0 children when visible | High |
| `NO_VIEWMODEL` | ProjectUserWidget missing ViewModel binding | Medium |
| `SAME_POSITION` | Multiple siblings at identical Canvas offset AND anchor | Medium |

**EMPTY_CONTAINER**: Checks that known grid hosts (`GridHostPrimary`, `GridHostSecondary`, `NearbyGridHost`, `LeftHandGridHost`, `RightHandGridHost`) have children when visible. Empty grid hosts indicate the ViewModel didn't properly populate grid dimensions, or `RebuildGrids()` wasn't called.

## Framework Consolidation Checks (Critical)

Use these checks when reviewing any UI refactor:

1. Framework ownership check
```powershell
rg -n "FInventoryPanelDragDrop|FInventoryGridHitDetector|FInventoryGridVisualState|UInventoryGridCell" Plugins/UI
```
Expected: no new usage outside historical docs.

2. Presenter/view-model binding check
- In dump reports, presenter-owned widgets must not show `NO_VIEWMODEL` when they depend on ViewModel data.
- If `NO_VIEWMODEL` appears in tooltip/context/menu trees, verify presenter-created widget `SetViewModel(...)` calls.

3. Menu/settings reuse check
```powershell
.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework.MainMenu.SettingsPopupPresenterReuse" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
```
Expected: settings root created once and reused across navigation.

4. Action policy SOT check
- Action buttons and context menu must consume the same descriptor policy output.
- Avoid widget-local action visibility logic duplication.

5. Inventory layout contract check
- Inventory container rendering must follow the contract in `Plugins/Features/ProjectInventory/docs/design_vision.md`.
- Keep this doc as a routing note only; do not duplicate the contract here.

### Integration with CI

Add to overnight run or pre-commit:

```powershell
# In scripts/autonomous/claude/overnight/main.ps1
.\scripts\ue\test\ui\check_inventory_layout.ps1
if ($LASTEXITCODE -ne 0) { throw "UI layout issues detected" }
```

---

## UI Inspection Example

**Goal:** Diagnose layout issues in inventory panel.

### Step 1: Get Dump

**Option A - Existing file:**
```
Read: Saved/Dumps/Inventory.json
```

**Option B - Fresh dump (user runs in PIE):**
```
UI.Debug.DumpTree out=Dumps/Inventory.json format=json filter=Inventory
```

### Step 2: Parse Key Fields

```json
{
  "name": "InventoryGridCell_16",
  "class": "W_InventoryGridCell",
  "width": 56, "height": 56,
  "desiredSize": { "width": 9, "height": 28 },
  "visible": true,
  "slotType": "Canvas",
  "position": { "x": 102, "y": 152 },
  "issues": []
}
```

**Key fields for diagnosis:**
- `position` - Where widget is placed (Canvas slots)
- `width/height` vs `desiredSize` - Layout vs content mismatch
- `visible` - Should be `Collapsed` if empty
- `issues` - Pre-detected problems

### Step 3: Common Issues

| Symptom | Diagnosis | Fix |
|---------|-----------|-----|
| Multiple widgets same position | Canvas slots not offset | Fix slot calculation |
| `desiredSize.width = 1` | Empty text field visible | Set Collapsed when empty |
| `visible: true` but no content | Container always shown | Collapse when no data |
| `arrangedSize = 0` but `desiredSize > 0` | Layout not computed | Call `ForceLayoutPrepass()` |

**Issue Types (in `issues` array):**

| Issue | Meaning | Severity |
|-------|---------|----------|
| `ZERO_SIZE` | Visible widget with 0x0 arranged size | High |
| `HIT_TEST_INVISIBLE` | Can receive input but not visible | High |
| `NO_VIEWMODEL` | ProjectUserWidget missing ViewModel | Medium |
| `UNEXPECTED_CLIP` | Container clipping children unexpectedly | Medium |
| `Z_ORDER_CONFLICT` | Overlapping Z-orders in same parent | Low |

### Step 4: Locate Code

Widget naming convention: `W_<WidgetClass>_<Instance>`

Search pattern:
```
Plugins/**/Widgets/W_<WidgetClass>.h
Plugins/**/Widgets/W_<WidgetClass>.cpp
```

---

## Flow Patterns

### Pattern 1: Layout Validation

```
Agent reads JSON -> Find widgets by name -> Check positions/sizes -> Report issues
```

**No UE execution required** - just parse existing dump.

### Pattern 2: Subsystem State

```
Test queries subsystem -> Logs/writes JSON -> Agent parses markers
```

**Log markers:**
```
===BEGIN_WIDGET_TREE_DUMP===
... content ...
===END_WIDGET_TREE_DUMP===
```

### Pattern 3: Event Verification

```
Test triggers action -> Records events -> Assert sequence -> Exit code = pass/fail
```

---

## Test Setup Pitfalls

### VIEWMODEL_PROPERTY Reflection Limitation

**Problem:** `VIEWMODEL_PROPERTY(int32, GridWidth)` generates:
- `protected int32 GridWidth` (UPROPERTY)
- `protected void UpdateGridWidth(int32)` - sets value AND notifies
- `public int32 GetGridWidth()` - getter only

**Why reflection fails:** Even if reflection CAN find and set the backing UPROPERTY field, it **bypasses UpdateGridWidth / NotifyPropertyChanged**, so UI bindings won't react. The value changes but nothing rebuilds.

**Wrong approach (value set, UI not updated):**
```cpp
// Reflection may set the field, but does NOT call UpdateGridWidth - UI won't rebuild
SetIntProperty(ViewModel, TEXT("GridWidth"), 6);
```

**Correct approach - add explicit setters to ViewModel:**
```cpp
// In InventoryViewModel.h - add public setter that calls protected Update:
void SetGridDimensions(int32 InWidth, int32 InHeight)
{
    UpdateGridWidth(InWidth);
    UpdateGridHeight(InHeight);
}

// In test:
ViewModel->SetGridDimensions(6, 6);
```

**Debug tip:** If test grids are empty but widget tree exists, check:
1. ViewModel dimensions are set (log `GetGridWidth()` value)
2. GridHost is found (log `GridHost=valid/NULL`)
3. BuildGrid returns valid panel (log `GridPanel=valid/NULL`)

See [ProjectUIInventoryDumpTreeTest.cpp](../../Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/ProjectUIInventoryDumpTreeTest.cpp) for working example.

---

## JSON Schema

### Root Structure

```json
{
  "determinism": {
    "viewportWidth": 1920,
    "viewportHeight": 1080,
    "dpiScale": 1.0
  },
  "widgets": [...]
}
```

### Widget Fields

| Field | Description |
|-------|-------------|
| `name` | Instance name (e.g., `W_InventoryPanel_0`) |
| `class` | Widget class |
| `width/height` | Arranged size |
| `desiredSize` | Content size |
| `visible` | Visibility state |
| `slotType` | Canvas, Overlay, HorizontalBox, etc. |
| `position` | Canvas slot offset |
| `anchors` | `minX/Y`, `maxX/Y` (0,0-1,1 = stretched) |
| `issues` | Pre-detected problems |
| `children` | Nested widgets |

### Slot Types

| Type | Key Fields |
|------|------------|
| Canvas | `position`, `slotSize`, `autoSize`, `anchors`, `zOrder` |
| Overlay | `padding`, `alignment` |
| HorizontalBox | `padding`, `sizeRule` (Auto/Fill) |
| VerticalBox | `padding`, `sizeRule` |

---

## Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| No tests found | Module is `Editor` not `DeveloperTool` | Change `.uplugin` type |
| No game world | Missing `-Game` or `-Map` flag | Add flags to script |
| All sizes zero | Layout not computed | `ForceLayoutPrepass()` |
| Sizes differ from PIE | Viewport size difference | Compare `desiredSize` not `arrangedSize` |

**Viewport note:** Automated tests run at 1920x1080. PIE window is smaller. Use `desiredSize` for comparisons.

---

## File Locations

| Item | Path |
|------|------|
| JSON dumps (single) | `Saved/Dumps/Inventory.json`, `Saved/Dumps/InventoryNearbyLoot.json`, `Saved/Dumps/InventoryNaked.json` |
| JSON dumps (multi-res) | `Saved/Dumps/Inventory_{720p,1080p,1440p,Ultrawide}.json` |
| **Agentic Tools** | |
| Layout report | `tools/agentic/ui/layout_report.py` |
| UI check wrapper | `scripts/ue/test/ui/check_inventory_layout.ps1` |
| **Test Infrastructure** | |
| Test script | `scripts/ue/test/unit/run_cpp_tests_safe.ps1` |
| Test config | `Plugins/Test/ProjectIntegrationTests/Config/DefaultProjectIntegrationTests.ini` |
| Test source | `Plugins/Test/ProjectIntegrationTests/Source/.../Integration/` |
| UE path config | `scripts/config/ue_path.conf` |
| UE logs | `Saved/Logs/Alis.log` |
| Test logs | `Saved/Logs/RunTests.log` |
| CI artifacts | `scripts/ue/artifacts/overnight/` (overnight runs only) |

---

## Console Commands

```
UI.Debug.DumpTree                                    # Log only
UI.Debug.DumpTree out=Dumps/all.txt                 # File (txt)
UI.Debug.DumpTree out=Dumps/inv.json format=json    # File (json)
UI.Debug.DumpTree filter=Inventory format=json      # Filter + json
```

---

## UE Built-in Debug Tools

**For human visual inspection (not agent-driven):**

### Widget Reflector
- **Open:** `Ctrl+Shift+W` or console `WidgetReflector`
- **Features:** Pick widget, see hierarchy, visibility, hit-test flags, desired vs arranged size, clipping
- **Use for:** Quick visual debugging, finding widget paths
- **Ref:** [UE Widget Reflector](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-slate-widget-reflector-in-unreal-engine)

### Slate Insights
- **Purpose:** Records UI update events, helps find invalidation root causes
- **Use for:** Performance debugging, finding excessive rebuilds
- **Ref:** [UE Slate Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/slate-insights-in-unreal-engine)

### Console Slate Debugger
```
SlateDebugger.Start                    // Start event logging
SlateDebugger.Stop                     // Stop
SlateDebugger.Event.LogInputEvent 1    // Log input routing
SlateDebugger.Event.LogFocusEvent 1    // Log focus changes
```
- **Ref:** [UE Console Slate Debugger](https://dev.epicgames.com/documentation/en-us/unreal-engine/console-slate-debugger-in-unreal-engine)

### Automation Driver (Input Simulation)
- **Purpose:** Programmatic UI input (click, drag, type)
- **API:** `IAutomationDriver`, `By::Id()`, `FindElement()`, `Click()`, `DragTo()`
- **Note:** Requires `automationId` in widget metadata for stable locators
- **Ref:** [UE Automation Driver](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-driver-in-unreal-engine)

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | All tests passed |
| 1 | Test failures |
| 2 | No tests found |
| 124 | Timeout |
| 255 | UE crash |

---

## Adding New Inspections

1. Create test in `ProjectIntegrationTests/Source/.../Integration/`
2. Use `ClientContext` flag
3. Output JSON to `Saved/Dumps/`
4. Document invariants in this guide
5. Target <60 seconds execution

**Minimal C++ Template:**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMyLayoutTest,
    "ProjectIntegrationTests.UI.Layout.MyWidget.DumpTree",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FMyLayoutTest::RunTest(const FString& Parameters)
{
    UWorld* World = AutomationCommon::GetAnyGameWorld();
    if (!TestNotNull(TEXT("World"), World)) return false;

    UGameInstance* GI = World->GetGameInstance();
    UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();

    UMyWidget* Widget = CreateWidget<UMyWidget>(GI);
    Widget->SetViewModel(CreateSyntheticViewModel());
    Widget->AddToViewport();
    Widget->ForceLayoutPrepass();

    return TestTrue(TEXT("Dump OK"),
        DebugSub->DumpWidgetTreeEx(TEXT("Dumps/MyWidget.json"), TEXT("json"), TEXT("MyWidget")));
}
```

**Requirements:** `ClientContext` flag, `DeveloperTool` module type, `-Game` + `-Map` flags

---

*Copyright ALIS. All Rights Reserved.*
