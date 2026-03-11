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
- Dialogue thought mapping is signal-driven and loaded from `Content/Data/Schema/Gameplay/ProjectMind/dialogue_thought_mappings.json`.
- Idle scan/beacon rules are loaded from `Content/Data/Schema/Gameplay/ProjectMind/scan_thought_rules.json`.
- Idle scan is input-driven: one-shot 3 second callback, re-armed by input pulses, no repeated scan polling timer.

Router
- Vision: [docs/vision.md](docs/vision.md)
- Architecture: [docs/architecture.md](docs/architecture.md)
- Runtime: [docs/runtime.md](docs/runtime.md)
- Boundaries and SOLID Guardrails: [docs/boundaries.md](docs/boundaries.md)
- Dialogue Integration: [docs/dialogue_integration.md](docs/dialogue_integration.md)
- UI Integration: [docs/ui_integration.md](docs/ui_integration.md)
- Data Model: [docs/data_model.md](docs/data_model.md)
