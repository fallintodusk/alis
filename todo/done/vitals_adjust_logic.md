# Vitals Config Refactor - Implementation Tracker

**Status:** ALL PHASES COMPLETE (2026-02-18) - Config + Code + UI
**Created:** 2026-02-16
**Priority:** High - Core survival mechanic

---

## Design Reference (DO NOT DUPLICATE HERE!)

**All design info lives in permanent docs:**
- **System overview:** @[Plugins/Gameplay/ProjectVitals/README.md](../../Plugins/Gameplay/ProjectVitals/README.md)
- **Config structure:** @[Plugins/Gameplay/ProjectVitals/Data/vitals_config.json](../../Plugins/Gameplay/ProjectVitals/Data/vitals_config.json)
- **Data flow:** @[Plugins/Gameplay/ProjectVitals/README.md#data-flow-tickvitals](../../Plugins/Gameplay/ProjectVitals/README.md)

**Key principles (from README):**
- Two-layer system: Metabolism (Layer 1) + Dying Mode (Layer 2)
- Real-world physiology: MET values, 1:1 time scale, activity-based calorie burn
- Timeline-based SOT: All rates derived from death/depletion timelines

---

## Config Refactor Progress

### ✅ Completed (2026-02-16)

**Timeline-based SOT conversion:**
- [x] Removed pre-calculated drain rates (`conditionDrainPerSec`, `baseRegenPerSec`)
- [x] Replaced with timeline targets (`conditionDeathDays`, `baseRegenDays`, `hoursToEmpty`)
- [x] Component will derive rates at runtime from timelines

**Naming consistency:**
- [x] Standardized all runtime values to `current/max` pattern
- [x] Renamed `severity` → `current` for status effects
- [x] Renamed `vitalDrainHours` → `hoursToEmpty` (clearer intent)

**Structure cleanup:**
- [x] Removed `version` field (Docker-style self-documenting schema)
- [x] Removed `tickInterval` from config (use UE's `PrimaryComponentTick.TickInterval = 1.0f`)
- [x] Removed duplicate `bleedingDrainPerSec` from metabolism (hardcode 0.5 in component)
- [x] Removed `drainMultiplier` from status effects (formula constants belong in code)
- [x] Nested `activity` under `metabolism` (shows it's part of metabolism calculation)

**Co-location improvements:**
- [x] Moved condition drain rates into survival resources (`calories.conditionDrainPerSec`)
- [x] Added status effect tiers (bleeding/poisoned/radiation severity thresholds)

**Documentation:**
- [x] Updated README with two-layer system emphasis
- [x] Added real-world physiology design philosophy section
- [x] Documented activity classification and metabolism flow
- [x] Added dying mode constraint principle

---

## Code Implementation (COMPLETE)

### Phase 1: VitalsConfig Loader

**File:** `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Private/VitalsConfig.h/cpp`

**Tasks:**
- [x] Create `FVitalsConfig` struct matching new JSON structure
- [x] Add `UVitalsConfigAsset` (UDataAsset) for loading JSON
- [x] Parse nested `condition.survival.*`, `condition.metabolism.activity.*`
- [x] Derive rates from timelines:
  - `BaseRegenPerSec = MaxCondition / (BaseRegenDays * 86400)`
  - `ConditionDeathPerSec = MaxCondition / (ConditionDeathDays * 86400)`
  - `FatigueGainPerSec = MaxFatigue / (HoursToEmpty * 3600)`

**Validation:**
- [x] Verify all 30 config values load correctly (see config file for count)
- [x] Add unit test for rate derivation math
- [x] Log derived rates on component start for debugging

---

### Phase 2: Component Integration

**File:** `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Private/ProjectVitalsComponent.cpp`

**Tasks:**
- [x] Remove hardcoded rates, load from `VitalsConfigAsset`
- [x] Set `PrimaryComponentTick.TickInterval = 1.0f` (time-based, not frame-based)
- [x] Update `ApplyMetabolism()`:
  - Use `metabolism.activity.speedThreshold*` for classification
  - Use `metabolism.activity.met*` for MET values
  - Use `metabolism.characterWeightKg` for calorie burn
  - Use `metabolism.hydrationPerKcal` for water loss
- [x] Update `ApplyConditionDelta()`:
  - Use derived `conditionDeathPerSec` from survival resources
  - Use derived `baseRegenPerSec` from condition config
- [x] Update `ApplyBleedingDrain()`:
  - Hardcode `BleedingDrainMultiplier = 0.5f` (formula constant)

**Testing:**
- [x] Boot test - component initializes without errors
- [x] Verify idle metabolism: ~2500 kcal lasts ~1 day
- [x] Verify sprint metabolism: ~2500 kcal lasts ~3 hours
- [x] Verify condition drain at 0% hydration: death in 1 day
- [x] Verify condition regen when healthy: 4 days to full heal

---

### Phase 3: Panel UI Redesign (Two-Column Layout)

**Goal:** Split vitals panel into two columns mirroring `vitals_config.json` structure.
Left = Condition (life/death), Right = Stamina (short-term energy).
Show ALL config values with visual hierarchy + tooltips on every parameter.

**Design ref:** @[Plugins/Gameplay/ProjectVitals/Data/vitals_config.json](../../Plugins/Gameplay/ProjectVitals/Data/vitals_config.json)

```
DEFAULT VIEW (STATUS + METABOLISM collapsed):
+================== VITALS ========================[X]+
|                                                     |
| CONDITION (Life/Death)    | STAMINA (Energy)        |
|                           |                         |
| [========BAR=====] 75 HP  | [========BAR======] 80  |
| Regen 1.0x | 4.0 days    | Regen 1.0x              |
| +0.000289 HP/s            | Regen: OK               |
|                           |                         |
| SURVIVAL                  | MOVEMENT                |
|                           |  Sprint     10.0 /s     |
|  Calories [===] 1100 kcal |  Jog         5.0 /s     |
|   -97.5 kcal/hr (Idle)   |  Base Regen  6.67 /s    |
|   Life: 13 days           |                         |
|                           | INTERACTIONS            |
|  Hydration [===] 1.5 L   |  Light 2.0  Medium 5.0  |
|   -0.10 L/hr (Idle)      |  Heavy 15.0 Min Req 5.0 |
|   Life: 1 day             |                         |
|                           | COMBAT                  |
|  Fatigue [===] 70%        |  Light Atk  10.0        |
|   +6.25%/hr               |  Heavy Atk  25.0        |
|   16h to empty | Life: 4d |  Dodge      15.0        |
|                           |  Block       5.0 /s     |
| [+] STATUS                |                         |
| [+] METABOLISM            |                         |
+==========================[H] to toggle==============+

EXPANDED VIEW (after clicking [+]):
| [-] STATUS                |                         |
|  Bleeding   0.0           |                         |
|  Poisoned   0.0           |                         |
|  Radiation  0.0           |                         |
| [-] METABOLISM            |                         |
|  Weight 75 kg             |                         |
|  Hydration 0.001 L/kcal  |                         |
|  Activity:                |                         |
|    Idle   1.3  < 10       |                         |
|    Walk   2.5  < 150      |                         |
|    Jog    6.0  < 350      |                         |
|    Sprint 10.0 >= 350     |                         |
```

**Design rules:**
- No "/ max" on values (bar shows full scale visually)
- `regenRate` + `baseRegenDays` on same row (compact)
- Sub-sections indented to mirror JSON nesting depth
- Section headers: `HeadingMedium` font for Condition/Stamina, `BodyMedium` for sub-sections
- `conditionDeathDays` shown as "Life: X days" (positive framing, user-friendly)
- Status shows `current` only; tiers shown in tooltip on hover
- Right column naturally shorter (stamina is simpler) - leave empty space
- Every value has tooltip with detailed explanation
- **Collapsible sections:** STATUS and METABOLISM default collapsed, [+]/[-] toggle
  - UMG pattern: Button([+/-] text) + VerticalBox(content, Collapsed by default)
  - Click toggles content VerticalBox visibility (Collapsed <-> Visible)
  - Button text toggles between [+] and [-]
  - State not persisted across panel close/open (always default collapsed)

---

#### Phase 3a: JSON Layout (VitalsPanel.json)

**File:** `Plugins/UI/ProjectVitalsUI/Data/VitalsPanel.json`

**Tasks:**
- [x] Replace single `VerticalBox` content with `HorizontalBox` holding two columns
- [x] Left column (`ConditionColumn`): `VerticalBox`, ~55% width
- [x] Right column (`StaminaColumn`): `VerticalBox`, ~45% width
- [x] Vertical divider `Border` (1px) between columns
- [x] Widen panel from 810 to ~900px (more content)

**Left column widgets (condition system):**
- [x] `ConditionHeader` - "CONDITION" HeadingMedium
- [x] `ConditionBar` + `ConditionValue` (no /max, just "75 HP")
- [x] `ConditionRegenRow` - "Regen 1.0x | 4.0 days" (two TextBlocks inline)
- [x] `ConditionRate` - "+0.000289 HP/s" (dynamic from ViewModel)
- [x] Divider
- [x] `SurvivalHeader` - "SURVIVAL" BodyMedium
- [x] `CaloriesBar` + `CaloriesValue` ("1100 kcal")
- [x] `CaloriesRate` - "-97.5 kcal/hr (Idle)" (dynamic)
- [x] `CaloriesLifeDays` - "Life: 13 days" BodySmall (static from config)
- [x] `HydrationBar` + `HydrationValue` ("1.5 L")
- [x] `HydrationRate` (dynamic)
- [x] `HydrationLifeDays` - "Life: 1 day" (static)
- [x] `FatigueBar` + `FatigueValue` ("70%")
- [x] `FatigueRate` (dynamic)
- [x] `FatigueTimeline` - "16h to empty | Life: 4 days" (static)
- [x] Divider
- [x] `StatusToggle` - Button "[+] STATUS" (click to expand/collapse)
- [x] `StatusContent` - VerticalBox (default: Collapsed)
  - [x] `BleedingRow` - "Bleeding  0.0" (dynamic from ViewModel)
  - [x] `PoisonedRow` - "Poisoned  0.0"
  - [x] `RadiationRow` - "Radiation 0.0"
- [x] Divider
- [x] `MetabolismToggle` - Button "[+] METABOLISM" (click to expand/collapse)
- [x] `MetabolismContent` - VerticalBox (default: Collapsed)
  - [x] `MetabolismWeightRow` - "Weight: 75 kg" (static)
  - [x] `MetabolismHydrationRow` - "Hydration: 0.001 L/kcal" (static)
  - [x] `ActivityHeader` - "Activity:" BodySmall
  - [x] `ActivityIdle` - "Idle   1.3  < 10 cm/s"
  - [x] `ActivityWalk` - "Walk   2.5  < 150 cm/s"
  - [x] `ActivityJog` - "Jog    6.0  < 350 cm/s"
  - [x] `ActivitySprint` - "Sprint 10.0 >= 350 cm/s"

**Right column widgets (stamina system):**
- [x] `StaminaHeader` - "STAMINA" HeadingMedium
- [x] `StaminaBar` + `StaminaValue` (no /max, just "80")
- [x] `StaminaRegenRow` - "Regen 1.0x"
- [x] `StaminaRate` - "Regen: OK" (dynamic)
- [x] Divider
- [x] `MovementHeader` - "MOVEMENT" BodyMedium
- [x] `MovementSprint` - "Sprint  10.0 /s"
- [x] `MovementJog` - "Jog      5.0 /s"
- [x] `MovementRegen` - "Regen    6.67 /s"
- [x] Divider
- [x] `InteractionsHeader` - "INTERACTIONS" BodyMedium
- [x] `InteractionsRow1` - "Light 2.0  Medium 5.0" (two pairs per row)
- [x] `InteractionsRow2` - "Heavy 15.0 Min Req 5.0"
- [x] Divider
- [x] `CombatHeader` - "COMBAT" BodyMedium
- [x] `CombatRow1` - "Light Atk 10.0  Heavy Atk 25.0"
- [x] `CombatRow2` - "Dodge 15.0      Block 5.0/s"

**Remove from current layout:**
- [x] Old single-column HEALTH/SURVIVAL NEEDS/STATUS EFFECTS/METABOLISM CONFIG sections
- [x] `ConfigText` TextBlock (replaced by structured metabolism/activity rows)
- [x] `SectionHealth`, `SectionNeeds`, `EffectsHeader`, `ConfigHeader` old headers

---

#### Phase 3b: Widget Code (W_VitalsPanel.cpp/.h)

**File:** `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Public/Widgets/W_VitalsPanel.h`
**File:** `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Private/Widgets/W_VitalsPanel.cpp`

**Tasks:**
- [x] Add cached widget pointers for new layout elements:
  - Static config rows: `CaloriesLifeDays`, `HydrationLifeDays`, `FatigueTimeline`
  - Static config rows: `MetabolismWeight`, `MetabolismHydration`
  - Static config rows: `ActivityIdle/Walk/Jog/Sprint`
  - Static config rows: `MovementSprint/Jog/Regen`
  - Static config rows: `InteractionsRow1/Row2`, `CombatRow1/Row2`
  - Dynamic: `BleedingValue`, `PoisonedValue`, `RadiationValue`
  - Layout: `ConditionRegenRow` (regenRate + baseRegenDays)
  - Collapsible: `StatusToggle` (Button), `StatusContent` (VerticalBox)
  - Collapsible: `MetabolismToggle` (Button), `MetabolismContent` (VerticalBox)
- [x] Update `NativeConstruct()`:
  - Bind new widgets via `Binder.FindRequired/FindOptional`
  - Remove old `ConfigText` binding
  - Bind `StatusToggle->OnClicked` -> `HandleStatusToggle()`
  - Bind `MetabolismToggle->OnClicked` -> `HandleMetabolismToggle()`
  - Set `StatusContent->SetVisibility(ESlateVisibility::Collapsed)` (default)
  - Set `MetabolismContent->SetVisibility(ESlateVisibility::Collapsed)` (default)
- [x] Add `HandleStatusToggle()` / `HandleMetabolismToggle()`:
  - Toggle content VerticalBox between Collapsed <-> Visible
  - Update button text: "[+] STATUS" <-> "[-] STATUS"
  - No state persistence (resets to collapsed on panel reopen)
- [x] Replace `PopulateConfigSection()` with `PopulateStaticValues()`:
  - Read `FVitalsConfigLoader::Get()` once
  - Set static texts: life days, metabolism, activity, movement, interactions, combat
  - Format: "Life: 13 days", "Weight: 75 kg", etc.
- [x] Update `RefreshFromViewModel()`:
  - Remove "/ max" from value formatting (just current)
  - Add status effect current values (bleeding/poisoned/radiation from ViewModel)
  - Keep dynamic: bars, current values, rate texts, status
- [x] Update `SetupRowTooltips()` - tooltips on EVERY parameter:
  - Condition bar: "Health points. Drains when survival resources depleted..."
  - baseRegenDays: "Time to regenerate 0->100 when all vitals healthy. Rate: X HP/s"
  - Life days per resource: "Survival time from X=0% until death. Total: Y days"
  - hoursToEmpty: "Hours of wakefulness until fatigue reaches 100%"
  - Bleeding current: "Severity 0-1. Tiers: light 0.02, moderate 0.1, severe 0.5, critical 1.0"
  - Weight: "Character mass for calorie burn formula: MET x Weight x 3.5 / 200"
  - MET values: "Metabolic Equivalent of Task. Higher = more calories burned per second"
  - Speed thresholds: "Movement speed cutoff (cm/s) for activity classification"
  - Sprint drain: "Stamina cost per second. At 100 stamina = 10s sprint"
  - Interaction costs: "Stamina cost per action. Min Required = cannot act below this"
  - Combat costs: "Stamina per attack/dodge. Block is per second held"
- [x] Update value formatters:
  - `FormatConditionValue(Current)` -> "75 HP" (drop Max param)
  - `FormatStaminaValue(Current)` -> "80" (drop Max param)
  - `FormatCaloriesValue(Current)` -> "1100 kcal" (drop Max param)
  - `FormatHydrationValue(Current)` -> "1.5 L" (drop Max param)
  - `FormatFatigueValue(Percent)` -> keep ("70%")

---

#### Phase 3c: ViewModel Updates

**File:** `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Private/MVVM/VitalsViewModel.cpp`
**File:** `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Public/MVVM/VitalsViewModel.h`

**Tasks:**
- [x] Add severity properties for numeric display:
  - `VIEWMODEL_PROPERTY_READONLY(float, BleedingSeverity)`
  - `VIEWMODEL_PROPERTY_READONLY(float, PoisonedSeverity)`
  - (RadiationLevel already exists as float)
- [x] Update `RefreshFromASC()`: set `BleedingSeverity` / `PoisonedSeverity` from StatusAttributeSet
- [x] Update `UpdateRateTexts()` if config struct fields changed (Phase 1 dependency)

---

#### Phase 3 Testing

- [x] Build: `scripts/ue/standalone/build.ps1`
- [x] Integration: `run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Vitals.*"`
- [x] Boot test: `scripts/ue/test/smoke/boot_test.bat`
- [x] Visual: Open panel (H key), verify two-column layout
- [x] Verify all tooltips appear on hover for every parameter
- [x] Verify dynamic values update (bars, rates, status effects)
- [x] Verify static values load from config (life days, metabolism, activity)
- [x] Verify ESC / click-outside / close button still work
- [x] Verify [+]/[-] toggles expand/collapse STATUS and METABOLISM sections
- [x] Verify sections default to collapsed on panel open

---

## Implementation Order

1. **Phase 1** (VitalsConfig loader) - must complete first, all phases depend on it
2. **Phase 2** (Component integration) - can run in parallel with Phase 3a
3. **Phase 3a** (JSON layout) - no code dependency, pure layout
4. **Phase 3b** (Widget code) - depends on Phase 1 (config struct) + Phase 3a (widget names)
5. **Phase 3c** (ViewModel) - depends on Phase 1 (config struct)

---

## Blockers / Decisions Needed

- **Phase 3c:** Add `BleedingSeverity`/`PoisonedSeverity` to ViewModel (follows existing pattern)

---

## References

- Config file: [vitals_config.json](../../Plugins/Gameplay/ProjectVitals/Data/vitals_config.json)
- README: [ProjectVitals/README.md](../../Plugins/Gameplay/ProjectVitals/README.md)
- Component: [ProjectVitalsComponent.cpp](../../Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Private/ProjectVitalsComponent.cpp)
- Panel widget: [W_VitalsPanel.cpp](../../Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Private/Widgets/W_VitalsPanel.cpp)
- Panel layout: [VitalsPanel.json](../../Plugins/UI/ProjectVitalsUI/Data/VitalsPanel.json)
- ViewModel: [VitalsViewModel.h](../../Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Public/MVVM/VitalsViewModel.h)
