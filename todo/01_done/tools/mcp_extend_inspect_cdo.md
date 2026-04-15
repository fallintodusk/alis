# Extend ue-mcp: inspect_cdo Action

**Repo:** G:\PublicRepos\Unreal_mcp (fork of ChiR24/Unreal_mcp)
**PR target:** ChiR24:main
**Motivation:** Cannot read Blueprint CDO property defaults (mesh assignments, AnimClass, BP variables) through MCP. Blocks modular Hero.json workflow where we need to know what skeleton BP_Hero sets on CharacterMesh0.

---

## Problem

| What works | What's missing |
|------------|----------------|
| `get_components` falls back to CDO -> returns name/class/transform | No property values (SkeletalMesh, AnimClass, etc.) |
| `get_component_property` reads any property via reflection | Only works on world actors, no CDO fallback |
| `inspect_object` returns component summary | No SkeletalMeshComponent support, no arbitrary props |

## Solution

Add `inspect_cdo` sub-action to the existing `inspect` tool. Uses `Blueprint->GeneratedClass->GetDefaultObject()` + existing `McpPropertyReflection` utilities.

## Files to Modify

**C++ plugin:**
- [ ] `plugins/McpAutomationBridge/.../McpAutomationBridge_EnvironmentHandlers.cpp` -- add inspect_cdo handler

**TypeScript server:**
- [ ] `src/tools/handlers/inspect-handlers.ts` -- add inspect_cdo case
- [ ] `src/tools/consolidated-tool-definitions.ts` -- add to action enum + new input props
- [ ] `src/types/handler-types.ts` -- add blueprintPath/detailed/propertyNames to InspectArgs

**Docs:**
- [ ] `CHANGELOG.md` -- under [Unreleased] / Added

## Key Reusable Infrastructure

- `LoadBlueprintAsset()` in McpAutomationBridgeHelpers.h:2860
- `ExportObjectToJson()` / `ExportPropertiesToJson()` in McpPropertyReflection.h
- `GetSkeletalMeshAsset()` compat pattern in AnimationHandlers.cpp:1972-1977
- `Blueprint->SimpleConstructionScript->GetAllNodes()` for SCS component hierarchy

## Design

**Modes:**
- Summary (default): BP-defined CDO properties only + component key fields (mesh, anim, transform)
- Detailed (`detailed: true`): full `ExportObjectToJson` on CDO + all component templates
- Component filter (`componentName`): single component full dump, skip CDO properties
- Property filter (`propertyNames`): selective export on both CDO and components

**Components:** SCS nodes (BP-added) + native CDO components (C++ inherited), deduplicated.

## Verification

- [ ] Plugin compiles with UE editor
- [ ] `npm run build` passes
- [ ] `inspect_cdo` on BP_Hero returns CharacterMesh0 SkeletalMesh path
- [ ] `inspect_cdo` with componentName filter returns single component
- [ ] `inspect_cdo` with invalid path returns clean error
- [ ] PR submitted to ChiR24/Unreal_mcp
