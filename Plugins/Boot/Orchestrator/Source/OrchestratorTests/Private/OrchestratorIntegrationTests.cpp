// Copyright ALIS. All Rights Reserved.

#include "OrchestratorManifest.h"
#include "OrchestratorState.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorManifestParsingTest, "Orchestrator.Integration.ManifestParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorManifestParsingTest::RunTest(const FString& Parameters)
{
	// TODO: Update this test to use the actual CDN manifest schema with UUID, platform, code/assets structure
	// See: <cdn-repo>\docs\manifest.schema.json
	AddError(TEXT("Test DISABLED: Uses outdated manifest schema - needs refactoring for CDN integration (UUID, platform, code/assets)"));
	return false;

	/* DISABLED UNTIL CDN INTEGRATION COMPLETE
	const FString ManifestJson = TEXT(R"({
		"manifest_version": "1.0.0",
		"engine_build_id": "5.5.0",
		"channel": "stable",
		"plugins": [
			{
				"name": "ProjectCore",
				"version": "1.0.0",
				"module": "ProjectCore",
				"code_hash": "abc123",
				"content_hash": "def456",
				"url_code": "https://cdn.example.com/ProjectCore-1.0.0-code.zip",
				"url_content": "https://cdn.example.com/ProjectCore-1.0.0-content.zip",
				"channel": "stable",
				"depends_on": []
			},
			{
				"name": "ProjectUI",
				"version": "1.0.0",
				"module": "ProjectUI",
				"code_hash": "ghi789",
				"content_hash": "jkl012",
				"url_code": "https://cdn.example.com/ProjectUI-1.0.0-code.zip",
				"url_content": "https://cdn.example.com/ProjectUI-1.0.0-content.zip",
				"channel": "stable",
				"depends_on": [
					{"name": "ProjectCore", "version": ">=1.0.0"}
				]
			}
		]
	})");

	const FString ManifestPath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_manifest.json");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(ManifestPath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	FFileHelper::SaveStringToFile(ManifestJson, *ManifestPath);

	// Test: Load and parse manifest
	FString LoadedJson;
	TestTrue(TEXT("Manifest file should load"), FFileHelper::LoadFileToString(LoadedJson, *ManifestPath));

	FOrchestratorManifest LoadedManifest;
	TestTrue(TEXT("Manifest should parse successfully"),
		FOrchestratorManifest::LoadFromString(LoadedJson, LoadedManifest));

	TestEqual(TEXT("Manifest version should match"), LoadedManifest.ManifestVersion, TEXT("1.0.0"));
	TestEqual(TEXT("Engine build ID should match"), LoadedManifest.EngineBuildId, TEXT("5.5.0"));
	TestEqual(TEXT("Plugins count should be 2"), LoadedManifest.Plugins.Num(), 2);

	// Verify ProjectCore plugin
	if (LoadedManifest.Plugins.Num() > 0)
	{
		const FOrchestratorPluginEntry& Core = LoadedManifest.Plugins[0];
		TestEqual(TEXT("Core plugin name"), Core.Name, TEXT("ProjectCore"));
		TestEqual(TEXT("Core plugin version"), Core.Version, TEXT("1.0.0"));
		TestEqual(TEXT("Core plugin module"), Core.Module, TEXT("ProjectCore"));
		TestEqual(TEXT("Core plugin code hash"), Core.CodeHash, TEXT("abc123"));
		TestEqual(TEXT("Core plugin has no dependencies"), Core.DependsOn.Num(), 0);
	}

	// Verify ProjectUI plugin
	if (LoadedManifest.Plugins.Num() > 1)
	{
		const FOrchestratorPluginEntry& UI = LoadedManifest.Plugins[1];
		TestEqual(TEXT("UI plugin name"), UI.Name, TEXT("ProjectUI"));
		TestEqual(TEXT("UI plugin has 1 dependency"), UI.DependsOn.Num(), 1);

		if (UI.DependsOn.Num() > 0)
		{
			TestEqual(TEXT("UI depends on ProjectCore"), UI.DependsOn[0].Name, TEXT("ProjectCore"));
			TestEqual(TEXT("UI dependency version constraint"), UI.DependsOn[0].VersionConstraint, TEXT(">=1.0.0"));
		}
	}

	// Cleanup
	PlatformFile.DeleteFile(*ManifestPath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
	*/
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDependencyResolutionTest, "Orchestrator.Integration.DependencyResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorDependencyResolutionTest::RunTest(const FString& Parameters)
{
	// TODO: Update this test to use the actual CDN manifest schema with UUID, platform, code/assets structure
	// See: <cdn-repo>\docs\manifest.schema.json
	AddError(TEXT("Test DISABLED: Uses outdated manifest schema - needs refactoring for CDN integration (UUID, platform, code/assets)"));
	return false;

	/* DISABLED UNTIL CDN INTEGRATION COMPLETE
	const FString ManifestJson = TEXT(R"({
		"manifest_version": "1.0.0",
		"engine_build_id": "5.5.0",
		"channel": "stable",
		"plugins": [
			{
				"name": "PluginC",
				"version": "1.0.0",
				"module": "PluginC",
				"code_hash": "hash_c",
				"content_hash": "content_c",
				"url_code": "http://example.com/c.zip",
				"url_content": "",
				"channel": "stable",
				"depends_on": []
			},
			{
				"name": "PluginB",
				"version": "1.0.0",
				"module": "PluginB",
				"code_hash": "hash_b",
				"content_hash": "content_b",
				"url_code": "http://example.com/b.zip",
				"url_content": "",
				"channel": "stable",
				"depends_on": [
					{"name": "PluginC", "version": ">=1.0.0"}
				]
			},
			{
				"name": "PluginA",
				"version": "1.0.0",
				"module": "PluginA",
				"code_hash": "hash_a",
				"content_hash": "content_a",
				"url_code": "http://example.com/a.zip",
				"url_content": "",
				"channel": "stable",
				"depends_on": [
					{"name": "PluginB", "version": ">=1.0.0"}
				]
			}
		]
	})");

	const FString ManifestPath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/dependency_test_manifest.json");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString TestDir = FPaths::GetPath(ManifestPath);
	PlatformFile.CreateDirectoryTree(*TestDir);

	FFileHelper::SaveStringToFile(ManifestJson, *ManifestPath);

	// Test: Load manifest
	FString LoadedJson;
	FFileHelper::LoadFileToString(LoadedJson, *ManifestPath);

	FOrchestratorManifest Manifest;
	TestTrue(TEXT("Manifest should load"), FOrchestratorManifest::LoadFromString(LoadedJson, Manifest));
	TestEqual(TEXT("Should have 3 plugins"), Manifest.Plugins.Num(), 3);

	// Note: Actual dependency resolution happens in OrchestratorCoreModule::ResolveDependencies()
	// This test validates the manifest structure supports dependency resolution
	// Expected activation order should be: C, B, A

	// Cleanup
	PlatformFile.DeleteFile(*ManifestPath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
	*/
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorUpdateClassificationTest, "Orchestrator.Integration.UpdateClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorUpdateClassificationTest::RunTest(const FString& Parameters)
{
	// TODO: Update this test to use the actual CDN manifest schema with UUID, platform, code/assets structure
	// See: <cdn-repo>\docs\manifest.schema.json
	AddError(TEXT("Test DISABLED: Uses outdated manifest schema - needs refactoring for CDN integration (UUID, platform, code/assets)"));
	return false;

	/* DISABLED UNTIL CDN INTEGRATION COMPLETE
	// Create manifest with various plugin states
	const FString ManifestJson = TEXT(R"({
		"manifest_version": "1.0.0",
		"engine_build_id": "5.5.0",
		"channel": "stable",
		"plugins": [
			{
				"name": "PluginUpToDate",
				"version": "1.0.0",
				"module": "PluginUpToDate",
				"code_hash": "same_code",
				"content_hash": "same_content",
				"url_code": "http://example.com/uptodate.zip",
				"url_content": "",
				"channel": "stable",
				"depends_on": []
			},
			{
				"name": "PluginHotUpdate",
				"version": "1.1.0",
				"module": "PluginHotUpdate",
				"code_hash": "same_code",
				"content_hash": "new_content",
				"url_code": "http://example.com/hot.zip",
				"url_content": "http://example.com/hot-content.zip",
				"channel": "stable",
				"depends_on": []
			},
			{
				"name": "PluginColdUpdate",
				"version": "2.0.0",
				"module": "PluginColdUpdate",
				"code_hash": "new_code",
				"content_hash": "new_content",
				"url_code": "http://example.com/cold.zip",
				"url_content": "",
				"channel": "stable",
				"depends_on": []
			},
			{
				"name": "PluginNew",
				"version": "1.0.0",
				"module": "PluginNew",
				"code_hash": "first_code",
				"content_hash": "first_content",
				"url_code": "http://example.com/new.zip",
				"url_content": "",
				"channel": "stable",
				"depends_on": []
			}
		]
	})");

	// Create corresponding latest.json state
	const FString StateJson = TEXT(R"({
		"manifest_version": "0.9.0",
		"engine_build_id": "5.5.0",
		"applied_at": "2025-10-31T10:00:00Z",
		"plugins": {
			"PluginUpToDate": {
				"name": "PluginUpToDate",
				"version": "1.0.0",
				"module": "PluginUpToDate",
				"code_hash": "same_code",
				"content_hash": "same_content",
				"installed_path": "/Game/Plugins/PluginUpToDate",
				"channel": "stable"
			},
			"PluginHotUpdate": {
				"name": "PluginHotUpdate",
				"version": "1.0.0",
				"module": "PluginHotUpdate",
				"code_hash": "same_code",
				"content_hash": "old_content",
				"installed_path": "/Game/Plugins/PluginHotUpdate",
				"channel": "stable"
			},
			"PluginColdUpdate": {
				"name": "PluginColdUpdate",
				"version": "1.0.0",
				"module": "PluginColdUpdate",
				"code_hash": "old_code",
				"content_hash": "old_content",
				"installed_path": "/Game/Plugins/PluginColdUpdate",
				"channel": "stable"
			}
		}
	})");

	const FString TestDir = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/classification_test");
	const FString ManifestPath = TestDir / TEXT("manifest.json");
	const FString StatePath = TestDir / TEXT("latest.json");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*TestDir);

	FFileHelper::SaveStringToFile(ManifestJson, *ManifestPath);
	FFileHelper::SaveStringToFile(StateJson, *StatePath);

	// Load manifest and state
	FString LoadedManifestJson;
	FFileHelper::LoadFileToString(LoadedManifestJson, *ManifestPath);

	FOrchestratorManifest Manifest;
	FOrchestratorState State;

	TestTrue(TEXT("Manifest should load"), FOrchestratorManifest::LoadFromString(LoadedManifestJson, Manifest));
	TestTrue(TEXT("State should load"), FOrchestratorState::LoadFromFile(StatePath, State));

	TestEqual(TEXT("Manifest should have 4 plugins"), Manifest.Plugins.Num(), 4);
	TestEqual(TEXT("State should have 3 plugins"), State.Plugins.Num(), 3);

	// Classification logic (implemented in OrchestratorCoreModule::ProcessManifest):
	// - PluginUpToDate: code_hash same, content_hash same -> Up-to-date
	// - PluginHotUpdate: code_hash same, content_hash changed -> HOT update
	// - PluginColdUpdate: code_hash changed -> COLD update
	// - PluginNew: not in state -> NEW plugin (cold install)

	// Verify state lookups
	TestTrue(TEXT("PluginUpToDate should exist in state"), State.Plugins.Contains(TEXT("PluginUpToDate")));
	TestTrue(TEXT("PluginHotUpdate should exist in state"), State.Plugins.Contains(TEXT("PluginHotUpdate")));
	TestTrue(TEXT("PluginColdUpdate should exist in state"), State.Plugins.Contains(TEXT("PluginColdUpdate")));
	TestFalse(TEXT("PluginNew should NOT exist in state"), State.Plugins.Contains(TEXT("PluginNew")));

	// Cleanup
	PlatformFile.DeleteFile(*ManifestPath);
	PlatformFile.DeleteFile(*StatePath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
	*/
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorStateRoundtripTest, "Orchestrator.Integration.StateRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorStateRoundtripTest::RunTest(const FString& Parameters)
{
	// Test full state management workflow
	const FString TestDir = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/state_roundtrip");
	const FString LatestPath = TestDir / TEXT("latest.json");
	const FString LastGoodPath = TestDir / TEXT("last_good.json");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*TestDir);

	// Create initial state
	FOrchestratorState InitialState;
	InitialState.ManifestVersion = TEXT("1.0.0");
	InitialState.EngineBuildId = TEXT("5.5.0");
	InitialState.AppliedAt = TEXT("2025-10-31T12:00:00Z");

	FPluginState Plugin1;
	Plugin1.Name = TEXT("Plugin1");
	Plugin1.Version = TEXT("1.0.0");
	Plugin1.Module = TEXT("Plugin1");
	Plugin1.CodeHash = TEXT("hash1");
	Plugin1.ContentHash = TEXT("content1");
	Plugin1.InstalledPath = TEXT("/Game/Plugins/Plugin1");
	Plugin1.Channel = TEXT("stable");

	InitialState.Plugins.Add(Plugin1.Name, Plugin1);

	// Save initial state
	TestTrue(TEXT("Initial state should save"), InitialState.SaveToFile(LatestPath));
	TestTrue(TEXT("Latest.json should exist"), PlatformFile.FileExists(*LatestPath));

	// Load and verify
	FOrchestratorState LoadedState;
	TestTrue(TEXT("State should load"), FOrchestratorState::LoadFromFile(LatestPath, LoadedState));
	TestEqual(TEXT("Manifest version should match"), LoadedState.ManifestVersion, InitialState.ManifestVersion);
	TestEqual(TEXT("Plugins count should match"), LoadedState.Plugins.Num(), 1);

	// Simulate save last good
	TestTrue(TEXT("Last good state should save"), LoadedState.SaveToFile(LastGoodPath));
	TestTrue(TEXT("Last_good.json should exist"), PlatformFile.FileExists(*LastGoodPath));

	// Update latest state
	LoadedState.ManifestVersion = TEXT("1.1.0");
	FPluginState Plugin2;
	Plugin2.Name = TEXT("Plugin2");
	Plugin2.Version = TEXT("1.0.0");
	Plugin2.Module = TEXT("Plugin2");
	Plugin2.CodeHash = TEXT("hash2");
	Plugin2.ContentHash = TEXT("content2");
	Plugin2.InstalledPath = TEXT("/Game/Plugins/Plugin2");
	Plugin2.Channel = TEXT("stable");
	LoadedState.Plugins.Add(Plugin2.Name, Plugin2);

	TestTrue(TEXT("Updated state should save"), LoadedState.SaveToFile(LatestPath));

	// Verify updated state
	FOrchestratorState UpdatedState;
	TestTrue(TEXT("Updated state should load"), FOrchestratorState::LoadFromFile(LatestPath, UpdatedState));
	TestEqual(TEXT("Updated manifest version"), UpdatedState.ManifestVersion, TEXT("1.1.0"));
	TestEqual(TEXT("Updated plugins count"), UpdatedState.Plugins.Num(), 2);

	// Verify last good still has old version
	FOrchestratorState LastGoodState;
	TestTrue(TEXT("Last good state should load"), FOrchestratorState::LoadFromFile(LastGoodPath, LastGoodState));
	TestEqual(TEXT("Last good manifest version"), LastGoodState.ManifestVersion, TEXT("1.0.0"));
	TestEqual(TEXT("Last good plugins count"), LastGoodState.Plugins.Num(), 1);

	// Cleanup
	PlatformFile.DeleteFile(*LatestPath);
	PlatformFile.DeleteFile(*LastGoodPath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorPendingUpdatesWorkflowTest, "Orchestrator.Integration.PendingUpdatesWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrchestratorPendingUpdatesWorkflowTest::RunTest(const FString& Parameters)
{
	// Test cold update restart cycle workflow
	const FString TestDir = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/pending_workflow");
	const FString PendingPath = TestDir / TEXT("pending_updates.json");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*TestDir);

	// Create pending updates
	FPendingUpdates PendingList;
	PendingList.ManifestVersion = TEXT("1.1.0");
	PendingList.EngineBuildId = TEXT("5.5.0");
	PendingList.CreatedAt = TEXT("2025-10-31T12:00:00Z");

	FPendingUpdate Update1;
	Update1.Name = TEXT("ProjectCore");
	Update1.FromVersion = TEXT("1.0.0");
	Update1.ToVersion = TEXT("1.1.0");
	Update1.Change = TEXT("code");
	Update1.StagedPath = TestDir / TEXT("Staged/ProjectCore");
	Update1.bRequiresRestart = true;

	PendingList.Updates.Add(Update1);

	// Save pending updates
	TestTrue(TEXT("Pending updates should save"), PendingList.SaveToFile(PendingPath));
	TestTrue(TEXT("Pending updates file should exist"), PlatformFile.FileExists(*PendingPath));

	// Simulate restart: Load pending updates
	FPendingUpdates LoadedPending;
	TestTrue(TEXT("Pending updates should load"), FPendingUpdates::LoadFromFile(PendingPath, LoadedPending));
	TestEqual(TEXT("Manifest version should match"), LoadedPending.ManifestVersion, PendingList.ManifestVersion);
	TestEqual(TEXT("Updates count should be 1"), LoadedPending.Updates.Num(), 1);

	if (LoadedPending.Updates.Num() > 0)
	{
		const FPendingUpdate& LoadedUpdate = LoadedPending.Updates[0];
		TestEqual(TEXT("Update name should match"), LoadedUpdate.Name, Update1.Name);
		TestEqual(TEXT("Update from version"), LoadedUpdate.FromVersion, Update1.FromVersion);
		TestEqual(TEXT("Update to version"), LoadedUpdate.ToVersion, Update1.ToVersion);
		TestEqual(TEXT("Update should require restart"), LoadedUpdate.bRequiresRestart, true);
	}

	// After successful application, pending_updates.json should be deleted
	// (This is tested in ApplyPendingUpdates workflow)

	// Cleanup
	PlatformFile.DeleteFile(*PendingPath);
	PlatformFile.DeleteDirectory(*TestDir);

	return true;
}
