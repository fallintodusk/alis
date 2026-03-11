# UE5 Source Build - Compile Time Reality

Technical documentation for building from Unreal Engine source.

---

## The Problem: Build.cs Changes Trigger Full Engine Rebuild

When modifying a plugin's `Build.cs` file (e.g., adding/removing module dependencies), UBT rebuilds **3000+ files** instead of just the affected module.

### Root Cause

1. **Makefile Invalidation**: Any `Build.cs` modification invalidates UBT's cached makefile
2. **Response File Regeneration**: ALL module `.rsp` files are regenerated
3. **Include Path Instability**: Regenerated `.rsp` files have slightly different include paths
4. **Cascade Effect**: Different `.rsp` = different compiler flags = full recompilation

**Evidence** (from UBT log):
```
Invalidating makefile for AlisEditor (ProjectObject.Build.cs modified)
Updating .../SunPosition/SunPosition.Shared.rsp: contents have changed
Updating .../GameplayAbilities/GameplayAbilities.Shared.rsp: contents have changed
[... hundreds more engine modules ...]
```

The actual difference in `.rsp` files:
```diff
< /I "../Intermediate/Build/Win64/x64/UnrealEditor/Development/UnrealEd"
```

A single include path change causes the entire module to recompile.

---

## Reality: No Quick Fix for Source Builds

### What DOESN'T Work

| Approach | Why It Fails |
|----------|--------------|
| `bUsePrecompiled` in Target.cs | Property doesn't exist in UE5.7 TargetRules |
| `<bUsePrecompiled>` in XML | Not supported - command-line only |
| `-UsePrecompiled` flag | Requires static `.lib` files that source builds don't produce |

**Key insight:** `-UsePrecompiled` requires precompiled static libraries that only exist in:
- Epic Games Launcher installed builds
- Output from `InstalledEngineBuild.xml`

Source builds compile engine as **DLLs**, not static libs. No static libs = flag does nothing.

---

## Actual Solutions

### Option 1: Accept Full Rebuilds (Simplest)

For source builds, full rebuilds on Build.cs changes are **by design**.

**Mitigation:**
- Batch Build.cs changes - add multiple dependencies at once
- Plan dependencies upfront before starting work
- Use forward declarations to minimize new dependencies

### Option 2: Create InstalledEngineBuild (Best for Mixed Workflow)

One-time investment to create precompiled engine:

```powershell
cd <ue-path>
.\Engine\Build\BatchFiles\RunUAT.bat BuildGraph -Script="Engine/Build/InstalledEngineBuild.xml" -Target="Make Installed Build Win64" -Set:WithDDC=false
```

**Time:** 4-12+ hours
**Disk:** ~100-200GB during build
**Result:** Precompiled engine with static libs

After completion:
- Point project to new engine location
- `-UsePrecompiled` flag will work
- Build.cs changes only rebuild game modules

### Option 3: Use Epic Launcher Build for Development

Use installed build from Epic Launcher for daily work, switch to source only when modifying engine.

---

## BuildConfiguration.xml (Minor Optimizations)

These don't prevent full rebuilds but may speed them up slightly.

Create `<user-home>\Documents\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
  <BuildConfiguration>
    <!-- Enable makefile caching -->
    <bUseUBTMakefiles>true</bUseUBTMakefiles>

    <!-- Use all CPU cores -->
    <bAllCores>true</bAllCores>
    <MaxParallelActions>32</MaxParallelActions>

    <!-- Skip outdated import library checks -->
    <bIgnoreOutdatedImportLibraries>true</bIgnoreOutdatedImportLibraries>
  </BuildConfiguration>

  <TargetRules>
    <!-- Force PCH for game modules -->
    <bForcePrecompiledHeaderForGameModules>true</bForcePrecompiledHeaderForGameModules>
  </TargetRules>
</Configuration>
```

---

## Best Practices for Source Builds

### Minimize Build.cs Changes

1. **Batch dependency changes**: Add multiple modules at once, not one-by-one
2. **Plan dependencies upfront**: Review what modules you'll need before starting work
3. **Use forward declarations**: Avoid adding dependencies for types only used in headers

### When Full Rebuild is Unavoidable

```powershell
# Just let it complete - don't cancel
Build.bat AlisEditor Win64 Development -project="<project-root>\Alis.uproject"
```

Future incremental builds will be fast again until next Build.cs change.

---

## UBT Makefile System

### How It Works

1. **First Build**: UBT creates makefile with all build actions, dependencies, response files
2. **Incremental Build**: UBT loads cached makefile, checks file timestamps
3. **Invalidation**: If any tracked file changes (Build.cs, Target.cs, etc.), makefile is regenerated

### What Invalidates Makefile

| File Type | Invalidates? | Impact |
|-----------|--------------|--------|
| `.cpp` / `.h` | No | Only that file recompiles |
| `Build.cs` | **Yes** | Full makefile regeneration |
| `Target.cs` | **Yes** | Full makefile regeneration |
| `.uplugin` | **Yes** | Full makefile regeneration |

---

## Quick Reference

| Goal | Solution |
|------|----------|
| Fast plugin iteration | Use Epic Launcher build OR create InstalledEngineBuild |
| Source access + fast builds | InstalledEngineBuild (one-time 4-12hr investment) |
| Pure source build | Accept full rebuilds, batch Build.cs changes |

---

## Related Links

- [Build Workflow](../build/workflow.md)
- [Build Commands](../build/README.md)
- [UE5.7 Build Configuration](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-configuration-for-unreal-engine)

---

**Last Updated:** 2026-01-22
**Category:** UE Engine Internals
