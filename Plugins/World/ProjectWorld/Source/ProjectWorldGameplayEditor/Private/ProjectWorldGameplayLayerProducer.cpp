#include "ProjectWorldGameplayLayerProducer.h"

#include "ProjectWorldGameplayPlacement.h"

#include "Dom/JsonObject.h"

FProjectWorldGameplayLayerProducer& FProjectWorldGameplayLayerProducer::Get()
{
	static FProjectWorldGameplayLayerProducer Instance;
	return Instance;
}

FProjectWorldGameplayLayerProducer::FProjectWorldGameplayLayerProducer()
{
	Declaration.GeneratorId = TEXT("project_gameplay_placement");
	Declaration.GeneratorVersion = 1;
	Declaration.LayerKind = EProjectWorldLayerKind::GeneratedGameplayPlacement;
	Declaration.DirtyGranularity = EProjectWorldDirtyGranularity::ObjectId;
	Declaration.CanonicalSelectors = {TEXT("gameplay_placements")};
	Declaration.SpatialOwnership = TEXT("object_local");
	Declaration.RuntimeMapping = TEXT("world_partition_spatial");
	Declaration.DependencyHaloCells = 0;
	Declaration.PermittedDependencies = TArray<FString>{TEXT("terrain")};
	Declaration.OwnedActors.OwnedTags = {FName(TEXT("ProjectWorld.GameplayPlacement.v1"))};
	Declaration.OwnedActors.CellTagPrefix = TEXT("ProjectWorld.Cell=");
}

const FProjectWorldLayerProducerDeclaration& FProjectWorldGameplayLayerProducer::GetDeclaration() const
{
	return Declaration;
}

bool FProjectWorldGameplayLayerProducer::ValidateSettings(
	const FString& NormalizedSettings, FString& OutError) const
{
	static const TCHAR* Fields[] = {
		TEXT("placement_source"), TEXT("surface_policy"), TEXT("runtime_state_policy")};
	TSharedPtr<FJsonObject> Settings;
	if (!ProjectWorldLayerProducerRegistry::ParseExactSettings(
		NormalizedSettings, MakeArrayView(Fields), Settings, OutError))
	{
		return false;
	}
	FString Source;
	FString SurfacePolicy;
	FString RuntimeStatePolicy;
	const bool bValid = Settings->TryGetStringField(TEXT("placement_source"), Source) &&
		Source.StartsWith(TEXT("GameplayPlacement/")) && Source.EndsWith(TEXT(".json")) &&
		!Source.Contains(TEXT("..")) && !Source.Contains(TEXT("\\")) &&
		Settings->TryGetStringField(TEXT("surface_policy"), SurfacePolicy) &&
		SurfacePolicy == TEXT("canonical_terrain_snap") &&
		Settings->TryGetStringField(TEXT("runtime_state_policy"), RuntimeStatePolicy) &&
		RuntimeStatePolicy == TEXT("external_to_generation");
	if (!bValid)
	{
		OutError = TEXT("Gameplay placement settings do not match the producer contract.");
	}
	return bValid;
}

bool FProjectWorldGameplayLayerProducer::HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
	FProjectWorldLayerUnitInputs& OutUnits, FString& OutError) const
{
	FProjectWorldGameplayPlacementSet Set;
	if (!ProjectWorldGameplayPlacement::Load(Inputs.Profile, Inputs.Layer, Set, OutError))
	{
		return false;
	}
	for (const FProjectWorldGameplayPlacement& Placement : Set.Placements)
	{
		FString CellId;
		FTransform Transform;
		FString Hash;
		if (!ProjectWorldGameplayPlacement::BuildInput(
			Inputs.Bundle, Inputs.Layer, Placement, CellId, Transform, Hash, OutError))
		{
			return false;
		}
		OutUnits.HashByUnit.Add(Placement.ObjectId, Hash);
		OutUnits.UnitsByCanonicalCell.FindOrAdd(CellId).Add(Placement.ObjectId);
	}
	return true;
}

bool FProjectWorldGameplayLayerProducer::Apply(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	if (!ProjectWorldGameplayPlacement::Apply(
		&Context.World, Context.Inputs.Bundle, Context.Inputs.Profile, OutResult, OutError))
	{
		return false;
	}
	Inventory.Metrics.Add(TEXT("placements_written"), OutResult.MutableProducerMetric(TEXT("project_gameplay_placement"), TEXT("placements_written")));
	return true;
}

bool FProjectWorldGameplayLayerProducer::CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	if (!ProjectWorldGameplayPlacement::Capture(&Context.World, Context.Inputs.Bundle,
		Context.Inputs.Profile, Inventory, OutResult, OutError))
	{
		return false;
	}
	Inventory.Metrics.Add(TEXT("placement_actors"), OutResult.MutableProducerMetric(TEXT("project_gameplay_placement"), TEXT("placement_actors")));
	return true;
}
