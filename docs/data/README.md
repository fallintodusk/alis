# Data Architecture

This section documents the **External, Text-Based Data Architecture** (JSON, INI) used for game tuning and configuration. It focuses on agent-editable files that do not require binary cooking or editor intervention.

## Core Documents

- **[Agent Workflow](workflow.md)** (The *How* & *Why*)
  - **Audience:** Designers, Agents tuning gameplay.
  - **Topics:** Iteration patterns (INI vs JSON vs CVAR), zero-restart workflows, "hot" reloading.
  - **Use when:** You want to know *how* to balance the game without recompiling.

- **[Data Specs & Layout](structure.md)** (The *Where* & *What*)
  - **Audience:** Engineers, Build System.
  - **Topics:** File paths (`Plugins/.../Data/`), packaging rules (`Build.cs` dependencies), C++ path resolution APIs.
  - **Use when:** You are implementing a new subsystem and need to know *where* to load files from.

## Key Principles

1.  **Keep it Text-Based**: Design data should be JSON/INI, not binary `.uasset`, to allow external tools and agents to edit it.
2.  **Plugin-Centric**: Data lives WITH the plugin that owns it, not in a global `/Game/Data` folder.
3.  **Runtime-Reloadable**: Systems must support `ReloadConfig` or file watchers to apply changes instantly.

## Data Pipeline Architecture

The project implements a **decentralized data pipeline** with three distinct flows, each owned by specialized plugins.

### Flow Terminology

| Term | What It Means | Owner Plugin | Example |
|------|---------------|--------------|---------|
| **SYNC** | Mechanical updates (bidirectional or reactive) | [ProjectAssetSync](../../Plugins/Editor/ProjectAssetSync/README.md) | UE Asset renamed -> JSON path updated, redirectors cleaned |
| **GENERATION** | One-way transformation with validation | [ProjectDefinitionGenerator](../../Plugins/Editor/ProjectDefinitionGenerator/README.md) | JSON file changed -> DataAsset regenerated |
| **PROPAGATION** | Cascading dependency updates | [ProjectPlacementEditor](../../Plugins/Editor/ProjectPlacementEditor/README.md) | Definition regenerated -> Scene actors updated |

**Why SYNC includes auto-fix redirectors:**
Epic's default workflow keeps redirectors indefinitely to protect complex branching scenarios. This project's workflow eliminates that need:
- **World Partition** = isolated actors (no deep cross-package deps)
- **Binary merge** = always "take incoming" on conflict
- **Data-driven** = most refs via JSON (already auto-synced)
- **Result**: Redirectors serve no purpose, auto-cleanup keeps content clean

### Architecture Principle

**"Orchestration where generic, ownership where specific"**

Each plugin owns its business logic. Utilities are extracted to shared plugins only when proven generic (3+ consumers).

```
ProjectAssetSync (cross-cutting sync utilities)
    |
    +-- JsonRefSync: Asset rename -> JSON path fix (GENERIC)
    +-- RedirectorAutoFix: Asset rename -> Auto-cleanup redirectors (GENERIC)
    +-- [Future]: Asset metadata sync, config sync, etc.

ProjectEditorCore (event bus - minimal)
    |
    +-- FDefinitionEvents (multicast delegates)

ProjectDefinitionGenerator (owns GENERATION logic)
    |
    +-- File watcher (JSON changes)
    +-- JSON -> DataAsset generation
    +-- FDefinitionValidator
    +-- Broadcasts: OnDefinitionRegenerated

ProjectPlacementEditor (owns PROPAGATION logic)
    |
    +-- Subscribes: OnDefinitionRegenerated
    +-- Definition -> Actor updates (Reapply/Replace strategies)
```

### Complete Data Flow Example

**User renames asset in Content Browser:**
1. **SYNC**: [ProjectAssetSync](../../Plugins/Editor/ProjectAssetSync/README.md) detects asset rename:
   - Updates JSON paths (JsonRefSync)
   - Fixes redirectors and saves packages (RedirectorAutoFix)
2. **GENERATION**: [ProjectDefinitionGenerator](../../Plugins/Editor/ProjectDefinitionGenerator/README.md) detects JSON change, regenerates DataAsset
3. **PROPAGATION**: [ProjectPlacementEditor](../../Plugins/Editor/ProjectPlacementEditor/README.md) detects definition change, updates scene actors

### Data-Driven Plugins

| Plugin | Data Type | Schema Location | Purpose |
|--------|-----------|-----------------|---------|
| [ProjectObject](../../Plugins/Resources/ProjectObject/README.md) | Object definitions (JSON) | `ProjectObject/Data/Schemas/` | Composed actors (doors, furniture, interactive elements, inventory items) |

**See individual plugin READMEs for detailed implementation.**

Note: Inventory items are defined in ObjectDefinition.Item (ProjectObject). There is no separate items plugin in this repo.

## Related Routers

- **[Architecture Router](../architecture/README.md)** - Where data fits in the overall plugin structure.
- **[UI Router](../ui/README.md)** - JSON-first UI and descriptor-driven rendering.
