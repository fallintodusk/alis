// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedVerifyFixture.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "Materials/MaterialInterface.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedVerifyTest,
	"Project.World.Realization.Verify.ConstructedSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedVerifyTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedVerifyFixture;
	const auto Canonical=Bundle(); const auto Definition=Layer();
	return ProjectWorldVerifyTestFlow::Run(*this,Spec,
		[&](FProjectWorldVerifyIdentity& Identity,FString& Error)
		{
			Identity.AddFixtureBundle(TEXT("memory:bundle"),Canonical);
			Identity.AddFixtureText(TEXT("memory:layer"),ProjectWorldVerifyTestFlow::SerializeLayer(Definition));
			return Identity.AddFixtureObject(Material,Error);
		},
		[&](UWorld* World,FString& Error)
		{
			const auto* Producer=ProjectWorldLayerProducerRegistry::Find(Spec.GeneratorId,1,Error);
			if (!Producer || !ProjectWorldLayerProducerRegistry::ValidateLayer(Definition,Error)) return false;
			FProjectWorldRealizationProfile Profile; Profile.ProfileId=TEXT("verify_constructed"); Profile.Layers={Definition};
			FProjectWorldAuthoredOverlaySet Overlays; const FProjectWorldLayerInputs Inputs{Canonical,Profile,Definition,Overlays};
			const TArray<FString> Dirty{TEXT("*")}; TMap<FName,UMaterialInterface*> Materials;
			Materials.Add(TEXT("road"),LoadObject<UMaterialInterface>(nullptr,Material));
			const FProjectWorldLayerApplyContext Context{*World,Inputs,Dirty,Materials};
			auto Inventory=ProjectWorldVerifyTestFlow::WholeLayer(Definition); FProjectWorldRealizationResult Result;
			return Producer->Apply(Context,Inventory,Result,Error) && Producer->CaptureArtifacts(Context,Inventory,Result,Error);
		});
}
#endif
