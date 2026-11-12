// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldConstructedOutline.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedOutlineTest,
    "Project.World.Realization.Constructed.Outline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedOutlineTest::RunTest(const FString&)
{
    // Captured projected approach grid: exact shared nodes, rotated ends and unequal float axis spacing.
    // OSM contributors/ODbL: way 1524187754 axis; width offsets and end pads are synthetic.
    // Flat height isolates footprint conversion from geographic policy and slope construction.
    const FVector2D Points[]={
		FVector2D(583372.4469119443,4508183.905848016),
		FVector2D(583370.8190431827,4508182.743934381),
		FVector2D(583373.0278687617,4508183.091913635),
		FVector2D(583371.4,4508181.93),
		FVector2D(583373.608825579,4508182.277979254),
		FVector2D(583371.9809568174,4508181.116065619),
		FVector2D(583370.4085431827,4508182.450934381),
		FVector2D(583370.9895,4508181.637),
		FVector2D(583371.5704568174,4508180.823065619),
		FVector2D(583369.9980431827,4508182.157934381),
		FVector2D(583370.579,4508181.344),
		FVector2D(583371.1599568174,4508180.530065618),
		FVector2D(583369.5875431827,4508181.864934381),
		FVector2D(583370.1685,4508181.051),
		FVector2D(583370.7494568174,4508180.237065619),
		FVector2D(583369.1770431827,4508181.571934381),
		FVector2D(583369.758,4508180.757999999),
		FVector2D(583370.3389568174,4508179.944065618),
		FVector2D(583368.7665431827,4508181.278934381),
		FVector2D(583369.3475,4508180.465),
		FVector2D(583369.9284568174,4508179.651065619),
		FVector2D(583368.3560431827,4508180.985934381),
		FVector2D(583368.937,4508180.172),
		FVector2D(583369.5179568174,4508179.358065619),
		FVector2D(583367.9455431827,4508180.692934381),
		FVector2D(583368.5265,4508179.879),
		FVector2D(583369.1074568174,4508179.065065619),
		FVector2D(583367.5350431827,4508180.399934381),
		FVector2D(583368.116,4508179.586),
		FVector2D(583368.6969568174,4508178.772065619),
		FVector2D(583367.1245431827,4508180.106934381),
		FVector2D(583367.7055,4508179.293),
		FVector2D(583368.2864568174,4508178.4790656185),
		FVector2D(583366.7140431826,4508179.813934381),
		FVector2D(583367.2949999999,4508179.0),
		FVector2D(583367.8759568173,4508178.186065619),
		FVector2D(583366.3035431826,4508179.5209343815),
		FVector2D(583366.8844999999,4508178.707),
		FVector2D(583367.4654568173,4508177.893065619),
		FVector2D(583365.8930431826,4508179.227934381),
		FVector2D(583366.4739999999,4508178.414),
		FVector2D(583367.0549568173,4508177.600065619),
		FVector2D(583365.4825431826,4508178.934934381),
		FVector2D(583366.0634999999,4508178.121),
		FVector2D(583366.6444568173,4508177.307065619),
		FVector2D(583365.0720431826,4508178.641934381),
		FVector2D(583365.6529999999,4508177.828),
		FVector2D(583366.2339568173,4508177.014065619),
		FVector2D(583364.6615431826,4508178.348934381),
		FVector2D(583365.2424999999,4508177.535),
		FVector2D(583365.8234568173,4508176.721065619),
		FVector2D(583364.2510431826,4508178.055934382),
		FVector2D(583364.8319999999,4508177.242000001),
		FVector2D(583365.4129568173,4508176.428065619),
		FVector2D(583363.8405431826,4508177.762934381),
		FVector2D(583364.4214999999,4508176.949),
		FVector2D(583365.0024568173,4508176.135065619),
		FVector2D(583363.4300431826,4508177.469934382),
		FVector2D(583364.0109999999,4508176.656),
		FVector2D(583364.5919568173,4508175.842065619),
		FVector2D(583363.0195431826,4508177.176934381),
		FVector2D(583363.6005,4508176.363),
		FVector2D(583364.1814568173,4508175.549065619),
		FVector2D(583362.6090431826,4508176.883934381),
		FVector2D(583363.19,4508176.07),
		FVector2D(583363.7709568173,4508175.256065619),
		FVector2D(583360.9811744209,4508175.722020746),
		FVector2D(583361.5621312383,4508174.908086365),
		FVector2D(583362.1430880557,4508174.094151984),
    };
    const FIntVector Indices[]={
		FIntVector(0,1,2),
		FIntVector(1,3,2),
		FIntVector(2,3,4),
		FIntVector(3,5,4),
		FIntVector(1,6,3),
		FIntVector(6,7,3),
		FIntVector(3,7,5),
		FIntVector(7,8,5),
		FIntVector(6,9,7),
		FIntVector(9,10,7),
		FIntVector(7,10,8),
		FIntVector(10,11,8),
		FIntVector(9,12,10),
		FIntVector(12,13,10),
		FIntVector(10,13,11),
		FIntVector(13,14,11),
		FIntVector(12,15,13),
		FIntVector(15,16,13),
		FIntVector(13,16,14),
		FIntVector(16,17,14),
		FIntVector(15,18,16),
		FIntVector(18,19,16),
		FIntVector(16,19,17),
		FIntVector(19,20,17),
		FIntVector(18,21,19),
		FIntVector(21,22,19),
		FIntVector(19,22,20),
		FIntVector(22,23,20),
		FIntVector(21,24,22),
		FIntVector(24,25,22),
		FIntVector(22,25,23),
		FIntVector(25,26,23),
		FIntVector(24,27,25),
		FIntVector(27,28,25),
		FIntVector(25,28,26),
		FIntVector(28,29,26),
		FIntVector(27,30,28),
		FIntVector(30,31,28),
		FIntVector(28,31,29),
		FIntVector(31,32,29),
		FIntVector(30,33,31),
		FIntVector(33,34,31),
		FIntVector(31,34,32),
		FIntVector(34,35,32),
		FIntVector(33,36,34),
		FIntVector(36,37,34),
		FIntVector(34,37,35),
		FIntVector(37,38,35),
		FIntVector(36,39,37),
		FIntVector(39,40,37),
		FIntVector(37,40,38),
		FIntVector(40,41,38),
		FIntVector(39,42,40),
		FIntVector(42,43,40),
		FIntVector(40,43,41),
		FIntVector(43,44,41),
		FIntVector(42,45,43),
		FIntVector(45,46,43),
		FIntVector(43,46,44),
		FIntVector(46,47,44),
		FIntVector(45,48,46),
		FIntVector(48,49,46),
		FIntVector(46,49,47),
		FIntVector(49,50,47),
		FIntVector(48,51,49),
		FIntVector(51,52,49),
		FIntVector(49,52,50),
		FIntVector(52,53,50),
		FIntVector(51,54,52),
		FIntVector(54,55,52),
		FIntVector(52,55,53),
		FIntVector(55,56,53),
		FIntVector(54,57,55),
		FIntVector(57,58,55),
		FIntVector(55,58,56),
		FIntVector(58,59,56),
		FIntVector(57,60,58),
		FIntVector(60,61,58),
		FIntVector(58,61,59),
		FIntVector(61,62,59),
		FIntVector(60,63,61),
		FIntVector(63,64,61),
		FIntVector(61,64,62),
		FIntVector(64,65,62),
		FIntVector(63,66,64),
		FIntVector(66,67,64),
		FIntVector(64,67,65),
		FIntVector(67,68,65),
    };
    TArray<FProjectWorldSurfaceTriangle> Field;
    for (const auto& T : Indices) Field.Add({{FVector(Points[T.X],3),FVector(Points[T.Y],3),FVector(Points[T.Z],3)}});
    FProjectWorldCanonicalBundle Bundle;
    Bundle.EngineGeoreferenceOriginMeters=FVector2D(585628,4511322);
    FProjectWorldCanonicalCell Cell;
    Cell.Bounds=FVector4(583303.41,4507602.45,584233.41,4508532.45);
    FMeshDescription Mesh; FVector Origin; FString Error;
    if (!TestTrue(TEXT("A complete edge-connected captured cover retains one physical component."),
        ProjectWorldConstructedMesh::Build(Bundle,Cell,Field,.001,Mesh,Origin,Error))) AddError(Error);
    else TestEqual(TEXT("The complete captured cover retains every native face."),Mesh.Triangles().Num(),88);
	using namespace UE::Geometry;
	using ProjectWorldConstructedOutline::Extract;
	auto Polygon=[](std::initializer_list<FVector2D> P) { return FGeneralPolygon2d(FPolygon2d(TArray<FVector2D>(P))); };
	TArray<FGeneralPolygon2d> Result;
	const TArray<FGeneralPolygon2d> Junction={
		Polygon({FVector2D(0,0),FVector2D(100,0),FVector2D(0,100)}),
		Polygon({FVector2D(100,0),FVector2D(100,100),FVector2D(50,50)}),
		Polygon({FVector2D(100,100),FVector2D(0,100),FVector2D(50,50)})};
	TestTrue(TEXT("An exact existing-node T junction remains one conforming outline."),Extract(Junction,Result,Error));
	if (TestEqual(TEXT("T junction has one component."),Result.Num(),1))
		TestEqual(TEXT("Temporary topology fans do not alter its source area."),Result[0].SignedArea(),10000.);
	const auto First=Polygon({FVector2D(0,0),FVector2D(100,0),FVector2D(100,100),FVector2D(0,100)});
	const auto Remote=Polygon({FVector2D(200,200),FVector2D(300,200),FVector2D(300,300),FVector2D(200,300)});
	const auto Touch=Polygon({FVector2D(100,100),FVector2D(200,100),FVector2D(200,200),FVector2D(100,200)});
	TestTrue(TEXT("Genuinely separated components retain separate outlines."),Extract({First,Remote},Result,Error));
	TestEqual(TEXT("Separated component count is exact."),Result.Num(),2);
	TestTrue(TEXT("A point contact cannot merge edge-connected components."),Extract({First,Touch},Result,Error));
	TestEqual(TEXT("Point-touching components remain two."),Result.Num(),2);
	TArray<FGeneralPolygon2d> Annulus;
	const FVector2D Outer[4]={FVector2D(0,0),FVector2D(100,0),FVector2D(100,100),FVector2D(0,100)};
	const FVector2D Inner[4]={FVector2D(40,40),FVector2D(60,40),FVector2D(60,60),FVector2D(40,60)};
	for (int32 I=0; I<4; ++I)
	{
		const int32 J=(I+1)%4;
		Annulus.Add(Polygon({Outer[I],Outer[J],Inner[J],Inner[I]}));
	}
	TestTrue(TEXT("A genuine hole retains its connected outer owner."),Extract(Annulus,Result,Error));
	if (TestEqual(TEXT("The annulus has one outer component."),Result.Num(),1))
		TestEqual(TEXT("The declared hole is neither filled nor unassigned."),Result[0].GetHoles().Num(),1);
	Annulus.Add(Polygon({FVector2D(45,45),FVector2D(55,45),FVector2D(55,55),FVector2D(45,55)}));
	TestTrue(TEXT("An island inside a hole retains its own edge-connected owner."),Extract(Annulus,Result,Error));
	TestEqual(TEXT("Nested island produces two physical components."),Result.Num(),2);
	TestFalse(TEXT("Duplicate source pieces refuse."),Extract({First,First},Result,Error));
	TestTrue(TEXT("Refusal clears prior outline output."),Result.IsEmpty());
	TestFalse(TEXT("Even sub-tolerance positive overlap cannot invent outline topology."),Extract({
		Polygon({FVector2D(0,0),FVector2D(1,0),FVector2D(0,1)}),
		Polygon({FVector2D(1-1e-6,0),FVector2D(2,0),FVector2D(1-1e-6,1)})},Result,Error));
	TestTrue(TEXT("The tiny overlap control hits exact positive overlap refusal."),Error.Contains(TEXT("exact positive area")));
	TestFalse(TEXT("A nonconvex piece is not silently repaired."),Extract({Polygon({
		FVector2D(0,0),FVector2D(100,0),FVector2D(25,25),FVector2D(0,100)})},Result,Error));
	TestFalse(TEXT("A collapsed piece has no certifiable closed boundary."),Extract({Polygon({
		FVector2D(0,0),FVector2D(50,50),FVector2D(100,100)})},Result,Error));
	TestTrue(TEXT("An empty clipped domain has an empty outline."),Extract({},Result,Error));
	TestTrue(TEXT("No stale components survive an empty domain."),Result.IsEmpty());
	return !HasAnyErrors();
}
#endif
