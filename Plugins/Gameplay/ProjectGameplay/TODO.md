# ProjectGameplay TODO

## Current State: Framework Complete

Contracts in `ProjectFeature`. This plugin provides gameplay framework (future base classes).

---

## Design Decision: Registration vs Initialization

```
REGISTRATION (module startup)          INITIALIZATION (GameMode controlled)
---------------------------------      ------------------------------------
- Happens when UE loads module         - Happens when GameMode decides
- Unordered (race conditions)          - Ordered by FeatureNames array
- Just stores lambda in registry       - Actually runs the lambda
- No pawn exists yet                   - Pawn exists, passed in context
```

**GameMode controls:** ORDER, TIMING, SELECTION
**Features own:** WHAT to attach, HOW to configure

---

## Dependency Structure

```
ProjectFeature (contracts only)
    ^
    |
+---+-------------------+
|                       |
Feature plugins     ProjectGameplay (this)
(Combat, etc.)              ^
                            |
                    ProjectSinglePlay
```

---

## See Also

- `Plugins/Gameplay/ProjectFeature/README.md` - Contract design
- `Plugins/Gameplay/ProjectSinglePlay/README.md` - Orchestrator
