#include "ProjectWorldVegetationLayerProducer.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldVegetationRealization.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"

FProjectWorldVegetationLayerProducer& FProjectWorldVegetationLayerProducer::Get()
{
	static FProjectWorldVegetationLayerProducer Instance;
	return Instance;
}

FProjectWorldVegetationLayerProducer::FProjectWorldVegetationLayerProducer()
{
	Declaration.GeneratorId = TEXT("project_vegetation_instances");
	Declaration.GeneratorVersion = 1;
	Declaration.CanonicalSelectors = {TEXT("vegetation")};
	Declaration.SpatialOwnership = TEXT("cell_local");
	Declaration.RuntimeMapping = TEXT("world_partition_spatial");
	Declaration.DependencyHaloCells = 0;
	Declaration.PermittedDependencies = TArray<FString>{TEXT("terrain"), TEXT("water"), TEXT("roads")};
	Declaration.OwnedActors.OwnedTags = {FName(TEXT("ProjectWorld.Vegetation=v1"))};
	Declaration.OwnedActors.CellTagPrefix = TEXT("ProjectWorld.VegetationCell=");
}

const FProjectWorldLayerProducerDeclaration& FProjectWorldVegetationLayerProducer::GetDeclaration() const
{
	return Declaration;
}

bool FProjectWorldVegetationLayerProducer::ValidateSettings(
	const FString& NormalizedSettings, FString& OutError) const
{
	static const TCHAR* Fields[] = {
		TEXT("mesh_assets"), TEXT("area_spacing_m"), TEXT("area_jitter_fraction"),
		TEXT("minimum_scale"), TEXT("maximum_scale"), TEXT("maximum_instances_per_cell"),
		TEXT("deterministic_seed"), TEXT("surface_offset_m"), TEXT("nanite"),
		TEXT("collision"), TEXT("placement_policy")};
	TSharedPtr<FJsonObject> Settings;
	if (!ProjectWorldLayerProducerRegistry::ParseExactSettings(
		NormalizedSettings, MakeArrayView(Fields), Settings, OutError))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Meshes = nullptr;
	TSet<FString> UniqueMeshes;
	if (!Settings->TryGetArrayField(TEXT("mesh_assets"), Meshes) || Meshes == nullptr || Meshes->IsEmpty())
	{
		OutError = TEXT("Vegetation mesh_assets must contain distinct paths.");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Meshes)
	{
		FString Path;
		if (!Value.IsValid() || !Value->TryGetString(Path) || !Path.StartsWith(TEXT("/Project")) ||
			!Path.Contains(TEXT(".")) || UniqueMeshes.Contains(Path))
		{
			OutError = TEXT("Vegetation mesh_assets must contain distinct project asset paths.");
			return false;
		}
		UniqueMeshes.Add(Path);
	}
	double Spacing = 0.0, Jitter = 0.0, MinimumScale = 0.0, MaximumScale = 0.0;
	double MaximumInstances = 0.0, Seed = 0.0, SurfaceOffset = 0.0;
	bool bNanite = false;
	FString Collision, PlacementPolicy;
	const bool bValid =
		Settings->TryGetNumberField(TEXT("area_spacing_m"), Spacing) && Spacing >= 5.0 && Spacing <= 100.0 &&
		Settings->TryGetNumberField(TEXT("area_jitter_fraction"), Jitter) && Jitter >= 0.0 && Jitter < 0.5 &&
		Settings->TryGetNumberField(TEXT("minimum_scale"), MinimumScale) && MinimumScale > 0.0 &&
		Settings->TryGetNumberField(TEXT("maximum_scale"), MaximumScale) && MaximumScale >= MinimumScale && MaximumScale <= 3.0 &&
		Settings->TryGetNumberField(TEXT("maximum_instances_per_cell"), MaximumInstances) &&
		FMath::IsFinite(MaximumInstances) && MaximumInstances >= 1.0 && MaximumInstances <= 8192.0 &&
		FMath::Floor(MaximumInstances) == MaximumInstances &&
		Settings->TryGetNumberField(TEXT("deterministic_seed"), Seed) &&
		FMath::IsFinite(Seed) && Seed >= 0.0 && Seed <= 2147483647.0 && FMath::Floor(Seed) == Seed &&
		Settings->TryGetNumberField(TEXT("surface_offset_m"), SurfaceOffset) && SurfaceOffset >= 0.0 && SurfaceOffset <= 2.0 &&
		Settings->TryGetBoolField(TEXT("nanite"), bNanite) && bNanite &&
		Settings->TryGetStringField(TEXT("collision"), Collision) && Collision == TEXT("no_collision") &&
		Settings->TryGetStringField(TEXT("placement_policy"), PlacementPolicy) &&
		PlacementPolicy == TEXT("canonical_points_and_lattice_areas");
	if (!bValid)
	{
		OutError = TEXT("Vegetation settings do not match the executable v1 tuple.");
	}
	return bValid;
}

