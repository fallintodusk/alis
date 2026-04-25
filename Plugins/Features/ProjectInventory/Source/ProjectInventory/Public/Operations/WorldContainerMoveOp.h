// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Pure-logic operation: move an entry within a world-container session.
 *
 * Consume-first + store-at-target + rollback on store failure. No UObject
 * ownership, no network code, no UI. Callable from any authority-side
 * service -- today the session subsystem, long-term a server-gated
 * UWorldSubsystem (authority callspace layer per the FILE SIZE GUARDRAIL
 * + callspace split in AGENTS.md CRITICAL Rules).
 */
struct PROJECTINVENTORY_API FWorldContainerMoveOp
{
	/**
	 * Move an entry to a new grid position in the same world container.
	 *
	 * Contract (authority-side):
	 * - SourceObject must implement IWorldContainerSessionSource.
	 * - SessionId must already be validated by the caller (the op does NOT
	 *   revalidate the session - that is the authority layer's job).
	 * - EntryInstanceId must exist in the container at the time of the call.
	 * - Quantity is clamped to the entry's current quantity.
	 * - TargetGridPos must be non-negative; placement bounds / overlap
	 *   validation is delegated to the container's StoreContainerEntries.
	 *
	 * Failure modes:
	 * - Source entry vanished between snapshot and consume -> OutError set,
	 *   returns false, no state change.
	 * - Consume rejected -> OutError set, returns false, no state change.
	 * - Store rejected -> rollback to snapshot, OutError set, returns false.
	 * - Rollback also rejected -> logged Error, OutError set, returns false
	 *   (state may drift -- higher layer should close the session and
	 *   resync).
	 */
	static bool Execute(
		UObject* SourceObject,
		const FGuid& SessionId,
		int32 EntryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);
};
