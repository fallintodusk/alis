// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldEvidenceReadiness
{
	static constexpr int32 RequiredReadyFrames = 3;

	bool Advance(uint64 FrameNumber, int32 RemainingCompilations)
	{
		if (FrameNumber == LastFrameNumber)
		{
			return false;
		}
		LastFrameNumber = FrameNumber;
		if (RemainingCompilations > 0)
		{
			ConsecutiveReadyFrames = 0;
			return false;
		}
		return ++ConsecutiveReadyFrames >= RequiredReadyFrames;
	}

private:
	uint64 LastFrameNumber = MAX_uint64;
	int32 ConsecutiveReadyFrames = 0;
};
