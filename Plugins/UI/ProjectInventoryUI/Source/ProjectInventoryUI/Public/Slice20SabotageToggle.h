// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

// ---------------------------------------------------------------------------
// Slice 20 architecture fitness sabotage toggle.
//
// DEFAULT: 0. Set to 1 to produce a deliberately-broken build that wires
// one pinpoint violation for each of the four architecture fitness tests.
// Used once per release to verify each fitness test is load-bearing
// (i.e. actually fails when its invariant is broken). Ship at 0.
//
// Red build expectation: four fitness tests fail with specific, named
// assertion messages. Green build expectation: all fitness tests pass.
//
// Sabotages wired:
//   - Fitness 1 (NoWidgetIncludesRouterOrDispatcher):
//       Adds a forbidden #include "MVVM/InventoryDropRouter.h" to
//       Plugins/UI/ProjectInventoryUI/.../Widgets/W_InventoryCellDropTarget.cpp
//   - Fitness 2 (OnlyDropTargetWidgetsOverrideDragHandlers):
//       Declares a no-op NativeOnDragOver override on UW_InventoryPanel
//       (which does NOT implement IInventoryDropTarget). The fitness test
//       must flag it.
//   - Fitness 3 (UserWidgetVisibilityContracted):
//       Flips UW_InventoryPanel's root Visibility contract to
//       SelfHitTestInvisible. Fitness test must catch the mismatch.
//   - Fitness 4 (EveryDragSessionEmitsCompletedOrCancelled):
//       InventoryUIDragHostSubsystem::CancelDrag short-circuits the
//       terminal Cancelled emit. Fuzz test must detect the missing
//       terminal.
//
// The toggle header is placed in ProjectInventoryUI/Public (not the test
// module) because it must be visible both to widget sources that carry
// the sabotage markers and to the test module's fitness test. Keeping
// it in the production plugin is acceptable because the macro has no
// runtime effect at its default value; the cost is a single #include
// in a handful of files.
// ---------------------------------------------------------------------------

#ifndef SLICE20_SABOTAGE
#define SLICE20_SABOTAGE 0
#endif
