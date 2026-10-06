#include "ProjectWorldWaterLayerProducer.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldWaterRealization.h"
#include "Utilities/ProjectSha256.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "MaterialShared.h"

namespace
{
	void AppendToken(FString& Target, const FString& Value)
	{
		Target += FString::Printf(TEXT("%d:"), Value.Len()) + Value;
	}

	void AppendNumber(FString& Target, double Value)
	{
		AppendToken(Target, FString::Printf(TEXT("%.17g"), Value));
	}

	void AppendPoint(FString& Target, const FVector2D& Point)
	{
		AppendNumber(Target, Point.X);
		AppendNumber(Target, Point.Y);
	}

	void AppendPolygon(FString& Target, const FProjectWorldCanonicalPolygon& Polygon)
	{
		AppendToken(Target, FString::FromInt(Polygon.Outer.Num()));
		for (const FVector2D& Point : Polygon.Outer)
		{
			AppendPoint(Target, Point);
		}
		AppendToken(Target, FString::FromInt(Polygon.Holes.Num()));
		for (const TArray<FVector2D>& Hole : Polygon.Holes)
		{
			AppendToken(Target, FString::FromInt(Hole.Num()));
			for (const FVector2D& Point : Hole)
			{
				AppendPoint(Target, Point);
			}
		}
	}

	bool HashText(const FString& Text, FString& OutHash)
	{
		FTCHARToUTF8 Utf8(*Text);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return FProjectSha256::HashBuffer(Bytes, OutHash);
	}

	bool HashCellInput(const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell, FString& OutHash)
	{
		TSet<FString> FeatureIds;
		FeatureIds.Append(Cell.OwnedFeatureIds);
		FeatureIds.Append(Cell.ReferencedFeatureIds);
		TArray<FString> SortedIds = FeatureIds.Array();
		SortedIds.Sort();
		FString Identity(TEXT("project_water_cell_input_v1"));
		AppendToken(Identity, Cell.CellId);
		AppendNumber(Identity, Cell.Bounds.X);
		AppendNumber(Identity, Cell.Bounds.Y);
		AppendNumber(Identity, Cell.Bounds.Z);
		AppendNumber(Identity, Cell.Bounds.W);
		AppendNumber(Identity, Bundle.EngineGeoreferenceOriginMeters.X);
		AppendNumber(Identity, Bundle.EngineGeoreferenceOriginMeters.Y);
		AppendNumber(Identity, Bundle.CoordinateQuantizationMeters);
		AppendNumber(Identity, Bundle.HeightQuantizationMeters);
		AppendNumber(Identity, Bundle.HeightOriginMeters);
		for (const FString& FeatureId : SortedIds)
		{
			const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
			if (Feature == nullptr || Feature->FeatureClass != TEXT("water"))
			{
				continue;
			}
			AppendToken(Identity, Feature->FeatureId);
			AppendNumber(Identity, Feature->WidthMeters);
			AppendToken(Identity, Feature->WaterSurface.bValid ? TEXT("1") : TEXT("0"));
			AppendToken(Identity, Feature->WaterSurface.SurfaceGroupId);
			AppendToken(Identity, Feature->WaterSurface.Geometry);
			AppendToken(Identity, Feature->WaterSurface.Behavior);
			AppendToken(Identity, Feature->WaterSurface.FunctionId);
			AppendToken(Identity, FString::FromInt(Feature->WaterSurface.FunctionVersion));
			AppendNumber(Identity, Feature->WaterSurface.LevelMeters);
			AppendToken(Identity, FString::FromInt(Feature->WaterSurface.Knots.Num()));
			for (const FVector& Knot : Feature->WaterSurface.Knots)
			{
				AppendNumber(Identity, Knot.X);
				AppendNumber(Identity, Knot.Y);
				AppendNumber(Identity, Knot.Z);
			}
			AppendToken(Identity, FString::FromInt(Feature->GeometryParts.Num()));
			for (const TArray<FVector2D>& Part : Feature->GeometryParts)
			{
				AppendToken(Identity, FString::FromInt(Part.Num()));
				for (const FVector2D& Point : Part)
				{
					AppendPoint(Identity, Point);
				}
			}
			AppendToken(Identity, FString::FromInt(Feature->GeometryPolygons.Num()));
			for (const FProjectWorldCanonicalPolygon& Polygon : Feature->GeometryPolygons)
			{
				AppendPolygon(Identity, Polygon);
			}
		}
		return HashText(Identity, OutHash);
	}
}

FProjectWorldWaterLayerProducer& FProjectWorldWaterLayerProducer::Get()
{
	static FProjectWorldWaterLayerProducer Instance;
	return Instance;
}

FProjectWorldWaterLayerProducer::FProjectWorldWaterLayerProducer()
{
	Declaration.GeneratorId = TEXT("project_water_mesh");
	Declaration.GeneratorVersion = 1;
	Declaration.CanonicalSelectors = {TEXT("water")};
	Declaration.SpatialOwnership = TEXT("cell_local");
	Declaration.RuntimeMapping = TEXT("world_partition_spatial");
	Declaration.OwnedActors.OwnedTags = {FName(TEXT("ProjectWorld.Water.v1"))};
	Declaration.OwnedActors.CellTagPrefix = TEXT("ProjectWorld.WaterCell=");
}

