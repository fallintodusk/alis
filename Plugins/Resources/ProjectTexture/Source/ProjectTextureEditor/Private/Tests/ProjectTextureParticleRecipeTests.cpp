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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureParticleRecipeTest,"Project.Texture.Particles.RecipeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectTextureParticleRecipeTest::RunTest(const FString&)
{
	const FString Root=FPaths::Combine(FPaths::ProjectPluginsDir(),TEXT("Resources/ProjectTexture/Data/TestFixtures"));
	const FString Path=FPaths::Combine(Root,TEXT("Aggregate/project_asphalt_fixture.pattern.json"));
	FString Json,Error; FProjectTexturePatternRecipe Default;
	if (!FFileHelper::LoadFileToString(Json,*Path) || !FProjectTexturePatternRecipeContract::Parse(Json,Path,Root,Default,Error))
	{ AddError(Error); return false; }
	TestEqual(TEXT("All admitted structural controls are explicit"),Default.StructureControls.Num(),22);
	for (const auto& Control : Default.StructureControls)
	{
		TSharedPtr<FJsonObject> Object; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object);
		Object->GetObjectField(TEXT("structure"))->SetNumberField(Control.Key.ToString(),
			Control.Key.ToString().EndsWith(TEXT("Frequency")) ? FMath::Max(1.,Control.Value/2) : Control.Value*.75);
		FString Changed; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Changed));
		FProjectTexturePatternRecipe Other;
		if (!TestTrue(TEXT("A bounded structural variation parses"),FProjectTexturePatternRecipeContract::Parse(Changed,Path,Root,Other,Error))) return false;
		TestNotEqual(TEXT("Changed controls alter recipe identity"),Other.RecipeSha256,Default.RecipeSha256);
		for (int32 I=0; I<2; ++I) TestNotEqual(TEXT("The coherent bundle identity includes consumed controls"),Other.Nodes[I].StructuralIdentity,Default.Nodes[I].StructuralIdentity);
	}
	auto Reject=[&](const TCHAR* Key,double Value)
	{
		TSharedPtr<FJsonObject> Object; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object);
		Object->GetObjectField(TEXT("structure"))->SetNumberField(Key,Value);
		FString Bad; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Bad));
		FProjectTexturePatternRecipe Other;
		TestFalse(TEXT("Unknown, out-of-range or fractional-period control is rejected"),FProjectTexturePatternRecipeContract::Parse(Bad,Path,Root,Other,Error));
	};
	Reject(TEXT("UnusedColor"),.5); Reject(TEXT("FineFrequency"),96.5); Reject(TEXT("WarpAmplitude"),1.0);
	return true;
}
#endif
