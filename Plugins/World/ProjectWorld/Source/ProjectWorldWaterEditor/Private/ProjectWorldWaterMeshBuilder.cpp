// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldWaterMeshBuilder.h"

#include "ProjectWorldCanonicalBundle.h"

#include "ConstrainedDelaunay2.h"
#include "Curve/GeneralPolygon2.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "Curve/PolygonOffsetUtils.h"
#include "StaticMeshAttributes.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldWaterMeshBuilder
{
	using namespace UE::Geometry;

	namespace
	{
		TArray<FVector2d> OpenRing(const TArray<FVector2D>& Ring)
		{
			TArray<FVector2d> Result;
			Result.Reserve(Ring.Num());
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

		bool BuildPolygon(
			const FProjectWorldCanonicalPolygon& Source,
			FGeneralPolygon2d& OutPolygon,
			FString& OutError)
		{
			TArray<FVector2d> Outer = OpenRing(Source.Outer);
			if (Outer.Num() < 3)
			{
				OutError = TEXT("Canonical water polygon has fewer than three outer vertices.");
				return false;
			}
			OutPolygon = FGeneralPolygon2d(FPolygon2d(MoveTemp(Outer)));
			for (const TArray<FVector2D>& SourceHole : Source.Holes)
			{
				TArray<FVector2d> Hole = OpenRing(SourceHole);
				if (Hole.Num() < 3 || !OutPolygon.AddHole(FPolygon2d(MoveTemp(Hole))))
				{
					OutError = TEXT("Canonical water polygon contains an invalid hole.");
					return false;
				}
			}
			return true;
		}

		FGeneralPolygon2d CellPolygon(const FVector4d& Bounds)
		{
			return FGeneralPolygon2d(FPolygon2d({
				FVector2d(Bounds.X, Bounds.Y),
				FVector2d(Bounds.Z, Bounds.Y),
				FVector2d(Bounds.Z, Bounds.W),
				FVector2d(Bounds.X, Bounds.W)}));
		}

		double Quantize(double Value, double Step)
		{
			return FMath::RoundToDouble(Value / Step) * Step;
		}

		double SurfaceZ(
			const FProjectWorldCanonicalWaterSurface& Surface,
			const FVector2d& Point,
			double HeightStep)
		{
			if (Surface.Behavior == TEXT("standing"))
			{
				return Surface.LevelMeters;
			}
			double BestDistanceSquared = TNumericLimits<double>::Max();
			double BestZ = Surface.Knots[0].Z;
			for (int32 Index = 0; Index + 1 < Surface.Knots.Num(); ++Index)
			{
				const FVector2d Start(Surface.Knots[Index].X, Surface.Knots[Index].Y);
				const FVector2d End(Surface.Knots[Index + 1].X, Surface.Knots[Index + 1].Y);
				const FVector2d Delta = End - Start;
				const double Denominator = Delta.SquaredLength();
				const double Alpha = Denominator > UE_DOUBLE_SMALL_NUMBER
					? FMath::Clamp((Point - Start).Dot(Delta) / Denominator, 0.0, 1.0)
					: 0.0;
				const double DistanceSquared = (Point - (Start + Delta * Alpha)).SquaredLength();
				if (DistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = DistanceSquared;
					BestZ = FMath::Lerp(Surface.Knots[Index].Z, Surface.Knots[Index + 1].Z, Alpha);
				}
			}
			return Quantize(BestZ, HeightStep);
		}

		bool SurfacePolygons(
			const FProjectWorldCanonicalFeature& Feature,
			const FVector4d& TargetBounds,
			TArray<FGeneralPolygon2d>& OutPolygons,
			FString& OutError)
		{
			TArray<FGeneralPolygon2d> Unclipped;
			if (Feature.WaterSurface.Geometry == TEXT("polygon"))
			{
				for (const FProjectWorldCanonicalPolygon& Source : Feature.GeometryPolygons)
				{
					FGeneralPolygon2d Polygon;
					if (!BuildPolygon(Source, Polygon, OutError))
					{
						return false;
					}
					Unclipped.Add(MoveTemp(Polygon));
				}
			}
			else
			{
				for (const TArray<FVector2D>& Part : Feature.GeometryParts)
				{
					TArray<FVector2d> Axis;
					for (const FVector2D& Point : Part)
					{
						Axis.Add(FVector2d(Point.X, Point.Y));
					}
					if (Axis.Num() < 2)
					{
						continue;
					}
					FOffsetPolygon2d Offset;
					Offset.Polygons.Add(MakeArrayView(Axis));
					// The exact UE 5.8 twin pins Offset as total realized open-path width.
					Offset.Offset = Feature.WidthMeters;
					Offset.JoinType = EPolygonOffsetJoinType::Round;
					Offset.EndType = EPolygonOffsetEndType::Round;
					Offset.MaxStepsPerRadian = 16.0 / PI;
					if (!Offset.ComputeResult())
					{
						OutError = TEXT("GeometryAlgorithms could not buffer a canonical water ribbon.");
						return false;
					}
					Unclipped.Append(MoveTemp(Offset.Result));
				}
			}
			if (Unclipped.IsEmpty())
			{
				return true;
			}
			const TArray<FGeneralPolygon2d> Cells{CellPolygon(TargetBounds)};
			if (!PolygonsIntersection(Unclipped, Cells, OutPolygons))
			{
				OutError = TEXT("GeometryAlgorithms could not clip canonical water to its cell.");
				return false;
			}
			return true;
		}

		void AddTriangle(
			FMeshDescription& Description,
			const FPolygonGroupID Group,
			const FVector Positions[3])
		{
			FStaticMeshAttributes Attributes(Description);
			TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
			TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
			TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
			TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
			TArray<FVertexInstanceID> Instances;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const FVector& Position = Positions[Corner];
				const FVertexID Vertex = Description.CreateVertex();
				VertexPositions[Vertex] = FVector3f(Position);
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
				Normals[Instance] = FVector3f::ZAxisVector;
				Tangents[Instance] = FVector3f::XAxisVector;
				BinormalSigns[Instance] = 1.0f;
				Colors[Instance] = FVector4f::One();
				UVs.Set(Instance, 0, FVector2f(Position.X, Position.Y) * 0.01f);
				Instances.Add(Instance);
			}
			Description.CreatePolygon(Group, Instances);
		}

		bool BuildSemanticDigest(
			const FProjectWorldCanonicalFeature& Feature,
			FProjectWorldWaterMeshBuildResult& Result)
		{
			FStaticMeshAttributes Attributes(Result.MeshDescription);
			TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
			FString SemanticText = FString::Printf(
				TEXT("water_mesh_v1|feature=%s|group=%s|geometry=%s|behavior=%s|function=%s:v%d|")
				TEXT("origin=%.17g,%.17g,%.17g|triangles=%d|"),
				*Feature.FeatureId,
				*Feature.WaterSurface.SurfaceGroupId,
				*Feature.WaterSurface.Geometry,
				*Feature.WaterSurface.Behavior,
				*Feature.WaterSurface.FunctionId,
				Feature.WaterSurface.FunctionVersion,
				Result.ActorOrigin.X,
				Result.ActorOrigin.Y,
				Result.ActorOrigin.Z,
				Result.TriangleCount);
			for (const FVertexID Vertex : Result.MeshDescription.Vertices().GetElementIDs())
			{
				const FVector3f Position = Positions[Vertex];
				SemanticText += FString::Printf(
					TEXT("v=%.9g,%.9g,%.9g|"), Position.X, Position.Y, Position.Z);
			}
			FTCHARToUTF8 Utf8(*SemanticText);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, Result.SemanticDigest);
		}
	}

	double EvaluateSurfaceZ(
		const FProjectWorldCanonicalFeature& Feature,
		const FVector2D& CanonicalPoint,
		double HeightQuantizationMeters)
	{
		return SurfaceZ(
			Feature.WaterSurface,
			FVector2d(CanonicalPoint),
			HeightQuantizationMeters);
	}

	bool PrepareSurface(
		const FProjectWorldCanonicalFeature& Feature,
		const FVector4d& TargetBounds,
		FProjectWorldPreparedWaterSurface& OutSurface,
		FString& OutError)
	{
		OutSurface = FProjectWorldPreparedWaterSurface();
		if (!Feature.WaterSurface.bValid || Feature.FeatureClass != TEXT("water") ||
			(Feature.WaterSurface.Behavior == TEXT("flowing") && Feature.WaterSurface.Knots.Num() < 2) ||
			(Feature.WaterSurface.Geometry == TEXT("ribbon") && Feature.WidthMeters <= 0.0))
		{
			OutError = TEXT("Canonical water surface authority is incomplete.");
			return false;
		}
		TArray<FGeneralPolygon2d> Polygons;
		if (!SurfacePolygons(Feature, TargetBounds, Polygons, OutError))
		{
			return false;
		}
		for (const FGeneralPolygon2d& Polygon : Polygons)
		{
			TArray<FVector2d> Vertices;
			for (const FIndex3i& Triangle : ConstrainedDelaunayTriangulateWithVertices(Polygon, Vertices))
			{
				OutSurface.TriangleVertices.Add(FVector2D(Vertices[Triangle.A]));
				OutSurface.TriangleVertices.Add(FVector2D(Vertices[Triangle.B]));
				OutSurface.TriangleVertices.Add(FVector2D(Vertices[Triangle.C]));
			}
		}
		return true;
	}

	bool BuildCellSurface(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldCanonicalFeature& Feature,
		const FProjectWorldPreparedWaterSurface& PreparedSurface,
		double SurfaceOffsetMeters,
		FProjectWorldWaterMeshBuildResult& OutResult,
		FString& OutError)
	{
		if (!FMath::IsFinite(SurfaceOffsetMeters) || SurfaceOffsetMeters < 0.0)
		{
			OutError = TEXT("Water surface offset must be finite and non-negative.");
			return false;
		}
		OutResult = FProjectWorldWaterMeshBuildResult();

		FStaticMeshAttributes Attributes(OutResult.MeshDescription);
		Attributes.Register();
		Attributes.GetVertexInstanceUVs().SetNumChannels(1);
		const FPolygonGroupID Group = OutResult.MeshDescription.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Water");
		OutResult.ActorOrigin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
			Bundle, FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
		const FAxisAlignedBox2d CellBox(
			FVector2d(Cell.Bounds.X, Cell.Bounds.Y),
			FVector2d(Cell.Bounds.Z, Cell.Bounds.W));
		for (int32 Index = 0; Index + 2 < PreparedSurface.TriangleVertices.Num(); Index += 3)
		{
			FPolygon2d Clipped({
				FVector2d(PreparedSurface.TriangleVertices[Index]),
				FVector2d(PreparedSurface.TriangleVertices[Index + 1]),
				FVector2d(PreparedSurface.TriangleVertices[Index + 2])});
			if (!Clipped.Bounds().Intersects(CellBox))
			{
				continue;
			}
			Clipped.ClipConvex(CellBox);
			if (Clipped.VertexCount() < 3)
			{
				continue;
			}
			OutResult.CanonicalAreaSquareMeters += FMath::Abs(Clipped.SignedArea());
			for (int32 TriangleIndex = 1; TriangleIndex + 1 < Clipped.VertexCount(); ++TriangleIndex)
			{
				const FVector2d CanonicalPoints[3] = {
					Clipped[0], Clipped[TriangleIndex], Clipped[TriangleIndex + 1]};
				FVector Local[3];
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const FVector2d CanonicalXY(
						Quantize(CanonicalPoints[Corner].X, Bundle.CoordinateQuantizationMeters),
						Quantize(CanonicalPoints[Corner].Y, Bundle.CoordinateQuantizationMeters));
					const double Z = SurfaceZ(
						Feature.WaterSurface,
						CanonicalXY,
						Bundle.HeightQuantizationMeters) + SurfaceOffsetMeters;
					Local[Corner] = FProjectWorldCanonicalLoader::CanonicalToUnreal(
						Bundle, FVector(CanonicalXY.X, CanonicalXY.Y, Z)) - OutResult.ActorOrigin;
				}
				// Unreal treats clockwise triangles as front-facing. Keep the authored +Z
				// vertex normals, but reverse counter-clockwise XY triangles so the
				// generated surface renders when viewed from above rather than below.
				if (FVector::CrossProduct(Local[1] - Local[0], Local[2] - Local[0]).Z > 0.0)
				{
					Swap(Local[1], Local[2]);
				}
				AddTriangle(OutResult.MeshDescription, Group, Local);
				++OutResult.TriangleCount;
			}
		}
		if (!BuildSemanticDigest(Feature, OutResult))
		{
			OutError = TEXT("Cannot hash generated water mesh semantics.");
			return false;
		}
		return true;
	}

	bool BuildCellSurface(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldCanonicalFeature& Feature,
		double SurfaceOffsetMeters,
		FProjectWorldWaterMeshBuildResult& OutResult,
		FString& OutError)
	{
		FProjectWorldPreparedWaterSurface PreparedSurface;
		return PrepareSurface(Feature, Cell.Bounds, PreparedSurface, OutError) &&
			BuildCellSurface(
				Bundle,
				Cell,
				Feature,
				PreparedSurface,
				SurfaceOffsetMeters,
				OutResult,
				OutError);
	}
}
