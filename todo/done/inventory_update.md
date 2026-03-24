# Inventory Update Status (2026-01-28)

## Status: Core Complete, Design Vision Gaps Identified

Core inventory system is implemented and functional. Design vision doc created - gaps identified for realistic survival mechanics.

## Completed Work
- Core pickup/loot/equip/use flow
- Containers + grid data model with Server_MoveItem validation
- UI: drag/drop, container tabs, equip slots, command bar
- Drop quantity on spawned pickup
- Container grants from equipment (item-driven + slot fallback)
- Persistence integration (auto-load/save with ProjectSaveSubsystem)
- SOLID refactoring of inventory UI (7 focused files from 2 monolithic)
- Design vision doc: docs/design_vision.md

## Pending Work (Design Vision Gaps)

### Weight System Enhancement
Current: Hard limit (MaxWeight blocks pickup).
Target: Keep hard limit as safety + add soft penalties.

1. ~~**Soft weight penalties (GAS integration)**~~ ✅ DONE
   - State.Weight.* tags (Light/Medium/Heavy/Overweight) set by InventoryComponent
   - Heavy (80-100%): StaminaRegenRate -15%, MoveSpeed -15%
   - Overweight (>100%): StaminaRegenRate -30%, MoveSpeed -45%
   - GE_ThresholdDebuff_WeightHeavy/Overweight in ProjectVitals
   - VitalsComponent applies/removes debuffs based on weight tags
   - ProjectCharacter checks weight tags for movement speed multiplier

2. ~~**Dynamic capacity from GAS state**~~ ✅ DONE
   - GetCapacityMultiplier() checks ASC for State.Fatigue.*/State.Condition.* tags
   - Healthy/Rested=100%, Tired/Low=80%, Exhausted/Critical=60%, Critical/Empty=40%
   - GetMaxWeight() applies multiplier to base capacity
   - Periodic timer (1s) refreshes weight state to catch capacity changes

### UI Feedback

3. ~~**Overweight state tags + ViewModel**~~ ✅ DONE (foundation)
   - Added State.Weight.* tags to ProjectGameplayTags (Light/Medium/Heavy/Overweight)
   - Added EWeightState enum and UpdateWeightStateTag() to inventory component
   - Updates ASC tags on inventory change (OnEntryAdded/Removed/Changed)
   - Added GetWeightRatio() and IsOverweight() to InventoryViewModel
   - HUD can query state tag or ViewModel to display weight status

4. ~~**Inventory error feedback delegate + Toast System**~~ ✅ DONE
   - Added `FOnInventoryError` delegate and `BroadcastError()` helper
   - Unequip blocked: "Cannot unequip - pockets not empty"
   - Weight exceeded: "Too heavy - cannot carry more"
   - Volume exceeded: "No space - inventory full"
   - Toast notification system (ProjectToastSubsystem + W_ToastNotification)
   - IInventoryReadOnly extended with OnInventoryErrorNative() delegate
   - InventoryViewModel subscribes and forwards errors
   - W_InventoryPanel shows toast on error via ProjectToastSubsystem

5. ~~**Capacity bar per container**~~ ✅ DONE
   - FInventoryContainerView now has CurrentWeight/CurrentVolume
   - InventoryViewModel exposes ContainerCurrentWeight/Max, ContainerCurrentVolume/Max
   - UI stats show selected container's fill % (e.g., "2.5/5.0 kg (50%)")

### Hand Slots

6. ~~**Hand quick-swap hotkey**~~ ✅ DONE
   - Added IA_SwapHands input action (X key) to SinglePlayerPlayerController
   - Added Server_SwapHands RPC to ProjectInventoryComponent
   - Added RequestSwapHands to IInventoryCommands interface
   - Swaps items between hand slot 0 and slot 1

7. ~~**Hands accept any item size**~~ ✅ DONE
   - Added `bSlotBased` flag to FInventoryContainerConfig
   - Hands container uses slot-based logic (ignores item grid size)
   - IsRectWithinContainer, DoesRectOverlap, FindFreeGridPos updated for slot-based mode
   - FInventoryContainerView exposes bSlotBased for UI

### Optional Polish
- ~~**Richer tooltips (durability, stats, attachments)**~~ ✅ DONE
  - FInventoryEntryView extended with Durability, MaxDurability, Ammo, Modifiers
  - W_ItemTooltip widget with name, description, weight/volume, durability bar, ammo, modifiers
  - ItemTooltipLayout.json for layout
- Grid art assets (no new art this pass)
- Sound feedback (pickup, drop, equip, overweight warning) (no new sounds this pass)

## Architecture Docs
- Design vision: Plugins/Features/ProjectInventory/docs/design_vision.md
- Technical architecture: Plugins/Features/ProjectInventory/docs/architecture.md
- Inventory UI: Plugins/UI/ProjectInventoryUI/docs/architecture.md
- MVVM pattern: Plugins/UI/ProjectUI/docs/ui_mvvm.md

## Key Decisions (Locked)
- Unequip overflow policy: block unequip if granted containers are not empty
- Container grants policy: item-driven grants are primary; slot grants are fallback only
- Weight policy: hard limit stays as safety, soft penalties added on top
- Hands: 2 slots (not grid-constrained), accept any item size, separate from pockets
- Container restrictions: cell-based only (no tag restrictions for now)
- Backpack access: can access contents without removing (no "take off" mechanic)
- Backpack nesting: NOT allowed (no backpacks inside backpacks - prevents recursion exploits)
- Item rotation: 2x3 item CAN fit in 4x2 container if rotated to 3x2
