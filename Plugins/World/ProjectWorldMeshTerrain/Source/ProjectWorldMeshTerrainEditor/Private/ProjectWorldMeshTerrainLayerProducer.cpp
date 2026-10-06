#include "ProjectWorldMeshTerrainLayerProducer.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldMeshTerrainProducer.h"

#include "Dom/JsonObject.h"

FProjectWorldMeshTerrainLayerProducer& FProjectWorldMeshTerrainLayerProducer::Get()
{
	static FProjectWorldMeshTerrainLayerProducer Instance;
	return Instance;
}

FProjectWorldMeshTerrainLayerProducer::FProjectWorldMeshTerrainLayerProducer()
{
	Declaration.GeneratorId = TEXT("project_mesh_terrain");
	Declaration.GeneratorVersion = 1;
	Declaration.CanonicalSelectors = {TEXT("terrain")};
	Declaration.SpatialOwnership = TEXT("compiled_sections_from_canonical_cells");
	Declaration.RuntimeMapping = TEXT("world_partition_spatial");
	Declaration.DependencyHaloCells = 0;
	Declaration.OwnedActors.OwnedTags = {FName(TEXT("ProjectWorld.MeshTerrain.Authoring.v1"))};
	Declaration.OwnedActors.CellTagPrefix = TEXT("ProjectWorld.MeshTerrain.Cell=");
	Declaration.OwnedActors.bCellExtentActors = true;
	FProjectWorldPostApplyBuilder Builder;
	Builder.InventoryCommandlet = TEXT("ProjectWorldMeshTerrainAudit");
	Builder.InventoryArguments = {TEXT("-ExpectedSectionsPerVariant=0"), TEXT("-AllowCommandletRendering")};
	Builder.InventorySchema = TEXT("project-world-mesh-terrain-audit:v2");
	Builder.BuilderCommandlet = TEXT("WorldPartitionBuilderCommandlet");
	Builder.BuilderArguments = {
		TEXT("-Builder=WorldPartitionMeshPartitionBuilder"), TEXT("-AllowCommandletRendering"),
		TEXT("-ini:Engine:[SystemSettings]:r.VolumetricFog=0"), TEXT("-SCCProvider=None"),
		TEXT("-NoAssetRegistryCache")};
	Declaration.PostApplyBuilder = MoveTemp(Builder);
}

const FProjectWorldLayerProducerDeclaration& FProjectWorldMeshTerrainLayerProducer::GetDeclaration() const
{
	return Declaration;
}

bool FProjectWorldMeshTerrainLayerProducer::ValidateSettings(
	const FString& NormalizedSettings, FString& OutError) const
{
	static const TCHAR* Fields[] = {
		TEXT("shared_definition"), TEXT("surface_contract_id"), TEXT("surface_contract_version"),
		TEXT("section_max_complexity"), TEXT("channel_texel_size_cm"),
		TEXT("channel_texture_max_dimension"), TEXT("collision"), TEXT("render_variants")};
	TSharedPtr<FJsonObject> Settings;
	if (!ProjectWorldLayerProducerRegistry::ParseExactSettings(
		NormalizedSettings, MakeArrayView(Fields), Settings, OutError))
	{
		return false;
	}
	FString DefinitionPath;
	FString SurfaceContract;
	FString Collision;
	double SurfaceVersion = 0.0;
	double MaxComplexity = 0.0;
	double TexelSize = 0.0;
	double MaxTexture = 0.0;
	const TArray<TSharedPtr<FJsonValue>>* Variants = nullptr;
	FString FirstVariant;
	FString SecondVariant;
	const bool bValid = Settings->TryGetStringField(TEXT("shared_definition"), DefinitionPath) &&
		DefinitionPath == ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath &&
		Settings->TryGetStringField(TEXT("surface_contract_id"), SurfaceContract) &&
		SurfaceContract == TEXT("terrain_surface_semantics") &&
		Settings->TryGetNumberField(TEXT("surface_contract_version"), SurfaceVersion) && SurfaceVersion == 1.0 &&
		Settings->TryGetNumberField(TEXT("section_max_complexity"), MaxComplexity) && MaxComplexity == 2048.0 &&
		Settings->TryGetNumberField(TEXT("channel_texel_size_cm"), TexelSize) && TexelSize == 3000.0 &&
		Settings->TryGetNumberField(TEXT("channel_texture_max_dimension"), MaxTexture) && MaxTexture == 4096.0 &&
		Settings->TryGetStringField(TEXT("collision"), Collision) && Collision == TEXT("complex_as_simple") &&
		Settings->TryGetArrayField(TEXT("render_variants"), Variants) && Variants != nullptr &&
		Variants->Num() == 2 && (*Variants)[0]->TryGetString(FirstVariant) &&
		(*Variants)[1]->TryGetString(SecondVariant) &&
		FirstVariant == TEXT("nanite") && SecondVariant == TEXT("fallback");
	if (!bValid)
	{
		OutError = TEXT("Mesh Terrain settings do not match the producer contract.");
	}
	return bValid;
}

bool FProjectWorldMeshTerrainLayerProducer::HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
	FProjectWorldLayerUnitInputs& OutUnits, FString&) const
{
	for (const FProjectWorldCanonicalCell& Cell : Inputs.Bundle.Cells)
	{
		OutUnits.HashByUnit.Add(Cell.CellId, Cell.Terrain.ArtifactHash);
	}
	return true;
}

bool FProjectWorldMeshTerrainLayerProducer::Apply(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult, FString& OutError) const
{
	UMaterialInterface* const* Material = Context.PresentationMaterials.Find(TEXT("terrain"));
	return ProjectWorldMeshTerrainProducer::ApplyLayer(&Context.World, Context.Inputs.Bundle,
		Context.Inputs.Layer.NormalizedSettings, Material != nullptr ? *Material : nullptr,
		Inventory.GeneratorFingerprint, OutResult, OutError);
}

bool FProjectWorldMeshTerrainLayerProducer::CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
	FString& OutError) const
{
	if (!ProjectWorldMeshTerrainProducer::CaptureLayer(
		&Context.World, Context.Inputs.Bundle, Inventory, OutResult, OutError))
	{
		return false;
	}
	Inventory.Metrics.Add(TEXT("sections"), OutResult.TerrainSectionCount);
	return true;
}

bool FProjectWorldMeshTerrainLayerProducer::Delete(const FProjectWorldLayerApplyContext& Context,
	FProjectWorldRealizationResult& OutResult, FString& OutError) const
{
	return ProjectWorldMeshTerrainProducer::DeleteLayer(
		&Context.World, Context.Inputs.Bundle, OutResult, OutError);
}
