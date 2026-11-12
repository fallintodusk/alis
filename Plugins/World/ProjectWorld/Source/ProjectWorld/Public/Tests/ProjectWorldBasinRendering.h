// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
class UWorld;
class UStaticMeshComponent;

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
enum class EProjectWorldBasinRenderProgress : uint8 { Pending,Passed,Failed };
class PROJECTWORLD_API FProjectWorldBasinRenderProbe
{
public:
    FProjectWorldBasinRenderProbe();
    ~FProjectWorldBasinRenderProbe();
    bool Begin(UWorld& World,const FBox& WaterBounds,TConstArrayView<UStaticMeshComponent*> Terrain,double MaximumStoredGradient,const FString& Root,FString& Error);
    EProjectWorldBasinRenderProgress Step(FJsonObject& Receipt,FString& Error);
    void Reset();
private:
    struct FState;
    TUniquePtr<FState> State;
};
#endif
