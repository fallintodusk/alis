# ProjectCore Interfaces - Abstraction Layer

**Purpose:** Define stable interfaces for cross-module communication to satisfy the **Dependency Inversion Principle (DIP)**.

## SOLID Principle: Dependency Inversion

> High-level modules should not depend on low-level modules.
> Both should depend on abstractions.

## Architecture Pattern

```
+----------------------------------------+
| High-Level: Game Systems               |
| (ProjectLoading, ProjectSession)       |
+-------------------+--------------------+
                    | depends on
                    v
+-------------------+--------------------+
| Abstraction: ProjectCore/Interfaces/   |  <- THIS DIRECTORY
| (IOrchestratorRegistry, etc.)          |
+-------------------+--------------------+
                    ^
                    | implements
+-------------------+--------------------+
| Low-Level: Boot Infrastructure         |
| (OrchestratorCore, BootROM)            |
+----------------------------------------+
```

## Rules for This Directory

1. **Pure interfaces only** - No implementation code
2. **No concrete dependencies** - Only depend on UE Core types
3. **Stable contracts** - Changes here affect many modules
4. **PROJECTCORE_API export** - All interfaces must be exported

## Example: IOrchestratorRegistry

**Problem (DIP Violation):**
```cpp
// ProjectLoading.Build.cs
PrivateDependencyModuleNames.Add("OrchestratorCore");  // [X] High -> Low dependency
```

**Solution (DIP Compliant):**
```cpp
// Interfaces/IOrchestratorRegistry.h - Define abstraction here
class PROJECTCORE_API IOrchestratorRegistry { /* ... */ };

// ProjectLoading depends on ProjectCore (abstraction)
// OrchestratorCore depends on ProjectCore (abstraction)
// OrchestratorCore implements and registers the interface
```

## Adding New Interfaces

1. Create interface in `Interfaces/I<Name>.h`
2. Use pure virtual methods (`= 0`)
3. Export with `PROJECTCORE_API`
4. Provide service locator accessors if needed
5. Document which modules implement vs consume

## Current Interfaces

- **IOrchestratorRegistry** - Query loaded feature plugins (implemented by OrchestratorCore, consumed by game systems)
- **IProjectFeatureModule** - Feature plugin lifecycle contract
- **IActorWatchEventListener** - Generic watch-event listener contract for capability-driven actor observation
- **IWorldContainerSessionSource** - Thin world-side contract for meaningful storage containers
- **IInventoryWorldContainerTransferBridge** - Inventory-side transfer/session bridge for nearby world containers
- **IMindThoughtFeed / IMindService** - Mind feed consumer/runtime contracts
- **IMindThoughtSource** - Mind source adapter contracts (default implementations in ProjectMind, optional overrides by gameplay plugins)
  - Guardrail: keep mind orchestration in ProjectMind, not in domain feature modules
- **ProjectLoadingHandle** - Async loading operation handle

---

**Key Takeaway:** This directory exists to decouple high-level game logic from low-level infrastructure.
Never bypass these abstractions by adding direct dependencies between layers.
