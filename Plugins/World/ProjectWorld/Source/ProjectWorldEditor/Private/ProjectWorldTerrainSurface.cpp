// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldTerrainSurface.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "CompGeom/ExactPredicates.h"
#include <limits>

namespace ProjectWorldTerrainSurface
{
	bool Height(const FProjectWorldSurfaceTriangle& T, const FVector2D& P, double& Out, FString& Error)
	{
		const FVector U = T.Points[1] - T.Points[0], V = T.Points[2] - T.Points[0];
		const double D = U.X * V.Y - U.Y * V.X;
		for (const FVector& Vertex : T.Points)
			if (Vertex.ContainsNaN()) { Error = TEXT("Surface triangle has non-finite coordinates."); return false; }
		if (P.ContainsNaN() || !FMath::IsFinite(D) || FMath::Abs(D) <= 1e-12)
		{ Error = TEXT("Surface triangle has no finite nondegenerate XY height function."); return false; }
		const double X = P.X - T.Points[0].X, Y = P.Y - T.Points[0].Y;
		Out = T.Points[0].Z + ((X * V.Y - Y * V.X) * U.Z + (U.X * Y - U.Y * X) * V.Z) / D;
		if (!FMath::IsFinite(Out)) { Error = TEXT("Surface triangle height overflowed."); return false; }
		return true;
	}

	double Area(const TArray<FVector2D>& Polygon)
	{
		double Sum = 0;
		for (int32 I = 1; I + 1 < Polygon.Num(); ++I)
		{
			const FVector2D A = Polygon[I] - Polygon[0], B = Polygon[I + 1] - Polygon[0];
			Sum += A.X * B.Y - A.Y * B.X;
		}
		return FMath::Abs(Sum) * 0.5;
	}

	bool IntersectXY(const FProjectWorldSurfaceTriangle& A, const FProjectWorldSurfaceTriangle& B,
		TArray<FVector2D>& Out, FString& Error)
	{
		using namespace UE::Geometry;
		Out.Reset();
		double Dummy;
		if (!Height(A, FVector2D(A.Points[0]), Dummy, Error) ||
			!Height(B, FVector2D(B.Points[0]), Dummy, Error)) return false;
		const FVector2D Origin(A.Points[0]);
		TArray<FVector2d> AP, BP;
		for (int32 I = 0; I < 3; ++I)
		{
			AP.Add(FVector2D(A.Points[I]) - Origin);
			BP.Add(FVector2D(B.Points[I]) - Origin);
		}
		TArray<FGeneralPolygon2d> Result;
		const TArray<FGeneralPolygon2d> Subjects{FGeneralPolygon2d(FPolygon2d(AP))};
		const TArray<FGeneralPolygon2d> Clips{FGeneralPolygon2d(FPolygon2d(BP))};
		if (!PolygonsIntersection(Subjects, Clips, Result) || Result.Num() > 1 ||
			(!Result.IsEmpty() && !Result[0].GetHoles().IsEmpty()))
		{ Error = TEXT("Native triangle intersection failed its convex contract."); return false; }
		if (!Result.IsEmpty())
			for (const FVector2d& Point : Result[0].GetOuter().GetVertices()) Out.Add(Point + Origin);
		return true;
	}

