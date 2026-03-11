# Hot Reload Architecture

**Purpose:** Document hot reload strategies for rapid iteration without editor restarts.

---

## Overview

The project supports **three-tier hot reload architecture**:

| Tier | Asset Type | Hot Reload | Iteration Time | Use Case |
|------|-----------|-----------|----------------|----------|
| **Tier 1** | JSON Layouts | [OK] ReloadLayout() | < 1 second | Layout changes, positioning, styling |
| **Tier 2** | ViewModel Properties | [OK] Property changes | < 1 second | Data binding, game state |
| **Tier 3** | Config Files (.ini) | [!] Editor restart | 10-30 seconds | Experience definitions, settings |

**Blueprint Assets (WBP_) and C++ (W_) are NOT hot reload targets** - they require editor restart or recompile.

---

## Architectural Decision: W_ Only (No WBP_ Wrappers)

**Decision Date:** 2025-10-21
**Status:** [OK] APPROVED

### Decision

**Use W_ C++ widgets directly. Delete all WBP_ Blueprint wrappers.**

### Rationale

| Criterion | W_ Only (Chosen) | W_ + WBP_ Hybrid |
|-----------|------------------|------------------|
| **Agentic-Friendly** | [OK] JSON = structured data AI can generate | [X] Blueprints = binary, AI can't edit |
| **Hot Reload** | [OK] < 1 second (ReloadLayout) | [!] 5-10 seconds (Blueprint compile) |
| **Iteration Speed** | [OK] Instant JSON edits | [X] Save -> Compile -> Wait |
| **Version Control** | [OK] Clean JSON diffs | [X] Binary Blueprint diffs |
| **Source of Truth** | [OK] Single (JSON) | [X] Dual (C++ + Blueprint) |
| **Designer Access** | [!] Requires JSON knowledge | [OK] Visual Blueprint editor |

### Why NOT HTML/CSS System?

The project **already has** an equivalent:
- **HTML/CSS "structure"** -> JSON layouts (`Config/UI/*.json`)
- **CSS "styling"** -> `UProjectUIThemeData` (colors, fonts, spacing)
- **CSS "hot reload"** -> `ReloadLayout()` command (< 1 second)
- **JavaScript "binding"** -> MVVM ViewModels

Implementing HTML/CSS would duplicate existing functionality.

### Implementation

**Files Affected:**
```
Plugins/GameFeatures/ProjectMenuCoreGF/Content/UI/Menus/
+-- WBP_MainMenu.uasset         <- DELETE
+-- WBP_LoadingScreen.uasset    <- DELETE
+-- WBP_MapBrowser.uasset       <- DELETE
+-- WBP_Settings.uasset         <- DELETE
```

**Keep:**
```
Plugins/GameFeatures/ProjectMenuCoreGF/Source/ProjectMenuCoreGF/Public/Widgets/
+-- W_MainMenu.h         <- KEEP (has JSON loading)
+-- W_LoadingScreen.h    <- KEEP
+-- W_MapBrowser.h       <- KEEP
+-- W_Settings.h         <- KEEP
```

**GameFeatureAction Configuration:**
```cpp
// GameFeatureAction_AddWidget in GFD_MenuExperience asset
WidgetClass = "/Script/ProjectMenuCoreGF.W_MainMenu"
//             ^ Direct C++ class reference (no WBP_ wrapper)
```

### Migration Path

1. [OK] Verify W_ widgets have JSON loading (DONE - see [W_MainMenu.cpp](../../Plugins/GameFeatures/ProjectMenuCoreGF/Source/ProjectMenuCoreGF/Private/Widgets/W_MainMenu.cpp))
2. [PAUSE] Delete WBP_ Blueprint assets (pending)
3. [PAUSE] Update GameFeatureData to reference W_ classes directly (pending)
4. [PAUSE] Test full boot flow -> menu display (pending)

### Agentic Workflow Example

**Before (W_ + WBP_):**
```
AI Agent -> Generate MainMenu.json -> Save
v
Human -> Open WBP_MainMenu Blueprint -> Manually refresh -> Compile -> Wait 5-10s
```

**After (W_ Only):**
```
AI Agent -> Generate MainMenu.json -> Save -> Execute ReloadLayout
v
< 1 second -> Changes visible
```

**Result:** Zero human intervention, instant feedback.

---

## Tier 1: JSON Layouts (Best - Instant)

