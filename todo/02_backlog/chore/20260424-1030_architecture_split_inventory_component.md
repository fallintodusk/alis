# Split ProjectInventory Component Responsibilities

Status: parked

## Current gap

`ProjectInventoryComponent.cpp` remains a first-party mega-file. Prior
extractions reduced it substantially, but equipment, queries, save behavior,
and client/server routing still share one translation unit.

## Investigation boundary

- Recount current responsibilities and callers before selecting a seam.
- Prefer callspace boundaries: client intent, network edge, server authority,
  pure mutation, and storage ownership.
- Preserve the existing world-container authority subsystem and local UI cache.
- Move RPCs only with measured or maintainability evidence and focused network
  regressions.

The active size guardrail and baseline live in
[canonical agent guidance](../../../docs/agents/canonical.md#10-mega-file-baseline--file-size-guardrail).
