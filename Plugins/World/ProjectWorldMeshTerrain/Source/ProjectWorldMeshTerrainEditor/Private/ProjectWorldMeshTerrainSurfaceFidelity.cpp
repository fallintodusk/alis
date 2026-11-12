#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
#include "ProjectWorldCanonicalBundle.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "CompGeom/ExactPredicates.h"
#include "Intersection/IntrTriangle2Triangle2.h"

namespace
{
	FProjectWorldSurfaceTriangle InComparisonFrame(const FProjectWorldSurfaceTriangle& Triangle, const FVector2D& Origin)
	{
		auto Local = Triangle;
		for (auto& Point : Local.Points) { Point.X -= Origin.X; Point.Y -= Origin.Y; }
		return Local;
	}

	bool IntersectComparedTriangles(const FProjectWorldSurfaceTriangle& A,const FProjectWorldSurfaceTriangle& B,
		TArray<FVector2D>& Out,FString& Error)
	{
		using namespace UE::Geometry;
		Out.Reset(); double Dummy;
		if (!ProjectWorldTerrainSurface::Height(A,FVector2D(A.Points[0]),Dummy,Error) ||
			!ProjectWorldTerrainSurface::Height(B,FVector2D(B.Points[0]),Dummy,Error)) return false;
		const FVector2D Origin(A.Points[0]);
		FTriangle2d Triangles[2];
		for (int32 Shape=0;Shape<2;++Shape)
		{
			const auto& Face=Shape==0 ? A : B;
			for (int32 I=0;I<3;++I)
			{
				Triangles[Shape].V[I]=FVector2D(Face.Points[I])-Origin;
				if (Triangles[Shape].V[I].ContainsNaN()) {Error=TEXT("Comparison triangle has nonfinite local coordinates.");return false;}
			}
			const double Winding=ExactPredicates::Orient2(Triangles[Shape].V[0],Triangles[Shape].V[1],Triangles[Shape].V[2]);
			if (!FMath::IsFinite(Winding) || Winding==0) {Error=TEXT("Comparison triangle has no finite local interior.");return false;}
			if (Winding<0)
				Swap(Triangles[Shape].V[1],Triangles[Shape].V[2]);
		}
		// Compare convex triangles directly; a general polygon overlay can split near-coincident edges.
		FIntrTriangle2Triangle2d Intersection(Triangles[0],Triangles[1]);
		Intersection.Find();
		if ((Intersection.Result!=EIntersectionResult::Intersects && Intersection.Result!=EIntersectionResult::NoIntersection) ||
			Intersection.Quantity<0 || Intersection.Quantity>6)
		{ Error=TEXT("Native comparison triangle intersection has an invalid result."); return false; }
		for (int32 I=0;I<Intersection.Quantity;++I)
		{
			const FVector2D Point=Intersection.Points[I]+Origin;
			if (Point.ContainsNaN()) {Out.Reset();Error=TEXT("Native comparison triangle intersection is nonfinite.");return false;}
			Out.Add(Point);
		}
		return true;
	}

	bool SeparatedInteriors(const FProjectWorldSurfaceTriangle& First,const FProjectWorldSurfaceTriangle& Second)
	{
		using UE::Geometry::ExactPredicates::Orient2;
		FVector2D Points[2][3]; double Winding[2];
		for (int32 Shape=0; Shape<2; ++Shape)
		{
			const auto& Face=Shape==0 ? First : Second;
			for (int32 I=0; I<3; ++I)
			{
				if (Face.Points[I].ContainsNaN()) return false;
				Points[Shape][I]=FVector2D(Face.Points[I]);
			}
			Winding[Shape]=Orient2(Points[Shape][0],Points[Shape][1],Points[Shape][2]);
			if (!FMath::IsFinite(Winding[Shape]) || Winding[Shape]==0) return false;
		}
		for (int32 Shape=0; Shape<2; ++Shape) for (int32 Edge=0; Edge<3; ++Edge)
		{
			bool Outside=true;
			for (const auto& Point : Points[1-Shape])
			{
				const double Side=Orient2(Points[Shape][Edge],Points[Shape][(Edge+1)%3],Point);
				if (!FMath::IsFinite(Side)) return false;
				if (Winding[Shape]>0 ? Side>0 : Side<0) { Outside=false; break; }
			}
			if (Outside) return true;
		}
		return false;
	}
}

