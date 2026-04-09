# Moving UCLASS Between Modules

When a UCLASS moves from one module to another, placed actors in levels have the old `/Script/OldModule.ClassName` path serialized. UE cannot find the class at the old path and falls back to `AActor` (losing all components and data).

---

## Fix Procedure

1. Add temporary `ActiveClassRedirects` in `DefaultEngine.ini`:
   ```ini
   [/Script/Engine.Engine]
   +ActiveClassRedirects=(OldClassName="/Script/OldModule.MyClass",NewClassName="/Script/NewModule.MyClass")
   ```
2. Open editor -- actors load correctly via redirect
3. Make affected actors dirty (modify any property via MCP or editor)
4. Save (Ctrl+Shift+S) -- external actor files rewrite with new class path
5. Remove temp changes / restore original property values
6. Save again -- clean state
7. Remove the redirect lines from `DefaultEngine.ini`
8. Commit the resaved external actor files -- no redirects in repo

## Find Affected Actors

```bash
find Plugins/World -name "*.uasset" | xargs grep -l "OldModule.MyClass"
```

## Mark Actors Dirty via MCP

UE does not mark actors dirty just because a redirect resolved them. Force dirty by modifying a property:

```
ue-mcp inspect -> add_tag -> actorName=<name> -> tag=_resave_temp
(save, then remove tag, save again)
```

Or via blueprint-mcp, or by moving the actor 0.01 units and back.

## Policy

- Core Redirects are temporary migration tools, not permanent config
- Internal refactors: fix assets, remove redirects before merge to main
- Public API / released mod content: redirects may stay for backward compatibility
- ALIS is a public architecture reference -- committed config should show the clean state

## UE References

- [Core Redirects](https://dev.epicgames.com/documentation/en-us/unreal-engine/core-redirects-in-unreal-engine)
- [One File Per Actor](https://dev.epicgames.com/documentation/en-us/unreal-engine/one-file-per-actor-in-unreal-engine)
