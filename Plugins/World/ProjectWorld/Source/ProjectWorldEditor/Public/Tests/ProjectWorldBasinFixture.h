// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
class UMaterialInterface;
struct FProjectWorldCanonicalBundle;
class FAutomationTestBase;
class UWorld;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinFixture
{
    PROJECTWORLDEDITOR_API bool AddWater(const FJsonObject& Document,FProjectWorldCanonicalBundle& Bundle,FString& Error);
    PROJECTWORLDEDITOR_API bool ResolveWaterMaterial(UMaterialInterface*& Material,TSharedPtr<FJsonObject>& Identity,FString& Error);
    PROJECTWORLDEDITOR_API bool CheckRefusals(const FJsonObject& Document,const FProjectWorldCanonicalBundle& Bundle,FAutomationTestBase& Test);
    PROJECTWORLDEDITOR_API bool ReadStreamingRange(UWorld& World,int32& Range,FString& Error);
}
#endif
