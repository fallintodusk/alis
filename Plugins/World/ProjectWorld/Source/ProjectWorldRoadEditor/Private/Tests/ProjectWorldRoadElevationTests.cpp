// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldRoadElevation.h"
#include "ProjectWorldRoadSurface.h"
#include "ProjectWorldRoadEmission.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldConstructedRoleQuality.h"
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
	bool GangwaySourceMatches(const FJsonObject& Surface, const FProjectWorldCanonicalBundle& B,
		const FProjectWorldCanonicalFeature& Road)
	{
		FString Id, Crs, Datum; const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
		if (!Surface.TryGetStringField(TEXT("source_feature_id"), Id) || Id != Road.FeatureId ||
			!Surface.TryGetStringField(TEXT("canonical_crs"), Crs) || Crs.IsEmpty() || Crs != B.CanonicalCrs ||
			!Surface.TryGetStringField(TEXT("vertical_datum"), Datum) || Datum != TEXT("EPSG:3855") || Datum != B.VerticalDatum ||
			!Surface.TryGetArrayField(TEXT("source_points"), Points) || Points->Num() != Road.GeometryPoints.Num()) return false;
		for (int32 I=0; I<Points->Num(); ++I)
		{
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr; double X, Y;
			if (!(*Points)[I]->TryGetArray(P) || P->Num() != 2 || !(*P)[0]->TryGetNumber(X) || !(*P)[1]->TryGetNumber(Y) ||
				!FMath::IsFinite(X) || !FMath::IsFinite(Y) || FVector2D(X,Y) != Road.GeometryPoints[I]) return false;
		}
		return true;
	}

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
	FProjectWorldStoredSurface StoredMesh(const FMeshDescription& Mesh,const FVector& Origin)
	{
		FProjectWorldStoredSurface Out; Out.Origin=Origin; const auto Positions=FStaticMeshConstAttributes(Mesh).GetVertexPositions();
		for (const auto Id : Mesh.Triangles().GetElementIDs())
		{
			FProjectWorldSurfaceTriangle T;
			for (int32 I=0; I<3; ++I) T.Points[I]=FVector(Positions[Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Id,I))]);
			Out.Triangles.Add(T);
		}
		return Out;
	}
	void CollectMesh(const FProjectWorldCanonicalBundle& Bundle, const FMeshDescription& Mesh, const FVector& Origin,
		TArray<TSharedPtr<FJsonValue>>& Vertices, TArray<TSharedPtr<FJsonValue>>& Faces,bool Native=false)
	{
		TMap<FVertexID, int32> Indices; const auto Positions = FStaticMeshConstAttributes(Mesh).GetVertexPositions();
		for (const auto V : Mesh.Vertices().GetElementIDs())
		{
			const FVector P = Native ? FVector(Positions[V]) : FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, FVector(Positions[V])+Origin);
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
	}

	bool ConstructedFixture(FAutomationTestBase& Test, const FString& Input)
	{
		FString Text, Error; TSharedPtr<FJsonObject> Fixture, Surface;
		if (!Test.TestTrue(TEXT("Explicit constructed fixture parses."), FFileHelper::LoadFileToString(Text, *Input) &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Fixture))) return false;
		bool CompleteStructure=false; Fixture->TryGetBoolField(TEXT("diagnostic_uncarved_structure"),CompleteStructure);
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
			(Surface->GetStringField(TEXT("model")) == TEXT("observed_constructed_tin_candidate_not_surveyed") ||
			 Surface->GetStringField(TEXT("model")) == TEXT("fixed_floating_gangway_candidate_v1")) &&
			Surface->GetStringField(TEXT("endpoint_relation")) == TEXT("constructed_surface"))) return false;
		FProjectWorldCanonicalBundle B; FProjectWorldCanonicalValidation Validation;
		if (!Test.TestTrue(TEXT("The diagnostic starts with an authenticated immutable canonical base."),
			FProjectWorldCanonicalLoader::Load(Fixture->GetStringField(TEXT("base_result")), B, Validation)))
		{ Test.AddError(Validation.Message); return false; }
		double DiagnosticHeightOrigin=B.HeightOriginMeters;
		if (Fixture->TryGetNumberField(TEXT("diagnostic_height_origin_m"),DiagnosticHeightOrigin))
		{
			if (!Test.TestTrue(TEXT("The disposable height-frame experiment is finite."),FMath::IsFinite(DiagnosticHeightOrigin))) return false;
			B.HeightOriginMeters=DiagnosticHeightOrigin;
		}
		const FString RoadId = Fixture->GetStringField(TEXT("road_id"));
		auto* Road = B.Features.Find(RoadId);
		if (!Test.TestNotNull(TEXT("The selected original Road is present."), Road)) return false;
		const bool bGangway = Surface->GetStringField(TEXT("model")) == TEXT("fixed_floating_gangway_candidate_v1");
		if (bGangway && !Test.TestTrue(TEXT("The gangway retains its exact directed source and canonical frame."),
			GangwaySourceMatches(*Surface, B, *Road))) return false;
		FProjectWorldCanonicalRoadElevation E;
		E.SourceFeatureId = RoadId; E.SourcePoints = Road->GeometryPoints;
		E.WidthMeters = Surface->GetNumberField(TEXT("width_m")); E.LengthMeters = Surface->GetNumberField(TEXT("length_m"));
		E.EndpointRelation = EProjectWorldRoadEndpointRelation::ConstructedSurface; E.ConstructedSurfaceHash = SurfaceHash;
		for (const auto& Pair : Fixture->GetObjectField(TEXT("terrain_artifacts"))->Values) E.TerrainArtifacts.Add(FString(Pair.Key), Pair.Value->AsString());
		for (const auto& V : Surface->GetArrayField(bGangway ? TEXT("triangles") : TEXT("uv_triangles")))
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
		TArray<FProjectWorldSurfaceTriangle> BodyField;
		FString AssemblyPath,AssemblyHash;
		if (Fixture->TryGetStringField(TEXT("assembly_path"),AssemblyPath))
		{
			TSharedPtr<FJsonObject> Assembly;
			if (!Test.TestTrue(TEXT("The optional complete body is independently authenticated in the same frame."),
				FProjectWorldCanonicalLoader::ComputeFileSha256(AssemblyPath,AssemblyHash) &&
				AssemblyHash==Fixture->GetStringField(TEXT("assembly_sha256")) &&
				FFileHelper::LoadFileToString(Text,*AssemblyPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Assembly) &&
				Assembly->GetStringField(TEXT("basis"))==TEXT("identified_model") &&
				Assembly->GetStringField(TEXT("canonical_crs"))==B.CanonicalCrs && Assembly->GetStringField(TEXT("vertical_datum"))==B.VerticalDatum)) return false;
			for (const auto& V : Assembly->GetArrayField(TEXT("triangles")))
			{
				const auto& Row=V->AsArray(); FProjectWorldSurfaceTriangle T;
				if (!Test.TestEqual(TEXT("Body triangles retain three corners."),Row.Num(),3)) return false;
				for (int32 I=0; I<3; ++I)
				{
					const auto& P=Row[I]->AsArray(); if (!Test.TestEqual(TEXT("Body corners retain XYZ."),P.Num(),3)) return false;
					T.Points[I]=FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber());
				}
				BodyField.Add(T);
			}
			if (!Test.TestTrue(TEXT("The supplied body is bounded and nonempty."),BodyField.Num()>0 && BodyField.Num()<=4096)) return false;
		}
		TArray<FString> BodySourceRoles; BodySourceRoles.SetNum(BodyField.Num());
		const TArray<TSharedPtr<FJsonValue>>* Referenced = nullptr;
		if (Fixture->TryGetArrayField(TEXT("constructed_sources"),Referenced))
		{
			if (!Test.TestTrue(TEXT("Direct field binding requires the authenticated complete assembly."),!BodyField.IsEmpty() && Referenced->Num()>0 && Referenced->Num()<=8)) return false;
			TSet<int32> BoundFaces; auto Elevation=Road->RoadElevation.GetValue();
			Elevation.Domain=EProjectWorldRoadElevationDomain::ReferencedConstructedSurface;
			Elevation.Triangles.Reset(); Elevation.ConstructedSurfaceHash=AssemblyHash;
			for (const auto& Value : *Referenced)
			{
				const auto Source=Value->AsObject(); FProjectWorldCanonicalFeature Body;
				Body.FeatureId=Source->GetStringField(TEXT("source_feature_id")); Body.FeatureClass=TEXT("constructed_surface");
				if (!Test.TestTrue(TEXT("Structural source bindings are unique and cannot replace the selected Road."),Body.FeatureId!=RoadId && !Elevation.ReferencedSurfaceModels.Contains(Body.FeatureId))) return false;
				Body.GeometryType=TEXT("Polygon"); Body.ConstructedSurface.Emplace(); auto& Model=Body.ConstructedSurface.GetValue();
				Model.SourceFeatureId=Body.FeatureId; Model.ModelHash=Source->GetStringField(TEXT("model_hash"));
				Model.CanonicalCrs=B.CanonicalCrs; Model.VerticalDatum=B.VerticalDatum;
				Model.Basis=TEXT("identified_model"); Model.Role=Source->GetStringField(TEXT("role"));
				for (const auto& Index : Source->GetArrayField(TEXT("triangle_indices")))
				{
					const double Number=Index->AsNumber(); const int32 I=FMath::FloorToInt(Number);
					if (!Test.TestTrue(TEXT("The structural source owns an exact unique assembly face."),Number==I && BodyField.IsValidIndex(I) && !BoundFaces.Contains(I))) return false;
					BoundFaces.Add(I); BodySourceRoles[I]=Model.Role;
					const auto& T=BodyField[I]; Model.Triangles.Add({{T.Points[0],T.Points[1],T.Points[2]}});
				}
				Elevation.ReferencedSurfaceModels.Add(Body.FeatureId,Model.ModelHash); B.Features.Add(Body.FeatureId,MoveTemp(Body));
			}
			if (!Test.TestEqual(TEXT("The reference path consumes the same complete body field exactly once."),BoundFaces.Num(),BodyField.Num())) return false;
			Road=B.Features.Find(RoadId);
			Road->RoadElevation=MoveTemp(Elevation);
		}
		ProjectWorldConstructedRoleQuality::FPolicy RolePolicy;
		if (!Test.TestTrue(TEXT("Optional native role quality authenticates complete original source bindings."),
			ProjectWorldConstructedRoleQuality::Load(*Fixture,AssemblyHash,BodySourceRoles,RolePolicy,Error))) { Test.AddError(Error); return false; }
		TArray<TSharedPtr<FJsonValue>> BodyVertices,BodyFaces,SlopeReceipts,NativeUnits,RoleReceipts;
		FString PolicyPath,PolicyHash; double MaximumGrade=0,MaximumCross=0;
		if (Fixture->TryGetStringField(TEXT("quality_policy_path"),PolicyPath))
		{
			TSharedPtr<FJsonObject> Policy;
			if (!Test.TestTrue(TEXT("Stored geometry uses the exact predeclared quality policy."),
				FProjectWorldCanonicalLoader::ComputeFileSha256(PolicyPath,PolicyHash) && PolicyHash==Fixture->GetStringField(TEXT("quality_policy_sha256")) &&
				FFileHelper::LoadFileToString(Text,*PolicyPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Policy))) return false;
			MaximumGrade=Policy->GetNumberField(TEXT("maximum_gangway_grade")); MaximumCross=Policy->GetNumberField(TEXT("maximum_cross_slope"));
		}
		auto CheckSlopes = [&](const FMeshDescription& Description,const FString& CellId,const FString& Kind)
		{
			if (PolicyHash.IsEmpty()) return true;
			TArray<FProjectWorldSurfaceTriangle> Native; const auto Positions=FStaticMeshConstAttributes(Description).GetVertexPositions();
			for (const auto Id : Description.Triangles().GetElementIDs())
			{
				FProjectWorldSurfaceTriangle T;
				for (int32 I=0; I<3; ++I) T.Points[I]=FVector(Positions[Description.GetVertexInstanceVertex(Description.GetTriangleVertexInstance(Id,I))]);
				Native.Add(T);
			}
			const auto Axis=Road->GeometryPoints.Last()-Road->GeometryPoints[0]; ProjectWorldConstructedMesh::FSlopeMetrics Metrics;
			const bool Accepted=ProjectWorldConstructedMesh::ValidateSlopes(Native,FVector2D(Axis.X,-Axis.Y),MaximumGrade,MaximumCross,Metrics,Error);
			auto Receipt=MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("cell_id"),CellId); Receipt->SetStringField(TEXT("surface"),Kind);
			Receipt->SetNumberField(TEXT("faces"),Metrics.FaceCount); Receipt->SetNumberField(TEXT("maximum_grade"),Metrics.MaximumGrade);
			Receipt->SetNumberField(TEXT("maximum_cross_slope"),Metrics.MaximumCrossSlope); SlopeReceipts.Add(MakeShared<FJsonValueObject>(Receipt));
			if (!Test.TestTrue(TEXT("Actual stored source geometry meets frozen slope limits."),Accepted))
			{
				Error+=TEXT(" cell=")+CellId+TEXT(" surface=")+Kind;
				if (Native.IsValidIndex(Metrics.FirstFailingFace)) for (const auto& P : Native[Metrics.FirstFailingFace].Points)
					Error+=FString::Printf(TEXT(" [%.17g,%.17g,%.17g]"),P.X,P.Y,P.Z);
				if (Native.IsValidIndex(Metrics.FirstFailingFace)) for (int32 J=0; J<Native.Num(); ++J)
				{
					if (J==Metrics.FirstFailingFace) continue;
					int32 Shared=0;
					for (const auto& P : Native[J].Points) for (const auto& Q : Native[Metrics.FirstFailingFace].Points) if (P==Q) ++Shared;
					if (Shared<2) continue;
					Error+=FString::Printf(TEXT(" adjacent_face=%d"),J);
					for (const auto& P : Native[J].Points) Error+=FString::Printf(TEXT(" [%.17g,%.17g,%.17g]"),P.X,P.Y,P.Z);
				}
				Test.AddError(Error); return false;
			}
			return true;
		};
		TArray<FProjectWorldStoredSurface> NativeBodies;
		if (!BodyField.IsEmpty() && !Test.TestTrue(TEXT("Only one complete physical structural surface is supported."),CompleteStructure)) return false;
		TArray<TSharedPtr<FJsonValue>> Vertices, Faces, BaseVertices, BaseFaces; int32 Built=0;
		for (const auto& C : B.Cells)
		{
			if (C.OwnedFeatureIds.IsEmpty()) continue;
			FMeshDescription Mesh; FVector Origin;
			if (!Test.TestTrue(TEXT("Actual native Road geometry consumes the explicit structural endpoint model."),
				ProjectWorldRoadRealization::BuildDescription(B, C, L, Mesh, Origin, Error))) { Test.AddError(Error); return false; }
			CollectMesh(B, Mesh, Origin, Vertices, Faces);
			if (!CheckSlopes(Mesh,C.CellId,TEXT("road"))) return false;
			if (!BodyField.IsEmpty())
			{
				FMeshDescription Body; FVector BodyOrigin; TArray<int32> Sources;
				if (!Test.TestTrue(TEXT("The complete physical surface consumes canonical XYZ without a presentation lift."),
					ProjectWorldConstructedMesh::Build(B,C,BodyField,.001,Body,BodyOrigin,Error,RolePolicy.Hash.IsEmpty() ? nullptr : &Sources)))
				{ Test.AddError(Error); return false; }
				if (!CheckSlopes(Body,C.CellId,TEXT("body"))) return false;
				if (!RolePolicy.Hash.IsEmpty())
				{
					const auto Axis=Road->GeometryPoints.Last()-Road->GeometryPoints[0]; TArray<TSharedPtr<FJsonValue>> Rows;
					if (!Test.TestTrue(TEXT("Every actual stored body face meets its original source-role limits."),
						ProjectWorldConstructedRoleQuality::Validate(RolePolicy,Body,Sources,FVector2D(Axis.X,-Axis.Y),Rows,Error))) { Test.AddError(Error); return false; }
					auto Receipt=MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("cell_id"),C.CellId);
					Receipt->SetArrayField(TEXT("roles"),Rows); RoleReceipts.Add(MakeShared<FJsonValueObject>(Receipt));
				}
				CollectMesh(B,Body,BodyOrigin,BodyVertices,BodyFaces);
				NativeBodies.Add(StoredMesh(Body,BodyOrigin));
				auto Unit=MakeShared<FJsonObject>(); Unit->SetStringField(TEXT("cell_id"),C.CellId);
				Unit->SetArrayField(TEXT("origin_cm"),{MakeShared<FJsonValueNumber>(BodyOrigin.X),
					MakeShared<FJsonValueNumber>(BodyOrigin.Y),MakeShared<FJsonValueNumber>(BodyOrigin.Z)});
				TArray<TSharedPtr<FJsonValue>> UnitVertices,UnitFaces;
				CollectMesh(B,Body,BodyOrigin,UnitVertices,UnitFaces,true);
				Unit->SetArrayField(TEXT("vertices_cm"),UnitVertices); Unit->SetArrayField(TEXT("triangles"),UnitFaces);
				if (!RolePolicy.Hash.IsEmpty())
				{
					TArray<TSharedPtr<FJsonValue>> Ordinals;
					for (const int32 Source : Sources) Ordinals.Add(MakeShared<FJsonValueNumber>(Source));
					Unit->SetArrayField(TEXT("source_triangle_ordinals"),Ordinals);
				}
				NativeUnits.Add(MakeShared<FJsonValueObject>(Unit));
			}
			if (bGangway)
			{
				const auto Profile = Road->RoadElevation; Road->RoadElevation.Reset();
				FMeshDescription Control; FVector ControlOrigin;
				const bool bControl = ProjectWorldRoadRealization::BuildDescription(B, C, L, Control, ControlOrigin, Error);
				Road->RoadElevation = Profile;
				if (!Test.TestTrue(TEXT("The unchanged native planform has an unprofiled control."), bControl))
				{ Test.AddError(Error); return false; }
				CollectMesh(B, Control, ControlOrigin, BaseVertices, BaseFaces);
			}
			++Built;
		}
		if (!Test.TestTrue(TEXT("The exact constructed candidate produces a nonempty native mesh."), !Faces.IsEmpty())) return false;
		if (!RolePolicy.Hash.IsEmpty())
		{
			TSet<FString> Measured;
			for (const auto& CellReceipt : RoleReceipts) for (const auto& Row : CellReceipt->AsObject()->GetArrayField(TEXT("roles")))
				if (Row->AsObject()->GetNumberField(TEXT("face_count"))>0) Measured.Add(Row->AsObject()->GetStringField(TEXT("role")));
			for (const auto& Region : RolePolicy.Regions)
				if (!Test.TestTrue(TEXT("Each declared body region has actual stored output measured across the selected cells."),Measured.Contains(Region.Role))) return false;
		}
		if (!NativeBodies.IsEmpty())
		{
			TArray<FProjectWorldSurfaceTriangle> AllBody;
			auto Append = [&](const TArray<FProjectWorldStoredSurface>& Inputs,TArray<FProjectWorldSurfaceTriangle>& Out)
			{
				for (const auto& Native : Inputs)
				{
					TArray<FProjectWorldSurfaceTriangle> Converted;
					if (!ProjectWorldStoredSurface::Reframe(Native,NativeBodies[0].Origin,Converted,Error)) return false;
					Out.Append(Converted);
				}
				return true;
			};
			if (!Test.TestTrue(TEXT("All cells preserve actual native positions in the assembly overlay."),Append(NativeBodies,AllBody)) ||
				!Test.TestTrue(TEXT("The whole physical body has no overlapping cells or unacceptable physical joins."),
					ProjectWorldConstructedMesh::ValidateStored(AllBody,.1,Error))) { Test.AddError(Error); return false; }
		}
		auto Result=MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("scope"), TEXT("Typed source-mesh diagnostic only; no production model admission, saved or cooked collision, incident join or geographic acceptance"));
		Result->SetStringField(TEXT("physical_owner"),TEXT("complete_structural_surface_diagnostic"));
		Result->SetArrayField(TEXT("native_physical_units"),NativeUnits);
		Result->SetArrayField(TEXT("engine_georeference_origin_m"),{MakeShared<FJsonValueNumber>(B.EngineGeoreferenceOriginMeters.X),
			MakeShared<FJsonValueNumber>(B.EngineGeoreferenceOriginMeters.Y)});
		Result->SetStringField(TEXT("canonical_crs"),B.CanonicalCrs); Result->SetStringField(TEXT("vertical_datum"),B.VerticalDatum);
		Result->SetNumberField(TEXT("presentation_lift_m"),0);
		TArray<TSharedPtr<FJsonValue>> Route,SourceFaces;
		for (const auto& P : Road->GeometryPoints) Route.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)}));
		for (const auto& T : BodyField)
		{
			TArray<TSharedPtr<FJsonValue>> Corners;
			for (const auto& P : T.Points) Corners.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
				MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}));
			SourceFaces.Add(MakeShared<FJsonValueArray>(Corners));
		}
		Result->SetArrayField(TEXT("source_route"),Route); Result->SetArrayField(TEXT("source_model_triangles"),SourceFaces);
		Result->SetStringField(TEXT("surface_sha256"), SurfaceHash); Result->SetNumberField(TEXT("built_cells"), Built);
		Result->SetArrayField(TEXT("vertices"), Vertices); Result->SetArrayField(TEXT("triangles"), Faces);
		Result->SetNumberField(TEXT("diagnostic_height_origin_m"),B.HeightOriginMeters);
		if (!RolePolicy.Hash.IsEmpty())
		{
			Result->SetStringField(TEXT("body_role_quality_sha256"),RolePolicy.Hash); Result->SetArrayField(TEXT("stored_role_quality_receipts"),RoleReceipts);
			TArray<TSharedPtr<FJsonValue>> Connector;
			for (int32 I=0; I<RolePolicy.RegionBySource.Num(); ++I)
				if (RolePolicy.Regions[RolePolicy.RegionBySource[I]].Role==TEXT("gangway")) Connector.Add(MakeShared<FJsonValueNumber>(I));
			Result->SetArrayField(TEXT("connector_source_triangle_ordinals"),Connector);
		}
		if (!PolicyHash.IsEmpty())
		{ Result->SetStringField(TEXT("quality_policy_sha256"),PolicyHash); Result->SetArrayField(TEXT("stored_slope_receipts"),SlopeReceipts); }
		if (!AssemblyHash.IsEmpty())
		{
			Result->SetStringField(TEXT("assembly_sha256"),AssemblyHash);
			Result->SetArrayField(TEXT("body_vertices"),BodyVertices); Result->SetArrayField(TEXT("body_triangles"),BodyFaces);
		}
		if (bGangway)
		{
			Result->SetArrayField(TEXT("unprofiled_vertices"), BaseVertices);
			Result->SetArrayField(TEXT("unprofiled_triangles"), BaseFaces);
		}
		FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text));
		Test.AddInfo(FString::Printf(TEXT("ConstructedRoadProof cells=%d vertices=%d triangles=%d model=%s"), Built, Vertices.Num(), Faces.Num(), *SurfaceHash));
		return Test.TestTrue(TEXT("Disposable native output is written to its declared path."), FFileHelper::SaveStringToFile(Text, *OutputPath));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldGangwaySourceBindingTest,
	"Project.World.Realization.Road.GangwaySourceBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldGangwaySourceBindingTest::RunTest(const FString&)
{
	using namespace ProjectWorldRoadElevationTests;
	auto B = Bundle(); B.CanonicalCrs = TEXT("EPSG:32618"); B.VerticalDatum = TEXT("EPSG:3855");
	const auto& Road = B.Features[TEXT("fixture:qualified-bridge")];
	auto Surface = MakeShared<FJsonObject>();
	Surface->SetStringField(TEXT("source_feature_id"), Road.FeatureId);
	Surface->SetStringField(TEXT("canonical_crs"), B.CanonicalCrs);
	Surface->SetStringField(TEXT("vertical_datum"), B.VerticalDatum);
	TArray<TSharedPtr<FJsonValue>> Points;
	for (const auto& P : Road.GeometryPoints) Points.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
		MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y)}));
	Surface->SetArrayField(TEXT("source_points"), Points);
	TestTrue(TEXT("The exact source binds."), GangwaySourceMatches(*Surface, B, Road));
	Surface->SetStringField(TEXT("source_feature_id"), TEXT("fixture:other-road"));
	TestFalse(TEXT("Another Road ID cannot bind."), GangwaySourceMatches(*Surface, B, Road));
	Surface->SetStringField(TEXT("source_feature_id"), Road.FeatureId);
	Swap(Points[0], Points[1]); Surface->SetArrayField(TEXT("source_points"), Points);
	TestFalse(TEXT("A reversed same-length axis cannot bind."), GangwaySourceMatches(*Surface, B, Road));
	Swap(Points[0], Points[1]); Surface->SetArrayField(TEXT("source_points"), Points);
	Surface->SetStringField(TEXT("canonical_crs"), TEXT("EPSG:32638"));
	TestFalse(TEXT("A different horizontal frame cannot bind."), GangwaySourceMatches(*Surface, B, Road));
	Surface->SetStringField(TEXT("canonical_crs"), B.CanonicalCrs);
	Surface->SetStringField(TEXT("vertical_datum"), TEXT("EPSG:5703"));
	TestFalse(TEXT("An unconverted vertical frame cannot bind."), GangwaySourceMatches(*Surface, B, Road));
	return !HasAnyErrors();
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
	FProjectWorldCanonicalBundle B;
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
		ProjectWorldRoadElevation::Append(B, Road, Road.GeometryPoints[0], Road.GeometryPoints[1],
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
