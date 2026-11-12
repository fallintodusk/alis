// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedContractParsing.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldCanonicalFiniteJson.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldConstructedContractParsing
{
	namespace
	{
		using FObject=TSharedPtr<FJsonObject>;
		using FValues=TArray<TSharedPtr<FJsonValue>>;
		bool Exact(const FObject& Object, std::initializer_list<const TCHAR*> Fields)
		{
			if (!Object || Object->Values.Num()!=static_cast<int32>(Fields.size())) return false;
			for (const TCHAR* Field : Fields) if (!Object->HasField(Field)) return false;
			return true;
		}
		bool String(const FObject& Object,const TCHAR* Field,FString& Out)
		{
			const auto Value=Object ? Object->TryGetField(Field) : nullptr;
			return Value && Value->Type==EJson::String && Value->TryGetString(Out) && !Out.IsEmpty();
		}
		bool Hash(const FString& Value)
		{
			if (Value.Len()!=64) return false;
			for (TCHAR C : Value) if (!((C>='0' && C<='9') || (C>='a' && C<='f'))) return false;
			return true;
		}
		bool HashField(const FObject& Object,const TCHAR* Field,FString& Out)
		{ return String(Object,Field,Out) && Hash(Out); }
		bool Number(const TSharedPtr<FJsonValue>& Value,double& Out)
		{ return Value && Value->Type==EJson::Number && Value->TryGetNumber(Out) && FMath::IsFinite(Out); }
		bool Array(const FObject& Object,const TCHAR* Field,const FValues*& Out,int32 Maximum)
		{ return Object && Object->TryGetArrayField(Field,Out) && Out && !Out->IsEmpty() && Out->Num()<=Maximum; }
		bool Object(const FObject& Parent,const TCHAR* Field,FObject& Out)
		{
			const FObject* Value=nullptr;
			if (!Parent || !Parent->TryGetObjectField(Field,Value) || !Value || !*Value) return false;
			Out=*Value; return true;
		}
		bool ProviderId(const FString& Value)
		{
			FString Kind,Id;
			if (!Value.Split(TEXT("/"),&Kind,&Id) || (Kind!=TEXT("way") && Kind!=TEXT("relation")) ||
				Id.IsEmpty() || Id[0]=='0') return false;
			for (TCHAR C : Id) if (C<'0' || C>'9') return false;
			return true;
		}
		bool Descriptor(const FObject& Value,bool Native,const FString& Context,FProjectWorldCanonicalConstructedSurface& Surface)
		{
			FString Path,Digest; double Bytes=0;
			if (!(Exact(Value,{TEXT("path"),TEXT("sha256"),TEXT("byte_size")}) &&
				HashField(Value,TEXT("sha256"),Digest) && String(Value,TEXT("path"),Path) &&
				Path==TEXT("refinement/inputs/")+Digest+TEXT(".json") &&
				Number(Value->TryGetField(TEXT("byte_size")),Bytes) && Bytes>=1 && Bytes<=16000000 && Bytes==FMath::FloorToDouble(Bytes))) return false;
			for (const auto& Existing : Surface.SourceSnapshots) if (Existing.Path==Path)
			{
				if (Existing.Hash!=Digest || Existing.ByteSize!=static_cast<int64>(Bytes) || Existing.bNative!=Native) return false;
				if (Existing.ContextId==Context) return true;
			}
			Surface.SourceSnapshots.Add({Path,Digest,static_cast<int64>(Bytes),Native,Context}); return true;
		}
		bool ReadAxis(const FObject& Geometry,FVector2D& Start,FVector2D& End)
		{
			FString Type; const FValues* Points=nullptr;
			if (!String(Geometry,TEXT("type"),Type) || Type!=TEXT("LineString") ||
				!Array(Geometry,TEXT("coordinates"),Points,2) || Points->Num()!=2) return false;
			for (int32 I=0; I<2; ++I)
			{
				const FValues* XY=nullptr; FVector2D& Point=I==0 ? Start : End;
				if (!(*Points)[I]->TryGetArray(XY) || !XY || XY->Num()!=2 ||
					!Number((*XY)[0],Point.X) || !Number((*XY)[1],Point.Y)) return false;
			}
			return Start!=End && FMath::IsFinite((End-Start).SizeSquared());
		}
		bool Parse(const FObject& Value,const FProjectWorldCanonicalFeature& Feature,
			FProjectWorldCanonicalConstructedSurface& Out)
		{
			double Version;
			if (!Exact(Value,{TEXT("schema_version"),TEXT("source_feature_id"),TEXT("model_sha256"),TEXT("canonical_crs"),
				TEXT("vertical_datum"),TEXT("basis"),TEXT("role"),TEXT("triangles"),TEXT("face_roles"),TEXT("source_bindings"),
				TEXT("policy_sha256"),TEXT("model_provenance")}) ||
				!ProjectWorldCanonicalJson::FiniteDocument(Value) ||
				!Number(Value->TryGetField(TEXT("schema_version")),Version) || Version!=1 ||
				!String(Value,TEXT("source_feature_id"),Out.SourceFeatureId) || Out.SourceFeatureId!=Feature.FeatureId ||
				!HashField(Value,TEXT("model_sha256"),Out.ModelHash) || !HashField(Value,TEXT("policy_sha256"),Out.PolicyHash) ||
				!String(Value,TEXT("canonical_crs"),Out.CanonicalCrs) || !Out.CanonicalCrs.StartsWith(TEXT("EPSG:")) ||
				!String(Value,TEXT("vertical_datum"),Out.VerticalDatum) || Out.VerticalDatum!=TEXT("EPSG:3855") ||
				!String(Value,TEXT("basis"),Out.Basis) || Out.Basis!=TEXT("identified_model") ||
				!String(Value,TEXT("role"),Out.Role) ||
				(Out.Role!=TEXT("fixed_pier") && Out.Role!=TEXT("floating_assembly") && Out.Role!=TEXT("identified_assembly") &&
				Out.Role!=TEXT("inferred_apron") && Out.Role!=TEXT("inferred_gate_apron"))) return false;
			const FString Epsg=Out.CanonicalCrs.Mid(5);
			if (Epsg.IsEmpty()) return false;
			for (TCHAR C : Epsg) if (C<'0' || C>'9') return false;
			FObject Provenance;
			if (!Object(Value,TEXT("model_provenance"),Provenance)) return false;
			const FValues *Bindings=nullptr,*Triangles=nullptr,*Roles=nullptr;
			if (!Array(Value,TEXT("source_bindings"),Bindings,32) || !Array(Value,TEXT("triangles"),Triangles,4096) ||
				!Array(Value,TEXT("face_roles"),Roles,64)) return false;
			TSet<FString> BindingKeys,Sources;
			TMap<FString,TArray<FObject>> SourceGeometries;
			for (const auto& Item : *Bindings)
			{
				const FObject* Row=nullptr; FObject Qualified,Geometry,Normalized,Native; FString Context,Id,Qualification;
				if (!Item->TryGetObject(Row) || !Row ||
					!Exact(*Row,{TEXT("context_id"),TEXT("provider_feature_id"),TEXT("qualification_identity"),TEXT("qualified"),
						TEXT("projected_geometry"),TEXT("normalized_input"),TEXT("native_input")}) ||
					!String(*Row,TEXT("context_id"),Context) || !String(*Row,TEXT("provider_feature_id"),Id) || !ProviderId(Id) ||
					!HashField(*Row,TEXT("qualification_identity"),Qualification) || !Object(*Row,TEXT("qualified"),Qualified) ||
					!Object(*Row,TEXT("projected_geometry"),Geometry) || !Object(*Row,TEXT("normalized_input"),Normalized) ||
					!Descriptor(Normalized,false,Context,Out)) return false;
				const auto NativeValue=(*Row)->TryGetField(TEXT("native_input"));
				if (!NativeValue || (NativeValue->Type==EJson::Null ? Id.StartsWith(TEXT("relation/")) :
					(!Object(*Row,TEXT("native_input"),Native) || !Descriptor(Native,true,Context,Out)))) return false;
				const FString Key=Context+TEXT("|")+Id;
				if (BindingKeys.Contains(Key)) return false;
				BindingKeys.Add(Key); Sources.Add(Id); Out.SourceProviderIds.AddUnique(Id);
				SourceGeometries.FindOrAdd(Id).Add(Geometry);
			}
			if (!Out.SourceProviderIds.ContainsByPredicate([&](const FString& Id)
				{return Feature.FeatureId.EndsWith(TEXT(":osm:")+Id.Replace(TEXT("/"),TEXT(":")));})) return false;
			for (const auto& Item : *Triangles)
			{
				const FValues* Points=nullptr; FProjectWorldCanonicalSurfaceTriangle Face;
				if (!Item->TryGetArray(Points) || !Points || Points->Num()!=3) return false;
				for (int32 I=0; I<3; ++I)
				{
					const FValues* XYZ=nullptr;
					if (!(*Points)[I]->TryGetArray(XYZ) || !XYZ || XYZ->Num()!=3) return false;
					for (int32 J=0; J<3; ++J) if (!Number((*XYZ)[J],Face.XYZ[I][J])) return false;
				}
				Out.Triangles.Add(Face);
			}
			TSet<int32> UsedFaces;
			for (const auto& Item : *Roles)
			{
				FProjectWorldCanonicalConstructedRegion Region;
				const FObject* Row=nullptr; const FValues *Ids=nullptr,*Faces=nullptr; FString Role,Axis;
				if (!Item->TryGetObject(Row) || !Row || !String(*Row,TEXT("role"),Role)) return false;
				const bool Gangway=Role==TEXT("gangway");
				if (Gangway ? !Exact(*Row,{TEXT("role"),TEXT("source_ids"),TEXT("triangles"),TEXT("axis_source_id")}) :
					!Exact(*Row,{TEXT("role"),TEXT("source_ids"),TEXT("triangles")})) return false;
				if (Role!=TEXT("fixed_pier") && Role!=TEXT("floating_deck") && !Gangway &&
					Role!=TEXT("inferred_apron") && Role!=TEXT("inferred_gate_apron")) return false;
				if ((Out.Role==TEXT("fixed_pier") && Role!=TEXT("fixed_pier")) ||
					(Out.Role==TEXT("floating_assembly") && Role!=TEXT("floating_deck") && !Gangway) ||
					(Out.Role==TEXT("inferred_apron") && Role!=TEXT("inferred_apron")) ||
					(Out.Role==TEXT("inferred_gate_apron") && Role!=TEXT("inferred_gate_apron"))) return false;
				if (!Array(*Row,TEXT("source_ids"),Ids,32) || !Array(*Row,TEXT("triangles"),Faces,4096)) return false;
				TSet<FString> Local;
				for (const auto& IdValue : *Ids)
				{
					FString Id;
					if (IdValue->Type!=EJson::String || !IdValue->TryGetString(Id) || !Sources.Contains(Id) || Local.Contains(Id)) return false;
					Local.Add(Id);
					Region.SourceIds.Add(Id);
				}
				if (Gangway && (!String(*Row,TEXT("axis_source_id"),Axis) || !Axis.StartsWith(TEXT("way/")) || !Local.Contains(Axis))) return false;
				Region.Role=Role; Region.AxisSourceId=Axis;
				if (Gangway)
				{
					const auto* Geometries=SourceGeometries.Find(Axis);
					if (!Geometries || Geometries->IsEmpty()) return false;
					bool First=true;
					for (const auto& Geometry : *Geometries)
					{
						FVector2D Start,End;
						if (!ReadAxis(Geometry,Start,End) || (!First && (Start!=Region.AxisStart || End!=Region.AxisEnd))) return false;
						Region.AxisStart=Start; Region.AxisEnd=End; First=false;
					}
				}
				for (const auto& FaceValue : *Faces)
				{
					double Index;
					if (!Number(FaceValue,Index) || Index<0 || Index>=Out.Triangles.Num() || Index!=FMath::FloorToDouble(Index) ||
						UsedFaces.Contains(static_cast<int32>(Index))) return false;
					UsedFaces.Add(static_cast<int32>(Index));
					Region.SourceTriangles.Add(static_cast<int32>(Index));
				}
				Out.FaceRegions.Add(MoveTemp(Region));
			}
			if (UsedFaces.Num()!=Out.Triangles.Num()) return false;
			FString Text;
			if (!FJsonSerializer::Serialize(Value.ToSharedRef(),TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text))) return false;
			FTCHARToUTF8 Utf8(*Text); TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes,Out.AdmissionHash);
		}
	}
	bool Read(const TSharedPtr<FJsonObject>& Attributes,FProjectWorldCanonicalFeature& Feature,
		FProjectWorldCanonicalValidation& Validation)
	{
		const bool Present=Attributes && Attributes->HasField(TEXT("constructed_surface"));
		if (!Present && Feature.FeatureClass!=TEXT("constructed_surface")) return true;
		FObject Value; FProjectWorldCanonicalConstructedSurface Candidate;
		if (Feature.FeatureClass==TEXT("constructed_surface") &&
			(Feature.GeometryType==TEXT("Polygon") || Feature.GeometryType==TEXT("MultiPolygon")) &&
			Object(Attributes,TEXT("constructed_surface"),Value) && Parse(Value,Feature,Candidate))
		{ Feature.ConstructedSurface=MoveTemp(Candidate); return true; }
		Validation.ErrorCode=TEXT("constructed-surface-contract");
		Validation.Message=TEXT("Canonical structural surface is missing, malformed or unsupported.");
		Validation.Detail=Feature.FeatureId; return false;
	}
	bool ReadPolicy(const TSharedPtr<FJsonObject>& Document,FProjectWorldCanonicalConstructedPolicy& Policy)
	{
		if (!Exact(Document,{TEXT("$schema"),TEXT("schema_version"),TEXT("maximum_triangles"),TEXT("maximum_ordinary_gradient"),
			TEXT("maximum_gangway_grade"),TEXT("maximum_cross_slope"),TEXT("maximum_numeric_error_m")})) return false;
		FString Schema; double Version,Triangles; FProjectWorldCanonicalConstructedPolicy Candidate;
		if (!String(Document,TEXT("$schema"),Schema) || Schema!=TEXT("https://alis.world/schemas/world-compiler/constructed-import-policy-v1.json") ||
			!Number(Document->TryGetField(TEXT("schema_version")),Version) || Version!=1 ||
			!Number(Document->TryGetField(TEXT("maximum_triangles")),Triangles) || Triangles<1 || Triangles>4096 || Triangles!=FMath::FloorToDouble(Triangles) ||
			!Number(Document->TryGetField(TEXT("maximum_ordinary_gradient")),Candidate.MaximumOrdinaryGradient) ||
			Candidate.MaximumOrdinaryGradient<=0 || Candidate.MaximumOrdinaryGradient>.02 ||
			!Number(Document->TryGetField(TEXT("maximum_gangway_grade")),Candidate.MaximumGangwayGrade) ||
			Candidate.MaximumGangwayGrade<=0 || Candidate.MaximumGangwayGrade>.125 ||
			!Number(Document->TryGetField(TEXT("maximum_cross_slope")),Candidate.MaximumCrossSlope) ||
			Candidate.MaximumCrossSlope<=0 || Candidate.MaximumCrossSlope>.02 ||
			!Number(Document->TryGetField(TEXT("maximum_numeric_error_m")),Candidate.MaximumNumericErrorMeters) ||
			Candidate.MaximumNumericErrorMeters<=0 || Candidate.MaximumNumericErrorMeters>1e-8) return false;
		Candidate.MaximumTriangles=static_cast<int32>(Triangles); Policy=Candidate; return true;
	}
}