	bool Triangles(const FProjectWorldCanonicalCell& Cell, const FBox2D& Bounds,
		TArray<FProjectWorldSurfaceTriangle>& Out, FString& Error, EProjectWorldTerrainQuery Query)
	{
		const auto& T = Cell.Terrain;
		Out.Reset();
		if (!Bounds.bIsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN() || T.Bounds.ContainsNaN() ||
			T.SamplesX < 2 || T.SamplesY < 2 || T.HeightsMeters.Num() != T.SamplesX * T.SamplesY ||
			T.SampleSpacing.ContainsNaN() || T.SampleSpacing.X <= 0 || T.SampleSpacing.Y <= 0)
		{ Error = TEXT("Canonical triangle surface has an invalid grid."); return false; }
		const int32 X0 = FMath::Clamp(FMath::FloorToInt((Bounds.Min.X - T.Bounds.X) / T.SampleSpacing.X), 0, T.SamplesX - 2);
		const int32 X1 = FMath::Clamp(FMath::FloorToInt((Bounds.Max.X - T.Bounds.X) / T.SampleSpacing.X), 0, T.SamplesX - 2);
		const double Y0D = FProjectWorldCanonicalLoader::TerrainSampleRow(T, Bounds.Max.Y);
		const double Y1D = FProjectWorldCanonicalLoader::TerrainSampleRow(T, Bounds.Min.Y);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(FMath::Min(Y0D, Y1D)), 0, T.SamplesY - 2);
		const int32 Y1 = FMath::Clamp(FMath::FloorToInt(FMath::Max(Y0D, Y1D)), 0, T.SamplesY - 2);
		auto Point = [&](int32 X, int32 Y)
		{ return FVector(T.Bounds.X + X * T.SampleSpacing.X,
			FProjectWorldCanonicalLoader::TerrainRowNorthing(T, Y), T.HeightsMeters[Y * T.SamplesX + X]); };
		for (int32 Y = Y0; Y <= Y1; ++Y) for (int32 X = X0; X <= X1; ++X)
		{
			const auto* Quad = Query == EProjectWorldTerrainQuery::ImmutableGrid ? nullptr :
				T.RefinedQuads.FindByPredicate([&](const auto& Q) { return Q.Coordinate == FIntPoint(X, Y); });
			if (Quad != nullptr)
			{
				if (Quad->Triangles.IsEmpty()) { Error = TEXT("Refined terrain has no replacement coverage."); return false; }
				for (const FIntVector& Face : Quad->Triangles)
				{
					if (!Quad->Vertices.IsValidIndex(Face.X) || !Quad->Vertices.IsValidIndex(Face.Y) || !Quad->Vertices.IsValidIndex(Face.Z))
					{ Error = TEXT("Refined terrain triangle index is invalid."); return false; }
					Out.Add(FProjectWorldSurfaceTriangle{{Quad->Vertices[Face.X], Quad->Vertices[Face.Y], Quad->Vertices[Face.Z]}});
				}
				continue;
			}
			const FVector A = Point(X, Y), B = Point(X + 1, Y), C = Point(X, Y + 1), D = Point(X + 1, Y + 1);
			Out.Add(FProjectWorldSurfaceTriangle{{A, C, B}});
			Out.Add(FProjectWorldSurfaceTriangle{{B, C, D}});
		}
		return true;
	}
    namespace
    {
        constexpr double ContinuityTolerance=1e-8;
		bool InsideFieldFace(const FProjectWorldSurfaceTriangle& T, const FVector& P)
		{
			const FVector A = T.Points[1]-T.Points[0], B = T.Points[2]-T.Points[0], D = P-T.Points[0];
			const double Determinant = A.X*B.Y-A.Y*B.X;
			const double U = (D.X*B.Y-D.Y*B.X)/Determinant, V = (A.X*D.Y-A.Y*D.X)/Determinant;
			return U >= -1e-9 && V >= -1e-9 && 1-U-V >= -1e-9;
		}
    }
	bool ValidateField(const TArray<FProjectWorldSurfaceTriangle>& Triangles,int32 MaximumTriangles,FString& Error)
	{
		using namespace ProjectWorldTerrainSurface;
		if (MaximumTriangles<1 || MaximumTriangles>MaximumFieldTriangles || Triangles.IsEmpty() || Triangles.Num() > MaximumTriangles)
		{ Error = TEXT("Surface needs a bounded finite triangle field."); return false; }
		TArray<FBox2D> Bounds;
		for (const auto& Face : Triangles)
		{
			double Z;
			if (!Height(Face,FVector2D(Face.Points[0]),Z,Error)) return false;
			FBox2D Box(ForceInit); for (const auto& P : Face.Points) Box += FVector2D(P);
			Bounds.Add(Box);
		}
		for (int32 I=0; I<Triangles.Num(); ++I) for (int32 J=I+1; J<Triangles.Num(); ++J)
		{
			if (!Bounds[I].Intersect(Bounds[J])) continue;
			TArray<FVector2D> Overlap;
			if (!IntersectXY(Triangles[I],Triangles[J],Overlap,Error)) return false;
			if (Area(Overlap) > 1e-9)
			{ Error = TEXT("Surface height fields overlap in XY."); return false; }
			for (int32 Side=0; Side<2; ++Side)
			{
				const auto& A = Triangles[Side ? J : I]; const auto& B = Triangles[Side ? I : J];
				for (const auto& P : A.Points) if (InsideFieldFace(B,P))
				{
					double Z;
					if (!Height(B,FVector2D(P),Z,Error)) return false;
					if (FMath::Abs(Z-P.Z) > ContinuityTolerance)
					{ Error = TEXT("Surface height field has a discontinuous edge or T junction."); return false; }
				}
			}
		}
		return true;
	}
	EFieldSampleResult TrySampleField(const TArray<FProjectWorldSurfaceTriangle>& Triangles,const FVector2D& Point,int32 MaximumTriangles,double& Out,FString& Error)
	{
		Out=std::numeric_limits<double>::quiet_NaN(); Error.Reset();
		if (MaximumTriangles<1 || MaximumTriangles>MaximumFieldTriangles || Triangles.IsEmpty() || Triangles.Num()>MaximumTriangles || Point.ContainsNaN())
		{ Error=TEXT("Surface sampling requires a bounded validated field and finite point."); return EFieldSampleResult::Invalid; }
		bool Found=false; double Candidate=0;
		for (const auto& T : Triangles)
		{
			for (const auto& P : T.Points) if (P.ContainsNaN())
			{ Error=TEXT("Surface sampling received a nonfinite field."); return EFieldSampleResult::Invalid; }
			using UE::Geometry::ExactPredicates::Orient2;
			const double Direction=Orient2(FVector2D(T.Points[0]),FVector2D(T.Points[1]),FVector2D(T.Points[2]));
			if (Direction==0 || !FMath::IsFinite(Direction))
			{ Error=TEXT("Surface sampling received a degenerate field face."); return EFieldSampleResult::Invalid; }
			bool Contains=true;
			for (int32 I=0; I<3; ++I)
				Contains &= Orient2(FVector2D(T.Points[I]),FVector2D(T.Points[(I+1)%3]),Point)*Direction>=0;
			if (!Contains) continue;
			double Height;
			if (!ProjectWorldTerrainSurface::Height(T,Point,Height,Error)) return EFieldSampleResult::Invalid;
			if (Found && FMath::Abs(Candidate-Height)>ContinuityTolerance)
			{ Error=TEXT("Surface sampling has contradictory containing faces."); return EFieldSampleResult::Invalid; }
			if (!Found) Candidate=Height;
			Found=true;
		}
		if (!Found) { Error=TEXT("Surface sample is outside the complete source field."); return EFieldSampleResult::Outside; }
		Out=Candidate; return EFieldSampleResult::Sampled;
	}
}
