// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "ObjectTools.h"

#include "DefinitionGeneratorSubsystem.h"
#include "DefinitionTypeInfo.h"
#include "GenerateDefinitionsCommandlet.h"
#include "ProjectPaths.h"

namespace
{
FString ReadSourceJsonPath(UObject* Asset)
{
	if (!Asset)
	{
		return FString();
	}
	if (FStrProperty* Property = CastField<FStrProperty>(
			Asset->GetClass()->FindPropertyByName(TEXT("SourceJsonPath"))))
	{
		return Property->GetPropertyValue_InContainer(Asset);
	}
	return FString();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefinitionGeneratorSafetyTest,
	"ProjectIntegrationTests.DefinitionGenerator.Safety.SameIdRenameAndCleanupIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDefinitionGeneratorSafetyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UDefinitionGeneratorSubsystem* Generator = UDefinitionGeneratorSubsystem::Get();
	TestNotNull(TEXT("Definition generator subsystem should exist"), Generator);
	if (!Generator)
	{
		return false;
	}
	Generator->DiscoverAndRegisterManifests();

	FDefinitionTypeInfo BaseInfo;
	if (!Generator->GetDefinitionTypeInfo(TEXT("ExperienceDefinition"), BaseInfo))
	{
		AddError(TEXT("ExperienceDefinition type info should be registered"));
		return false;
	}

	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	const FString TestType = TEXT("DefinitionGeneratorSafetyTest_") + Suffix;
	const FString TestId = TEXT("ZzDefGenTest_") + Suffix;
	const FString SourceDirName = TEXT("DefinitionGeneratorSafetyTest_") + Suffix;
	const FString ContentPath = TEXT("/ProjectExperienceData/DefinitionGeneratorSafetyTest_") + Suffix;

	FDefinitionTypeInfo TypeInfo = BaseInfo;
	TypeInfo.SourceSubDir = SourceDirName;
	TypeInfo.GeneratedContentPath = ContentPath;
	TypeInfo.SourceFileGlob = TEXT("*.json");
	Generator->RegisterDefinitionType(TestType, TypeInfo);

	const FString SourceDir = Generator->GetSourceDirectoryForType(TestType);
	const FString PluginRoot = FPaths::GetPath(FProjectPaths::GetPluginDataDir(TypeInfo.PluginName));
	const FString ContentDir = PluginRoot / TEXT("Content") / SourceDirName;
	IFileManager& Files = IFileManager::Get();

	bool bCreatedSource = false;
	bool bCreatedContent = false;
	bool bCreatedAsset = false;
	auto Cleanup = [&]()
	{
		if (bCreatedAsset)
		{
			if (UObject* Asset = Generator->FindExistingAssetForType(TestType, TestId))
			{
				TArray<UObject*> AssetsToDelete;
				AssetsToDelete.Add(Asset);
				ObjectTools::DeleteObjects(AssetsToDelete, false);
			}
			bCreatedAsset = false;
		}
		if (bCreatedContent && Files.DirectoryExists(*ContentDir))
		{
			Files.DeleteDirectory(*ContentDir, true, true);
			bCreatedContent = false;
		}
		if (bCreatedSource && Files.DirectoryExists(*SourceDir))
		{
			Files.DeleteDirectory(*SourceDir, true, true);
		}
		bCreatedSource = false;
		Generator->UnregisterDefinitionType(TestType);
	};

	// Refuse a collision instead of deleting pre-existing project content.
	if (Files.DirectoryExists(*SourceDir) || Files.DirectoryExists(*ContentDir)
		|| Generator->FindExistingAssetForType(TestType, TestId) != nullptr)
	{
		AddError(FString::Printf(TEXT("Test fixture collision for unique type %s; refusing to delete it"), *TestType));
		Cleanup();
		return false;
	}

	const FString TemplatePath = FProjectPaths::GetPluginDataDir(BaseInfo.PluginName)
		/ BaseInfo.SourceSubDir
		/ TEXT("kazan_territory.json");
	FString Template;
	if (!FFileHelper::LoadFileToString(Template, *TemplatePath))
	{
		AddError(FString::Printf(TEXT("Failed to read test template: %s"), *TemplatePath));
		Cleanup();
		return false;
	}
	const FString RenameJson = Template.Replace(
		TEXT("\"ExperienceName\": \"KazanTerritory\""),
		*(FString(TEXT("\"ExperienceName\": \"")) + TestId + TEXT("\"")));
	TestFalse(TEXT("Test identity should replace the template identity"), RenameJson == Template);
	if (RenameJson == Template)
	{
		Cleanup();
		return false;
	}

	if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SourceDir))
	{
		AddError(FString::Printf(TEXT("Failed to create isolated source directory: %s"), *SourceDir));
		Cleanup();
		return false;
	}
	bCreatedSource = true;

	const FString OldPath = SourceDir / TEXT("old.json");
	const FString NewPath = SourceDir / TEXT("new.json");
	if (!FFileHelper::SaveStringToFile(RenameJson, *OldPath))
	{
		AddError(FString::Printf(TEXT("Failed to write isolated JSON fixture: %s"), *OldPath));
		Cleanup();
		return false;
	}

	UGenerateDefinitionsCommandlet* Commandlet = NewObject<UGenerateDefinitionsCommandlet>(
		GetTransientPackage(), NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("Commandlet should be constructible for editor automation"), Commandlet))
	{
		Cleanup();
		return false;
	}

	TestEqual(TEXT("Initial isolated generation should succeed"),
		Commandlet->GenerateType(TestType, false, false), 0);
	bCreatedContent = Files.DirectoryExists(*ContentDir);
	UObject* Asset = Generator->FindExistingAssetForType(TestType, TestId);
	if (!TestNotNull(TEXT("Isolated generated asset should exist"), Asset))
	{
		Cleanup();
		return false;
	}
	bCreatedAsset = true;
	const FString PackageBefore = Asset->GetOutermost()->GetName();
	TestEqual(TEXT("Recorded source path before rename"), ReadSourceJsonPath(Asset), FString(TEXT("old.json")));

	if (!Files.Move(*NewPath, *OldPath))
	{
		AddError(FString::Printf(TEXT("Failed to rename isolated JSON: %s -> %s"), *OldPath, *NewPath));
		Cleanup();
		return false;
	}

	// Invalid JSON immediately after the filename rename: the existing asset still
	// records old.json, so a cleanup that runs before failure counting would delete it.
	AddExpectedError(TEXT("Failed to parse JSON"), EAutomationExpectedErrorFlags::Contains, 2);
	const FString InvalidJson = FString::Printf(
		TEXT("{\"ExperienceName\": \"%s\", INVALID"), *TestId);
	if (!FFileHelper::SaveStringToFile(InvalidJson, *NewPath))
	{
		AddError(FString::Printf(TEXT("Failed to write invalid JSON fixture: %s"), *NewPath));
		Cleanup();
		return false;
	}
	TestTrue(TEXT("Invalid JSON generation should fail"),
		Commandlet->GenerateType(TestType, false, true) > 0);
	Asset = Generator->FindExistingAssetForType(TestType, TestId);
	TestNotNull(TEXT("Invalid JSON must not let cleanup delete the existing asset"), Asset);
	TestEqual(TEXT("Invalid JSON must preserve the pre-rename source path"),
		ReadSourceJsonPath(Asset), FString(TEXT("old.json")));

	// Restore valid JSON and verify the same-ID rename still refreshes metadata.
	if (!FFileHelper::SaveStringToFile(RenameJson, *NewPath))
	{
		AddError(FString::Printf(TEXT("Failed to restore valid JSON fixture: %s"), *NewPath));
		Cleanup();
		return false;
	}
	TestEqual(TEXT("Same-ID filename rename should succeed"),
		Commandlet->GenerateType(TestType, false, true), 0);
	Asset = Generator->FindExistingAssetForType(TestType, TestId);
	if (!TestNotNull(TEXT("Same-ID filename rename should preserve the generated asset"), Asset))
	{
		Cleanup();
		return false;
	}
	TestEqual(TEXT("Same-ID filename rename should preserve the package identity"),
		Asset->GetOutermost()->GetName(), PackageBefore);
	TestEqual(TEXT("Same-ID filename rename should update the recorded source path"),
		ReadSourceJsonPath(Asset), FString(TEXT("new.json")));

	// A missing source directory must fail safely without orphan cleanup.
	AddExpectedError(TEXT("Source directory is missing or unavailable"), EAutomationExpectedErrorFlags::Contains, 1);
	Files.DeleteDirectory(*SourceDir, true, true);
	bCreatedSource = false;
	TestTrue(TEXT("Missing source directory should fail"),
		Commandlet->GenerateType(TestType, false, true) > 0);
	Asset = Generator->FindExistingAssetForType(TestType, TestId);
	TestNotNull(TEXT("Missing source directory must not let cleanup delete the existing asset"), Asset);

	// With a clean, available (empty) source directory, cleanup may delete the orphan.
	if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SourceDir))
	{
		AddError(FString::Printf(TEXT("Failed to recreate isolated source directory: %s"), *SourceDir));
		Cleanup();
		return false;
	}
	bCreatedSource = true;
	TestEqual(TEXT("Clean empty source directory should allow orphan cleanup"),
		Commandlet->GenerateType(TestType, false, true), 0);
	TestNull(TEXT("Successful cleanup should remove the orphaned test asset"),
		Generator->FindExistingAssetForType(TestType, TestId));
	bCreatedAsset = false;

	Cleanup();
	return true;
}
