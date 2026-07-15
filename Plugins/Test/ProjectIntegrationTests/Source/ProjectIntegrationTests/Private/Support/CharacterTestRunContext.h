// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace ProjectCharacterTest
{
	inline FString ResolveCaptureRunId()
	{
		FString RequestedRunId;
		if (FParse::Value(
			FCommandLine::Get(),
			TEXT("CharacterCaptureRunId="),
			RequestedRunId))
		{
			const FString SanitizedRunId = FPaths::MakeValidFileName(RequestedRunId);
			if (!SanitizedRunId.IsEmpty())
			{
				return SanitizedRunId;
			}
		}

		return FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	}
}
