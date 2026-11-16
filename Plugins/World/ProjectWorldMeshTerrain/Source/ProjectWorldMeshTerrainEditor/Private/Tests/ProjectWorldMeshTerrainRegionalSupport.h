#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"
class FJsonObject;
namespace ProjectWorldRegionalSceneTest {struct FScene;}

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionalSupportTest
{
    enum class ESupportLayer {Ground,Structure};
    struct FSample {double Height=0,WaterHeight=0;bool Structure=false,Wet=false;};
    struct FSupport
    {
        TArray<FProjectWorldSurfaceTriangle> Ground,Structures,Water;
        int32 GroundCapacity=4096;
        bool Read(const ProjectWorldRegionalSceneTest::FScene& Scene,FString& Error);
        bool Sample(const FVector& Position,FSample& Out,FString& Error) const;
        bool FloorEnvelope(const FVector& Position,double Radius,double FloorDistance,ESupportLayer Owner,double& Bound,FString& Error) const;
    };
    struct FRoute
    {
        FString Id;double WidthMeters=0;TArray<FVector> Points;
    };
    bool ReadRoutes(const FJsonObject& Input,TArray<FRoute>& Routes,FString& Error);
}
#endif
