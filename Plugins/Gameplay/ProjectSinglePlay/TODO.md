# ProjectSinglePlay TODO

## Current State: Core Implementation Complete

GameMode orchestrates feature init via `FFeatureRegistry`. Features self-register and own their init logic.
Client-only UI/input lives in `ProjectSinglePlayClient`.

---

## Future

### Testing
- [ ] Functional tests for URL-based travel
- [ ] World-agnostic tests (same mode on different maps)
- [ ] Edge case tests (missing config, PIE reload)

### Rules Location (Deferred)
- [ ] Identify which rules belong here vs feature plugins
- [ ] Movement/stamina/health baseline rules
- [ ] Damage routing to combat feature

---

## Architecture Notes

**URL-based selection:**
```
ServerTravel("MapName?Mode=Medium?game=/Script/ProjectSinglePlay.SinglePlayerGameMode")
```
Note: Use `?` separators for all options and keep `game=` last.

**Feature init flow:**
1. Module startup: Feature registers lambda with `FFeatureRegistry`
2. GameMode: Loops `FeatureNames` in order
3. GameMode: Calls `FFeatureRegistry::InitializeFeature()` with context
4. Feature: Lambda runs, attaches components to `Context.Pawn`

**Invariants:**
- No hard dependencies on feature plugins
- No references to specific worlds
- Features self-register, GameMode just orchestrates

---

## How to Add Modes

Edit `SinglePlayModeDefaults.cpp`:

```cpp
DEFINE_SINGLEPLAY_MODE(MyNewMode)
{
    Config.ModeName = FName(TEXT("MyNewMode"));
    Config.FeatureNames.Add(TEXT("Combat"));
    Config.FeatureNames.Add(TEXT("Inventory"));
    Config.FeatureConfigs.Add(FName(TEXT("Combat")), TEXT("{\"damageMultiplier\":1.5}"));
}
```

Or add JSON override in `Config/SinglePlay/ModeOverrides.json`.

---

## See Also

- `Plugins/Gameplay/ProjectFeature/README.md` - Feature registry design
- `Plugins/Gameplay/ProjectGameplay/README.md` - Gameplay framework
