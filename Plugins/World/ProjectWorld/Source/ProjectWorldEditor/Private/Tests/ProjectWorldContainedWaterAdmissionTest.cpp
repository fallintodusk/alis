// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldWaterContractParsing.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldContainedWaterAdmissionTest,
    "Project.World.Water.ContainedFunctionAdmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldContainedWaterAdmissionTest::RunTest(const FString&)
{
    const FString Json=TEXT(R"json({"surface_group_id":"basin","surface_group_members":["way/1"],
      "surface_geometry":"polygon","surface_behavior":"standing","surface_role":"area",
      "surface_function":{"$schema":"https://alis.world/schemas/world-compiler/contained-water-v1.json",
      "function_id":"contained_ground_quantile","function_version":1,
      "vertical_basis":"qualified_ground_tin","accuracy":"derived_not_surveyed",
      "level_m":13.2,"spill_level_m":13.38,"sample_count":592,"quantile":0.5,"quantization_m":0.1,
      "ground_tin_sha256":"HASH","context_sha256":"HASH","wet_geometry_sha256":"HASH","baseline_function_sha256":"HASH",
      "mapped_geometry_sha256":"HASH","footprint_model":"mapped_containment"}})json");
    const FString Valid=Json.Replace(TEXT("HASH"),*FString::ChrN(64,TEXT('a')));
    const auto Read=[&](const FString& Text)
    {
        TSharedPtr<FJsonObject> Attributes;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Attributes)) return false;
        FProjectWorldCanonicalFeature Feature; Feature.FeatureClass=TEXT("water");
        FProjectWorldCanonicalValidation Validation;
        const bool Accepted=ProjectWorldWaterContractParsing::Read(Attributes,Feature,Validation);
        if (Accepted) TestTrue(TEXT("The consumed spill bound survives canonical admission"),
            Feature.WaterSurface.ContainmentSpillMeters.IsSet() && *Feature.WaterSurface.ContainmentSpillMeters==13.38);
        return Accepted;
    };
    TestTrue(TEXT("Closed contained Ground model is admitted"),Read(Valid));
    TestFalse(TEXT("Typed inferred-basin consumption cannot open canonical JSON admission"),Read(Valid.Replace(
        TEXT("contained_ground_quantile"),TEXT("identified_static_lined_basin"))));
    TestTrue(TEXT("Observed Ground footprint is a separately identified admitted model"),Read(Valid.Replace(
        TEXT("mapped_containment"),TEXT("observed_ground_component"))));
    TestFalse(TEXT("Unknown footprint policy refuses"),Read(Valid.Replace(TEXT("mapped_containment"),TEXT("ignore_source"))));
    TestFalse(TEXT("Spill equality cannot claim containment"),Read(Valid.Replace(TEXT("13.38"),TEXT("13.2"))));
    TestFalse(TEXT("A layer lift cannot be smuggled into the closed function"),Read(Valid.Replace(
        TEXT("\"level_m\":13.2"),TEXT("\"level_m\":13.2,\"offset_m\":0.25"))));
    TestFalse(TEXT("Unknown function version refuses"),Read(Valid.Replace(TEXT("\"function_version\":1"),TEXT("\"function_version\":2"))));
    TestFalse(TEXT("Numeric strings refuse"),Read(Valid.Replace(TEXT("\"level_m\":13.2"),TEXT("\"level_m\":\"13.2\""))));
    TestFalse(TEXT("Missing Ground provenance refuses"),Read(Valid.Replace(
        *FString::Printf(TEXT("\"ground_tin_sha256\":\"%s\","),*FString::ChrN(64,TEXT('a'))),TEXT(""))));
    TestFalse(TEXT("The model cannot become a flowing ribbon"),Read(Valid.Replace(TEXT("\"standing\""),TEXT("\"flowing\""))));
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldContainedWaterAdmissionTest,
    "Project.World.Water.ContainedFunctionAdmission","[Fast][Unit][World]")
#endif
