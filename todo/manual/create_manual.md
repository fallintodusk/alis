# Manual Asset Creation Guide

**Prerequisites:** Phase 8.4 (placeholders) must be complete before starting.

**Goal:** Create empty WBP_ Blueprint shells to activate agent-created W_ widgets.

---

## 🎨 Manual Work Required (15 minutes)

Note: One-time editor setup. After these shells exist, further UI changes flow via hot-reload JSON/config and agent-compatible code; no repeated manual edits needed.

### 1. Create Empty WBP_ Blueprints (10 min)

**Location:** `Plugins/GameFeatures/ProjectMenuCore/Content/UI/Menus/`

**Steps for each widget:**
1. Enable "Show Plugin Content" in Content Browser
2. Navigate to plugin content folder (create `UI/Menus/` if needed)
3. Right-click → User Interface → Widget Blueprint
4. Name: `WBP_MainMenu` (or WBP_MapBrowser, WBP_LoadingScreen, WBP_Settings)
5. Open blueprint → File → Reparent Blueprint → Select C++ parent (W_MainMenu, etc.)
6. **Verify widget hierarchy is EMPTY** (no CanvasPanel, no widgets)
7. Save and close

**Create 4 widgets (manual, not auto-scanned as TODO):**
- WBP_MainMenu (parent: W_MainMenu)
- WBP_MapBrowser (parent: W_MapBrowser)
- WBP_LoadingScreen (parent: W_LoadingScreen)
- WBP_Settings (parent: W_Settings)

### 2. Update Plugin Settings (5 min)

**Edit:** `Config/DefaultGame.ini` or Project Settings → ProjectMenuUI

```ini
MenuScreenClass=/Game/Plugins/GameFeatures/ProjectMenuCore/Content/UI/Menus/WBP_MainMenu.WBP_MainMenu_C
LoadingScreenClass=/Game/Plugins/GameFeatures/ProjectMenuCore/Content/UI/Menus/WBP_LoadingScreen.WBP_LoadingScreen_C
```

**Note:** Reference WBP_ shells, not W_ C++ classes.

### 3. Test (PIE)

- Play in Editor → Main menu appears with default layout
- All buttons functional (Play/Settings/Credits/Quit)
- Click "Play" → Map browser appears
- Select map → Loading screen shows progress

---

## 🔧 Optional Customization

### Replace Placeholder Textures
**Location:** `Plugins/GameFeatures/ProjectMenuCore/Content/UI/Textures/Placeholders/`

1. Import final artwork to `/Textures/`
2. Edit JSON config (e.g., `/Config/UI/MainMenu.json`)
3. Change texture path from placeholder to final asset
4. Hot reload triggers automatically

### Customize Layouts
**Files:** `/Config/UI/*.json`

1. Edit JSON (change positions, sizes, colors, text)
2. Save file
3. Hot reload: Console command `W_MainMenu.ReloadLayout()` or restart PIE
4. Verify changes

### Create Custom Theme
**Only if different colors/fonts needed:**

1. Create Blueprint Data Asset: `DA_CustomTheme`
2. Derive from: `UProjectUIThemeData`
3. Customize: Colors, Typography, Spacing, Animation timings
4. Set in plugin settings: `DefaultTheme` → `DA_CustomTheme`

---

## 📚 Documentation

- [Widget Naming Conventions](../docs/widget_naming.md) - W_ vs WBP_ pattern
- [UI System](../docs/ui_system.md) - MVVM architecture
- [ProjectUI Plugin](../Plugins/Core/ProjectUI/README.md) - Base classes
- [create_architecture.md](create_architecture.md) - Phase 8 tasks

---

## Deprecation Consideration (Hot-Reload Agentic Path)

These WBP shells exist only as thin presentation wrappers over C+++JSON. If we fully adopt Slate/CommonUI + JSON layouts at runtime, we can remove the need to create WBP shells entirely and bind screens directly in C++/data. Until configs and UI loaders are updated to that mode, keep this one-time manual step.
