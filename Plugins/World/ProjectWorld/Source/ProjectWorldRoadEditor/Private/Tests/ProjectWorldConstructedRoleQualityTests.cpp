// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRoleQuality.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedRoleQualityTest,
	"Project.World.Realization.Road.ConstructedRoleQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedRoleQualityTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedRoleQuality;
	const FString Hash=FString::ChrN(64,'a'); const TArray<FString> Roles={TEXT("fixed_pier"),TEXT("floating_assembly"),TEXT("floating_assembly")};
	auto Document = [&]()
	{
		auto Root=MakeShared<FJsonObject>(); Root->SetStringField(TEXT("model"),TEXT("native_body_role_quality_v1")); Root->SetStringField(TEXT("assembly_sha256"),Hash);
		TArray<TSharedPtr<FJsonValue>> Regions;
		for (int32 I=0; I<3; ++I)
		{
			auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("role"),I==0 ? TEXT("fixed_pier") : I==1 ? TEXT("floating_deck") : TEXT("gangway"));
			Row->SetArrayField(TEXT("source_triangle_indices"),{MakeShared<FJsonValueNumber>(I)});
			if (I<2) Row->SetNumberField(TEXT("maximum_gradient"),.02);
			else { Row->SetNumberField(TEXT("maximum_grade"),.125); Row->SetNumberField(TEXT("maximum_cross_slope"),.02); }
			Regions.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("regions"),Regions); return Root;
	};
	FPolicy Policy; FString Error;
	if (!TestTrue(TEXT("Every source ordinal binds to one supported role in the exact assembly."),Parse(*Document(),Hash,Roles,Policy,Error))) return false;
	FProjectWorldCanonicalBundle Bundle; Bundle.CanonicalCrs=TEXT("EPSG:32618"); Bundle.VerticalDatum=TEXT("EPSG:3855");
	FProjectWorldCanonicalCell Cell; Cell.CellId=TEXT("role_quality"); Cell.Bounds=FVector4(0,0,6,6);
	const TArray<FProjectWorldSurfaceTriangle> Field={
		{{FVector(0,0,0),FVector(1,0,.01),FVector(0,1,0)}},
		{{FVector(2,2,1.2),FVector(3,2,1.2),FVector(2,3,1.2)}},
		{{FVector(4,4,1.2),FVector(5,4,1.3),FVector(4,5,1.2)}}};
	FMeshDescription Mesh; FVector Origin; TArray<int32> Sources;
	if (!TestTrue(TEXT("The role gate consumes actual emitted float geometry and source provenance."),
		ProjectWorldConstructedMesh::Build(Bundle,Cell,Field,.001,Mesh,Origin,Error,&Sources))) { AddError(Error); return false; }
	TArray<TSharedPtr<FJsonValue>> Receipts;
	TestTrue(TEXT("Ordinary and gangway surfaces pass their separate frozen limits."),Validate(Policy,Mesh,Sources,FVector2D(1,0),Receipts,Error));
	TestEqual(TEXT("All three source roles emit measured receipts."),Receipts.Num(),3);
	FMeshDescription Empty;
	TestFalse(TEXT("No stored output cannot establish any measured role quality."),Validate(Policy,Empty,{},FVector2D(1,0),Receipts,Error));
	TestTrue(TEXT("No-output refusal exposes no measured receipts."),Receipts.IsEmpty());
	auto Optional=MakeShared<FJsonObject>(); FPolicy Loaded;
	TestTrue(TEXT("Neither optional policy field preserves the original fixture path."),Load(*Optional,Hash,Roles,Loaded,Error));
	Optional->SetStringField(TEXT("body_role_quality_sha256"),Hash);
	TestFalse(TEXT("A hash-only policy configuration refuses instead of disabling quality."),Load(*Optional,Hash,Roles,Loaded,Error));
	Optional->RemoveField(TEXT("body_role_quality_sha256")); Optional->SetStringField(TEXT("body_role_quality_path"),TEXT("unread"));
	TestFalse(TEXT("A path-only policy configuration refuses before reading."),Load(*Optional,Hash,Roles,Loaded,Error));
	FStaticMeshAttributes Attributes(Mesh); auto Positions=Attributes.GetVertexPositions(); bool Changed=false; int32 Ordinal=0;
	for (const auto Triangle : Mesh.Triangles().GetElementIDs())
	{
		if (Sources[Ordinal++]==0)
		{
			for (int32 I=0; I<3; ++I)
			{
				const auto Vertex=Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Triangle,I)); auto P=Positions[Vertex];
				if (P.X==100) { P.Z+=8; Positions[Vertex]=P; Changed=true; break; }
			}
		}
	}
	TestTrue(TEXT("The bad control changes the actual stored ordinary face, not its source field."),Changed);
	TArray<FProjectWorldSurfaceTriangle> Actual;
	for (const auto Triangle : Mesh.Triangles().GetElementIDs())
	{
		FProjectWorldSurfaceTriangle Face;
		for (int32 I=0; I<3; ++I) Face.Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Triangle,I))]);
		Actual.Add(Face);
	}
	ProjectWorldConstructedMesh::FSlopeMetrics Global;
	TestTrue(TEXT("The old undifferentiated gangway bound cannot reject this excessive ordinary slope."),
		ProjectWorldConstructedMesh::ValidateSlopes(Actual,FVector2D(1,0),.125,.02,Global,Error));
	TestFalse(TEXT("The exact same native output refuses under the fixed-deck role policy."),Validate(Policy,Mesh,Sources,FVector2D(1,0),Receipts,Error));
	TestTrue(TEXT("The refused measurement identifies the actual fixed source role."),Error.Contains(TEXT("role=fixed_pier")));
	auto Invalid=Document(); auto Regions=Invalid->GetArrayField(TEXT("regions")); Regions.Pop(); Invalid->SetArrayField(TEXT("regions"),Regions);
	TestFalse(TEXT("Missing source-role coverage refuses."),Parse(*Invalid,Hash,Roles,Policy,Error));
	Invalid=Document(); Invalid->GetArrayField(TEXT("regions"))[0]->AsObject()->SetArrayField(TEXT("source_triangle_indices"),{MakeShared<FJsonValueNumber>(0),MakeShared<FJsonValueNumber>(0)});
	TestFalse(TEXT("Duplicate source-role coverage refuses."),Parse(*Invalid,Hash,Roles,Policy,Error));
	Invalid=Document(); Invalid->GetArrayField(TEXT("regions"))[0]->AsObject()->SetNumberField(TEXT("maximum_gradient"),.03);
	TestFalse(TEXT("A stale or loosened ordinary limit refuses."),Parse(*Invalid,Hash,Roles,Policy,Error));
	Invalid=Document(); Invalid->GetArrayField(TEXT("regions"))[2]->AsObject()->SetStringField(TEXT("role"),TEXT("unknown_apron"));
	TestFalse(TEXT("Unknown physical quality roles refuse."),Parse(*Invalid,Hash,Roles,Policy,Error));
	TestTrue(TEXT("Policy refusal exposes no partial role map."),Policy.Regions.IsEmpty() && Policy.RegionBySource.IsEmpty());
	TestFalse(TEXT("The policy cannot be replayed against another assembly identity."),Parse(*Document(),FString::ChrN(64,'b'),Roles,Policy,Error));
	return !HasAnyErrors();
}
#endif
