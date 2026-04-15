# ProjectMind Data Model

## Responsibility

ProjectMind behavior should be data-driven:
- thought templates
- scan thresholds and timing
- cooldown and dedupe policy
- optional mode-specific policy knobs

## Data Principles

- keep player-facing text externalized and localization-ready
- keep balance/tuning values outside C++ where practical
- keep schema validation explicit and portable

## Data Placement Guidance

Use project data conventions:
- runtime schema data in project data schema paths
- include `$schema` in JSON data files
- resolve paths via project path helpers

## Dialogue Mapping Data

Dialogue integration should be data-driven and identifier-based.

Recommended placement:
- `Plugins/Gameplay/ProjectMind/Data/`

Recommended mapping fields:
- `signal_tag` (single dialogue signal key)
- `signal_tags` (many keys sharing one template)
- `ThoughtTag`
- `Priority`
- `CooldownSec`
- `ChannelPolicy`
- optional durable record lifecycle:
- `RecordId`
- `RecordState` (`Active` or `Resolved`)

Legacy fallback (supported, not preferred):
- `tree_id` + `node_id` are accepted and converted to `Dialogue.<TreeId>.<NodeId>`

Design rule:
- do not map by localized text
- keep mapping stable across localization changes

Cross-reference dependency:
- signal_tags reference dialogue tree IDs and node IDs from `DLG_*.json`
- renaming a tree `id` or node key without updating signal_tags causes silent failure
- validate after changes: `python scripts/ue/check/gameplay/projectmind/validate_data.py`
- pre-commit hook (`.githooks/pre-commit`) runs this automatically

## Idle Scan Rule Data

Idle scan/beacon guidance is data-driven:
- `Plugins/Gameplay/ProjectMind/Data/scan_thought_rules.json`

Rule fields:
- `rule_id`
- `match_any_tags`
- `distance_cm` (`min`, `max`)
- `min_view_dot`
- `min_screen_area_ratio` (approx on-screen footprint ratio, 0..1)
- `los_required`
- `thought_id_prefix`
- `text_template` (supports `{actor_label}`)
- `channel`
- `priority_base`
- `source_type`
- `time_to_live_sec`
- `cooldown_sec`
- `dedupe_window_sec`
- `dedupe_key_prefix`

Design split:
- C++ keeps perception math and runtime safety (camera source, FOV/LOS checks, scoring)
- JSON keeps content policy (what tags matter, what thought text/channel/priority to emit)

## Deferred Persistence

History save/load is final-stage and currently blocked.

Prepare data contracts now so persistence can be added without breaking runtime/UI contracts.
