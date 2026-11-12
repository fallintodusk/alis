// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRuntimeInput.h"
#include "ProjectWorldConstructedRuntimeRoutes.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedGroupedRuntimeInputTest,
	"Project.World.Realization.Constructed.GroupedRuntimeInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedGroupedRuntimeInputTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedRuntimeTest;
	FInput Model; Model.Grouped=true; FString Error; double Height;
	Model.SourceGroups={{TEXT("body"),TEXT("constructed_body"),{{{FVector(0,0,1),FVector(1,0,1),FVector(0,1,1)}}}},
		{TEXT("ground"),TEXT("inferred_ground"),{{{FVector(1,0,1),FVector(1,1,1),FVector(0,1,1)}}}}};
	TestTrue(TEXT("Grouped support can come from a distinct structural field."),CanonicalHeight(Model,FVector2D(.2,.2),Height,Error) && Height==1);
	TestTrue(TEXT("Ground support is consumed from its own field, without structural aliasing."),CanonicalHeight(Model,FVector2D(.8,.8),Height,Error) && Height==1);
	TestTrue(TEXT("A shared model boundary requires consistent support."),CanonicalHeight(Model,FVector2D(.5,.5),Height,Error) && Height==1);
	Height=123;
	TestFalse(TEXT("Unknown grouped support refuses."),CanonicalHeight(Model,FVector2D(2,2),Height,Error));
	TestTrue(TEXT("Unknown support cannot preserve an earlier successful height."),FMath::IsNaN(Height));
	for (auto& P : Model.SourceGroups[1].Field[0].Points) P.Z=2;
	TestFalse(TEXT("Contradictory source groups refuse at their shared boundary."),CanonicalHeight(Model,FVector2D(.5,.5),Height,Error));
	TestTrue(TEXT("Contradictory grouped support remains unknown."),FMath::IsNaN(Height));
	Model.SourceGroups[1].Field.Reset();
	TestFalse(TEXT("An invalid group cannot be silently treated as absent even when another group supports the point."),CanonicalHeight(Model,FVector2D(.2,.2),Height,Error));
	FInput WaterModel; WaterModel.Grouped=true; WaterModel.Frame.HeightOriginMeters=10;
	WaterModel.SourceGroups={{TEXT("body"),TEXT("constructed_body"),{}},{TEXT("ground"),TEXT("inferred_ground"),{}}};
	FProjectWorldStoredSurface Deck; Deck.Origin=FVector(0,0,-1000);
	Deck.Triangles={{{FVector(0,0,125),FVector(100,0,125),FVector(0,100,125)}}};
	WaterModel.Units={Deck,Deck}; WaterModel.UnitGroups={TEXT("body"),TEXT("ground")};
	for (auto& P : WaterModel.Units[1].Triangles[0].Points) P.Z=-500;
	TMap<FString,double> Measured;
	TestTrue(TEXT("Actual native deck height consumes local cm, native origin, height datum and representative Water; Ground is distinct."),
		MeasureDeckWater(WaterModel,0,1.2,Measured,Error) && Measured.Num()==1 && Measured.FindChecked(TEXT("body"))==1.25);
	TestFalse(TEXT("Raising representative Water must detect insufficient deck separation."),MeasureDeckWater(WaterModel,.1,1.2,Measured,Error));
	TestTrue(TEXT("Failed Water measurement clears previous successful values."),Measured.IsEmpty());
	TestFalse(TEXT("Unknown Water cannot become zero."),MeasureDeckWater(WaterModel,std::numeric_limits<double>::quiet_NaN(),1.2,Measured,Error));
	WaterModel.Units[0].Triangles[0].Points[1].Z=119;
	TestFalse(TEXT("A low noncentral deck vertex refuses even when the face centroid passes."),MeasureDeckWater(WaterModel,0,1.2,Measured,Error));
	WaterModel.Units[0]=Deck; WaterModel.SourceGroups.Add({TEXT("missing_apron"),TEXT("inferred_gate_apron"),{}});
	TestFalse(TEXT("A declared structural owner without actual stored faces cannot pass."),MeasureDeckWater(WaterModel,0,1.2,Measured,Error));
	WaterModel.SourceGroups.Pop(); WaterModel.SourceGroups[0].Role=TEXT("unknown");
	TestFalse(TEXT("An unknown structural role cannot hide from the Water gate."),MeasureDeckWater(WaterModel,0,1.2,Measured,Error));
	FInput LaneModel; TArray<FWalkLane> Lanes;
	if (!TestTrue(TEXT("Lane controls use the self-contained structural runtime fixture."),Load(LaneModel,Error,TEXT(""),TEXT("")))) return false;
	LaneModel.Routes={{TEXT("source_polyline"),2,{LaneModel.Start,FVector(0,0,135),LaneModel.End}}};
	TestTrue(TEXT("Source lane construction retains every original point at centre and both capsule margins."),
		BuildWalkingLanes(LaneModel,34,Lanes,Error) && Lanes.Num()==3 && Lanes[0].Points==LaneModel.Routes[0].Points &&
		Lanes[1].OffsetCentimeters==-66 && Lanes[2].OffsetCentimeters==66 && Lanes[1].Points.Num()==3 && Lanes[2].Points.Num()==3);
	LaneModel.Routes[0].WidthMeters=.4;
	TestFalse(TEXT("A nominal source width narrower than the capsule refuses instead of shrinking the capsule."),BuildWalkingLanes(LaneModel,34,Lanes,Error));
	TestTrue(TEXT("Lane refusal cannot expose an earlier valid plan."),Lanes.IsEmpty());
	LaneModel.Routes[0].WidthMeters=2; LaneModel.Routes[0].Points[1]=FVector(1e6,1e6,0);
	TestFalse(TEXT("An intermediate source point outside model support cannot be skipped."),BuildWalkingLanes(LaneModel,34,Lanes,Error));
	const FString Path=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_INPUT"));
	if (!Path.IsEmpty())
	{
		FInput Input; const FString Hash=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_SHA256"));
		if (!TestTrue(TEXT("The exact grouped runtime fixture binds all physical sources, native meshes and complete routes."),Load(Input,Error,Path,Hash))) { AddError(Error); return false; }
		TestTrue(TEXT("The grouped input preserves separate fields and explicit native source namespaces."),Input.Grouped && Input.Field.IsEmpty() &&
			Input.SourceGroups.Num()==5 && Input.UnitGroups.Num()==Input.Units.Num() && Input.UnitSources.Num()==Input.Units.Num());
		int32 Segments=0,Vertices=0,Faces=0;
		for (const auto& Route : Input.Routes) { Segments+=Route.Points.Num()-1; Vertices+=Route.Points.Num(); }
		for (const auto& Unit : Input.Units) Faces+=Unit.Triangles.Num();
		TestEqual(TEXT("Every structural owner has measured actual native deck/Water separation."),Input.DeckHeightsAboveWater.Num(),4);
		for (const auto& Pair : Input.DeckHeightsAboveWater)
			AddInfo(FString::Printf(TEXT("NativeDeckAboveRepresentativeWater owner=%s minimum_m=%.17g; top surface only, not underside clearance"),*Pair.Key,Pair.Value));
		TestEqual(TEXT("All eleven full source routes are retained."),Input.Routes.Num(),11);
		TestTrue(TEXT("Curved/perimeter routes retain their intermediate source vertices."),Segments>Input.Routes.Num());
		AddInfo(FString::Printf(TEXT("GroupedRuntimeInput groups=%d units=%d faces=%d routes=%d vertices=%d segments=%d input=%s; parsing/model support only, not gameplay"),
			Input.SourceGroups.Num(),Input.Units.Num(),Faces,Input.Routes.Num(),Vertices,Segments,*Hash));
		TestFalse(TEXT("A stale runtime identity cannot consume this scene."),Load(Input,Error,Path,FString::ChrN(64,'0')));
		TestTrue(TEXT("Runtime identity refusal leaves no partial groups or physical output."),Input.Units.IsEmpty() && Input.SourceGroups.IsEmpty());
	}
	return !HasAnyErrors();
}
#endif
