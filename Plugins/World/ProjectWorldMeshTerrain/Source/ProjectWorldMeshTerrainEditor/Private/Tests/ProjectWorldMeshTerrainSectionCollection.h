#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"

class FAutomationTestBase;
class FJsonObject;
class FJsonValue;
class UWorld;
namespace UE::MeshPartition { class ACompiledSection; }

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainSectionCollection
{
    struct FCollected
    {
        TArray<FProjectWorldSurfaceTriangle> Actual,Uncompensated;
        TArray<TArray<FProjectWorldSurfaceTriangle>> PerSection;
        TArray<TSharedPtr<FJsonValue>> Components;
        TSharedPtr<FJsonObject> RequiredPhysics;
        TSharedPtr<FJsonObject> TerrainPhysicsIsolation;
    };
    bool Collect(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
        TConstArrayView<UE::MeshPartition::ACompiledSection*> Sections,FName VariantName,
        bool StorageOnly,bool AllowAdjacent,FCollected& Out,TConstArrayView<FVector> RequiredPoints = {});
}
#endif
