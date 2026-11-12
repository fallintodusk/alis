#pragma once
#include "ProjectBuildingAssemblyRealization.h"
#include "ProjectWorldBuildingMeshBuilder.h"
#include "ProjectWorldTerrainSurface.h"

namespace ProjectWorldBuildingAssembly
{
    struct FPrepared
    {
        FString FeatureId, TerrainArtifact;
        ProjectBuildingAssembly::FInput Input;
        ProjectBuildingAssembly::FPlan Plan;
        FTransform Transform;
        double SupportMinimum = 0, SupportMaximum = 0, CoveredArea = 0;
        TArray<FVector2D> Footprint;
        FProjectWorldBuildingMeshBuildResult RemainingMassing;
    };
    bool Prepare(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldCanonicalCell& Cell,
        const FProjectWorldAuthoredOverlaySet& Overlays,const FString& FeatureId,
        const ProjectBuildingAssembly::FKit& Kit,FPrepared& Out,FString& Error, bool bBuildRemainingMassing = true);
}
