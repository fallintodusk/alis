# Task Tracker

## Structure

```
00_current/      active work (flat, no subfolders)
01_done/         completed (sorted by category)
02_backlog/      parked work, not scheduled (sorted by category)
03_cancelled/    superseded, abandoned, no longer relevant
```

## Categories

Categories are consistent between `01_done/` and `02_backlog/`.
When finishing or starting a task, keep the same category subfolder.

```
  chore/        docs cleanup, tech debt, housekeeping
  content/      Content/ folder structure, assets, materials, packs
  debug/        investigating issues, regressions, crash analysis
  diagnostics/  FPS counter, dev overlay, in-game monitoring
  editor/       thumbnails, asset pickers, editor UX
  gameplay/     character, combat, dialogue, interaction, vitals
  rendering/    anti-aliasing, post-process, TSR, visual artifacts
  systems/      save/load, multiplayer, loading, networking
  test/         packaged build reports, QA, test results
  tools/        build scripts, MCP, workflow, automation
  ui/           layout, panels, widgets, HUD
  world/        environment, foliage, landscape, vehicles, lighting
```

If a task does not fit any category -- create a new one in both
`01_done/` and `02_backlog/` to keep them mirrored.

## Naming

- Every newly created task todo starts with its creation timestamp:
  `YYYYMMDD-HHMM_<descriptive_name>.md`, for example
  `20260827-1936_world_stabilize_kazan_release.md`. Preserve that timestamp
  forever when moving the todo between `00_current`, `02_backlog`, and
  `01_done`; edits do not change it. Do not rename existing todos merely to add
  timestamps. Router/control files such as `00_focus.md` are exempt.
- After the timestamp, use `topic_verb_noun` so the topic prefix groups related
  tasks together.
- `00_` prefix reserved for meta/dashboard files only (`00_focus.md`, `00_release_1.0.0.md`)
- No generic `todo.md` names -- every file earns its own name

## Workflow

- Start task: move from `02_backlog/<category>/` to `00_current/`
- Finish task: move from `00_current/` to `01_done/<category>/`
- Cancel task: move from anywhere to `03_cancelled/`
- No index to maintain -- file names are the index
- Multi-slice initiatives keep exactly one clearly numbered slice file active.
  On acceptance, move that file to its done category before creating the next.

## Rules

- Only todo files link to docs, never docs to todo (one-way dependency)
- Plugin-scoped work stays in `Plugins/*/TODO.md`
- Cross-plugin or unowned work lives here
