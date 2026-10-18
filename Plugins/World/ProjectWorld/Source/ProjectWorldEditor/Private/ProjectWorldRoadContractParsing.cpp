// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadContractParsing.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"

namespace ProjectWorldRoadContractParsing
{
	namespace
	{
		bool Hash(const FString& Text)
		{
			if (Text.Len() != 64) return false;
			for (const TCHAR C : Text) if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
			return true;
		}
		bool Exact(const TSharedPtr<FJsonObject>& Object, std::initializer_list<const TCHAR*> Fields)
		{
			if (Object->Values.Num() != static_cast<int32>(Fields.size())) return false;
			for (const auto* Field : Fields) if (!Object->HasField(Field)) return false;
			return true;
		}
		bool Is(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const TCHAR* Expected)
		{
			FString Value;
			return Object->TryGetStringField(Field, Value) && Value == Expected;
		}
		bool HashField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
		{
			FString Value;
			return Object->TryGetStringField(Field, Value) && Hash(Value);
		}
		bool Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& Out)
		{
			const auto Value = Object->TryGetField(Field);
			return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(Out) && FMath::IsFinite(Out);
		}
		bool ReadProfile(const TSharedPtr<FJsonObject>& Attributes, FProjectWorldCanonicalFeature& Feature)
		{
			const TSharedPtr<FJsonObject> *Wrapper = nullptr, *Surface = nullptr, *Support = nullptr, *Construction = nullptr;
			double Version;
			if (Feature.FeatureClass != TEXT("road") || Feature.GeometryType != TEXT("LineString") ||
				!Attributes->TryGetObjectField(TEXT("road_elevation_profile"), Wrapper) || !Wrapper ||
				!Exact(*Wrapper, {TEXT("version"), TEXT("surface"), TEXT("support_terrain_artifacts"), TEXT("base_feature_sha256")}) ||
				!Number(*Wrapper, TEXT("version"), Version) || Version != 1 ||
				!HashField(*Wrapper, TEXT("base_feature_sha256")) ||
				!(*Wrapper)->TryGetObjectField(TEXT("surface"), Surface) || !Surface ||
				!(*Wrapper)->TryGetObjectField(TEXT("support_terrain_artifacts"), Support) || !Support ||
				!Exact(*Surface, {TEXT("$schema"), TEXT("schema_version"), TEXT("interpolation"), TEXT("coordinate_rule"),
					TEXT("height_rule"), TEXT("model"), TEXT("length_m"), TEXT("width_m"), TEXT("triangles"),
					TEXT("source_feature_id"), TEXT("source_points"),
					TEXT("consumed_feature_sha256"), TEXT("consumed_terrain"), TEXT("implementation_sha256"), TEXT("execution_identity")}) ||
				!Is(*Surface, TEXT("$schema"), TEXT("https://alis.world/schemas/world-compiler/road-elevation-v1.json")) ||
				!Number(*Surface, TEXT("schema_version"), Version) || Version != 1 ||
				!Is(*Surface, TEXT("interpolation"), TEXT("affine_uv_triangles_v1")) ||
				!Is(*Surface, TEXT("coordinate_rule"), TEXT("source_arclength_m_signed_lateral_left_m")) ||
				!Is(*Surface, TEXT("height_rule"), TEXT("canonical_absolute_m_without_presentation_offset")) ||
				!Is(*Surface, TEXT("model"), TEXT("source_abutment_support_candidate_not_surveyed")) ||
				!HashField(*Surface, TEXT("consumed_feature_sha256")) || !HashField(*Surface, TEXT("implementation_sha256")) ||
				!HashField(*Surface, TEXT("execution_identity")) ||
				!(*Surface)->TryGetObjectField(TEXT("consumed_terrain"), Construction) || !Construction ||
				(*Support)->Values.IsEmpty() || (*Support)->Values.Num() != (*Construction)->Values.Num()) return false;
			FProjectWorldCanonicalRoadElevation Candidate;
			if (!Number(*Surface, TEXT("length_m"), Candidate.LengthMeters) ||
				!Number(*Surface, TEXT("width_m"), Candidate.WidthMeters) ||
				!(*Surface)->TryGetStringField(TEXT("source_feature_id"), Candidate.SourceFeatureId) ||
				Candidate.SourceFeatureId != Feature.FeatureId ||
				!FMath::IsFinite(Candidate.LengthMeters) || Candidate.LengthMeters <= 0 ||
				!FMath::IsFinite(Candidate.WidthMeters) || Candidate.WidthMeters <= 0) return false;
			const TArray<TSharedPtr<FJsonValue>>* SourcePoints = nullptr;
			if (!(*Surface)->TryGetArrayField(TEXT("source_points"), SourcePoints) || !SourcePoints || SourcePoints->Num() < 2) return false;
			for (const auto& Point : *SourcePoints)
			{
				const TArray<TSharedPtr<FJsonValue>>* XY = nullptr;
				FVector2D P;
				if (!Point->TryGetArray(XY) || !XY || XY->Num() != 2) return false;
				for (int32 I = 0; I < 2; ++I)
					if ((*XY)[I]->Type != EJson::Number || !(*XY)[I]->TryGetNumber(P[I]) || !FMath::IsFinite(P[I])) return false;
				Candidate.SourcePoints.Add(P);
			}
			if (Candidate.SourcePoints != Feature.GeometryPoints || Candidate.WidthMeters != Feature.WidthMeters) return false;
			for (const auto& Pair : (*Support)->Values)
			{
				FString Value;
				if (Pair.Key.IsEmpty() || !Pair.Value->TryGetString(Value) || !Hash(Value) ||
					!HashField(*Construction, *Pair.Key)) return false;
				Candidate.TerrainArtifacts.Add(FString(*Pair.Key), Value);
			}
			const TArray<TSharedPtr<FJsonValue>>* Faces = nullptr;
			if (!(*Surface)->TryGetArrayField(TEXT("triangles"), Faces) || !Faces || Faces->Num() < 2 || Faces->Num() > 4096) return false;
			for (const auto& Value : *Faces)
			{
				const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
				if (!Value->TryGetArray(Points) || !Points || Points->Num() != 3) return false;
				FProjectWorldCanonicalRoadElevationTriangle Face;
				for (int32 I = 0; I < 3; ++I)
				{
					const TArray<TSharedPtr<FJsonValue>>* XYZ = nullptr;
					if (!(*Points)[I]->TryGetArray(XYZ) || !XYZ || XYZ->Num() != 3) return false;
					for (int32 J = 0; J < 3; ++J)
						if ((*XYZ)[J]->Type != EJson::Number || !(*XYZ)[J]->TryGetNumber(Face.UVZ[I][J]) || !FMath::IsFinite(Face.UVZ[I][J])) return false;
				}
				Candidate.Triangles.Add(Face);
			}
			Feature.RoadElevation = MoveTemp(Candidate);
			return true;
		}
	}
	bool Read(const TSharedPtr<FJsonObject>& Attributes, FProjectWorldCanonicalFeature& Feature,
		FProjectWorldCanonicalValidation& Validation)
	{
		if (!Attributes->HasField(TEXT("road_elevation_profile"))) return true;
		if (ReadProfile(Attributes, Feature)) return true;
		Validation.ErrorCode = TEXT("road-elevation-contract");
		Validation.Message = TEXT("Present canonical Road elevation profile is malformed or unsupported.");
		Validation.Detail = Feature.FeatureId;
		return false;
	}
}
