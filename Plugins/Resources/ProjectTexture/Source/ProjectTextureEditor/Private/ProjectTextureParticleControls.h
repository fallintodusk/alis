// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace ProjectTextureParticleControls
{
bool Parse(const TSharedPtr<FJsonObject>&,TMap<FName,double>&,FString&);
bool Validate(const TMap<FName,double>&,FString&);
FString Normalize(const TMap<FName,double>&);
}