### What Can Hot Reload

**JSON Config Files:**
```
Config/UI/
+-- MainMenu.json          <- Hot reload
+-- LoadingScreen.json     <- Hot reload
+-- Settings.json          <- Hot reload
+-- MapBrowser.json        <- Hot reload
```

**Changes That Hot Reload:**
- Widget positions and sizes
- Button text and colors
- Layout structure (add/remove widgets)
- Theme references (color names, font names)
- Image paths
- Padding and margins

### How to Hot Reload

**Method 1: Console Command**
```
ReloadLayout
```

**Method 2: File Watcher (Editor Only)**
```cpp
#if WITH_EDITOR
// Automatic reload when JSON file changes on disk
FProjectWidgetLayoutWatcher ConfigWatcher;
#endif
```

### Example Workflow

1. **Edit JSON:**
   ```json
   // Config/UI/MainMenu.json
   {
     "Type": "CanvasPanel",
     "Children": [
       {
         "Type": "Button",
         "Text": "NEW BUTTON TEXT",  // Change this
         "Position": {"X": 960, "Y": 540}
       }
     ]
   }
   ```

2. **Save file** (Ctrl+S)
3. **In game console:** `ReloadLayout`
4. **Result:** Button text updates instantly

**Iteration time:** < 1 second

---

## Tier 2: ViewModel Properties (Fast)

### What Can Hot Reload

**ViewModel Property Changes:**
```cpp
// In W_MainMenu::NativeConstruct()
MenuViewModel->SetShowQuitConfirmation(true);  // Change this

// Or in code
MenuViewModel->SetCurrentScreen(EMenuScreen::Settings);
```

**Changes That Propagate:**
- All `VIEWMODEL_PROPERTY()` changes
- Array modifications (`Add`, `Remove`, `Clear`)
- Enum state changes
- Boolean flags

### How It Works

```cpp
// ViewModel (C++)
UCLASS()
class UProjectMenuViewModel : public UProjectViewModel
{
    VIEWMODEL_PROPERTY(bool, bShowQuitConfirmation);  // Auto-notifies on change
};

// Widget (C++)
void UW_MainMenu::OnQuitConfirmationChanged()
{
    // Called automatically when bShowQuitConfirmation changes
    if (QuitDialog)
    {
        QuitDialog->SetVisibility(MenuViewModel->GetShowQuitConfirmation()
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed);
    }
}
```

**No recompile needed** for property value changes if you add debug console commands:

```cpp
UFUNCTION(Exec)
void SetDebugMenuState(int32 StateIndex)
{
    MenuViewModel->SetCurrentScreen(static_cast<EMenuScreen>(StateIndex));
}
```

Then in console: `SetDebugMenuState 2`

**Iteration time:** < 1 second (if exec commands exist)

---

## Tier 3: Config Files (.ini) - Editor Restart Required

### What Requires Editor Restart

**Config/DefaultProject.ini:**
```ini
[/Script/ProjectExperience.ProjectExperienceSettings]
+ExperienceDefinitions=(ExperienceName="Menu", ...)  // [!] Restart required
```

**Config/DefaultProjectBoot.ini:**
```ini
[/Script/ProjectBoot.ProjectBootSettings]
BootMap=/Game/Project/Maps/Boot/L_OrchestratorBoot  // [!] Restart required
```

**Why restart needed:** UE loads config on engine init, cached in CDO (Class Default Objects).

**Iteration time:** 10-30 seconds (editor restart)

---

## W_ vs WBP_ Pattern (Hot Reload Impact)

### Architecture Overview

```
W_MainMenu (C++)                    WBP_MainMenu (Blueprint)
+-- Logic                           +-- Inherits from W_MainMenu
+-- JSON loading                    +-- Optional visual scripting
+-- ViewModel binding               +-- Designer customizations
+-- ReloadLayout()                  +-- NOT hot reload friendly
    [OK] Hot reload via JSON              [X] Requires editor save + compile
```

### Pattern Analysis

**W_ Widgets (C++ Base Class):**
- Contains all logic
- Loads layout from JSON
- Binds to ViewModels
- [OK] **Hot reload via JSON**
- [X] Recompile needed for C++ changes

**WBP_ Widgets (Blueprint Wrapper):**
- Optional shell inheriting from W_
- Allows Blueprint visual scripting
- Designer can add custom logic
- [!] **Requires editor save + Blueprint compile**
- [X] Not hot reload friendly

