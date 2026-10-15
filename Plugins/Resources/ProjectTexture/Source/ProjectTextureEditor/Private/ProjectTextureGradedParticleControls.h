// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace ProjectTextureGradedParticleControls
{
inline constexpr const TCHAR* FilterFunctionPath=TEXT("/Engine/Functions/Engine_MaterialFunctions01/Texturing/SpiralBlur-Texture.SpiralBlur-Texture");
bool Parse(const TSharedPtr<FJsonObject>& Root,TMap<FName,double>& Out,FString& Error);
bool Validate(const TMap<FName,double>& Controls,FString& Error);
bool NativeFilterIdentity(FString& Identity,FString& Error);
}
