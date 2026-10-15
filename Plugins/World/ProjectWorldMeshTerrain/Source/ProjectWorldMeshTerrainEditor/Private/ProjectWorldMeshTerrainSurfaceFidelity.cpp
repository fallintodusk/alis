#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
#include "ProjectWorldCanonicalBundle.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

bool ProjectWorldMeshTerrainProducer::CompareRefinedSurface(const FProjectWorldCanonicalBundle& Bundle,
	const TArray<FProjectWorldSurfaceTriangle>& Actual, double ToleranceMeters,
	FProjectWorldMeshTerrainSurfaceFidelity& Result, FString& Error)
{
	using namespace UE::Geometry;
	Result = {};
	Error.Reset();
	TArray<FProjectWorldSurfaceTriangle> Expected;
	for (const auto& Cell : Bundle.Cells) if (!Cell.Terrain.RefinedQuads.IsEmpty())
	{
		TArray<FProjectWorldSurfaceTriangle> Faces;
		if (!ProjectWorldTerrainSurface::Triangles(Cell, FBox2D(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),
			FVector2D(Cell.Bounds.Z,Cell.Bounds.W)), Faces, Error)) return false;
		Expected.Append(Faces);
	}
	Result.ExpectedTriangles = Expected.Num();
	if (Expected.IsEmpty()) return true;
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
	FString FirstFailure;
	for (const auto& Face : Expected)
	{
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
			TArray<FVector2D> Intersection;
			if (!ProjectWorldTerrainSurface::IntersectXY(Face, Actual[Id], Intersection, Error)) { bValid=false; return; }
			const double Area = ProjectWorldTerrainSurface::Area(Intersection);
			if (Area <= 1e-10) return;
			for (int32 Prior : Relevant) for (int32 I=1; I+1<Intersection.Num(); ++I)
			{
				FProjectWorldSurfaceTriangle Clip{{FVector(Intersection[0],0),FVector(Intersection[I],0),FVector(Intersection[I+1],0)}};
				TArray<FVector2D> Overlap;
				if (!ProjectWorldTerrainSurface::IntersectXY(Clip,Actual[Prior],Overlap,Error) || ProjectWorldTerrainSurface::Area(Overlap) > 1e-8)
				{ if (FirstFailure.IsEmpty()) FirstFailure=TEXT("Overlapping compiled faces"); bValid=false; return; }
			}
			Relevant.Add(Id);
			Coverage += Area;
			for (const FVector2D& P : Intersection)
			{
				double E=0, A=0;
				if (!ProjectWorldTerrainSurface::Height(Face,P,E,Error) || !ProjectWorldTerrainSurface::Height(Actual[Id],P,A,Error))
				{ bValid=false; return; }
				Result.MaximumErrorMeters = FMath::Max(Result.MaximumErrorMeters,FMath::Abs(E-A));
				if (FMath::Abs(E-A) > ToleranceMeters) bValid=false;
			}
		};
		Tree.DoTraversal(Traversal);
		const double Area = ProjectWorldTerrainSurface::Area({FVector2D(Face.Points[0]),FVector2D(Face.Points[1]),FVector2D(Face.Points[2])});
		if (!bValid || FMath::Abs(Coverage-Area) > FMath::Max(1e-8,Area*1e-10))
		{
			++Result.MismatchedTriangles;
			if (FirstFailure.IsEmpty()) FirstFailure=FString::Printf(TEXT("coverage_m2=%.17g expected_m2=%.17g max_height_error_m=%.17g detail=%s"),Coverage,Area,Result.MaximumErrorMeters,*Error);
		}
	}
	if (Result.MismatchedTriangles > 0)
	{ Error = TEXT("Compiled final terrain has missing/overlapping coverage or incorrect interior heights: ")+FirstFailure; return false; }
	return true;
}
