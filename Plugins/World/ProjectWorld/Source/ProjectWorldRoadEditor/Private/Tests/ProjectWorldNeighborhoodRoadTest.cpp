// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadEmission.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldNeighborhoodRoadTest
{
	TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
	{ TArray<TSharedPtr<FJsonValue>> Out; for (const double V : Values) Out.Add(MakeShared<FJsonValueNumber>(V)); return Out; }
	bool Read(const FString& Path,const FString& Expected,TSharedPtr<FJsonObject>& Out)
	{
		FString Actual,Text;
		return Expected.Len()==64 && FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) && Actual==Expected &&
			FFileHelper::LoadFileToString(Text,*Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out);
	}
	FProjectWorldRealizationLayer Layer(const TSet<FString>& Classes)
	{
		auto Settings=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Selected; auto Sorted=Classes.Array(); Sorted.Sort();
		for (const auto& Class : Sorted) Selected.Add(MakeShared<FJsonValueString>(Class));
		Settings->SetArrayField(TEXT("selected_classes"),Selected); Settings->SetNumberField(TEXT("surface_offset_m"),.15);
		Settings->SetNumberField(TEXT("maximum_segment_length_m"),7.5); Settings->SetBoolField(TEXT("nanite"),true);
		Settings->SetStringField(TEXT("collision"),TEXT("complex_as_simple")); Settings->SetStringField(TEXT("structure_fallback"),TEXT("terrain_drape"));
		Settings->SetStringField(TEXT("intersection_policy"),TEXT("overlap_same_owner")); FProjectWorldRealizationLayer Result;
		Result.LayerId=TEXT("roads"); Result.GeneratorId=TEXT("project_road_mesh"); Result.GeneratorVersion=1; Result.DependencyHaloCells=1;
		FJsonSerializer::Serialize(Settings,TJsonWriterFactory<>::Create(&Result.NormalizedSettings)); return ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Result));
	}
	void Small(FProjectWorldCanonicalBundle& B,TSharedPtr<FJsonObject>& Policy,TArray<FString>& Cells)
	{
		B.CoordinateQuantizationMeters=.01; B.CanonicalCrs=TEXT("EPSG:32618"); B.VerticalDatum=TEXT("EPSG:3855");
		FProjectWorldCanonicalFeature Road; Road.FeatureId=TEXT("fixture:path"); Road.FeatureClass=TEXT("road"); Road.RoadClass=TEXT("footway");
		Road.GeometryType=TEXT("LineString"); Road.GeometryPoints={FVector2D(2,5),FVector2D(18,5)}; Road.WidthMeters=2;
		for (int32 I=0; I<2; ++I)
		{
			auto& Cell=B.Cells.AddDefaulted_GetRef(); Cell.CellId=FString::Printf(TEXT("cell_%d"),I); Cell.CellX=I;
			Cell.Bounds=Cell.Terrain.Bounds=FVector4(I*10,0,(I+1)*10,10); Cell.Terrain.SamplesX=Cell.Terrain.SamplesY=2;
			Cell.Terrain.SampleSpacing=FVector2D(10,10); Cell.Terrain.HeightsMeters.Init(0,4); Cell.Terrain.ArtifactHash=FString::ChrN(64,I ? 'b' : 'a');
			if (I==0) { Road.OwnerCellId=Cell.CellId; Cell.OwnedFeatureIds={Road.FeatureId}; } else Cell.ReferencedFeatureIds={Road.FeatureId};
			auto& R=Road.Representations.AddDefaulted_GetRef(); R.CellId=Cell.CellId; R.Kind=TEXT("road_fragment");
			R.Parts={{FVector2D(I ? 10 : 2,5),FVector2D(I ? 18 : 10,5)}}; Cells.Add(Cell.CellId);
		}
		B.Features.Add(Road.FeatureId,Road); Policy=MakeShared<FJsonObject>(); auto Case=MakeShared<FJsonObject>();
		Case->SetStringField(TEXT("id"),Road.FeatureId); Case->SetNumberField(TEXT("nominal_width_m"),2);
		Case->SetArrayField(TEXT("source_axis"),{MakeShared<FJsonValueArray>(Numbers({2,5})),MakeShared<FJsonValueArray>(Numbers({18,5}))});
		Policy->SetArrayField(TEXT("cases"),{MakeShared<FJsonValueObject>(Case)});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldNeighborhoodRoadTest,
	"Project.World.Realization.Road.NeighborhoodPlanform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldNeighborhoodRoadTest::RunTest(const FString&)
{
	using namespace ProjectWorldNeighborhoodRoadTest;
	const FString Path=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_ROAD_NEIGHBORHOOD_FIXTURE"));
	FProjectWorldCanonicalBundle B; TSharedPtr<FJsonObject> Policy,Fixture; TArray<FString> Cells; FString Output,BaseHash,PolicyHash,Error;
	if (Path.IsEmpty()) Small(B,Policy,Cells);
	else
	{
		if (!TestTrue(TEXT("The exact Road-planform fixture is authenticated."),Read(Path,FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_ROAD_NEIGHBORHOOD_SHA256")),Fixture))) return false;
		BaseHash=Fixture->GetStringField(TEXT("base_result_sha256")); PolicyHash=Fixture->GetStringField(TEXT("case_policy_sha256"));
		TSharedPtr<FJsonObject> Base; FProjectWorldCanonicalValidation Validation;
		if (!TestTrue(TEXT("Road emission consumes the immutable original base and source cases."),
			Read(Fixture->GetStringField(TEXT("base_result")),BaseHash,Base) && Read(Fixture->GetStringField(TEXT("case_policy_path")),PolicyHash,Policy) &&
			FProjectWorldCanonicalLoader::Load(Fixture->GetStringField(TEXT("base_result")),B,Validation))) { AddError(Validation.Message); return false; }
		for (const auto& Value : Fixture->GetArrayField(TEXT("selected_cells")))
		{
			const FString Id=Value->AsString();
			if (!TestTrue(TEXT("Selected Road cells are present and unique."),!Cells.Contains(Id) && B.Cells.ContainsByPredicate([&](const auto& C){ return C.CellId==Id; }))) return false;
			Cells.Add(Id);
		}
		Output=Fixture->GetStringField(TEXT("output_path"));
		if (!TestTrue(TEXT("Planform output is fresh and confined to project scratch."),!FPaths::FileExists(Output) &&
			FPaths::IsUnderDirectory(Output,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))) return false;
	}
	const auto& Cases=Policy->GetArrayField(TEXT("cases"));
	if (!TestTrue(TEXT("The complete explicit Road/cell selection is bounded and nonempty."),Cases.Num()>0 && Cases.Num()<=32 && Cells.Num()>0 && Cells.Num()<=2)) return false;
	TSet<FString> Selected,Classes;
	for (const auto& Value : Cases)
	{
		const auto Case=Value->AsObject(); const FString Id=Case->GetStringField(TEXT("id")); const auto* Road=B.Features.Find(Id);
		if (!TestTrue(TEXT("Every original Road is unique and retains the declared complete width."),Road && Road->FeatureClass==TEXT("road") &&
			!Selected.Contains(Id) && Road->WidthMeters==Case->GetNumberField(TEXT("nominal_width_m")) && !Road->RoadClass.IsEmpty())) return false;
		const auto& Axis=Case->GetArrayField(TEXT("source_axis"));
		if (!TestEqual(TEXT("The declared case retains the complete canonical source axis."),Axis.Num(),Road->GeometryPoints.Num())) return false;
		for (int32 I=0; I<Axis.Num(); ++I)
		{
			const auto& Point=Axis[I]->AsArray();
			if (!TestTrue(TEXT("The original source axis has no inferred XY correction."),Point.Num()==2 && FVector2D(Point[0]->AsNumber(),Point[1]->AsNumber())==Road->GeometryPoints[I])) return false;
		}
		if (!TestTrue(TEXT("Additional geometry parts cannot silently escape the declared axis."),Road->GeometryParts.IsEmpty() || (Road->GeometryParts.Num()==1 && Road->GeometryParts[0]==Road->GeometryPoints))) return false;
		for (const auto& R : Road->Representations) if (R.Kind==TEXT("road_fragment"))
			if (!TestTrue(TEXT("All source Road fragments lie in the selected neighborhood."),Cells.Contains(R.CellId))) return false;
		Selected.Add(Id); Classes.Add(Road->RoadClass);
	}
	const auto Settings=Layer(Classes); TArray<TSharedPtr<FJsonValue>> Roads; int32 Total=0;
	for (const auto& Value : Cases)
	{
		const auto Case=Value->AsObject(); const FString Id=Case->GetStringField(TEXT("id")); const auto& Road=B.Features.FindChecked(Id);
		auto Result=MakeShared<FJsonObject>(); Result->SetStringField(TEXT("source_feature_id"),Id);
		Result->SetNumberField(TEXT("source_width_m"),Road.WidthMeters); Result->SetArrayField(TEXT("source_axis"),Case->GetArrayField(TEXT("source_axis")));
		TArray<TSharedPtr<FJsonValue>> Units; int32 Faces=0;
		for (const auto& CellId : Cells)
		{
			const auto& Cell=*B.Cells.FindByPredicate([&](const auto& C){ return C.CellId==CellId; });
			const bool Expected=(Cell.OwnedFeatureIds.Contains(Id) || Cell.ReferencedFeatureIds.Contains(Id)) && Road.Representations.ContainsByPredicate([&](const auto& R){ return R.CellId==CellId && R.Kind==TEXT("road_fragment") && !R.Parts.IsEmpty(); });
			FProjectWorldRoadEmission Emission;
			if (!TestTrue(TEXT("The actual existing Road emitter produces its original prospective stored geometry."),ProjectWorldRoadEmission::Read(B,Cell,Settings,{Id},Emission,Error))) { AddError(Id+TEXT("/")+CellId+TEXT(": ")+Error); return false; }
			if (!TestTrue(TEXT("Native eligibility equals the declared source-fragment closure."),Emission.LocallyEligibleFeatureIds.Contains(Id)==Expected && Emission.LocallyEligibleFeatureIds.Num()==(Expected ? 1 : 0))) return false;
			if (!Expected) { if (!TestTrue(TEXT("Noncontributing cells emit no hidden Road faces."),Emission.Surface.Triangles.IsEmpty())) return false; continue; }
			if (!TestTrue(TEXT("Every required native Road/cell contribution is nonempty."),!Emission.Surface.Triangles.IsEmpty())) return false;
			auto Unit=MakeShared<FJsonObject>(); Unit->SetStringField(TEXT("cell_id"),CellId); Unit->SetStringField(TEXT("geometry_sha256"),Emission.GeometryHash);
			const auto& O=Emission.Surface.Origin; Unit->SetArrayField(TEXT("origin_cm"),Numbers({O.X,O.Y,O.Z}));
			TArray<TSharedPtr<FJsonValue>> Vertices,Indices; int32 Index=0;
			for (const auto& Face : Emission.Surface.Triangles)
			{
				for (const auto& P : Face.Points) Vertices.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
				Indices.Add(MakeShared<FJsonValueArray>(Numbers({double(Index),double(Index+1),double(Index+2)}))); Index+=3;
			}
			Unit->SetArrayField(TEXT("vertices_cm"),Vertices); Unit->SetArrayField(TEXT("triangles"),Indices); Units.Add(MakeShared<FJsonValueObject>(Unit)); Faces+=Emission.Surface.Triangles.Num();
		}
		if (!TestTrue(TEXT("The complete selected source Road has actual native output."),Faces>0)) return false;
		Result->SetArrayField(TEXT("native_units"),Units); Result->SetNumberField(TEXT("faces"),Faces); Roads.Add(MakeShared<FJsonValueObject>(Result)); Total+=Faces;
	}
	if (!Output.IsEmpty())
	{
		auto Root=MakeShared<FJsonObject>(); Root->SetStringField(TEXT("scope"),TEXT("Actual original-base Road emission. Only XY planform is evidence for inferred surface support; emitted base Z is not corrected-model height or gameplay acceptance."));
		Root->SetStringField(TEXT("base_result_sha256"),BaseHash); Root->SetStringField(TEXT("case_policy_sha256"),PolicyHash);
		Root->SetArrayField(TEXT("engine_georeference_origin_m"),Numbers({B.EngineGeoreferenceOriginMeters.X,B.EngineGeoreferenceOriginMeters.Y}));
		Root->SetNumberField(TEXT("height_origin_m"),B.HeightOriginMeters); Root->SetArrayField(TEXT("roads"),Roads); FString Text;
		if (!TestTrue(TEXT("The complete prospective Road footprint is written only to declared scratch."),FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text)) && FFileHelper::SaveStringToFile(Text,*Output))) return false;
	}
	AddInfo(FString::Printf(TEXT("NeighborhoodRoadPlanform roads=%d faces=%d; original XY only, corrected heights/full support/gameplay remain unverified"),Roads.Num(),Total)); return !HasAnyErrors();
}
#endif
