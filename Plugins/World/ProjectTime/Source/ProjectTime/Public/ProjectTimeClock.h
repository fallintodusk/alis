#pragma once

#include "Interfaces/ProjectTimeView.h"

/** Calendar arithmetic only; callers supply UTC and monotonic samples. */
class PROJECTTIME_API FProjectTimeClock
{
public:
	bool Read(FDateTime LiveUtc, double MonotonicSeconds, FProjectTimeSnapshot& Out) const;
	void SetControlsAllowed(bool bAllowed);
	bool Seek(FDateTime Utc, double MonotonicSeconds);
	bool SetRate(double Rate, FDateTime LiveUtc, double MonotonicSeconds);
	bool Reset();
	bool ControlsAllowed() const { return bControlsAllowed; }
	bool IsDemo() const { return bDemo; }

private:
	bool bControlsAllowed = false;
	bool bDemo = false;
	double Rate = 1.0;
	FDateTime AnchorUtc;
	double AnchorSeconds = 0.0;
};
