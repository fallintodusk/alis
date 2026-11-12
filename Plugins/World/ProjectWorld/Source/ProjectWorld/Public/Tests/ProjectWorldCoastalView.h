// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UWorld;
class UStaticMeshComponent;
class FJsonObject;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
class PROJECTWORLD_API FProjectWorldCoastalView
{
public:
    FProjectWorldCoastalView();
    ~FProjectWorldCoastalView();
    bool Begin(UWorld& World,const FVector& Camera,double Width,TConstArrayView<int32> Visible,TConstArrayView<int32> Occluded,
        TConstArrayView<UStaticMeshComponent*> Terrain,const FString& Root,FString& Error);
    // Returns true on terminal success or failure; Error distinguishes failure.
    bool Step(FJsonObject& Receipt,FString& Error);
    void Reset();
private:
    struct FState;
    TUniquePtr<FState> State;
};
#endif
