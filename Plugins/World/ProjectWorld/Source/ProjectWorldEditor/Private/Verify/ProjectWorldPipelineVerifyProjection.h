// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class FProjectWorldProjection;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldRealizationProfile;
struct FProjectWorldVerifyIdentity;

namespace ProjectWorldPipelineVerifyProjection
{
	bool Project(UWorld& World, const FProjectWorldCanonicalBundle& Bundle, const FString& ManifestRoot,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldVerifyIdentity& Identity, FProjectWorldProjection& Projection,
		TMap<FString, FString>& PackageDigests, FString& OutError);
}
