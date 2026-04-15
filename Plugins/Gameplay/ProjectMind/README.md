# ProjectMind

Purpose
- Runtime gameplay plugin for player inner-voice guidance ("thoughts").
- Owns thought generation and runtime decision logic.
- UI rendering is handled by UI plugins through existing ProjectUI framework patterns.
- Mind-specific UI lives in `Plugins/UI/ProjectMindUI`.

Current status
- Runtime `IMindService` is registered in `FProjectServiceLocator`.
- Local pawn context bootstrap is owned by `UProjectMindRuntimeBootstrapSubsystem`.
- Initial dialogue listener is wired through `IDialogueService`.
- Default vitals/inventory thought sources are registered by ProjectMind.
- Quest source is optional and fail-soft. No mandatory centralized quest controller is required.
- Dialogue thought mapping is signal-driven and loaded from `Plugins/Gameplay/ProjectMind/Data/dialogue_thought_mappings.json`.
- Idle scan/beacon rules are loaded from `Plugins/Gameplay/ProjectMind/Data/scan_thought_rules.json`.
- Idle scan is input-driven: one-shot 3 second callback, re-armed by input pulses, no repeated scan polling timer.

Cross-reference dependencies (check before renaming)
- `dialogue_thought_mappings.json` signal_tags reference DLG_*.json tree IDs and node IDs
- `vitals_thought_mappings.json` state_tag references ProjectVitals hysteresis tags
- `scan_thought_rules.json` match_any_tags references gameplay tags on world actors

Validate after renaming dialogue trees/nodes:
```bash
python scripts/ue/check/gameplay/projectmind/validate_data.py
```

Pre-commit hook (`.githooks/pre-commit`) runs this automatically.

Router
- Vision: [docs/vision.md](docs/vision.md)
- Architecture: [docs/architecture.md](docs/architecture.md)
- Runtime: [docs/runtime.md](docs/runtime.md)
- Boundaries and SOLID Guardrails: [docs/boundaries.md](docs/boundaries.md)
- Dialogue Integration: [docs/dialogue_integration.md](docs/dialogue_integration.md)
- UI Integration: [docs/ui_integration.md](docs/ui_integration.md)
- Data Model: [docs/data_model.md](docs/data_model.md)
