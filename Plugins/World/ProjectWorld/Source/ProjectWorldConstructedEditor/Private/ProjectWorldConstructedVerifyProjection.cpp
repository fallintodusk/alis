// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldVerify.h"
#include "ProjectWorldConstructedPlan.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"

namespace ProjectWorldConstructedVerifyProjection
{
	bool Project(const FProjectWorldVerifyScope& Scope,FProjectWorldProjection& Output,FString& Error)
	{
		TArray<const UStaticMesh*> Meshes;
		for (TActorIterator<AStaticMeshActor> It(Scope.World); It; ++It)
		{
			if (!It->Tags.Contains(FName(TEXT("ProjectWorld.ConstructedSurface.v1")))) continue;
			const auto* Component=It->GetStaticMeshComponent();
			if (!Component || !Component->GetStaticMesh()) {Error=TEXT("Structural projection requires an intact physical mesh."); return false;}
			const UStaticMesh* Mesh=Component->GetStaticMesh();
			auto& Record=Output.Add(TEXT("actor"),ProjectWorldProjection::RootRelative(Mesh->GetOutermost()->GetName(),Scope.ArtifactRoot));
			ProjectWorldProjection::AddActorCommon(Record,**It);
			ProjectWorldProjection::AddStaticMeshComponent(Record,TEXT("component."),*Component,Scope.ArtifactRoot);
			Meshes.AddUnique(Mesh);
		}
		for (const auto* Mesh : Meshes)
		{
			auto& Record=Output.Add(TEXT("mesh"),ProjectWorldProjection::RootRelative(Mesh->GetOutermost()->GetName(),Scope.ArtifactRoot));
			ProjectWorldProjection::AddStaticMesh(Record,*Mesh,Scope.ArtifactRoot);
			const auto* Description=Mesh->GetMeshDescription(0);
			if (!Description || !Description->TriangleAttributes().HasAttributeOfType<int32>(ProjectWorldConstructedRealization::SourceFaceAttribute))
			{Error=TEXT("Structural projection lost stored source-face correspondence."); return false;}
			const auto Sources=Description->TriangleAttributes().GetAttributesRef<int32>(ProjectWorldConstructedRealization::SourceFaceAttribute);
			FString Ordinals;
			for (const auto Triangle : Description->Triangles().GetElementIDs()) Ordinals+=FString::Printf(TEXT("%d;"),Sources[Triangle]);
			Record.Fields.Add(TEXT("source_faces.sha256"),ProjectWorldProjection::HashText(Ordinals));
		}
		return true;
	}
	const FProjectWorldVerifyProjectionRegistrar Registrar(TEXT("project_constructed_surface"),1,1,&Project);
}
