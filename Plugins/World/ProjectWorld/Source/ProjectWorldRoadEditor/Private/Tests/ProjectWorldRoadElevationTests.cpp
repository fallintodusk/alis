// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldRoadElevation.h"
#include "ProjectWorldRoadSurface.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRoadElevationTests
{
	FProjectWorldCanonicalBundle Bundle()
	{
		FProjectWorldCanonicalBundle Result;
		Result.CoordinateQuantizationMeters = 0.01;
		for (int32 X = 0; X < 2; ++X)
		{
			auto& C = Result.Cells.AddDefaulted_GetRef();
			C.CellId = FString::Printf(TEXT("cell_%d"), X);
			C.CellX = X;
			C.Bounds = C.Terrain.Bounds = FVector4d(X * 10, 0, (X + 1) * 10, 10);
			C.Terrain.SamplesX = 3; C.Terrain.SamplesY = 2;
			C.Terrain.SampleSpacing = FVector2D(5, 10);
			C.Terrain.HeightsMeters = X ? TArray<double>{0, 5, 5, 0, 5, 5} : TArray<double>{5, 5, 0, 5, 5, 0};
			C.Terrain.ArtifactHash = FString::ChrN(64, X ? 'b' : 'a');
		}
		FProjectWorldCanonicalFeature Road;
		Road.FeatureId = TEXT("fixture:qualified-bridge"); Road.FeatureClass = TEXT("road");
		Road.GeometryType = TEXT("LineString"); Road.GeometryPoints = {FVector2D(2, 5), FVector2D(18, 5)};
		Road.RoadClass = TEXT("primary"); Road.WidthMeters = 4; Road.OwnerCellId = TEXT("cell_0");
		for (int32 X = 0; X < 2; ++X)
		{
			auto& R = Road.Representations.AddDefaulted_GetRef();
			R.CellId = Result.Cells[X].CellId; R.Kind = TEXT("road_fragment");
			R.Parts = {{FVector2D(X ? 10 : 2, 5), FVector2D(X ? 18 : 10, 5)}};
		}
		Result.Cells[0].OwnedFeatureIds = {Road.FeatureId};
		Result.Cells[1].ReferencedFeatureIds = {Road.FeatureId};
		Result.Features.Add(Road.FeatureId, MoveTemp(Road));
		return Result;
	}
	FProjectWorldRealizationLayer Layer()
	{
		FProjectWorldRealizationLayer L;
		L.LayerId = TEXT("roads"); L.GeneratorId = TEXT("project_road_mesh"); L.GeneratorVersion = 1;
		L.DependencyHaloCells = 1;
		L.NormalizedSettings = TEXT("{\"selected_classes\":[\"primary\"],\"surface_offset_m\":0.15,\"maximum_segment_length_m\":7.5,")
			TEXT("\"nanite\":true,\"collision\":\"complex_as_simple\",\"structure_fallback\":\"terrain_drape\",\"intersection_policy\":\"overlap_same_owner\"}");
		return ProjectWorldVerifyTestFlow::Finalize(MoveTemp(L));
	}
	void AddProfile(FProjectWorldCanonicalBundle& B)
	{
		FProjectWorldCanonicalRoadElevation E;
		E.LengthMeters = 16; E.WidthMeters = 4;
		const auto& Road = B.Features[TEXT("fixture:qualified-bridge")];
		E.SourceFeatureId = Road.FeatureId; E.SourcePoints = Road.GeometryPoints;
		E.Triangles = {{{FVector(0, -2, 5), FVector(16, -2, 5), FVector(0, 2, 5)}},
			{{FVector(16, -2, 5), FVector(16, 2, 5), FVector(0, 2, 5)}}};
		for (const auto& C : B.Cells) E.TerrainArtifacts.Add(C.CellId, C.Terrain.ArtifactHash);
		B.Features[TEXT("fixture:qualified-bridge")].RoadElevation = MoveTemp(E);
	}
	double Area(const FMeshDescription& Mesh)
	{
		const auto Positions = FStaticMeshConstAttributes(Mesh).GetVertexPositions();
		double Result = 0;
		for (const auto Id : Mesh.Triangles().GetElementIDs())
		{
			FVector P[3];
			for (int32 I = 0; I < 3; ++I) P[I] = FVector(Positions[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Id, I))]);
			Result += FMath::Abs(FVector::CrossProduct(P[1] - P[0], P[2] - P[0]).Z) / 20000.0;
		}
		return Result;
	}
	bool ConstructedFixture(FAutomationTestBase& Test, const FString& Input)
	{
		FString Text, Error; TSharedPtr<FJsonObject> Fixture, Surface;
		if (!Test.TestTrue(TEXT("Explicit constructed fixture parses."), FFileHelper::LoadFileToString(Text, *Input) &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Fixture))) return false;
		const FString SurfacePath = Fixture->GetStringField(TEXT("surface_path"));
		const FString OutputPath = Fixture->GetStringField(TEXT("output_path"));
		if (!Test.TestTrue(TEXT("Diagnostic output is new and confined to project tmp."),
			FPaths::IsUnderDirectory(OutputPath, FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp")))) &&
			!FPaths::FileExists(OutputPath))) return false;
		FString SurfaceHash;
		if (!Test.TestTrue(TEXT("The exact pure constructor output is consumed."),
			FProjectWorldCanonicalLoader::ComputeFileSha256(SurfacePath, SurfaceHash) &&
			SurfaceHash == Fixture->GetStringField(TEXT("surface_sha256")) && FFileHelper::LoadFileToString(Text, *SurfacePath) &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Surface) &&
			Surface->GetStringField(TEXT("model")) == TEXT("observed_constructed_tin_candidate_not_surveyed") &&
			Surface->GetStringField(TEXT("endpoint_relation")) == TEXT("constructed_surface"))) return false;
		FProjectWorldCanonicalBundle B; FProjectWorldCanonicalValidation Validation;
		if (!Test.TestTrue(TEXT("The diagnostic starts with an authenticated immutable canonical base."),
			FProjectWorldCanonicalLoader::Load(Fixture->GetStringField(TEXT("base_result")), B, Validation)))
		{ Test.AddError(Validation.Message); return false; }
		const FString RoadId = Fixture->GetStringField(TEXT("road_id"));
		auto* Road = B.Features.Find(RoadId);
		if (!Test.TestNotNull(TEXT("The selected original Road is present."), Road)) return false;
		FProjectWorldCanonicalRoadElevation E;
		E.SourceFeatureId = RoadId; E.SourcePoints = Road->GeometryPoints;
		E.WidthMeters = Surface->GetNumberField(TEXT("width_m")); E.LengthMeters = Surface->GetNumberField(TEXT("length_m"));
		E.EndpointRelation = EProjectWorldRoadEndpointRelation::ConstructedSurface; E.ConstructedSurfaceHash = SurfaceHash;
		for (const auto& Pair : Fixture->GetObjectField(TEXT("terrain_artifacts"))->Values) E.TerrainArtifacts.Add(FString(Pair.Key), Pair.Value->AsString());
		for (const auto& V : Surface->GetArrayField(TEXT("uv_triangles")))
		{
			FProjectWorldCanonicalRoadElevationTriangle Face; const auto Corners = V->AsArray();
			if (!Test.TestEqual(TEXT("Surface triangles have three corners."), Corners.Num(), 3)) return false;
			for (int32 I=0; I<3; ++I)
			{
				const auto P = Corners[I]->AsArray(); if (!Test.TestEqual(TEXT("Surface points are UVZ triples."), P.Num(), 3)) return false;
				Face.UVZ[I] = FVector(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
			}
			E.Triangles.Add(Face);
		}
		Road->RoadElevation = MoveTemp(E);
		for (auto& C : B.Cells) { C.OwnedFeatureIds.Reset(); C.ReferencedFeatureIds.Reset(); }
		for (const auto& V : Fixture->GetArrayField(TEXT("selected_cells")))
		{
			auto* C = B.Cells.FindByPredicate([&](const auto& Cell){ return Cell.CellId == V->AsString(); });
			if (!Test.TestNotNull(TEXT("The explicit Road consumer cell exists."), C)) return false;
			C->OwnedFeatureIds = {RoadId};
		}
		auto L = Layer(); L.NormalizedSettings.ReplaceInline(TEXT("primary"), TEXT("footway"));
		L = ProjectWorldVerifyTestFlow::Finalize(MoveTemp(L));
		TArray<TSharedPtr<FJsonValue>> Vertices, Faces; int32 Built=0;
		for (const auto& C : B.Cells)
		{
			if (C.OwnedFeatureIds.IsEmpty()) continue;
			FMeshDescription Mesh; FVector Origin;
			if (!Test.TestTrue(TEXT("Actual native Road geometry consumes the explicit structural endpoint model."),
				ProjectWorldRoadRealization::BuildDescription(B, C, L, Mesh, Origin, Error))) { Test.AddError(Error); return false; }
			TMap<FVertexID, int32> Indices; const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
			for (const auto V : Mesh.Vertices().GetElementIDs())
			{
				const FVector P=FProjectWorldCanonicalLoader::UnrealToCanonical(B, FVector(Positions[V])+Origin);
				Indices.Add(V, Vertices.Num());
				Vertices.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
					MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y), MakeShared<FJsonValueNumber>(P.Z)}));
			}
			for (const auto T : Mesh.Triangles().GetElementIDs())
			{
				TArray<TSharedPtr<FJsonValue>> Row;
				for (int32 I=0; I<3; ++I) Row.Add(MakeShared<FJsonValueNumber>(Indices[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(T,I))]));
				Faces.Add(MakeShared<FJsonValueArray>(Row));
			}
			++Built;
		}
		if (!Test.TestTrue(TEXT("The exact constructed candidate produces a nonempty native mesh."), !Faces.IsEmpty())) return false;
		auto Result=MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("scope"), TEXT("Typed source-mesh diagnostic only; no production model admission, saved or cooked collision, incident join or geographic acceptance"));
		Result->SetStringField(TEXT("surface_sha256"), SurfaceHash); Result->SetNumberField(TEXT("built_cells"), Built);
		Result->SetArrayField(TEXT("vertices"), Vertices); Result->SetArrayField(TEXT("triangles"), Faces);
		FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text));
		Test.AddInfo(FString::Printf(TEXT("ConstructedRoadProof cells=%d vertices=%d triangles=%d model=%s"), Built, Vertices.Num(), Faces.Num(), *SurfaceHash));
		return Test.TestTrue(TEXT("Disposable native output is written to its declared path."), FFileHelper::SaveStringToFile(Text, *OutputPath));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadElevationTest,
	"Project.World.Realization.Road.CanonicalElevation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadElevationTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRoadElevationTests;
	auto B = Bundle(); const auto L = Layer();
	FString Error;
	FMeshDescription Drape;
	FVector Origin;
	if (!TestTrue(TEXT("Existing drape builds the known flooded control."),
		ProjectWorldRoadRealization::BuildDescription(B, B.Cells[0], L, Drape, Origin, Error))) { AddError(Error); return false; }
	double Minimum = 100;
	for (const auto Id : Drape.Vertices().GetElementIDs())
		Minimum = FMath::Min(Minimum, FProjectWorldCanonicalLoader::UnrealToCanonical(B,
			FVector(FStaticMeshConstAttributes(Drape).GetVertexPositions()[Id]) + Origin).Z);
	TestTrue(TEXT("Draping a bridge follows the valley below the 5 m water control."), Minimum < 1);
	AddProfile(B);
	TMap<FString, FString> Batch;
	if (!TestTrue(TEXT("Profile batch identity validates exactly the realized terrain."),
		ProjectWorldRoadRealization::HashUnitInputs(B, L, Batch, Error))) { AddError(Error); return false; }
	for (const auto& Cell : B.Cells)
	{
		FMeshDescription Deck; FString Identity;
		if (!TestTrue(TEXT("Profile builds without changing canonical Terrain."),
			ProjectWorldRoadRealization::BuildDescription(B, Cell, L, Deck, Origin, Error))) { AddError(Error); return false; }
		TestTrue(TEXT("Original width and clipped planform area retained."), FMath::Abs(Area(Deck) - 32) < 1e-5);
		for (const auto Id : Deck.Vertices().GetElementIDs())
		{
			const auto P = FProjectWorldCanonicalLoader::UnrealToCanonical(B,
				FVector(FStaticMeshConstAttributes(Deck).GetVertexPositions()[Id]) + Origin);
			TestTrue(TEXT("Source elevation receives presentation offset exactly once."), FMath::Abs(P.Z - 5.15) < 1e-6);
		}
		TestTrue(TEXT("Single and batch input paths agree."), ProjectWorldRoadRealization::HashCellInput(B, Cell, L, Identity, Error));
		TestEqual(TEXT("Identical consumed input identity."), Identity, Batch[Cell.CellId]);
	}
	const auto Original = B;
	for (auto& Cell : B.Cells)
	{
		Cell.Terrain.BaseArtifactHash = Cell.Terrain.ArtifactHash;
		Cell.Terrain.ArtifactHash = FString::ChrN(64, 'e');
	}
	FString RefinedIdentity;
	TestTrue(TEXT("Explicit UV grade support remains qualified against its immutable grid."),
		ProjectWorldRoadRealization::HashCellInput(B, B.Cells[0], L, RefinedIdentity, Error));
	TestNotEqual(TEXT("Final Terrain clearance has its own consumed identity."), RefinedIdentity, Batch[TEXT("cell_0")]);
	FMeshDescription SameDeck;
	TestTrue(TEXT("Changing the final artifact identity does not refit the accepted UV deck."),
		ProjectWorldRoadRealization::BuildDescription(B, B.Cells[0], L, SameDeck, Origin, Error));
	B = Original;
	auto& Field = B.Features[TEXT("fixture:qualified-bridge")].RoadElevation.GetValue();
	Field.Triangles = {{{FVector(0, -2, 5), FVector(8, -2, 6), FVector(0, 2, 5)}},
		{{FVector(8, -2, 6), FVector(8, 2, 6), FVector(0, 2, 5)}},
		{{FVector(8, -2, 6), FVector(16, -2, 5), FVector(8, 2, 6)}},
		{{FVector(16, -2, 5), FVector(16, 2, 5), FVector(8, 2, 6)}}};
	FString Changed;
	TestTrue(TEXT("Changed canonical elevation hashes."), ProjectWorldRoadRealization::HashCellInput(B, B.Cells[0], L, Changed, Error));
	TestNotEqual(TEXT("Consumed elevation invalidates Road."), Changed, Batch[TEXT("cell_0")]);
	const auto Interior = B;
	TMap<double, double> SharedSection;
	for (const auto& Cell : B.Cells)
	{
		FMeshDescription Mesh;
		TSet<double> CurrentSection;
		if (!TestTrue(TEXT("Sloping profile builds in each adjacent cell."),
			ProjectWorldRoadRealization::BuildDescription(B, Cell, L, Mesh, Origin, Error))) { AddError(Error); return false; }
		for (const auto Id : Mesh.Vertices().GetElementIDs())
		{
			const auto P = FProjectWorldCanonicalLoader::UnrealToCanonical(B,
				FVector(FStaticMeshConstAttributes(Mesh).GetVertexPositions()[Id]) + Origin);
			if (P.X != 10) continue;
			CurrentSection.Add(P.Y);
			if (Cell.CellX == 0) SharedSection.Add(P.Y, P.Z);
			else
			{
				const auto* Previous = SharedSection.Find(P.Y);
				if (TestNotNull(TEXT("Second cell consumes the first cell's shared vertex."), Previous))
					TestEqual(TEXT("Stored adjacent-cell profile heights agree exactly."), P.Z, *Previous);
			}
		}
		TestTrue(TEXT("Both cell outputs contain their complete shared section."), CurrentSection.Contains(3) && CurrentSection.Contains(7));
	}
	TestTrue(TEXT("The actual shared section is sampled across the full width."), SharedSection.Contains(3) && SharedSection.Contains(7));
	for (int32 Defect = 0; Defect < 9; ++Defect)
	{
		B = Original;
		auto& P = B.Features[TEXT("fixture:qualified-bridge")].RoadElevation.GetValue();
		if (Defect == 0) B.Cells[0].Terrain.ArtifactHash = FString::ChrN(64, 'c');
		if (Defect == 1) P.Triangles.RemoveAt(0);
		if (Defect == 2) P.Triangles[1] = P.Triangles[0];
		if (Defect == 3) P.Triangles[1].UVZ[0].Z += 0.01;
		if (Defect == 4) for (auto& T : P.Triangles) for (auto& Point : T.UVZ) Point.Z = -1;
		if (Defect == 5) for (auto& T : P.Triangles) for (auto& Point : T.UVZ) Point.Z += 1;
		if (Defect == 6)
		{
			auto& Road = B.Features[TEXT("fixture:qualified-bridge")];
			for (auto& Point : Road.GeometryPoints) Point.Y += 1;
			for (auto& R : Road.Representations) for (auto& Part : R.Parts) for (auto& Point : Part) Point.Y += 1;
		}
		if (Defect == 7) { P.TerrainArtifacts.Remove(TEXT("cell_1")); B.Cells[1].Terrain.ArtifactHash = FString::ChrN(64, 'd'); }
		if (Defect == 8)
		{
			B = Interior;
			for (auto& T : B.Features[TEXT("fixture:qualified-bridge")].RoadElevation->Triangles)
				for (auto& Point : T.UVZ) if (Point.X == 8) Point.Z = 1e300;
		}
		FMeshDescription Refused;
		Error.Reset();
		TestFalse(TEXT("Stale, missing, overlapping, discontinuous or buried profile refuses realization."),
			ProjectWorldRoadRealization::BuildDescription(B, B.Cells[0], L, Refused, Origin, Error));
		TestTrue(TEXT("Refusal is observable."), !Error.IsEmpty());
		TestEqual(TEXT("Refusal emits no partial mesh."), Refused.Triangles().Num(), 0);
		TestFalse(TEXT("The input planner refuses the same invalid profile."),
			ProjectWorldRoadRealization::HashCellInput(B, B.Cells[0], L, Changed, Error));
	}
	B = Bundle();
	for (auto& C : B.Cells) for (auto& H : C.Terrain.HeightsMeters) H = 0;
	ProjectWorldRoadSurface::FContext Context;
	TestTrue(TEXT("Flat control context indexes."), ProjectWorldRoadSurface::Index(B, Context, Error));
	FProjectWorldCanonicalRoadElevation Support;
	for (const auto& C : B.Cells) Support.TerrainArtifacts.Add(C.CellId, C.Terrain.ArtifactHash);
	TArray<ProjectWorldRoadSurface::FRibbonTriangle> Ribbon;
	auto& Underlay = Ribbon.AddDefaulted_GetRef();
	Underlay.Surface = FProjectWorldSurfaceTriangle{{FVector(2, 3, -0.1), FVector(8, 3, -0.1), FVector(2, 7, -0.1)}};
	Underlay.bCanonicalHeight = true; Underlay.Elevation = &Support;
	for (const bool Emit : {false, true})
	{
		FMeshDescription Description; TSet<FString> Terrain;
		TestTrue(TEXT("A source underlay with positive final clearance is accepted."),
			ProjectWorldRoadSurface::Conform(B, B.Cells[0], Context, Ribbon, FVector::ZeroVector, 0.15, 1, Emit ? &Description : nullptr, Terrain, Error));
		for (auto& P : Ribbon[0].Surface.Points) P.Z = -0.2;
		TestFalse(TEXT("Actually buried final geometry refuses."),
			ProjectWorldRoadSurface::Conform(B, B.Cells[0], Context, Ribbon, FVector::ZeroVector, 0.15, 1, Emit ? &Description : nullptr, Terrain, Error));
		for (auto& P : Ribbon[0].Surface.Points) P.Z = -0.1;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadMappedSplitTest,
	"Project.World.Realization.Road.MappedSplit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadMappedSplitTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalFeature Road;
	Road.FeatureId = TEXT("fixture:map-roundoff"); Road.WidthMeters = 4;
	Road.GeometryPoints = {FVector2D(381000, 6185000), FVector2D(381016, 6185000)};
	FProjectWorldCanonicalRoadElevation E;
	E.LengthMeters = 16; E.WidthMeters = 4;
	const double Near = -2 + 1e-10;
	E.Triangles = {{{FVector(0, -2, 5), FVector(16, -2, 5), FVector(0, Near, 5)}},
		{{FVector(16, -2, 5), FVector(16, Near, 5), FVector(0, Near, 5)}},
		{{FVector(0, Near, 5), FVector(16, Near, 5), FVector(0, 2, 5)}},
		{{FVector(16, Near, 5), FVector(16, 2, 5), FVector(0, 2, 5)}}};
	Road.RoadElevation = MoveTemp(E);
	TArray<ProjectWorldRoadSurface::FRibbonTriangle> Output;
	FString Error;
	if (!TestTrue(TEXT("A source-valid UV partition maps at projected coordinates."),
		ProjectWorldRoadElevation::Append(Road, Road.GeometryPoints[0], Road.GeometryPoints[1],
			FVector2D(0, 2), FVector2D(0, 2), 0.01, 0.15, Output, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("The complete original ribbon still has positive source coverage."), !Output.IsEmpty());
	for (const auto& Piece : Output)
		TestTrue(TEXT("Mapping never emits a fan face with an exactly duplicated XYZ edge."),
			Piece.Surface.Points[0] != Piece.Surface.Points[1] && Piece.Surface.Points[1] != Piece.Surface.Points[2] && Piece.Surface.Points[2] != Piece.Surface.Points[0]);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadConstructedSurfaceTest,
	"Project.World.Realization.Road.ConstructedCanonicalSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadConstructedSurfaceTest::RunTest(const FString&)
{
	using namespace ProjectWorldRoadElevationTests;
	const FString Fixture = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_CONSTRUCTED_SURFACE_FIXTURE"));
	if (!Fixture.IsEmpty()) return ConstructedFixture(*this, Fixture);
	auto B = Bundle(); AddProfile(B); const auto L = Layer();
	for (auto& C : B.Cells) for (auto& H : C.Terrain.HeightsMeters) H = 0;
	auto& E = B.Features[TEXT("fixture:qualified-bridge")].RoadElevation.GetValue();
	FString Error, Before, After; FVector Origin; FMeshDescription Mesh;
	TestFalse(TEXT("The existing Terrain-abutment model refuses ends above immutable Terrain."),
		ProjectWorldRoadRealization::BuildDescription(B, B.Cells[0], L, Mesh, Origin, Error));
	E.EndpointRelation = EProjectWorldRoadEndpointRelation::ConstructedSurface;
	E.ConstructedSurfaceHash = FString::ChrN(64, 'c');
	if (!TestTrue(TEXT("An explicitly typed constructed endpoint consumes its own surface, not a Terrain abutment."),
		ProjectWorldRoadRealization::BuildDescription(B, B.Cells[0], L, Mesh, Origin, Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("Constructed input identity is computed."),
		ProjectWorldRoadRealization::HashCellInput(B, B.Cells[0], L, Before, Error));
	for (const auto V : Mesh.Vertices().GetElementIDs())
	{
		const auto P = FProjectWorldCanonicalLoader::UnrealToCanonical(B,
			FVector(FStaticMeshConstAttributes(Mesh).GetVertexPositions()[V]) + Origin);
		TestTrue(TEXT("Every stored constructed point includes the single presentation offset."), FMath::Abs(P.Z - 5.15) < 1e-6);
	}
	E.ConstructedSurfaceHash = FString::ChrN(64, 'd');
	TestTrue(TEXT("Changed supporting model is admitted as a different input."),
		ProjectWorldRoadRealization::HashCellInput(B, B.Cells[0], L, After, Error));
	TestNotEqual(TEXT("Supporting-model identity is actually consumed."), Before, After);
	E.ConstructedSurfaceHash.Reset();
	TestFalse(TEXT("A constructed endpoint without a support identity refuses."),
		ProjectWorldRoadRealization::HashCellInput(B, B.Cells[0], L, After, Error));
	E.ConstructedSurfaceHash = FString::ChrN(64, 'd');
	E.EndpointRelation = static_cast<EProjectWorldRoadEndpointRelation>(255);
	TestFalse(TEXT("Unknown endpoint relations refuse."),
		ProjectWorldRoadRealization::HashCellInput(B, B.Cells[0], L, After, Error));
	E.EndpointRelation = EProjectWorldRoadEndpointRelation::ConstructedSurface;
	for (auto& C : B.Cells) for (auto& H : C.Terrain.HeightsMeters) H = 6;
	FMeshDescription Buried;
	TestFalse(TEXT("Constructed support does not disable actual Terrain clearance."),
		ProjectWorldRoadRealization::BuildDescription(B, B.Cells[0], L, Buried, Origin, Error));
	TestEqual(TEXT("Refusal leaves no partial description."), Buried.Triangles().Num(), 0);
	return true;
}
#endif
