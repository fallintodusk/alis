#include "ProjectWorldBuildingLayerProducer.h"

#include "ProjectWorldBuildingInventory.h"
#include "ProjectWorldBuildingRealization.h"
#include "ProjectWorldCanonicalBundle.h"

#include "Dom/JsonObject.h"

FProjectWorldBuildingLayerProducer& FProjectWorldBuildingLayerProducer::Get()
{
	static FProjectWorldBuildingLayerProducer Producer;
	return Producer;
}

FProjectWorldBuildingLayerProducer::FProjectWorldBuildingLayerProducer()
{
	Declaration.GeneratorId = TEXT("project_building_massing");
	Declaration.GeneratorVersion = 2;
	Declaration.CanonicalSelectors = {TEXT("buildings")};
	Declaration.SpatialOwnership = TEXT("cell_local");
	Declaration.RuntimeMapping = TEXT("world_partition_spatial");
	Declaration.DependencyHaloCells = 0;
	Declaration.PermittedDependencies = TArray<FString>{TEXT("terrain")};
	Declaration.OwnedActors.OwnedTags = {FName(TEXT("ProjectWorld.BuildingMassing.v2"))};
	Declaration.OwnedActors.CellTagPrefix = TEXT("ProjectWorld.BuildingCell=");
}

const FProjectWorldLayerProducerDeclaration& FProjectWorldBuildingLayerProducer::GetDeclaration() const
{
	return Declaration;
}

bool FProjectWorldBuildingLayerProducer::ValidateSettings(
	const FString& NormalizedSettings, FString& OutError) const
{
	static const TCHAR* Fields[] = {
		TEXT("maximum_height_m"), TEXT("terrain_anchor_policy"), TEXT("topology_policy"),
		TEXT("duplicate_policy"), TEXT("contained_policy"), TEXT("conflict_policy"),
		TEXT("nanite"), TEXT("collision"), TEXT("navigation")};
	TSharedPtr<FJsonObject> Settings;
	if (!ProjectWorldLayerProducerRegistry::ParseExactSettings(
		NormalizedSettings, MakeArrayView(Fields), Settings, OutError))
	{
		return false;
	}
	double MaximumHeight = 0.0;
	bool bNanite = false;
	FString AnchorPolicy, TopologyPolicy, DuplicatePolicy, ContainedPolicy;
	FString ConflictPolicy, Collision, Navigation;
	const bool bValid =
		Settings->TryGetNumberField(TEXT("maximum_height_m"), MaximumHeight) &&
		MaximumHeight >= 50.0 && MaximumHeight <= 1000.0 &&
		Settings->TryGetStringField(TEXT("terrain_anchor_policy"), AnchorPolicy) &&
		AnchorPolicy == TEXT("owner_cell_clamped_bounds_center") &&
		Settings->TryGetStringField(TEXT("topology_policy"), TopologyPolicy) &&
		TopologyPolicy == TEXT("logical_building_classify_v2") &&
		Settings->TryGetStringField(TEXT("duplicate_policy"), DuplicatePolicy) &&
		DuplicatePolicy == TEXT("stable_feature_id") &&
		Settings->TryGetStringField(TEXT("contained_policy"), ContainedPolicy) &&
		ContainedPolicy == TEXT("associate_with_container") &&
		Settings->TryGetStringField(TEXT("conflict_policy"), ConflictPolicy) &&
		ConflictPolicy == TEXT("reject_affected_fragments") &&
		Settings->TryGetBoolField(TEXT("nanite"), bNanite) && bNanite &&
		Settings->TryGetStringField(TEXT("collision"), Collision) && Collision == TEXT("complex_as_simple") &&
		Settings->TryGetStringField(TEXT("navigation"), Navigation) && Navigation == TEXT("no_navigation");
	if (!bValid)
	{
		OutError = TEXT("Building settings do not match the executable tuple.");
	}
	return bValid;
}

bool FProjectWorldBuildingLayerProducer::HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
	FProjectWorldLayerUnitInputs& OutUnits, FString& OutError) const
{
	for (const FProjectWorldCanonicalCell& Cell : Inputs.Bundle.Cells)
	{
		FString Hash;
		if (!ProjectWorldBuildingRealization::HashCellInput(
			Inputs.Bundle, Cell, Inputs.Layer, Inputs.AuthoredOverlays, Hash, OutError))
		{
			return false;
		}
		OutUnits.HashByUnit.Add(Cell.CellId, Hash);
	}
	return true;
}

bool FProjectWorldBuildingLayerProducer::Apply(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	UMaterialInterface* const* Material = Context.PresentationMaterials.Find(TEXT("building"));
	if (!ProjectWorldBuildingRealization::Apply(&Context.World, Context.Inputs.Bundle,
		Context.Inputs.Profile, Context.Inputs.AuthoredOverlays,
		Material != nullptr ? *Material : nullptr, OutResult, OutError))
	{
		return false;
	}
	Inventory.Metrics.Add(TEXT("triangles_written"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("triangles_written")));
	return true;
}

bool FProjectWorldBuildingLayerProducer::CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	if (!ProjectWorldBuildingInventory::Capture(&Context.World, Context.Inputs.Bundle,
		Context.Inputs.Layer, Context.Inputs.AuthoredOverlays, Inventory, OutResult, OutError))
	{
		return false;
	}
	Inventory.Metrics.Add(TEXT("cell_actors"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("cell_actors")));
	Inventory.Metrics.Add(TEXT("mesh_assets"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("mesh_assets")));
	Inventory.Metrics.Add(TEXT("triangles"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("triangles")));
	Inventory.Metrics.Add(TEXT("candidate_fragments"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("candidate_fragments")));
	Inventory.Metrics.Add(TEXT("accepted_fragments"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("accepted_fragments")));
	Inventory.Metrics.Add(TEXT("duplicate_fragments"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("duplicate_fragments")));
	Inventory.Metrics.Add(TEXT("contained_fragments"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("contained_fragments")));
	Inventory.Metrics.Add(TEXT("conflict_fragments"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("conflict_fragments")));
	Inventory.Metrics.Add(TEXT("malformed_fragments"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("malformed_fragments")));
	Inventory.Metrics.Add(TEXT("authored_mask_exclusions"), OutResult.MutableProducerMetric(TEXT("project_building_massing"), TEXT("authored_mask_exclusions")));
	return true;
}
