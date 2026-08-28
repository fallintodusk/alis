// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

enum class ESinglePlayScenarioParseResult : uint8
{
	Absent,
	Selected,
	Unknown
};

struct FSinglePlayScenarioSelection
{
	FName ScenarioId;
	ESinglePlayScenarioParseResult Result = ESinglePlayScenarioParseResult::Absent;

	bool IsEnabled() const
	{
		return Result == ESinglePlayScenarioParseResult::Selected && !ScenarioId.IsNone();
	}
};

namespace FSinglePlayScenarioPolicy
{
	PROJECTSINGLEPLAY_API const TCHAR* OptionName();
	PROJECTSINGLEPLAY_API FSinglePlayScenarioSelection Resolve(const FString& Options);
}
