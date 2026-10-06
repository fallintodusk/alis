// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWorldEvidenceSubject.h"

namespace ProjectWorldEvidenceReadiness
{
	/**
	 * Subjects are drawn when every present subject expects at least one unit, draws all of them,
	 * and has no pending build. Reports that are not present are ignored, so a world without any
	 * subject counts as drawn.
	 */
	inline bool AreSubjectsDrawn(TConstArrayView<FProjectWorldEvidenceSubjectReport> Reports)
	{
		for (const FProjectWorldEvidenceSubjectReport& Report : Reports)
		{
			if (!Report.bPresent)
			{
				continue;
			}
			if (Report.ExpectedUnits <= 0 || Report.DrawnUnits != Report.ExpectedUnits || Report.PendingBuilds != 0)
			{
				return false;
			}
		}
		return true;
	}
}

struct FProjectWorldEvidenceReadiness
{
	static constexpr int32 RequiredReadyFrames = 3;

	// A frame with a pending compilation or an undrawn subject resets the count.
	bool Advance(uint64 FrameNumber, int32 RemainingCompilations, bool bSubjectsDrawn)
	{
		if (FrameNumber == LastFrameNumber)
		{
			return false;
		}
		LastFrameNumber = FrameNumber;
		if (RemainingCompilations > 0 || !bSubjectsDrawn)
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