bool FProjectWorldVegetationLayerProducer::HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
	FProjectWorldLayerUnitInputs& OutUnits, FString& OutError) const
{
	for (const FProjectWorldCanonicalCell& Cell : Inputs.Bundle.Cells)
	{
		FString Hash;
		if (!ProjectWorldVegetationRealization::HashCellInput(
			Inputs.Bundle, Cell, Inputs.Layer, Inputs.Profile, Inputs.AuthoredOverlays, Hash, OutError))
		{
			return false;
		}
		OutUnits.HashByUnit.Add(Cell.CellId, Hash);
	}
	return true;
}

bool FProjectWorldVegetationLayerProducer::Apply(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	if (!ProjectWorldVegetationRealization::Apply(
		&Context.World, Context.Inputs.Bundle, Context.Inputs.Profile,
		Context.Inputs.AuthoredOverlays, OutResult, OutError))
	{
		return false;
	}
	Inventory.Metrics.Add(TEXT("instances_written"), OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("instances_written")));
	return true;
}

bool FProjectWorldVegetationLayerProducer::CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("candidates")) = 0;
	OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("road_exclusions")) = 0;
	OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("water_exclusions")) = 0;
	OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("authored_mask_exclusions")) = 0;
	TSet<FString> ExpectedCells;
	for (const FProjectWorldCanonicalCell& Cell : Context.Inputs.Bundle.Cells)
	{
		TArray<FProjectWorldVegetationInstance> Instances;
		FProjectWorldVegetationPlacementStats Stats;
		FString Semantic;
		if (!ProjectWorldVegetationRealization::BuildCellInstances(
			Context.Inputs.Bundle, Cell, Context.Inputs.Layer, Context.Inputs.Profile,
			Context.Inputs.AuthoredOverlays, Instances, &Stats, Semantic, OutError))
		{
			return false;
		}
		OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("candidates")) += Stats.CandidateCount;
		OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("road_exclusions")) += Stats.RoadExcludedCount;
		OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("water_exclusions")) += Stats.WaterExcludedCount;
		OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("authored_mask_exclusions")) += Stats.AuthoredMaskExcludedCount;
		if (!Instances.IsEmpty())
		{
			ExpectedCells.Add(Cell.CellId);
		}
	}
	TSet<FString> ActualCells;
	for (TActorIterator<AActor> It(&Context.World); It; ++It)
	{
		FString CellId;
		FString Semantic;
		if (!ProjectWorldVegetationRealization::ReadActorIdentity(*It, CellId, Semantic))
		{
			continue;
		}
		TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
		It->GetComponents(Components);
		FString Failure;
		int32 ActorInstances = 0;
		if (!ExpectedCells.Contains(CellId)) Failure = TEXT("cell has no canonical vegetation");
		else if (ActualCells.Contains(CellId)) Failure = TEXT("cell actor is duplicated");
		else if (Components.IsEmpty()) Failure = TEXT("actor has no HISM components");
		else if (!It->GetIsSpatiallyLoaded()) Failure = TEXT("actor is not spatially loaded");
		else if (It->bEnableAutoLODGeneration || It->GetHLODLayer() != nullptr)
			Failure = TEXT("actor participates in HLOD");
		for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
		{
			UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
			if (Mesh == nullptr || !Mesh->GetNaniteSettings().bEnabled ||
				Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
				Component->CanEverAffectNavigation() || Component->GetInstanceCount() <= 0)
			{
				Failure = TEXT("HISM component violates mesh, Nanite, collision, navigation, or population policy");
				break;
			}
			ActorInstances += Component->GetInstanceCount();
		}
		if (!Failure.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Persistent vegetation ownership is invalid for cell %s: %s"),
				*CellId, *Failure);
			return false;
		}
		ActualCells.Add(CellId);
		OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("components")) += Components.Num();
		OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("instances")) += ActorInstances;
		if (!ProjectWorldLayerProducerRegistry::AddPackageArtifact(
			It->GetPackage()->GetName(), TEXT("external_actor"), Semantic, Inventory, OutError))
		{
			return false;
		}
	}
	if (ActualCells.Num() != ExpectedCells.Num())
	{
		OutError = TEXT("Vegetation cell inventory does not exactly cover canonical vegetation.");
		return false;
	}
	OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("cell_actors")) = ActualCells.Num();
	Inventory.Metrics.Add(TEXT("cell_actors"), ActualCells.Num());
	Inventory.Metrics.Add(TEXT("components"), OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("components")));
	Inventory.Metrics.Add(TEXT("instances"), OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("instances")));
	Inventory.Metrics.Add(TEXT("candidates"), OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("candidates")));
	Inventory.Metrics.Add(TEXT("road_exclusions"), OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("road_exclusions")));
	Inventory.Metrics.Add(TEXT("water_exclusions"), OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("water_exclusions")));
	Inventory.Metrics.Add(TEXT("authored_mask_exclusions"), OutResult.MutableProducerMetric(TEXT("project_vegetation_instances"), TEXT("authored_mask_exclusions")));
	return true;
}
