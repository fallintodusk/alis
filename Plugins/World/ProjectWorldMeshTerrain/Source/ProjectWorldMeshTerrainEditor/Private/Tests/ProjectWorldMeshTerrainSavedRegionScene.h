#pragma once
#include "CoreMinimal.h"
class UPrimitiveComponent;
class UWorld;
namespace ProjectWorldRegionalSceneTest {struct FScene;}

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldSavedRegionSceneTest
{
    struct FProductScene
    {
        FString Map;
        TArray<FString> Packages;
        bool Read(ProjectWorldRegionalSceneTest::FScene& Reference,const FString& Path,const FString& Hash,
            const FString& GameplayHash,FString& Error);
        bool Ready(UWorld* World,FString& Error) const;
        bool Owns(const UPrimitiveComponent* Component) const;
    };
}
#endif
