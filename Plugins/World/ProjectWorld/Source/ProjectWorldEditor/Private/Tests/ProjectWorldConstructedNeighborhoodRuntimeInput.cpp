// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedNeighborhoodInput.h"
#include "ProjectWorldConstructedRuntimeInput.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	namespace
	{
		bool ReadPinned(const FJsonObject& Root,const TCHAR* PathKey,const TCHAR* HashKey,TSharedPtr<FJsonObject>& Out,FString& Error)
		{
			FString Path,Expected,Actual,Text; Out.Reset();
			if (!Root.TryGetStringField(PathKey,Path) || !Root.TryGetStringField(HashKey,Expected) || Expected.Len()!=64 ||
				!FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) || Actual!=Expected ||
				!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out))
			{ Error=TEXT("Grouped runtime cannot authenticate the exact ")+FString(PathKey)+TEXT(" input."); return false; }
			return true;
		}
	}
	bool LoadGrouped(const FJsonObject& Root,const FString& Hash,FInput& Out,FString& Error)
	{
		using namespace ProjectWorldConstructedNeighborhood;
		Out=FInput(); Error.Reset(); FInput Candidate; Candidate.Hash=Hash; Candidate.Grouped=true;
		FString ManifestPath,ManifestHash;
		if (Hash.Len()!=64 || !Root.TryGetStringField(TEXT("scene_manifest_path"),ManifestPath) || !Root.TryGetStringField(TEXT("scene_manifest_sha256"),ManifestHash))
		{ Error=TEXT("Grouped runtime requires an exact scene manifest."); return false; }
		ProjectWorldConstructedNeighborhood::FScene Scene;
		if (!ProjectWorldConstructedNeighborhood::Load(ManifestPath,ManifestHash,Scene,Error) || !Scene.SharedPartition) return false;
		TSharedPtr<FJsonObject> Native,Policy,Planform;
		if (!ReadPinned(Root,TEXT("native_scene_path"),TEXT("native_scene_sha256"),Native,Error) ||
			!ReadPinned(Root,TEXT("case_policy_path"),TEXT("case_policy_sha256"),Policy,Error) ||
			!ReadPinned(Root,TEXT("road_planform_path"),TEXT("road_planform_sha256"),Planform,Error)) return false;
		FString Owner,NativeManifest,GeometryHash,CaseHash,PlanformCase,BaseHash;
		if (!Native->TryGetStringField(TEXT("physical_owner"),Owner) || Owner!=TEXT("grouped_neighborhood_diagnostic_v1") ||
			!Native->TryGetStringField(TEXT("manifest_sha256"),NativeManifest) || NativeManifest!=ManifestHash ||
			!Native->TryGetStringField(TEXT("source_geometry_sha256"),GeometryHash) || GeometryHash!=Scene.GeometryHash ||
			!Root.TryGetStringField(TEXT("case_policy_sha256"),CaseHash) || !Planform->TryGetStringField(TEXT("case_policy_sha256"),PlanformCase) || PlanformCase!=CaseHash ||
			!Planform->TryGetStringField(TEXT("base_result_sha256"),BaseHash) || !Root.TryGetStringField(TEXT("connector_route_id"),Candidate.ConnectorRoute))
		{ Error=TEXT("Grouped runtime artifacts have conflicting source identities."); return false; }
		TSharedPtr<FJsonObject> Manifest;
		if (!ReadPinned(Root,TEXT("scene_manifest_path"),TEXT("scene_manifest_sha256"),Manifest,Error) || BaseHash!=Manifest->GetStringField(TEXT("base_result_sha256")))
		{ Error=TEXT("Grouped runtime Road emission belongs to another canonical base."); return false; }
		const TArray<TSharedPtr<FJsonValue>>* Units=nullptr;
		int32 RequiredUnits=0; for (const auto& Group : Scene.Groups) RequiredUnits+=Group.RequiredCells.Num();
		if (!Native->TryGetArrayField(TEXT("native_physical_units"),Units) || !Units || Units->Num()!=RequiredUnits)
		{ Error=TEXT("Grouped runtime omits required owner/cell units."); return false; }
		TArray<FUnit> Decoded; FExpectedOutputs Expected;
		for (const auto& Value : *Units)
		{
			if (!Value.IsValid() || Value->Type!=EJson::Object) { Error=TEXT("Grouped runtime unit is not an object."); return false; }
			const auto Object=Value->AsObject(); FUnit Unit;
			if (!Object->TryGetStringField(TEXT("group_id"),Unit.GroupId) || !Object->TryGetStringField(TEXT("cell_id"),Unit.CellId) ||
				!Vector(Object->TryGetField(TEXT("origin_cm")),Unit.Surface.Origin))
			{ Error=TEXT("Grouped runtime unit lacks its owner/cell/frame."); return false; }
			const auto* Group=Scene.Groups.FindByPredicate([&](const auto& G){ return G.Id==Unit.GroupId; });
			const TArray<TSharedPtr<FJsonValue>>* Points=nullptr; const TArray<TSharedPtr<FJsonValue>>* Faces=nullptr; const TArray<TSharedPtr<FJsonValue>>* Trace=nullptr;
			if (!Group || !Object->TryGetArrayField(TEXT("vertices_cm"),Points) || !Points || Points->Num()>15*Group->Field.Num() ||
				!Object->TryGetArrayField(TEXT("triangles"),Faces) || !Faces || Faces->IsEmpty() || Faces->Num()>5*Group->Field.Num() ||
				!Object->TryGetArrayField(TEXT("source_triangle_ordinals"),Trace) || !Trace || Trace->Num()!=Faces->Num())
			{ Error=TEXT("Grouped runtime geometry exceeds its original per-field clipping bounds or lacks exact provenance."); return false; }
			TArray<FVector> Vertices;
			for (const auto& Point : *Points)
			{
				FVector P;
				if (!Vector(Point,P) || FVector(FVector3f(P))!=P) { Error=TEXT("Grouped runtime geometry is not actual finite F32 storage."); return false; }
				Vertices.Add(P);
			}
			for (int32 I=0; I<Faces->Num(); ++I)
			{
				FVector Indices; FProjectWorldSurfaceTriangle Triangle;
				if (!Vector((*Faces)[I],Indices)) { Error=TEXT("Grouped runtime face indices are invalid."); return false; }
				for (int32 J=0; J<3; ++J)
				{
					if (Indices[J]<0 || Indices[J]>=Vertices.Num() || Indices[J]!=FMath::FloorToDouble(Indices[J]))
					{ Error=TEXT("Grouped runtime face leaves its actual vertex array."); return false; }
					Triangle.Points[J]=Vertices[static_cast<int32>(Indices[J])];
				}
				const auto& Source=(*Trace)[I];
				if (!Source.IsValid() || Source->Type!=EJson::Number || !FMath::IsFinite(Source->AsNumber()) || Source->AsNumber()<0 ||
					Source->AsNumber()>=Group->Field.Num() || Source->AsNumber()!=FMath::FloorToDouble(Source->AsNumber()))
				{ Error=TEXT("Grouped runtime source ordinal leaves its declared owner."); return false; }
				Unit.Surface.Triangles.Add(Triangle); Unit.Sources.Add(static_cast<int32>(Source->AsNumber()));
			}
			// The pinned prior export already passed Build, role and owner coverage.
			// These expectations bind its decoding, not an arbitrary new candidate.
			if (!CaptureBuildOutput(Unit,Expected,Error)) return false;
			Decoded.Add(MoveTemp(Unit));
		}
		if (!ValidateCensus(Scene,Decoded,Expected,Error)) return false;
		Candidate.Frame=MoveTemp(Scene.Frame);
		for (const auto& Group : Scene.Groups)
		{
			Candidate.SourceGroups.Add({Group.Id,Group.Role,Group.Field});
			if (Group.Role==TEXT("constructed_body")) Candidate.ConnectorGroup=Group.Id;
		}
		Candidate.ConnectorSources=Scene.Body.ConnectorSources; TSet<int32> EmittedConnector;
		for (auto& Unit : Decoded)
		{
			if (Unit.GroupId==Candidate.ConnectorGroup) for (const int32 Source : Unit.Sources) if (Candidate.ConnectorSources.Contains(Source)) EmittedConnector.Add(Source);
			Candidate.UnitGroups.Add(Unit.GroupId); Candidate.Units.Add(MoveTemp(Unit.Surface)); Candidate.UnitSources.Add(MoveTemp(Unit.Sources));
		}
		if (Candidate.ConnectorGroup.IsEmpty() || Candidate.ConnectorSources.IsEmpty() || EmittedConnector.Num()!=Candidate.ConnectorSources.Num())
		{ Error=TEXT("Grouped runtime has no complete explicitly attributed gangway control."); return false; }
		double Water,MinimumDeckHeight;
		if (!Manifest->TryGetNumberField(TEXT("water_m"),Water) || Water!=0 ||
			!Manifest->TryGetNumberField(TEXT("minimum_structural_clearance_m"),MinimumDeckHeight) || MinimumDeckHeight!=1.2 ||
			!MeasureDeckWater(Candidate,Water,MinimumDeckHeight,Candidate.DeckHeightsAboveWater,Error))
		{ if (Error.IsEmpty()) Error=TEXT("Grouped runtime requires the frozen representative Water and deck-height policy."); return false; }
		const TArray<TSharedPtr<FJsonValue>>* Cases=nullptr; const TArray<TSharedPtr<FJsonValue>>* Emissions=nullptr;
		if (!Policy->TryGetArrayField(TEXT("cases"),Cases) || !Cases || Cases->IsEmpty() || Cases->Num()>32 ||
			!Planform->TryGetArrayField(TEXT("roads"),Emissions) || !Emissions || Emissions->Num()!=Cases->Num())
		{ Error=TEXT("Grouped runtime requires its complete source Road and emission census."); return false; }
		TSet<FString> SourceIds,EmissionIds;
		for (const auto& Value : *Cases)
		{
			if (!Value.IsValid() || Value->Type!=EJson::Object) { Error=TEXT("Grouped route is not an object."); return false; }
			const auto Object=Value->AsObject(); FRoute Route; const TArray<TSharedPtr<FJsonValue>>* Axis=nullptr;
			if (!Object->TryGetStringField(TEXT("id"),Route.Id) || Route.Id.IsEmpty() || SourceIds.Contains(Route.Id) ||
				!Object->TryGetNumberField(TEXT("nominal_width_m"),Route.WidthMeters) || !FMath::IsFinite(Route.WidthMeters) || Route.WidthMeters<=0 ||
				!Object->TryGetArrayField(TEXT("source_axis"),Axis) || !Axis || Axis->Num()<2 || Axis->Num()>256)
			{ Error=TEXT("Grouped runtime route identity/width/full axis is invalid."); return false; }
			for (const auto& Point : *Axis)
			{
				FVector P;
				if (!Vector(Point,P,2) || !CanonicalHeight(Candidate,FVector2D(P),P.Z,Error)) return false;
				Route.Points.Add(FProjectWorldCanonicalLoader::CanonicalToUnreal(Candidate.Frame,P));
			}
			SourceIds.Add(Route.Id); Candidate.Routes.Add(MoveTemp(Route));
		}
		for (const auto& Value : *Emissions)
		{
			FString Id;
			if (!Value.IsValid() || Value->Type!=EJson::Object || !Value->AsObject()->TryGetStringField(TEXT("source_feature_id"),Id) ||
				!SourceIds.Contains(Id) || EmissionIds.Contains(Id))
			{ Error=TEXT("Grouped runtime Road emission is missing, substituted or duplicated."); return false; }
			EmissionIds.Add(Id);
		}
		const auto* Connector=Candidate.Routes.FindByPredicate([&](const auto& Route){ return Route.Id==Candidate.ConnectorRoute; });
		if (!Connector || Connector->Points.Num()!=2 || FVector2D(Connector->Points[0])!=FVector2D(Scene.Body.Start) ||
			FVector2D(Connector->Points[1])!=FVector2D(Scene.Body.End))
		{ Error=TEXT("Grouped runtime connector route differs from its authenticated body model."); return false; }
		Candidate.Start=Connector->Points[0]; Candidate.End=Connector->Points[1]; Out=MoveTemp(Candidate); return true;
	}
}
#endif
