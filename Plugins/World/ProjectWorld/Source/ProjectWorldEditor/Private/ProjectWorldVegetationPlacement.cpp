// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVegetationRealization.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldVegetationExclusions.h"

#include "Curve/GeneralPolygon2.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldVegetationRealization
{
	using namespace UE::Geometry;

	namespace
	{
		struct FVegetationSettings
		{
			TArray<FString> MeshAssets;
			double AreaSpacingMeters = 0.0;
			double AreaJitterFraction = 0.0;
			double MinimumScale = 1.0;
			double MaximumScale = 1.0;
			double SurfaceOffsetMeters = 0.0;
			int32 MaximumInstancesPerCell = 0;
			int32 DeterministicSeed = 0;
		};

		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		void AppendToken(FString& Target, const FString& Value)
		{
			Target += FString::Printf(TEXT("|%d:%s"), Value.Len(), *Value);
		}

		void AppendNumber(FString& Target, double Value)
		{
			AppendToken(Target, FString::Printf(TEXT("%.17g"), Value));
		}

		double StableUnit(const FString& Value)
		{
			FTCHARToUTF8 Utf8(*Value);
			uint64 Hash = 1469598103934665603ull;
			for (int32 Index = 0; Index < Utf8.Length(); ++Index)
			{
				Hash ^= static_cast<uint8>(Utf8.Get()[Index]);
				Hash *= 1099511628211ull;
			}
			return static_cast<double>(Hash & MAX_uint32) / static_cast<double>(MAX_uint32);
		}

		bool ResolveSettings(const FProjectWorldRealizationLayer& Layer, FVegetationSettings& OutSettings, FString& OutError)
		{
			TSharedPtr<FJsonObject> Settings;
			const TArray<TSharedPtr<FJsonValue>>* Meshes = nullptr;
			double MaximumInstances = 0.0;
			double Seed = 0.0;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Layer.NormalizedSettings), Settings) ||
				!Settings.IsValid() || !Settings->TryGetArrayField(TEXT("mesh_assets"), Meshes) || Meshes == nullptr || Meshes->IsEmpty() ||
				!Settings->TryGetNumberField(TEXT("area_spacing_m"), OutSettings.AreaSpacingMeters) ||
				!Settings->TryGetNumberField(TEXT("area_jitter_fraction"), OutSettings.AreaJitterFraction) ||
				!Settings->TryGetNumberField(TEXT("minimum_scale"), OutSettings.MinimumScale) ||
				!Settings->TryGetNumberField(TEXT("maximum_scale"), OutSettings.MaximumScale) ||
				!Settings->TryGetNumberField(TEXT("maximum_instances_per_cell"), MaximumInstances) ||
				!Settings->TryGetNumberField(TEXT("deterministic_seed"), Seed) ||
				!Settings->TryGetNumberField(TEXT("surface_offset_m"), OutSettings.SurfaceOffsetMeters))
			{
				OutError = TEXT("Vegetation layer has no executable v1 settings contract.");
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Meshes)
			{
				FString Path;
				if (!Value.IsValid() || !Value->TryGetString(Path) || Path.IsEmpty())
				{
					OutError = TEXT("Vegetation mesh_assets contains an invalid path.");
					return false;
				}
				OutSettings.MeshAssets.Add(Path);
			}
			OutSettings.MaximumInstancesPerCell = static_cast<int32>(MaximumInstances);
			OutSettings.DeterministicSeed = static_cast<int32>(Seed);
			return true;
		}

		TArray<FString> CellFeatureIds(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell)
		{
			TSet<FString> Candidates;
			Candidates.Append(Cell.OwnedFeatureIds);
			Candidates.Append(Cell.ReferencedFeatureIds);
			TArray<FString> Result;
			for (const FString& FeatureId : Candidates)
			{
				const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
				if (Feature != nullptr && (Feature->FeatureClass == TEXT("vegetation_area") ||
					Feature->FeatureClass == TEXT("foliage_point")))
				{
					Result.Add(FeatureId);
				}
			}
			Result.Sort();
			return Result;
		}

		TArray<FVector2d> OpenRing(const TArray<FVector2D>& Ring)
		{
			TArray<FVector2d> Result;
			for (const FVector2D& Point : Ring)
			{
				Result.Add(FVector2d(Point.X, Point.Y));
			}
			if (Result.Num() > 1 && Result[0].Equals(Result.Last(), UE_DOUBLE_SMALL_NUMBER))
			{
				Result.Pop();
			}
			return Result;
		}

		bool BuildPolygon(const FProjectWorldCanonicalPolygon& Source, FGeneralPolygon2d& OutPolygon)
		{
			TArray<FVector2d> Outer = OpenRing(Source.Outer);
			if (Outer.Num() < 3)
			{
				return false;
			}
			OutPolygon = FGeneralPolygon2d(FPolygon2d(MoveTemp(Outer)));
			for (const TArray<FVector2D>& SourceHole : Source.Holes)
			{
				TArray<FVector2d> Hole = OpenRing(SourceHole);
				if (Hole.Num() < 3 || !OutPolygon.AddHole(FPolygon2d(MoveTemp(Hole))))
				{
					return false;
				}
			}
			return true;
		}

		bool InCell(const FVector2D& Point, const FVector4d& Bounds)
		{
			return Point.X >= Bounds.X && Point.X < Bounds.Z && Point.Y >= Bounds.Y && Point.Y < Bounds.W;
		}

		void AddInstance(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const FVegetationSettings& Settings,
			const FProjectWorldVegetationExclusionContext& Exclusions,
			FProjectWorldVegetationPlacementStats& Stats,
			const FString& StableId,
			const FVector2D& CanonicalPoint,
			TArray<FProjectWorldVegetationInstance>& OutInstances)
		{
			++Stats.CandidateCount;
			switch (Exclusions.Classify(CanonicalPoint))
			{
			case EProjectWorldVegetationExclusion::Road:
				++Stats.RoadExcludedCount;
				return;
			case EProjectWorldVegetationExclusion::Water:
				++Stats.WaterExcludedCount;
				return;
			case EProjectWorldVegetationExclusion::AuthoredMask:
				++Stats.AuthoredMaskExcludedCount;
				return;
			default:
				break;
			}
			const FString Seed = FString::Printf(TEXT("%d|%s"), Settings.DeterministicSeed, *StableId);
			const double Height = ProjectWorldGeneratedGeometry::SampleTerrain(Cell, CanonicalPoint.X, CanonicalPoint.Y) +
				Settings.SurfaceOffsetMeters;
			const FVector ActorOrigin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle, FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
			const FVector World = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(CanonicalPoint, Height));
			const double Scale = FMath::Lerp(Settings.MinimumScale, Settings.MaximumScale, StableUnit(Seed + TEXT("|scale")));
			FProjectWorldVegetationInstance& Instance = OutInstances.AddDefaulted_GetRef();
			Instance.StableId = StableId;
			Instance.MeshIndex = FMath::Min(
				static_cast<int32>(StableUnit(Seed + TEXT("|mesh")) * Settings.MeshAssets.Num()),
				Settings.MeshAssets.Num() - 1);
			Instance.Transform = FTransform(
				FRotator(0.0, StableUnit(Seed + TEXT("|yaw")) * 360.0, 0.0),
				World - ActorOrigin,
				FVector(Scale));
		}

		void AddAreaInstances(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const FProjectWorldCanonicalFeature& Feature,
			const FVegetationSettings& Settings,
			const FProjectWorldVegetationExclusionContext& Exclusions,
			FProjectWorldVegetationPlacementStats& Stats,
			TArray<FProjectWorldVegetationInstance>& OutInstances)
		{
			for (int32 PolygonIndex = 0; PolygonIndex < Feature.GeometryPolygons.Num(); ++PolygonIndex)
			{
				FGeneralPolygon2d Polygon;
				if (!BuildPolygon(Feature.GeometryPolygons[PolygonIndex], Polygon))
				{
					continue;
				}
				const FAxisAlignedBox2d Bounds = Polygon.Bounds();
				const int32 MinX = FMath::FloorToInt(FMath::Max(Bounds.Min.X, Cell.Bounds.X) / Settings.AreaSpacingMeters);
				const int32 MaxX = FMath::CeilToInt(FMath::Min(Bounds.Max.X, Cell.Bounds.Z) / Settings.AreaSpacingMeters);
				const int32 MinY = FMath::FloorToInt(FMath::Max(Bounds.Min.Y, Cell.Bounds.Y) / Settings.AreaSpacingMeters);
				const int32 MaxY = FMath::CeilToInt(FMath::Min(Bounds.Max.Y, Cell.Bounds.W) / Settings.AreaSpacingMeters);
				for (int32 GridY = MinY; GridY <= MaxY; ++GridY)
				{
					for (int32 GridX = MinX; GridX <= MaxX; ++GridX)
					{
						const FString StableId = FString::Printf(TEXT("area|%s|%d|%d|%d"),
							*Feature.FeatureId, PolygonIndex, GridX, GridY);
						const FString Seed = FString::Printf(TEXT("%d|%s"), Settings.DeterministicSeed, *StableId);
						const double Jitter = Settings.AreaSpacingMeters * Settings.AreaJitterFraction;
						const FVector2D Point(
							(GridX + 0.5) * Settings.AreaSpacingMeters + (StableUnit(Seed + TEXT("|x")) * 2.0 - 1.0) * Jitter,
							(GridY + 0.5) * Settings.AreaSpacingMeters + (StableUnit(Seed + TEXT("|y")) * 2.0 - 1.0) * Jitter);
						if (InCell(Point, Cell.Bounds) && Polygon.Contains(FVector2d(Point.X, Point.Y)))
						{
							AddInstance(Bundle, Cell, Settings, Exclusions, Stats, StableId, Point, OutInstances);
						}
					}
				}
			}
		}

		bool HashResolvedCellInput(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const FProjectWorldRealizationLayer& Layer,
			const FProjectWorldVegetationExclusionContext& Exclusions,
			FString& OutHash)
		{
			FString Identity(TEXT("project_vegetation_cell_input_v1"));
			AppendToken(Identity, Cell.CellId);
			AppendToken(Identity, Cell.Terrain.ArtifactHash);
			AppendToken(Identity, Layer.ContractHash);
			AppendToken(Identity, Exclusions.InputHash);
			for (const FString& FeatureId : CellFeatureIds(Bundle, Cell))
			{
				const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
				AppendToken(Identity, Feature.FeatureId);
				AppendToken(Identity, Feature.FeatureClass);
				AppendToken(Identity, Feature.VegetationClass);
				AppendToken(Identity, Feature.FoliageClass);
				AppendToken(Identity, Feature.LeafType);
				AppendToken(Identity, Feature.LeafCycle);
				AppendToken(Identity, Feature.Species);
				for (const FVector2D& Point : Feature.GeometryPoints)
				{
					AppendNumber(Identity, Point.X);
					AppendNumber(Identity, Point.Y);
				}
				for (const FProjectWorldCanonicalPolygon& Polygon : Feature.GeometryPolygons)
				{
					for (const FVector2D& Point : Polygon.Outer)
					{
						AppendNumber(Identity, Point.X);
						AppendNumber(Identity, Point.Y);
					}
					for (const TArray<FVector2D>& Hole : Polygon.Holes)
					{
						AppendToken(Identity, FString::FromInt(Hole.Num()));
						for (const FVector2D& Point : Hole)
						{
							AppendNumber(Identity, Point.X);
							AppendNumber(Identity, Point.Y);
						}
					}
				}
			}
			return HashText(Identity, OutHash);
		}
	}

	bool HashCellInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FString& OutHash,
		FString& OutError)
	{
		FVegetationSettings Settings;
		if (!ResolveSettings(Layer, Settings, OutError))
		{
			return false;
		}
		FProjectWorldVegetationExclusionContext Exclusions;
		if (!ProjectWorldVegetationExclusions::Build(
			Bundle, Cell, Profile, Layer, AuthoredOverlaySet, Exclusions, OutError))
		{
			return false;
		}
		return HashResolvedCellInput(Bundle, Cell, Layer, Exclusions, OutHash);
	}

	bool BuildCellInstances(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		TArray<FProjectWorldVegetationInstance>& OutInstances,
		FProjectWorldVegetationPlacementStats* OutStats,
		FString& OutSemanticHash,
		FString& OutError)
	{
		OutInstances.Reset();
		FVegetationSettings Settings;
		if (!ResolveSettings(Layer, Settings, OutError))
		{
			return false;
		}
		FProjectWorldVegetationExclusionContext Exclusions;
		if (!ProjectWorldVegetationExclusions::Build(
			Bundle, Cell, Profile, Layer, AuthoredOverlaySet, Exclusions, OutError))
		{
			return false;
		}
		FProjectWorldVegetationPlacementStats LocalStats;
		FProjectWorldVegetationPlacementStats& Stats = OutStats != nullptr ? *OutStats : LocalStats;
		Stats = FProjectWorldVegetationPlacementStats();
		for (const FString& FeatureId : CellFeatureIds(Bundle, Cell))
		{
			const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
			if (Feature.FeatureClass == TEXT("foliage_point"))
			{
				for (int32 Index = 0; Index < Feature.GeometryPoints.Num(); ++Index)
				{
					if (InCell(Feature.GeometryPoints[Index], Cell.Bounds))
					{
						AddInstance(Bundle, Cell, Settings, Exclusions, Stats,
							FString::Printf(TEXT("point|%s|%d"), *Feature.FeatureId, Index),
							Feature.GeometryPoints[Index], OutInstances);
					}
				}
			}
			else
			{
				AddAreaInstances(Bundle, Cell, Feature, Settings, Exclusions, Stats, OutInstances);
			}
		}
		OutInstances.Sort([](const auto& Left, const auto& Right) { return Left.StableId < Right.StableId; });
		if (OutInstances.Num() > Settings.MaximumInstancesPerCell)
		{
			OutInstances.SetNum(Settings.MaximumInstancesPerCell);
		}
		FString InputHash;
		if (!HashResolvedCellInput(Bundle, Cell, Layer, Exclusions, InputHash))
		{
			OutError = TEXT("Cannot hash resolved vegetation cell input.");
			return false;
		}
		FString Semantic = TEXT("project_vegetation_cell_v1|") + Cell.CellId + TEXT("|") + InputHash;
		for (const FProjectWorldVegetationInstance& Instance : OutInstances)
		{
			AppendToken(Semantic, Instance.StableId);
			AppendToken(Semantic, FString::FromInt(Instance.MeshIndex));
			const FVector Location = Instance.Transform.GetLocation();
			AppendNumber(Semantic, Location.X);
			AppendNumber(Semantic, Location.Y);
			AppendNumber(Semantic, Location.Z);
			AppendNumber(Semantic, Instance.Transform.Rotator().Yaw);
			AppendNumber(Semantic, Instance.Transform.GetScale3D().X);
		}
		return HashText(Semantic, OutSemanticHash);
	}

	bool ExpectsCellOutput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		bool& bOutExpected,
		FString& OutError)
	{
		TArray<FProjectWorldVegetationInstance> Instances;
		FString Semantic;
		if (!BuildCellInstances(
			Bundle, Cell, Layer, Profile, AuthoredOverlaySet, Instances, nullptr, Semantic, OutError))
		{
			return false;
		}
		bOutExpected = !Instances.IsEmpty();
		return true;
	}
}
