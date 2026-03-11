# ProjectEditorCore

Shared editor abstractions for decoupled plugin communication.

## Purpose

Provides delegates and interfaces that editor plugins can use without depending on each other directly. This follows the Dependency Inversion Principle (DIP) from SOLID.

## Delegates

### FDefinitionEvents::OnDefinitionRegenerated()

Broadcast when a definition asset (UObjectDefinition or other definition assets) is regenerated from JSON.

```cpp
#include "DefinitionEvents.h"

// Subscribe (in ProjectPlacementEditor)
FDefinitionEvents::OnDefinitionRegenerated().AddUObject(
    this, &UDefinitionActorSyncSubsystem::OnDefinitionRegenerated);

// Broadcast (in ProjectDefinitionGenerator)
FDefinitionEvents::OnDefinitionRegenerated().Broadcast(TypeName, Asset);
```

**Architecture:**

```
ProjectEditorCore (this plugin)
+-- FDefinitionEvents::OnDefinitionRegenerated()
         ^                    ^
         |                    |
ProjectDefinitionGenerator    ProjectPlacementEditor
(broadcasts)                  (subscribes)
```

## Dependencies

None - this is a leaf module that other editor plugins depend on.

## Adding New Delegates

1. Declare delegate type in a new header (e.g., `FOnSomethingHappened`)
2. Add static accessor in a class (pattern from `DefinitionEvents.h`)
3. Document in this README
4. Add to plugin dependencies in consuming modules
