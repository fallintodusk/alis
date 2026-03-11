// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ProjectHotReloadManager.h"
#include "Theme/ProjectUIThemeData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

// Test: Valid JSON parses successfully
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_ValidateLayoutString_ValidJson,
	"Project.UI.HotReloadManager.ValidateLayoutString.ValidJson",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_ValidateLayoutString_ValidJson::RunTest(const FString& Parameters)
{
	// Valid JSON with proper structure
	FString ValidJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel",
			"children": [
				{
					"type": "TextBlock",
					"text": "Hello World"
				}
			]
		}
	})");

	// Get subsystem (create if needed for test)
	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance)
	{
		AddError(TEXT("No GameInstance available for test"));
		return false;
	}

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager)
	{
		AddError(TEXT("Failed to get UProjectHotReloadManager subsystem"));
		return false;
	}

	FProjectHotReloadValidationResult Result = Manager->ValidateLayoutString(ValidJson, nullptr);

	TestTrue(TEXT("Validation should succeed"), Result.bSuccess);
	TestEqual(TEXT("Should have no errors"), Result.ErrorMessages.Num(), 0);
	TestEqual(TEXT("Should have 2 widgets (CanvasPanel + TextBlock)"), Result.WidgetCount, 2);

	return true;
}

// Test: Invalid JSON syntax fails validation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_ValidateLayoutString_InvalidSyntax,
	"Project.UI.HotReloadManager.ValidateLayoutString.InvalidSyntax",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_ValidateLayoutString_InvalidSyntax::RunTest(const FString& Parameters)
{
	// Invalid JSON (missing closing brace)
	FString InvalidJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel"
	})");

	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	FProjectHotReloadValidationResult Result = Manager->ValidateLayoutString(InvalidJson, nullptr);

	TestFalse(TEXT("Validation should fail"), Result.bSuccess);
	TestTrue(TEXT("Should have at least one error"), Result.ErrorMessages.Num() > 0);

	return true;
}

// Test: Missing root object fails validation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_ValidateLayoutString_MissingRoot,
	"Project.UI.HotReloadManager.ValidateLayoutString.MissingRoot",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_ValidateLayoutString_MissingRoot::RunTest(const FString& Parameters)
{
	// Valid JSON but missing root object
	FString JsonMissingRoot = TEXT(R"({
		"notRoot": {
			"type": "CanvasPanel"
		}
	})");

	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	FProjectHotReloadValidationResult Result = Manager->ValidateLayoutString(JsonMissingRoot, nullptr);

	TestFalse(TEXT("Validation should fail"), Result.bSuccess);
	TestTrue(TEXT("Should have error about missing root"), Result.ErrorMessages.Num() > 0);

	return true;
}

// Test: Missing widget type fails validation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_ValidateLayoutString_MissingType,
	"Project.UI.HotReloadManager.ValidateLayoutString.MissingType",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_ValidateLayoutString_MissingType::RunTest(const FString& Parameters)
{
	// Widget missing type field
	FString JsonMissingType = TEXT(R"({
		"root": {
			"text": "No type specified"
		}
	})");

	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	FProjectHotReloadValidationResult Result = Manager->ValidateLayoutString(JsonMissingType, nullptr);

	TestFalse(TEXT("Validation should fail"), Result.bSuccess);
	TestTrue(TEXT("Should have error about missing type"), Result.ErrorMessages.Num() > 0);

	return true;
}

// Test: Unknown widget type fails validation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_ValidateLayoutString_UnknownType,
	"Project.UI.HotReloadManager.ValidateLayoutString.UnknownType",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_ValidateLayoutString_UnknownType::RunTest(const FString& Parameters)
{
	// Unknown widget type
	FString JsonUnknownType = TEXT(R"({
		"root": {
			"type": "SuperAwesomeWidgetThatDoesNotExist"
		}
	})");

	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	FProjectHotReloadValidationResult Result = Manager->ValidateLayoutString(JsonUnknownType, nullptr);

	TestFalse(TEXT("Validation should fail"), Result.bSuccess);
	TestTrue(TEXT("Should have error about unknown type"), Result.ErrorMessages.Num() > 0);

	return true;
}

