// Copyright ALIS. All Rights Reserved.

#include "OrchestratorState.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorStateSerializationTest, "Orchestrator.State.Serialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorStateSerializationTest::RunTest(const FString& Parameters)
{
	// Create test state
	FOrchestratorState TestState;
	TestState.ManifestVersion = TEXT("1.0.0");
	TestState.EngineBuildId = TEXT("5.5.0");
	TestState.AppliedAt = TEXT("2025-10-31T12:00:00Z");

	// Add plugin states
	FPluginState Plugin1;
	Plugin1.Name = TEXT("TestPlugin1");
	Plugin1.Version = TEXT("1.0.0");
	Plugin1.Module = TEXT("TestModule1");
	Plugin1.CodeHash = TEXT("abc123");
	Plugin1.ContentHash = TEXT("def456");
	Plugin1.InstalledPath = TEXT("/Path/To/Plugin1");
	Plugin1.Channel = TEXT("stable");

	FPluginState Plugin2;
	Plugin2.Name = TEXT("TestPlugin2");
	Plugin2.Version = TEXT("2.0.0");
	Plugin2.Module = TEXT("");  // Content-only plugin
	Plugin2.CodeHash = TEXT("ghi789");
	Plugin2.ContentHash = TEXT("jkl012");
	Plugin2.InstalledPath = TEXT("/Path/To/Plugin2");
	Plugin2.Channel = TEXT("experimental");

	TestState.Plugins.Add(Plugin1.Name, Plugin1);
	TestState.Plugins.Add(Plugin2.Name, Plugin2);

	// Test: SaveToFile and LoadFromFile roundtrip
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_state.json");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(TestFilePath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	TestTrue(TEXT("SaveToFile should succeed"), TestState.SaveToFile(TestFilePath));
	TestTrue(TEXT("State file should exist"), PlatformFile.FileExists(*TestFilePath));

	// Load and verify
	FOrchestratorState LoadedState;
	TestTrue(TEXT("LoadFromFile should succeed"), FOrchestratorState::LoadFromFile(TestFilePath, LoadedState));

	TestEqual(TEXT("ManifestVersion should match"), LoadedState.ManifestVersion, TestState.ManifestVersion);
	TestEqual(TEXT("EngineBuildId should match"), LoadedState.EngineBuildId, TestState.EngineBuildId);
	TestEqual(TEXT("AppliedAt should match"), LoadedState.AppliedAt, TestState.AppliedAt);
	TestEqual(TEXT("Plugins count should match"), LoadedState.Plugins.Num(), 2);

	// Verify plugin 1
	const FPluginState* LoadedPlugin1 = LoadedState.Plugins.Find(TEXT("TestPlugin1"));
	TestNotNull(TEXT("Plugin1 should exist"), LoadedPlugin1);
	if (LoadedPlugin1)
	{
		TestEqual(TEXT("Plugin1 Name"), LoadedPlugin1->Name, Plugin1.Name);
		TestEqual(TEXT("Plugin1 Version"), LoadedPlugin1->Version, Plugin1.Version);
		TestEqual(TEXT("Plugin1 Module"), LoadedPlugin1->Module, Plugin1.Module);
		TestEqual(TEXT("Plugin1 CodeHash"), LoadedPlugin1->CodeHash, Plugin1.CodeHash);
		TestEqual(TEXT("Plugin1 ContentHash"), LoadedPlugin1->ContentHash, Plugin1.ContentHash);
		TestEqual(TEXT("Plugin1 InstalledPath"), LoadedPlugin1->InstalledPath, Plugin1.InstalledPath);
		TestEqual(TEXT("Plugin1 Channel"), LoadedPlugin1->Channel, Plugin1.Channel);
	}

	// Verify plugin 2
	const FPluginState* LoadedPlugin2 = LoadedState.Plugins.Find(TEXT("TestPlugin2"));
	TestNotNull(TEXT("Plugin2 should exist"), LoadedPlugin2);
	if (LoadedPlugin2)
	{
		TestEqual(TEXT("Plugin2 Module should be empty"), LoadedPlugin2->Module, FString(TEXT("")));
		TestEqual(TEXT("Plugin2 Channel"), LoadedPlugin2->Channel, TEXT("experimental"));
	}

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorStateAtomicWriteTest, "Orchestrator.State.AtomicWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorStateAtomicWriteTest::RunTest(const FString& Parameters)
{
	// Create test state
	FOrchestratorState TestState;
	TestState.ManifestVersion = TEXT("1.0.0");
	TestState.EngineBuildId = TEXT("5.5.0");

	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/atomic_test.json");
	const FString TempFilePath = TestFilePath + TEXT(".tmp");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(TestFilePath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	// Test: Atomic write should not leave temp file
	TestTrue(TEXT("SaveToFile should succeed"), TestState.SaveToFile(TestFilePath));
	TestTrue(TEXT("Final file should exist"), PlatformFile.FileExists(*TestFilePath));
	TestFalse(TEXT("Temp file should not exist"), PlatformFile.FileExists(*TempFilePath));

	// Test: Overwriting existing file should work
	TestState.ManifestVersion = TEXT("2.0.0");
	TestTrue(TEXT("Overwrite should succeed"), TestState.SaveToFile(TestFilePath));

	FOrchestratorState LoadedState;
	FOrchestratorState::LoadFromFile(TestFilePath, LoadedState);
	TestEqual(TEXT("Updated version should be saved"), LoadedState.ManifestVersion, TEXT("2.0.0"));

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPendingUpdatesSerializationTest, "Orchestrator.State.PendingUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPendingUpdatesSerializationTest::RunTest(const FString& Parameters)
{
	// Create test pending updates
	FPendingUpdates TestUpdates;
	TestUpdates.ManifestVersion = TEXT("1.0.0");
	TestUpdates.EngineBuildId = TEXT("5.5.0");
	TestUpdates.CreatedAt = TEXT("2025-10-31T12:00:00Z");

	// Add pending updates
	FPendingUpdate Update1;
	Update1.Name = TEXT("Plugin1");
	Update1.FromVersion = TEXT("1.0.0");
	Update1.ToVersion = TEXT("1.1.0");
	Update1.Change = TEXT("code");
	Update1.StagedPath = TEXT("/Staged/Plugin1/1.1.0");
	Update1.bRequiresRestart = true;

	FPendingUpdate Update2;
	Update2.Name = TEXT("Plugin2");
	Update2.FromVersion = TEXT("(none)");
	Update2.ToVersion = TEXT("1.0.0");
	Update2.Change = TEXT("code");
	Update2.StagedPath = TEXT("/Staged/Plugin2/1.0.0");
	Update2.bRequiresRestart = true;

	TestUpdates.Updates.Add(Update1);
	TestUpdates.Updates.Add(Update2);

	// Test: SaveToFile and LoadFromFile roundtrip
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/pending_updates.json");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(TestFilePath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	TestTrue(TEXT("SaveToFile should succeed"), TestUpdates.SaveToFile(TestFilePath));
	TestTrue(TEXT("File should exist"), PlatformFile.FileExists(*TestFilePath));

	// Load and verify
	FPendingUpdates LoadedUpdates;
	TestTrue(TEXT("LoadFromFile should succeed"), FPendingUpdates::LoadFromFile(TestFilePath, LoadedUpdates));

	TestEqual(TEXT("ManifestVersion should match"), LoadedUpdates.ManifestVersion, TestUpdates.ManifestVersion);
	TestEqual(TEXT("EngineBuildId should match"), LoadedUpdates.EngineBuildId, TestUpdates.EngineBuildId);
	TestEqual(TEXT("CreatedAt should match"), LoadedUpdates.CreatedAt, TestUpdates.CreatedAt);
	TestEqual(TEXT("Updates count should match"), LoadedUpdates.Updates.Num(), 2);

	// Verify update 1
	if (LoadedUpdates.Updates.Num() > 0)
	{
		const FPendingUpdate& LoadedUpdate1 = LoadedUpdates.Updates[0];
		TestEqual(TEXT("Update1 Name"), LoadedUpdate1.Name, Update1.Name);
		TestEqual(TEXT("Update1 FromVersion"), LoadedUpdate1.FromVersion, Update1.FromVersion);
		TestEqual(TEXT("Update1 ToVersion"), LoadedUpdate1.ToVersion, Update1.ToVersion);
		TestEqual(TEXT("Update1 Change"), LoadedUpdate1.Change, Update1.Change);
		TestEqual(TEXT("Update1 StagedPath"), LoadedUpdate1.StagedPath, Update1.StagedPath);
		TestEqual(TEXT("Update1 bRequiresRestart"), LoadedUpdate1.bRequiresRestart, true);
	}

	// Verify update 2
	if (LoadedUpdates.Updates.Num() > 1)
	{
		const FPendingUpdate& LoadedUpdate2 = LoadedUpdates.Updates[1];
		TestEqual(TEXT("Update2 Name"), LoadedUpdate2.Name, Update2.Name);
		TestEqual(TEXT("Update2 FromVersion"), LoadedUpdate2.FromVersion, TEXT("(none)"));
	}

	// Cleanup
	PlatformFile.DeleteFile(*TestFilePath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorStateErrorHandlingTest, "Orchestrator.State.ErrorHandling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorStateErrorHandlingTest::RunTest(const FString& Parameters)
{
	// Test: Loading non-existent file should fail gracefully
	FOrchestratorState LoadedState;
	TestFalse(TEXT("Loading non-existent file should fail"),
		FOrchestratorState::LoadFromFile(TEXT("/non/existent/file.json"), LoadedState));

	// Test: Loading invalid JSON should fail gracefully
	const FString InvalidJsonPath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/invalid.json");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(InvalidJsonPath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	FFileHelper::SaveStringToFile(TEXT("{ invalid json }"), *InvalidJsonPath);

	FOrchestratorState InvalidState;
	TestFalse(TEXT("Loading invalid JSON should fail"),
		FOrchestratorState::LoadFromFile(InvalidJsonPath, InvalidState));

	// Test: Empty JSON should succeed but have empty data
	const FString EmptyJsonPath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/empty.json");
	FFileHelper::SaveStringToFile(TEXT("{}"), *EmptyJsonPath);

	FOrchestratorState EmptyState;
	TestTrue(TEXT("Loading empty JSON should succeed"),
		FOrchestratorState::LoadFromFile(EmptyJsonPath, EmptyState));
	TestTrue(TEXT("ManifestVersion should be empty"), EmptyState.ManifestVersion.IsEmpty());
	TestEqual(TEXT("Plugins should be empty"), EmptyState.Plugins.Num(), 0);

	// Cleanup
	PlatformFile.DeleteFile(*InvalidJsonPath);
	PlatformFile.DeleteFile(*EmptyJsonPath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPluginStateToJsonTest, "Orchestrator.State.PluginStateToJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPluginStateToJsonTest::RunTest(const FString& Parameters)
{
	// Create plugin state
	FPluginState Plugin;
	Plugin.Name = TEXT("TestPlugin");
	Plugin.Version = TEXT("1.0.0");
	Plugin.Module = TEXT("TestModule");
	Plugin.CodeHash = TEXT("abc123");
	Plugin.ContentHash = TEXT("def456");
	Plugin.InstalledPath = TEXT("/Path/To/Plugin");
	Plugin.Channel = TEXT("stable");

	// Convert to JSON
	TSharedPtr<FJsonObject> JsonObject = Plugin.ToJson();
	TestNotNull(TEXT("ToJson should return valid object"), JsonObject.Get());

	if (JsonObject.IsValid())
	{
		FString JsonString;
		JsonObject->TryGetStringField(TEXT("name"), JsonString);
		TestEqual(TEXT("JSON name field"), JsonString, Plugin.Name);

		JsonObject->TryGetStringField(TEXT("version"), JsonString);
		TestEqual(TEXT("JSON version field"), JsonString, Plugin.Version);

		JsonObject->TryGetStringField(TEXT("module"), JsonString);
		TestEqual(TEXT("JSON module field"), JsonString, Plugin.Module);

		JsonObject->TryGetStringField(TEXT("code_hash"), JsonString);
		TestEqual(TEXT("JSON code_hash field"), JsonString, Plugin.CodeHash);

		JsonObject->TryGetStringField(TEXT("content_hash"), JsonString);
		TestEqual(TEXT("JSON content_hash field"), JsonString, Plugin.ContentHash);

		JsonObject->TryGetStringField(TEXT("installed_path"), JsonString);
		TestEqual(TEXT("JSON installed_path field"), JsonString, Plugin.InstalledPath);

		JsonObject->TryGetStringField(TEXT("channel"), JsonString);
		TestEqual(TEXT("JSON channel field"), JsonString, Plugin.Channel);
	}

	// Test: FromJson roundtrip
	FPluginState LoadedPlugin = FPluginState::FromJson(JsonObject);
	TestEqual(TEXT("Roundtrip Name"), LoadedPlugin.Name, Plugin.Name);
	TestEqual(TEXT("Roundtrip Version"), LoadedPlugin.Version, Plugin.Version);
	TestEqual(TEXT("Roundtrip Module"), LoadedPlugin.Module, Plugin.Module);
	TestEqual(TEXT("Roundtrip CodeHash"), LoadedPlugin.CodeHash, Plugin.CodeHash);
	TestEqual(TEXT("Roundtrip ContentHash"), LoadedPlugin.ContentHash, Plugin.ContentHash);
	TestEqual(TEXT("Roundtrip InstalledPath"), LoadedPlugin.InstalledPath, Plugin.InstalledPath);
	TestEqual(TEXT("Roundtrip Channel"), LoadedPlugin.Channel, Plugin.Channel);

	return true;
}
