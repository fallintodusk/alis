# Add Player Thoughts

Vision source of truth moved to:
- `Plugins/Gameplay/ProjectMind/docs/vision.md`
- `Plugins/Gameplay/ProjectMind/docs/architecture.md`

Quick link:
- [ProjectMind Vision](../../Plugins/Gameplay/ProjectMind/docs/vision.md)
- [ProjectMind Architecture](../../Plugins/Gameplay/ProjectMind/docs/architecture.md)
- [ProjectMind Runtime](../../Plugins/Gameplay/ProjectMind/docs/runtime.md)
- [ProjectMind Dialogue Integration](../../Plugins/Gameplay/ProjectMind/docs/dialogue_integration.md)
- [ProjectMind UI Integration](../../Plugins/Gameplay/ProjectMind/docs/ui_integration.md)
- [ProjectMind Data Model](../../Plugins/Gameplay/ProjectMind/docs/data_model.md)

Implementation tracker (no tight coupling)

Phase 0 - Guardrails
- [x] Create gameplay plugin stub `ProjectMind`
- [x] Split high-level vision from technical architecture docs
- [x] Freeze dependency rule: ProjectMind runtime only depends on ProjectCore contracts (no direct runtime calls into ProjectDialogue UI/runtime classes)
- [x] Freeze integration rule: external plugins expose data via interfaces/services/delegates, ProjectMind consumes contracts only

Phase 1 - Core contracts (ProjectCore)
- [x] Add shared thought contracts in ProjectCore (initial `IMindService` + thought view)
- [x] Add `IMindThoughtSource`-style provider interface in ProjectCore for source adapters
- [x] Add `IMindThoughtFeed`-style consumer interface in ProjectCore for UI/view-model consumption
- [x] Extend `IDialogueService` with stable context accessors required by ProjectMind mapping:
- [x] `GetCurrentTreeId()`
- [x] `GetCurrentNodeId()`
- [x] `GetActiveDialogueOwner()` (or equivalent stable actor context)
- [x] Keep existing `OnDialogueStateChanged` as contract-level trigger (no ProjectMind subscription to dialogue component internals)

Phase 2 - External plugin support for interfaces
- [x] ProjectDialogue: implement new `IDialogueService` context accessors in `FDialogueServiceImpl`
- [x] ProjectDialogue: ensure context values are valid for watcher-started conversation paths (grandpa/door flow)
- [x] ProjectDialogue: add tests for tree/node context updates through node changes and conversation end
- [x] ProjectVitals: expose threshold-crossing signal through ProjectCore contract or delegate bridge (not direct ProjectMind call)
- [x] ProjectInventory: expose lightweight read-only query/signal contract needed by thought providers
- [ ] Optional: ProjectQuests (or current quest source) exposes objective/hint contract when explicit objective guidance is needed
- [x] For every external plugin adapter: if plugin/service is unavailable, fail soft (no crash, no hard dependency)

Phase 3 - ProjectMind runtime slice
- [x] Add local player mind runtime bootstrap owned by ProjectMind (GameInstance subsystem)
- [x] Add provider pipeline with strict contract boundaries (dialogue, vitals, nearby interactables, quest beacons)
- [x] Add deterministic aggregator (priority, dedupe, cooldown, global rate limit)
- [x] Add data-driven dialogue mapping table keyed by `TreeId + NodeId` (never by localized text)
- [x] Add initial mapping for `DLG_GrandPa_Door:greeting` -> high-priority water guidance thought
- [x] Keep all runtime output as thought feed events only (no widget calls)
- [x] Add idle scan flow: input pulse -> one-shot 3s idle timer -> single scan pass (no repeated scan timer)
- [x] Use existing world actor tags for scan hints (no new scan-specific world entities)
- [x] Implement default inventory thought source in ProjectMind only (consumable guidance + overweight hints via `IInventoryReadOnly`)

Phase 4 - UI integration via existing framework
- [x] ProjectUI/ProjectMindUI: add mind feed view-model consuming `IMindService`
- [x] Register top-right temporary thought presentation via `ProjectMindUI` definition on `HUD.Slot.MindThought`
- [x] Add journal panel entry (J key path) through existing menu/layer patterns
- [x] Mirror temporary feed events into journal history tab via view-model, not runtime logic

Phase 5 - Data and validation
- [x] Define JSON schema for ProjectMind mappings and policy under `Content/Data/Schema/Gameplay/ProjectMind/`
- [x] Require `$schema` in all ProjectMind data files
- [x] Add validation script/check coverage for ProjectMind data files
- [x] Move idle scan/beacon hint policy to data-driven rules (`scan_thought_rules.json`) while keeping camera/LOS math in C++

Phase 6 - Tests and verification
- [x] Unit tests for aggregator policy (priority, cooldown, dedupe, rate-limit)
- [x] Integration test: dialogue node change in `DLG_GrandPa_Door` produces mapped thought event
- [x] Integration test: missing ProjectDialogue service does not break ProjectMind runtime
- [x] Integration test: missing/invalid scan rule file falls back to defaults without runtime failure
- [x] Integration test: missing `IMindQuestThoughtSource` service fails soft (no crash, no hard dependency)
- [x] Integration test: UI view-model updates from feed without direct ProjectMind widget dependency
- [x] Smoke check in game: input pulse + 3s idle scan and dialogue hint behavior both work together (`scripts/ue/test/smoke/boot_test.bat` + `scripts/ue/test/test.bat --filter ProjectIntegrationTests.UI.HUD.MindThought`)

Blocked/Deferred
- [x] Keep quest/objective guidance provider deferred until content needs explicit objectives (no centralized quest controller now)
- [ ] Save/load of thought history (blocked by save system milestone)