### Recommendation: When to Use Each

| Use Case | Recommended Approach | Reason |
|----------|---------------------|--------|
| **Menu UI** (buttons, navigation) | W_ directly (no WBP_) | JSON hot reload is sufficient |
| **Data-driven UI** (lists, grids) | W_ directly | ViewModel handles data |
| **Complex animations** | WBP_ wrapper | Blueprint animation editor |
| **Designer customization** | WBP_ wrapper | Non-programmers can modify |
| **Prototyping** | W_ with JSON | Fastest iteration |

### Example: Direct W_ Usage (Recommended)

```cpp
// GameFeatureAction_AddWidget.h
UPROPERTY(EditAnywhere, Category = "UI")
TSoftClassPtr<UUserWidget> WidgetClass = "/Script/ProjectMenuCoreGF.W_MainMenu";
//                                        ^ Direct C++ class reference
//                                        No WBP_ wrapper needed!
```

**Benefits:**
- [OK] JSON hot reload works
- [OK] ViewModel changes work
- [OK] No Blueprint compile overhead
- [OK] Simpler architecture

### Example: WBP_ Wrapper (When Needed)

**Create WBP_ if:**
- Designers need Blueprint visual scripting
- Custom animations via Animation Blueprints
- Team has non-programmers who edit UI

**How to create:**
1. In Content Browser -> Right-click
2. User Interface -> Widget Blueprint
3. Reparent to W_MainMenu
4. Leave empty (or add Blueprint-specific logic)
5. Reference WBP_ in GameFeatureAction

```cpp
// GameFeatureAction_AddWidget.h
UPROPERTY(EditAnywhere, Category = "UI")
TSoftClassPtr<UUserWidget> WidgetClass = "/Game/UI/WBP_MainMenu.WBP_MainMenu_C";
//                                        ^ Blueprint wrapper
//                                        Allows designer customization
```

**Trade-off:**
- [X] Slower iteration (Blueprint compile)
- [OK] Designer-friendly

---

## Hot Reload Strategy by Asset Type

### JSON Files (.json)

**[OK] TIER 1: Full Hot Reload**

**Files:**
- `Config/UI/*.json` - Widget layouts

**Hot Reload Method:**
```
ReloadLayout
```

**What reloads:**
- Widget hierarchy
- Positions, sizes, colors
- Text content
- Theme references

**What does NOT reload:**
- C++ logic
- ViewModel bindings (set once in NativeConstruct)
- Event handlers

**Iteration time:** < 1 second

---

### Data Assets (.uasset)

**[!] TIER 2-3: Partial Hot Reload**

**Theme Assets:**
```
Content/Project/UI/Themes/
+-- DA_DefaultTheme.uasset      [!] Requires hot reload command
+-- DA_DarkTheme.uasset         [!] Requires hot reload command
+-- DA_MinimalTheme.uasset      [!] Requires hot reload command
```

**Hot Reload Method:**
```cpp
// In UProjectUIThemeManager
UFUNCTION(Exec)
void SetTheme(FName ThemeName)
{
    // Loads new theme and refreshes all widgets
    ApplyTheme(ThemeName);
}
```

**Console:**
```
SetTheme DarkTheme
```

**Iteration time:** 1-2 seconds

**Experience Manifests:**
```
Content/Project/Data/Experiences/
+-- DA_MenuExperience.uasset    [X] No hot reload (requires travel)
```

**No hot reload:** Experiences load once per world. Must travel to new world to reload.

---

### Blueprint Assets (.uasset)

**[X] NO HOT RELOAD**

**WBP_ Widgets:**
```
Content/UI/Menus/
+-- WBP_MainMenu.uasset         [X] Requires editor save
+-- WBP_LoadingScreen.uasset    [X] Requires editor save
+-- WBP_Settings.uasset         [X] Requires editor save
```

**Workflow:**
1. Edit WBP_ in Blueprint editor
2. Compile (Ctrl+S)
3. Close editor window
4. Widget MAY update in PIE (depends on UE caching)
5. If not visible: Stop PIE and restart

**Iteration time:** 5-10 seconds

**Recommendation:** Avoid WBP_ for rapid iteration. Use W_ + JSON instead.

---

### C++ Files (.cpp, .h)

**[X] NO HOT RELOAD**

