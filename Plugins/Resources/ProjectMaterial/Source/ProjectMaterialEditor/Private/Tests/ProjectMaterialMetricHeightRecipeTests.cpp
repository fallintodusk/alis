// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialSurfaceRecipe.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialMetricHeightRecipeTest,
    "Project.Material.HeightMasks.MetricRecipe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialMetricHeightRecipeTest::RunTest(const FString& Parameters)
{
    const FString Root=FPaths::Combine(FPaths::ProjectPluginsDir(),TEXT("Resources/ProjectMaterial/Data/TestFixtures"));
    const FString Path=FPaths::Combine(Root,TEXT("MetricHeight/M_ProjectMetricHeightFixture.surface.json"));
    FString Json,Error;
    FProjectMaterialSurfaceRecipe Recipe;
    if (!TestTrue(TEXT("Read owned metric recipe"),FFileHelper::LoadFileToString(Json,*Path))) return false;
    if (!TestTrue(TEXT("Approved metric composition parses"),FProjectMaterialSurfaceRecipeContract::Parse(Json,Path,Root,Recipe,Error)))
    {AddError(Error); return false;}
    TestEqual(TEXT("Metric projection is explicit"),Recipe.ProjectionFrame,EProjectMaterialProjectionFrame::WorldMetricXY2D);
    TestEqual(TEXT("Sixteen consumed appearance controls"),Recipe.Scalars.Num(),16);
    TestEqual(TEXT("No invented canonical semantic channels"),Recipe.SemanticBindings.Num(),0);
    auto Invalid=Json.Replace(TEXT("\"MacroBlendEndMeters\": 18.0"),TEXT("\"MacroBlendEndMeters\": 5.0"));
    TestFalse(TEXT("Zero distance interval refuses"),FProjectMaterialSurfaceRecipeContract::Parse(Invalid,Path,Root,Recipe,Error));
    Invalid=Json.Replace(TEXT("\"SecondaryScaleRatio\": 1.73"),TEXT("\"SecondaryScaleRatio\": 0.0"));
    TestFalse(TEXT("Zero secondary metric scale refuses"),FProjectMaterialSurfaceRecipeContract::Parse(Invalid,Path,Root,Recipe,Error));
    return true;
}
#endif
