#include "ProjectWorldRoadLayerProducer.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"

FProjectWorldRoadLayerProducer& FProjectWorldRoadLayerProducer::Get()
{
	static FProjectWorldRoadLayerProducer Instance;
	return Instance;
}

FProjectWorldRoadLayerProducer::FProjectWorldRoadLayerProducer()
{
	Declaration.GeneratorId = TEXT("project_road_mesh");
	Declaration.GeneratorVersion = 1;
	Declaration.CanonicalSelectors = {TEXT("roads")};
	Declaration.SpatialOwnership = TEXT("cell_local");
	Declaration.RuntimeMapping = TEXT("world_partition_spatial");
	Declaration.DependencyHaloCells = 1;
	Declaration.PermittedDependencies = TArray<FString>{TEXT("terrain")};
	Declaration.OwnedActors.OwnedTags = {FName(TEXT("ProjectWorld.Road.v1"))};
	Declaration.OwnedActors.CellTagPrefix = TEXT("ProjectWorld.RoadCell=");
}

const FProjectWorldLayerProducerDeclaration& FProjectWorldRoadLayerProducer::GetDeclaration() const
{
	return Declaration;
}

bool FProjectWorldRoadLayerProducer::ValidateSettings(
	const FString& NormalizedSettings, FString& OutError) const
{
	return ProjectWorldRoadRealization::ValidateSettings(NormalizedSettings, OutError);
}

bool FProjectWorldRoadLayerProducer::HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
	FProjectWorldLayerUnitInputs& OutUnits, FString& OutError) const
{
	for (const FProjectWorldCanonicalCell& Cell : Inputs.Bundle.Cells)
	{
		FString Hash;
		if (!ProjectWorldRoadRealization::HashCellInput(
			Inputs.Bundle, Cell, Inputs.Layer, Hash, OutError))
		{
			return false;
		}
		OutUnits.HashByUnit.Add(Cell.CellId, Hash);
	}
	return true;
}

bool FProjectWorldRoadLayerProducer::Apply(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	UMaterialInterface* const* Material = Context.PresentationMaterials.Find(TEXT("road"));
	if (!ProjectWorldRoadRealization::Apply(&Context.World, Context.Inputs.Bundle,
		Context.Inputs.Profile, Material != nullptr ? *Material : nullptr, OutResult, OutError))
	{
		return false;
	}
	Inventory.Metrics.Add(TEXT("triangles_written"), OutResult.MutableProducerMetric(TEXT("project_road_mesh"), TEXT("triangles_written")));
	return true;
}

bool FProjectWorldRoadLayerProducer::CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	TSet<FString> ExpectedCells;
	for (const FProjectWorldCanonicalCell& Cell : Context.Inputs.Bundle.Cells)
	{
		bool bExpected = false;
		if (!ProjectWorldRoadRealization::ExpectsCellOutput(
			Context.Inputs.Bundle, Cell, Context.Inputs.Layer, bExpected, OutError))
		{
			return false;
		}
		if (bExpected)
		{
			ExpectedCells.Add(Cell.CellId);
		}
	}
	TSet<FString> ActualCells;
	for (TActorIterator<AStaticMeshActor> It(&Context.World); It; ++It)
	{
		FString CellId;
		FString MeshSemantic;
		if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, MeshSemantic))
		{
			continue;
		}
		UStaticMeshComponent* Component = It->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
		FString Failure;
		if (!ExpectedCells.Contains(CellId)) Failure = TEXT("cell has no selected canonical roads");
		else if (ActualCells.Contains(CellId)) Failure = TEXT("cell actor is duplicated");
		else if (Mesh == nullptr) Failure = TEXT("StaticMesh is missing");
		else if (!Mesh->GetNaniteSettings().bEnabled) Failure = TEXT("road mesh has Nanite disabled");
		else if (Mesh->GetBodySetup() == nullptr ||
			Mesh->GetBodySetup()->CollisionTraceFlag != CTF_UseComplexAsSimple)
			Failure = TEXT("road collision is not complex-as-simple");
		else if (!It->GetIsSpatiallyLoaded()) Failure = TEXT("actor is not spatially loaded");
		else if (It->bEnableAutoLODGeneration) Failure = TEXT("actor permits HLOD generation");
		else if (It->GetHLODLayer() != nullptr) Failure = TEXT("actor has an HLOD layer");
		else if (Component->CanEverAffectNavigation()) Failure = TEXT("road affects navigation");
		if (!Failure.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Persistent road ownership is invalid for cell %s: %s"),
				*CellId, *Failure);
			return false;
		}
		ActualCells.Add(CellId);
		if (!ProjectWorldLayerProducerRegistry::AddPackageArtifact(
			Mesh->GetOutermost()->GetName(), TEXT("asset"), MeshSemantic, Inventory, OutError))
		{
			return false;
		}
		FString ActorSemantic;
		if (!ProjectWorldRoadRealization::HashActorSemantic(CellId, MeshSemantic, ActorSemantic) ||
			!ProjectWorldLayerProducerRegistry::AddPackageArtifact(
				It->GetPackage()->GetName(), TEXT("external_actor"), ActorSemantic, Inventory, OutError))
		{
			return false;
		}
	}
	if (ActualCells.Num() != ExpectedCells.Num())
	{
		OutError = TEXT("Road cell inventory does not exactly cover selected canonical roads.");
		return false;
	}
	OutResult.MutableProducerMetric(TEXT("project_road_mesh"), TEXT("cell_actors")) = ActualCells.Num();
	OutResult.MutableProducerMetric(TEXT("project_road_mesh"), TEXT("mesh_assets")) = ActualCells.Num();
	Inventory.Metrics.Add(TEXT("cell_actors"), ActualCells.Num());
	Inventory.Metrics.Add(TEXT("mesh_assets"), ActualCells.Num());
	return true;
}
