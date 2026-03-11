# ProjectMind Boundaries and SOLID Guardrails

Purpose
- Prevent architecture drift and SOLID regressions when extending mind systems.
- Keep ownership clear: ProjectMind orchestrates thoughts, domain plugins expose domain state.

## 1. No Centralized Quest Controller Requirement

ProjectMind does not require a centralized quest controller plugin.

Allowed guidance model:
- POI/beacon-driven hints from world context
- dialogue context hints
- vitals/inventory condition hints
- optional objective/quest hints when a quest source exists

Rule:
- `IMindQuestThoughtSource` is optional.
- Missing quest source must be fail-soft (no crash, no hard dependency).

## 2. SOLID Ownership Rules

Single Responsibility:
- ProjectMind owns thought aggregation, prioritization, dedupe, cooldown, and feed emission.
- Domain plugins own only their domain logic (inventory, vitals, dialogue, quests, etc.).

Dependency Inversion:
- Cross-plugin communication happens only through `ProjectCore` interfaces.
- No direct runtime calls from ProjectMind to feature internals.

Open/Closed:
- Extend thought behavior by adding providers/data in ProjectMind.
- Do not modify domain plugin internals to add mind orchestration.

## 3. Hard Prohibitions

Do not do:
- add `IMind*ThoughtSource` implementation inside `ProjectInventory`, `ProjectVitals`, or other domain modules
- register mind services from non-mind modules
- move thought JSON mapping parsing into domain plugins
- let domain plugins emit UI thoughts directly

## 4. Allowed Extension Paths

Preferred:
- implement default sources inside ProjectMind
- use domain read-only interfaces/events from ProjectCore
- keep mappings/data in `Content/Data/Schema/Gameplay/ProjectMind/`

Optional advanced path:
- if a custom source override is needed, place it in a dedicated integration module/plugin
- keep domain module code unchanged and mind-agnostic

## 5. Agent Checklist Before Merge

- Is thought orchestration code only in ProjectMind?
- Are non-mind plugins free of `IMind*ThoughtSource` implementations?
- Is missing optional source handled fail-soft?
- Are new dependencies pointing to ProjectCore abstractions, not concrete feature internals?
- Are player-facing texts and mappings still data-driven under ProjectMind schema paths?
