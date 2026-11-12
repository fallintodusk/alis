// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedNeighborhoodInput.h"
#include "ProjectWorldConstructedSurface.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedNeighborhood
{
	using ProjectWorldConstructedRuntimeTest::Vector;
	FString UnitKey(const FUnit& Unit)
	{ return FString::Printf(TEXT("%d:%s%d:%s"),Unit.GroupId.Len(),*Unit.GroupId,Unit.CellId.Len(),*Unit.CellId); }
	bool ContentHash(const FUnit& Unit,FString& Out)
	{
		FString Text=UnitKey(Unit);
		auto Point = [&](const FVector& P){ Text+=FString::Printf(TEXT("|%.17g|%.17g|%.17g"),P.X,P.Y,P.Z); };
		Point(Unit.Surface.Origin); Text+=FString::Printf(TEXT("|%d|%d"),Unit.Surface.Triangles.Num(),Unit.Sources.Num());
		for (const auto& Face : Unit.Surface.Triangles) for (const auto& P : Face.Points) Point(P);
		for (const int32 Source : Unit.Sources) Text+=FString::Printf(TEXT("|%d"),Source);
		FTCHARToUTF8 Utf8(*Text); TArray<uint8> Bytes; Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
		return FProjectSha256::HashBuffer(Bytes,Out);
	}
	bool CaptureBuildOutput(const FUnit& Unit,FExpectedOutputs& Out,FString& Error)
	{
		FExpectedUnit Expected; Expected.Faces=Unit.Surface.Triangles.Num(); const FString Key=UnitKey(Unit);
		if (Expected.Faces==0 || Unit.Sources.Num()!=Expected.Faces || Out.Contains(Key) || !ContentHash(Unit,Expected.ContentHash))
		{ Error=TEXT("Authoritative native output cannot supply a complete unique geometry/trace expectation."); return false; }
		Out.Add(Key,MoveTemp(Expected)); return true;
	}
	bool Read(const FString& Path,const FString& Expected,TSharedPtr<FJsonObject>& Out,FString& Error)
	{
		Out.Reset(); FString Actual,Text;
		if (Expected.Len()!=64 || !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) || Actual!=Expected ||
			!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out))
		{ Error=TEXT("The exact diagnostic scene input cannot be authenticated."); return false; }
		return true;
	}
	bool ParseGroups(const FJsonObject& Manifest,const FJsonObject& Surfaces,
		const FProjectWorldCanonicalBundle& Frame,TArray<FGroup>& Out,FString& Error)
	{
		Out.Reset(); Error.Reset(); const TArray<TSharedPtr<FJsonValue>>* Groups=nullptr;
		if (!Manifest.TryGetArrayField(TEXT("required_groups"),Groups) || !Groups || Groups->IsEmpty() || Groups->Num()>5 ||
			Surfaces.Values.IsEmpty() || Surfaces.Values.Num()>8)
		{ Error=TEXT("The scene requires a closed finite owner census."); return false; }
		TArray<FGroup> Candidate; TSet<FString> Ids,BoundSurfaces; int32 Bodies=0;
		for (const auto& Value : *Groups)
		{
			if (!Value.IsValid() || Value->Type!=EJson::Object) { Error=TEXT("Scene owner is not an object."); return false; }
			const auto Object=Value->AsObject(); FGroup Group; double ExpectedFaces;
			const TArray<TSharedPtr<FJsonValue>>* Names=nullptr; const TArray<TSharedPtr<FJsonValue>>* Cells=nullptr;
			if (!Object->TryGetStringField(TEXT("id"),Group.Id) || Group.Id.IsEmpty() || Group.Id.Len()>64 || Ids.Contains(Group.Id) ||
				!Object->TryGetStringField(TEXT("role"),Group.Role) || !Object->TryGetArrayField(TEXT("source_surfaces"),Names) || !Names || Names->IsEmpty() || Names->Num()>8 ||
				!Object->TryGetArrayField(TEXT("required_cells"),Cells) || !Cells || Cells->IsEmpty() || Cells->Num()>2 ||
				!Object->TryGetNumberField(TEXT("source_triangle_count"),ExpectedFaces) || !FMath::IsFinite(ExpectedFaces) ||
				ExpectedFaces<1 || ExpectedFaces>4096 || ExpectedFaces!=FMath::FloorToDouble(ExpectedFaces))
			{ Error=TEXT("Scene owner has an invalid identity, source count or cell census."); return false; }
			if (Group.Role==TEXT("constructed_body")) ++Bodies;
			else if (Group.Role!=TEXT("inferred_ground") && Group.Role!=TEXT("inferred_structural_apron") && Group.Role!=TEXT("inferred_gate_apron"))
			{ Error=TEXT("Scene owner has an unsupported diagnostic role."); return false; }
			for (const auto& Cell : *Cells)
			{
				FString Id;
				if (!Cell.IsValid() || !Cell->TryGetString(Id) || Group.RequiredCells.Contains(Id) ||
					!Frame.Cells.ContainsByPredicate([&](const auto& C){ return C.CellId==Id; }))
				{ Error=TEXT("Scene owner names a repeated or unknown canonical cell."); return false; }
				Group.RequiredCells.Add(Id);
			}
			for (const auto& Name : *Names)
			{
				FString Id; const TArray<TSharedPtr<FJsonValue>>* Faces=nullptr;
				if (!Name.IsValid() || !Name->TryGetString(Id) || BoundSurfaces.Contains(Id) || !Surfaces.TryGetArrayField(Id,Faces) || !Faces || Faces->IsEmpty() || Faces->Num()>4096)
				{ Error=TEXT("Scene owner omits, repeats or invents a source field."); return false; }
				BoundSurfaces.Add(Id);
				for (const auto& Face : *Faces)
				{
					const TArray<TSharedPtr<FJsonValue>>* Points=nullptr; FProjectWorldSurfaceTriangle Triangle;
					if (!Face.IsValid() || !Face->TryGetArray(Points) || Points->Num()!=3)
					{ Error=TEXT("Scene source face is not a finite triangle."); return false; }
					for (int32 I=0; I<3; ++I) if (!Vector((*Points)[I],Triangle.Points[I]))
					{ Error=TEXT("Scene source face has invalid XYZ."); return false; }
					Group.Field.Add(Triangle);
					if (Group.Field.Num()>4096) { Error=TEXT("Scene grouping cannot exceed the existing per-field source limit."); return false; }
				}
			}
			if (Group.Field.Num()!=ExpectedFaces) { Error=TEXT("Scene owner does not consume its exact declared source-face census."); return false; }
			if (!ProjectWorldConstructedSurface::ValidateField(Group.Field,Error)) return false;
			Ids.Add(Group.Id); Candidate.Add(MoveTemp(Group));
		}
		if (Bodies!=1 || BoundSurfaces.Num()!=Surfaces.Values.Num())
		{ Error=TEXT("The scene must retain one body and every source field exactly once."); return false; }
		Out=MoveTemp(Candidate); return true;
	}
	bool Load(const FString& Path,const FString& Expected,FScene& Out,FString& Error)
	{
		Out=FScene(); Error.Reset(); TSharedPtr<FJsonObject> Manifest,Geometry,Body; FScene Candidate;
		if (!Read(Path,Expected,Manifest,Error)) return false;
		FString Model,BasePath,BaseHash,SourcePath,SourceHash,Crs,Datum,BodyPath,BodyHash; bool Diagnostic=false;
		FVector Geo; double Height,Agreement,Norm,Join;
		if (!Manifest->TryGetStringField(TEXT("model"),Model) || Model!=TEXT("grouped_neighborhood_candidate_v1") ||
			!Manifest->TryGetBoolField(TEXT("diagnostic_only"),Diagnostic) || !Diagnostic ||
			!Manifest->TryGetStringField(TEXT("base_result"),BasePath) || !Manifest->TryGetStringField(TEXT("base_result_sha256"),BaseHash) ||
			!Manifest->TryGetStringField(TEXT("source_geometry_path"),SourcePath) || !Manifest->TryGetStringField(TEXT("source_geometry_sha256"),SourceHash) ||
			!Manifest->TryGetStringField(TEXT("body_native_path"),BodyPath) || !Manifest->TryGetStringField(TEXT("body_native_sha256"),BodyHash) ||
			!Manifest->TryGetStringField(TEXT("canonical_crs"),Crs) || Crs.IsEmpty() ||
			!Manifest->TryGetStringField(TEXT("vertical_datum"),Datum) || Datum!=TEXT("EPSG:3855") ||
			!Vector(Manifest->TryGetField(TEXT("engine_georeference_origin_m")),Geo,2) ||
			!Manifest->TryGetNumberField(TEXT("height_origin_m"),Height) || !FMath::IsFinite(Height) ||
			!Manifest->TryGetNumberField(TEXT("maximum_model_agreement_m"),Agreement) || Agreement!=.001 ||
			!Manifest->TryGetNumberField(TEXT("maximum_ordinary_gradient"),Norm) || Norm!=.02 ||
			!Manifest->TryGetNumberField(TEXT("maximum_join_step_m"),Join) || Join!=.001 ||
			!Manifest->TryGetStringField(TEXT("output_path"),Candidate.OutputPath))
		{ Error=TEXT("The grouped diagnostic requires explicit identities, unchanged frame and frozen quality limits."); return false; }
		TSharedPtr<FJsonObject> Base;
		if (!Read(BasePath,BaseHash,Base,Error) || !Read(SourcePath,SourceHash,Geometry,Error) || !Read(BodyPath,BodyHash,Body,Error)) return false;
		FProjectWorldCanonicalValidation Validation;
		if (!FProjectWorldCanonicalLoader::Load(BasePath,Candidate.Frame,Validation)) { Error=Validation.Message; return false; }
		if (Candidate.Frame.CanonicalCrs!=Crs || Candidate.Frame.VerticalDatum!=Datum || Candidate.Frame.HeightOriginMeters!=Height || Candidate.Frame.EngineGeoreferenceOriginMeters!=FVector2D(Geo))
		{ Error=TEXT("The grouped scene cannot replace the authenticated canonical frame."); return false; }
		const TArray<TSharedPtr<FJsonValue>>* Cells=nullptr; TArray<FProjectWorldCanonicalCell> Selected;
		if (!Manifest->TryGetArrayField(TEXT("selected_cells"),Cells) || !Cells || Cells->IsEmpty() || Cells->Num()>2)
		{ Error=TEXT("The grouped scene requires its exact bounded selected cells."); return false; }
		for (const auto& Value : *Cells)
		{
			const TSharedPtr<FJsonObject>* Object=nullptr; FString Id; const TArray<TSharedPtr<FJsonValue>>* Bounds=nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !(*Object)->TryGetStringField(TEXT("cell_id"),Id) ||
				!(*Object)->TryGetArrayField(TEXT("bounds"),Bounds) || !Bounds || Bounds->Num()!=4 || Selected.ContainsByPredicate([&](const auto& C){ return C.CellId==Id; }))
			{ Error=TEXT("The grouped selected-cell census is malformed or repeated."); return false; }
			const auto* Cell=Candidate.Frame.Cells.FindByPredicate([&](const auto& C){ return C.CellId==Id; });
			if (!Cell) { Error=TEXT("The grouped cell is absent from the canonical base."); return false; }
			for (int32 I=0; I<4; ++I) if (!(*Bounds)[I].IsValid() || (*Bounds)[I]->Type!=EJson::Number || (*Bounds)[I]->AsNumber()!=Cell->Bounds[I])
			{ Error=TEXT("The grouped scene cannot change canonical cell bounds."); return false; }
			Selected.Add(*Cell);
		}
		Candidate.Frame.Cells=MoveTemp(Selected); const TSharedPtr<FJsonObject>* Surfaces=nullptr;
		if (!Geometry->TryGetObjectField(TEXT("surfaces"),Surfaces) || !Surfaces) { Error=TEXT("The scene has no declared source surfaces."); return false; }
		if (!ParseGroups(*Manifest,**Surfaces,Candidate.Frame,Candidate.Groups,Error)) return false;
		if (!ProjectWorldConstructedRuntimeTest::Load(Candidate.Body,Error,BodyPath,BodyHash)) return false;
		FString BodyCrs,BodyDatum,RoleHash,AssemblyHash,RequiredRole,RequiredAssembly;
		if (!Body->TryGetStringField(TEXT("canonical_crs"),BodyCrs) || BodyCrs!=Crs || !Body->TryGetStringField(TEXT("vertical_datum"),BodyDatum) || BodyDatum!=Datum ||
			Candidate.Body.Frame.HeightOriginMeters!=Height || Candidate.Body.Frame.EngineGeoreferenceOriginMeters!=FVector2D(Geo) ||
			!Body->TryGetStringField(TEXT("body_role_quality_sha256"),RoleHash) || !Manifest->TryGetStringField(TEXT("body_role_quality_sha256"),RequiredRole) || RoleHash!=RequiredRole ||
			!Body->TryGetStringField(TEXT("assembly_sha256"),AssemblyHash) || !Manifest->TryGetStringField(TEXT("body_assembly_sha256"),RequiredAssembly) || AssemblyHash!=RequiredAssembly)
		{ Error=TEXT("The previously qualified body does not belong to this exact scene/frame/model."); return false; }
		const auto* BodyGroup=Candidate.Groups.FindByPredicate([](const auto& G){ return G.Role==TEXT("constructed_body"); });
		if (BodyGroup->Field.Num()!=Candidate.Body.Field.Num()) { Error=TEXT("The scene body source census changed."); return false; }
		for (int32 I=0; I<BodyGroup->Field.Num(); ++I) for (int32 J=0; J<3; ++J)
			if (BodyGroup->Field[I].Points[J]!=Candidate.Body.Field[I].Points[J]) { Error=TEXT("The scene body source geometry changed."); return false; }
		for (const auto& Value : Body->GetArrayField(TEXT("native_physical_units")))
		{
			FString Id;
			if (!Value->AsObject()->TryGetStringField(TEXT("cell_id"),Id) || Candidate.BodyCells.Contains(Id) || !BodyGroup->RequiredCells.Contains(Id))
			{ Error=TEXT("The body has a repeated or unexpected native cell."); return false; }
			Candidate.BodyCells.Add(Id);
		}
		if (Candidate.BodyCells.Num()!=BodyGroup->RequiredCells.Num()) { Error=TEXT("The body omits required native cells."); return false; }
		if (Manifest->HasField(TEXT("native_partition_mode")) || Manifest->HasField(TEXT("body_role_quality_path")))
		{
			FString Mode,PolicyPath,PolicyAssembly; const TArray<TSharedPtr<FJsonValue>>* Route=nullptr; FVector A,B;
			if (!Manifest->TryGetStringField(TEXT("native_partition_mode"),Mode) || Mode!=TEXT("shared_physical_partition_v1") ||
				!Manifest->TryGetStringField(TEXT("body_role_quality_path"),PolicyPath) || !Read(PolicyPath,RequiredRole,Candidate.BodyQuality,Error))
			{ Error=TEXT("Shared storage requires its explicit mode and exact existing body role policy."); return false; }
			if (!Candidate.BodyQuality->TryGetStringField(TEXT("assembly_sha256"),PolicyAssembly) || PolicyAssembly!=RequiredAssembly ||
				!Body->TryGetArrayField(TEXT("source_route"),Route) || !Route || Route->Num()!=2 || !Vector((*Route)[0],A,2) || !Vector((*Route)[1],B,2) || A==B)
			{ Error=TEXT("Shared storage cannot change the body policy source or declared gangway axis."); return false; }
			Candidate.BodyAxis=FVector2D(B.X-A.X,A.Y-B.Y); Candidate.SharedPartition=true;
		}
		Candidate.Hash=Expected; Candidate.GeometryHash=SourceHash; Out=MoveTemp(Candidate); return true;
	}
	bool ValidateCensus(const FScene& Scene,const TArray<FUnit>& Units,const FExpectedOutputs& Expected,FString& Error)
	{
		Error.Reset(); TSet<FString> Seen; TMap<FString,int32> Counts;
		for (const auto& Unit : Units)
		{
			const auto* Group=Scene.Groups.FindByPredicate([&](const auto& G){ return G.Id==Unit.GroupId; });
			const auto* Cell=Scene.Frame.Cells.FindByPredicate([&](const auto& C){ return C.CellId==Unit.CellId; });
			const FString Key=UnitKey(Unit);
			if (!Group || !Cell || !Group->RequiredCells.Contains(Unit.CellId) || Seen.Contains(Key) || Unit.Surface.Triangles.IsEmpty() ||
				Unit.Surface.Triangles.Num()>5*Group->Field.Num() || Unit.Sources.Num()!=Unit.Surface.Triangles.Num() ||
				Unit.Surface.Origin!=FProjectWorldCanonicalLoader::CanonicalToUnreal(Scene.Frame,FVector(Cell->Bounds.X,Cell->Bounds.W,Scene.Frame.HeightOriginMeters)))
			{ Error=TEXT("Actual grouped output omits or contradicts the required owner/cell/frame census."); return false; }
			for (int32 I=0; I<Unit.Sources.Num(); ++I)
			{
				if (!Group->Field.IsValidIndex(Unit.Sources[I])) { Error=TEXT("Actual source ordinal belongs to another group or is out of range."); return false; }
				const auto& Face=Unit.Surface.Triangles[I]; const FVector Center=(Face.Points[0]+Face.Points[1]+Face.Points[2])/3+Unit.Surface.Origin;
				const auto Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Scene.Frame,Center); double Z;
				if (!ProjectWorldConstructedSurface::Sample(Group->Field,FVector2D(Canonical),Z,Error) || FMath::Abs(Z-Canonical.Z)>.001)
				{ Error=TEXT("Actual grouped output has no matching declared source support."); return false; }
			}
			const auto* Reference=Expected.Find(Key); FString ActualHash;
			if (!Reference || Reference->Faces!=Unit.Surface.Triangles.Num() || !ContentHash(Unit,ActualHash) || ActualHash!=Reference->ContentHash)
			{ Error=TEXT("Actual geometry or provenance differs from the complete authoritative native output."); return false; }
			Seen.Add(Key); ++Counts.FindOrAdd(Unit.GroupId);
		}
		for (const auto& Group : Scene.Groups) if (Counts.FindRef(Group.Id)!=Group.RequiredCells.Num())
		{ Error=TEXT("Actual output omits a required group or canonical cell."); return false; }
		if (Seen.Num()!=Expected.Num()) { Error=TEXT("Actual output does not consume the complete authoritative native census."); return false; }
		return true;
	}
}
#endif
