# Implement Inventory Design Vision

This file now acts as a completion router, not a second behavior document.

Behavior SOT:
- `../../Plugins/Features/ProjectInventory/docs/design_vision.md`

Testing routes:
- `../../docs/testing/README.md`
- `../../docs/testing/unit_tests.md`
- `../../docs/testing/integration_tests.md`

Existing test modules:
- `../../Plugins/Foundation/ProjectCore/Source/ProjectCoreTests/`
- `../../Plugins/Features/ProjectInventory/Source/ProjectInventoryTests/`
- `../../Plugins/Test/ProjectIntegrationTests/`

Do not restate or fork inventory behavior here.

## Status Summary

| Feature | Status | Priority |
|---------|--------|----------|
| 2D Grid System | DONE | - |
| Item Rotation | DONE | - |
| Weight and Volume System | DONE | - |
| Container Grants | DONE | - |
| Placement Validation | DONE | - |
| AllowedTags Enforcement | DONE | - |
| Depth Stacking | DONE | - |
| Drag-time Rotation | DONE | - |

---

## Current State

No active code-vs-vision gaps remain from this checklist.
This checklist covers inventory vision items only, not the broader world-container / loot-place roadmap.

Recent completions:
- Depth stacking now uses `CellDepthUnits` on containers and `UnitsPerDepthUnit` on items, with shared rule math in `ProjectCore`.
- Drag-time rotation now updates the active drag operation footprint and preview through the existing inventory panel flow.

Validation routes used for closure:
- `ProjectCore.Types.InventoryStackRules.*`
- `ProjectIntegrationTests.InventoryLootPlaces.*`

---

## References

- Behavior SOT: `../../Plugins/Features/ProjectInventory/docs/design_vision.md`
- Testing router: `../../docs/testing/README.md`
- Inventory runtime: `../../Plugins/Features/ProjectInventory/Source/`
- Inventory UI: `../../Plugins/UI/ProjectInventoryUI/Source/`
