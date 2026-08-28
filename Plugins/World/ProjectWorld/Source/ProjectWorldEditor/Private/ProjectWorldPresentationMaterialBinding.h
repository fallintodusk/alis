// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

struct FProjectWorldPresentationResources;
class FString;

namespace ProjectWorldPresentationMaterialBinding
{
	bool ResolveTerrain(
		FProjectWorldPresentationResources& InOutResources,
		FString& OutError);
}
