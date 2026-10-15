// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTexturePatternRecipe.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureGradedRecipeTest,"Project.Texture.GradedParticles.RecipeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTextureGradedRecipeTest,"Project.Texture.GradedParticles.RecipeContract",
    "[Fast][Unit][ProjectTexture]")
bool FProjectTextureGradedRecipeTest::RunTest(const FString&)
{
    const FString Root=FPaths::Combine(FPaths::ProjectPluginsDir(),TEXT("Resources/ProjectTexture/Data/TestFixtures"));
    const FString Path=FPaths::Combine(Root,TEXT("GradedParticles/project_graded_fixture.pattern.json"));
    FString Json,Error; FProjectTexturePatternRecipe Reference;
    if (!TestTrue(TEXT("Tracked graded-particle recipe loads"),FFileHelper::LoadFileToString(Json,*Path)) ||
        !TestTrue(TEXT("Four-stage source with three exports is admitted"),FProjectTexturePatternRecipeContract::Parse(Json,Path,Root,Reference,Error)))
    { AddError(Error); return false; }
    TestEqual(TEXT("Raw intermediate is owned inside the DAG"),Reference.Nodes.Num(),4);
    TestEqual(TEXT("Public bundle stays three textures"),Reference.Exports.Num(),3);
    TestEqual(TEXT("All structural controls are explicit"),Reference.StructureControls.Num(),23);
    for (const auto& Control : Reference.StructureControls)
    {
        TSharedPtr<FJsonObject> Object; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object);
        Object->GetObjectField(TEXT("structure"))->SetNumberField(Control.Key.ToString(),Control.Value*.5);
        FString Changed; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Changed));
        FProjectTexturePatternRecipe Candidate;
        if (!TestTrue(TEXT("Bounded variation parses"),FProjectTexturePatternRecipeContract::Parse(Changed,Path,Root,Candidate,Error))) return false;
        for (int32 I=0;I<4;++I) TestNotEqual(TEXT("Consumed controls change the complete DAG identity"),Candidate.Nodes[I].StructuralIdentity,Reference.Nodes[I].StructuralIdentity);
    }
    auto Reject=[&](const TCHAR* Field,double Value)
    {
        TSharedPtr<FJsonObject> Object; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object);
        Object->GetObjectField(TEXT("structure"))->SetNumberField(Field,Value);
        FString Bad; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Bad));
        FProjectTexturePatternRecipe Candidate;
        TestFalse(TEXT("Unconsumed, invalid or unsupported-period controls refuse"),FProjectTexturePatternRecipeContract::Parse(Bad,Path,Root,Candidate,Error));
    };
    Reject(TEXT("UnusedColor"),.5); Reject(TEXT("LargeFrequency"),15); Reject(TEXT("GrainHeight"),2);
    return true;
}
#endif
