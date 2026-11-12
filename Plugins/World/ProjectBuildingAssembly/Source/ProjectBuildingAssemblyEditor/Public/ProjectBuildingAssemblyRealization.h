#pragma once
#include "CoreMinimal.h"
#include "ProjectBuildingAssemblyPlan.h"
class AActor;
class UWorld;
class UMaterialInterface;
class UStaticMesh;

namespace ProjectBuildingAssembly
{
    struct FNativeOptions
    {
        TArray<UStaticMesh*> Modules;
        FGuid ActorGuid;
        FName ActorName;
        // External updates require the caller's enclosing World transaction.
        bool bExternalActor = false;
    };
    PROJECTBUILDINGASSEMBLYEDITOR_API UStaticMesh* CreateModule(UObject& Outer, FName Name,
        EPart Part, const FKit& Kit, UMaterialInterface* Material, EObjectFlags Flags);
    PROJECTBUILDINGASSEMBLYEDITOR_API bool ModuleMatches(UStaticMesh* Mesh, EPart Part, const FKit& Kit);
    PROJECTBUILDINGASSEMBLYEDITOR_API bool ReadKit(const FString& Path, FKit& Out, FString& Error);
    PROJECTBUILDINGASSEMBLYEDITOR_API FString GeometryIdentity(const FString& FeatureId,
        const FPlan& Plan, const FKit& Kit, const FTransform& Transform);
    PROJECTBUILDINGASSEMBLYEDITOR_API bool Apply(UWorld& World, const FString& FeatureId,
        const FPlan& Plan, const FKit& Kit, const FTransform& Transform, UMaterialInterface* Material,
        AActor*& OutActor, bool& Reused, FString& Error, const FNativeOptions& Options = {});
    PROJECTBUILDINGASSEMBLYEDITOR_API bool Matches(const AActor& Actor, const FString& Identity,
        const FPlan& Plan, const FKit& Kit, const FTransform& Transform, UMaterialInterface* Material,
        TConstArrayView<UStaticMesh*> Modules = {});
}
