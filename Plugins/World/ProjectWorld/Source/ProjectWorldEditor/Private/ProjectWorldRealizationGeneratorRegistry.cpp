// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRealizationGeneratorRegistry.h"

#include "ProjectWorldRealizationProfile.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ProjectWorldRealizationGeneratorRegistry
{
	namespace
	{
		bool HasOnlyFields(
			const TSharedPtr<FJsonObject>& Object,
			std::initializer_list<const TCHAR*> Allowed,
			FString& OutError)
		{
			TSet<FString> AllowedFields;
			for (const TCHAR* Field : Allowed)
			{
				AllowedFields.Add(Field);
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
			{
				if (!AllowedFields.Contains(Field.Key))
				{
					OutError = FString::Printf(TEXT("Unknown generator setting: %s"), *Field.Key);
					return false;
				}
			}
			return Object->Values.Num() == AllowedFields.Num();
		}

		bool HasSingleSelector(const FProjectWorldRealizationLayer& Layer, const TCHAR* Selector)
		{
			return Layer.CanonicalSelectors.Num() == 1 && Layer.CanonicalSelectors[0] == Selector;
		}

		bool HasNonnegativeRgb(const TSharedPtr<FJsonObject>& Settings, const TCHAR* Field)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!Settings->TryGetArrayField(Field, Values) || Values == nullptr || Values->Num() != 3)
			{
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				double Number = 0.0;
				if (!Value.IsValid() || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number) || Number < 0.0)
				{
					return false;
				}
			}
			return true;
		}

		bool HasUniqueStringArray(const TSharedPtr<FJsonObject>& Settings, const TCHAR* Field)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			TSet<FString> UniqueValues;
			if (!Settings->TryGetArrayField(Field, Values) || Values == nullptr || Values->IsEmpty())
			{
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				FString Text;
				if (!Value.IsValid() || !Value->TryGetString(Text) || Text.IsEmpty() || UniqueValues.Contains(Text))
				{
					return false;
				}
				UniqueValues.Add(Text);
			}
			return true;
		}

		bool HasMeshAssets(const TSharedPtr<FJsonObject>& Settings)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			TSet<FString> UniqueValues;
			if (!Settings->TryGetArrayField(TEXT("mesh_assets"), Values) || Values == nullptr || Values->IsEmpty())
			{
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				FString Path;
				if (!Value.IsValid() || !Value->TryGetString(Path) || !Path.StartsWith(TEXT("/Project")) ||
					!Path.Contains(TEXT(".")) || UniqueValues.Contains(Path))
				{
					return false;
				}
				UniqueValues.Add(Path);
			}
			return true;
		}

		bool IsWholeNumber(double Value, double Minimum, double Maximum)
		{
			return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum && FMath::Floor(Value) == Value;
		}

		bool ValidateLandscape(const FProjectWorldRealizationLayer& Layer, const TSharedPtr<FJsonObject>& Settings, FString& OutError)
		{
			double ComponentsPerProxy = 0.0;
			return Layer.LayerKind == EProjectWorldLayerKind::GeneratedGeography &&
				HasSingleSelector(Layer, TEXT("terrain")) &&
				Layer.SpatialOwnership == TEXT("logical_landscape_with_cell_proxies") &&
				Layer.DirtyGranularity == EProjectWorldDirtyGranularity::CanonicalCell &&
				Layer.RuntimeMapping == TEXT("world_partition_owner") &&
				HasOnlyFields(Settings, {TEXT("components_per_proxy")}, OutError) &&
				Settings->TryGetNumberField(TEXT("components_per_proxy"), ComponentsPerProxy) &&
				ComponentsPerProxy == 1.0;
		}

		bool ValidateWater(const FProjectWorldRealizationLayer& Layer, const TSharedPtr<FJsonObject>& Settings, FString& OutError)
		{
			FString ShadingModel;
			bool bNanite = true;
			return Layer.LayerKind == EProjectWorldLayerKind::GeneratedGeography &&
				HasSingleSelector(Layer, TEXT("water")) && Layer.SpatialOwnership == TEXT("cell_local") &&
				Layer.DirtyGranularity == EProjectWorldDirtyGranularity::CanonicalCell &&
				Layer.RuntimeMapping == TEXT("world_partition_spatial") &&
				HasOnlyFields(Settings, {TEXT("material_shading_model"), TEXT("nanite"),
					TEXT("scattering_coefficients"), TEXT("absorption_coefficients")}, OutError) &&
				Settings->TryGetStringField(TEXT("material_shading_model"), ShadingModel) &&
				ShadingModel == TEXT("single_layer_water") &&
				Settings->TryGetBoolField(TEXT("nanite"), bNanite) && !bNanite &&
				HasNonnegativeRgb(Settings, TEXT("scattering_coefficients")) &&
				HasNonnegativeRgb(Settings, TEXT("absorption_coefficients"));
		}

		bool ValidateRoads(const FProjectWorldRealizationLayer& Layer, const TSharedPtr<FJsonObject>& Settings, FString& OutError)
		{
			double SurfaceOffset = 0.0;
			double MaximumSegmentLength = 0.0;
			bool bNanite = false;
			FString Collision;
			FString StructureFallback;
			FString IntersectionPolicy;
			return Layer.LayerKind == EProjectWorldLayerKind::GeneratedGeography &&
				HasSingleSelector(Layer, TEXT("roads")) && Layer.SpatialOwnership == TEXT("cell_local") &&
				Layer.DirtyGranularity == EProjectWorldDirtyGranularity::CanonicalCell &&
				Layer.RuntimeMapping == TEXT("world_partition_spatial") && Layer.DependencyHaloCells == 1 &&
				HasOnlyFields(Settings, {TEXT("selected_classes"), TEXT("surface_offset_m"),
					TEXT("maximum_segment_length_m"), TEXT("nanite"), TEXT("collision"),
					TEXT("structure_fallback"), TEXT("intersection_policy")}, OutError) &&
				HasUniqueStringArray(Settings, TEXT("selected_classes")) &&
				Settings->TryGetNumberField(TEXT("surface_offset_m"), SurfaceOffset) &&
				SurfaceOffset > 0.0 && SurfaceOffset <= 2.0 &&
				Settings->TryGetNumberField(TEXT("maximum_segment_length_m"), MaximumSegmentLength) &&
				MaximumSegmentLength > 0.0 && MaximumSegmentLength <= 30.0 &&
				Settings->TryGetBoolField(TEXT("nanite"), bNanite) && bNanite &&
				Settings->TryGetStringField(TEXT("collision"), Collision) && Collision == TEXT("complex_as_simple") &&
				Settings->TryGetStringField(TEXT("structure_fallback"), StructureFallback) && StructureFallback == TEXT("terrain_drape") &&
				Settings->TryGetStringField(TEXT("intersection_policy"), IntersectionPolicy) && IntersectionPolicy == TEXT("overlap_same_owner");
		}

		bool ValidateVegetation(const FProjectWorldRealizationLayer& Layer, const TSharedPtr<FJsonObject>& Settings, FString& OutError)
		{
			double Spacing = 0.0;
			double Jitter = 0.0;
			double MinimumScale = 0.0;
			double MaximumScale = 0.0;
			double MaximumInstances = 0.0;
			double Seed = 0.0;
			double SurfaceOffset = 0.0;
			bool bNanite = false;
			FString Collision;
			FString PlacementPolicy;
			return Layer.LayerKind == EProjectWorldLayerKind::GeneratedGeography &&
				HasSingleSelector(Layer, TEXT("vegetation")) && Layer.DependsOn.Num() == 3 &&
				Layer.DependsOn[0] == TEXT("terrain") && Layer.DependsOn[1] == TEXT("water") &&
				Layer.DependsOn[2] == TEXT("roads") && Layer.SpatialOwnership == TEXT("cell_local") &&
				Layer.DirtyGranularity == EProjectWorldDirtyGranularity::CanonicalCell &&
				Layer.RuntimeMapping == TEXT("world_partition_spatial") && Layer.DependencyHaloCells == 0 &&
				HasOnlyFields(Settings, {TEXT("mesh_assets"), TEXT("area_spacing_m"),
					TEXT("area_jitter_fraction"), TEXT("minimum_scale"), TEXT("maximum_scale"),
					TEXT("maximum_instances_per_cell"), TEXT("deterministic_seed"),
					TEXT("surface_offset_m"), TEXT("nanite"), TEXT("collision"),
					TEXT("placement_policy")}, OutError) && HasMeshAssets(Settings) &&
				Settings->TryGetNumberField(TEXT("area_spacing_m"), Spacing) && Spacing >= 5.0 && Spacing <= 100.0 &&
				Settings->TryGetNumberField(TEXT("area_jitter_fraction"), Jitter) && Jitter >= 0.0 && Jitter < 0.5 &&
				Settings->TryGetNumberField(TEXT("minimum_scale"), MinimumScale) && MinimumScale > 0.0 &&
				Settings->TryGetNumberField(TEXT("maximum_scale"), MaximumScale) && MaximumScale >= MinimumScale && MaximumScale <= 3.0 &&
				Settings->TryGetNumberField(TEXT("maximum_instances_per_cell"), MaximumInstances) && IsWholeNumber(MaximumInstances, 1.0, 8192.0) &&
				Settings->TryGetNumberField(TEXT("deterministic_seed"), Seed) && IsWholeNumber(Seed, 0.0, 2147483647.0) &&
				Settings->TryGetNumberField(TEXT("surface_offset_m"), SurfaceOffset) && SurfaceOffset >= 0.0 && SurfaceOffset <= 2.0 &&
				Settings->TryGetBoolField(TEXT("nanite"), bNanite) && bNanite &&
				Settings->TryGetStringField(TEXT("collision"), Collision) && Collision == TEXT("no_collision") &&
				Settings->TryGetStringField(TEXT("placement_policy"), PlacementPolicy) &&
				PlacementPolicy == TEXT("canonical_points_and_lattice_areas");
		}

		bool ValidateBuildings(const FProjectWorldRealizationLayer& Layer, const TSharedPtr<FJsonObject>& Settings, FString& OutError)
		{
			double MaximumHeight = 0.0;
			bool bNanite = false;
			FString AnchorPolicy;
			FString TopologyPolicy;
			FString DuplicatePolicy;
			FString ContainedPolicy;
			FString ConflictPolicy;
			FString Collision;
			FString Navigation;
			return Layer.LayerKind == EProjectWorldLayerKind::GeneratedGeography &&
				HasSingleSelector(Layer, TEXT("buildings")) && Layer.DependsOn.Num() == 1 &&
				Layer.DependsOn[0] == TEXT("terrain") && Layer.SpatialOwnership == TEXT("cell_local") &&
				Layer.DirtyGranularity == EProjectWorldDirtyGranularity::CanonicalCell &&
				Layer.RuntimeMapping == TEXT("world_partition_spatial") && Layer.DependencyHaloCells == 0 &&
				HasOnlyFields(Settings, {TEXT("maximum_height_m"), TEXT("terrain_anchor_policy"),
					TEXT("topology_policy"), TEXT("duplicate_policy"), TEXT("contained_policy"),
					TEXT("conflict_policy"), TEXT("nanite"), TEXT("collision"), TEXT("navigation")}, OutError) &&
				Settings->TryGetNumberField(TEXT("maximum_height_m"), MaximumHeight) &&
				MaximumHeight >= 50.0 && MaximumHeight <= 1000.0 &&
				Settings->TryGetStringField(TEXT("terrain_anchor_policy"), AnchorPolicy) &&
				AnchorPolicy == TEXT("owner_cell_clamped_bounds_center") &&
				Settings->TryGetStringField(TEXT("topology_policy"), TopologyPolicy) &&
				TopologyPolicy == TEXT("cell_local_classify_v1") &&
				Settings->TryGetStringField(TEXT("duplicate_policy"), DuplicatePolicy) &&
				DuplicatePolicy == TEXT("stable_feature_id") &&
				Settings->TryGetStringField(TEXT("contained_policy"), ContainedPolicy) &&
				ContainedPolicy == TEXT("associate_with_container") &&
				Settings->TryGetStringField(TEXT("conflict_policy"), ConflictPolicy) &&
				ConflictPolicy == TEXT("reject_affected_fragments") &&
				Settings->TryGetBoolField(TEXT("nanite"), bNanite) && bNanite &&
				Settings->TryGetStringField(TEXT("collision"), Collision) && Collision == TEXT("complex_as_simple") &&
				Settings->TryGetStringField(TEXT("navigation"), Navigation) && Navigation == TEXT("no_navigation");
		}

		bool ValidateGameplayPlacement(
			const FProjectWorldRealizationLayer& Layer,
			const TSharedPtr<FJsonObject>& Settings,
			FString& OutError)
		{
			FString Source;
			FString SurfacePolicy;
			FString RuntimeStatePolicy;
			return Layer.LayerKind == EProjectWorldLayerKind::GeneratedGameplayPlacement &&
				HasSingleSelector(Layer, TEXT("gameplay_placements")) && Layer.DependsOn.Num() == 1 &&
				Layer.DependsOn[0] == TEXT("terrain") && Layer.SpatialOwnership == TEXT("object_local") &&
				Layer.DirtyGranularity == EProjectWorldDirtyGranularity::ObjectId &&
				Layer.RuntimeMapping == TEXT("world_partition_spatial") && Layer.DependencyHaloCells == 0 &&
				HasOnlyFields(Settings, {TEXT("placement_source"), TEXT("surface_policy"),
					TEXT("runtime_state_policy")}, OutError) &&
				Settings->TryGetStringField(TEXT("placement_source"), Source) &&
				Source.StartsWith(TEXT("GameplayPlacement/")) && Source.EndsWith(TEXT(".json")) &&
				!Source.Contains(TEXT("..")) && !Source.Contains(TEXT("\\")) &&
				Settings->TryGetStringField(TEXT("surface_policy"), SurfacePolicy) &&
				SurfacePolicy == TEXT("canonical_terrain_snap") &&
				Settings->TryGetStringField(TEXT("runtime_state_policy"), RuntimeStatePolicy) &&
				RuntimeStatePolicy == TEXT("external_to_generation");
		}
	}

	bool IsRegistered(const FString& GeneratorId, int32 GeneratorVersion, EProjectWorldLayerKind LayerKind)
	{
		return GeneratorVersion == 1 && ((LayerKind == EProjectWorldLayerKind::GeneratedGeography &&
			(GeneratorId == TEXT("project_landscape") || GeneratorId == TEXT("project_water_mesh") ||
			 GeneratorId == TEXT("project_road_mesh") || GeneratorId == TEXT("project_vegetation_instances") ||
			 GeneratorId == TEXT("project_building_massing"))) ||
			(LayerKind == EProjectWorldLayerKind::GeneratedGameplayPlacement &&
				GeneratorId == TEXT("project_gameplay_placement")));
	}

	bool ValidateSettings(const FProjectWorldRealizationLayer& Layer, FString& OutError)
	{
		TSharedPtr<FJsonObject> Settings;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Layer.NormalizedSettings), Settings) || !Settings.IsValid())
		{
			OutError = FString::Printf(TEXT("Generator settings are not an object: %s"), *Layer.LayerId);
			return false;
		}
		bool bValid = false;
		if (Layer.GeneratorId == TEXT("project_landscape")) bValid = ValidateLandscape(Layer, Settings, OutError);
		else if (Layer.GeneratorId == TEXT("project_water_mesh")) bValid = ValidateWater(Layer, Settings, OutError);
		else if (Layer.GeneratorId == TEXT("project_road_mesh")) bValid = ValidateRoads(Layer, Settings, OutError);
		else if (Layer.GeneratorId == TEXT("project_vegetation_instances")) bValid = ValidateVegetation(Layer, Settings, OutError);
		else if (Layer.GeneratorId == TEXT("project_building_massing")) bValid = ValidateBuildings(Layer, Settings, OutError);
		else if (Layer.GeneratorId == TEXT("project_gameplay_placement")) bValid = ValidateGameplayPlacement(Layer, Settings, OutError);
		else
		{
			OutError = FString::Printf(TEXT("Generator settings are not registered for: %s:v%d"), *Layer.GeneratorId, Layer.GeneratorVersion);
			return false;
		}
		if (!bValid && OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s:v%d does not match its registered executable tuple."), *Layer.GeneratorId, Layer.GeneratorVersion);
		}
		return bValid;
	}
}
