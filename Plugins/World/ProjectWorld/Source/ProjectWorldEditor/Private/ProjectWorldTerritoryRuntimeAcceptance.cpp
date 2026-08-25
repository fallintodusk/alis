// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldTerritoryRuntimeAcceptance.h"

#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldPartitionPolicy.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRuntimeProfile.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "WorldPartition/HLOD/HLODActor.h"

namespace ProjectWorldTerritoryRuntimeAcceptance
{
	bool CaptureAndCheck(
		UWorld* World,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FName RoadTag(TEXT("ProjectWorld.Road.v1"));
		const FName BuildingTag(TEXT("ProjectWorld.BuildingMassing.v1"));
		const FName VegetationTag(TEXT("ProjectWorld.Vegetation=v1"));
		bool bFoundRoad = false;
		bool bFoundBuilding = false;
		bool bFoundVegetation = false;
		bool bNaniteAccepted = true;
		bool bInstancingAccepted = true;
		int32 AlwaysLoadedPlayerStarts = 0;
		OutResult.HlodLayerReferenceCount = ProjectWorldPartitionPolicy::CountHLODLayerReferences(World);

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<AWorldPartitionHLOD>())
			{
				++OutResult.HlodProxyActorCount;
			}
			if (!It->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag))
			{
				continue;
			}
			++OutResult.GeneratedActorCount;
			OutResult.SpatiallyLoadedActorCount += It->GetIsSpatiallyLoaded() ? 1 : 0;
			if (It->bEnableAutoLODGeneration || It->GetHLODLayer() != nullptr)
			{
				++OutResult.HlodEligibleGeneratedActorCount;
			}
			const bool bPlayerStart = It->IsA<APlayerStart>() && It->Tags.ContainsByPredicate([](const FName& Tag)
			{
				return Tag == TEXT("ProjectWorld.RuntimeRole=PlayerStart");
			});
			AlwaysLoadedPlayerStarts += bPlayerStart && !It->GetIsSpatiallyLoaded() ? 1 : 0;

			const bool bNaniteLayer = It->Tags.Contains(RoadTag) || It->Tags.Contains(BuildingTag);
			bFoundRoad |= It->Tags.Contains(RoadTag);
			bFoundBuilding |= It->Tags.Contains(BuildingTag);
			bFoundVegetation |= It->Tags.Contains(VegetationTag);
			TInlineComponentArray<UStaticMeshComponent*> Meshes;
			It->GetComponents(Meshes);
			for (UStaticMeshComponent* MeshComponent : Meshes)
			{
				UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
				if (bNaniteLayer || It->Tags.Contains(VegetationTag))
				{
					bNaniteAccepted &= Mesh != nullptr && Mesh->GetNaniteSettings().bEnabled;
				}
				if (It->Tags.Contains(VegetationTag))
				{
					const UHierarchicalInstancedStaticMeshComponent* HISM =
						Cast<UHierarchicalInstancedStaticMeshComponent>(MeshComponent);
					bInstancingAccepted &= HISM != nullptr && HISM->GetInstanceCount() > 0;
				}
			}
		}

		OutResult.RuntimeAlwaysLoadedActorCount = AlwaysLoadedPlayerStarts;
		OutResult.bRuntimeStreamingPolicyProbed = AlwaysLoadedPlayerStarts == 1;
		OutResult.bRuntimeNanitePolicyProbed = bFoundRoad && bFoundBuilding && bNaniteAccepted;
		OutResult.bRuntimeInstancingPolicyProbed = bFoundVegetation && bInstancingAccepted;
		OutResult.bRuntimeHlodPolicyProbed = OutResult.HlodProxyActorCount == 0 &&
			OutResult.HlodLayerReferenceCount == 0 && OutResult.HlodEligibleGeneratedActorCount == 0;
		if (!OutResult.bRuntimeStreamingPolicyProbed || !OutResult.bRuntimeNanitePolicyProbed ||
			!OutResult.bRuntimeInstancingPolicyProbed || !OutResult.bRuntimeHlodPolicyProbed)
		{
			OutError = FString::Printf(
				TEXT("Territory runtime policy mismatch: player_start=%d nanite=%d instancing=%d hlod=%d."),
				AlwaysLoadedPlayerStarts,
				OutResult.bRuntimeNanitePolicyProbed ? 1 : 0,
				OutResult.bRuntimeInstancingPolicyProbed ? 1 : 0,
				OutResult.bRuntimeHlodPolicyProbed ? 1 : 0);
			return false;
		}
		if (OutResult.GeneratedSourceBytes > Profile.Budgets.GeneratedSourceBytes ||
			OutResult.GeneratedActorCount > Profile.Budgets.GeneratedActorCount)
		{
			OutError = FString::Printf(
				TEXT("Territory runtime structural budget exceeded: source=%lld actors=%d."),
				OutResult.GeneratedSourceBytes,
				OutResult.GeneratedActorCount);
			return false;
		}
		OutResult.bRuntimeStructuralBudgetsPassed = true;
		return true;
	}
}
