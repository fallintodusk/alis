# TODO: Vitals Panel - DONE (2026-02-18)

## Completed

- [x] Rate-of-change text per vital row
- [x] Hover tooltips with config details
- [x] Metabolism config section
- [x] ScrollBox fix (not in LayoutWidgetRegistry)
- [x] Layout readability pass (larger text, margins, accents)
- [x] Two-column layout (condition left, stamina right)
- [x] Collapsible STATUS and METABOLISM sections
- [x] Config-driven values (vitals_config.json SOT)
- [x] Timing realism (timeline-based rates in config)

## Deferred (not worth implementing now)

**Vitals-specific DumpTree test + script:**
Skipped. Vitals panel is static two-column layout (text + bars), unlike inventory which has
dynamic grids, container rebuilds, and multi-resolution concerns. Existing 4 UI integration
tests (DefinitionsContract, LayoutContract, ViewModelTagPriority, SharedViewModelContract)
already cover widget structure, data binding, and VM sharing. Revisit only if vitals panel
gains dynamic layout behavior (e.g., descriptor-driven sections).

**ScrollBox in LayoutWidgetRegistry:**
Not needed - panel content fits at current size. Add if panel grows.

**Current MET replication:**
Server-only value. Would need attribute replication for cosmetic-only info. Low ROI.

**Dynamic tooltip refresh:**
Tooltips are set once from config. Live condition rate changes are shown in rate text
(ConditionRateText updates every RefreshFromASC). Tooltip showing exact drain breakdown
is nice-to-have, not blocking.

**Localization:**
All UI text is English hardcoded, consistent with rest of project. Localize when
the project adds i18n support globally.

## References

- Config: [vitals_config.json](../../Plugins/Gameplay/ProjectVitals/Data/vitals_config.json)
- Design: [design_vision.md](../../Plugins/Gameplay/ProjectVitals/docs/design_vision.md)
- Panel layout: [VitalsPanel.json](../../Plugins/UI/ProjectVitalsUI/Data/VitalsPanel.json)
- Panel widget: [W_VitalsPanel.cpp](../../Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Private/Widgets/W_VitalsPanel.cpp)
