#include "ProjectTimeZone.h"
#include "HAL/PlatformTime.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"

bool ProjectTimeZone::IsValid(const FString& Zone)
{
	if (Zone.IsEmpty() || Zone.Len() > 128) return false;
#if UE_ENABLE_ICU
	// VV returns the resolved zone ID, exposing ICU's unknown-zone substitution.
	// Use Core's initialized formatter rather than another statically linked ICU.
	const FString Resolved = FText::AsDateTime(FDateTime(2026, 1, 1), TEXT("VV"), Zone,
		FInternationalization::Get().GetInvariantCulture()).ToString();
	return Resolved == Zone && Resolved != TEXT("Etc/Unknown");
#else
	return false;
#endif
}

FDateTime ProjectTimeZone::ReadPcUtc()
{
	int32 Year, Month, WeekDay, Day, Hour, Minute, Second, Millisecond;
	FPlatformTime::UtcTime(Year, Month, WeekDay, Day, Hour, Minute, Second, Millisecond);
	return FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
}

FText ProjectTimeZone::Format(FDateTime Utc, const FString& Zone)
{
	return IsValid(Zone) ? FText::AsDateTime(Utc, TEXT("yyyy-MM-dd HH:mm:ss z"), Zone) : FText::GetEmpty();
}
