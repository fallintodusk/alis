#pragma once

#include "CoreMinimal.h"
#include "ProjectWorldCivilGrade.h"

struct FProjectWorldCanonicalCell;

namespace ProjectWorldGeneratedGeometry
{
	extern PROJECTWORLDEDITOR_API const FName GeneratedTag;
	PROJECTWORLDEDITOR_API FGuid StableGuid(const FString& Value);
	PROJECTWORLDEDITOR_API bool SampleTerrain(const FProjectWorldCanonicalCell& Cell, double X, double Y,
		double& OutHeight, FString& OutError, EProjectWorldTerrainQuery Query = EProjectWorldTerrainQuery::FinalSurface);
}
