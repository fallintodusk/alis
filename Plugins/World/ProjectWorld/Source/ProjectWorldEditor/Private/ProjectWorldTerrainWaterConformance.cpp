// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldTerrainWaterConformance.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldWaterMeshBuilder.h"

namespace ProjectWorldTerrainWaterConformance
{
	namespace
	{
		double Cross(const FVector2D& A, const FVector2D& B, const FVector2D& Point)
		{
			return static_cast<double>(B.X - A.X) * static_cast<double>(Point.Y - A.Y) -
				static_cast<double>(B.Y - A.Y) * static_cast<double>(Point.X - A.X);
		}

		bool TriangleContains(
			const FVector2D& A,
			const FVector2D& B,
			const FVector2D& C,
			const FVector2D& Point,
			double Tolerance)
		{
			const double AB = Cross(A, B, Point);
			const double BC = Cross(B, C, Point);
			const double CA = Cross(C, A, Point);
			const bool bNegative = AB < -Tolerance || BC < -Tolerance || CA < -Tolerance;
			const bool bPositive = AB > Tolerance || BC > Tolerance || CA > Tolerance;
			return !(bNegative && bPositive);
		}

		enum class EPointInRing : uint8
		{
			Outside,
			Inside,
			Boundary
		};

		EPointInRing PointInRing(
			const TArray<FVector2D>& Ring,
			const FVector2D& Point,
			double Tolerance)
		{
			if (Ring.Num() < 3)
			{
				return EPointInRing::Outside;
			}
			bool bInside = false;
			const double ToleranceSquared = FMath::Square(Tolerance);
			for (int32 Index = 0, Previous = Ring.Num() - 1; Index < Ring.Num(); Previous = Index++)
			{
				const FVector2D& A = Ring[Previous];
				const FVector2D& B = Ring[Index];
				const FVector2D Delta = B - A;
				const double Denominator = Delta.SquaredLength();
				const double Alpha = Denominator > UE_DOUBLE_SMALL_NUMBER
					? FMath::Clamp(static_cast<double>((Point - A).Dot(Delta)) / Denominator, 0.0, 1.0)
					: 0.0;
				if (FVector2D::DistSquared(Point, A + Delta * Alpha) <= ToleranceSquared)
				{
					return EPointInRing::Boundary;
				}
				if ((A.Y > Point.Y) != (B.Y > Point.Y))
				{
					const double IntersectionX = A.X +
						(Point.Y - A.Y) * (B.X - A.X) / (B.Y - A.Y);
					if (Point.X < IntersectionX)
					{
						bInside = !bInside;
					}
				}
			}
			return bInside ? EPointInRing::Inside : EPointInRing::Outside;
		}

		bool PolygonContains(
			const FProjectWorldCanonicalPolygon& Polygon,
			const FVector2D& Point,
			double Tolerance)
		{
			const EPointInRing Outer = PointInRing(Polygon.Outer, Point, Tolerance);
			if (Outer == EPointInRing::Outside)
			{
				return false;
			}
			for (const TArray<FVector2D>& Hole : Polygon.Holes)
			{
				if (PointInRing(Hole, Point, Tolerance) == EPointInRing::Inside)
				{
					return false;
				}
			}
			return true;
		}

	}

	bool Prepare(
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldTerrainWaterConformanceContext& OutContext,
		FString& OutError)
	{
		OutContext = FProjectWorldTerrainWaterConformanceContext();
		TMap<FString, FVector4d> FeatureDomainBounds;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			TSet<FString> FeatureIds;
			FeatureIds.Append(Cell.OwnedFeatureIds);
			FeatureIds.Append(Cell.ReferencedFeatureIds);
			for (const FString& FeatureId : FeatureIds)
			{
				const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
				if (Feature == nullptr || Feature->FeatureClass != TEXT("water") ||
					!Feature->WaterSurface.bValid)
				{
					continue;
				}
				FVector4d* Bounds = FeatureDomainBounds.Find(FeatureId);
				if (Bounds == nullptr)
				{
					FeatureDomainBounds.Add(FeatureId, Cell.Bounds);
					continue;
				}
				Bounds->X = FMath::Min(Bounds->X, Cell.Bounds.X);
				Bounds->Y = FMath::Min(Bounds->Y, Cell.Bounds.Y);
				Bounds->Z = FMath::Max(Bounds->Z, Cell.Bounds.Z);
				Bounds->W = FMath::Max(Bounds->W, Cell.Bounds.W);
			}
		}

