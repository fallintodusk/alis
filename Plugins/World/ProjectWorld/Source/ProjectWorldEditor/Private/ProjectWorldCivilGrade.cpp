// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCivilGrade.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"

namespace ProjectWorldCivilGrade
{
	namespace
	{
		bool Hash(const FString& Value)
		{
			if (Value.Len() != 64) return false;
			for (const TCHAR C : Value) if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
			return true;
		}
		bool Is(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const TCHAR* Expected)
		{ FString Value; return Object->TryGetStringField(Field, Value) && Value == Expected; }
	}

	FString BaseHash(const FProjectWorldCanonicalTerrain& Terrain)
	{
		if (!Terrain.BaseArtifactHash.IsEmpty()) return Terrain.BaseArtifactHash;
		return Terrain.RefinedQuads.IsEmpty() ? Terrain.ArtifactHash : FString();
	}

	bool Read(const TSharedPtr<FJsonObject>& Attributes, FProjectWorldCanonicalFeature& Feature,
		FProjectWorldCanonicalValidation& Validation)
	{
		if (!Attributes->HasField(TEXT("civil_grade_reference"))) return true;
		if (Attributes->HasField(TEXT("surface_context")))
		{
			const TSharedPtr<FJsonObject>* Context = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Unresolved = nullptr;
			FString Relation, Ford, Tunnel;
			if (!Attributes->TryGetObjectField(TEXT("surface_context"), Context) || !Context ||
				!(*Context)->TryGetStringField(TEXT("ground_relation"), Relation) ||
				!(*Context)->TryGetStringField(TEXT("ford"), Ford) || !(*Context)->TryGetStringField(TEXT("tunnel"), Tunnel) ||
				!(*Context)->TryGetArrayField(TEXT("unresolved"), Unresolved) || !Unresolved ||
				(Relation != TEXT("unknown") && Relation != TEXT("at_ground") && Relation != TEXT("above")) ||
				(Ford != TEXT("absent") && Ford != TEXT("unknown")) || (Tunnel != TEXT("absent") && Tunnel != TEXT("unknown")))
			{
				Validation.ErrorCode = TEXT("civil-grade-contract"); Validation.Message = TEXT("Civil preservation conflicts with source crossing evidence.");
				return false;
			}
			for (const auto& Value : *Unresolved)
			{
				FString Marker;
				if (!Value.IsValid() || Value->Type != EJson::String || !Value->TryGetString(Marker) ||
					Marker == TEXT("unsupported_bridge") || Marker == TEXT("unsupported_ford") ||
					Marker == TEXT("unsupported_tunnel") || Marker == TEXT("crossing_mode_required") || Marker == TEXT("conflicting_bridge_tunnel"))
				{
					Validation.ErrorCode = TEXT("civil-grade-contract"); Validation.Message = TEXT("Explicit unresolved crossing evidence cannot use civil grade preservation.");
					return false;
				}
			}
		}
		const TSharedPtr<FJsonObject> *Reference = nullptr, *Support = nullptr;
		FString FeatureHash;
		double Version = 0;
		if ((Feature.FeatureClass != TEXT("road") && Feature.FeatureClass != TEXT("building")) ||
			!Attributes->TryGetObjectField(TEXT("civil_grade_reference"), Reference) || !Reference ||
			(*Reference)->Values.Num() != 5 ||
			!Is(*Reference, TEXT("$schema"), TEXT("https://alis.world/schemas/world-compiler/civil-grade-reference-v1.json")) ||
			!Is(*Reference, TEXT("model"), TEXT("preserve_accepted_grid_grade_not_structure_classification")) ||
			!(*Reference)->HasTypedField<EJson::Number>(TEXT("version")) ||
			!(*Reference)->TryGetNumberField(TEXT("version"), Version) || Version != 1 ||
			!(*Reference)->TryGetStringField(TEXT("base_feature_sha256"), FeatureHash) || !Hash(FeatureHash) ||
			!(*Reference)->TryGetObjectField(TEXT("terrain_artifacts"), Support) || !Support || (*Support)->Values.IsEmpty())
		{
			Validation.ErrorCode = TEXT("civil-grade-contract"); Validation.Message = TEXT("Civil grade reference is malformed or unsupported.");
			return false;
		}
		FProjectWorldCanonicalCivilGrade Grade;
		for (const auto& Pair : (*Support)->Values)
		{
			FString Digest;
			if (Pair.Key.IsEmpty() || !Pair.Value->TryGetString(Digest) || !Hash(Digest))
			{
				Validation.ErrorCode = TEXT("civil-grade-contract"); Validation.Message = TEXT("Civil grid support identity is malformed.");
				return false;
			}
			Grade.TerrainArtifacts.Add(FString(*Pair.Key), Digest);
		}
		Feature.CivilGrade = MoveTemp(Grade);
		return true;
	}

	bool Validate(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalFeature& Feature,
		FString& Identity, FString& Error)
	{
		Identity.Reset();
		if (!Feature.CivilGrade.IsSet()) return true;
		TSet<FString> Expected;
		if (Feature.FeatureClass == TEXT("building")) Expected.Add(Feature.OwnerCellId);
		else if (Feature.FeatureClass == TEXT("road"))
		{
			TArray<TArray<FVector2D>> Lines = Feature.GeometryParts;
			if (Feature.GeometryType == TEXT("LineString")) Lines = {Feature.GeometryPoints};
			if (Lines.IsEmpty() || !FMath::IsFinite(Feature.WidthMeters) || Feature.WidthMeters <= 0)
			{ Error = TEXT("Civil Road grade has no complete source lines or width."); return false; }
			for (const auto& Line : Lines)
			{
				if (Line.Num() < 2) { Error = TEXT("Civil Road grade line is incomplete."); return false; }
				for (int32 I = 0; I + 1 < Line.Num(); ++I)
				{
					if (Line[I].ContainsNaN() || Line[I+1].ContainsNaN()) { Error = TEXT("Civil grade is nonfinite."); return false; }
					FBox2D Bounds(ForceInit); Bounds += Line[I]; Bounds += Line[I+1];
					Bounds = Bounds.ExpandBy(Feature.WidthMeters * 0.5);
					for (const auto& Cell : Bundle.Cells)
						if (Bounds.Intersect(FBox2D(FVector2D(Cell.Bounds.X, Cell.Bounds.Y), FVector2D(Cell.Bounds.Z, Cell.Bounds.W))))
							Expected.Add(Cell.CellId);
				}
			}
		}
		else { Error = TEXT("Civil grade belongs only to a Road or Building."); return false; }
		const auto& Support = Feature.CivilGrade->TerrainArtifacts;
		if (Expected.IsEmpty() || Expected.Num() != Support.Num())
		{ Error = TEXT("Civil grade omits or expands its consumed grid support."); return false; }
		Identity = TEXT("immutable_civil_grade_v1|");
		TArray<FString> Keys; Support.GetKeys(Keys); Keys.Sort();
		for (const auto& Key : Keys)
		{
			const auto* Cell = Bundle.Cells.FindByPredicate([&](const auto& C) { return C.CellId == Key; });
			if (!Expected.Contains(Key) || !Cell || !Hash(Support[Key]) || BaseHash(Cell->Terrain) != Support[Key])
			{ Error = TEXT("Civil grade support is missing, stale or not an authenticated immutable grid."); return false; }
			Identity += FString::Printf(TEXT("%d:%s%s|"), Key.Len(), *Key, *Support[Key]);
		}
		return true;
	}
}
