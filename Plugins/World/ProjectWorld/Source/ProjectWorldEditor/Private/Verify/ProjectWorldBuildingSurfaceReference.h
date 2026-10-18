// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace ProjectWorldBuildingSurfaceReference
{
	bool Write(UWorld& World, const FString& Map, const FString& RuntimeHash,
		const FString& ResultPath, FString& OutError);
}
