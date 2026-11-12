// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedPartitionFixture.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedMeshCoverage.h"
#include "ProjectWorldConstructedOutline.h"
#include "ProjectWorldConstructedSurface.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedPartitionTest,
	"Project.World.Realization.Constructed.SharedPartition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedPartitionTest::RunTest(const FString&)
{
	const auto Sources=ProjectWorldConstructedPartitionFixture::Sources();
	FProjectWorldCanonicalBundle Frame; Frame.CanonicalCrs=TEXT("EPSG:32618"); Frame.VerticalDatum=TEXT("EPSG:3855");
	Frame.EngineGeoreferenceOriginMeters=FVector2D(585628,4511322);
	FProjectWorldCanonicalCell Cell; Cell.CellId=TEXT("captured_junction"); Cell.Bounds=FVector4(583303.41,4507602.45,584233.41,4508532.45);
	TArray<FProjectWorldSurfaceTriangle> Land,Complete;
	for (const auto& Source : Sources) { Complete.Add(Source.Face); if (Source.Owner==1) Land.Add(Source.Face); }
	FMeshDescription Mesh; FVector Origin; FString Error; TArray<int32> Trace; double Radius=123;
	TestFalse(TEXT("The captured Land needle alone cannot supply a native source-supported boundary."),
		ProjectWorldConstructedMesh::Build(Frame,Cell,Land,.001,Mesh,Origin,Error,&Trace,&Radius));
	TestTrue(TEXT("The reduced fixture retains the actual bounded-storage refusal."),Error.Contains(TEXT("No source-supported boundary within the bounded float storage interval")));
	TestTrue(TEXT("Source-boundary refusal leaves no partial output, provenance or measured displacement."),Mesh.Triangles().Num()==0 && Origin==FVector::ZeroVector && Trace.IsEmpty() && FMath::IsNaN(Radius));
	if (!TestTrue(TEXT("The unchanged source partition supplies shared physical storage without changing its frame or field."),
		ProjectWorldConstructedMesh::Build(Frame,Cell,Complete,.001,Mesh,Origin,Error,&Trace,&Radius))) { AddError(Error); return false; }
	TestEqual(TEXT("The complete field retains the original canonical cell frame."),Origin,
		FProjectWorldCanonicalLoader::CanonicalToUnreal(Frame,FVector(Cell.Bounds.X,Cell.Bounds.W,0)));
	using namespace UE::Geometry;
	TArray<FProjectWorldSurfaceTriangle> All,OwnerFaces[3],OwnerFields[3];
	TArray<FGeneralPolygon2d> Pieces[3],Expected[3]; TArray<int32> OwnerOrdinals[3],OwnerSources[3];
	for (const auto& Source : Sources)
	{
		OwnerFields[Source.Owner].Add(Source.Face); TArray<FVector2D> Points;
		for (const auto& P : Source.Face.Points) Points.Add(FVector2D(P)-FVector2D(Cell.Bounds.X,Cell.Bounds.Y));
		FPolygon2d Polygon(Points); if (Polygon.IsClockwise()) Polygon.Reverse(); Pieces[Source.Owner].Add(FGeneralPolygon2d(Polygon));
	}
	TSet<int32> SeenSources; double Measured=0,MaximumNorm=0;
	const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions(); int32 Ordinal=0;
	for (const auto Id : Mesh.Triangles().GetElementIDs())
	{
		if (!TestTrue(TEXT("Every native output maps to one original combined source ordinal."),Trace.IsValidIndex(Ordinal) && Sources.IsValidIndex(Trace[Ordinal]))) return false;
		const int32 SourceIndex=Trace[Ordinal]; const auto& Source=Sources[SourceIndex]; FProjectWorldSurfaceTriangle Face; TSet<int32> Matched;
		for (int32 I=0; I<3; ++I) Face.Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Id,I))]);
		for (const auto& P : Face.Points)
		{
			double Closest=TNumericLimits<double>::Max(); int32 Match=INDEX_NONE;
			for (int32 I=0; I<3; ++I)
			{
				const auto Raw=FProjectWorldCanonicalLoader::CanonicalToUnreal(Frame,Source.Face.Points[I])-Origin;
				const double Distance=(FVector2D(P)-FVector2D(Raw)).Length();
				if (Distance<Closest) { Closest=Distance; Match=I; }
			}
			Measured=FMath::Max(Measured,Closest); Matched.Add(Match);
		}
		TestEqual(TEXT("The unclipped captured source retains a one-to-one corner correspondence."),Matched.Num(),3);
		const auto U=Face.Points[1]-Face.Points[0],V=Face.Points[2]-Face.Points[0]; const double D=U.X*V.Y-U.Y*V.X;
		const double Norm=FVector2D((U.Z*V.Y-U.Y*V.Z)/D,(U.X*V.Z-U.Z*V.X)/D).Length(); MaximumNorm=FMath::Max(MaximumNorm,Norm);
		const auto Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Frame,(Face.Points[0]+Face.Points[1]+Face.Points[2])/3+Origin); double Z;
		TestTrue(TEXT("Each split owner still supports its actual stored centroid."),ProjectWorldConstructedSurface::Sample(OwnerFields[Source.Owner],FVector2D(Canonical),Z,Error));
		OwnerFaces[Source.Owner].Add(Face); OwnerOrdinals[Source.Owner].Add(Ordinal++); OwnerSources[Source.Owner].Add(SourceIndex);
		SeenSources.Add(SourceIndex); All.Add(Face);
	}
	TestTrue(TEXT("A nonzero independent actual XY measurement is bounded by the published conversion displacement."),Measured>0 && FMath::IsFinite(Radius) && Measured<=Radius);
	TestTrue(TEXT("Every captured ordinary stored face meets the unchanged norm limit."),FMath::IsFinite(MaximumNorm) && MaximumNorm<=.02);
	TestEqual(TEXT("No positive unclipped source face vanishes from the common partition."),SeenSources.Num(),Sources.Num());
	for (int32 Owner=0; Owner<3; ++Owner)
	{
		TArray<FGeneralPolygon2d> SourceOutline;
		if (!TestTrue(TEXT("Each original owner's complete outline is independently retained."),ProjectWorldConstructedOutline::Extract(Pieces[Owner],SourceOutline,Error))) { AddError(Error); return false; }
		auto Ring = [&](const FPolygon2d& Input,bool Hole)
		{
			TArray<FVector2D> Points; for (const auto& P : Input.GetVertices()) Points.Add(FVector2D(P.X*100,(Cell.Bounds.W-Cell.Bounds.Y-P.Y)*100));
			FPolygon2d Out(Points); if (Out.IsClockwise()!=Hole) Out.Reverse(); return Out;
		};
		for (const auto& Polygon : SourceOutline)
		{
			FGeneralPolygon2d Out(Ring(Polygon.GetOuter(),false));
			for (const auto& Hole : Polygon.GetHoles()) if (!TestTrue(TEXT("Original owner holes survive frame conversion."),Out.AddHole(Ring(Hole,true)))) return false;
			Expected[Owner].Add(MoveTemp(Out));
		}
		ProjectWorldConstructedMeshCoverage::FMetrics Metrics;
		if (!TestTrue(TEXT("Each owner separately preserves its domain, components, holes and protected core at the measured bound."),
			ProjectWorldConstructedMeshCoverage::Validate(Expected[Owner],OwnerFaces[Owner],Radius,Metrics,Error))) { AddError(Error); return false; }
	}
	TArray<int32> Rejoined;
	for (int32 Owner=0; Owner<3; ++Owner) for (int32 I=0; I<OwnerFaces[Owner].Num(); ++I)
	{
		const int32 Original=OwnerOrdinals[Owner][I]; Rejoined.Add(Original);
		for (int32 J=0; J<3; ++J) TestTrue(TEXT("Splitting preserves exact actual ordered XYZ."),OwnerFaces[Owner][I].Points[J]==All[Original].Points[J]);
		TestEqual(TEXT("Splitting preserves the original combined source ordinal."),OwnerSources[Owner][I],Trace[Original]);
	}
	Rejoined.Sort(); TestEqual(TEXT("All split faces rejoin to the complete native census."),Rejoined.Num(),All.Num());
	for (int32 I=0; I<Rejoined.Num(); ++I) TestEqual(TEXT("No native output face is lost or duplicated by splitting."),Rejoined[I],I);
	int32 Needle=INDEX_NONE; double Smallest=TNumericLimits<double>::Max();
	for (int32 I=0; I<Sources.Num(); ++I) if (Sources[I].Owner==1)
	{
		const auto& F=Sources[I].Face; const double Area=FVector::CrossProduct(F.Points[1]-F.Points[0],F.Points[2]-F.Points[0]).Z;
		if (Area<Smallest) { Smallest=Area; Needle=I; }
	}
	auto Missing=OwnerFaces[1]; const int32 Remove=OwnerSources[1].Find(Needle);
	if (!TestTrue(TEXT("The actual positive needle remains available for a loss counterexample."),Missing.IsValidIndex(Remove) && Missing.Num()>1)) return false;
	Missing.RemoveAt(Remove); ProjectWorldConstructedMeshCoverage::FMetrics Refused;
	TestFalse(TEXT("Per-owner coverage rejects losing the actual needle while other owner faces remain."),
		ProjectWorldConstructedMeshCoverage::Validate(Expected[1],Missing,Radius,Refused,Error));
	TestTrue(TEXT("The compound field has actual stored C0 and no positive overlap."),ProjectWorldConstructedMesh::ValidateStored(All,.1,Error));
	AddInfo(FString::Printf(TEXT("SharedPartitionProbe source_faces=%d stored_faces=%d owners=3 measured_xy_cm=%.17g bound_xy_cm=%.17g maximum_norm=%.17g; full scene and gameplay remain unverified"),Sources.Num(),All.Num(),Measured,Radius,MaximumNorm));
	return !HasAnyErrors();
}
#endif
