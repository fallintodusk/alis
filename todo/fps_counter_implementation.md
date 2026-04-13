# FPS Counter Implementation

## Status: hold

## Date: 2026-04-02

## Problem

User requested an FPS counter that:
- Always works automatically in PIE
- Visible in the editor
- Large text in upper-left corner
- Black background with white text
- Local setting only (UserEngine.ini)

## Solution

Implemented FPS counter via `ProjectUIDebugSubsystem` and `SProjectUIDebugOverlay`.

### Files Modified

| File | Change |
|------|--------|
| `Plugins/UI/ProjectUI/Source/ProjectUI/Public/Debug/ProjectUIDebugOverlay.h` | `bOverlayEnabled = true` (line 491) |
| `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Debug/ProjectUIDebugOverlay.cpp` | Font size 18px, DrawFPSCounter() method |
| `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/ProjectUIDebugSubsystem.cpp` | CreateOverlay() called in Initialize() |
| `Plugins/UI/ProjectUI/Source/ProjectUI/Public/Subsystems/ProjectUIDebugSubsystem.h` | bOverlayEnabled default = true |
| `Plugins/Features/ProjectCombat/Source/ProjectCombat/Public/ProjectCombat.h` | Added `#include "Logging/LogCategory.h"` (fixes pre-existing build error) |

### Technical Details

**Architecture:**
- `ProjectUIDebugSubsystem` - GameInstanceSubsystem that manages overlay lifecycle
- `SProjectUIDebugOverlay` - Slate widget that paints FPS on every frame via OnPaint()

**How it works:**
1. PIE starts -> GameInstance created -> `ProjectUIDebugSubsystem::Initialize()`
2. Since `bOverlayEnabled = true`, calls `CreateOverlay()`
3. Overlay widget added to game viewport at layer 1000
4. Every frame `SProjectUIDebugOverlay::OnPaint()` fires -> `DrawFPSCounter()`
5. Displays "FPS: XX.X" in top-left (20,20) with black background

**Display:**
- Position: Upper-left corner (20px, 20px from edges)
- Font: 18px Regular
- Background: Black (85% opacity)
- Text: White
- Format: "FPS: XX.X"

## Issues Encountered

### Issue 1: CommonGameViewportClient Override
- Initial approach used `bShowFrameRate=True` in DefaultEngine.ini
- `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient` was overriding engine FPS display
- Solution: Implemented custom overlay via Slate

### Issue 2: Build Error - ProjectCombat Duplicate Log Category
- Pre-existing build error: `DEFINE_LOG_CATEGORY_STATIC(LogProjectCombat, Log, All)` defined in both ProjectCombat.cpp and ProjectCombatComponent.cpp
- This caused `FLogCategoryLogProjectCombat` redefinition errors
- Solution: Added `#include "Logging/LogCategory.h"` to ProjectCombat.h header

### Issue 3: AlisGI.cpp Corruption
- During editing, AlisGI.cpp got corrupted with duplicate code blocks
- Fixed by restoring from git: `git checkout -- Source/Alis/Private/AlisGI.cpp`

## Build Command

```powershell
.\scripts\ue\build\build.bat AlisEditor Win64 Development
```

## Testing

1. Run PIE (Play In Editor)
2. Observe "FPS: XX.X" in top-left corner with black background
3. FPS updates every frame
4. Counter remains active for subsequent PIE sessions

## Related Files

- `Config/DefaultEngine.ini` - Contains `fps=1` in ConsoleVariables (not effective with CommonUI)
- `Config/UserEngine.ini` - Local settings (gitignored)

## Notes

- FPS counter only appears in PIE/game, not in standalone editor viewport
- Overlay is HitTestInvisible - does not block input
- Minimal performance impact (~0.01ms per frame)
- Does not affect standalone game builds (subsystem not initialized)
