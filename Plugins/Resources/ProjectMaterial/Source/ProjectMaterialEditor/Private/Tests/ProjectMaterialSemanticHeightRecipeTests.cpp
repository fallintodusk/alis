// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialSurfaceRecipe.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialSemanticHeightRecipeTest,
    "Project.Material.HeightMasks.SemanticTerrainRecipe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectMaterialSemanticHeightRecipeTest,
    "Project.Material.HeightMasks.SemanticTerrainRecipe", "[Fast][Unit][ProjectMaterial]")

bool FProjectMaterialSemanticHeightRecipeTest::RunTest(const FString&)
{
    const FString Root=FPaths::ProjectPluginsDir()/TEXT("Resources/ProjectMaterial/Data/TestFixtures");
    const FString Path=Root/TEXT("SemanticMetricHeight/M_ProjectSemanticHeightFixture.surface.json");
    FString Json, Error;
    FProjectMaterialSurfaceRecipe Recipe;
    if (!TestTrue(TEXT("Read semantic terrain recipe"),FFileHelper::LoadFileToString(Json,*Path))) return false;
    if (!TestTrue(TEXT("Frozen soil detail composes with declared terrain semantics"),
        FProjectMaterialSurfaceRecipeContract::Parse(Json,Path,Root,Recipe,Error)))
    {AddError(Error); return false;}
    TestTrue(TEXT("Terrain layout authentication is required"),FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(Recipe.Archetype));
    TestEqual(TEXT("Physical soil projection remains XY"),Recipe.ProjectionFrame,EProjectMaterialProjectionFrame::WorldMetricXY2D);
    TestEqual(TEXT("Existing semantic ABI has two channels"),Recipe.SemanticBindings.Num(),2);
    TestEqual(TEXT("Existing soil controls plus hydro control"),Recipe.Scalars.Num(),17);
    const FString Invalid=Json.Replace(TEXT("\"hydro_transition\""),TEXT("\"invented_water_mask\""));
    TestFalse(TEXT("Unknown semantic channel refuses"),FProjectMaterialSurfaceRecipeContract::Parse(Invalid,Path,Root,Recipe,Error));
    return true;
}
#endif
