// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadNominalFootprint.h"
#include "ProjectWorldRoadEmission.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldVerifyTestFlow.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadNominalFootprintTest,
	"Project.World.Realization.Road.NominalFootprintWithoutHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadNominalFootprintTest::RunTest(const FString&)
{
	// Captured two-meter source gangway axis; no Terrain or height hypothesis enters this read.
	const FVector2D A(583300.93,4508130.52),B(583322.21,4508127.52);
	FProjectWorldCanonicalBundle Bundle; Bundle.CanonicalCrs=TEXT("EPSG:32618"); Bundle.CoordinateQuantizationMeters=.01;
	FProjectWorldCanonicalCell Cell; Cell.CellId=TEXT("source_pier25"); Cell.Bounds=FVector4d(583273.41,4508092.45,583333.41,4508152.45);
	Cell.OwnedFeatureIds={TEXT("alis:osm:way:1171058927")}; Bundle.Cells.Add(Cell);
	FProjectWorldCanonicalFeature Road; Road.FeatureId=Cell.OwnedFeatureIds[0]; Road.FeatureClass=TEXT("road"); Road.RoadClass=TEXT("footway");
	Road.WidthMeters=2; Road.OwnerCellId=Cell.CellId; Road.GeometryType=TEXT("LineString"); Road.GeometryPoints={A,B};
	auto& Representation=Road.Representations.AddDefaulted_GetRef(); Representation.CellId=Cell.CellId;
	Representation.Kind=TEXT("road_fragment"); Representation.Parts={{A,B}}; Bundle.Features.Add(Road.FeatureId,Road);
	FProjectWorldRealizationLayer Layer; Layer.LayerId=TEXT("roads"); Layer.GeneratorId=TEXT("project_road_mesh"); Layer.GeneratorVersion=1;
	Layer.DependencyHaloCells=1; Layer.NormalizedSettings=TEXT("{\"selected_classes\":[\"footway\"],\"surface_offset_m\":0.15,")
		TEXT("\"maximum_segment_length_m\":7.5,\"nanite\":true,\"collision\":\"complex_as_simple\",")
		TEXT("\"structure_fallback\":\"terrain_drape\",\"intersection_policy\":\"overlap_same_owner\"}");
	const TSet<FString> Selected{Road.FeatureId}; FProjectWorldRoadNominalFootprint Nominal; FString Error;
	if (!TestTrue(TEXT("Nominal source-width formation needs no Terrain or elevation support."),
		ProjectWorldRoadNominalFootprint::Read(Bundle,Cell,Layer,Selected,Nominal,Error))) {AddError(Error); return false;}
	TestEqual(TEXT("Three source subdivisions retain both ribbon halves."),Nominal.Triangles.Num(),6);
	TestTrue(TEXT("The selected local closure is explicit."),Nominal.LocallyEligibleFeatureIds.Contains(Road.FeatureId));
	double Area=0,Minimum=DBL_MAX,Maximum=-DBL_MAX; const FVector2D Direction=(B-A).GetSafeNormal(),Normal(-Direction.Y,Direction.X);
	for (const auto& Face : Nominal.Triangles)
	{
		const auto U=Face.Points[1]-Face.Points[0],V=Face.Points[2]-Face.Points[0]; Area+=FMath::Abs(U.X*V.Y-U.Y*V.X)*.5;
		for (const auto& P : Face.Points)
		{
			TestEqual(TEXT("Nominal XY carries no inferred height or lift."),P.Z,0.);
			const double Side=FVector2D::DotProduct(FVector2D(P)-A,Normal); Minimum=FMath::Min(Minimum,Side); Maximum=FMath::Max(Maximum,Side);
		}
	}
	const double CoordinateBound=32*std::numeric_limits<double>::epsilon()*FMath::Max(FMath::Abs(A.Y),FMath::Abs(B.Y));
	TestTrue(TEXT("Both original source-width sides survive binary64 formation."),FMath::Abs(Minimum+1)<=CoordinateBound && FMath::Abs(Maximum-1)<=CoordinateBound);
	TestTrue(TEXT("Nominal analytic ribbon area is retained within binary64 coordinate evaluation."),FMath::Abs(Area-2*(B-A).Length())<=CoordinateBound*(B-A).Length()*4);
	FProjectWorldRoadEmission Actual;
	TestFalse(TEXT("The actual stored-emission read still refuses missing Terrain support."),
		ProjectWorldRoadEmission::Read(Bundle,Cell,Layer,Selected,Actual,Error));
	Bundle.Features.FindChecked(Road.FeatureId).RoadElevation.Emplace(); FProjectWorldRoadNominalFootprint HeightChanged;
	TestTrue(TEXT("An unqualified height model does not contaminate nominal XY formation."),
		ProjectWorldRoadNominalFootprint::Read(Bundle,Cell,Layer,Selected,HeightChanged,Error));
	TestEqual(TEXT("Unconsumed elevation metadata leaves nominal geometry identity unchanged."),HeightChanged.GeometryHash,Nominal.GeometryHash);
	for (const TCHAR* Kind : {TEXT("missing"),TEXT("fragment"),TEXT("nonfinite"),TEXT("work")})
	{
		auto Broken=Bundle; auto Definition=Layer; TSet<FString> Targets=Selected;
		if (FString(Kind)==TEXT("missing")) Targets={TEXT("missing")};
		else if (FString(Kind)==TEXT("fragment")) Broken.Features.FindChecked(Road.FeatureId).Representations.Reset();
		else if (FString(Kind)==TEXT("nonfinite")) Broken.Features.FindChecked(Road.FeatureId).GeometryPoints[0].X=std::numeric_limits<double>::quiet_NaN();
		else Definition.NormalizedSettings=Definition.NormalizedSettings.Replace(TEXT(":7.5"),TEXT(":1e-10"));
		FProjectWorldRoadNominalFootprint Refused=Nominal;
		TestFalse(TEXT("Unknown selections, absent fragments, nonfinite context and excessive subdivisions refuse."),
			ProjectWorldRoadNominalFootprint::Read(Broken,Cell,Definition,Targets,Refused,Error));
		TestTrue(TEXT("Refusal publishes no partial nominal coverage or identity."),Refused.Triangles.IsEmpty() && Refused.GeometryHash.IsEmpty());
	}
	auto Large=Bundle; auto LargeCell=Cell; auto Fine=Layer; Large.Features.Reset(); LargeCell.OwnedFeatureIds.Reset();
	TSet<FString> LargeSelection;
	for (int32 I=0; I<2; ++I)
	{
		auto Copy=Road; Copy.FeatureId=FString::Printf(TEXT("nominal_budget_%d"),I);
		LargeCell.OwnedFeatureIds.Add(Copy.FeatureId); LargeSelection.Add(Copy.FeatureId); Large.Features.Add(Copy.FeatureId,Copy);
	}
	Large.Cells={LargeCell}; Fine.NormalizedSettings=Fine.NormalizedSettings.Replace(TEXT(":7.5"),TEXT(":0.001"));
	FProjectWorldRoadNominalFootprint Refused=Nominal;
	TestFalse(TEXT("The aggregate nominal read refuses when individually bounded features exceed its total face capacity."),
		ProjectWorldRoadNominalFootprint::Read(Large,LargeCell,Fine,LargeSelection,Refused,Error));
	TestTrue(TEXT("Aggregate work refusal leaves no partial footprint or hash."),Refused.Triangles.IsEmpty() && Refused.GeometryHash.IsEmpty());
	FProjectWorldCanonicalCell Remote=Cell; Remote.CellId=TEXT("remote"); Remote.OwnedFeatureIds.Reset(); FProjectWorldRoadNominalFootprint Empty;
	TestTrue(TEXT("A global selection with no local contribution is explicitly empty."),
		ProjectWorldRoadNominalFootprint::Read(Bundle,Remote,Layer,Selected,Empty,Error) && Empty.Triangles.IsEmpty() && Empty.LocallyEligibleFeatureIds.IsEmpty());
	return !HasAnyErrors();
}
#endif
