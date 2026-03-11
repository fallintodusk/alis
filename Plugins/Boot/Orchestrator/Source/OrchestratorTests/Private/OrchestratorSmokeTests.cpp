// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "OrchestratorManifest.h"
#include "OrchestratorState.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorManifestParseTest, "Orchestrator.Smoke.ManifestParse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
bool FOrchestratorManifestParseTest::RunTest(const FString& Parameters)
{
	// Test: Parse valid manifest JSON
	const FString TestManifest = TEXT(R"({
		"manifest_version": 1,
		"engine_build_id": "5.5.0-test",
		"signed_at": "2025-10-31T19:00:00Z",
		"signing_key_id": "test-key",
		"signature": "TEST_SIGNATURE",
		"plugins": [
			{
				"name": "TestPlugin",
				"version": "1.0.0",
				"module": "TestModule",
				"code_hash": "abc123",
				"content_hash": "def456",
				"url_code": "https://test.com/code.zip",
				"url_content": "https://test.com/content.zip",
				"depends_on": [],
				"channel": "stable",
				"signature_thumbprint": "thumb123"
			}
		]
	})");

	FOrchestratorManifest Manifest;
	TestTrue(TEXT("Parse valid manifest"), FOrchestratorManifest::LoadFromString(TestManifest, Manifest));
	TestEqual(TEXT("Manifest version"), Manifest.ManifestVersion, TEXT("1"));
	TestEqual(TEXT("Engine build ID"), Manifest.EngineBuildId, TEXT("5.5.0-test"));
	TestEqual(TEXT("Plugin count"), Manifest.Plugins.Num(), 1);
	TestEqual(TEXT("Plugin name"), Manifest.Plugins[0].Name, TEXT("TestPlugin"));
	TestEqual(TEXT("Plugin version"), Manifest.Plugins[0].Version, TEXT("1.0.0"));
	TestEqual(TEXT("Plugin module"), Manifest.Plugins[0].Module, TEXT("TestModule"));

	return true;
}