**W_ Widgets:**
```
Plugins/GameFeatures/ProjectMenuCoreGF/Source/
+-- Widgets/
    +-- W_MainMenu.cpp          [X] Requires recompile
    +-- W_LoadingScreen.cpp     [X] Requires recompile
    +-- W_Settings.cpp          [X] Requires recompile
```

**Workflow:**
1. Edit W_ C++ code
2. Build (Ctrl+Alt+F11 in VS)
3. Wait for compile (30-120 seconds)
4. Restart editor (10-30 seconds)
5. Test changes

**Iteration time:** 1-2 minutes

**Recommendation:** Use JSON for layout, ViewModels for data. Reserve C++ for logic only.

---

### Config Files (.ini)

**[!] TIER 3: Editor Restart Required**

**Files:**
```
Config/
+-- DefaultProject.ini          [!] Editor restart
+-- DefaultProjectBoot.ini      [!] Editor restart
+-- DefaultProjectExperience.ini [!] Editor restart (DEPRECATED - use DefaultProject.ini)
+-- DefaultEngine.ini           [!] Editor restart
```

**Workflow:**
1. Edit .ini file
2. Save
3. Close editor
4. Reopen editor
5. Config loads from disk

**Iteration time:** 10-30 seconds

---

## Hot Reload Console Commands

### Available Commands

```cpp
// Reload JSON layout
ReloadLayout

// Change theme
SetTheme <ThemeName>

// Debug menu state (if implemented)
SetDebugMenuState <StateIndex>

// Reload config (NOT hot reload - requires restart)
// No command available - must restart editor
```

### Implementing Custom Hot Reload Commands

```cpp
// In your widget class
UCLASS()
class UW_MainMenu : public UProjectUserWidget
{
public:
    // Hot reload JSON layout
    UFUNCTION(Exec)
    void ReloadLayout()
    {
        LoadLayoutFromConfig();
        BindCallbacks();
    }

    // Debug: Force menu state
    UFUNCTION(Exec)
    void DebugSetMenuScreen(int32 ScreenIndex)
    {
        if (MenuViewModel)
        {
            MenuViewModel->SetCurrentScreen(static_cast<EMenuScreen>(ScreenIndex));
        }
    }
};
```

**Usage:**
```
ReloadLayout
DebugSetMenuScreen 2
```

---

## Hot Reload Best Practices

### 1. Layer Your Architecture for Hot Reload

```
+-------------------------------------------------+
| JSON Layouts (Tier 1 - Instant Hot Reload)     |  <- Iterate here
+-------------------------------------------------+
| ViewModel Properties (Tier 2 - Fast)           |  <- Add debug commands
+-------------------------------------------------+
| W_ Logic (C++) (Slow - Recompile)              |  <- Minimize changes
+-------------------------------------------------+
| WBP_ Wrappers (Moderate - Blueprint compile)   |  <- Use sparingly
+-------------------------------------------------+
| Config Files (Slowest - Editor restart)        |  <- Set once, rarely change
+-------------------------------------------------+
```

**Guideline:** Push changes UP the stack for faster iteration.

### 2. Prefer JSON Over WBP_ for Layout

**[X] Bad:**
```
WBP_MainMenu.uasset (Blueprint)
+-- Button positions, colors, text <- Blueprint compile required
```

**[OK] Good:**
```
W_MainMenu.cpp (C++)
+-- Loads Config/UI/MainMenu.json <- ReloadLayout() works
```

### 3. Use ViewModels for Data

**[X] Bad:**
```cpp
// Hardcoded in widget
void UW_MainMenu::ShowSettings()
{
    SettingsPanel->SetVisibility(ESlateVisibility::Visible);  // <- Must recompile to change
}
```

**[OK] Good:**
```cpp
// Driven by ViewModel
void UW_MainMenu::OnCurrentScreenChanged()
{
    bool bShowSettings = (MenuViewModel->GetCurrentScreen() == EMenuScreen::Settings);
    SettingsPanel->SetVisibility(bShowSettings ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

// In console: DebugSetMenuScreen 1  <- No recompile!
```

### 4. Add Debug Exec Commands

```cpp
UCLASS()
class UW_MainMenu : public UProjectUserWidget
{
public:
    // Hot reload layout
    UFUNCTION(Exec)
    void ReloadLayout();

    // Debug: Test all screens
    UFUNCTION(Exec)
    void DebugCycleScreens();

    // Debug: Force error state
    UFUNCTION(Exec)
    void DebugShowError(const FString& ErrorText);
};
```