static bool CompareSurface(const FProjectWorldCanonicalBundle& Bundle,
	const TArray<FProjectWorldSurfaceTriangle>& Actual, double ToleranceMeters,
	FProjectWorldMeshTerrainSurfaceFidelity& Result, FString& Error,bool IncludeBase)
{
	using namespace UE::Geometry;
	Result = {};
	Error.Reset();
	TArray<FProjectWorldSurfaceTriangle> Expected;
	for (const auto& Cell : Bundle.Cells) if (IncludeBase || !Cell.Terrain.RefinedQuads.IsEmpty())
	{
		TArray<FProjectWorldSurfaceTriangle> Faces;
		if (!ProjectWorldTerrainSurface::Triangles(Cell, FBox2D(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),
			FVector2D(Cell.Bounds.Z,Cell.Bounds.W)), Faces, Error)) return false;
		Expected.Append(Faces);
	}
	Result.ExpectedTriangles = Expected.Num();
	if (Expected.IsEmpty())
	{
		if (IncludeBase) Error=TEXT("Selected canonical Terrain has no expected triangles.");
		return !IncludeBase;
	}
	if (Actual.IsEmpty()) { Error = TEXT("Compiled refined terrain is missing."); Result.MismatchedTriangles = Expected.Num(); return false; }
	FDynamicMesh3 Planar;
	for (const auto& Face : Actual)
	{
		int32 Vertices[3];
		for (int32 I=0; I<3; ++I)
		{
			if (Face.Points[I].ContainsNaN()) { Error = TEXT("Compiled refined terrain is nonfinite."); return false; }
			Vertices[I] = Planar.AppendVertex(FVector(Face.Points[I].X, Face.Points[I].Y, 0));
		}
		if (Planar.AppendTriangle(Vertices[0],Vertices[1],Vertices[2]) < 0)
		{ Error = TEXT("Compiled refined terrain has invalid topology."); return false; }
	}
	FDynamicMeshAABBTree3 Tree(&Planar, true);
	TArray<double> ActualCoverage;
	if (IncludeBase) ActualCoverage.Init(0,Actual.Num());
	FString FirstFailure;
	for (const auto& Face : Expected)
	{
		// Restoring derived cuts to large canonical XY can round away coverage.
		// Keep area, height and derived overlap in the same local comparison frame.
		const FVector2D ComparisonOrigin(Face.Points[0]);
		const auto LocalFace = InComparisonFrame(Face,ComparisonOrigin);
		FBox2D Bounds(ForceInit);
		for (const FVector& P : Face.Points) Bounds += FVector2D(P);
		bool bValid = true;
		double Coverage = 0;
		TArray<int32> Relevant;
		FDynamicMeshAABBTree3::FTreeTraversal Traversal;
		// Native 3D box intersection excludes zero-thickness projected boxes.
		Traversal.NextBoxF = [&](const FAxisAlignedBox3d& Box, int)
		{ return FBox2D(FVector2D(Box.Min),FVector2D(Box.Max)).Intersect(Bounds); };
		Traversal.NextTriangleF = [&](int32 Id)
		{
			if (!bValid) return;
			FBox2D ActualBounds(ForceInit);
			for (const FVector& P : Actual[Id].Points) ActualBounds += FVector2D(P);
			if (!ActualBounds.Intersect(Bounds)) return;
			// Disjoint interiors cannot contribute coverage, regardless of overlay rounding.
			if (SeparatedInteriors(Face,Actual[Id])) return;
			const auto LocalActual = InComparisonFrame(Actual[Id],ComparisonOrigin);
			TArray<FVector2D> Intersection;
			if (!IntersectComparedTriangles(LocalFace, LocalActual, Intersection, Error)) { bValid=false; return; }
			const double Area = ProjectWorldTerrainSurface::Area(Intersection);
			if (Area <= 1e-10) return;
			const auto Winding=[](const FProjectWorldSurfaceTriangle& Triangle)
			{
				return ExactPredicates::Orient2(FVector2D(Triangle.Points[0]),FVector2D(Triangle.Points[1]),FVector2D(Triangle.Points[2]));
			};
			const double SourceWinding=Winding(Face),StoredWinding=Winding(Actual[Id]);
			if (!FMath::IsFinite(SourceWinding) || !FMath::IsFinite(StoredWinding) || SourceWinding==0 || StoredWinding==0 ||
				(SourceWinding>0)!=(StoredWinding>0))
			{ Error=TEXT("Compiled face reverses or loses canonical winding."); bValid=false; return; }
			for (int32 Prior : Relevant)
			{
				// Native overlay can move a derived clip beyond its source face.
				// Exact separation of the actual faces rules out that synthetic overlap.
				if (SeparatedInteriors(Actual[Id],Actual[Prior])) continue;
				for (int32 I=1; I+1<Intersection.Num(); ++I)
				{
					FProjectWorldSurfaceTriangle Clip{{FVector(Intersection[0],0),FVector(Intersection[I],0),FVector(Intersection[I+1],0)}};
					TArray<FVector2D> Overlap;
					if (!IntersectComparedTriangles(Clip,InComparisonFrame(Actual[Prior],ComparisonOrigin),Overlap,Error) || ProjectWorldTerrainSurface::Area(Overlap) > 1e-8)
					{ if (FirstFailure.IsEmpty()) FirstFailure=TEXT("Overlapping compiled faces"); bValid=false; return; }
				}
			}
			Relevant.Add(Id);
			Coverage += Area;
			if (IncludeBase) ActualCoverage[Id] += Area;
			for (const FVector2D& P : Intersection)
			{
				double E=0, A=0;
				if (!ProjectWorldTerrainSurface::Height(LocalFace,P,E,Error) || !ProjectWorldTerrainSurface::Height(LocalActual,P,A,Error))
				{ bValid=false; return; }
				Result.MaximumErrorMeters = FMath::Max(Result.MaximumErrorMeters,FMath::Abs(E-A));
				if (FMath::Abs(E-A) > ToleranceMeters) bValid=false;
			}
		};
		Tree.DoTraversal(Traversal);
		const double Area = ProjectWorldTerrainSurface::Area({FVector2D(LocalFace.Points[0]),FVector2D(LocalFace.Points[1]),FVector2D(LocalFace.Points[2])});
		if (!bValid || FMath::Abs(Coverage-Area) > FMath::Max(1e-8,Area*1e-10))
		{
			++Result.MismatchedTriangles;
			if (FirstFailure.IsEmpty()) FirstFailure=FString::Printf(TEXT("coverage_m2=%.17g expected_m2=%.17g max_height_error_m=%.17g detail=%s"),Coverage,Area,Result.MaximumErrorMeters,*Error);
		}
	}
	if (Result.MismatchedTriangles > 0)
	{ Error = TEXT("Compiled final terrain has missing/overlapping coverage or incorrect interior heights: ")+FirstFailure; return false; }
	if (IncludeBase) for (int32 Id=0;Id<Actual.Num();++Id)
	{
		const auto Local=InComparisonFrame(Actual[Id],FVector2D(Actual[Id].Points[0]));
		const double Area=ProjectWorldTerrainSurface::Area({FVector2D(Local.Points[0]),FVector2D(Local.Points[1]),FVector2D(Local.Points[2])});
		if (!FMath::IsFinite(Area) || Area<=0 || !FMath::IsFinite(ActualCoverage[Id]) || ActualCoverage[Id]<=0 ||
			FMath::Abs(ActualCoverage[Id]-Area)>FMath::Max(1e-8,Area*1e-10))
		{
			++Result.MismatchedTriangles;
			Error=TEXT("Compiled Terrain has degenerate faces or coverage outside the complete selected canonical field.");
			return false;
		}
	}
	return true;
}

bool ProjectWorldMeshTerrainProducer::CompareRefinedSurface(const FProjectWorldCanonicalBundle& Bundle,
	const TArray<FProjectWorldSurfaceTriangle>& Actual,double ToleranceMeters,
	FProjectWorldMeshTerrainSurfaceFidelity& Result,FString& Error)
{
	return CompareSurface(Bundle,Actual,ToleranceMeters,Result,Error,false);
}

bool ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(const FProjectWorldCanonicalBundle& Bundle,
	const TArray<FProjectWorldSurfaceTriangle>& Actual,double ToleranceMeters,
	FProjectWorldMeshTerrainSurfaceFidelity& Result,FString& Error)
{
	return CompareSurface(Bundle,Actual,ToleranceMeters,Result,Error,true);
}
