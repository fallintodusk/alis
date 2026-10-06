// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldRoadRealization.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"

// The road layer owns its cell actors and cell meshes, including the bound presentation material.
namespace ProjectWorldRoadVerifyProjection
{
	bool Project(const FProjectWorldVerifyScope& Scope, FProjectWorldProjection& Projection, FString& OutError)
	{
		TArray<const UStaticMesh*> Meshes;
		for (TActorIterator<AStaticMeshActor> It(Scope.World); It; ++It)
		{
			FString CellId;
			FString Semantic;
			if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, Semantic))
			{
				continue;
			}
			const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			if (Component == nullptr)
			{
				OutError = FString::Printf(TEXT("Road actor has no mesh component: %s"), *CellId);
				return false;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("actor"), TEXT("road:") + CellId);
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

	const FProjectWorldVerifyProjectionRegistrar Registrar(TEXT("project_road_mesh"), 1, 1, &Project);
}
