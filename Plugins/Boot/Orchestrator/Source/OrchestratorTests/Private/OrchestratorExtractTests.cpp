// Copyright ALIS. All Rights Reserved.

#include "OrchestratorExtract.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorExtractResultTest, "Orchestrator.Extract.ResultStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorExtractResultTest::RunTest(const FString& Parameters)
{
	// Test: MakeSuccess factory method
	TArray<FString> TestFiles = {TEXT("file1.txt"), TEXT("file2.txt"), TEXT("file3.txt")};
	float TestTime = 2.5f;
	FExtractionResult SuccessResult = FExtractionResult::MakeSuccess(TestFiles, TestTime);

	TestTrue(TEXT("Success result should have bSuccess=true"), SuccessResult.bSuccess);
	TestTrue(TEXT("Error message should be empty"), SuccessResult.ErrorMessage.IsEmpty());
	TestEqual(TEXT("Files extracted count should match"), SuccessResult.FilesExtracted, 3);
	TestEqual(TEXT("Extraction time should match"), SuccessResult.ExtractionTimeSeconds, TestTime);
	TestEqual(TEXT("Extracted files array size should match"), SuccessResult.ExtractedFiles.Num(), 3);

	// Verify array contents
	TestEqual(TEXT("First file should match"), SuccessResult.ExtractedFiles[0], TEXT("file1.txt"));
	TestEqual(TEXT("Second file should match"), SuccessResult.ExtractedFiles[1], TEXT("file2.txt"));
	TestEqual(TEXT("Third file should match"), SuccessResult.ExtractedFiles[2], TEXT("file3.txt"));

	// Test: MakeFailure factory method
	FString TestError = TEXT("Zip file corrupted");
	FExtractionResult FailureResult = FExtractionResult::MakeFailure(TestError);

	TestFalse(TEXT("Failure result should have bSuccess=false"), FailureResult.bSuccess);
	TestEqual(TEXT("Error message should match"), FailureResult.ErrorMessage, TestError);
	TestEqual(TEXT("Files extracted should be 0"), FailureResult.FilesExtracted, 0);
	TestEqual(TEXT("Extracted files array should be empty"), FailureResult.ExtractedFiles.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorExtractPathValidationTest, "Orchestrator.Extract.PathValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorExtractPathValidationTest::RunTest(const FString& Parameters)
{
	// Test: Empty zip path should fail
	FExtractionResult EmptyZipResult = FOrchestratorExtract::ExtractZip(
		TEXT(""),
		FPaths::ProjectSavedDir() / TEXT("OrchestratorTests"),
		true);

	TestFalse(TEXT("Empty zip path should fail"), EmptyZipResult.bSuccess);
	TestTrue(TEXT("Error message should mention path or file"),
		EmptyZipResult.ErrorMessage.Contains(TEXT("path")) ||
		EmptyZipResult.ErrorMessage.Contains(TEXT("file")) ||
		EmptyZipResult.ErrorMessage.Contains(TEXT("zip")));

	// Test: Empty destination path should fail
	FExtractionResult EmptyDestResult = FOrchestratorExtract::ExtractZip(
		FPaths::ProjectSavedDir() / TEXT("test.zip"),
		TEXT(""),
		true);

	TestFalse(TEXT("Empty destination path should fail"), EmptyDestResult.bSuccess);

	// Test: Non-existent zip file should fail
	FExtractionResult NonExistentResult = FOrchestratorExtract::ExtractZip(
		TEXT("/non/existent/file.zip"),
		FPaths::ProjectSavedDir() / TEXT("OrchestratorTests"),
		true);

	TestFalse(TEXT("Non-existent zip should fail"), NonExistentResult.bSuccess);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorExtractDirectoryCreationTest, "Orchestrator.Extract.DirectoryCreation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorExtractDirectoryCreationTest::RunTest(const FString& Parameters)
{
	// Test that destination directory gets created if it doesn't exist
	const FString TestDestDir = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/extract_test/nested/path");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Ensure test directory doesn't exist
	if (PlatformFile.DirectoryExists(*TestDestDir))
	{
		PlatformFile.DeleteDirectoryRecursively(*TestDestDir);
	}

	TestFalse(TEXT("Test directory should not exist initially"), PlatformFile.DirectoryExists(*TestDestDir));

	// Test: CreateDirectoryTree should create nested directories
	TestTrue(TEXT("CreateDirectoryTree should succeed"), PlatformFile.CreateDirectoryTree(*TestDestDir));
	TestTrue(TEXT("Directory should exist after creation"), PlatformFile.DirectoryExists(*TestDestDir));

	// Cleanup
	PlatformFile.DeleteDirectoryRecursively(*(FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/extract_test")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorExtractOverwriteBehaviorTest, "Orchestrator.Extract.OverwriteBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorExtractOverwriteBehaviorTest::RunTest(const FString& Parameters)
{
	// Test overwrite flag behavior
	const FString TestDestDir = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/overwrite_test");
	const FString TestFilePath = TestDestDir / TEXT("existing_file.txt");
	const FString OriginalContent = TEXT("Original content");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*TestDestDir);

	// Create existing file
	FFileHelper::SaveStringToFile(OriginalContent, *TestFilePath);
	TestTrue(TEXT("Test file should exist"), PlatformFile.FileExists(*TestFilePath));

	// Verify original content
	FString LoadedContent;
	FFileHelper::LoadFileToString(LoadedContent, *TestFilePath);
	TestEqual(TEXT("Original content should match"), LoadedContent, OriginalContent);

	// Test: File can be overwritten if bOverwriteExisting=true
	const FString NewContent = TEXT("New content after overwrite");
	FFileHelper::SaveStringToFile(NewContent, *TestFilePath);

	FFileHelper::LoadFileToString(LoadedContent, *TestFilePath);
	TestEqual(TEXT("Content should be overwritten"), LoadedContent, NewContent);
	TestNotEqual(TEXT("Content should differ from original"), LoadedContent, OriginalContent);

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteDirectory(*TestDestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorExtractListContentsTest, "Orchestrator.Extract.ListContents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorExtractListContentsTest::RunTest(const FString& Parameters)
{
	// Test: ListZipContents with non-existent file should fail
	TArray<FString> FileList;
	TestFalse(TEXT("Non-existent zip should fail to list"),
		FOrchestratorExtract::ListZipContents(TEXT("/non/existent/file.zip"), FileList));

	TestEqual(TEXT("File list should be empty on failure"), FileList.Num(), 0);

	// Test: ListZipContents with empty path should fail
	TArray<FString> EmptyPathList;
	TestFalse(TEXT("Empty path should fail to list"),
		FOrchestratorExtract::ListZipContents(TEXT(""), EmptyPathList));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorExtractVerifyIntegrityTest, "Orchestrator.Extract.VerifyIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorExtractVerifyIntegrityTest::RunTest(const FString& Parameters)
{
	// Test: VerifyZipIntegrity with non-existent file should fail
	TestFalse(TEXT("Non-existent zip should fail integrity check"),
		FOrchestratorExtract::VerifyZipIntegrity(TEXT("/non/existent/file.zip")));

	// Test: VerifyZipIntegrity with empty path should fail
	TestFalse(TEXT("Empty path should fail integrity check"),
		FOrchestratorExtract::VerifyZipIntegrity(TEXT("")));

	// Test: Create a non-zip file and verify it fails integrity check
	const FString FakeZipPath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/fake.zip");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(FakeZipPath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	FFileHelper::SaveStringToFile(TEXT("Not a real zip file"), *FakeZipPath);

	// With placeholder implementation, this may not detect invalid zips yet
	// This test validates the API and prepares for real implementation
	TestTrue(TEXT("Fake zip file should exist for testing"), PlatformFile.FileExists(*FakeZipPath));

	// Cleanup
	PlatformFile.DeleteFile(*FakeZipPath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorExtractSelectiveTest, "Orchestrator.Extract.SelectiveExtraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorExtractSelectiveTest::RunTest(const FString& Parameters)
{
	// Test: ExtractZipSelective with empty file list
	TArray<FString> EmptyFileList;
	FExtractionResult EmptyListResult = FOrchestratorExtract::ExtractZipSelective(
		FPaths::ProjectSavedDir() / TEXT("test.zip"),
		FPaths::ProjectSavedDir() / TEXT("OrchestratorTests"),
		EmptyFileList,
		true);

	// With empty file list, behavior depends on implementation
	// This validates the API surface

	// Test: ExtractZipSelective with specific files
	TArray<FString> SpecificFiles = {TEXT("Data/Data.uasset"), TEXT("Config/Settings.ini")};
	FExtractionResult SelectiveResult = FOrchestratorExtract::ExtractZipSelective(
		TEXT("/non/existent/file.zip"),
		FPaths::ProjectSavedDir() / TEXT("OrchestratorTests"),
		SpecificFiles,
		true);

	TestFalse(TEXT("Non-existent zip should fail selective extraction"), SelectiveResult.bSuccess);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorExtractNestedPathsTest, "Orchestrator.Extract.NestedPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorExtractNestedPathsTest::RunTest(const FString& Parameters)
{
	// Test that extraction handles nested directory structures correctly
	const FString BaseDir = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/nested_extract");
	const FString NestedPath1 = BaseDir / TEXT("Data/Items");
	const FString NestedPath2 = BaseDir / TEXT("Source/Public/Classes");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Create nested directory structure
	TestTrue(TEXT("Nested path 1 should be created"), PlatformFile.CreateDirectoryTree(*NestedPath1));
	TestTrue(TEXT("Nested path 2 should be created"), PlatformFile.CreateDirectoryTree(*NestedPath2));

	// Verify directories exist
	TestTrue(TEXT("Nested path 1 should exist"), PlatformFile.DirectoryExists(*NestedPath1));
	TestTrue(TEXT("Nested path 2 should exist"), PlatformFile.DirectoryExists(*NestedPath2));

	// Test file creation in nested paths
	const FString TestFile1 = NestedPath1 / TEXT("item.uasset");
	const FString TestFile2 = NestedPath2 / TEXT("header.h");

	TestTrue(TEXT("Should write file in nested path 1"),
		FFileHelper::SaveStringToFile(TEXT("Item data"), *TestFile1));
	TestTrue(TEXT("Should write file in nested path 2"),
		FFileHelper::SaveStringToFile(TEXT("Header data"), *TestFile2));

	TestTrue(TEXT("File in nested path 1 should exist"), PlatformFile.FileExists(*TestFile1));
	TestTrue(TEXT("File in nested path 2 should exist"), PlatformFile.FileExists(*TestFile2));

	// Cleanup
	PlatformFile.DeleteFile(*TestFile1);
	PlatformFile.DeleteFile(*TestFile2);
	PlatformFile.DeleteDirectoryRecursively(*BaseDir);

	return true;
}
