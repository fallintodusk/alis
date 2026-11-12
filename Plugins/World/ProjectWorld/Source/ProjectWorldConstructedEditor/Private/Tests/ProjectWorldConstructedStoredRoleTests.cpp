// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedVerifyFixture.h"
#include "ProjectWorldConstructedPlan.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedStoredRoleTest,
	"Project.World.Realization.Constructed.StoredRoleQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedStoredRoleTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedRealization;
	const auto Bundle=ProjectWorldConstructedVerifyFixture::Bundle();
	FPlan Planned; FString Error;
	if (!TestTrue(TEXT("The production planner accepts independently oriented gangways and cross-cell ordinary support."),
		Plan(Bundle,ProjectWorldConstructedVerifyFixture::Layer(),Planned,Error))) {AddError(Error); return false;}
	TestEqual(TEXT("The two features emit three physical units."),Planned.Units.Num(),3);
	for (const auto& Unit : Planned.Units)
	{
		const auto& Surface=*Unit.Surface;
		TestTrue(TEXT("Actual native descriptions satisfy their own declared roles."),Qualify(Surface,Unit.Description,Error));
		if (Unit.FeatureId==TEXT("fixture:gangways"))
		{
			auto WrongAxis=Surface; WrongAxis.FaceRegions[1].AxisEnd=WrongAxis.FaceRegions[1].AxisStart+FVector2D(4,0);
			TestFalse(TEXT("A different same-role region cannot borrow the first gangway axis."),Qualify(WrongAxis,Unit.Description,Error));
			auto Missing=Surface; Missing.FaceRegions[1].SourceTriangles.Reset();
			TestFalse(TEXT("Missing region correspondence refuses."),Qualify(Missing,Unit.Description,Error));
			auto Misrouted=Unit.Description; auto Sources=Misrouted.TriangleAttributes().GetAttributesRef<int32>(SourceFaceAttribute);
			for (const auto T : Misrouted.Triangles().GetElementIDs()) Sources[T]=(Sources[T]+2)%4;
			TestFalse(TEXT("Routing stored faces to the perpendicular source region refuses."),Qualify(Surface,Misrouted,Error));
			auto TooSteep=Unit.Description; auto Positions=FStaticMeshAttributes(TooSteep).GetVertexPositions();
			for (const auto V : TooSteep.Vertices().GetElementIDs()) Positions[V].Z*=2;
			TestFalse(TEXT("Actual stored along-axis grade above policy refuses."),Qualify(Surface,TooSteep,Error));
			auto Crossfall=Unit.Description; auto CrossPositions=FStaticMeshAttributes(Crossfall).GetVertexPositions();
			const auto CrossSources=Crossfall.TriangleAttributes().GetAttributesRef<int32>(SourceFaceAttribute); TSet<FVertexID> CrossVertices;
			for (const auto T : Crossfall.Triangles().GetElementIDs()) if (CrossSources[T]<2)
				for (int32 I=0; I<3; ++I) CrossVertices.Add(Crossfall.GetVertexInstanceVertex(Crossfall.GetTriangleVertexInstance(T,I)));
			for (const auto V : CrossVertices) CrossPositions[V].Z+=CrossPositions[V].Y*.03f;
			TestFalse(TEXT("Actual stored crossfall refuses while along-axis grade remains within policy."),Qualify(Surface,Crossfall,Error));
		}
		else
		{
			auto TooSteep=Unit.Description; auto Positions=FStaticMeshAttributes(TooSteep).GetVertexPositions();
			for (const auto V : TooSteep.Vertices().GetElementIDs()) Positions[V].Z+=Positions[V].Y*.03f;
			TestFalse(TEXT("Actual stored ordinary gradient norm above policy refuses."),Qualify(Surface,TooSteep,Error));
		}
	}
	return !HasAnyErrors();
}
#endif