**Usage during development:**
```
ReloadLayout
DebugCycleScreens
DebugShowError "Test error message"
```

### 5. Document Hot Reload Capabilities

**In widget header:**
```cpp
/**
 * W_MainMenu - Main menu widget
 *
 * Hot Reload:
 * - JSON layout: ReloadLayout() command
 * - Theme: SetTheme <ThemeName> command
 * - Debug: DebugSetMenuScreen <Index> command
 *
 * NOT hot reload:
 * - C++ logic changes (requires recompile)
 * - ViewModel property bindings (set in NativeConstruct)
 */
UCLASS()
class UW_MainMenu : public UProjectUserWidget { ... };
```

---

## File Organization for Hot Reload

### Recommended Structure

```
Project Root/
+-- Config/                         <- Tier 3: Editor restart
|   +-- DefaultProject.ini
|   +-- DefaultProjectBoot.ini
|   +-- UI/                         <- Tier 1: Hot reload
|       +-- MainMenu.json
|       +-- LoadingScreen.json
|       +-- Settings.json
|       +-- MapBrowser.json
|
+-- Plugins/
|   +-- Core/ProjectUI/
|   |   +-- Source/                 <- C++ logic (slow)
|   |       +-- ViewModels/         <- Data layer
|   |
|   +-- GameFeatures/ProjectMenuCoreGF/
|       +-- Source/
|       |   +-- Widgets/            <- W_ classes (slow)
|       |       +-- W_MainMenu.cpp
|       |       +-- W_LoadingScreen.cpp
|       |
|       +-- Content/UI/Menus/       <- WBP_ wrappers (moderate)
|           +-- WBP_MainMenu.uasset     <- OPTIONAL
|           +-- WBP_LoadingScreen.uasset <- OPTIONAL
|
+-- Content/Project/UI/Themes/      <- Tier 2: Theme hot reload
    +-- DA_DefaultTheme.uasset
    +-- DA_DarkTheme.uasset
```

**Key:**
- Keep hot reload assets (JSON) in `Config/UI/`
- W_ classes define logic (C++, slow)
- WBP_ wrappers are OPTIONAL (moderate, use sparingly)
- Themes support hot reload via `SetTheme` command

---

## Troubleshooting Hot Reload

### "ReloadLayout doesn't work"

**Check:**
1. Is JSON file path correct?
   ```cpp
   FString ConfigFilePath = FPaths::ProjectConfigDir() / TEXT("UI/MainMenu.json");
   ```

2. Is file watcher initialized (editor only)?
   ```cpp
   #if WITH_EDITOR
   InitializeConfigWatcher();
   #endif
   ```

3. Is widget still valid?
   ```cpp
   if (!IsValid(this)) return;
   ```

### "ViewModel changes don't update UI"

**Check:**
1. Are you calling setter (not direct property access)?
   ```cpp
   MenuViewModel->SetCurrentScreen(EMenuScreen::Settings);  // [OK] Notifies
   MenuViewModel->CurrentScreen = EMenuScreen::Settings;    // [X] No notification
   ```

2. Did you bind the property change handler?
   ```cpp
   MenuViewModel->OnCurrentScreenChanged.AddUObject(this, &UW_MainMenu::OnCurrentScreenChanged);
   ```

### "Config changes don't apply"

**Config files require editor restart.** This is by design. For hot reload, use JSON or ViewModel debug commands instead.

---

## Agent Notes

**When implementing hot reload:**

1. **Default to JSON** for all layout/positioning
2. **Add Exec commands** for ViewModel state changes
3. **Avoid WBP_ wrappers** unless designers need Blueprint
4. **Document hot reload commands** in class headers
5. **Test hot reload** as part of feature development

**When advising users:**

- "Config changes require editor restart" <- Be clear about this
- "Use ReloadLayout for JSON changes" <- Promote this workflow
- "WBP_ slows iteration" <- Discourage unless needed
- "Add debug commands for testing" <- Encourage this pattern

---

## See Also

- [UI](README.md) - Router to layout, MVVM, and theme docs
- [Core Principles](../architecture/core_principles.md) - Loose coupling and event-driven design
- [Plugin Architecture](../architecture/plugin_architecture.md) - Module organization
