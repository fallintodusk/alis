// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRuntimeInput.h"
#include "ProjectWorldConstructedSurface.h"
#include "Dom/JsonObject.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	bool Vector(const TSharedPtr<FJsonValue>& Value,FVector& Out,int32 Dimensions)
	{
		if (!Value.IsValid() || Value->Type!=EJson::Array || Value->AsArray().Num()!=Dimensions) return false;
		Out=FVector::ZeroVector;
		for (int32 I=0; I<Dimensions; ++I)
		{
			const auto& V=Value->AsArray()[I]; if (!V.IsValid() || V->Type!=EJson::Number) return false;
			Out[I]=V->AsNumber();
		}
		return !Out.ContainsNaN();
	}
	bool Load(FInput& Out,FString& Error,const FString& Path,const FString& Expected)
	{
		Out=FInput(); Error.Reset();
		if (Path.IsEmpty())
		{
			FProjectWorldStoredSurface Unit;
			Unit.Triangles={{{FVector(-400,-300,135),FVector(0,300,135),FVector(0,-300,135)}},
				{{FVector(-400,-300,135),FVector(-400,300,135),FVector(0,300,135)}},
				{{FVector(0,-150,135),FVector(0,150,135),FVector(2000,-150,370)}},
				{{FVector(2000,-150,370),FVector(0,150,135),FVector(2000,150,370)}},
				{{FVector(2000,-300,370),FVector(2000,300,370),FVector(2400,-300,370)}},
				{{FVector(2400,-300,370),FVector(2000,300,370),FVector(2400,300,370)}}};
			Out.Units={Unit};
			for (auto T : Unit.Triangles)
			{
				for (auto& P : T.Points) P=FVector(P.X*.01,-P.Y*.01,P.Z*.01);
				Out.Field.Add(T);
			}
			return true;
		}
		FString Text; TSharedPtr<FJsonObject> Root;
		if (Expected.Len()!=64 || !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Out.Hash) || Out.Hash!=Expected ||
			!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root))
		{ Error=TEXT("The exact complete-structure candidate input cannot be authenticated."); return false; }
		FString Owner;
		if (Root->TryGetStringField(TEXT("physical_owner"),Owner) && Owner==TEXT("grouped_neighborhood_runtime_v1"))
		{
			FInput Candidate;
			if (!LoadGrouped(*Root,Out.Hash,Candidate,Error)) { Out=FInput(); return false; }
			Out=MoveTemp(Candidate); return true;
		}
		if (!Root->TryGetStringField(TEXT("physical_owner"),Owner) ||
			(Owner!=TEXT("complete_structural_surface_diagnostic") && Owner!=TEXT("modeled_approach_diagnostic")))
		{ Error=TEXT("Runtime candidate requires an explicitly supported diagnostic physical owner."); return false; }
		Out.Approach=Owner==TEXT("modeled_approach_diagnostic");
		const bool ExplicitConnector=Root->HasField(TEXT("connector_source_triangle_ordinals"));
		if ((Out.Approach && ExplicitConnector) || (Root->HasField(TEXT("body_role_quality_sha256")) && !ExplicitConnector))
		{ Error=TEXT("Source-role runtime controls require explicit native-unit connector provenance."); return false; }
		if ((Out.Approach && Root->HasField(TEXT("native_physical_units"))) || (!Out.Approach && Root->HasField(TEXT("canonical_cell"))))
		{ Error=TEXT("Canonical-field and actual-native-unit fixture modes cannot be mixed."); return false; }
		int32 Total=0;
		if (!Out.Approach)
		{
		const TArray<TSharedPtr<FJsonValue>>* Units=nullptr;
		if (!Root->TryGetArrayField(TEXT("native_physical_units"),Units) || !Units || Units->IsEmpty() || Units->Num()>64)
		{ Error=TEXT("The candidate has no bounded actual native surface census."); return false; }
		for (const auto& V : *Units)
		{
			if (!V.IsValid() || V->Type!=EJson::Object) { Error=TEXT("Candidate native surface is not an object."); return false; }
			const auto Object=V->AsObject(); FProjectWorldStoredSurface Unit;
			const TArray<TSharedPtr<FJsonValue>>* Vertices=nullptr; const TArray<TSharedPtr<FJsonValue>>* Faces=nullptr;
			if (!Object.IsValid() || !Vector(Object->TryGetField(TEXT("origin_cm")),Unit.Origin) ||
				!Object->TryGetArrayField(TEXT("vertices_cm"),Vertices) || !Vertices || Vertices->Num()>12288 ||
				!Object->TryGetArrayField(TEXT("triangles"),Faces) || !Faces || Total+Faces->Num()>4096)
			{ Error=TEXT("The candidate native surface frame or geometry census is invalid."); return false; }
			TArray<FVector> Points;
			for (const auto& P : *Vertices)
			{
				FVector Point;
				if (!Vector(P,Point) || FVector(FVector3f(Point))!=Point)
				{ Error=TEXT("Candidate geometry is not the actual finite native float representation."); return false; }
				Points.Add(Point);
			}
			for (const auto& F : *Faces)
			{
				FVector Indices; if (!Vector(F,Indices)) { Error=TEXT("Candidate triangle indices are invalid."); return false; }
				FProjectWorldSurfaceTriangle T;
				for (int32 I=0; I<3; ++I)
				{
					if (Indices[I]<0 || Indices[I]>=Points.Num() || Indices[I]!=FMath::FloorToDouble(Indices[I]))
					{ Error=TEXT("Candidate triangle leaves its actual vertex census."); return false; }
					T.Points[I]=Points[static_cast<int32>(Indices[I])];
				}
				Unit.Triangles.Add(T);
			}
			TArray<int32> Sources; const TArray<TSharedPtr<FJsonValue>>* Trace=nullptr;
			if (ExplicitConnector)
			{
				if (!Object->TryGetArrayField(TEXT("source_triangle_ordinals"),Trace) || !Trace || Trace->Num()!=Faces->Num())
				{ Error=TEXT("Connector control needs one original source ordinal per actual native face."); return false; }
				for (const auto& Value : *Trace)
				{
					if (!Value.IsValid() || Value->Type!=EJson::Number || !FMath::IsFinite(Value->AsNumber()) || Value->AsNumber()<0 ||
						Value->AsNumber()>=4096 || Value->AsNumber()!=FMath::FloorToDouble(Value->AsNumber()))
					{ Error=TEXT("Connector control source ordinal is invalid."); return false; }
					Sources.Add(static_cast<int32>(Value->AsNumber()));
				}
			}
			else if (Object->HasField(TEXT("source_triangle_ordinals")))
			{ Error=TEXT("Native provenance cannot silently lose its declared connector control."); return false; }
			Out.UnitSources.Add(MoveTemp(Sources)); Total+=Faces->Num(); Out.Units.Add(MoveTemp(Unit));
		}
		}
		FProjectWorldCanonicalBundle Frame; FVector Geo;
		if (!Vector(Root->TryGetField(TEXT("engine_georeference_origin_m")),Geo,2) ||
			!Root->TryGetNumberField(TEXT("diagnostic_height_origin_m"),Frame.HeightOriginMeters) || !FMath::IsFinite(Frame.HeightOriginMeters))
		{ Error=TEXT("Candidate canonical/native frame is missing."); return false; }
		Frame.EngineGeoreferenceOriginMeters=FVector2D(Geo);
		const TArray<TSharedPtr<FJsonValue>>* Source=nullptr; const TArray<TSharedPtr<FJsonValue>>* Route=nullptr;
		double Lift;
		if (!Root->TryGetArrayField(TEXT("source_model_triangles"),Source) || !Source || Source->IsEmpty() || Source->Num()>4096 ||
			!Root->TryGetArrayField(TEXT("source_route"),Route) || !Route || Route->Num()!=2 ||
			!Root->TryGetNumberField(TEXT("presentation_lift_m"),Lift) || !FMath::IsFinite(Lift))
		{ Error=TEXT("Candidate source support or directed route is missing."); return false; }
		TArray<FProjectWorldSurfaceTriangle> Field;
		for (const auto& V : *Source)
		{
			if (!V.IsValid() || V->Type!=EJson::Array || V->AsArray().Num()!=3) { Error=TEXT("Invalid candidate source triangle."); return false; }
			FProjectWorldSurfaceTriangle T;
			for (int32 I=0; I<3; ++I) if (!Vector(V->AsArray()[I],T.Points[I])) { Error=TEXT("Invalid candidate source coordinate."); return false; }
			Field.Add(T);
		}
		if (!ProjectWorldConstructedSurface::ValidateField(Field,Error)) return false;
		if (ExplicitConnector)
		{
			const TArray<TSharedPtr<FJsonValue>>* Connector=nullptr; FString RoleHash,AssemblyHash;
			if (!Root->TryGetStringField(TEXT("body_role_quality_sha256"),RoleHash) || RoleHash.Len()!=64 ||
				!Root->TryGetStringField(TEXT("assembly_sha256"),AssemblyHash) || AssemblyHash.Len()!=64 ||
				!Root->TryGetArrayField(TEXT("connector_source_triangle_ordinals"),Connector) || !Connector || Connector->IsEmpty() || Connector->Num()>Field.Num())
			{ Error=TEXT("Connector control requires a pinned role policy and nonempty finite source selection."); return false; }
			for (const auto& Value : *Connector)
			{
				if (!Value.IsValid() || Value->Type!=EJson::Number || !FMath::IsFinite(Value->AsNumber()) || Value->AsNumber()<0 ||
					Value->AsNumber()>=Field.Num() || Value->AsNumber()!=FMath::FloorToDouble(Value->AsNumber()) || Out.ConnectorSources.Contains(static_cast<int32>(Value->AsNumber())))
				{ Error=TEXT("Connector source selection is duplicated or outside the authenticated field."); return false; }
				Out.ConnectorSources.Add(static_cast<int32>(Value->AsNumber()));
			}
			TSet<int32> Emitted;
			for (const auto& Sources : Out.UnitSources) for (const int32 SourceIndex : Sources)
			{
				if (!Field.IsValidIndex(SourceIndex)) { Error=TEXT("Native provenance leaves the authenticated source field."); return false; }
				Emitted.Add(SourceIndex);
			}
			for (const int32 SourceIndex : Out.ConnectorSources)
				if (!Emitted.Contains(SourceIndex)) { Error=TEXT("Declared connector source has no actual native output to remove."); return false; }
		}
		FVector* Ends[2]={&Out.Start,&Out.End};
		for (int32 I=0; I<2; ++I)
		{
			FVector P;
			if (!Vector((*Route)[I],P,2) || !ProjectWorldConstructedSurface::Sample(Field,FVector2D(P),P.Z,Error)) return false;
			P.Z+=Lift; *Ends[I]=FProjectWorldCanonicalLoader::CanonicalToUnreal(Frame,P);
		}
		if (Out.Approach)
		{
			const TSharedPtr<FJsonObject>* CellObject=nullptr; const TArray<TSharedPtr<FJsonValue>>* Bounds=nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Core=nullptr; FProjectWorldCanonicalCell Cell;
			double Grade,Cross,Agreement;
			if (Lift!=0 || !Root->TryGetObjectField(TEXT("canonical_cell"),CellObject) || !CellObject ||
				!(*CellObject)->TryGetStringField(TEXT("cell_id"),Cell.CellId) || Cell.CellId.IsEmpty() ||
				!(*CellObject)->TryGetArrayField(TEXT("bounds"),Bounds) || !Bounds || Bounds->Num()!=4 ||
				!Root->TryGetArrayField(TEXT("mapped_source_route"),Core) || !Core || Core->Num()!=2 ||
				!Root->TryGetStringField(TEXT("canonical_crs"),Frame.CanonicalCrs) || Frame.CanonicalCrs.IsEmpty() ||
				!Root->TryGetStringField(TEXT("vertical_datum"),Frame.VerticalDatum) || Frame.VerticalDatum!=TEXT("EPSG:3855") ||
				!Root->TryGetNumberField(TEXT("maximum_grade"),Grade) || !FMath::IsFinite(Grade) || Grade<=0 || Grade>.05 ||
				!Root->TryGetNumberField(TEXT("maximum_cross_slope"),Cross) || !FMath::IsFinite(Cross) || Cross<=0 || Cross>.02 ||
				!Root->TryGetNumberField(TEXT("maximum_model_error_m"),Agreement) || !FMath::IsFinite(Agreement) || Agreement<=0 || Agreement>.001)
			{ Error=TEXT("Approach fixture needs one finite cell/frame, zero lift, an explicit core and unchanged or stricter quality gates."); return false; }
			for (int32 I=0; I<4; ++I)
			{
				const auto& V=(*Bounds)[I];
				if (!V.IsValid() || V->Type!=EJson::Number || !FMath::IsFinite(V->AsNumber()))
				{ Error=TEXT("Approach cell bounds are nonfinite or invalid."); return false; }
				Cell.Bounds[I]=V->AsNumber();
			}
			FVector* CoreEnds[2]={&Out.CoreStart,&Out.CoreEnd};
			for (int32 I=0; I<2; ++I)
			{
				FVector P;
				if (!Vector((*Core)[I],P,2) || !ProjectWorldConstructedSurface::Sample(Field,FVector2D(P),P.Z,Error)) return false;
				*CoreEnds[I]=FProjectWorldCanonicalLoader::CanonicalToUnreal(Frame,P);
			}
			FVector2D Axis(Out.CoreEnd-Out.CoreStart);
			if (Axis.SizeSquared()<=0) { Error=TEXT("Approach core axis is degenerate."); return false; }
			FMeshDescription Description; FProjectWorldStoredSurface Unit;
			if (!ProjectWorldConstructedMesh::Build(Frame,Cell,Field,Agreement,Description,Unit.Origin,Error)) return false;
			const auto Positions=FStaticMeshConstAttributes(Description).GetVertexPositions();
			for (const auto Id : Description.Triangles().GetElementIDs())
			{
				FProjectWorldSurfaceTriangle T;
				for (int32 I=0; I<3; ++I) T.Points[I]=FVector(Positions[Description.GetVertexInstanceVertex(Description.GetTriangleVertexInstance(Id,I))]);
				Unit.Triangles.Add(T);
			}
			if (!ProjectWorldConstructedMesh::ValidateSlopes(Unit.Triangles,Axis,Grade,Cross,Out.Slopes,Error)) return false;
			Total=Unit.Triangles.Num(); Out.Units.Add(MoveTemp(Unit));
		}
		Out.Frame=MoveTemp(Frame); Out.Field=MoveTemp(Field); Out.Lift=Lift;
		if (Total==0) { Error=TEXT("Candidate has no actual physical triangles."); return false; }
		return true;
	}
	bool CanonicalHeight(const FInput& Input,const FVector2D& Point,double& Out,FString& Error)
	{
		Out=std::numeric_limits<double>::quiet_NaN(); Error.Reset();
		if (!Input.Grouped) return ProjectWorldConstructedSurface::Sample(Input.Field,Point,Out,Error);
		bool Found=false; double Height=0;
		for (const auto& Group : Input.SourceGroups)
		{
			double Value; const auto Result=ProjectWorldConstructedSurface::TrySample(Group.Field,Point,Value,Error);
			if (Result==ProjectWorldConstructedSurface::ESampleResult::Invalid) return false;
			if (Result==ProjectWorldConstructedSurface::ESampleResult::Outside) continue;
			if (Found && FMath::Abs(Height-Value)>1e-8) { Error=TEXT("Grouped model has contradictory support at the requested location."); return false; }
			Height=Value; Found=true;
		}
		if (!Found) { Error=TEXT("Grouped model support is unknown at the requested location."); return false; }
		Out=Height; return true;
	}
	bool ExpectedHeight(const FInput& Input,const FVector& Point,double& Height)
	{
		const auto Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Input.Frame,Point); FString Error; double Z;
		if (!CanonicalHeight(Input,FVector2D(Canonical),Z,Error)) return false;
		Height=(Z+Input.Lift-Input.Frame.HeightOriginMeters)*100; return true;
	}
}
#endif
