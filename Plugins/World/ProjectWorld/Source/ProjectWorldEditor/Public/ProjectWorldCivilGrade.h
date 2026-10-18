// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalFeature;
struct FProjectWorldCanonicalTerrain;
struct FProjectWorldCanonicalValidation;
class FJsonObject;

enum class EProjectWorldTerrainQuery : uint8 { FinalSurface, ImmutableGrid };

namespace ProjectWorldCivilGrade
{
	PROJECTWORLDEDITOR_API FString BaseHash(const FProjectWorldCanonicalTerrain& Terrain);
	PROJECTWORLDEDITOR_API bool Read(const TSharedPtr<FJsonObject>& Attributes,
		FProjectWorldCanonicalFeature& Feature, FProjectWorldCanonicalValidation& Validation);
	PROJECTWORLDEDITOR_API bool Validate(const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalFeature& Feature, FString& Identity, FString& Error);
}
