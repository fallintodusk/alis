# ProjectSharedTypes Plugin

Shared cross-plugin primitives used across multiple plugins. No gameplay logic,
no AssetManager calls.

## Overview
- Purpose: small shared structs, enums, and pure stateless primitive helpers used
  by more than one plugin
- Scope: stable, low churn, no world or asset dependencies
- Non-goals: gameplay logic, systems, UI, or asset loading

## Architecture
- Foundation tier plugin
- Data-only types and pure primitive helpers that act as a shared kernel

## Dependencies
Engine Modules:
- Core
- CoreUObject
- GameplayTags

Project Plugins:
- None

No dependencies on:
- ProjectCore (interfaces)
- ProjectGAS (gameplay framework)

## Relationship to Other Plugins
- ProjectInventory depends on this for shared data types

## API Documentation
- `FMagnitudeOverride`: tag + value overrides for per-instance magnitudes
- `FInventoryGridGeometry`: pure rectangle overlap helper shared by inventory
  runtime and inventory UI

## Agent Implementation Guide
When adding a type here:
1. Verify it is used by 2+ plugins in different tiers.
2. Keep it data-only or pure stateless primitive logic (no systems, singletons,
   world access, gameplay orchestration, or asset loads).
3. Prefer small structs and enums with minimal includes.

## Testing
- No direct tests. Covered indirectly by feature/plugin tests that use these types.

## Documentation
- docs/architecture/plugin_rules.md
- docs/architecture/principles.md
