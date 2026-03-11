# ProjectMind Dialogue Integration

## Goal

ProjectMind must consume dialogue semantic signals as mind inputs.

Example:
- grandpa asks for water in dialogue
- ProjectMind emits an inner-voice hint with high priority

## Source Contract and Coupling

Primary source:
- `IDialogueService` from `ProjectCore`
- resolve through `FProjectServiceLocator`
- subscribe to `OnDialogueSignal()`

Design rule:
- do not depend on `UProjectDialogueComponent` internals in ProjectMind runtime path
- keep dependency at ProjectCore interface level

## Dialogue Signal Contract

Runtime contract:
- dialogue provider emits stable `SignalTag` values (example: `Dialogue.DLG_GrandPa_Door.greeting`)
- dialogue provider also emits option-selection signals (example: `Dialogue.Option.DLG_GrandPa_Door.greeting.Next.give_water`)
- mind runtime maps signal tags to thought templates
- signal tags are logic keys, not localized text

Hard rule:
- do not use localized dialogue text as primary logic key

## Data-Driven Mapping

Dialogue to thought mapping should live in plugin data:
- `Content/Data/Schema/Gameplay/ProjectMind/`

Recommended mapping key:
- `signal_tag` (single signal)
- or `signal_tags` (multiple signals sharing one template)

Recommended mapping value:
- `ThoughtTag`
- `Priority`
- `CooldownSec`
- `ChannelPolicy` (toast, journal, both)
- optional durable record lifecycle fields:
- `RecordId` (stable key for important journal record)
- `RecordState` (`Active` or `Resolved`)

Example intent:
- `Dialogue.DLG_GrandPa_Door.greeting` -> `Mind.Dialogue.Grandpa.NeedsWater` (P1)
- `Dialogue.DLG_GrandPa_Door.thanks` -> same `RecordId` with `RecordState=Resolved`

Legacy compatibility:
- old `tree_id + node_id` entries are still accepted as fallback
- new content should use `signal_tag` or `signal_tags`

## Priority Posture

Dialogue-driven thoughts use the same aggregator as all other sources.

Suggested baseline:
- P1: critical request from active dialogue (example: urgent water need)
- P2: objective reinforcement after dialogue progression
- P3: ambient flavor hints (journal-only by default)

All dialogue thoughts pass through:
- per-tag cooldown
- dedupe window
- global rate limit

Important journal lifecycle:
- emit `RecordState=Active` when request/observation becomes relevant
- emit `RecordState=Resolved` on completion node
- UI keeps latest state per `RecordId`, and resolved records are shown below active ones

## Multiplayer and Client Side

Current baseline:
- mind evaluation and display is client-side for local player

Future stricter multiplayer mode:
- keep same signal contract
- tighten policy rules only (visibility and confidence), not integration topology
