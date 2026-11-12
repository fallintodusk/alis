#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldRealizationProfile.h"
class FAutomationTestBase;
class FJsonObject;
class UWorld;
struct FProjectWorldCanonicalBundle;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionWater
{
    struct FState
    {
        FProjectWorldRealizationProfile Profile;
        TSharedPtr<FJsonObject> Before;
        TArray<TWeakObjectPtr<UObject>> OriginalMeshes;
        FString ProfileHash;
    };
    bool Apply(FAutomationTestBase& Test,UWorld& World,const FJsonObject& Document,
        const FProjectWorldCanonicalBundle& Bundle,const FString& Mount,TArray<FString>& Packages,FState& State);
    bool Observe(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
        FState& State,FJsonObject& Receipt,bool Reloaded);
}
#endif
