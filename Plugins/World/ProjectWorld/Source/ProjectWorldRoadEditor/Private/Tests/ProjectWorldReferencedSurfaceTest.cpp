// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldRoadEmission.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldRoadReferencedSurface.h"
#include "ProjectWorldRoadElevation.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"
#include <limits>
#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldReferencedSurfaceTest
{
	TArray<FProjectWorldCanonicalSurfaceTriangle> CapturedRamp()
	{
		return {{{FVector(583322.21,4508127.52,3.55),FVector(583303.56,4508131.61,1.2),FVector(583322.39,4508128.86,3.55)}},
			{{FVector(583303.56,4508131.61,1.2),FVector(583322.21,4508127.52,3.55),FVector(583322,4508126.2,3.55)}},
			{{FVector(583303.56,4508131.61,1.2),FVector(583322,4508126.2,3.55),FVector(583302.87,4508128.99,1.2)}}};
	}
	FProjectWorldCanonicalBundle Bundle()
	{
		FProjectWorldCanonicalBundle B; B.CoordinateQuantizationMeters = .01;
		B.CanonicalCrs = TEXT("EPSG:32618"); B.VerticalDatum = TEXT("EPSG:3855");
		auto& Cell = B.Cells.AddDefaulted_GetRef();
		Cell.CellId = TEXT("cell_0"); Cell.Bounds = Cell.Terrain.Bounds = FVector4d(0,0,20,20);
		Cell.Terrain.SampleSpacing = FVector2D(10,10); Cell.Terrain.SamplesX = Cell.Terrain.SamplesY = 3;
		Cell.Terrain.HeightsMeters.Init(0,9); Cell.Terrain.ArtifactHash = FString::ChrN(64,'a');
		FProjectWorldCanonicalFeature Deck; Deck.FeatureId = TEXT("fixture:fixed-deck");
		Deck.FeatureClass = TEXT("constructed_surface"); Deck.GeometryType = TEXT("Polygon");
		Deck.ConstructedSurface.Emplace(); auto& Surface = Deck.ConstructedSurface.GetValue();
		Surface.SourceFeatureId = Deck.FeatureId; Surface.ModelHash = FString::ChrN(64,'c');
		Surface.CanonicalCrs = B.CanonicalCrs; Surface.VerticalDatum = B.VerticalDatum;
		Surface.Basis = TEXT("identified_model"); Surface.Role = TEXT("fixed_pier");
		auto Point = [](double X,double Y) { return FVector(X,Y,3.5+.01*X+.02*Y); };
		Surface.Triangles = {{{Point(0,0),Point(20,0),Point(0,20)}},{{Point(20,0),Point(20,20),Point(0,20)}}};
		B.Features.Add(Deck.FeatureId,Deck);
		FProjectWorldCanonicalFeature Road; Road.FeatureId = TEXT("fixture:platform-path");
		Road.FeatureClass = TEXT("road"); Road.GeometryType = TEXT("LineString"); Road.RoadClass = TEXT("footway");
		Road.GeometryPoints = {FVector2D(3,5),FVector2D(9,5),FVector2D(9,16)};
		Road.WidthMeters = 2; Road.OwnerCellId = Cell.CellId;
		auto& Representation = Road.Representations.AddDefaulted_GetRef();
		Representation.CellId = Cell.CellId; Representation.Kind = TEXT("road_fragment"); Representation.Parts = {Road.GeometryPoints};
		Road.RoadElevation.Emplace(); auto& E = Road.RoadElevation.GetValue();
		E.SourceFeatureId = Road.FeatureId; E.SourcePoints = Road.GeometryPoints;
		E.LengthMeters = 17; E.WidthMeters = 2; E.TerrainArtifacts.Add(Cell.CellId,Cell.Terrain.ArtifactHash);
		E.EndpointRelation = EProjectWorldRoadEndpointRelation::ConstructedSurface;
		E.ConstructedSurfaceHash = FString::ChrN(64,'b'); E.Domain = EProjectWorldRoadElevationDomain::ReferencedConstructedSurface;
		E.ReferencedSurfaceModels.Add(Deck.FeatureId,Surface.ModelHash);
		Cell.OwnedFeatureIds = {Road.FeatureId}; B.Features.Add(Road.FeatureId,Road);
		return B;
	}
	FProjectWorldRealizationLayer Layer()
	{
		FProjectWorldRealizationLayer L; L.LayerId = TEXT("roads"); L.GeneratorId = TEXT("project_road_mesh");
		L.GeneratorVersion = 1; L.DependencyHaloCells = 1;
		L.NormalizedSettings = TEXT("{\"selected_classes\":[\"footway\"],\"surface_offset_m\":0.15,\"maximum_segment_length_m\":7.5,")
			TEXT("\"nanite\":true,\"collision\":\"complex_as_simple\",\"structure_fallback\":\"terrain_drape\",\"intersection_policy\":\"overlap_same_owner\"}");
		return ProjectWorldVerifyTestFlow::Finalize(MoveTemp(L));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldReferencedSurfaceTest,
	"Project.World.Realization.Road.ReferencedConstructedSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldReferencedSurfaceTest::RunTest(const FString&)
{
	using namespace ProjectWorldReferencedSurfaceTest;
	auto B = Bundle(); const auto L = Layer(); FMeshDescription Mesh; FVector Origin; FString Error, Before, After;
	if (!TestTrue(TEXT("A curved Road consumes the single referenced world-space deck field."),
		ProjectWorldRoadRealization::BuildDescription(B,B.Cells[0],L,Mesh,Origin,Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("The referenced native output is nonempty."), Mesh.Triangles().Num() > 0);
	const auto Positions = FStaticMeshConstAttributes(Mesh).GetVertexPositions();
	for (const auto V : Mesh.Vertices().GetElementIDs())
	{
		const auto P = FProjectWorldCanonicalLoader::UnrealToCanonical(B,FVector(Positions[V])+Origin);
		TestTrue(TEXT("Native corner/resampling geometry samples the same XYZ field, with one declared lift."),
			FMath::Abs(P.Z-(3.5+.01*P.X+.02*P.Y+.15)) < 1e-5);
	}
	FProjectWorldRoadEmission Read;
	TestTrue(TEXT("The public read uses the same native stored geometry without asset writes."),
		ProjectWorldRoadEmission::Read(B,B.Cells[0],L,{TEXT("fixture:platform-path")},Read,Error));
	TestEqual(TEXT("The filtered native read retains the actual face census."),Read.Surface.Triangles.Num(),Mesh.Triangles().Num());
	TestEqual(TEXT("The emission exposes its actual native frame."),Read.Surface.Origin,Origin);
	TestTrue(TEXT("The read declares its actual local eligibility closure."),Read.LocallyEligibleFeatureIds.Contains(TEXT("fixture:platform-path")));
	TestEqual(TEXT("The current Road presentation policy is returned explicitly."),Read.PresentationLiftMeters,.15);
	for (int32 I=0; I<Read.Surface.Triangles.Num(); ++I) for (int32 J=0; J<3; ++J)
	{
		const auto Vertex = Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(FTriangleID(I),J));
		TestEqual(TEXT("The read does not reconstruct or approximate the native positions."),Read.Surface.Triangles[I].Points[J],FVector(Positions[Vertex]));
	}
	for (const FString Omission : {TEXT("remote"),TEXT("class"),TEXT("width")})
	{
		auto Other=Bundle(); auto& Road=Other.Features[TEXT("fixture:platform-path")];
		if (Omission==TEXT("remote")) Other.Cells[0].OwnedFeatureIds.Reset();
		if (Omission==TEXT("class")) Road.RoadClass=TEXT("motorway");
		if (Omission==TEXT("width")) Road.WidthMeters=0;
		FProjectWorldRoadEmission Empty;
		TestTrue(TEXT("A global selection can have no eligible local Road."),ProjectWorldRoadEmission::Read(Other,Other.Cells[0],L,{Road.FeatureId},Empty,Error));
		TestEqual(TEXT("An omitted Road cannot be mistaken for local eligibility."),Empty.LocallyEligibleFeatureIds.Num(),0);
		TestEqual(TEXT("An omitted Road contributes no native triangles."),Empty.Surface.Triangles.Num(),0);
	}
	TestFalse(TEXT("Unknown selected features cannot yield a valid emission."),
		ProjectWorldRoadEmission::Read(B,B.Cells[0],L,{TEXT("fixture:missing")},Read,Error));
	TestEqual(TEXT("Failed read does not retain stale source geometry."),Read.Surface.Triangles.Num(),0);
	{
		auto Mixed=Bundle(); auto Missing=Mixed.Features[TEXT("fixture:platform-path")];
		Missing.FeatureId=TEXT("fixture:no-fragment"); Missing.Representations.Reset(); Missing.RoadElevation.Reset();
		Mixed.Features.Add(Missing.FeatureId,Missing); Mixed.Cells[0].OwnedFeatureIds.Add(Missing.FeatureId);
		TestFalse(TEXT("One valid Road cannot hide another selected local Road's missing fragment."),
			ProjectWorldRoadEmission::Read(Mixed,Mixed.Cells[0],L,{TEXT("fixture:platform-path"),Missing.FeatureId},Read,Error));
		TestTrue(TEXT("Mixed-closure refusal clears both census and geometry."),Read.Surface.Triangles.IsEmpty() && Read.LocallyEligibleFeatureIds.IsEmpty());
	}
	TestTrue(TEXT("The actual referenced inputs hash."), ProjectWorldRoadRealization::HashCellInput(B,B.Cells[0],L,Before,Error));
	B.Features[TEXT("fixture:fixed-deck")].ConstructedSurface->ModelHash = FString::ChrN(64,'d');
	B.Features[TEXT("fixture:platform-path")].RoadElevation->ReferencedSurfaceModels[TEXT("fixture:fixed-deck")] = FString::ChrN(64,'d');
	TestTrue(TEXT("A changed valid model pin hashes."), ProjectWorldRoadRealization::HashCellInput(B,B.Cells[0],L,After,Error));
	TestNotEqual(TEXT("The supporting source model identity is consumed."), Before,After);
	for (const FString Kind : {TEXT("missing"),TEXT("pin"),TEXT("frame"),TEXT("role"),TEXT("source"),
		TEXT("mixed"),TEXT("gap"),TEXT("overlap"),TEXT("crack"),TEXT("buried"),TEXT("nan")})
	{
		auto Broken = Bundle(); auto& Path = Broken.Features[TEXT("fixture:platform-path")];
		auto& S = Broken.Features[TEXT("fixture:fixed-deck")].ConstructedSurface.GetValue();
		if (Kind == TEXT("pin")) S.ModelHash = FString::ChrN(64,'e');
		if (Kind == TEXT("frame")) S.VerticalDatum = TEXT("EPSG:5703");
		if (Kind == TEXT("role")) S.Role = TEXT("unknown");
		if (Kind == TEXT("source")) Path.RoadElevation->SourcePoints[0].X += .1;
		if (Kind == TEXT("mixed")) Path.RoadElevation->Triangles.Add({{FVector(0,-1,3),FVector(17,-1,3),FVector(0,1,3)}});
		if (Kind == TEXT("gap")) S.Triangles.Pop();
		if (Kind == TEXT("overlap")) { const auto Duplicate = S.Triangles[0]; S.Triangles.Add(Duplicate); }
		if (Kind == TEXT("crack")) S.Triangles[0].XYZ[1].Z += .1;
		if (Kind == TEXT("buried")) for (auto& Z : Broken.Cells[0].Terrain.HeightsMeters) Z = 6;
		if (Kind == TEXT("nan")) S.Triangles[0].XYZ[0].Z = std::numeric_limits<double>::quiet_NaN();
		if (Kind == TEXT("missing")) Broken.Features.Remove(TEXT("fixture:fixed-deck"));
		FMeshDescription Rejected; FVector RejectedOrigin;
		TestFalse(*FString::Printf(TEXT("%s referenced evidence refuses."),*Kind),
			ProjectWorldRoadRealization::BuildDescription(Broken,Broken.Cells[0],L,Rejected,RejectedOrigin,Error));
		TestEqual(TEXT("Refusal exposes no partial output mesh."), Rejected.Triangles().Num(),0);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedBodyMeshTest,
    "Project.World.Realization.Road.ConstructedBodyMesh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedBodyMeshTest::RunTest(const FString&)
{
    using namespace ProjectWorldReferencedSurfaceTest;
    auto B=Bundle(); FString Error;
	TArray<FProjectWorldSurfaceTriangle> Field;
	for (const auto& Face : B.Features[TEXT("fixture:fixed-deck")].ConstructedSurface->Triangles)
		Field.Add({{Face.XYZ[0],Face.XYZ[1],Face.XYZ[2]}});
	FMeshDescription Body; FVector Origin;
	if (!TestTrue(TEXT("The complete physical structure builds without a second Road surface or lift."),
		ProjectWorldConstructedMesh::Build(B,B.Cells[0],Field,.001,Body,Origin,Error))) { AddError(Error); return false; }
	double Area=0; const auto Positions=FStaticMeshConstAttributes(Body).GetVertexPositions();
	for (const auto Id : Body.Triangles().GetElementIDs())
	{
		FProjectWorldSurfaceTriangle Face;
		for (int32 I=0; I<3; ++I)
		{
			const FVector Stored(Positions[Body.GetVertexInstanceVertex(Body.GetTriangleVertexInstance(Id,I))]);
			Face.Points[I]=FProjectWorldCanonicalLoader::UnrealToCanonical(B,Stored+Origin); const auto& P=Face.Points[I];
			TestTrue(TEXT("Physical vertices retain canonical XYZ with zero presentation lift."),FMath::Abs(P.Z-(3.5+.01*P.X+.02*P.Y))<1e-5);
		}
		Area+=ProjectWorldTerrainSurface::Area({FVector2D(Face.Points[0]),FVector2D(Face.Points[1]),FVector2D(Face.Points[2])});
	}
	TestTrue(TEXT("The single physical mesh covers the complete 400 m2 structure."),FMath::Abs(Area-400)<1e-6);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedStoredPrecisionTest,
	"Project.World.Realization.Road.ConstructedStoredPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedStoredPrecisionTest::RunTest(const FString&)
{
	auto B=ProjectWorldReferencedSurfaceTest::Bundle(); auto& C=B.Cells[0]; C.Bounds=FVector4d(0,0,480,480);
	const FVector Corner(100.+1./3.,329.5,3.5);
	auto P = [](double X,double Y) { return FVector(X,Y,3.5+.123*(3*X+2*Y-960)); };
	TArray<FProjectWorldSurfaceTriangle> Field={
		{{FVector(0,0,3.5),FVector(320,0,3.5),Corner}},{{FVector(0,0,3.5),Corner,FVector(0,480,3.5)}},
		{{P(320,0),P(480,0),Corner}},{{P(480,0),P(480,480),Corner}},{{P(480,480),P(0,480),Corner}}};
	const FVector2D Rounded(static_cast<float>(Corner.X*100)*.01,480-static_cast<float>((480-Corner.Y)*100)*.01);
	double OldHeight; FString Error;
	TestTrue(TEXT("The asymmetric known-bad old plane evaluation is measurable."),ProjectWorldTerrainSurface::Height(Field[2],Rounded,OldHeight,Error));
	TestNotEqual(TEXT("Independent post-XY-rounding plane evaluation creates different stored seam heights."),static_cast<float>(OldHeight*100),350.f);
	FMeshDescription Mesh; FVector Origin;
	if (!TestTrue(TEXT("An asymmetric C0 crease retains a single stored height."),
		ProjectWorldConstructedMesh::Build(B,C,Field,.001,Mesh,Origin,Error))) { AddError(Error); return false; }
	const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions(); TMap<FVector2D,double> Heights;
	int32 Repeated=0;
	for (const auto V : Mesh.Vertices().GetElementIDs())
	{
		const FVector Point(Positions[V]); const FVector2D XY(Point);
		if (const auto* Before=Heights.Find(XY)) { ++Repeated; TestEqual(TEXT("Every aliased stored XY retains exact C0 height."),Point.Z,*Before); }
		else Heights.Add(XY,Point.Z);
	}
	TestTrue(TEXT("The crease control actually exercises shared vertices."),Repeated>0);
	C.Bounds=FVector4d(0,0,.01,.01);
	Field={{{FVector(0,0,0),FVector(.00001,0,0),FVector(0,.00001,0)}}};
	if (!TestTrue(TEXT("A tiny positive-area face retains a valid native shading frame."),
		ProjectWorldConstructedMesh::Build(B,C,Field,.001,Mesh,Origin,Error))) { AddError(Error); return false; }
	const auto Attributes=FStaticMeshConstAttributes(Mesh);
	TestTrue(TEXT("The small valid-face control emits geometry."),Mesh.Triangles().Num()>0);
	for (const auto VI : Mesh.VertexInstances().GetElementIDs())
	{
		TestTrue(TEXT("Tiny face normal remains normalized."),Attributes.GetVertexInstanceNormals()[VI].SizeSquared()>.99f);
		TestTrue(TEXT("Tiny face tangent remains normalized."),Attributes.GetVertexInstanceTangents()[VI].SizeSquared()>.99f);
	}
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedStoredRelationsTest,
	"Project.World.Realization.Road.ConstructedStoredRelations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedStoredRelationsTest::RunTest(const FString&)
{
	const FProjectWorldSurfaceTriangle A{{FVector(0,0,0),FVector(10,0,0),FVector(0,10,0)}};
	const FProjectWorldSurfaceTriangle B{{FVector(10,0,0),FVector(10,10,0),FVector(0,10,0)}};
	FString Error;
	TestTrue(TEXT("Exactly adjacent stored faces have no positive overlap or join step."),ProjectWorldConstructedMesh::ValidateStored({A,B},.1,Error));
	TestFalse(TEXT("Exact duplicate stored body faces refuse."),ProjectWorldConstructedMesh::ValidateStored({A,A},.1,Error));
	auto Raised=B; for (auto& P : Raised.Points) P.Z=.2;
	TestFalse(TEXT("A stored body/body seam above the explicit limit refuses."),ProjectWorldConstructedMesh::ValidateStored({A,Raised},.1,Error));
	const FProjectWorldSurfaceTriangle T1{{FVector(10,0,0),FVector(10,10,0),FVector(5,5,.2)}};
	const FProjectWorldSurfaceTriangle T2{{FVector(10,10,0),FVector(0,10,0),FVector(5,5,.2)}};
	TestFalse(TEXT("An interior T-junction step cannot hide behind equal edge endpoints."),ProjectWorldConstructedMesh::ValidateStored({A,T1,T2},.1,Error));
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldReferencedProjectedFanTest,
	"Project.World.Realization.Road.ReferencedProjectedFan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldReferencedProjectedFanTest::RunTest(const FString&)
{
	auto B=ProjectWorldReferencedSurfaceTest::Bundle();
	auto& Fixed=B.Features[TEXT("fixture:fixed-deck")].ConstructedSurface.GetValue();
	Fixed.Triangles={{{FVector(583322.21,4508127.52,3.55),FVector(583366.48,4508116.95,3.55),FVector(583322,4508126.2,3.55)}},
		{{FVector(583322.39,4508128.86,3.55),FVector(583366.48,4508116.95,3.55),FVector(583322.21,4508127.52,3.55)}}};
	auto Floating=B.Features[TEXT("fixture:fixed-deck")]; Floating.FeatureId=TEXT("fixture:captured-floating");
	auto& F=Floating.ConstructedSurface.GetValue(); F.SourceFeatureId=Floating.FeatureId; F.Role=TEXT("floating_assembly");
	F.Triangles=ProjectWorldReferencedSurfaceTest::CapturedRamp();
	const auto FloatingId=Floating.FeatureId; const auto FloatingHash=F.ModelHash;
	B.Features.Add(FloatingId,MoveTemp(Floating));
	auto& Path=B.Features[TEXT("fixture:platform-path")]; Path.RoadElevation->ReferencedSurfaceModels.Add(FloatingId,FloatingHash);
	const FVector2D End(583322.21,4508127.52),Tangent=(End-FVector2D(583300.93,4508130.52)).GetSafeNormal();
	const FVector2D Start=FMath::Lerp(FVector2D(583303.41,4508130.17),End,2./3.),Side(-Tangent.Y,Tangent.X);
	const FProjectWorldSurfaceTriangle Domain[2]={
		{{FVector(Start+Side,0),FVector(Start-Side,0),FVector(End+Side,0)}},
		{{FVector(Start-Side,0),FVector(End-Side,0),FVector(End+Side,0)}}};
	FString Error; TArray<ProjectWorldRoadSurface::FRibbonTriangle> Out;
	if (!TestTrue(TEXT("The captured projected end domain references its fixed/floating fields."),
		ProjectWorldRoadReferencedSurface::Append(B,Path,Domain,Out,Error))) { AddError(Error); return false; }
	TestTrue(TEXT("The direct field intersection emits a nonempty domain."),Out.Num()>0);
	for (const auto& T : Out)
		TestTrue(TEXT("Canonical restoration must not introduce zero-area fan faces."),
			ProjectWorldTerrainSurface::Area({FVector2D(T.Surface.Points[0]),FVector2D(T.Surface.Points[1]),FVector2D(T.Surface.Points[2])})>0);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldReferencedProjectedStorageTest,
	"Project.World.Realization.Road.ReferencedProjectedStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldReferencedProjectedStorageTest::RunTest(const FString&)
{
	auto B=ProjectWorldReferencedSurfaceTest::Bundle();
	B.EngineGeoreferenceOriginMeters=FVector2D(585628,4511322);
	auto& C=B.Cells[0]; C.Bounds=C.Terrain.Bounds=FVector4d(583303.41,4507602.45,584233.41,4508532.45);
	C.Terrain.SampleSpacing=FVector2D(465,465);
	auto& Model=B.Features[TEXT("fixture:fixed-deck")].ConstructedSurface.GetValue();
	Model.Role=TEXT("floating_assembly"); Model.Triangles=ProjectWorldReferencedSurfaceTest::CapturedRamp();
	const FVector2D SourceEnd(583322.21,4508127.52),SourceStart(583300.93,4508130.52),A(583303.41,4508130.17);
	const FVector2D End=FMath::Lerp(A,SourceEnd,1./3.),Tangent=(SourceEnd-SourceStart).GetSafeNormal(),Side(-Tangent.Y,Tangent.X);
	auto& Road=B.Features[TEXT("fixture:platform-path")]; Road.GeometryPoints={SourceStart,SourceEnd};
	Road.RoadElevation->SourcePoints=Road.GeometryPoints; Road.RoadElevation->LengthMeters=(SourceEnd-SourceStart).Length();
	const FProjectWorldSurfaceTriangle Domain[2]={
		{{FVector(A+Side,0),FVector(A-Side,0),FVector(End+Side,0)}},{{FVector(A-Side,0),FVector(End-Side,0),FVector(End+Side,0)}}};
	FString Error; TArray<ProjectWorldRoadSurface::FRibbonTriangle> Ribbon;
	if (!TestTrue(TEXT("The source-faithful projected first fragment reads the three shared ramp planes."),
		ProjectWorldRoadReferencedSurface::Append(B,Road,Domain,Ribbon,Error))) { AddError(Error); return false; }
	ProjectWorldRoadSurface::FContext Context;
	if (!TestTrue(TEXT("Only a constant zero Ground control is needed for the storage regression."),ProjectWorldRoadSurface::Index(B,Context,Error))) return false;
	const FVector Origin=FProjectWorldCanonicalLoader::CanonicalToUnreal(B,FVector(C.Bounds.X,C.Bounds.W,B.HeightOriginMeters));
	FMeshDescription Mesh; TSet<FString> Consumed;
	if (!TestTrue(TEXT("Projected direct references retain source C0 through native group storage."),
		ProjectWorldRoadSurface::Conform(B,C,Context,Ribbon,Origin,.15,1,&Mesh,Consumed,Error))) { AddError(Error); return false; }
	TestTrue(TEXT("The qualified fragment produces native faces."),Mesh.Triangles().Num()>0);
	TArray<FProjectWorldSurfaceTriangle> Field;
	for (const auto& T : Model.Triangles) Field.Add({{T.XYZ[0],T.XYZ[1],T.XYZ[2]}});
	int32 HeightMismatches=0; TOptional<double> CapturedHeight;
	const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
	for (const auto V : Mesh.Vertices().GetElementIDs())
	{
		const FVector Stored(Positions[V]);
		const auto Actual=FProjectWorldCanonicalLoader::UnrealToCanonical(B,Stored+Origin); double Expected;
		if (!TestTrue(TEXT("Every actual stored XY remains supported by the complete field."),
			ProjectWorldConstructedSurface::Sample(Field,FVector2D(Actual),Expected,Error))) return false;
		const float ExpectedStored=static_cast<float>((Expected+.15-B.HeightOriginMeters)*100-Origin.Z);
		if (Stored.Z!=ExpectedStored) ++HeightMismatches;
		if (Stored.X==590.98455810546875 && Stored.Y==40210.31640625) CapturedHeight=Stored.Z;
	}
	TestEqual(TEXT("Referenced heights are sampled at actual stored XY, after horizontal conversion."),HeightMismatches,0);
	FProjectWorldSurfaceTriangle Captured{{FVector(591.0010986328125,-150.6812744140625,207.5792236328125),
		FVector(15,-277,135),FVector(590.98455810546875,-150.68359375,207.57713317871094)}};
	ProjectWorldConstructedMesh::FSlopeMetrics Metrics; const FVector2D Axis(SourceEnd.X-SourceStart.X,SourceStart.Y-SourceEnd.Y);
	TestFalse(TEXT("The captured pre-conversion Road height makes the adjoining body edge unwalkable."),
		ProjectWorldConstructedMesh::ValidateSlopes({Captured},Axis,.125,.02,Metrics,Error));
	if (TestTrue(TEXT("The reduced fragment includes the actual failing Road vertex."),CapturedHeight.IsSet()))
	{
		Captured.Points[2].Z=CapturedHeight.GetValue();
		TestTrue(TEXT("Actual stored-XY field sampling fixes the captured final body slope without moving XY."),
			ProjectWorldConstructedMesh::ValidateSlopes({Captured},Axis,.125,.02,Metrics,Error));
	}
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedStoredHeightLimitTest,
	"Project.World.Realization.Road.ConstructedStoredHeightLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedStoredHeightLimitTest::RunTest(const FString&)
{
	auto B=ProjectWorldReferencedSurfaceTest::Bundle(); auto& C=B.Cells[0]; C.Bounds=FVector4d(0,0,6,6);
	constexpr double Height=3.7000002;
	const TArray<FProjectWorldSurfaceTriangle> Field={{{FVector(0,0,Height),FVector(6,0,Height),FVector(0,6,Height)}}};
	FMeshDescription Mesh; FVector Origin; FString Error;
	if (!TestTrue(TEXT("The complete body remains qualified under the loose explicit field limit."),
		ProjectWorldConstructedMesh::Build(B,C,Field,3e-7,Mesh,Origin,Error))) { AddError(Error); return false; }
	TestFalse(TEXT("Final float Z must refuse a source limit tighter than native representation can preserve."),
		ProjectWorldConstructedMesh::Build(B,C,Field,5e-8,Mesh,Origin,Error));
	TestTrue(TEXT("The refusal measures actual stored source-field agreement."),Error.Contains(TEXT("stored height exceeds")));
	TestEqual(TEXT("A failed final storage limit leaves no partial body."),Mesh.Triangles().Num(),0);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedEndpointIdentityTest,
	"Project.World.Realization.Verify.ConstructedEndpointIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedEndpointIdentityTest::RunTest(const FString&)
{
	using namespace ProjectWorldReferencedSurfaceTest;
	const FString RoadId=TEXT("fixture:platform-path"),DeckId=TEXT("fixture:fixed-deck");
	auto Digest = [](const FProjectWorldCanonicalBundle& B)
	{
		FProjectWorldVerifyIdentity Identity; Identity.AddFixtureBundle(TEXT("memory:constructed-endpoints"),B);
		return Identity.FixtureInputs[0].Value;
	};
	auto Valid = [&](const FProjectWorldCanonicalBundle& B,FString& Identity)
	{
		FString Error; const bool Accepted=ProjectWorldRoadElevation::Validate(B,B.Features.FindChecked(RoadId),Identity,Error);
		if (!Accepted) AddError(Error); return TestTrue(TEXT("Identity counterexamples remain valid typed Road inputs."),Accepted);
	};
	auto Ordinary=Bundle(); Ordinary.Features.Remove(DeckId); auto& Profile=Ordinary.Features[RoadId].RoadElevation.GetValue();
	Profile.Domain=EProjectWorldRoadElevationDomain::SourceParameter; Profile.ReferencedSurfaceModels.Reset();
	Profile.EndpointRelation=EProjectWorldRoadEndpointRelation::TerrainAbutment; Profile.ConstructedSurfaceHash.Reset();
	Profile.Triangles={{{FVector(0,-1,0),FVector(17,-1,0),FVector(0,1,0)}},{{FVector(17,-1,0),FVector(17,1,0),FVector(0,1,0)}}};
	FString Consumed,ChangedConsumed;
	if (!Valid(Ordinary,Consumed)) return false;
	const FString OrdinaryHash=Digest(Ordinary); AddInfo(TEXT("UnchangedOrdinaryEndpointFixture=")+OrdinaryHash);
	TestEqual(TEXT("Default ordinary endpoint fixture identity preserves the recorded pre-fix value."),OrdinaryHash,
		FString(TEXT("8f9add8b9e02de948e177bcf1e74a57c5a52a7c3a0c043bf86f07bd8bf62fdda")));
	auto Constructed=Ordinary; auto& ChangedProfile=Constructed.Features[RoadId].RoadElevation.GetValue();
	ChangedProfile.EndpointRelation=EProjectWorldRoadEndpointRelation::ConstructedSurface; ChangedProfile.ConstructedSurfaceHash=FString::ChrN(64,'b');
	if (!Valid(Constructed,ChangedConsumed)) return false;
	TestNotEqual(TEXT("The existing consumer already distinguishes supported endpoint meaning at identical geometry."),Consumed,ChangedConsumed);
	const FString ConstructedHash=Digest(Constructed);
	TestNotEqual(TEXT("Verification must bind a valid changed endpoint relation with identical source/UVZ geometry."),OrdinaryHash,ConstructedHash);
	ChangedProfile.ConstructedSurfaceHash=FString::ChrN(64,'e');
	if (!Valid(Constructed,ChangedConsumed)) return false;
	TestNotEqual(TEXT("Verification must bind the supporting endpoint model even when geometry is identical."),ConstructedHash,Digest(Constructed));
	auto Referenced=Bundle(); if (!Valid(Referenced,Consumed)) return false;
	const FString ReferenceHash=Digest(Referenced);
	Referenced.Features[DeckId].ConstructedSurface->ModelHash=FString::ChrN(64,'d');
	Referenced.Features[RoadId].RoadElevation->ReferencedSurfaceModels[DeckId]=FString::ChrN(64,'d');
	if (!Valid(Referenced,ChangedConsumed)) return false;
	TestNotEqual(TEXT("The consumer binds a changed valid referenced model with unchanged XYZ."),Consumed,ChangedConsumed);
	TestNotEqual(TEXT("Verification must also bind the changed model and its Road reference."),ReferenceHash,Digest(Referenced));
	const FString ModelHash=Digest(Referenced);
	Referenced.Features[DeckId].ConstructedSurface->Basis=TEXT("observed");
	if (!Valid(Referenced,ChangedConsumed)) return false;
	TestNotEqual(TEXT("Verification must bind observed versus identified-model basis at identical XYZ and model pin."),ModelHash,Digest(Referenced));
	return !HasAnyErrors();
}
#endif
