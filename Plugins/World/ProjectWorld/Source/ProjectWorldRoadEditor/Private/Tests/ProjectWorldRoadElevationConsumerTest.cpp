// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "AssetCompilingManager.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadElevationConsumerTest,
	"Project.World.Realization.Road.ElevationSavedConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadElevationConsumerTest::RunTest(const FString& Parameters)
{
	// An optional authenticated input exercises the same consumer with a supplied source-shaped candidate.
	const FString Input = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_ROAD_ELEVATION_INPUT"));
	const FString Evidence = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_ROAD_ELEVATION_EVIDENCE"));
	FProjectWorldCanonicalBundle Bundle;
	if (!Input.IsEmpty())
	{
		FProjectWorldCanonicalValidation Validation;
		if (!FProjectWorldCanonicalLoader::Load(Input, Bundle, Validation))
		{ AddError(Validation.ErrorCode + TEXT(": ") + Validation.Message + TEXT(" ") + Validation.Detail); return false; }
	}
	else
	{
		Bundle.CoordinateQuantizationMeters = 0.01;
		auto& C = Bundle.Cells.AddDefaulted_GetRef();
		C.CellId = TEXT("cell_0"); C.Bounds = C.Terrain.Bounds = FVector4d(0, 0, 30, 30);
		C.Terrain.SampleSpacing = FVector2D(30, 30); C.Terrain.SamplesX = C.Terrain.SamplesY = 2;
		C.Terrain.HeightsMeters = {0, 0, 0, 0}; C.Terrain.ArtifactHash = FString::ChrN(64, 'a');
		FProjectWorldCanonicalFeature Road;
		Road.FeatureId = TEXT("fixture:bridge-consumer"); Road.FeatureClass = TEXT("road");
		Road.RoadClass = TEXT("primary"); Road.GeometryType = TEXT("LineString"); Road.WidthMeters = 4;
		Road.GeometryPoints = {FVector2D(5, 15), FVector2D(25, 15)};
		Road.OwnerCellId = C.CellId;
		auto& R = Road.Representations.AddDefaulted_GetRef();
		R.CellId = C.CellId; R.Kind = TEXT("road_fragment"); R.Parts = {Road.GeometryPoints};
		FProjectWorldCanonicalRoadElevation E;
		E.SourceFeatureId = Road.FeatureId; E.SourcePoints = Road.GeometryPoints;
		E.WidthMeters = 4; E.LengthMeters = 20; E.TerrainArtifacts.Add(C.CellId, C.Terrain.ArtifactHash);
		E.Triangles = {{{FVector(0, -2, 0), FVector(10, -2, 1), FVector(0, 2, 0)}},
			{{FVector(10, -2, 1), FVector(10, 2, 2), FVector(0, 2, 0)}},
			{{FVector(10, -2, 1), FVector(20, -2, 0), FVector(10, 2, 2)}},
			{{FVector(20, -2, 0), FVector(20, 2, 0), FVector(10, 2, 2)}}};
		Road.RoadElevation = MoveTemp(E); C.OwnedFeatureIds = {Road.FeatureId};
		Bundle.Features.Add(Road.FeatureId, MoveTemp(Road));
	}
	// Optional join evidence retains only roads with an exact source endpoint shared with a profile.
	TArray<FVector2D> Abutments;
	for (const auto& Feature : Bundle.Features)
		if (Feature.Value.RoadElevation.IsSet())
		{ Abutments.Add(Feature.Value.GeometryPoints[0]); Abutments.Add(Feature.Value.GeometryPoints.Last()); }
	const bool bIncident = !Input.IsEmpty() &&
		FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_ROAD_ELEVATION_INCLUDE_INCIDENT")) == TEXT("1");
	for (auto It = Bundle.Features.CreateIterator(); It; ++It)
	{
		const auto& Feature = It.Value();
		const bool bEndpoint = bIncident && Feature.FeatureClass == TEXT("road") &&
			Feature.GeometryType == TEXT("LineString") && !Feature.GeometryPoints.IsEmpty() &&
			(Abutments.Contains(Feature.GeometryPoints[0]) || Abutments.Contains(Feature.GeometryPoints.Last()));
		if (!Feature.RoadElevation.IsSet() && !bEndpoint) It.RemoveCurrent();
	}
	if (!TestTrue(TEXT("Candidate contains a profiled Road."), !Bundle.Features.IsEmpty())) return false;
	for (auto& C : Bundle.Cells)
	{
		C.OwnedFeatureIds.RemoveAll([&](const auto& Id) { return !Bundle.Features.Contains(Id); });
		C.ReferencedFeatureIds.RemoveAll([&](const auto& Id) { return !Bundle.Features.Contains(Id); });
	}
	FProjectWorldRealizationLayer Layer;
	Layer.LayerId = TEXT("roads"); Layer.GeneratorId = TEXT("project_road_mesh"); Layer.GeneratorVersion = 1;
	Layer.DependencyHaloCells = 1; Layer.ArtifactRoot = TEXT("/ProjectWorldVerify/RoadElevationConsumer/Generated/");
	Layer.NormalizedSettings = TEXT("{\"selected_classes\":[\"primary\"],\"surface_offset_m\":0.15,\"maximum_segment_length_m\":7.5,")
		TEXT("\"nanite\":true,\"collision\":\"complex_as_simple\",\"structure_fallback\":\"terrain_drape\",\"intersection_policy\":\"overlap_same_owner\"}");
	TSet<FString> Classes;
	for (const auto& Feature : Bundle.Features) Classes.Add(Feature.Value.RoadClass);
	TArray<FString> OrderedClasses = Classes.Array(); OrderedClasses.Sort();
	TArray<TSharedPtr<FJsonValue>> SelectedClasses;
	for (const FString& Class : OrderedClasses) SelectedClasses.Add(MakeShared<FJsonValueString>(Class));
	TSharedPtr<FJsonObject> Settings;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Layer.NormalizedSettings), Settings))
	{ AddError(TEXT("Native consumer fixture settings are invalid.")); return false; }
	Settings->SetArrayField(TEXT("selected_classes"), SelectedClasses);
	Layer.NormalizedSettings.Reset();
	FJsonSerializer::Serialize(Settings.ToSharedRef(), TJsonWriterFactory<>::Create(&Layer.NormalizedSettings));
	Layer = ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
	FProjectWorldRealizationProfile Profile; Profile.Layers.Add(Layer);
	FString Error; TMap<FString, FString> InputHashes;
	if (!ProjectWorldRoadRealization::HashUnitInputs(Bundle, Layer, InputHashes, Error))
	{ AddError(Error); return false; }
	FProjectWorldVerifyWorld World(TEXT("RoadElevationConsumer"));
	FProjectWorldRealizationResult Result;
	Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Layer));
	if (!World.IsReady(Error) || !ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile,
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), Result, Error) ||
		!World.SaveOwned(Error) || !World.Reload(Error)) { AddError(Error); return false; }
	TArray<TSharedPtr<FJsonValue>> Faces;
	TArray<TSharedPtr<FJsonValue>> FallbackFaces, CollisionFaces;
	TArray<TSharedPtr<FJsonValue>> ProfileFaces, ProfileFallbackFaces, ProfileCollisionFaces;
	TArray<TSharedPtr<FJsonValue>> SettingsEvidence;
	TSet<FString> PendingProfileCells;
	for (const auto& Cell : Bundle.Cells)
		for (const auto& Entry : Bundle.Features)
			if (Entry.Value.RoadElevation.IsSet() &&
				(Cell.OwnedFeatureIds.Contains(Entry.Key) || Cell.ReferencedFeatureIds.Contains(Entry.Key)))
				PendingProfileCells.Add(Cell.CellId);
	int32 Actors = 0, Rays = 0, Hits = 0;
	for (TActorIterator<AStaticMeshActor> It(World.GetWorld()); It; ++It)
	{
		FString CellId, Semantic;
		if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, Semantic)) continue;
		++Actors;
		const bool bProfiled = PendingProfileCells.Remove(CellId) > 0;
		const int32 SourceBegin = Faces.Num(), FallbackBegin = FallbackFaces.Num(), CollisionBegin = CollisionFaces.Num();
		UStaticMesh* Mesh = It->GetStaticMeshComponent()->GetStaticMesh();
		FAssetCompilingManager::Get().FinishAllCompilation();
		const auto* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
		if (!TestNotNull(TEXT("Reloaded native source mesh exists."), Description)) return false;
		const auto Positions = FStaticMeshConstAttributes(*Description).GetVertexPositions();
		const auto& Nanite = Mesh->GetNaniteSettings();
		auto NativeSettings = MakeShared<FJsonObject>();
		NativeSettings->SetStringField(TEXT("cell_id"), CellId);
		NativeSettings->SetBoolField(TEXT("contains_canonical_profile"), bProfiled);
		NativeSettings->SetNumberField(TEXT("fallback_target"), static_cast<uint8>(Nanite.FallbackTarget));
		NativeSettings->SetNumberField(TEXT("fallback_percent"), Nanite.FallbackPercentTriangles);
		NativeSettings->SetNumberField(TEXT("fallback_error"), Nanite.FallbackRelativeError);
		NativeSettings->SetNumberField(TEXT("keep_percent"), Nanite.KeepPercentTriangles);
		NativeSettings->SetNumberField(TEXT("trim_error"), Nanite.TrimRelativeError);
		SettingsEvidence.Add(MakeShared<FJsonValueObject>(NativeSettings));
		auto Encode = [&](const FVector P[3])
		{
			TArray<TSharedPtr<FJsonValue>> Points;
			for (int32 I = 0; I < 3; ++I)
			{
				const auto Q = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, P[I] + It->GetActorLocation());
				Points.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
					MakeShared<FJsonValueNumber>(Q.X), MakeShared<FJsonValueNumber>(Q.Y), MakeShared<FJsonValueNumber>(Q.Z)}));
			}
			return MakeShared<FJsonValueArray>(Points);
		};
		for (const auto T : Description->Triangles().GetElementIDs())
		{
			FVector Points[3];
			for (int32 I = 0; I < 3; ++I)
			{
				const auto V = Description->GetVertexInstanceVertex(Description->GetTriangleVertexInstance(T, I));
				Points[I] = FVector(Positions[V]);
			}
			Faces.Add(Encode(Points));
			if (bProfiled) ProfileFaces.Add(Faces.Last());
		}
		const auto* Render = Mesh->GetRenderData();
		if (!TestTrue(TEXT("Compiled fallback render data exists."), Render && !Render->LODResources.IsEmpty())) return false;
		const auto& LOD = Render->LODResources[0];
		const auto Indices = LOD.IndexBuffer.GetArrayView();
		for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
		{
			const FVector Points[3] = {FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I])),
				FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I + 1])),
				FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I + 2]))};
			FallbackFaces.Add(Encode(Points));
			if (bProfiled) ProfileFallbackFaces.Add(FallbackFaces.Last());
		}
		FTriMeshCollisionData Collision;
		if (!TestTrue(TEXT("Reloaded native collision has triangle coverage."), Mesh->GetPhysicsTriMeshData(&Collision, true) && !Collision.Indices.IsEmpty())) return false;
		Mesh->GetBodySetup()->CreatePhysicsMeshes();
		for (const auto& T : Collision.Indices)
		{
			const FVector Points[3] = {FVector(Collision.Vertices[T.v0]), FVector(Collision.Vertices[T.v1]), FVector(Collision.Vertices[T.v2])};
			CollisionFaces.Add(Encode(Points));
			if (bProfiled) ProfileCollisionFaces.Add(CollisionFaces.Last());
			const FVector Center = (FVector(Collision.Vertices[T.v0]) + FVector(Collision.Vertices[T.v1]) + FVector(Collision.Vertices[T.v2])) / 3 + It->GetActorLocation();
			FHitResult Hit; FCollisionQueryParams Query; Query.bTraceComplex = true;
			++Rays;
			if (It->GetStaticMeshComponent()->LineTraceComponent(Hit, Center + FVector(0, 0, 100), Center - FVector(0, 0, 100), Query)) ++Hits;
		}
		NativeSettings->SetNumberField(TEXT("source_triangles"), Faces.Num() - SourceBegin);
		NativeSettings->SetNumberField(TEXT("fallback_triangles"), FallbackFaces.Num() - FallbackBegin);
		NativeSettings->SetNumberField(TEXT("collision_triangles"), CollisionFaces.Num() - CollisionBegin);
	}
	TestTrue(TEXT("Only candidate Road outputs were created."), Actors > 0 && Actors <= Bundle.Cells.Num());
	TestEqual(TEXT("Every profiled cell has a saved native output."), PendingProfileCells.Num(), 0);
	TestTrue(TEXT("The exact profile comparison contains real source triangles."), !ProfileFaces.IsEmpty());
	TestEqual(TEXT("Saved native collision supports all sampled face interiors."), Hits, Rays);
	auto FaceKeys = [](const TArray<TSharedPtr<FJsonValue>>& Values)
	{
		TArray<FString> Keys;
		for (const auto& Face : Values)
		{
			TArray<FString> Points;
			for (const auto& Point : Face->AsArray())
			{
				const auto& P = Point->AsArray();
				Points.Add(FString::Printf(TEXT("%.17g,%.17g,%.17g"), P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber()));
			}
			Points.Sort(); Keys.Add(FString::Join(Points, TEXT(";")));
		}
		Keys.Sort(); return Keys;
	};
	const auto SourceKeys = FaceKeys(ProfileFaces);
	TestTrue(TEXT("Reloaded fallback retains every profiled source triangle and position."), SourceKeys == FaceKeys(ProfileFallbackFaces));
	TestTrue(TEXT("Reloaded physics retains every profiled source triangle and position."), SourceKeys == FaceKeys(ProfileCollisionFaces));
	if (!Evidence.IsEmpty())
	{
		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp")));
		if (!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Evidence), Root))
		{ AddError(TEXT("Diagnostic evidence must remain under project tmp.")); return false; }
		auto Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("source_result_sha256"), Bundle.CompileResultHash);
		Report->SetBoolField(TEXT("includes_incident_roads"), bIncident);
		TArray<TSharedPtr<FJsonValue>> FeatureIds;
		for (const auto& Feature : Bundle.Features) FeatureIds.Add(MakeShared<FJsonValueString>(Feature.Key));
		Report->SetArrayField(TEXT("selected_features"), FeatureIds);
		Report->SetArrayField(TEXT("triangles"), Faces);
		Report->SetArrayField(TEXT("fallback_triangles"), FallbackFaces);
		Report->SetArrayField(TEXT("collision_triangles"), CollisionFaces);
		Report->SetArrayField(TEXT("profile_triangles"), ProfileFaces);
		Report->SetArrayField(TEXT("profile_fallback_triangles"), ProfileFallbackFaces);
		Report->SetArrayField(TEXT("profile_collision_triangles"), ProfileCollisionFaces);
		Report->SetArrayField(TEXT("native_settings"), SettingsEvidence);
		// Keep byte copies of disposable saved meshes for an independent live-editor pixel/PIE probe.
		const FString Snapshot = FPaths::Combine(FPaths::GetPath(Evidence), TEXT("native-mesh-content"));
		TArray<TSharedPtr<FJsonValue>> MeshPaths;
		for (TActorIterator<AStaticMeshActor> It(World.GetWorld()); It; ++It)
		{
			FString CellId, Semantic;
			if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, Semantic)) continue;
			UStaticMesh* Mesh = It->GetStaticMeshComponent()->GetStaticMesh();
			const FString Package = Mesh->GetOutermost()->GetName();
			if (!Package.StartsWith(World.GetRoot())) { AddError(TEXT("Snapshot mesh escaped disposable ownership.")); return false; }
			FString Relative = Package;
			Relative.RemoveFromStart(TEXT("/ProjectWorldVerify/"));
			const FString Source = FPackageName::LongPackageNameToFilename(Package);
			const FString Destination = FPaths::Combine(Snapshot, Relative);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true);
			for (const TCHAR* Extension : {TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk")})
			{
				if (IFileManager::Get().FileExists(*(Source + Extension)) &&
					IFileManager::Get().Copy(*(Destination + Extension), *(Source + Extension)) != COPY_OK)
				{ AddError(TEXT("Cannot copy disposable native mesh evidence.")); return false; }
			}
			if (!IFileManager::Get().FileExists(*(Destination + TEXT(".uasset")))) return false;
			MeshPaths.Add(MakeShared<FJsonValueString>(Mesh->GetPathName()));
		}
		Report->SetStringField(TEXT("mesh_snapshot_content"), Snapshot);
		Report->SetArrayField(TEXT("mesh_paths"), MeshPaths);
		Report->SetNumberField(TEXT("actors"), Actors); Report->SetNumberField(TEXT("collision_rays"), Rays);
		Report->SetNumberField(TEXT("collision_hits"), Hits); Report->SetBoolField(TEXT("production_adoption"), false);
		FString Text;
		FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Evidence), true);
		TestTrue(TEXT("Native diagnostic evidence writes."), FFileHelper::SaveStringToFile(Text, *Evidence, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	}
	FProjectWorldRealizationResult NoOp;
	NoOp.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Layer));
	NoOp.LayerInventories[0].FinalDirtyUnits.Reset();
	if (!ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile,
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), NoOp, Error))
	{ AddError(Error); return false; }
	TestEqual(TEXT("Cold unchanged profile saves no actors."), NoOp.SelfSavedActorMutationCount, 0);
	TestEqual(TEXT("Cold unchanged profile updates no actors."), NoOp.UpdatedActorCount, 0);
	if (Input.IsEmpty())
	{
		for (auto& Feature : Bundle.Features) Feature.Value.RoadElevation.Reset();
		FProjectWorldRealizationResult Removed;
		Removed.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Layer));
		if (!ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile,
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), Removed, Error) ||
			!World.SaveOwned(Error) || !World.Reload(Error)) { AddError(Error); return false; }
		TestEqual(TEXT("Removing the fixture profile updates its one cell."), Removed.UpdatedActorCount, 1);
		for (TActorIterator<AStaticMeshActor> It(World.GetWorld()); It; ++It)
		{
			FString CellId, Semantic;
			if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, Semantic)) continue;
			const auto& Actual = It->GetStaticMeshComponent()->GetStaticMesh()->GetNaniteSettings();
			const FMeshNaniteSettings Defaults;
			TestTrue(TEXT("Profile removal restores ordinary native fallback and retains Nanite."), Actual.bEnabled &&
				Actual.FallbackTarget == Defaults.FallbackTarget && Actual.FallbackPercentTriangles == Defaults.FallbackPercentTriangles &&
				Actual.FallbackRelativeError == Defaults.FallbackRelativeError && Actual.KeepPercentTriangles == Defaults.KeepPercentTriangles &&
				Actual.TrimRelativeError == Defaults.TrimRelativeError);
		}
	}
	AddInfo(FString::Printf(TEXT("Native saved consumer: %d actors, %d source faces, %d/%d collision rays."), Actors, Faces.Num(), Hits, Rays));
	return true;
}
#endif
