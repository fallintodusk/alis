// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadStoredPolygon.h"
#include "ProjectWorldTerrainSurface.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadStoredPolygonTest,
	"Project.World.Realization.Road.StoredPolygon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadStoredPolygonTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Output; TArray<FIntVector> Faces; FString Error;
	ProjectWorldRoadStoredPolygon::FVertexMap Vertices;
	const TArray<FVector> Redundant = {FVector(0, 0, 1), FVector(1, 0, 1), FVector(1, 1, 1), FVector(1 - 1e-15, 1, 1), FVector(0, 1, 1)};
	TestTrue(TEXT("Sub-ULP redundant edge preserves the complete stored square."),
		ProjectWorldRoadStoredPolygon::Store(Redundant, {{Redundant[0], Redundant[1], Redundant[2]}}, Vertices, Output, Faces, Error));
	TestEqual(TEXT("Only exactly equal consecutive stored positions merge."), Output.Num(), 4);
	TestEqual(TEXT("Square has positive nondegenerate coverage."), Faces.Num(), 2);
	TArray<FVector> Affine;
	for (const auto& XY : {FVector2D(49999, 49999), FVector2D(50001, 49999), FVector2D(50000.0001, 50000),
		FVector2D(49999.9999, 50000), FVector2D(49999, 50000)})
		Affine.Add(FVector(XY, 6000.0009765625 + 0.1 * XY.X + 0.2 * XY.Y));
	TestTrue(TEXT("An affine surface shares source height after XY storage without creating a false vertical face."),
		ProjectWorldRoadStoredPolygon::Store(Affine, {{Affine[0], Affine[1], Affine[2]}}, Vertices, Output, Faces, Error));
	TestEqual(TEXT("Stored affine duplicate vertices merge exactly."), Output.Num(), 4);
	const double Edge = 40000.0015625, Z = 6000.00026;
	const TArray<FVector> Left{FVector(Edge - 1, 0, Z - 0.1), FVector(Edge, 0, Z), FVector(Edge, 1, Z)};
	const TArray<FVector> Right{FVector(Edge + 0.0002, 0, Z - 0.00002), FVector(Edge + 1, 0, Z - 0.1), FVector(Edge + 0.0002, 1, Z - 0.00002)};
	Vertices.Reset();
	TestTrue(TEXT("First sloping side stores."), ProjectWorldRoadStoredPolygon::Store(Left, {{Left[0], Left[1], Left[2]}}, Vertices, Output, Faces, Error));
	const double SharedZ = Vertices[FVector2f(40000, 0)].Stored.Z;
	TestTrue(TEXT("Incident plane reuses the original shared height."), ProjectWorldRoadStoredPolygon::Store(Right, {{Right[0], Right[1], Right[2]}}, Vertices, Output, Faces, Error));
	TestNotEqual(TEXT("Control heights independently round to different bins."), double(float(Right[0].Z)), SharedZ);
	TestEqual(TEXT("Emitted near-ULP shared-edge height uses the shared binding."), Output[0].Z, SharedZ);
	const auto SavedOutput = Output;
	const auto SavedFaces = Faces;
	const int32 SavedBindings = Vertices.Num();
	const TArray<FVector> Conflict{FVector(Edge, 0, Z + 1), FVector(Edge + 1, 0, Z + 1), FVector(Edge, 1, Z + 1)};
	TestFalse(TEXT("A genuinely different source surface cannot alias an existing binding."),
		ProjectWorldRoadStoredPolygon::Store(Conflict, {{Conflict[0], Conflict[1], Conflict[2]}}, Vertices, Output, Faces, Error));
	TestTrue(TEXT("Conflict preserves populated output and map."), Output == SavedOutput && Faces == SavedFaces && Vertices.Num() == SavedBindings && Vertices[FVector2f(40000, 0)].Stored.Z == SharedZ);
	for (const auto& Invalid : {TArray<FVector>{FVector(0, 0, 0), FVector(1, 0, 0), FVector(1, 0, 1), FVector(0, 1, 0)},
		TArray<FVector>{FVector(1e10, 1e10, 0), FVector(1e10 + 1, 1e10, 0), FVector(1e10, 1e10 + 1, 0)},
		TArray<FVector>{FVector(0, 0, 0), FVector(1, 0, 1e300), FVector(0, 1, 0)}})
	{
		Output.Reset(); Faces.Reset(); Error.Reset(); Vertices.Reset();
		TestFalse(TEXT("Unequal collapsed heights, whole-polygon collapse or overflow refuses."),
			ProjectWorldRoadStoredPolygon::Store(Invalid, {{FVector(0, 0, 0), FVector(1, 0, 0), FVector(0, 1, 0)}}, Vertices, Output, Faces, Error));
		TestTrue(TEXT("Refusal keeps output empty and explains cause."), Output.IsEmpty() && Faces.IsEmpty() && !Error.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadStoredGroupTest,
	"Project.World.Realization.Road.StoredGroup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadStoredGroupTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRoadStoredPolygon;
	const FVector A(40000, 0, 100), B(40001, 0, 100.2), C(40000, 1, 100);
	const FVector Kink(40000.0001, 0, 100.00001);
	const TArray<FVector2D> Domain{FVector2D(A), FVector2D(B), FVector2D(C)};
	const FProjectWorldSurfaceTriangle Thin{{A, Kink, C}}, Remainder{{Kink, B, C}};
	FVertexMap Vertices;
	TArray<FProjectWorldSurfaceTriangle> Faces;
	FString Error;
	if (!TestTrue(TEXT("A true source-height kink may collapse internally while its original domain stays complete."),
		StoreGroup({Thin, Remainder}, Domain, Vertices, Faces, Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Only the exactly collapsed split is omitted."), Faces.Num(), 1);
	TestEqual(TEXT("Collapsed-piece binding participates in the surviving shared edge."), Faces[0].Points[0].Z, double(float(A.Z)));
	const auto Saved = Faces;
	const int32 Bindings = Vertices.Num();
	for (const auto& Invalid : {TArray<FProjectWorldSurfaceTriangle>{Thin},
		TArray<FProjectWorldSurfaceTriangle>{Thin, Remainder, Remainder}})
	{
		TestFalse(TEXT("Missing positive coverage and duplicate positive faces refuse independently."),
			StoreGroup(Invalid, Domain, Vertices, Faces, Error));
		TestTrue(TEXT("Group refusal preserves populated output and bindings."), Faces.Num() == Saved.Num() &&
			Faces[0].Points[0] == Saved[0].Points[0] && Faces[0].Points[1] == Saved[0].Points[1] && Faces[0].Points[2] == Saved[0].Points[2] && Vertices.Num() == Bindings);
	}
	const FVector N0(40000.25, 0, 100.05), N1(40000.25390625, 0, 100 + 0.2 * 0.25390625), N2(40000.25, 0.04, 100.05);
	const TArray<FProjectWorldSurfaceTriangle> Notch{Thin, {{C, A, N0}}, {{C, N0, N2}}, {{N2, N1, B}}, {{N2, B, C}}};
	TestTrue(TEXT("Sabotage notch area is below the measured whole-domain area allowance."),
		0.5 * (N1.X - N0.X) * N2.Y < 2 * (2 + FMath::Sqrt(2.0)) * (Kink.X - A.X));
	TestFalse(TEXT("Small-area boundary notch still cannot remove an original-domain interior."),
		StoreGroup(Notch, Domain, Vertices, Faces, Error));
	TestTrue(TEXT("The inward-core guard rejects the notch beyond its conversion radius."), Error.Contains(TEXT("interior")));
	const FVector S0(N0.X, 0, 100.05), S1(N1.X, 0, 100 + 0.2 * (N1.X - A.X)), S2(N0.X, -0.04, 100.05);
	TestFalse(TEXT("Small-area exterior spike still cannot leave the conversion envelope."),
		StoreGroup({Thin, {{A, B, C}}, {{S0, S2, S1}}}, Domain, Vertices, Faces, Error));
	TestTrue(TEXT("The exterior vertex guard rejects the spike beyond its conversion radius."), Error.Contains(TEXT("vertex leaves")));
	TestTrue(TEXT("Geometric guard refusals preserve populated state."), Faces.Num() == Saved.Num() &&
		Faces[0].Points[0] == Saved[0].Points[0] && Vertices.Num() == Bindings);
	Vertices.Reset(); Faces.Reset();
	const FVector D(1e10, 1e10, 0), E(1e10 + 1, 1e10, 0), F(1e10, 1e10 + 1, 0);
	TestFalse(TEXT("A collapsed original domain still refuses."),
		StoreGroup({{{D, E, F}}}, {FVector2D(D), FVector2D(E), FVector2D(F)}, Vertices, Faces, Error));
	TestTrue(TEXT("Collapsed-domain refusal emits no bindings or geometry."), Vertices.IsEmpty() && Faces.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadStoredCollinearTest,
    "Project.World.Realization.Road.StoredCollinear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadStoredCollinearTest::RunTest(const FString& Parameters)
{
    // Captured shared-edge geometry; constant height isolates the native XY union normalization.
    const TArray<FProjectWorldSurfaceTriangle> Pieces{
		{{FVector(35470.565015519969, 54075.487023591995, 100), FVector(35470.56192109012, 54075.522972084582, 100), FVector(35489.738576475065, 54077.173689939082, 100)}},
		{{FVector(35470.565015519969, 54075.487023591995, 100), FVector(35489.738576475065, 54077.173689939082, 100), FVector(35489.741997310193, 54077.137769479305, 100)}},
		{{FVector(35489.741997310193, 54077.137769479305, 100), FVector(35489.738576475065, 54077.173689939082, 100), FVector(35489.739042776637, 54077.173730079085, 100)}},
		{{FVector(35490.217658592155, 54071.34835831821, 100), FVector(35472.228251304477, 54056.164944451302, 100), FVector(35470.565015519969, 54075.487023591995, 100)}},
		{{FVector(35490.217658592155, 54071.34835831821, 100), FVector(35470.565015519969, 54075.487023591995, 100), FVector(35489.741997310193, 54077.137769479305, 100)}},
		{{FVector(35497.910028585466, 54077.840869966894, 100), FVector(35489.741997310193, 54077.137769479305, 100), FVector(35489.739042776637, 54077.173730079085, 100)}},
		{{FVector(35497.910028585466, 54077.840869966894, 100), FVector(35489.739042776637, 54077.173730079085, 100), FVector(35497.957809211221, 54077.881197724491, 100)}},
		{{FVector(35490.217658592155, 54071.34835831821, 100), FVector(35489.741997310193, 54077.137769479305, 100), FVector(35490.247840189841, 54071.373832132667, 100)}},
		{{FVector(35490.247840189841, 54071.373832132667, 100), FVector(35489.741997310193, 54077.137769479305, 100), FVector(35497.910028585466, 54077.840869966894, 100)}},
		{{FVector(34501.645285222912, 53992.082853708416, 100), FVector(34501.642190793063, 53992.118802201003, 100), FVector(35179.869510233402, 54050.500301551074, 100)}},
		{{FVector(34501.645285222912, 53992.082853708416, 100), FVector(35179.869510233402, 54050.500301551074, 100), FVector(35179.884148814017, 54050.465346779674, 100)}},
		{{FVector(35179.884148814017, 54050.465346779674, 100), FVector(35179.869510233402, 54050.500301551074, 100), FVector(35179.886001994601, 54050.501721166074, 100)}},
		{{FVector(35167.051723611075, 53798.589949682355, 100), FVector(34562.24219079013, 53288.118802197278, 100), FVector(34501.645285222912, 53992.082853708416, 100)}},
		{{FVector(35167.051723611075, 53798.589949682355, 100), FVector(34501.645285222912, 53992.082853708416, 100), FVector(35179.884148814017, 54050.465346779674, 100)}},
		{{FVector(35470.565015519969, 54075.487023591995, 100), FVector(35179.884148814017, 54050.465346779674, 100), FVector(35179.886001994601, 54050.501721166074, 100)}},
		{{FVector(35470.565015519969, 54075.487023591995, 100), FVector(35179.886001994601, 54050.501721166074, 100), FVector(35470.56192109012, 54075.522972084582, 100)}},
		{{FVector(35472.228251304477, 54056.164944451302, 100), FVector(35211.178665101761, 53835.833956766874, 100), FVector(35179.884148814017, 54050.465346779674, 100)}},
		{{FVector(35472.228251304477, 54056.164944451302, 100), FVector(35179.884148814017, 54050.465346779674, 100), FVector(35470.565015519969, 54075.487023591995, 100)}},
		{{FVector(35167.051723611075, 53798.589949682355, 100), FVector(35179.884148814017, 54050.465346779674, 100), FVector(35211.178665101761, 53835.833956766874, 100)}}
    };
    const TArray<FVector2D> Domain{FVector2D(34501.642190793063, 53992.118802201003), FVector2D(35497.957809211221, 54077.881197724491), FVector2D(34562.24219079013, 53288.118802197278)};
    ProjectWorldRoadStoredPolygon::FVertexMap Bindings;
    TArray<FProjectWorldSurfaceTriangle> Faces;
    FString Error;
    if (!TestTrue(TEXT("Exact stored shared edges remain one connected original domain."),
        ProjectWorldRoadStoredPolygon::StoreGroup(Pieces, Domain, Bindings, Faces, Error)))
    { AddError(Error); return false; }
    TestTrue(TEXT("Native certificate retains positive geometry."), !Faces.IsEmpty());
    const auto Saved = Faces;
    const int32 Count = Bindings.Num();
    const auto P = [](double X, double Y) { return FVector(X, Y, 100); };
    const TArray<FProjectWorldSurfaceTriangle> Touching{{{P(0,0), P(1,0), P(0,1)}}, {{P(0,1), P(1,1), P(0,2)}}};
    TestFalse(TEXT("Point-touch-only pieces cannot certify an edge-connected surface."),
        ProjectWorldRoadStoredPolygon::StoreGroup(Touching, {FVector2D(0,0), FVector2D(2,0), FVector2D(0,2)}, Bindings, Faces, Error));
    TestTrue(TEXT("Exact connectivity refuses point-only contact."), Error.Contains(TEXT("edge-connected")));
    const TArray<FProjectWorldSurfaceTriangle> Gapped{{{P(0,0),P(0.9,0),P(0,2)}}, {{P(0.9,0),P(0.9,2),P(0,2)}},
        {{P(1.1,0),P(2.2,0),P(1.1,2)}}, {{P(2.2,0),P(2.2,2),P(1.1,2)}}};
    TestFalse(TEXT("A genuine gap refuses even when exterior area compensates its area."),
        ProjectWorldRoadStoredPolygon::StoreGroup(Gapped, {FVector2D(0,0),FVector2D(2,0),FVector2D(2,2),FVector2D(0,2)}, Bindings, Faces, Error));
    TestTrue(TEXT("Exact connectivity refuses separated positive surfaces."), Error.Contains(TEXT("edge-connected")));
    TestTrue(TEXT("Connectivity refusal preserves populated output and bindings."), Faces.Num()==Saved.Num() &&
        Faces[0].Points[0]==Saved[0].Points[0] && Bindings.Num()==Count);
    return true;
}

#endif
