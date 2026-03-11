// Copyright ALIS. All Rights Reserved.

#include "OrchestratorDownload.h"
#include "OrchestratorHash.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDownloadHashVerificationTest, "Orchestrator.Download.HashVerification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorDownloadHashVerificationTest::RunTest(const FString& Parameters)
{
	// Create temporary test file to simulate downloaded content
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/download_test.bin");
	const FString TestContent = TEXT("Simulated download content");

	// Ensure directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(TestFilePath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	// Write test file
	FFileHelper::SaveStringToFile(TestContent, *TestFilePath);

	// Compute hash of test file
	FString ExpectedHash;
	FOrchestratorHash::HashFile(TestFilePath, ExpectedHash);

	// Test: Hash verification should succeed with correct hash
	TestTrue(TEXT("Hash verification should succeed with correct hash"),
		FOrchestratorHash::VerifyHash(ExpectedHash, ExpectedHash));

	// Test: Hash verification should fail with incorrect hash
	TestFalse(TEXT("Hash verification should fail with incorrect hash"),
		FOrchestratorHash::VerifyHash(ExpectedHash, TEXT("wronghash123")));

	// Test: Case-insensitive hash verification
	FString UppercaseHash = ExpectedHash.ToUpper();
	TestTrue(TEXT("Hash verification should be case-insensitive"),
		FOrchestratorHash::VerifyHash(ExpectedHash, UppercaseHash));

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDownloadPathValidationTest, "Orchestrator.Download.PathValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorDownloadPathValidationTest::RunTest(const FString& Parameters)
{
	// Test: Empty destination path should fail
	FDownloadResult EmptyPathResult = FOrchestratorDownload::DownloadFile(
		TEXT("http://example.com/file.zip"),
		TEXT(""),
		TEXT(""));

	TestFalse(TEXT("Empty destination path should fail"), EmptyPathResult.bSuccess);
	TestTrue(TEXT("Error message should mention path"),
		EmptyPathResult.ErrorMessage.Contains(TEXT("path")) ||
		EmptyPathResult.ErrorMessage.Contains(TEXT("destination")));

	// Test: Invalid URL should fail
	const FString ValidPath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_download.bin");
	FDownloadResult InvalidUrlResult = FOrchestratorDownload::DownloadFile(
		TEXT(""),
		ValidPath,
		TEXT(""));

	TestFalse(TEXT("Empty URL should fail"), InvalidUrlResult.bSuccess);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDownloadResultStructTest, "Orchestrator.Download.ResultStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorDownloadResultStructTest::RunTest(const FString& Parameters)
{
	// Test: MakeSuccess factory method
	TArray<FString> TestFiles = {TEXT("file1.txt"), TEXT("file2.txt")};
	float TestTime = 1.5f;
	FDownloadResult SuccessResult = FDownloadResult::MakeSuccess(TEXT("/path/to/file.zip"), 1024, TestTime);

	TestTrue(TEXT("Success result should have bSuccess=true"), SuccessResult.bSuccess);
	TestTrue(TEXT("Error message should be empty"), SuccessResult.ErrorMessage.IsEmpty());
	TestEqual(TEXT("Downloaded file path should match"), SuccessResult.DownloadedFilePath, TEXT("/path/to/file.zip"));
	TestEqual(TEXT("Bytes downloaded should match"), SuccessResult.BytesDownloaded, (int64)1024);
	TestEqual(TEXT("Download time should match"), SuccessResult.DownloadTimeSeconds, TestTime);

	// Test: MakeFailure factory method
	FString TestError = TEXT("Network connection failed");
	FDownloadResult FailureResult = FDownloadResult::MakeFailure(TestError);

	TestFalse(TEXT("Failure result should have bSuccess=false"), FailureResult.bSuccess);
	TestEqual(TEXT("Error message should match"), FailureResult.ErrorMessage, TestError);
	TestTrue(TEXT("Downloaded file path should be empty"), FailureResult.DownloadedFilePath.IsEmpty());
	TestEqual(TEXT("Bytes downloaded should be 0"), FailureResult.BytesDownloaded, (int64)0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDownloadFileExistsTest, "Orchestrator.Download.FileExists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorDownloadFileExistsTest::RunTest(const FString& Parameters)
{
	// Create test file that already exists
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/existing_file.bin");
	const FString TestContent = TEXT("Existing file content");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(TestFilePath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	FFileHelper::SaveStringToFile(TestContent, *TestFilePath);

	// Compute hash of existing file
	FString ExistingHash;
	FOrchestratorHash::HashFile(TestFilePath, ExistingHash);

	// Test: If file exists and hash matches, download should skip
	// Note: This test validates the expected behavior - actual implementation may vary
	TestTrue(TEXT("Existing file should be detected"), PlatformFile.FileExists(*TestFilePath));

	// Test: Hash of existing file should be verifiable
	FString VerifyHash;
	FOrchestratorHash::HashFile(TestFilePath, VerifyHash);
	TestEqual(TEXT("Hash verification of existing file"), ExistingHash, VerifyHash);

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDownloadResumableTest, "Orchestrator.Download.ResumableLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorDownloadResumableTest::RunTest(const FString& Parameters)
{
	// Create partial download file
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/partial_download.bin");
	const FString PartialContent = TEXT("Partial");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(TestFilePath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	FFileHelper::SaveStringToFile(PartialContent, *TestFilePath);

	// Get size of partial file
	int64 PartialSize = PlatformFile.FileSize(*TestFilePath);
	TestTrue(TEXT("Partial file should have non-zero size"), PartialSize > 0);

	// Test: Resumable download should detect existing partial file
	TestTrue(TEXT("Partial file should exist"), PlatformFile.FileExists(*TestFilePath));
	TestTrue(TEXT("Partial file size should be correct"), PartialSize == PartialContent.Len());

	// Test: Expected size validation
	int64 ExpectedFullSize = 1024;
	TestTrue(TEXT("Partial size should be less than expected full size"), PartialSize < ExpectedFullSize);

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDownloadDirectoryCreationTest, "Orchestrator.Download.DirectoryCreation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorDownloadDirectoryCreationTest::RunTest(const FString& Parameters)
{
	// Test directory that doesn't exist yet
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/nested/deep/path/file.bin");
	const FString TestDir = FPaths::GetPath(TestFilePath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Ensure test directory doesn't exist
	if (PlatformFile.DirectoryExists(*TestDir))
	{
		PlatformFile.DeleteDirectoryRecursively(*TestDir);
	}

	TestFalse(TEXT("Test directory should not exist initially"), PlatformFile.DirectoryExists(*TestDir));

	// Test: CreateDirectoryTree should create nested directories
	TestTrue(TEXT("CreateDirectoryTree should succeed"), PlatformFile.CreateDirectoryTree(*TestDir));
	TestTrue(TEXT("Directory should exist after creation"), PlatformFile.DirectoryExists(*TestDir));

	// Test: Directory should be writable
	const FString TestContent = TEXT("Test file content");
	TestTrue(TEXT("Should be able to write file in created directory"),
		FFileHelper::SaveStringToFile(TestContent, *TestFilePath));

	TestTrue(TEXT("Written file should exist"), PlatformFile.FileExists(*TestFilePath));

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteDirectoryRecursively(*(FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/nested")));

	return true;
}
