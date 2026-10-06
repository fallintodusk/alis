#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalCell;

namespace ProjectWorldGeneratedGeometry
{
	extern PROJECTWORLDEDITOR_API const FName GeneratedTag;
	PROJECTWORLDEDITOR_API FGuid StableGuid(const FString& Value);
	PROJECTWORLDEDITOR_API double SampleTerrain(const FProjectWorldCanonicalCell& Cell, double X, double Y);
}
