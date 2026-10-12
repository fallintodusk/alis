// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"

class UProjectTextureOutputSlot;
class UTextureRenderTarget2D;

namespace ProjectTextureBundlePreflight
{
bool Validate(const UProjectTextureOutputSlot* Requested,
	TMap<FName, UTextureRenderTarget2D*>& OutTargets, FString& OutError, bool bRequireMaterial = true);
}
