// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UInventoryUIDragHostSubsystem;

/**
 * Optional Verbose-level logger that subscribes to the drag event
 * multicast and writes one line per event to LogInventoryDragHost.
 * Gated by CVar `inv.drag.log` (int, default 0) for zero cost when
 * disabled. Compiled out in Shipping.
 *
 * Install via FInventoryDragLogger::Install(Subsystem) where the
 * subsystem is available (e.g. when a widget first resolves its
 * drag host). Live installations survive with the subsystem's
 * lifetime; the logger holds a weak reference to the subsystem
 * and a delegate handle to detach on subsystem destruction.
 */
#if !UE_BUILD_SHIPPING
class FInventoryDragLogger
{
public:
    /**
     * Attach the logger to the supplied subsystem. No-op if the CVar
     * `inv.drag.log` is zero at call time. Safe to call multiple times
     * for the same subsystem - subsequent calls re-bind so toggling the
     * CVar between sessions has the expected effect.
     */
    static void Install(UInventoryUIDragHostSubsystem* Subsystem);
};
#endif
