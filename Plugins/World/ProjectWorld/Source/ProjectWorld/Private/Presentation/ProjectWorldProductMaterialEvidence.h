// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UWorld;

namespace ProjectWorldProductMaterialEvidence
{
	bool ReadBindings(UWorld& World, FJsonObject& Receipt, FString& OutError);
}
