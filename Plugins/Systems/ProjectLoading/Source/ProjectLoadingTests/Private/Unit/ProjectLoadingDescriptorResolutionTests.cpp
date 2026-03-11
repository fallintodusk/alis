// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "LoadingTestExperienceDescriptor.h"
#include "Types/ProjectLoadRequest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Forward declaration from ProjectLoadingSubsystem.cpp (test-only helper)
PROJECTLOADING_API FLoadRequest BuildResolvedLoadRequest_ForTests(const UProjectExperienceDescriptorBase& Descriptor);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoading_DescriptorResolution_UsesSoftPath,
	"ProjectLoading.DescriptorResolution.UsesSoftPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoading_DescriptorResolution_UsesSoftPath::RunTest(const FString& Parameters)
{
	ULoadingTestExperienceDescriptor* Descriptor = NewObject<ULoadingTestExperienceDescriptor>();
	const FLoadRequest Request = BuildResolvedLoadRequest_ForTests(*Descriptor);

	TestEqual(TEXT("ExperienceName propagated"), Request.ExperienceName, FName(TEXT("TestLoading")));
	TestEqual(TEXT("MapSoftPath propagated"), Request.MapSoftPath.ToString(), Descriptor->LoadAssets.Map.ToSoftObjectPath().ToString());

	// Without AssetManager scan, MapAssetId may remain invalid; ensure we do not fabricate IDs.
	TestFalse(TEXT("MapAssetId remains invalid when not scanned"), Request.MapAssetId.IsValid());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
