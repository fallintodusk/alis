// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class APawn;

enum class ESinglePlayTraversalMode : uint8
{
	Default,
	PreviewFlight
};

enum class ESinglePlayTraversalParseResult : uint8
{
	Absent,
	Supported,
	Unknown
};

struct FSinglePlayTraversalSelection
{
	ESinglePlayTraversalMode Mode = ESinglePlayTraversalMode::Default;
	ESinglePlayTraversalParseResult ParseResult = ESinglePlayTraversalParseResult::Absent;
};

namespace ProjectSinglePlayTraversal
{
	PROJECTSINGLEPLAY_API const TCHAR* OptionName();
	PROJECTSINGLEPLAY_API const TCHAR* PreviewFlightValue();
	PROJECTSINGLEPLAY_API FSinglePlayTraversalSelection Resolve(const FString& Value);
	PROJECTSINGLEPLAY_API bool Apply(APawn* Pawn, ESinglePlayTraversalMode Mode);
}