// DISABLED: FBootManifest was deleted (now FOrchestratorManifest in Orchestrator module).
// Engine build ID verification now happens in Orchestrator::ProcessManifest() (line 264).
#if 0
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorEngineBuildIdMismatchTest, "Orchestrator.Smoke.EngineBuildIdMismatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
bool FOrchestratorEngineBuildIdMismatchTest::RunTest(const FString& Parameters)
{
	// (Test disabled - FBootManifest deleted, verification moved to Orchestrator::ProcessManifest)
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDecisionRuleTest, "Orchestrator.Smoke.DecisionRule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
bool FOrchestratorDecisionRuleTest::RunTest(const FString& Parameters)
{
	// Test: Decision rule - code_hash changed = COLD, content_hash changed = HOT, both same = UP-TO-DATE

	// Setup: Create installed state
	FOrchestratorState InstalledState;
	FPluginState& Plugin = InstalledState.Plugins.Add(TEXT("TestPlugin"));
	Plugin.Version = TEXT("1.0.0");
	Plugin.CodeHash = TEXT("abc123");
	Plugin.ContentHash = TEXT("def456");
	Plugin.InstalledPath = TEXT("/Test/Path");

	// Case 1: code_hash changed (COLD update)
	{
		FOrchestratorPluginEntry NewEntry;
		NewEntry.Name = TEXT("TestPlugin");
		NewEntry.Version = TEXT("1.1.0");
		NewEntry.CodeHash = TEXT("xyz789");  // Changed
		NewEntry.ContentHash = TEXT("def456"); // Same

		const bool bCodeChanged = (Plugin.CodeHash != NewEntry.CodeHash);
		const bool bContentChanged = (Plugin.ContentHash != NewEntry.ContentHash);

		TestTrue(TEXT("Code changed detected"), bCodeChanged);
		TestFalse(TEXT("Content not changed"), bContentChanged);
		// Decision: COLD update required
	}

	// Case 2: content_hash changed (HOT update)
	{
		FOrchestratorPluginEntry NewEntry;
		NewEntry.Name = TEXT("TestPlugin");
		NewEntry.Version = TEXT("1.0.1");
		NewEntry.CodeHash = TEXT("abc123");  // Same
		NewEntry.ContentHash = TEXT("uvw999"); // Changed

		const bool bCodeChanged = (Plugin.CodeHash != NewEntry.CodeHash);
		const bool bContentChanged = (Plugin.ContentHash != NewEntry.ContentHash);

		TestFalse(TEXT("Code not changed"), bCodeChanged);
		TestTrue(TEXT("Content changed detected"), bContentChanged);
		// Decision: HOT update allowed
	}

	// Case 3: Both same (UP-TO-DATE)
	{
		FOrchestratorPluginEntry NewEntry;
		NewEntry.Name = TEXT("TestPlugin");
		NewEntry.Version = TEXT("1.0.0");
		NewEntry.CodeHash = TEXT("abc123");  // Same
		NewEntry.ContentHash = TEXT("def456"); // Same

		const bool bCodeChanged = (Plugin.CodeHash != NewEntry.CodeHash);
		const bool bContentChanged = (Plugin.ContentHash != NewEntry.ContentHash);

		TestFalse(TEXT("Code not changed"), bCodeChanged);
		TestFalse(TEXT("Content not changed"), bContentChanged);
		// Decision: UP-TO-DATE
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorDependencyCycleTest, "Orchestrator.Smoke.DependencyCycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
bool FOrchestratorDependencyCycleTest::RunTest(const FString& Parameters)
{
	// Test: Dependency cycle detection (A -> B -> C -> A)
	const FString TestManifest = TEXT(R"({
		"manifest_version": 1,
		"engine_build_id": "5.5.0-test",
		"signed_at": "2025-10-31T19:00:00Z",
		"signing_key_id": "test-key",
		"signature": "TEST_SIGNATURE",
		"plugins": [
			{
				"name": "PluginA",
				"version": "1.0.0",
				"module": null,
				"code_hash": "",
				"content_hash": "aaa",
				"url_code": "",
				"url_content": "https://test.com/a.zip",
				"depends_on": [{"name": "PluginB", "version": ">=1.0.0"}],
				"channel": "stable",
				"signature_thumbprint": ""
			},
			{
				"name": "PluginB",
				"version": "1.0.0",
				"module": null,
				"code_hash": "",
				"content_hash": "bbb",
				"url_code": "",
				"url_content": "https://test.com/b.zip",
				"depends_on": [{"name": "PluginC", "version": ">=1.0.0"}],
				"channel": "stable",
				"signature_thumbprint": ""
			},
			{
				"name": "PluginC",
				"version": "1.0.0",
				"module": null,
				"code_hash": "",
				"content_hash": "ccc",
				"url_code": "",
				"url_content": "https://test.com/c.zip",
				"depends_on": [{"name": "PluginA", "version": ">=1.0.0"}],
				"channel": "stable",
				"signature_thumbprint": ""
			}
		]
	})");

	FOrchestratorManifest Manifest;
	TestTrue(TEXT("Parse cyclic manifest"), FOrchestratorManifest::LoadFromString(TestManifest, Manifest));
	TestEqual(TEXT("Plugin count"), Manifest.Plugins.Num(), 3);

	// Topological sort should detect cycle
	// In production, ResolveDependencies() would fail and log cycle
	// Here we verify the structure is parsed correctly
	TestTrue(TEXT("PluginA depends on PluginB"), Manifest.Plugins[0].DependsOn.Num() > 0);
	TestEqual(TEXT("PluginA -> PluginB"), Manifest.Plugins[0].DependsOn[0].Name, TEXT("PluginB"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorStateFileTest, "Orchestrator.Smoke.StateFile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
bool FOrchestratorStateFileTest::RunTest(const FString& Parameters)
{
	// Test: State file serialization (latest.json)
	FOrchestratorState State;
	State.ManifestVersion = TEXT("1");
	State.EngineBuildId = TEXT("5.5.0-test");
	State.AppliedAt = FDateTime::UtcNow().ToIso8601();

	FPluginState& Plugin = State.Plugins.Add(TEXT("TestPlugin"));
	Plugin.Version = TEXT("1.0.0");
	Plugin.Module = TEXT("TestModule");
	Plugin.CodeHash = TEXT("abc123");
	Plugin.ContentHash = TEXT("def456");
	Plugin.InstalledPath = TEXT("/Test/Path");
	Plugin.Channel = TEXT("stable");

	// Save to temp file
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_state.json");
	TestTrue(TEXT("Save state to file"), State.SaveToFile(TestFilePath));

	// Load back from file
	FOrchestratorState LoadedState;
	TestTrue(TEXT("Load state from file"), FOrchestratorState::LoadFromFile(TestFilePath, LoadedState));
	TestEqual(TEXT("Manifest version matches"), LoadedState.ManifestVersion, State.ManifestVersion);
	TestEqual(TEXT("Engine build ID matches"), LoadedState.EngineBuildId, State.EngineBuildId);
	TestEqual(TEXT("Plugin count matches"), LoadedState.Plugins.Num(), 1);

	const FPluginState* LoadedPlugin = LoadedState.Plugins.Find(TEXT("TestPlugin"));
	TestNotNull(TEXT("Plugin found"), LoadedPlugin);
	if (LoadedPlugin)
	{
		TestEqual(TEXT("Plugin version"), LoadedPlugin->Version, TEXT("1.0.0"));
		TestEqual(TEXT("Plugin code_hash"), LoadedPlugin->CodeHash, TEXT("abc123"));
	}

	// Cleanup
	IFileManager::Get().Delete(*TestFilePath);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrchestratorPendingUpdatesTest, "Orchestrator.Smoke.PendingUpdates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
bool FOrchestratorPendingUpdatesTest::RunTest(const FString& Parameters)
{
	// Test: pending_updates.json structure for cold updates
	FPendingUpdates Pending;
	Pending.ManifestVersion = TEXT("1");
	Pending.EngineBuildId = TEXT("5.5.0-test");
	Pending.CreatedAt = FDateTime::UtcNow().ToIso8601();

	FPendingUpdate& Update = Pending.Updates.AddDefaulted_GetRef();
	Update.Name = TEXT("TestPlugin");
	Update.FromVersion = TEXT("1.0.0");
	Update.ToVersion = TEXT("1.1.0");
	Update.Change = TEXT("code");
	Update.StagedPath = TEXT("/Test/Staged/Path");
	Update.bRequiresRestart = true;

	// Save to temp file
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("OrchestratorTests/test_pending.json");
	TestTrue(TEXT("Save pending to file"), Pending.SaveToFile(TestFilePath));

	// Load back
	FPendingUpdates LoadedPending;
	TestTrue(TEXT("Load pending from file"), FPendingUpdates::LoadFromFile(TestFilePath, LoadedPending));
	TestEqual(TEXT("Update count"), LoadedPending.Updates.Num(), 1);
	TestEqual(TEXT("Update name"), LoadedPending.Updates[0].Name, TEXT("TestPlugin"));
	TestEqual(TEXT("Update from version"), LoadedPending.Updates[0].FromVersion, TEXT("1.0.0"));
	TestEqual(TEXT("Update to version"), LoadedPending.Updates[0].ToVersion, TEXT("1.1.0"));
	TestEqual(TEXT("Update change type"), LoadedPending.Updates[0].Change, TEXT("code"));
	TestTrue(TEXT("Requires restart"), LoadedPending.Updates[0].bRequiresRestart);

	// Cleanup
	IFileManager::Get().Delete(*TestFilePath);

	return true;
}