// Test: Invalid theme color generates warning
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_ValidateLayoutString_InvalidThemeColor,
	"Project.UI.HotReloadManager.ValidateLayoutString.InvalidThemeColor",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_ValidateLayoutString_InvalidThemeColor::RunTest(const FString& Parameters)
{
	// TextBlock with invalid color reference
	FString JsonInvalidColor = TEXT(R"({
		"root": {
			"type": "TextBlock",
			"text": "Test",
			"color": "NonexistentColor"
		}
	})");

	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	// Create dummy theme
	UProjectUIThemeData* Theme = NewObject<UProjectUIThemeData>();

	FProjectHotReloadValidationResult Result = Manager->ValidateLayoutString(JsonInvalidColor, Theme);

	TestTrue(TEXT("Validation should succeed (with warning)"), Result.bSuccess);
	TestTrue(TEXT("Should have at least one warning"), Result.WarningMessages.Num() > 0);

	return true;
}

// Test: Snapshot creation stores JSON
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_CreateSnapshot_StoresJson,
	"Project.UI.HotReloadManager.CreateSnapshot.StoresJson",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_CreateSnapshot_StoresJson::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	FString TestJson = TEXT(R"({"root": {"type": "CanvasPanel"}})");
	FString TestPath = TEXT("/Test/Path.json");

	Manager->CreateSnapshot(TestPath, TestJson);

	// Verify snapshot can be rolled back (implicitly tests snapshot storage)
	UWidget* RolledBackWidget = Manager->RollbackToSnapshot(Manager, TestPath, nullptr);
	TestNotNull(TEXT("Rollback should return valid widget"), RolledBackWidget);

	return true;
}

// Test: Rollback without snapshot returns nullptr
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_RollbackToSnapshot_NoSnapshot,
	"Project.UI.HotReloadManager.RollbackToSnapshot.NoSnapshot",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_RollbackToSnapshot_NoSnapshot::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	FString NonexistentPath = TEXT("/Nonexistent/Path.json");

	UWidget* RolledBackWidget = Manager->RollbackToSnapshot(Manager, NonexistentPath, nullptr);
	TestNull(TEXT("Rollback should return nullptr when no snapshot exists"), RolledBackWidget);

	return true;
}

// Test: Complex nested widget tree validates correctly
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_ValidateLayoutString_NestedWidgets,
	"Project.UI.HotReloadManager.ValidateLayoutString.NestedWidgets",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_ValidateLayoutString_NestedWidgets::RunTest(const FString& Parameters)
{
	// Complex nested structure
	FString NestedJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel",
			"children": [
				{
					"type": "VerticalBox",
					"children": [
						{
							"type": "TextBlock",
							"text": "Title"
						},
						{
							"type": "HorizontalBox",
							"children": [
								{
									"type": "Button",
									"text": "OK"
								},
								{
									"type": "Button",
									"text": "Cancel"
								}
							]
						}
					]
				}
			]
		}
	})");

	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	FProjectHotReloadValidationResult Result = Manager->ValidateLayoutString(NestedJson, nullptr);

	TestTrue(TEXT("Validation should succeed"), Result.bSuccess);
	TestEqual(TEXT("Should count all 6 widgets"), Result.WidgetCount, 6);

	return true;
}

// Test: ProgressBar percent out of range generates warning
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectHotReloadManager_ValidateLayoutString_ProgressBarOutOfRange,
	"Project.UI.HotReloadManager.ValidateLayoutString.ProgressBarOutOfRange",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)

bool FProjectHotReloadManager_ValidateLayoutString_ProgressBarOutOfRange::RunTest(const FString& Parameters)
{
	// ProgressBar with percent > 1.0
	FString JsonOutOfRange = TEXT(R"({
		"root": {
			"type": "ProgressBar",
			"percent": 1.5
		}
	})");

	UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance;
	if (!GameInstance) return false;

	UProjectHotReloadManager* Manager = GameInstance->GetSubsystem<UProjectHotReloadManager>();
	if (!Manager) return false;

	FProjectHotReloadValidationResult Result = Manager->ValidateLayoutString(JsonOutOfRange, nullptr);

	TestTrue(TEXT("Validation should succeed (with warning)"), Result.bSuccess);
	TestTrue(TEXT("Should have at least one warning about percent"), Result.WarningMessages.Num() > 0);

	return true;
}
