# ProjectMind Runtime

## Responsibility

ProjectMind runtime owns:
- thought signal generation
- scan timing/state
- prioritization and dedupe
- publication to consumers via abstractions

ProjectMind runtime does not own:
- widget creation
- viewport/layout concerns
- direct UI calls

## Bootstrap Ownership

Current ownership:
- `UProjectMindRuntimeBootstrapSubsystem` (GameInstance subsystem) bootstraps local pawn context.
- subsystem resolves primary local pawn and updates `IMindRuntimeControl`.
- no runtime coupling from `ProjectSinglePlayClient` to `ProjectMind`.

## Runtime Building Blocks

- local context bootstrap subsystem (client-side pawn resolve)
- thought provider pipeline (modular sources via ProjectCore contracts)
- aggregator policy (cooldown, dedupe, priority)
- publish stream via core contracts
- dialogue signal listener via `IDialogueService`
- idle scan scheduler (one-shot 3s callback, re-armed by input activity)

## Runtime Rules

- no direct dependency on UI widgets
- no direct calls to ProjectUI subsystems from runtime generation path
- keep provider extensions additive and isolated
- do not depend on dialogue feature internals when consuming dialogue state
- non-dialogue thoughts come from source contracts:
  - `IMindVitalsThoughtSource`
  - `IMindInventoryThoughtSource`
  - `IMindQuestThoughtSource`
- default source implementations are owned by ProjectMind runtime
- `IMindQuestThoughtSource` is optional and may be absent
- external plugins may override a source contract, but should stay domain-only
  (no thought orchestration in non-mind modules)
- non-mind modules must not register mind sources/services from their StartupModule
- default inventory source uses `IInventoryReadOnly` + existing state tags to emit
  consumable guidance (hydration/calories) and overweight hints
- scene scan uses existing actor tags only (`World.*`, `Item.*`, `Scenario.*`)
- scan/beacon hint policy is data-driven from
  `Content/Data/Schema/Gameplay/ProjectMind/scan_thought_rules.json`
- do not require new world actors/components to support scan hints
- no repeated scan polling loop; scan runs once after idle timeout and waits for next input pulse

Boundary reference:
- see `boundaries.md` for merge-time guardrails

## Dialogue Signal Path

Runtime flow:
- resolve `IDialogueService` from service locator
- subscribe to `OnDialogueSignal()`
- evaluate dialogue signal tag against data-driven mappings
- emit ranked thought candidates into common aggregator

Behavior rule:
- dialogue-driven hints share cooldown/dedupe/rate-limit with all other thought sources

## Idle Scan Path

Runtime flow:
- local input pulse -> `IMindRuntimeControl::NotifyPlayerInputActivity()`
- runtime invalidates previous idle timer
- runtime starts one-shot 3 second timer
- timer callback triggers one scan pass
- scan pass evaluates existing world tags with camera/FOV/LOS checks plus on-screen footprint checks and emits ranked candidates
- distance is secondary; on-screen footprint is the primary visibility signal for far large landmarks
- camera and LOS math stays in C++; rule matching and thought templates come from scan rule data
- LOS-required scan rules are strict and do not pass through occluders
- scan resource/interactable hints remain temporary (`Toast`) unless rules explicitly set durable channel

Performance rule:
- no per-frame scan tick
- no repeating scan timer
- input activity must re-arm idle timer explicitly
