// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldShoreTransitionParsing.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldShoreTransitionTest
{
    using FObject=TSharedPtr<FJsonObject>;
    FObject Parse(const FString& Text)
    {FObject O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O);return O;}
    FObject Object() {return MakeShared<FJsonObject>();}
    bool Fixture(FObject& Input,FObject& Operation,FObject& Wrapper)
    {
        FString Text;if (!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("tools/World/CanonicalCompilation/tests/fixtures/pier_shore_transition.json")))) return false;
        const auto Capture=Parse(Text);if (!Capture) return false;
        const auto Cell=Capture->GetObjectField(TEXT("cell")),Water=Capture->GetObjectField(TEXT("water"));
        const FString Grid=Cell->GetStringField(TEXT("grid_id")),Id=Cell->GetStringField(TEXT("cell_id")),WaterId=Water->GetStringField(TEXT("feature_id"));
        const FString GroundHash=FString::ChrN(64,'a'),WaterHash=FString::ChrN(64,'b');
        Input=Object();Input->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/shore-transition-input-v1.json"));
        Input->SetNumberField(TEXT("schema_version"),1);Input->SetStringField(TEXT("grid_id"),Grid);
        Input->SetStringField(TEXT("ground_input_sha256"),GroundHash);Input->SetStringField(TEXT("water_feature_id"),WaterId);
        Input->SetStringField(TEXT("base_water_sha256"),WaterHash);Input->SetStringField(TEXT("effective_water_sha256"),WaterHash);
        Input->SetObjectField(TEXT("effective_water"),Water);Input->SetObjectField(TEXT("parameters"),Capture->GetObjectField(TEXT("parameters")));
        auto Selected=Object();Selected->SetArrayField(Id,{MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(0),MakeShared<FJsonValueNumber>(0)})});
        Input->SetObjectField(TEXT("selected"),Selected);
        auto Provenance=Object();Provenance->SetStringField(TEXT("basis"),TEXT("authored_game_world_transition_not_surveyed"));
        Provenance->SetStringField(TEXT("description"),TEXT("Captured-input metadata boundary; geometry acceptance is separate."));Input->SetObjectField(TEXT("provenance"),Provenance);
        Operation=Object();Operation->SetStringField(TEXT("kind"),TEXT("inferred_shore_transition"));Operation->SetObjectField(TEXT("input"),Object());
        Operation->SetArrayField(TEXT("feature_ids"),{MakeShared<FJsonValueString>(WaterId)});
        auto Base=Object();Base->SetStringField(WaterId,WaterHash);Operation->SetObjectField(TEXT("base_features"),Base);
        Wrapper=Object();Wrapper->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/terrain-refinement-v1.json"));
        Wrapper->SetNumberField(TEXT("schema_version"),1);Wrapper->SetStringField(TEXT("grid_id"),Grid);Wrapper->SetStringField(TEXT("cell_id"),Id);
        Wrapper->SetObjectField(TEXT("base"),Object());Wrapper->SetStringField(TEXT("coordinate_rule"),TEXT("canonical_boundary_intersections_float64"));
        Wrapper->SetStringField(TEXT("construction"),TEXT("inferred_east_bank_v1"));
        auto Consumed=Object();Consumed->SetStringField(TEXT("shore_input_sha256"),FString::ChrN(64,'c'));
        Consumed->SetStringField(TEXT("ground_input_sha256"),GroundHash);Consumed->SetStringField(TEXT("water_feature_id"),WaterId);
        Consumed->SetStringField(TEXT("base_water_sha256"),WaterHash);Consumed->SetStringField(TEXT("water_sha256"),WaterHash);
        for (const TCHAR* K:{TEXT("domain_sha256"),TEXT("implementation_sha256"),TEXT("execution_identity")}) Consumed->SetStringField(K,FString::ChrN(64,'d'));
        Wrapper->SetObjectField(TEXT("consumed"),Consumed);Wrapper->SetObjectField(TEXT("policy"),Input->GetObjectField(TEXT("parameters")));
        auto Q=Parse(TEXT(R"({"coordinate":[0,0],"vertices":[[0,30,-1,1,0],[30,30,-1,1,0],[0,0,-1,1,0],[30,0,-1,1,0]],"triangles":[[0,2,1],[1,2,3]],"water_triangles":[0,1]})"));
        Wrapper->SetArrayField(TEXT("quads"),{MakeShared<FJsonValueObject>(Q)});
        Wrapper->SetObjectField(TEXT("metrics"),Parse(TEXT(R"({"affected_quads":1,"triangles":2,"maximum_bank_gradient":0,"maximum_restore_m":0,"wet_height_upper_m":-1})")));
        return true;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldShoreTransitionInputTest,"Project.World.Terrain.ShoreTransitionInput",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldShoreTransitionInputTest::RunTest(const FString&)
{
    using namespace ProjectWorldShoreTransitionTest;using namespace ProjectWorldShoreTransitionParsing;
    FObject Input,Operation,Wrapper;if (!TestTrue(TEXT("The tracked captured input loads."),Fixture(Input,Operation,Wrapper))) return false;
    const FString Hash=FString::ChrN(64,'c'),Grid=Input->GetStringField(TEXT("grid_id")),Cell=Wrapper->GetStringField(TEXT("cell_id"));
    FInputs Inputs;TestTrue(TEXT("The closed retained role is admitted."),ReadInput(Operation,Input,Hash,Inputs));
    TestTrue(TEXT("The exact selected wrapper binds once."),ReadTerrain(Wrapper,Grid,Cell,Inputs));
    TestFalse(TEXT("Repeated cell use refuses."),ReadTerrain(Wrapper,Grid,Cell,Inputs));
    FString DependencyValue;TestTrue(TEXT("The exact Ground hash binds the additional dependency."),Dependency(Wrapper,FString::ChrN(64,'a'),DependencyValue));
    TestTrue(TEXT("Dependency is a complete SHA256 role."),DependencyValue.StartsWith(TEXT("refinement:")) && DependencyValue.Len()==75);
    TestFalse(TEXT("Another Ground input cannot claim the wrapper."),Dependency(Wrapper,FString::ChrN(64,'e'),DependencyValue));
    auto Features=Object();Features->SetArrayField(TEXT("features"),{MakeShared<FJsonValueObject>(Input->GetObjectField(TEXT("effective_water")))});
    TestTrue(TEXT("The retained actual Water snapshot matches."),ValidateSources(Features,Inputs));
    FObject OtherInput,OtherOperation,OtherWrapper;Fixture(OtherInput,OtherOperation,OtherWrapper);
    OtherInput->GetObjectField(TEXT("effective_water"))->GetObjectField(TEXT("attributes"))->GetObjectField(TEXT("surface_function"))->SetNumberField(TEXT("level_m"),0);
    Features->SetArrayField(TEXT("features"),{MakeShared<FJsonValueObject>(OtherInput->GetObjectField(TEXT("effective_water")))});
    TestFalse(TEXT("A resealed changed actual Water stage refuses."),ValidateSources(Features,Inputs));
    for (const TCHAR* Defect:{TEXT("ground"),TEXT("water"),TEXT("extra_quad"),TEXT("policy")})
    {
        Fixture(Input,Operation,Wrapper);FInputs Fresh;ReadInput(Operation,Input,Hash,Fresh);
        if (FString(Defect)==TEXT("ground")) Wrapper->GetObjectField(TEXT("consumed"))->SetStringField(TEXT("ground_input_sha256"),FString::ChrN(64,'e'));
        if (FString(Defect)==TEXT("water")) Wrapper->GetObjectField(TEXT("consumed"))->SetStringField(TEXT("water_sha256"),FString::ChrN(64,'e'));
        if (FString(Defect)==TEXT("extra_quad")) {auto Quads=Wrapper->GetArrayField(TEXT("quads"));const auto Duplicate=Quads[0];Quads.Add(Duplicate);Wrapper->SetArrayField(TEXT("quads"),Quads);}
        if (FString(Defect)==TEXT("policy")) Wrapper->GetObjectField(TEXT("metrics"))->SetNumberField(TEXT("maximum_bank_gradient"),1);
        TestFalse(FString::Printf(TEXT("Owned metadata refuses %s."),Defect),ReadTerrain(Wrapper,Grid,Cell,Fresh));
    }
    Fixture(Input,Operation,Wrapper);FProjectWorldCanonicalTerrain Terrain;FProjectWorldCanonicalTerrainQuad Quad;
    Quad.Vertices={FVector(0,30,-1),FVector(30,30,-1),FVector(0,0,-1),FVector(30,0,-1)};
    Quad.Triangles={FIntVector(0,2,1),FIntVector(1,2,3)};Terrain.RefinedQuads.Add(Quad);
    TestTrue(TEXT("The encoded flat control respects its grade."),ValidateEncoded(Wrapper,Terrain));
    Terrain.RefinedQuads[0].Vertices[0].Z=100;
    TestFalse(TEXT("Actual encoded steep triangles refuse despite unchanged green metrics."),ValidateEncoded(Wrapper,Terrain));
    FProjectWorldCanonicalBundle Bundle;FProjectWorldCanonicalCell C;C.CellId=Cell;C.Terrain.SampleSpacing=FVector2D(30,30);
    Quad.Coordinate=FIntPoint(2,0);Quad.Vertices={FVector(583363.41,4508262.45,-1),FVector(583393.41,4508262.45,-1),
        FVector(583363.41,4508232.45,-1),FVector(583393.41,4508232.45,-1),FVector(583378.41,4508247.45,-1)};
    Quad.Triangles={FIntVector(0,2,4),FIntVector(2,3,4),FIntVector(3,1,4),FIntVector(1,0,4)};C.Terrain.RefinedQuads={Quad};Bundle.Cells={C};
    FInput WetInput;WetInput.Selected.Add(Cell,{FIntPoint(2,0)});FProjectWorldCanonicalFeature Water;Water.WaterSurface.LevelMeters=-1;
    FProjectWorldCanonicalPolygon Polygon;
    const auto Ring=Input->GetObjectField(TEXT("effective_water"))->GetObjectField(TEXT("geometry"))->GetArrayField(TEXT("coordinates"))[0]->AsArray();
    for (const auto& P:Ring) Polygon.Outer.Add(FVector2D(P->AsArray()[0]->AsNumber(),P->AsArray()[1]->AsNumber()));Water.GeometryPolygons.Add(Polygon);
    double ActualWet=0;TestTrue(TEXT("The complete encoded wet field matches the actual footprint and stage."),ValidateWetField(WetInput,Water,Bundle,ActualWet));
    TestEqual(TEXT("Wet height is measured from actual XYZ, not its green metric."),ActualWet,-1.);
    Bundle.Cells[0].Terrain.RefinedQuads[0].Vertices[4].Z+=1e-9;
    TestFalse(TEXT("A one-nanometre interior wet fault refuses with unchanged exterior and metrics."),ValidateWetField(WetInput,Water,Bundle,ActualWet));
    return !HasAnyErrors();
}
#endif