		TArray<FString> FeatureIds;
		FeatureDomainBounds.GetKeys(FeatureIds);
		FeatureIds.Sort();
		for (const FString& FeatureId : FeatureIds)
		{
			const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
			check(Feature != nullptr);
			FProjectWorldTerrainWaterSurface& Entry = OutContext.Surfaces.Add(FeatureId);
			Entry.Feature = Feature;
			if (Feature->WaterSurface.Geometry == TEXT("polygon"))
			{
				for (int32 PolygonIndex = 0; PolygonIndex < Feature->GeometryPolygons.Num(); ++PolygonIndex)
				{
					const FProjectWorldCanonicalPolygon& Polygon = Feature->GeometryPolygons[PolygonIndex];
					if (Polygon.Outer.IsEmpty())
					{
						continue;
					}
					FProjectWorldTerrainWaterPolygon& PreparedPolygon = Entry.Polygons.AddDefaulted_GetRef();
					PreparedPolygon.PolygonIndex = PolygonIndex;
					PreparedPolygon.Bounds = FVector4d(
						Polygon.Outer[0].X,
						Polygon.Outer[0].Y,
						Polygon.Outer[0].X,
						Polygon.Outer[0].Y);
					for (const FVector2D& Point : Polygon.Outer)
					{
						PreparedPolygon.Bounds.X = FMath::Min(PreparedPolygon.Bounds.X, Point.X);
						PreparedPolygon.Bounds.Y = FMath::Min(PreparedPolygon.Bounds.Y, Point.Y);
						PreparedPolygon.Bounds.Z = FMath::Max(PreparedPolygon.Bounds.Z, Point.X);
						PreparedPolygon.Bounds.W = FMath::Max(PreparedPolygon.Bounds.W, Point.Y);
					}
				}
				continue;
			}
			FProjectWorldPreparedWaterSurface PreparedSurface;
			if (!ProjectWorldWaterMeshBuilder::PrepareSurface(
				*Feature,
				FeatureDomainBounds.FindChecked(FeatureId),
				PreparedSurface,
				OutError))
			{
				OutError = FString::Printf(
					TEXT("Cannot prepare Water footprint %s for Terrain conditioning: %s"),
					*FeatureId,
					*OutError);
				return false;
			}
			for (int32 Index = 0; Index + 2 < PreparedSurface.TriangleVertices.Num(); Index += 3)
			{
				FProjectWorldTerrainWaterTriangle& Triangle = Entry.Triangles.AddDefaulted_GetRef();
				Triangle.A = PreparedSurface.TriangleVertices[Index];
				Triangle.B = PreparedSurface.TriangleVertices[Index + 1];
				Triangle.C = PreparedSurface.TriangleVertices[Index + 2];
				Triangle.Bounds = FVector4d(
					FMath::Min3(Triangle.A.X, Triangle.B.X, Triangle.C.X),
					FMath::Min3(Triangle.A.Y, Triangle.B.Y, Triangle.C.Y),
					FMath::Max3(Triangle.A.X, Triangle.B.X, Triangle.C.X),
					FMath::Max3(Triangle.A.Y, Triangle.B.Y, Triangle.C.Y));
			}
		}
		return true;
	}

	bool BuildCellHeights(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldTerrainWaterConformanceContext& Context,
		const FProjectWorldCanonicalCell& Cell,
		TArray<double>& OutHeightsMeters,
		FProjectWorldTerrainWaterConformanceStats& OutStats,
		FString& OutError)
	{
		OutHeightsMeters = Cell.Terrain.HeightsMeters;
		OutStats = FProjectWorldTerrainWaterConformanceStats();
		if (OutHeightsMeters.Num() != Cell.Terrain.SamplesX * Cell.Terrain.SamplesY)
		{
			OutError = FString::Printf(
				TEXT("Terrain sample inventory is incomplete for Water conformance: %s"),
				*Cell.CellId);
			return false;
		}

		const double CoordinateTolerance = Bundle.CoordinateQuantizationMeters;
		const double CrossTolerance = FMath::Square(CoordinateTolerance);
		TArray<double> WaterHeights;
		WaterHeights.Init(TNumericLimits<double>::Max(), OutHeightsMeters.Num());
		TSet<FString> CellFeatureIds;
		CellFeatureIds.Append(Cell.OwnedFeatureIds);
		CellFeatureIds.Append(Cell.ReferencedFeatureIds);
		for (const FString& FeatureId : CellFeatureIds)
		{
			const FProjectWorldTerrainWaterSurface* Entry = Context.Surfaces.Find(FeatureId);
			if (Entry == nullptr)
			{
				continue;
			}
			auto SetWaterHeight = [&](int32 Row, int32 Column, const FVector2D& Point)
			{
				double& WaterHeight = WaterHeights[Row * Cell.Terrain.SamplesX + Column];
				WaterHeight = FMath::Min(
					WaterHeight,
					ProjectWorldWaterMeshBuilder::EvaluateSurfaceZ(
						*Entry->Feature,
						Point,
						Bundle.HeightQuantizationMeters));
			};
			for (const FProjectWorldTerrainWaterPolygon& PreparedPolygon : Entry->Polygons)
			{
				if (!Entry->Feature->GeometryPolygons.IsValidIndex(PreparedPolygon.PolygonIndex))
				{
					OutError = FString::Printf(
						TEXT("Prepared Water polygon index is invalid for Terrain conditioning: %s"),
						*FeatureId);
					return false;
				}
				if (PreparedPolygon.Bounds.Z < Cell.Terrain.Bounds.X - CoordinateTolerance ||
					PreparedPolygon.Bounds.X > Cell.Terrain.Bounds.Z + CoordinateTolerance ||
					PreparedPolygon.Bounds.W < Cell.Terrain.Bounds.Y - CoordinateTolerance ||
					PreparedPolygon.Bounds.Y > Cell.Terrain.Bounds.W + CoordinateTolerance)
				{
					continue;
				}
				const FProjectWorldCanonicalPolygon& Polygon =
					Entry->Feature->GeometryPolygons[PreparedPolygon.PolygonIndex];
				const int32 MinimumColumn = FMath::Clamp(
					FMath::CeilToInt((PreparedPolygon.Bounds.X - CoordinateTolerance - Cell.Terrain.Bounds.X) /
						Cell.Terrain.SampleSpacing.X),
					0,
					Cell.Terrain.SamplesX - 1);
				const int32 MaximumColumn = FMath::Clamp(
					FMath::FloorToInt((PreparedPolygon.Bounds.Z + CoordinateTolerance - Cell.Terrain.Bounds.X) /
						Cell.Terrain.SampleSpacing.X),
					0,
					Cell.Terrain.SamplesX - 1);
				const int32 MinimumRow = FMath::Clamp(
					FMath::CeilToInt((Cell.Terrain.Bounds.W - PreparedPolygon.Bounds.W - CoordinateTolerance) /
						Cell.Terrain.SampleSpacing.Y),
					0,
					Cell.Terrain.SamplesY - 1);
				const int32 MaximumRow = FMath::Clamp(
					FMath::FloorToInt((Cell.Terrain.Bounds.W - PreparedPolygon.Bounds.Y + CoordinateTolerance) /
						Cell.Terrain.SampleSpacing.Y),
					0,
					Cell.Terrain.SamplesY - 1);
				for (int32 Row = MinimumRow; Row <= MaximumRow; ++Row)
				{
					const double Y = FProjectWorldCanonicalLoader::TerrainRowNorthing(Cell.Terrain, Row);
					for (int32 Column = MinimumColumn; Column <= MaximumColumn; ++Column)
					{
						const FVector2D Point(
							Cell.Terrain.Bounds.X + Column * Cell.Terrain.SampleSpacing.X,
							Y);
						if (PolygonContains(Polygon, Point, CoordinateTolerance))
						{
							SetWaterHeight(Row, Column, Point);
						}
					}
				}
			}
			for (const FProjectWorldTerrainWaterTriangle& Triangle : Entry->Triangles)
			{
				if (Triangle.Bounds.Z < Cell.Terrain.Bounds.X - CoordinateTolerance ||
					Triangle.Bounds.X > Cell.Terrain.Bounds.Z + CoordinateTolerance ||
					Triangle.Bounds.W < Cell.Terrain.Bounds.Y - CoordinateTolerance ||
					Triangle.Bounds.Y > Cell.Terrain.Bounds.W + CoordinateTolerance)
				{
					continue;
				}
				const int32 MinimumColumn = FMath::Clamp(
					FMath::CeilToInt((Triangle.Bounds.X - CoordinateTolerance - Cell.Terrain.Bounds.X) /
						Cell.Terrain.SampleSpacing.X),
					0,
					Cell.Terrain.SamplesX - 1);
				const int32 MaximumColumn = FMath::Clamp(
					FMath::FloorToInt((Triangle.Bounds.Z + CoordinateTolerance - Cell.Terrain.Bounds.X) /
						Cell.Terrain.SampleSpacing.X),
					0,
					Cell.Terrain.SamplesX - 1);
				const int32 MinimumRow = FMath::Clamp(
					FMath::CeilToInt((Cell.Terrain.Bounds.W - Triangle.Bounds.W - CoordinateTolerance) /
						Cell.Terrain.SampleSpacing.Y),
					0,
					Cell.Terrain.SamplesY - 1);
				const int32 MaximumRow = FMath::Clamp(
					FMath::FloorToInt((Cell.Terrain.Bounds.W - Triangle.Bounds.Y + CoordinateTolerance) /
						Cell.Terrain.SampleSpacing.Y),
					0,
					Cell.Terrain.SamplesY - 1);
				for (int32 Row = MinimumRow; Row <= MaximumRow; ++Row)
				{
					const double Y = FProjectWorldCanonicalLoader::TerrainRowNorthing(Cell.Terrain, Row);
					for (int32 Column = MinimumColumn; Column <= MaximumColumn; ++Column)
					{
						const FVector2D Point(
							Cell.Terrain.Bounds.X + Column * Cell.Terrain.SampleSpacing.X,
							Y);
						if (!TriangleContains(
							Triangle.A, Triangle.B, Triangle.C, Point, CrossTolerance))
						{
							continue;
						}
						SetWaterHeight(Row, Column, Point);
					}
				}
			}
		}
		for (int32 Index = 0; Index < WaterHeights.Num(); ++Index)
		{
			const double WaterHeight = WaterHeights[Index];
			if (WaterHeight == TNumericLimits<double>::Max())
			{
				continue;
			}
			++OutStats.WaterFootprintSampleCount;
			double& Height = OutHeightsMeters[Index];
			if (Height > WaterHeight)
			{
				OutStats.MaximumCorrectionMeters = FMath::Max(
					OutStats.MaximumCorrectionMeters,
					Height - WaterHeight);
				Height = WaterHeight;
				++OutStats.ConditionedSampleCount;
			}
		}
		return true;
	}

	bool BuildCellHeights(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		TArray<double>& OutHeightsMeters,
		FProjectWorldTerrainWaterConformanceStats& OutStats,
		FString& OutError)
	{
		FProjectWorldTerrainWaterConformanceContext Context;
		return Prepare(Bundle, Context, OutError) &&
			BuildCellHeights(Bundle, Context, Cell, OutHeightsMeters, OutStats, OutError);
	}
}
