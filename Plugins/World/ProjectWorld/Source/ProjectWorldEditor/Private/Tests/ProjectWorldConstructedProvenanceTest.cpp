// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldCanonicalBundle.h"
#include "CompGeom/ExactPredicates.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedProvenanceTest,
	"Project.World.Realization.Constructed.SourceProvenance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldConstructedProvenanceTest::RunTest(const FString&)
{
	FProjectWorldCanonicalBundle Bundle; Bundle.CanonicalCrs=TEXT("EPSG:32618"); Bundle.VerticalDatum=TEXT("EPSG:3855");
	FProjectWorldCanonicalCell Cell; Cell.CellId=TEXT("provenance"); Cell.Bounds=FVector4(0,0,4,4);
	// Binary-exact axis intersections isolate source provenance from clipping precision.
	const TArray<FProjectWorldSurfaceTriangle> Field={
		{{FVector(-6,0,99),FVector(-4,0,99),FVector(-6,2,99)}},
		{{FVector(-1,-1,0),FVector(7,-1,0),FVector(-1,7,0)}},
		{{FVector(7,-1,0),FVector(7,7,0),FVector(-1,7,0)}}};
	FMeshDescription Plain,Traced; FVector PlainOrigin,TracedOrigin; FString Error; TArray<int32> Sources={-1}; double Radius=-1;
	if (!TestTrue(TEXT("The original geometry path remains valid."),
		ProjectWorldConstructedMesh::Build(Bundle,Cell,Field,.001,Plain,PlainOrigin,Error))) { AddError(Error); return false; }
	if (!TestTrue(TEXT("The same physical mesh can return source provenance."),
		ProjectWorldConstructedMesh::Build(Bundle,Cell,Field,.001,Traced,TracedOrigin,Error,&Sources,&Radius))) { AddError(Error); return false; }
	TestTrue(TEXT("Successful conversion publishes a finite nonnegative measured XY bound."),FMath::IsFinite(Radius) && Radius>=0);
	TestTrue(TEXT("Requesting provenance preserves the exact original frame."),PlainOrigin==TracedOrigin);
	TestEqual(TEXT("Requesting provenance preserves vertex allocation."),Plain.Vertices().Num(),Traced.Vertices().Num());
	TestEqual(TEXT("Requesting provenance preserves instance allocation."),Plain.VertexInstances().Num(),Traced.VertexInstances().Num());
	TestEqual(TEXT("One source ordinal corresponds to each actual output triangle."),Sources.Num(),Traced.Triangles().Num());
	int32 FirstSourceFaces=0; for (const int32 Source : Sources) if (Source==1) ++FirstSourceFaces;
	TestTrue(TEXT("Clipping the larger source triangle creates multiple physical output faces."),FirstSourceFaces>1);
	TestFalse(TEXT("Clipped-out input faces do not renumber the remaining source ordinals."),Sources.Contains(0));
	FStaticMeshConstAttributes Attributes(Traced);
	int32 Ordinal=0;
	for (const auto Triangle : Traced.Triangles().GetElementIDs())
	{
		if (!TestTrue(TEXT("Each actual output ordinal names an original source face."),
			Sources.IsValidIndex(Ordinal) && Field.IsValidIndex(Sources[Ordinal]))) return false;
		FVector Centre=FVector::ZeroVector;
		for (int32 I=0; I<3; ++I)
			Centre+=FVector(Attributes.GetVertexPositions()[Traced.GetVertexInstanceVertex(Traced.GetTriangleVertexInstance(Triangle,I))])+TracedOrigin;
		Centre=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Centre/3);
		const auto& Source=Field[Sources[Ordinal++]]; using UE::Geometry::ExactPredicates::Orient2;
		for (int32 I=0; I<3; ++I)
			TestTrue(TEXT("The actual clipped triangle remains within its reported source support."),
				Orient2(FVector2D(Source.Points[I]),FVector2D(Source.Points[(I+1)%3]),FVector2D(Centre))>=0);
	}
	auto Snapshot = [](const FMeshDescription& Mesh)
	{
		TArray<FString> Result; FStaticMeshConstAttributes A(Mesh);
		for (const auto Triangle : Mesh.Triangles().GetElementIDs())
		{
			Result.Add(A.GetPolygonGroupMaterialSlotNames()[Mesh.GetTrianglePolygonGroup(Triangle)].ToString());
			for (int32 I=0; I<3; ++I)
			{
				const auto Instance=Mesh.GetTriangleVertexInstance(Triangle,I);
				const auto P=A.GetVertexPositions()[Mesh.GetVertexInstanceVertex(Instance)];
				const auto N=A.GetVertexInstanceNormals()[Instance],T=A.GetVertexInstanceTangents()[Instance];
				const auto UV=A.GetVertexInstanceUVs().Get(Instance,0); const auto C=A.GetVertexInstanceColors()[Instance];
				Result.Add(FString::Printf(TEXT("%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g"),
					P.X,P.Y,P.Z,N.X,N.Y,N.Z,T.X,T.Y,T.Z,UV.X,UV.Y,C.X,C.Y,C.Z,C.W,A.GetVertexInstanceBinormalSigns()[Instance]));
			}
		}
		return Result;
	};
	TestTrue(TEXT("Trace-on and trace-off preserve positions, winding, shading, UVs, colors and materials exactly."),Snapshot(Plain)==Snapshot(Traced));
	const FProjectWorldSurfaceTriangle Exact={{FVector(0,0,0),FVector(1,0,0),FVector(0,1,0)}};
	const FProjectWorldSurfaceTriangle Inexact={{FVector(2,2,.123456789),FVector(3,2,.123456789),FVector(2,3,.123456789)}};
	FMeshDescription Failed; FVector FailedOrigin; TArray<int32> FailedSources;
	if (!TestTrue(TEXT("The first face emits successfully under the exact-height policy."),
		ProjectWorldConstructedMesh::Build(Bundle,Cell,{Exact},0,Failed,FailedOrigin,Error,&FailedSources,&Radius))) { AddError(Error); return false; }
	TestTrue(TEXT("The successful first face has actual emitted source provenance."),!FailedSources.IsEmpty());
	TestFalse(TEXT("A later nonrepresentable height refuses after the first input face can emit."),
		ProjectWorldConstructedMesh::Build(Bundle,Cell,{Exact,Inexact},0,Failed,FailedOrigin,Error,&FailedSources,&Radius));
	TestTrue(TEXT("This is the late stored-height refusal, not an invalid source preflight."),Error.Contains(TEXT("stored height exceeds")));
	TestTrue(TEXT("Late refusal exposes neither partial geometry nor a stale source trace."),
		Failed.Triangles().Num()==0 && FailedOrigin.IsZero() && FailedSources.IsEmpty() && FMath::IsNaN(Radius));
	Radius=123;
	TestFalse(TEXT("Early invalid input also refuses an apparent displacement measurement."),
		ProjectWorldConstructedMesh::Build(Bundle,Cell,{},.001,Failed,FailedOrigin,Error,&FailedSources,&Radius));
	TestTrue(TEXT("An unread source conversion cannot report a successful zero measurement."),FMath::IsNaN(Radius));
	return !HasAnyErrors();
}
#endif
