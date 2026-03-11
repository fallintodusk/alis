// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "TestExperienceDescriptor.h"
#include "Experience/ProjectExperienceRegistry.h"
#include "Types/ProjectLoadRequest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ExperienceDescriptor_BuildLoadRequest,
	"ProjectCore.ExperienceDescriptor.BuildLoadRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ExperienceDescriptor_BuildLoadRequest::RunTest(const FString& Parameters)
{
	UTestExperienceDescriptor* Descriptor = NewObject<UTestExperienceDescriptor>();
	FLoadRequest Request;
	Descriptor->BuildLoadRequest(Request);

	TestEqual(TEXT("ExperienceName copied"), Request.ExperienceName, Descriptor->ExperienceName);
	TestEqual(TEXT("MapSoftPath copied"), Request.MapSoftPath.ToString(), Descriptor->LoadAssets.Map.ToSoftObjectPath().ToString());
	TestFalse(TEXT("MapAssetId should remain invalid in data-only BuildLoadRequest"), Request.MapAssetId.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ExperienceDescriptor_Registry,
	"ProjectCore.ExperienceDescriptor.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ExperienceDescriptor_Registry::RunTest(const FString& Parameters)
{
	UProjectExperienceRegistry* Registry = UProjectExperienceRegistry::Get();
	TestNotNull(TEXT("Registry should be available"), Registry);

	UTestExperienceDescriptor* Descriptor = NewObject<UTestExperienceDescriptor>();
	Registry->RegisterDescriptor(Descriptor);
	UProjectExperienceDescriptorBase* Found = Registry->FindDescriptor(TEXT("TestExperience"));

	TestNotNull(TEXT("Descriptor should be registered and retrievable"), Found);
	TestEqual(TEXT("Retrieved descriptor name matches"), Found->ExperienceName, FName(TEXT("TestExperience")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ExperienceDescriptor_ScanSpecs,
	"ProjectCore.ExperienceDescriptor.ScanSpecs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ExperienceDescriptor_ScanSpecs::RunTest(const FString& Parameters)
{
	UTestExperienceDescriptor* Descriptor = NewObject<UTestExperienceDescriptor>();
	TArray<FExperienceAssetScanSpec> Specs;
	Descriptor->GetAssetScanSpecs(Specs);

	TestEqual(TEXT("One scan spec returned"), Specs.Num(), 1);
	if (Specs.Num() == 1)
	{
		const FExperienceAssetScanSpec& Spec = Specs[0];
		TestEqual(TEXT("Type is Map"), Spec.PrimaryAssetType, FName(TEXT("Map")));
		TestTrue(TEXT("BaseClass is UWorld"), Spec.BaseClass.Get() == UWorld::StaticClass());
		TestEqual(TEXT("Directories count"), Spec.Directories.Num(), 1);
		if (Spec.Directories.Num() == 1)
		{
			TestEqual(TEXT("Directory path matches"), Spec.Directories[0], TEXT("/PluginTest/Maps"));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
