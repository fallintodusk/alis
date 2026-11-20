#pragma once

#include "CoreMinimal.h"

namespace ProjectTimeZone
{
	PROJECTTIME_API bool IsValid(const FString& Zone);
	PROJECTTIME_API FDateTime ReadPcUtc();
	PROJECTTIME_API FText Format(FDateTime Utc, const FString& Zone);
}
