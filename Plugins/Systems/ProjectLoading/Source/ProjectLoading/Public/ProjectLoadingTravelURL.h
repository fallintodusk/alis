// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FURL;

namespace ProjectLoadingTravelURL
{
	inline constexpr const TCHAR* ProvenanceOption = TEXT("ProjectLoadingRoute=1");

	// Builds the authoritative travel URL. The provenance option lets the
	// destination prove that it was entered through ProjectLoading, while the
	// game override remains last for Unreal's URL parser.
	PROJECTLOADING_API FString Build(
		const FString& MapPath,
		const TMap<FString, FString>& CustomOptions);

	PROJECTLOADING_API bool HasProvenance(const FURL& URL);
}
