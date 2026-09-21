# Task Tracker

Tasks record active, queued, externally blocked, completed, or cancelled work.
They are not architecture authorities.

## Structure

```text
00_current/  active work, flat
01_done/     completed work, grouped by owning domain
02_backlog/  actionable future work, grouped by owning domain
03_parked/   started work blocked by an external prerequisite, grouped by owning domain
04_cancelled/ cancelled work, grouped by owning domain
```

Completed and cancelled task files remain in their lifecycle section as the
reviewable execution record. Still-current facts must first move to their
durable owner; task files never become architecture authorities.

## Ownership

- Keep one task per unresolved problem in this central tree.
- Do not create plugin-local `TODO.md` files.
- Stable code and documentation must not link to tasks.
- A task may link to stable owners for evidence; it must not restate their
  contracts.
- Do not build task-to-task dependency graphs. Merge duplicates or name the
  single owner in prose.

## Naming

New tasks use `YYYYMMDD-HHMM_topic_verb_noun.md`. Preserve the creation
timestamp when moving between current, backlog, and parked. Existing tasks keep
their current names until materially re-investigated; rewritten tasks receive
a timestamp.

## Lifecycle

- Start: move one task from its backlog category into `00_current/`.
- Resume: move a parked task into `00_current/` only after its named blocker is
  cleared.
- Defer: move actionable but unselected work from current to its owning
  `02_backlog/<domain>/` category.
- Park: move started work that cannot proceed until a named external condition
  changes to `03_parked/<domain>/`.
- Finish: migrate durable facts, record the outcome, then move the task to
  `01_done/<domain>/`.
- Cancel: migrate any durable facts, record why work stopped, then move the
  task to `04_cancelled/<domain>/`.
- Keep only one numbered slice active for a multi-slice initiative.

File names and directories are the index; no second dashboard is maintained.
