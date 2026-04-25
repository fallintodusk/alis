// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MVVM/InventoryDragEvent.h"

class UInventoryUIDragHostSubsystem;

/**
 * Test-side observer for the inventory drag event stream. Subscribes
 * to Subsystem->OnDragEvent on construct, unsubscribes on destruct so
 * tests can treat it as a scoped fixture.
 *
 * Recorder partial-match semantics: AssertSequence compares each
 * expected event's Kind + any fields the expected entry explicitly
 * set (i.e. not the default sentinel). Fields left at their struct
 * defaults in the expected entry are skipped. This lets tests pin
 * only the axes they care about without having to duplicate all
 * session-level fields on every expected row.
 */
class FInventoryDragEventRecorder
{
public:
    explicit FInventoryDragEventRecorder(UInventoryUIDragHostSubsystem* InSubsystem);
    ~FInventoryDragEventRecorder();

    FInventoryDragEventRecorder(const FInventoryDragEventRecorder&) = delete;
    FInventoryDragEventRecorder& operator=(const FInventoryDragEventRecorder&) = delete;

    void Clear();
    const TArray<FInventoryDragEvent>& GetEvents() const { return Events; }

    /**
     * Assert the recorded stream contains the expected events in order
     * (partial-field matching described above). On mismatch emits a
     * SPECIFIC error naming the offending step so sabotage verification
     * produces a pinpoint failure message (not a generic "mismatch").
     */
    bool AssertSequence(FAutomationTestBase& Test, TArrayView<const FInventoryDragEvent> Expected) const;

    /**
     * Assert the exact number of recorded events. Emits a specific
     * error containing both counts on failure.
     */
    bool AssertEventCount(FAutomationTestBase& Test, int32 ExpectedCount) const;

private:
    TWeakObjectPtr<UInventoryUIDragHostSubsystem> Subsystem;
    FDelegateHandle Handle;
    TArray<FInventoryDragEvent> Events;
};
