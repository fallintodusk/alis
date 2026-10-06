// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldBuildingRealization.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"

// The building layer owns its cell actors and cell massing meshes, including the bound presentation material.
namespace ProjectWorldBuildingVerifyProjection
{
	bool Project(const FProjectWorldVerifyScope& Scope, FProjectWorldProjection& Projection, FString& OutError)
	{
		TArray<const UStaticMesh*> Meshes;
		for (TActorIterator<AStaticMeshActor> It(Scope.World); It; ++It)
		{
			FString CellId;
			FString Semantic;
			if (!ProjectWorldBuildingRealization::ReadActorIdentity(*It, CellId, Semantic))
			{
				for (const FName& Tag : It->Tags)
				{
					const FString Value = Tag.ToString();
					const FString Prefix = TEXT("ProjectWorld.BuildingCell=");
					if (Value.StartsWith(Prefix))
					{
						CellId = Value.RightChop(Prefix.Len());
						break;
					}
				}
				if (CellId.IsEmpty())
				{
					continue;
				}
			}
			const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			if (Component == nullptr)
			{
				OutError = FString::Printf(TEXT("Building actor has no mesh component: %s"), *CellId);
				return false;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("actor"), TEXT("building:") + CellId);
			ProjectWorldProjection::AddActorCommon(Record, **It);
			ProjectWorldProjection::AddStaticMeshComponent(Record, TEXT("component."), *Component, Scope.ArtifactRoot);
			if (Component->GetStaticMesh() != nullptr)
			{
				Meshes.AddUnique(Component->GetStaticMesh());
			}
		}
		for (const UStaticMesh* Mesh : Meshes)
		{
			FProjectWorldProjectionRecord& Record = Projection.Add(
				TEXT("mesh"), ProjectWorldProjection::RootRelative(Mesh->GetOutermost()->GetName(), Scope.ArtifactRoot));
			ProjectWorldProjection::AddStaticMesh(Record, *Mesh, Scope.ArtifactRoot);
		}
		return true;
	}

	const FProjectWorldVerifyProjectionRegistrar Registrar(TEXT("project_building_massing"), 2, 1, &Project);
}
