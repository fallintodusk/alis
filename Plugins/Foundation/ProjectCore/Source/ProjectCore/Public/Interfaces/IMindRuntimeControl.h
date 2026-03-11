// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APawn;

/**
 * Runtime-only control plane for local mind context wiring.
 *
 * UI should use IMindService; gameplay bootstrap code uses this interface
 * to provide local player context (pawn/controller ownership).
 */
class IMindRuntimeControl
{
public:
	virtual ~IMindRuntimeControl() = default;

	static FName ServiceKey() { return FName(TEXT("IMindRuntimeControl")); }

	virtual void SetLocalPlayerPawn(APawn* InPawn) = 0;
	virtual void ClearLocalPlayerPawn(const APawn* InPawn) = 0;

	/**
	 * Input activity pulse from local controller.
	 * Runtime can use this to re-arm idle-based scans without polling.
	 */
	virtual void NotifyPlayerInputActivity() = 0;
};
