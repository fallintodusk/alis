# ProjectMind Vision

## Mental Model

ProjectMind is the player's internal voice.
It helps orientation and decision-making while preserving immersion.

The player has two layers:
- external player (real person controlling character)
- internal player (contextual helper voice)

The internal voice should feel natural, not mechanical.

## Player Experience Targets

- Observe the world through player camera context.
- Surface useful, short hints when relevant.
- Support deliberate scan behavior (hold input, focus, reveal).
- Show temporary hints and keep a journal history for review.
- Keep language concise, practical, and localization-ready.

## Presentation Model

- Top-right HUD feed shows short internal voice messages for all thought events
  (important and non-important).
- Each message is source-aware:
- Header confirms inner voice context.
- Meta line explains source (Dialogue, Vitals, Scan, Quest, POI).
- Body line is concise action-oriented thought text.
- Thought arbitration is priority-driven and context-first:
- If a useful object is observed in camera, this should be surfaced before a generic need reminder.
- Example: seeing bread/water first -> then hunger/thirst reminder if still relevant.
- Journal has two tabs with clear intent:
- `Important` for durable records that matter later.
- `Recent Log` for temporary activity recap.
- `J` opens the journal so player can reread prior top-right messages.
- `Important` is a long-lived guidance track (quest/dialogue/critical observations).
- `Recent Log` is a short-lived activity recap from temporary HUD messages.
- Important record lifecycle target:
- Records start active when first discovered.
- When resolved (for example, objective completed), record becomes inactive and moves lower in the list.

## Experience Principles

- Helpful, not noisy.
- Contextual, not omniscient.
- Modular, so new thought aspects can be added later.
- Respect existing ALIS UI and gameplay frameworks.

## Scope Boundary

This document is only high-level intent.

Detailed design lives in:
- `architecture.md`
- `runtime.md`
- `dialogue_integration.md`
- `ui_integration.md`
- `data_model.md`
