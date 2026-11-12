// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldWaterMeshBuilder.h"
#include "ProjectWorldContainedStage.h"

#include "ProjectWorldCanonicalBundle.h"

#include "ConstrainedDelaunay2.h"
#include "Curve/GeneralPolygon2.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "Curve/PolygonOffsetUtils.h"
#include "StaticMeshAttributes.h"
#include "Utilities/ProjectSha256.h"
#include <limits>

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

		struct FSegmentSample
		{
			FVector2d Closest;
			double Height;
		};

		FSegmentSample SegmentSample(const FVector& A, const FVector& B, const FVector2d& Point)
		{
			const FVector2d Start(A.X,A.Y), Delta(B.X-A.X,B.Y-A.Y);
			const double Denominator=Delta.SquaredLength();
			const double Alpha=Denominator>UE_DOUBLE_SMALL_NUMBER
				? FMath::Clamp((Point-Start).Dot(Delta)/Denominator,0.0,1.0) : 0.0;
			return {Start+Delta*Alpha,FMath::Lerp(A.Z,B.Z,Alpha)};
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
				const auto Sample=SegmentSample(Surface.Knots[Index],Surface.Knots[Index+1],Point);
				const double DistanceSquared=(Point-Sample.Closest).SquaredLength();
				if (DistanceSquared < BestDistanceSquared)
				{
					BestDistanceSquared = DistanceSquared;
					BestZ=Sample.Height;
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

	void AppendTriangle(FMeshDescription& Description, const FVector Positions[3])
	{
		AddTriangle(Description, FPolygonGroupID(0), Positions);
	}

	double PresentationOffset(const FProjectWorldCanonicalFeature& Feature, double LayerOffsetMeters)
	{
		const auto& Surface = Feature.WaterSurface;
        if (!ProjectWorldContainedStage::Recognizes(Surface.FunctionId)) return LayerOffsetMeters;
		return Surface.bValid && Surface.FunctionVersion == 1 && Surface.Behavior == TEXT("standing") &&
			Surface.Geometry == TEXT("polygon") && Surface.ContainmentSpillMeters.IsSet() &&
			FMath::IsFinite(*Surface.ContainmentSpillMeters) && FMath::IsFinite(Surface.LevelMeters) &&
			Surface.LevelMeters < *Surface.ContainmentSpillMeters ? 0.0 : std::numeric_limits<double>::quiet_NaN();
	}

	bool ValidateStoredLevel(const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalFeature& Feature, const FVector& ActorOrigin, FString& OutError)
	{
        if (!ProjectWorldContainedStage::Recognizes(Feature.WaterSurface.FunctionId)) return true;
		const auto& Surface = Feature.WaterSurface;
		if (!FMath::IsFinite(PresentationOffset(Feature, 0.0)))
		{ OutError = TEXT("Contained Water lacks a finite authenticated spill bound."); return false; }
		const FVector Local = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,
			FVector(0, 0, Surface.LevelMeters)) - ActorOrigin;
		const double Stored = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,
			FVector(0, 0, FVector3f(Local).Z) + ActorOrigin).Z;
		if (!FMath::IsFinite(Stored) || Stored >= *Surface.ContainmentSpillMeters)
		{ OutError = TEXT("Stored float Water plane reaches or exceeds its contained spill bound."); return false; }
		return true;
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

	bool ConstantSurfaceZ(const FProjectWorldCanonicalFeature& Feature,
		TConstArrayView<FVector2D> Region,double Step,double& OutHeight)
	{
		if (Region.Num()<3) return false;
		FVector2d Center=FVector2d::Zero();
		FBox2D Bounds(ForceInit);
		double Scale=1;
		for (const auto& P : Region)
		{
			if (P.ContainsNaN()) return false;
			Bounds+=P; Center+=FVector2d(P);
			Scale=FMath::Max3(Scale,FMath::Abs(P.X),FMath::Abs(P.Y));
		}
		Center/=Region.Num();
		const auto& Surface=Feature.WaterSurface;
		if (Surface.Behavior==TEXT("standing"))
		{OutHeight=Surface.LevelMeters;return FMath::IsFinite(OutHeight);}
		if (Surface.Behavior!=TEXT("flowing") || Surface.Knots.Num()<2 || !FMath::IsFinite(Step) || Step<=0) return false;
		for (const auto& Knot : Surface.Knots)
		{
			if (Knot.ContainsNaN()) return false;
			Scale=FMath::Max3(Scale,FMath::Abs(Knot.X),FMath::Abs(Knot.Y));
		}
		double DistanceUpper=TNumericLimits<double>::Max();
		for (int32 I=0;I+1<Surface.Knots.Num();++I)
		{
			const auto Fixed=SegmentSample(Surface.Knots[I],Surface.Knots[I+1],Center).Closest;
			if (Fixed.ContainsNaN()) return false;
			double Radius=0;
			for (const auto& P : Region) Radius=FMath::Max(Radius,(FVector2d(P)-Fixed).Length());
			DistanceUpper=FMath::Min(DistanceUpper,Radius);
		}
		if (!FMath::IsFinite(DistanceUpper)) return false;
		const double Roundoff=64*std::numeric_limits<double>::epsilon();
		const double DistanceGuard=Roundoff*FMath::Max(Scale,DistanceUpper);
		double Minimum=TNumericLimits<double>::Max(),Maximum=-TNumericLimits<double>::Max();
		for (int32 I=0;I+1<Surface.Knots.Num();++I)
		{
			const FVector& A=Surface.Knots[I];const FVector& B=Surface.Knots[I+1];
			const double DX=FMath::Max3(FMath::Min(A.X,B.X)-Bounds.Max.X,
				Bounds.Min.X-FMath::Max(A.X,B.X),0.0);
			const double DY=FMath::Max3(FMath::Min(A.Y,B.Y)-Bounds.Max.Y,
				Bounds.Min.Y-FMath::Max(A.Y,B.Y),0.0);
			// The nearest segment cannot be farther than a fixed point on any
			// source segment. Box distance safely excludes impossible candidates.
			if (FMath::Sqrt(DX*DX+DY*DY)>DistanceUpper+DistanceGuard) continue;
			const double Length=FVector2d(B.X-A.X,B.Y-A.Y).Length();
			const double Gradient=Length>FMath::Sqrt(UE_DOUBLE_SMALL_NUMBER) ? FMath::Abs(B.Z-A.Z)/Length : 0;
			const double CoordinateScale=FMath::Max3(Scale,FMath::Max(FMath::Abs(A.X),FMath::Abs(A.Y)),
				FMath::Max(FMath::Abs(B.X),FMath::Abs(B.Y)));
			const double HeightGuard=Roundoff*(1+FMath::Abs(A.Z)+FMath::Abs(B.Z)+CoordinateScale*Gradient);
			// A clamped affine segment height reaches its extrema at convex
			// region vertices. Include every possibly-nearest segment and ties.
			for (const auto& P : Region)
			{
				const double H=SegmentSample(A,B,FVector2d(P)).Height;
				Minimum=FMath::Min(Minimum,H-HeightGuard);Maximum=FMath::Max(Maximum,H+HeightGuard);
			}
		}
		if (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) || Minimum>Maximum) return false;
		const double Low=Quantize(Minimum,Step),High=Quantize(Maximum,Step);
		if (Low!=High) return false;
		OutHeight=Low;return true;
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
		return PreparePolygons(Polygons, OutSurface, OutError);
	}

	bool PreparePolygons(const TArray<FGeneralPolygon2d>& Polygons,
		FProjectWorldPreparedWaterSurface& OutSurface, FString& OutError)
	{
		OutSurface = {};
		OutSurface.Footprints = Polygons;
		for (const FGeneralPolygon2d& Polygon : Polygons)
		{
			TArray<FVector2d> Vertices;
			const TArray<FIndex3i> Triangles = ConstrainedDelaunayTriangulateWithVertices(Polygon, Vertices);
			if (Triangles.IsEmpty() && FMath::Abs(Polygon.SignedArea()) > UE_DOUBLE_SMALL_NUMBER)
			{
				OutError = TEXT("GeometryAlgorithms could not triangulate the water footprint.");
				return false;
			}
			for (const FIndex3i& Triangle : Triangles)
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
		FString& OutError,
		const FVector* CommonOrigin)
	{
        OutResult = FProjectWorldWaterMeshBuildResult();
        const double Offset = PresentationOffset(Feature, SurfaceOffsetMeters);
		if (!FMath::IsFinite(SurfaceOffsetMeters) || SurfaceOffsetMeters < 0.0 || !FMath::IsFinite(Offset))
		{
			OutError = TEXT("Water surface offset must be finite and non-negative.");
			return false;
		}
		const bool bJointBoundary = ProjectWorldContainedStage::IsJointBoundaryModel(
			Feature.WaterSurface.FunctionId,Feature.WaterSurface.FunctionVersion);
		const FVector ActorOrigin = CommonOrigin ? *CommonOrigin : bJointBoundary ? FVector::ZeroVector :
			FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
		if (ActorOrigin.ContainsNaN()) { OutError=TEXT("Water mesh storage origin must be finite."); return false; }
		if (bJointBoundary && ActorOrigin!=FVector::ZeroVector)
		{OutError=TEXT("Joint Water requires the shared engine storage frame.");return false;}
		if (!ValidateStoredLevel(Bundle, Feature, ActorOrigin, OutError)) return false;

		FStaticMeshAttributes Attributes(OutResult.MeshDescription);
		Attributes.Register();
		Attributes.GetVertexInstanceUVs().SetNumChannels(1);
		const FPolygonGroupID Group = OutResult.MeshDescription.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Water");
		OutResult.ActorOrigin = ActorOrigin;
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
                    // The joint inferred model already owns its noded boundary.
                    // A second centimeter snap changes its wet/dry relationship.
                    const FVector2d CanonicalXY = bJointBoundary ? CanonicalPoints[Corner] : FVector2d(
                        Quantize(CanonicalPoints[Corner].X, Bundle.CoordinateQuantizationMeters),
                        Quantize(CanonicalPoints[Corner].Y, Bundle.CoordinateQuantizationMeters));
					const double Z = SurfaceZ(
						Feature.WaterSurface,
						CanonicalXY,
						Bundle.HeightQuantizationMeters) + Offset;
					Local[Corner] = FProjectWorldCanonicalLoader::CanonicalToUnreal(
						Bundle, FVector(CanonicalXY.X, CanonicalXY.Y, Z)) - OutResult.ActorOrigin;
				}
				// Reject exact degeneracy on the float vertices that will be emitted.
				// Quantization can collapse CDT edges even after a successful union.
				const FVector3f A(Local[0]), B(Local[1]), C(Local[2]);
				if (FVector3f::CrossProduct(B - A, C - A).Z == 0.0f)
				{
					continue;
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
