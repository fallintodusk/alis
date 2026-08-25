// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimeProfile.h"

#include "ProjectWorldSchemaReference.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldRuntimeProfile
{
	namespace
	{
		const TCHAR* ExpectedSchemaFilename = TEXT("project_world_runtime_profile.schema.json");

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
					OutError = FString::Printf(TEXT("Unknown field: %s"), *Field.Key);
					return false;
				}
			}
			return true;
		}

		bool RequireString(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			FString& OutValue,
			FString& OutError)
		{
			if (!Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Missing or empty string: %s"), Name);
				return false;
			}
			return true;
		}

		bool RequireNumber(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			double Minimum,
			double Maximum,
			double& OutValue,
			FString& OutError)
		{
			if (!Object->TryGetNumberField(Name, OutValue) || !FMath::IsFinite(OutValue) ||
				OutValue < Minimum || OutValue > Maximum)
			{
				OutError = FString::Printf(TEXT("Number is missing or outside its contract: %s"), Name);
				return false;
			}
			return true;
		}

		bool RequireBool(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			bool& OutValue,
			FString& OutError)
		{
			if (!Object->TryGetBoolField(Name, OutValue))
			{
				OutError = FString::Printf(TEXT("Missing boolean: %s"), Name);
				return false;
			}
			return true;
		}

		bool IsIdentifier(const FString& Value)
		{
			if (Value.IsEmpty())
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				const bool bLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
				const bool bDigitAscii = Character >= TEXT('0') && Character <= TEXT('9');
				if (!bLowerAscii && !bDigitAscii && Character != TEXT('_'))
				{
					return false;
				}
			}
			return true;
		}

		bool IsGridIdentifier(const FString& Value)
		{
			if (!Value.StartsWith(TEXT("grid_")) || Value.Len() <= 5)
			{
				return false;
			}
			for (int32 Index = 5; Index < Value.Len(); ++Index)
			{
				const TCHAR Character = Value[Index];
				const bool bLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
				const bool bDigitAscii = Character >= TEXT('0') && Character <= TEXT('9');
				if (!bLowerAscii && !bDigitAscii)
				{
					return false;
				}
			}
			return true;
		}

		bool RequireIntegerNumber(double Value, const TCHAR* Name, FString& OutError)
		{
			if (Value != static_cast<double>(static_cast<int64>(Value)))
			{
				OutError = FString::Printf(TEXT("Number must be an integer: %s"), Name);
				return false;
			}
			return true;
		}

		bool IsSha256(const FString& Value)
		{
			if (Value.Len() != 64)
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				const bool bDigit = Character >= TEXT('0') && Character <= TEXT('9');
				const bool bHex = Character >= TEXT('a') && Character <= TEXT('f');
				if (!bDigit && !bHex)
				{
					return false;
				}
			}
			return true;
		}
	}

	bool Load(
		const FString& Path,
		FProjectWorldRuntimeProfile& OutProfile,
		FString& OutErrorCode,
		FString& OutError)
	{
		OutProfile = FProjectWorldRuntimeProfile();
		OutErrorCode.Reset();
		OutError.Reset();
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutErrorCode = TEXT("runtime-profile-read");
			OutError = Path;
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
			!HasOnlyFields(Root, {
				TEXT("$schema"), TEXT("schema_version"), TEXT("profile_id"), TEXT("profile_kind"),
				TEXT("grid_id"), TEXT("runtime_partition"), TEXT("gameplay_route"),
				TEXT("optimization_policy"), TEXT("budgets")}, OutError))
		{
			OutErrorCode = TEXT("runtime-profile-json");
			return false;
		}

		FString Schema;
		double SchemaVersion = 0.0;
		if (!RequireString(Root, TEXT("$schema"), Schema, OutError) ||
			!ProjectWorldSchemaReference::ResolvesToCanonical(
				Path,
				Schema,
				ExpectedSchemaFilename,
				OutError) ||
			!RequireNumber(Root, TEXT("schema_version"), 2.0, 2.0, SchemaVersion, OutError) ||
			!RequireString(Root, TEXT("profile_id"), OutProfile.ProfileId, OutError) ||
			!IsIdentifier(OutProfile.ProfileId) ||
			!RequireString(Root, TEXT("profile_kind"), OutProfile.ProfileKind, OutError) ||
			(OutProfile.ProfileKind != TEXT("bounded_procedural_route") &&
				OutProfile.ProfileKind != TEXT("territory_product")) ||
			!RequireString(Root, TEXT("grid_id"), OutProfile.GridId, OutError) ||
			!IsGridIdentifier(OutProfile.GridId))
		{
			OutErrorCode = TEXT("runtime-profile-contract");
			return false;
		}

		const TSharedPtr<FJsonObject>* Partition = nullptr;
		double CellSizeMeters = 0.0;
		double LoadingRangeMeters = 0.0;
		if (!Root->TryGetObjectField(TEXT("runtime_partition"), Partition) || Partition == nullptr ||
			!HasOnlyFields(*Partition, {
				TEXT("partition_class"), TEXT("cell_size_m"), TEXT("loading_range_m"),
				TEXT("block_on_slow_streaming")}, OutError) ||
			!RequireString(*Partition, TEXT("partition_class"), OutProfile.RuntimePartitionClass, OutError) ||
			OutProfile.RuntimePartitionClass != TEXT("lhgrid_2d") ||
			!RequireNumber(*Partition, TEXT("cell_size_m"), 1.0, 8192.0, CellSizeMeters, OutError) ||
			!RequireNumber(*Partition, TEXT("loading_range_m"), 1.0, 32768.0, LoadingRangeMeters, OutError) ||
			!RequireIntegerNumber(CellSizeMeters, TEXT("cell_size_m"), OutError) ||
			!RequireIntegerNumber(LoadingRangeMeters, TEXT("loading_range_m"), OutError) ||
			LoadingRangeMeters < CellSizeMeters ||
			!RequireBool(*Partition, TEXT("block_on_slow_streaming"), OutProfile.bBlockOnSlowStreaming, OutError))
		{
			OutErrorCode = TEXT("runtime-profile-partition");
			return false;
		}
		OutProfile.RuntimeCellSizeMeters = static_cast<int32>(CellSizeMeters);
		OutProfile.RuntimeLoadingRangeMeters = static_cast<int32>(LoadingRangeMeters);

		const TSharedPtr<FJsonObject>* Route = nullptr;
		if (!Root->TryGetObjectField(TEXT("gameplay_route"), Route) || Route == nullptr ||
			!HasOnlyFields(*Route, {
				TEXT("route_id"), TEXT("feature_id"), TEXT("endpoint_inset_m"), TEXT("surface_offset_m"),
				TEXT("navigation_padding_m"), TEXT("navigation_height_m")}, OutError) ||
			!RequireString(*Route, TEXT("route_id"), OutProfile.RouteId, OutError) ||
			!IsIdentifier(OutProfile.RouteId) ||
			!RequireString(*Route, TEXT("feature_id"), OutProfile.RouteFeatureId, OutError) ||
			!RequireNumber(*Route, TEXT("endpoint_inset_m"), 0.0, 100.0, OutProfile.EndpointInsetMeters, OutError) ||
			!RequireNumber(*Route, TEXT("surface_offset_m"), 0.0, 10.0, OutProfile.RouteSurfaceOffsetMeters, OutError) ||
			!RequireNumber(*Route, TEXT("navigation_padding_m"), 1.0, 1000.0, OutProfile.NavigationPaddingMeters, OutError) ||
			!RequireNumber(*Route, TEXT("navigation_height_m"), 2.0, 1000.0, OutProfile.NavigationHeightMeters, OutError))
		{
			OutErrorCode = TEXT("runtime-profile-route");
			return false;
		}

		const TSharedPtr<FJsonObject>* Policy = nullptr;
		if (!Root->TryGetObjectField(TEXT("optimization_policy"), Policy) || Policy == nullptr ||
			!HasOnlyFields(*Policy, {TEXT("nanite"), TEXT("instancing"), TEXT("hlod")}, OutError) ||
			!RequireString(*Policy, TEXT("nanite"), OutProfile.NanitePolicy, OutError) ||
			!RequireString(*Policy, TEXT("instancing"), OutProfile.InstancingPolicy, OutError) ||
			!RequireString(*Policy, TEXT("hlod"), OutProfile.HlodPolicy, OutError))
		{
			OutErrorCode = TEXT("runtime-profile-optimization");
			return false;
		}
		const bool bBoundedRoutePolicies =
			OutProfile.NanitePolicy == TEXT("not_applicable_procedural_mesh") &&
			OutProfile.InstancingPolicy == TEXT("not_applicable_unique_geometry") &&
			OutProfile.HlodPolicy == TEXT("disabled_for_bounded_route");
		const bool bTerritoryPolicies =
			OutProfile.NanitePolicy == TEXT("enabled_for_supported_generated_static_meshes") &&
			OutProfile.InstancingPolicy == TEXT("cell_owned_hism") &&
			OutProfile.HlodPolicy == TEXT("disabled_for_territory");
		if ((OutProfile.ProfileKind == TEXT("bounded_procedural_route") && !bBoundedRoutePolicies) ||
			(OutProfile.ProfileKind == TEXT("territory_product") && !bTerritoryPolicies))
		{
			OutErrorCode = TEXT("runtime-profile-optimization");
			OutError = TEXT("Optimization policies do not match the runtime profile kind.");
			return false;
		}

		const TSharedPtr<FJsonObject>* Budgets = nullptr;
		double SourceBytes = 0.0;
		double MeshBytes = 0.0;
		double ActorCount = 0.0;
		double DrawCalls = 0.0;
		if (!Root->TryGetObjectField(TEXT("budgets"), Budgets) || Budgets == nullptr ||
			!HasOnlyFields(*Budgets, {
				TEXT("generated_source_bytes"), TEXT("procedural_mesh_buffer_bytes"),
				TEXT("generated_actor_count"), TEXT("mesh_section_draw_call_upper_bound"),
				TEXT("regeneration_seconds"), TEXT("p95_frame_time_ms")}, OutError) ||
			!RequireNumber(*Budgets, TEXT("generated_source_bytes"), 1.0, 1073741824.0, SourceBytes, OutError) ||
			!RequireNumber(*Budgets, TEXT("procedural_mesh_buffer_bytes"), 1.0, 1073741824.0, MeshBytes, OutError) ||
			!RequireNumber(*Budgets, TEXT("generated_actor_count"), 1.0, 10000.0, ActorCount, OutError) ||
			!RequireNumber(*Budgets, TEXT("mesh_section_draw_call_upper_bound"), 1.0, 10000.0, DrawCalls, OutError) ||
			!RequireIntegerNumber(SourceBytes, TEXT("generated_source_bytes"), OutError) ||
			!RequireIntegerNumber(MeshBytes, TEXT("procedural_mesh_buffer_bytes"), OutError) ||
			!RequireIntegerNumber(ActorCount, TEXT("generated_actor_count"), OutError) ||
			!RequireIntegerNumber(DrawCalls, TEXT("mesh_section_draw_call_upper_bound"), OutError) ||
			!RequireNumber(*Budgets, TEXT("regeneration_seconds"), 0.001, 3600.0, OutProfile.Budgets.RegenerationSeconds, OutError) ||
			!RequireNumber(
				*Budgets,
				TEXT("p95_frame_time_ms"),
				0.1,
				1000.0,
				OutProfile.Budgets.P95FrameTimeMilliseconds,
				OutError))
		{
			OutErrorCode = TEXT("runtime-profile-budgets");
			return false;
		}
		OutProfile.Budgets.GeneratedSourceBytes = static_cast<int64>(SourceBytes);
		OutProfile.Budgets.ProceduralMeshBufferBytes = static_cast<int64>(MeshBytes);
		OutProfile.Budgets.GeneratedActorCount = static_cast<int32>(ActorCount);
		OutProfile.Budgets.MeshSectionDrawCallUpperBound = static_cast<int32>(DrawCalls);

		if (!FProjectSha256::HashFile(Path, OutProfile.ProfileHash) ||
			!IsSha256(OutProfile.ProfileHash))
		{
			OutErrorCode = TEXT("runtime-profile-hash");
			OutError = Path;
			return false;
		}
		return true;
	}
}
