#include "ProjectTimeClock.h"

bool FProjectTimeClock::Read(FDateTime LiveUtc, double MonotonicSeconds, FProjectTimeSnapshot& Out) const
{
	if (!FMath::IsFinite(MonotonicSeconds) || MonotonicSeconds < 0.0 || LiveUtc.GetTicks() < 0 ||
		LiveUtc > FDateTime::MaxValue()) return false;
	FDateTime Utc = LiveUtc;
	if (bDemo)
	{
		const double Elapsed = MonotonicSeconds - AnchorSeconds;
		const double DeltaTicks = Elapsed * Rate * ETimespan::TicksPerSecond;
		const int64 Remaining = FDateTime::MaxValue().GetTicks() - AnchorUtc.GetTicks();
		if (Elapsed < 0.0 || !FMath::IsFinite(DeltaTicks) || DeltaTicks > static_cast<double>(Remaining))
			return false;
		const int64 Advance = static_cast<int64>(DeltaTicks);
		if (Advance > Remaining) return false;
		Utc = FDateTime(AnchorUtc.GetTicks() + Advance);
	}
	Out.Utc = Utc;
	Out.bDemo = bDemo;
	Out.Rate = bDemo ? Rate : 1.0;
	return true;
}

void FProjectTimeClock::SetControlsAllowed(bool bAllowed)
{
	bControlsAllowed = bAllowed;
	if (!bAllowed)
	{
		bDemo = false;
		Rate = 1.0;
	}
}

bool FProjectTimeClock::Seek(FDateTime Utc, double MonotonicSeconds)
{
	if (!bControlsAllowed || !FMath::IsFinite(MonotonicSeconds) || MonotonicSeconds < 0.0 ||
		Utc.GetTicks() < 0 || Utc > FDateTime::MaxValue()) return false;
	AnchorUtc = Utc;
	AnchorSeconds = MonotonicSeconds;
	Rate = 0.0;
	bDemo = true;
	return true;
}

bool FProjectTimeClock::SetRate(double InRate, FDateTime LiveUtc, double MonotonicSeconds)
{
	if (!bControlsAllowed || !FMath::IsFinite(InRate) || InRate < 0.0 || InRate > 60.0) return false;
	FProjectTimeSnapshot Current;
	if (!Read(LiveUtc, MonotonicSeconds, Current)) return false;
	AnchorUtc = Current.Utc;
	AnchorSeconds = MonotonicSeconds;
	Rate = InRate;
	bDemo = true;
	return true;
}

bool FProjectTimeClock::Reset()
{
	if (!bControlsAllowed) return false;
	bDemo = false;
	Rate = 1.0;
	return true;
}
