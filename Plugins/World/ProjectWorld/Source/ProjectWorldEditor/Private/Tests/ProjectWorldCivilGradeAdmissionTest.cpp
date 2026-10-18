// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCivilGrade.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldCivilGradeAdmissionTest,
    "Project.World.Terrain.CivilGradeAdmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldCivilGradeAdmissionTest::RunTest(const FString& Parameters)
{
    const FString Hash = FString::ChrN(64, 'a');
    const FString Good = FString::Printf(TEXT("{\"civil_grade_reference\":{\"$schema\":\"https://alis.world/schemas/world-compiler/civil-grade-reference-v1.json\",\"version\":1,\"model\":\"preserve_accepted_grid_grade_not_structure_classification\",\"base_feature_sha256\":\"%s\",\"terrain_artifacts\":{\"cell\":\"%s\"}}}"), *Hash, *Hash);
    TSharedPtr<FJsonObject> Attributes;
    FProjectWorldCanonicalFeature Feature;
    Feature.FeatureClass = TEXT("building"); Feature.OwnerCellId = TEXT("cell");
    FProjectWorldCanonicalValidation Validation;
    TestTrue(TEXT("Reference JSON parses."), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Good), Attributes));
    TestTrue(TEXT("Closed reference is admitted."), ProjectWorldCivilGrade::Read(Attributes, Feature, Validation));
    FProjectWorldCanonicalBundle Bundle;
    auto& Cell = Bundle.Cells.AddDefaulted_GetRef(); Cell.CellId = TEXT("cell"); Cell.Terrain.ArtifactHash = Hash;
    FString Identity, Error;
    TestTrue(TEXT("An unwrapped grid has the same base/final identity."), ProjectWorldCivilGrade::Validate(Bundle, Feature, Identity, Error));
    const FString MissingTags = Good.Replace(TEXT("{\"civil_grade_reference\":"),
        TEXT("{\"surface_context\":{\"ground_relation\":\"unknown\",\"ford\":\"unknown\",\"tunnel\":\"unknown\",\"unresolved\":[]},\"civil_grade_reference\":"));
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MissingTags), Attributes);
    TestTrue(TEXT("Genuinely missing tags retain the explicit preservation model."), ProjectWorldCivilGrade::Read(Attributes, Feature, Validation));
    for (const auto& Bad : {Good.Replace(TEXT("\"version\":1"), TEXT("\"version\":true")),
        Good.Replace(TEXT("\"version\":1"), TEXT("\"version\":2")),
        Good.Replace(TEXT("\"model\":"), TEXT("\"unexpected\":")),
        Good.Replace(TEXT("\"terrain_artifacts\":{"), TEXT("\"terrain_artifacts\":null,\"extra\":{")),
        Good.Replace(TEXT("{\"civil_grade_reference\":"), TEXT("{\"surface_context\":{\"ground_relation\":\"at_ground\",\"ford\":\"present\",\"tunnel\":\"absent\"},\"civil_grade_reference\":"))})
    {
        Feature.CivilGrade.Reset(); Validation = {};
        TestTrue(TEXT("Known-bad JSON is syntactically valid."), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Bad), Attributes));
        TestFalse(TEXT("Unsupported/malformed or contradictory reference refuses without a query fallback."), ProjectWorldCivilGrade::Read(Attributes, Feature, Validation));
        TestFalse(TEXT("Rejected reference stays absent."), Feature.CivilGrade.IsSet());
    }
    for (const auto* Marker : {TEXT("unsupported_ford"), TEXT("unsupported_tunnel"), TEXT("unsupported_bridge"),
        TEXT("crossing_mode_required"), TEXT("conflicting_bridge_tunnel")})
    {
        const FString Bad = MissingTags.Replace(TEXT("\"unresolved\":[]"), *FString::Printf(TEXT("\"unresolved\":[\"%s\"]"), Marker));
        Feature.CivilGrade.Reset(); Validation = {};
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Bad), Attributes);
        TestFalse(TEXT("Explicit unsupported crossing is not silently reclassified as missing metadata."), ProjectWorldCivilGrade::Read(Attributes, Feature, Validation));
    }
    Feature.FeatureClass = TEXT("water"); Validation = {};
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Good), Attributes);
    TestFalse(TEXT("Wrong-class reference refuses."), ProjectWorldCivilGrade::Read(Attributes, Feature, Validation));
    Feature.FeatureClass = TEXT("building");
    for (const auto* Value : {TEXT("1"), TEXT("true"), TEXT("null")})
    {
        const FString Bad = MissingTags.Replace(TEXT("\"unresolved\":[]"), *FString::Printf(TEXT("\"unresolved\":[%s]"), Value));
        Feature.CivilGrade.Reset(); Validation = {};
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Bad), Attributes);
        TestFalse(TEXT("Unresolved evidence requires actual JSON strings, without native coercion."), ProjectWorldCivilGrade::Read(Attributes, Feature, Validation));
    }
    return true;
}
#endif