const FProjectWorldLayerProducerDeclaration& FProjectWorldWaterLayerProducer::GetDeclaration() const
{
	return Declaration;
}

bool FProjectWorldWaterLayerProducer::ValidateSettings(
	const FString& NormalizedSettings, FString& OutError) const
{
	return ProjectWorldWaterRealization::ValidateSettings(NormalizedSettings, OutError);
}

bool FProjectWorldWaterLayerProducer::HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
	FProjectWorldLayerUnitInputs& OutUnits, FString& OutError) const
{
	for (const FProjectWorldCanonicalCell& Cell : Inputs.Bundle.Cells)
	{
		FString Hash;
		if (!HashCellInput(Inputs.Bundle, Cell, Hash))
		{
			OutError = FString::Printf(TEXT("Cannot hash water input for cell: %s"), *Cell.CellId);
			return false;
		}
		OutUnits.HashByUnit.Add(Cell.CellId, Hash);
	}
	return true;
}

bool FProjectWorldWaterLayerProducer::Apply(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	if (!ProjectWorldWaterRealization::Apply(
		&Context.World, Context.Inputs.Bundle, Context.Inputs.Profile, OutResult, OutError))
	{
		return false;
	}
	Inventory.Metrics.Add(TEXT("triangles_written"), OutResult.MutableProducerMetric(TEXT("project_water_mesh"), TEXT("triangles_written")));
	return true;
}

bool FProjectWorldWaterLayerProducer::CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	TSet<FString> CanonicalCellIds;
	for (const FProjectWorldCanonicalCell& Cell : Context.Inputs.Bundle.Cells)
	{
		CanonicalCellIds.Add(Cell.CellId);
	}
	FString MaterialSemantic;
	if (!HashText(TEXT("project_water_material_v3|") + Inventory.NormalizedLayerContractHash,
		MaterialSemantic) ||
		!ProjectWorldLayerProducerRegistry::AddPackageArtifact(
			Inventory.ArtifactRoot + TEXT("M_ProjectWorldWater"), TEXT("asset"),
			MaterialSemantic, Inventory, OutError))
	{
		return false;
	}
	TSet<FString> ActualCells;
	for (TActorIterator<AStaticMeshActor> It(&Context.World); It; ++It)
	{
		FString CellId;
		FString MeshSemantic;
		if (!ProjectWorldWaterRealization::ReadActorIdentity(*It, CellId, MeshSemantic))
		{
			continue;
		}
		UStaticMeshComponent* Component = It->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
		FString Failure;
		if (!CanonicalCellIds.Contains(CellId)) Failure = TEXT("cell is outside the canonical target");
		else if (ActualCells.Contains(CellId)) Failure = TEXT("cell actor is duplicated");
		else if (Mesh == nullptr) Failure = TEXT("StaticMesh is missing");
		else if (Mesh->GetNaniteSettings().bEnabled) Failure = TEXT("water mesh has Nanite enabled");
		else if (!It->GetIsSpatiallyLoaded()) Failure = TEXT("actor is not spatially loaded");
		else if (It->bEnableAutoLODGeneration) Failure = TEXT("actor permits HLOD generation");
		else if (It->GetHLODLayer() != nullptr) Failure = TEXT("actor has an HLOD layer");
		else if (Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			Failure = TEXT("water collision is enabled");
		else if (Mesh->GetStaticMaterials().IsEmpty()) Failure = TEXT("water material slot is missing");
		else if (Mesh->GetStaticMaterials()[0].MaterialInterface == nullptr)
			Failure = TEXT("water material is missing");
		else if (!Mesh->GetStaticMaterials()[0].MaterialInterface->GetShadingModels().HasShadingModel(MSM_DefaultLit) ||
			Mesh->GetStaticMaterials()[0].MaterialInterface->GetBlendMode() != BLEND_Opaque)
			Failure = TEXT("material is not solid opaque Default Lit");
		if (!Failure.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Persistent water ownership is invalid for cell %s: %s"),
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
		if (!HashText(FString::Printf(TEXT("project_water_actor_v1|%s|%s|%.17g,%.17g,%.17g"),
			*CellId, *MeshSemantic, It->GetActorLocation().X,
			It->GetActorLocation().Y, It->GetActorLocation().Z), ActorSemantic) ||
			!ProjectWorldLayerProducerRegistry::AddPackageArtifact(
				It->GetPackage()->GetName(), TEXT("external_actor"), ActorSemantic, Inventory, OutError))
		{
			return false;
		}
	}
	OutResult.MutableProducerMetric(TEXT("project_water_mesh"), TEXT("cell_actors")) = ActualCells.Num();
	OutResult.MutableProducerMetric(TEXT("project_water_mesh"), TEXT("mesh_assets")) = ActualCells.Num();
	Inventory.Metrics.Add(TEXT("cell_actors"), ActualCells.Num());
	Inventory.Metrics.Add(TEXT("mesh_assets"), ActualCells.Num());
	return true;
}
