// Copyright ALIS. All Rights Reserved.

#include "OrchestratorHash.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorHashFileTest, "Orchestrator.Hash.HashFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorHashFileTest::RunTest(const FString& Parameters)
{
	// Create temporary test file
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_file.txt");
	const FString TestContent = TEXT("Hello Orchestrator!");

	// Ensure directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(TestFilePath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	// Write test file
	if (!FFileHelper::SaveStringToFile(TestContent, *TestFilePath))
	{
		AddError(FString::Printf(TEXT("Failed to create test file: %s"), *TestFilePath));
		return false;
	}

	// Test: HashFile should compute hash successfully
	FString ComputedHash;
	TestTrue(TEXT("HashFile should succeed"), FOrchestratorHash::HashFile(TestFilePath, ComputedHash));
	TestTrue(TEXT("Hash should not be empty"), !ComputedHash.IsEmpty());
	TestEqual(TEXT("Hash length should be 40 characters (SHA-1)"), ComputedHash.Len(), 40);

	// Test: Same file should produce same hash
	FString ComputedHash2;
	TestTrue(TEXT("Second HashFile should succeed"), FOrchestratorHash::HashFile(TestFilePath, ComputedHash2));
	TestEqual(TEXT("Same file should produce same hash"), ComputedHash, ComputedHash2);

	// Test: Different content should produce different hash
	const FString TestFilePath2 = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_file2.txt");
	const FString TestContent2 = TEXT("Different content");
	FFileHelper::SaveStringToFile(TestContent2, *TestFilePath2);

	FString DifferentHash;
	FOrchestratorHash::HashFile(TestFilePath2, DifferentHash);
	TestNotEqual(TEXT("Different files should have different hashes"), ComputedHash, DifferentHash);

	// Test: Non-existent file should fail
	FString NonExistentHash;
	TestFalse(TEXT("Non-existent file should fail"),
		FOrchestratorHash::HashFile(TEXT("/non/existent/file.txt"), NonExistentHash));

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteFile(*TestFilePath2);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorHashDirectoryTest, "Orchestrator.Hash.HashDirectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorHashDirectoryTest::RunTest(const FString& Parameters)
{
	// Create temporary test directory with files
	const FString TestDir = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_dir");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*TestDir);

	// Create test files
	FFileHelper::SaveStringToFile(TEXT("File 1"), *(TestDir / TEXT("file1.txt")));
	FFileHelper::SaveStringToFile(TEXT("File 2"), *(TestDir / TEXT("file2.txt")));

	// Create subdirectory with file
	const FString SubDir = TestDir / TEXT("subdir");
	PlatformFile.CreateDirectory(*SubDir);
	FFileHelper::SaveStringToFile(TEXT("File 3"), *(SubDir / TEXT("file3.txt")));

	// Test: HashDirectory should compute hash successfully
	FString DirectoryHash;
	TestTrue(TEXT("HashDirectory should succeed"), FOrchestratorHash::HashDirectory(TestDir, DirectoryHash));
	TestTrue(TEXT("Directory hash should not be empty"), !DirectoryHash.IsEmpty());

	// Test: Same directory should produce same hash
	FString DirectoryHash2;
	TestTrue(TEXT("Second HashDirectory should succeed"), FOrchestratorHash::HashDirectory(TestDir, DirectoryHash2));
	TestEqual(TEXT("Same directory should produce same hash"), DirectoryHash, DirectoryHash2);

	// Test: Adding a file should change the hash
	FFileHelper::SaveStringToFile(TEXT("File 4"), *(TestDir / TEXT("file4.txt")));
	FString ModifiedHash;
	FOrchestratorHash::HashDirectory(TestDir, ModifiedHash);
	TestNotEqual(TEXT("Modified directory should have different hash"), DirectoryHash, ModifiedHash);

	// Test: Non-existent directory should fail
	FString NonExistentHash;
	TestFalse(TEXT("Non-existent directory should fail"),
		FOrchestratorHash::HashDirectory(TEXT("/non/existent/dir"), NonExistentHash));

	// Cleanup
	PlatformFile.DeleteFile(*(TestDir / TEXT("file1.txt")));
	PlatformFile.DeleteFile(*(TestDir / TEXT("file2.txt")));
	PlatformFile.DeleteFile(*(TestDir / TEXT("file4.txt")));
	PlatformFile.DeleteFile(*(SubDir / TEXT("file3.txt")));
	PlatformFile.DeleteDirectory(*SubDir);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorHashVerifyTest, "Orchestrator.Hash.VerifyHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorHashVerifyTest::RunTest(const FString& Parameters)
{
	const FString TestHash = TEXT("abc123def456");

	// Test: Same hash (case-insensitive)
	TestTrue(TEXT("Identical hashes should verify"),
		FOrchestratorHash::VerifyHash(TestHash, TestHash));

	// Test: Different case should still match (case-insensitive)
	TestTrue(TEXT("Case-insensitive comparison should work"),
		FOrchestratorHash::VerifyHash(TEXT("ABC123DEF456"), TestHash));
	TestTrue(TEXT("Case-insensitive comparison should work (reversed)"),
		FOrchestratorHash::VerifyHash(TestHash, TEXT("ABC123DEF456")));

	// Test: Different hash should not verify
	TestFalse(TEXT("Different hashes should not verify"),
		FOrchestratorHash::VerifyHash(TestHash, TEXT("different123")));

	// Test: Empty hashes
	TestTrue(TEXT("Empty hashes should match"),
		FOrchestratorHash::VerifyHash(TEXT(""), TEXT("")));
	TestFalse(TEXT("Empty vs non-empty should not match"),
		FOrchestratorHash::VerifyHash(TEXT(""), TestHash));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorHashCodeHashTest, "Orchestrator.Hash.ComputeCodeHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorHashCodeHashTest::RunTest(const FString& Parameters)
{
	// Create temporary plugin structure
	const FString PluginRoot = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_plugin");
	const FString BinariesDir = PluginRoot / TEXT("Binaries/Win64");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*BinariesDir);

	// Create .uplugin file
	FFileHelper::SaveStringToFile(TEXT("{\"FileVersion\": 3}"), *(PluginRoot / TEXT("TestPlugin.uplugin")));

	// Create binary files
	FFileHelper::SaveStringToFile(TEXT("Binary content"), *(BinariesDir / TEXT("TestModule.dll")));

	// Test: ComputeCodeHash should succeed
	FString CodeHash;
	TestTrue(TEXT("ComputeCodeHash should succeed"),
		FOrchestratorHash::ComputeCodeHash(PluginRoot, CodeHash));
	TestTrue(TEXT("Code hash should not be empty"), !CodeHash.IsEmpty());

	// Test: Same plugin should produce same hash
	FString CodeHash2;
	TestTrue(TEXT("Second ComputeCodeHash should succeed"),
		FOrchestratorHash::ComputeCodeHash(PluginRoot, CodeHash2));
	TestEqual(TEXT("Same plugin should produce same code hash"), CodeHash, CodeHash2);

	// Test: Modifying binary should change hash
	FFileHelper::SaveStringToFile(TEXT("Modified binary"), *(BinariesDir / TEXT("TestModule.dll")));
	FString ModifiedHash;
	FOrchestratorHash::ComputeCodeHash(PluginRoot, ModifiedHash);
	TestNotEqual(TEXT("Modified binary should change code hash"), CodeHash, ModifiedHash);

	// Test: Plugin without .uplugin should fail
	PlatformFile.DeleteFile(*(PluginRoot / TEXT("TestPlugin.uplugin")));
	FString InvalidHash;
	TestFalse(TEXT("Plugin without .uplugin should fail"),
		FOrchestratorHash::ComputeCodeHash(PluginRoot, InvalidHash));

	// Cleanup
	PlatformFile.DeleteFile(*(BinariesDir / TEXT("TestModule.dll")));
	PlatformFile.DeleteDirectory(*BinariesDir);
	PlatformFile.DeleteDirectory(*(PluginRoot / TEXT("Binaries")));
	PlatformFile.DeleteDirectory(*PluginRoot);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorHashContentHashTest, "Orchestrator.Hash.ComputeContentHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorHashContentHashTest::RunTest(const FString& Parameters)
{
	// Create temporary plugin structure with content
	const FString PluginRoot = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_content_plugin");
	const FString ContentDir = PluginRoot / TEXT("Content/Paks");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*ContentDir);

	// Create content files (.utoc/.ucas)
	FFileHelper::SaveStringToFile(TEXT("TOC data"), *(ContentDir / TEXT("Content.utoc")));
	FFileHelper::SaveStringToFile(TEXT("CAS data"), *(ContentDir / TEXT("Content.ucas")));

	// Test: ComputeContentHash should succeed
	FString ContentHash;
	TestTrue(TEXT("ComputeContentHash should succeed"),
		FOrchestratorHash::ComputeContentHash(PluginRoot, ContentHash));
	TestTrue(TEXT("Content hash should not be empty"), !ContentHash.IsEmpty());

	// Test: Same content should produce same hash
	FString ContentHash2;
	TestTrue(TEXT("Second ComputeContentHash should succeed"),
		FOrchestratorHash::ComputeContentHash(PluginRoot, ContentHash2));
	TestEqual(TEXT("Same content should produce same hash"), ContentHash, ContentHash2);

	// Test: Modifying content should change hash
	FFileHelper::SaveStringToFile(TEXT("Modified TOC"), *(ContentDir / TEXT("Content.utoc")));
	FString ModifiedHash;
	FOrchestratorHash::ComputeContentHash(PluginRoot, ModifiedHash);
	TestNotEqual(TEXT("Modified content should change hash"), ContentHash, ModifiedHash);

	// Test: Plugin without content files should return empty hash (valid for code-only plugins)
	PlatformFile.DeleteFile(*(ContentDir / TEXT("Content.utoc")));
	PlatformFile.DeleteFile(*(ContentDir / TEXT("Content.ucas")));
	FString EmptyHash;
	TestTrue(TEXT("Plugin without content should succeed with empty hash"),
		FOrchestratorHash::ComputeContentHash(PluginRoot, EmptyHash));
	TestTrue(TEXT("Content hash should not be empty (hash of empty)"), !EmptyHash.IsEmpty());

	// Cleanup
	PlatformFile.DeleteDirectory(*ContentDir);
	PlatformFile.DeleteDirectory(*(PluginRoot / TEXT("Content")));
	PlatformFile.DeleteDirectory(*PluginRoot);

	return true;
}
