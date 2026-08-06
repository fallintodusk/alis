// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldPresentationProfile;
struct FProjectWorldPresentationResources;

namespace ProjectWorldPresentationMaterialRealization
{
	bool Prepare(
		const FProjectWorldPresentationProfile& Profile,
		FProjectWorldPresentationResources& InOutResources,
		FString& OutError);
}
