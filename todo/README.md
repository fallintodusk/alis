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
  systems/      save/load, multiplayer, loading, networking
  tools/        build scripts, MCP, workflow, automation
  ui/           layout, panels, widgets, HUD
  world/        environment, foliage, landscape, vehicles, lighting
```

If a task does not fit any category -- create a new one in both
`01_done/` and `02_backlog/` to keep them mirrored.

## Naming

- Files: `topic_verb_noun.md` -- topic prefix groups related tasks together
  - `inventory_refactor_ui.md`, `inventory_implement_vision.md`
  - `mcp_inspect_cdo_followups.md`, `mcp_ue_blueprint_reader.md`
  - `dialogue_fix_grandpa.md`, `vitals_implement_panel.md`
- `00_` prefix reserved for meta/dashboard files only (`00_focus.md`, `00_release_1.0.0.md`)
- No generic `todo.md` names -- every file earns its own name

## Workflow

- Start task: move from `02_backlog/<category>/` to `00_current/`
- Finish task: move from `00_current/` to `01_done/<category>/`
- Cancel task: move from anywhere to `03_cancelled/`
- No index to maintain -- file names are the index

## Rules

- Only todo files link to docs, never docs to todo (one-way dependency)
- Plugin-scoped work stays in `Plugins/*/TODO.md`
- Cross-plugin or unowned work lives here
