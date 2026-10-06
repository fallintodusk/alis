// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldVegetationRealization.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

// The vegetation layer owns one actor per occupied cell and its instanced components; the tree meshes
// are declared data inputs, observed here by reference.
namespace ProjectWorldVegetationVerifyProjection
{
	bool Project(const FProjectWorldVerifyScope& Scope, FProjectWorldProjection& Projection, FString& OutError)
	{
		for (TActorIterator<AActor> It(Scope.World); It; ++It)
		{
			FString CellId;
			FString Semantic;
			if (!ProjectWorldVegetationRealization::ReadActorIdentity(*It, CellId, Semantic))
			{
				continue;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("actor"), TEXT("vegetation:") + CellId);
			ProjectWorldProjection::AddActorCommon(Record, **It);
			TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
			It->GetComponents(Components);
			Components.Sort([](const UHierarchicalInstancedStaticMeshComponent& Left, const UHierarchicalInstancedStaticMeshComponent& Right)
			{
				return Left.GetName().Compare(Right.GetName(), ESearchCase::CaseSensitive) < 0;
			});
			TArray<FString> Names;
			for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
			{
				const FString Prefix = TEXT("hism.") + Component->GetName() + TEXT(".");
				ProjectWorldProjection::AddStaticMeshComponent(Record, Prefix, *Component, Scope.ArtifactRoot);
				Record.Fields.Add(Prefix + TEXT("attach_parent"), Component->GetAttachParent() != nullptr
					? Component->GetAttachParent()->GetName() : FString(TEXT("none")));
				Record.Fields.Add(Prefix + TEXT("instance_count"), FString::FromInt(Component->GetInstanceCount()));
				Record.Fields.Add(Prefix + TEXT("instances"), ProjectWorldProjection::Instances(*Component));
				Names.Add(Component->GetName());
			}
			if (Components.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Vegetation actor has no instanced components: %s"), *CellId);
				return false;
			}
			Record.Fields.Add(TEXT("components"), FString::Join(Names, TEXT(",")));
			Record.Fields.Add(TEXT("root"), It->GetRootComponent() != nullptr
				? It->GetRootComponent()->GetName() : FString(TEXT("none")));
		}
		return true;
	}

	const FProjectWorldVerifyProjectionRegistrar Registrar(TEXT("project_vegetation_instances"), 1, 1, &Project);
}
