# ProjectUI Troubleshooting

Scope: UI plugin-specific issues (layouts, themes, MVVM bindings). Integration issues remain in `docs/agents/troubleshooting.md`.

## Menu not appearing or blank
- Verify layout JSON exists under `Config/UI/*.json` and paths match the widget `ConfigFilePath`.
- Check logs for `UProjectWidgetLayoutLoader` errors; invalid JSON or missing file will skip build.
- Ensure theme asset loads: `UProjectUIThemeManager` should resolve an active theme.

## Buttons or bindings not working
- Confirm ViewModel is created and bound before widget usage (`SetViewModel` in `UProjectUserWidget`).
- Check that button actions in JSON map to handler functions (avoid per-name hardcoding).
- Use `VIEWMODEL_PROPERTY` macros so property changes notify bound widgets.

## Theme not applying
- Verify theme keys used in JSON (e.g., `PrimaryColor`, `HeadingLargeFont`) exist in the active theme asset.
- Ensure widgets listen for theme changes; `UProjectUserWidget` registers automatically in `NativeConstruct`.

## Hot reload not updating layouts
- Run the registered console command (e.g., `ReloadLayout` or screen-specific command) while in PIE.
- If still stale, close PIE, reload map, and check for JSON parse errors in the log.

## Debug Tools

### Widget Tree Dump (Console Command)

Inspect the full widget hierarchy at runtime:

```
UI.Debug.DumpTree                                    # Log only
UI.Debug.DumpTree out=Dumps/layout.txt              # Save to Saved/Dumps/layout.txt
UI.Debug.DumpTree out=Dumps/inv.json format=json    # JSON format
UI.Debug.DumpTree filter=Inventory                  # Filter by widget name (dump subtree)
```

Output includes:
- Determinism inputs (viewport size, DPI scale, SafeZone, aspect ratio)
- Widget tree with: Name, Class, Desired/Arranged size, Slot info, Flags
- Flags: `ZERO!` (zero size), `HIDDEN`, `NOVM` (no ViewModel), `NOHIT` (not interactive), `CLIP`

Log markers for automation parsing:
```
===BEGIN_WIDGET_TREE_DUMP===
... content ...
===END_WIDGET_TREE_DUMP===
```

### CLI Usage (Headless/CI)

```bat
UnrealEditor-Cmd.exe Alis.uproject ^
  -unattended -nopause -NullRHI ^
  -stdout -FullStdOutLogOutput ^
  -ExecCmds="UI.Debug.DumpTree out=Dumps/layout.json format=json; Quit"
```

### Automation Test (Inventory Dump)

```bat
for /f "tokens=1,* delims==" %%A in ('findstr /b InventoryDumpMap Plugins\\Test\\ProjectIntegrationTests\\Config\\DefaultProjectIntegrationTests.ini') do set INV_MAP=%%B

UnrealEditor-Cmd.exe Alis.uproject %INV_MAP% ^
  -unattended -nopause -NullRHI ^
  -stdout -FullStdOutLogOutput ^
  -ExecCmds="Automation RunTests ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree; Quit" ^
  -TestExit="Automation Test Queue Empty" ^
  -ABSLOG="Saved/Logs/RunTests.log"
```

The test lives in ProjectIntegrationTests and writes `Saved/Dumps/Inventory.json`.
It builds inventory layout using synthetic ViewModel data, so no gameplay mode is required.
Map path is read from `ProjectIntegrationTests.InventoryDumpMap` in the integration tests config.
ProjectIntegrationTests is enabled in `Alis.uproject`.

Agent flow:
1) Run the command above
2) Read `Saved/Dumps/Inventory.json`
3) Parse and assert invariants (zero size, hit-testable but hidden, unexpected clipping)

### Other Debug Commands

- `UI.Debug.Verbosity [0-5]` - Set debug verbosity (0=Silent, 5=VeryVerbose)
- Widget Reflector: `Ctrl+Shift+W` - Interactive widget inspection

### Programmatic Access

```cpp
// C++ - dump to file
UProjectUIDebugSubsystem* DebugSub = GI->GetSubsystem<UProjectUIDebugSubsystem>();
DebugSub->DumpWidgetTreeEx(TEXT("ProjectUI/Dumps/layout.json"), TEXT("json"), TEXT(""));

// C++ - diagnose single widget
FProjectWidgetDiagnostics Diag = DebugSub->DiagnoseWidget(MyWidget);
UE_LOG(LogTemp, Display, TEXT("%s"), *Diag.ToString());
```

## References
- Layout system: `ui_layout.md`
- MVVM: `ui_mvvm.md`
- Theme system: `ui_theme.md`
