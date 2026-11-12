// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldIdentifiedBasinParsing.h"
#include "ProjectWorldCanonicalFiniteJson.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldIdentifiedBasinParsing
{
	namespace
	{
		using FObject=TSharedPtr<FJsonObject>;
		using FValues=TArray<TSharedPtr<FJsonValue>>;
		bool String(const FObject& O,const TCHAR* Key,FString& Out)
		{ const auto V=O ? O->TryGetField(Key) : nullptr; return V && V->Type==EJson::String && V->TryGetString(Out) && !Out.IsEmpty(); }
		bool Is(const FObject& O,const TCHAR* Key,const FString& Expected)
		{ FString Value; return String(O,Key,Value) && Value==Expected; }
		bool Object(const FObject& O,const TCHAR* Key,FObject& Out)
		{ const FObject* V=nullptr; if (!O || !O->TryGetObjectField(Key,V) || !V || !*V) return false; Out=*V; return true; }
		bool Array(const FObject& O,const TCHAR* Key,const FValues*& Out)
		{ return O && O->TryGetArrayField(Key,Out) && Out; }
		bool Number(const TSharedPtr<FJsonValue>& V,double& Out)
		{ return V && V->Type==EJson::Number && V->TryGetNumber(Out) && FMath::IsFinite(Out); }
		bool Number(const FObject& O,const TCHAR* Key,double& Out)
		{ return O && Number(O->TryGetField(Key),Out); }
		bool Integer(const FObject& O,const TCHAR* Key,double Min,double Max,int32& Out)
		{ double V; if (!Number(O,Key,V) || V<Min || V>Max || V!=FMath::FloorToDouble(V)) return false; Out=static_cast<int32>(V); return true; }
		bool Hash(const FString& V)
		{
			if (V.Len()!=64) return false;
			for (TCHAR C : V) if (!((C>='0' && C<='9') || (C>='a' && C<='f'))) return false;
			return true;
		}
		bool Hash(const FObject& O,const TCHAR* Key,FString& Out)
		{ return String(O,Key,Out) && Hash(Out); }
		bool Frame(const FObject& O,FString& Crs)
		{
			if (!String(O,TEXT("canonical_crs"),Crs) || !Crs.StartsWith(TEXT("EPSG:")) || !Is(O,TEXT("vertical_datum"),TEXT("EPSG:3855"))) return false;
			const FString Suffix=Crs.Mid(5);
			if (Suffix.IsEmpty()) return false;
			for (TCHAR C : Suffix) if (C<'0' || C>'9') return false;
			return true;
		}
		bool Descriptor(const FObject& O,bool Input,FProjectWorldCanonicalBasinArtifact& Out)
		{
			double Bytes;
			if (!O || O->Values.Num()!=3 || !String(O,TEXT("path"),Out.Path) || !Hash(O,TEXT("sha256"),Out.Hash) ||
				!Number(O,TEXT("byte_size"),Bytes) || Bytes<1 || Bytes>=static_cast<double>(MAX_int64) || Bytes!=FMath::FloorToDouble(Bytes)) return false;
			if (Input ? (Bytes>16000000 || Out.Path!=TEXT("refinement/inputs/")+Out.Hash+TEXT(".json")) :
				(!Out.Path.StartsWith(TEXT("canonical/refined_terrain/")) || !Out.Path.EndsWith(TEXT(".json")) ||
				 Out.Path.Mid(FString(TEXT("canonical/refined_terrain/")).Len()).Contains(TEXT("/")))) return false;
			Out.ByteSize=static_cast<int64>(Bytes); return true;
		}
		bool Parameters(const FObject& O,TMap<FString,double>& Out)
		{
			if (!O || O->Values.Num()!=8) return false;
			for (const TCHAR* Key : {TEXT("level_m"),TEXT("depth_m"),TEXT("rim_freeboard_m"),TEXT("inner_bank_width_m"),
				TEXT("rim_outset_m"),TEXT("transition_extent_m"),TEXT("maximum_gradient"),TEXT("minimum_spill_margin_m")})
			{
				double V; if (!Number(O,Key,V) || (FString(Key)!=TEXT("level_m") && V<=0)) return false;
				Out.Add(Key,V);
			}
			return true;
		}
		bool Evidence(const FObject& O,const FString& Crs)
		{
			FString Value;
			return O && O->Values.Num()==6 && Is(O,TEXT("basis"),TEXT("raster_ground_model")) &&
				Is(O,TEXT("canonical_crs"),Crs) && Is(O,TEXT("vertical_datum"),TEXT("EPSG:3855")) &&
				Hash(O,TEXT("grid_content_sha256"),Value) && Hash(O,TEXT("source_sha256"),Value) && Hash(O,TEXT("processing_sha256"),Value);
		}
		bool Ring(const TSharedPtr<FJsonValue>& Value)
		{
			const FValues* Points=nullptr; FVector2D First,Last;
			if (!Value || !Value->TryGetArray(Points) || !Points || Points->Num()<4 || Points->Num()>512) return false;
			for (int32 I=0; I<Points->Num(); ++I)
			{
				const FValues* Row=nullptr; double X,Y;
				if (!(*Points)[I]->TryGetArray(Row) || !Row || Row->Num()!=2 || !Number((*Row)[0],X) || !Number((*Row)[1],Y)) return false;
				Last=FVector2D(X,Y); if (I==0) First=Last;
			}
			return First==Last;
		}
		bool Model(const FObject& O,FRetainedInput& Out)
		{
			int32 Version; FString Digest; FObject Contours,Support,Role,Params; const FValues *Routes=nullptr,*References=nullptr;
			if (!O || O->Values.Num()!=11 || !Integer(O,TEXT("version"),1,1,Version) || !Frame(O,Out.CanonicalCrs) ||
				!Is(O,TEXT("basis"),TEXT("identified_static_basin")) || !Hash(O,TEXT("source_geometry_sha256"),Digest) ||
				!Is(O,TEXT("representation"),TEXT("strict_binary64_source_edge_subdivision_v1")) ||
				!Is(O,TEXT("role"),TEXT("inferred_constructed_basin")) || !Object(O,TEXT("contours"),Contours) || Contours->Values.Num()!=3 ||
				!Object(O,TEXT("parameters"),Params) || !Parameters(Params,Out.Parameters) ||
				!Object(O,TEXT("support"),Support) || Support->Values.Num()!=4 ||
				!Is(Support,TEXT("mode"),TEXT("inferred_solid_plaza")) || !Is(Support,TEXT("unmapped_under_routes"),TEXT("unknown")) ||
				!Is(Support,TEXT("basis"),TEXT("explicit_model_assumption_not_observed_ground")) ||
				!Array(Support,TEXT("known_canonical_under_routes"),Routes) || !Routes->IsEmpty() ||
				!Object(O,TEXT("role_evidence"),Role) || Role->Values.Num()!=3 ||
				!Is(Role,TEXT("basis"),TEXT("primary_project_association_not_survey")) || !Hash(Role,TEXT("source_query_policy_sha256"),Digest) ||
				!Array(Role,TEXT("references"),References) || References->IsEmpty() || References->Num()>16) return false;
			TSet<FString> Unique;
			for (const auto& Value : *References)
			{ FString Text; if (Value->Type!=EJson::String || !Value->TryGetString(Text) || !Text.StartsWith(TEXT("https://")) || Unique.Contains(Text)) return false; Unique.Add(Text); }
			for (const TCHAR* Key : {TEXT("core"),TEXT("rim"),TEXT("influence")}) if (!Ring(Contours->TryGetField(Key))) return false;
			return true;
		}
		bool Grid(const FObject& O,const FString& Crs)
		{
			int32 Version; FObject FrameObject; double Nodata; const FValues *Size=nullptr,*Origin=nullptr,*Spacing=nullptr,*Rows=nullptr;
			if (!O || O->Values.Num()!=10 || !Is(O,TEXT("$schema"),TEXT("https://alis.world/schemas/world-source/ground-grid-v1.json")) ||
				!Integer(O,TEXT("schema_version"),1,1,Version) || !Is(O,TEXT("basis"),TEXT("raster_ground_model")) ||
				!Is(O,TEXT("sample_rule"),TEXT("grid_nodes")) || !Object(O,TEXT("frame"),FrameObject) || FrameObject->Values.Num()!=3 ||
				!Is(FrameObject,TEXT("horizontal"),Crs) || !Is(FrameObject,TEXT("vertical"),TEXT("EPSG:3855")) || !Is(FrameObject,TEXT("unit"),TEXT("m")) ||
				!Number(O,TEXT("nodata"),Nodata) || !Array(O,TEXT("size"),Size) || Size->Num()!=2 ||
				!Array(O,TEXT("origin"),Origin) || Origin->Num()!=2 || !Array(O,TEXT("spacing"),Spacing) || Spacing->Num()!=2 ||
				!Array(O,TEXT("samples"),Rows)) return false;
			double Dimensions[2],Value;
			for (int32 I=0; I<2; ++I)
				if (!Number((*Size)[I],Dimensions[I]) || Dimensions[I]<2 || Dimensions[I]>4096 || Dimensions[I]!=FMath::FloorToDouble(Dimensions[I]) ||
					!Number((*Origin)[I],Value) || !Number((*Spacing)[I],Value) || Value==0) return false;
			if (Dimensions[0]*Dimensions[1]>4096 || Rows->Num()!=Dimensions[1]) return false;
			for (const auto& RowValue : *Rows)
			{
				const FValues* Row=nullptr;
				if (!RowValue->TryGetArray(Row) || !Row || Row->Num()!=Dimensions[0]) return false;
				for (const auto& Item : *Row) if (!Number(Item,Value) || Value==Nodata) return false;
			}
			return true;
		}
		bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
		{
			if (!A || !B || A->Type!=B->Type) return false;
			if (A->Type==EJson::Object)
			{
				const auto X=A->AsObject(),Y=B->AsObject();
				if (!X || !Y || X->Values.Num()!=Y->Values.Num()) return false;
				for (const auto& Pair : X->Values) if (!Equal(Pair.Value,Y->TryGetField(Pair.Key))) return false;
				return true;
			}
			if (A->Type==EJson::Array)
			{
				const auto& X=A->AsArray(); const auto& Y=B->AsArray();
				if (X.Num()!=Y.Num()) return false;
				for (int32 I=0; I<X.Num(); ++I) if (!Equal(X[I],Y[I])) return false;
				return true;
			}
			if (A->Type==EJson::Number) return A->AsNumber()==B->AsNumber();
			if (A->Type==EJson::String) return A->AsString()==B->AsString();
			if (A->Type==EJson::Boolean) return A->AsBool()==B->AsBool();
			return A->Type==EJson::Null;
		}
	}

	bool ReadFunction(const FObject& O,FProjectWorldCanonicalWaterSurface& Surface)
	{
		FProjectWorldCanonicalBasinAdmission Candidate; FObject Input,Support; int32 Version; double Level,Spill;
		if (!O || O->Values.Num()!=13 || !ProjectWorldCanonicalJson::FiniteDocument(O) ||
			!Is(O,TEXT("function_id"),TEXT("identified_static_lined_basin")) || !Integer(O,TEXT("function_version"),1,1,Version) ||
			!Is(O,TEXT("vertical_basis"),TEXT("identified_static_basin_pwa")) || !Is(O,TEXT("accuracy"),TEXT("identified_inferred_static_model_not_surveyed")) ||
			!Frame(O,Candidate.CanonicalCrs) || !Number(O,TEXT("level_m"),Level) || !Number(O,TEXT("spill_level_m"),Spill) || Spill<=Level ||
			!Hash(O,TEXT("source_feature_sha256"),Candidate.SourceFeatureHash) || !Hash(O,TEXT("model_sha256"),Candidate.ModelHash) ||
			!Hash(O,TEXT("boundary_encoding_sha256"),Candidate.BoundaryEncodingHash) || !Object(O,TEXT("model_input"),Input) || !Descriptor(Input,true,Candidate.Input) ||
			!Object(O,TEXT("terrain_support"),Support) || Support->Values.IsEmpty() || Support->Values.Num()>16) return false;
		for (const auto& Pair : Support->Values)
		{
			const FObject* Value=nullptr; FProjectWorldCanonicalBasinArtifact Artifact;
			if (Pair.Key.IsEmpty() || !Pair.Value->TryGetObject(Value) || !Value || !Descriptor(*Value,false,Artifact)) return false;
			Candidate.TerrainSupport.Add(FString(*Pair.Key),MoveTemp(Artifact));
		}
		FString Text;
		if (!FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text))) return false;
		FTCHARToUTF8 Utf8(*Text); TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
		if (!FProjectSha256::HashBuffer(Bytes,Candidate.AdmissionHash)) return false;
		Surface.LevelMeters=Level; Surface.ContainmentSpillMeters=Spill; Surface.BasinAdmission=MoveTemp(Candidate);
		return true;
	}

	bool ReadInput(const FObject& Operation,const FObject& Document,const FProjectWorldCanonicalBasinArtifact& Artifact,FRetainedInputs& Inputs)
	{
		FRetainedInput Candidate; Candidate.Artifact=Artifact; Candidate.Document=Document;
		FObject Original,ModelObject,GridObject,EvidenceObject,Selected,Base,Attributes,Function; const FValues* Ids=nullptr;
		FString Id,OriginalId,OriginalOwner,Construction; int32 Version;
		if (!Operation || Operation->Values.Num()!=5 || !Hash(Operation,TEXT("construction_identity"),Construction) ||
			!Array(Operation,TEXT("feature_ids"),Ids) || Ids->Num()!=1 || (*Ids)[0]->Type!=EJson::String || !(*Ids)[0]->TryGetString(Id) || Id.IsEmpty() || Inputs.Contains(Id) ||
			!Object(Operation,TEXT("base_features"),Base) || Base->Values.Num()!=1 || !Hash(Base,*Id,Candidate.SourceFeatureHash) ||
			!Document || Document->Values.Num()!=9 || !ProjectWorldCanonicalJson::FiniteDocument(Document) ||
			!Is(Document,TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/identified-basin-input-v1.json")) ||
			!Integer(Document,TEXT("schema_version"),1,1,Version) || !Is(Document,TEXT("reference_basis"),TEXT("identified_retained_grid_node_pwa")) ||
			!Integer(Document,TEXT("maximum_triangles"),2,4096,Candidate.MaximumTriangles) ||
			!Object(Document,TEXT("original_feature"),Original) || !Is(Original,TEXT("feature_id"),Id) || !Is(Original,TEXT("feature_class"),TEXT("water")) ||
			!String(Original,TEXT("owner_cell_id"),OriginalOwner) || !Object(Original,TEXT("attributes"),Attributes) ||
			!Object(Attributes,TEXT("surface_function"),Function) || Is(Function,TEXT("function_id"),TEXT("identified_static_lined_basin")) ||
			!Object(Document,TEXT("model"),ModelObject) || !Model(ModelObject,Candidate) ||
			!Object(Document,TEXT("grid"),GridObject) || !Grid(GridObject,Candidate.CanonicalCrs) ||
			!Object(Document,TEXT("evidence"),EvidenceObject) || !Evidence(EvidenceObject,Candidate.CanonicalCrs) ||
			!Object(Document,TEXT("selected"),Selected) || Selected->Values.IsEmpty() || Selected->Values.Num()>16) return false;
		for (const auto& Pair : Selected->Values)
		{
			const FValues* Quads=nullptr; TSet<FIntPoint> Coordinates;
			if (Pair.Key.IsEmpty() || !Pair.Value->TryGetArray(Quads) || !Quads || Quads->IsEmpty() || Quads->Num()>16) return false;
			for (const auto& Quad : *Quads)
			{
				const FValues* Row=nullptr; double XY[2];
				if (!Quad->TryGetArray(Row) || !Row || Row->Num()!=2) return false;
				for (int32 I=0; I<2; ++I) if (!Number((*Row)[I],XY[I]) || XY[I]<0 || XY[I]>MAX_int32 || XY[I]!=FMath::FloorToDouble(XY[I])) return false;
				const FIntPoint Coordinate(static_cast<int32>(XY[0]),static_cast<int32>(XY[1]));
				if (Coordinates.Contains(Coordinate)) return false;
				Coordinates.Add(Coordinate);
			}
			Candidate.Selected.Add(FString(*Pair.Key),MoveTemp(Coordinates));
		}
		if (!Candidate.Selected.Contains(OriginalOwner)) return false;
		Inputs.Add(Id,MoveTemp(Candidate)); return true;
	}

	bool ReadTerrain(const FObject& Document,FProjectWorldCanonicalTerrain& Terrain)
	{
		FProjectWorldCanonicalBasinConstruction Candidate; FObject Consumed,Policy,Metrics,Ground; FString Digest; int32 Maximum=0;
		if (!Document || Document->Values.Num()!=11 || !Is(Document,TEXT("construction"),TEXT("identified_static_basin_pwa_v1")) ||
			!ProjectWorldCanonicalJson::FiniteDocument(Document) || !Object(Document,TEXT("consumed"),Consumed) || Consumed->Values.Num()!=12 ||
			!String(Consumed,TEXT("canonical_crs"),Candidate.CanonicalCrs) || !Hash(Consumed,TEXT("water_sha256"),Candidate.SourceFeatureHash) ||
			!Hash(Consumed,TEXT("model_sha256"),Candidate.ModelHash) || !Hash(Consumed,TEXT("model_input_sha256"),Candidate.InputHash) ||
			!Integer(Consumed,TEXT("maximum_triangles"),2,4096,Maximum) || !Object(Consumed,TEXT("ground_evidence"),Ground) || !Evidence(Ground,Candidate.CanonicalCrs) ||
			!Object(Document,TEXT("policy"),Policy) || !Parameters(Policy,Candidate.Parameters) ||
			!Object(Document,TEXT("metrics"),Metrics) || Metrics->Values.Num()!=4) return false;
		for (const TCHAR* Key : {TEXT("implementation_sha256"),TEXT("geometry_execution_identity"),TEXT("rounding_verifier_identity"),
			TEXT("affine_verifier_identity"),TEXT("domain_sha256"),TEXT("boundary_trace_sha256")}) if (!Hash(Consumed,Key,Digest)) return false;
		for (const TCHAR* Key : {TEXT("maximum_gradient"),TEXT("modeled_spill_margin_m"),TEXT("modeled_wet_volume_m3"),TEXT("maximum_outside_reference_error_upper_m")})
		{
			double Value; if (!Number(Metrics,Key,Value) || Value<0 ||
				((FString(Key)==TEXT("modeled_spill_margin_m") || FString(Key)==TEXT("modeled_wet_volume_m3")) && Value==0)) return false;
			if (FString(Key)==TEXT("maximum_gradient") && Value>Candidate.Parameters[TEXT("maximum_gradient")]) return false;
			if (FString(Key)==TEXT("modeled_spill_margin_m") && Value<Candidate.Parameters[TEXT("minimum_spill_margin_m")]) return false;
			if (FString(Key)==TEXT("modeled_spill_margin_m")) Candidate.SpillMarginMeters=Value;
		}
		for (const auto& Pair : Ground->Values) Candidate.GroundEvidence.Add(FString(*Pair.Key),Pair.Value->AsString());
		Candidate.MaximumTriangles=Maximum; Terrain.BasinConstruction=MoveTemp(Candidate); return true;
	}

	bool ValidateSources(const FObject& Document,const FRetainedInputs& Inputs,FProjectWorldCanonicalValidation& Validation)
	{
		const FValues* Features=nullptr;
		if (!Array(Document,TEXT("features"),Features)) return false;
		for (const auto& Value : *Features)
		{
			const FObject* Feature=nullptr; FString Id;
			if (!Value->TryGetObject(Feature) || !Feature || !String(*Feature,TEXT("feature_id"),Id)) return false;
			const auto* Input=Inputs.Find(Id); if (!Input) continue;
			const auto Original=Input->Document->GetObjectField(TEXT("original_feature"));
			FObject Attributes,OriginalAttributes;
			bool Same=(*Feature)->Values.Num()==Original->Values.Num() && Object(*Feature,TEXT("attributes"),Attributes) &&
				Object(Original,TEXT("attributes"),OriginalAttributes) && Attributes->Values.Num()==OriginalAttributes->Values.Num();
			for (const auto& Pair : Original->Values)
				if (Pair.Key!=TEXT("geometry") && Pair.Key!=TEXT("attributes")) Same &= Equal(Pair.Value,(*Feature)->TryGetField(Pair.Key));
			if (Same) for (const auto& Pair : OriginalAttributes->Values)
				if (Pair.Key!=TEXT("surface_function")) Same &= Equal(Pair.Value,Attributes->TryGetField(Pair.Key));
			if (!Same)
			{
				Validation.ErrorCode=TEXT("identified-basin-source"); Validation.Message=TEXT("Modeled Water changed its retained original source association."); Validation.Detail=Id;
				return false;
			}
		}
		return true;
	}

	bool ValidateBundle(const FProjectWorldCanonicalBundle& Bundle,const FRetainedInputs& Inputs,
		const TMap<FString,FProjectWorldCanonicalBasinArtifact>& LoadedTerrain,
		TFunctionRef<bool(const FString&,const FString&,int64,bool)> Verified,FProjectWorldCanonicalValidation& Validation)
	{
		auto Reject=[&](const FString& Detail)
		{ Validation.ErrorCode=TEXT("identified-basin-input"); Validation.Message=TEXT("Joint basin input, source, frame or Terrain support is inconsistent."); Validation.Detail=Detail; return false; };
		TSet<FString> BoundFeatures,BoundCells;
		for (const auto& Pair : Bundle.Features)
		{
			const auto& Feature=Pair.Value; const auto& Water=Feature.WaterSurface;
			if (!Water.BasinAdmission.IsSet()) continue;
			const auto& Basin=*Water.BasinAdmission; const auto* Input=Inputs.Find(Pair.Key);
			if (!Input || Input->Artifact.Path!=Basin.Input.Path || Input->Artifact.Hash!=Basin.Input.Hash || Input->Artifact.ByteSize!=Basin.Input.ByteSize ||
				!Verified(Basin.Input.Path,Basin.Input.Hash,Basin.Input.ByteSize,false) || Basin.SourceFeatureHash!=Input->SourceFeatureHash ||
				Basin.CanonicalCrs!=Bundle.CanonicalCrs || Basin.CanonicalCrs!=Input->CanonicalCrs || Bundle.VerticalDatum!=TEXT("EPSG:3855") ||
				Water.LevelMeters!=Input->Parameters.FindChecked(TEXT("level_m")) || !Water.ContainmentSpillMeters.IsSet() ||
				*Water.ContainmentSpillMeters!=Water.LevelMeters+Input->Parameters.FindChecked(TEXT("rim_freeboard_m")) ||
				*Water.ContainmentSpillMeters-Water.LevelMeters<Input->Parameters.FindChecked(TEXT("minimum_spill_margin_m")) ||
				Water.SurfaceGroupMembers.Num()!=1 || Basin.TerrainSupport.Num()!=Input->Selected.Num() ||
				!Is(Input->Document->GetObjectField(TEXT("original_feature")),TEXT("owner_cell_id"),Feature.OwnerCellId)) return Reject(Pair.Key);
			BoundFeatures.Add(Pair.Key);
			for (const auto& Support : Basin.TerrainSupport)
			{
				const auto* Selected=Input->Selected.Find(Support.Key);
				const auto* Actual=LoadedTerrain.Find(Support.Key);
				const auto* Cell=Bundle.Cells.FindByPredicate([&](const auto& Value){return Value.CellId==Support.Key;});
				if (!Selected || !Cell || !Actual || Actual->Path!=Support.Value.Path || Actual->Hash!=Support.Value.Hash || Actual->ByteSize!=Support.Value.ByteSize ||
					BoundCells.Contains(Support.Key) || !Cell->Terrain.BasinConstruction.IsSet() ||
					!Verified(Support.Value.Path,Support.Value.Hash,Support.Value.ByteSize,false) || Cell->Terrain.ArtifactHash!=Support.Value.Hash) return Reject(Support.Key);
				const auto& Construction=*Cell->Terrain.BasinConstruction;
				TMap<FString,FString> GroundEvidence;
				for (const auto& Item : Input->Document->GetObjectField(TEXT("evidence"))->Values) GroundEvidence.Add(FString(*Item.Key),Item.Value->AsString());
				if (Construction.CanonicalCrs!=Bundle.CanonicalCrs || Construction.SourceFeatureHash!=Basin.SourceFeatureHash ||
					Construction.ModelHash!=Basin.ModelHash || Construction.InputHash!=Basin.Input.Hash || Construction.MaximumTriangles!=Input->MaximumTriangles ||
					!Construction.Parameters.OrderIndependentCompareEqual(Input->Parameters) || !Construction.GroundEvidence.OrderIndependentCompareEqual(GroundEvidence) ||
					Construction.SpillMarginMeters!=*Water.ContainmentSpillMeters-Water.LevelMeters || Cell->Terrain.RefinedQuads.Num()!=Selected->Num()) return Reject(Support.Key);
				for (const auto& Quad : Cell->Terrain.RefinedQuads)
					if (!Selected->Contains(Quad.Coordinate) || Quad.Triangles.Num()>Input->MaximumTriangles) return Reject(Support.Key);
				BoundCells.Add(Support.Key);
			}
		}
		if (BoundFeatures.Num()!=Inputs.Num()) return Reject(TEXT("unrealized declared basin input"));
		for (const auto& Cell : Bundle.Cells)
			if (Cell.Terrain.BasinConstruction.IsSet() && !BoundCells.Contains(Cell.CellId)) return Reject(Cell.CellId);
		return true;
	}
}
