#include "ProjectWorldLayerProducerRegistry.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBuildingSelectionSettingsTest,
    "Project.World.BuildingAssembly.SelectionSettings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldBuildingSelectionSettingsTest::RunTest(const FString&)
{
    FString Error;
    const auto* Producer=ProjectWorldLayerProducerRegistry::Find(TEXT("project_building_massing"),2,Error);
    if (!TestNotNull(TEXT("Existing buildings lifecycle owns modular selection."),Producer)) return false;
    const FString Base=TEXT("{\"maximum_height_m\":300,\"terrain_anchor_policy\":\"owner_cell_clamped_bounds_center\","
        "\"topology_policy\":\"logical_building_classify_v2\",\"duplicate_policy\":\"stable_feature_id\","
        "\"contained_policy\":\"associate_with_container\",\"conflict_policy\":\"reject_affected_fragments\","
        "\"nanite\":true,\"collision\":\"complex_as_simple\",\"navigation\":\"no_navigation\"");
    TestTrue(TEXT("Existing massing settings remain executable."),Producer->ValidateSettings(Base+TEXT("}"),Error));
    TestTrue(TEXT("One stable selection document is accepted by the normal producer."),
        Producer->ValidateSettings(Base+TEXT(",\"assembly_source\":\"BuildingAssembly/neighborhood.json\"}"),Error));
    TestFalse(TEXT("Selection cannot escape the data owner."),
        Producer->ValidateSettings(Base+TEXT(",\"assembly_source\":\"../outside.json\"}"),Error));
    TestFalse(TEXT("Unknown settings remain closed."),
        Producer->ValidateSettings(Base+TEXT(",\"unknown\":true}"),Error));
    return true;
}
#endif
