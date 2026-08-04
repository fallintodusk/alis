// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldLandscapeLayout;
struct FProjectWorldRealizationResult;
class AActor;
class UWorld;

struct FProjectWorldLandscapeHeightfield
{
	FIntPoint VertexCount = FIntPoint::ZeroValue;
	TArray<uint16> EncodedHeights;
	double MinimumHeightMeters = 0.0;
	double MaximumHeightMeters = 0.0;
};

namespace ProjectWorldLandscapeRealization
{
	bool IsGeneratedLandscape(const AActor* Actor);

	bool BuildHeightfield(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		FProjectWorldLandscapeHeightfield& OutHeightfield,
		FString& OutError);

	bool CreateOrUpdate(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldLandscapeLayout& Layout,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);

	bool ClearGeneratedLayers(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
