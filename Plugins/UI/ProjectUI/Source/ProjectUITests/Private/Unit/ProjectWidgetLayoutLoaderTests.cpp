// Copyright ALIS. All Rights Reserved.

#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Theme/ProjectUIThemeData.h"
#include "Misc/AutomationTest.h"
#include "Components/CanvasPanel.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"

/**
 * Unit tests for UProjectWidgetLayoutLoader
 *
 * Tests JSON parsing, widget instantiation, property binding, and theme integration.
 */

#if WITH_DEV_AUTOMATION_TESTS

// Test: JSON parsing with valid JSON
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_ValidJSON,
	"ProjectUI.WidgetLayoutLoader.ValidJSON",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_ValidJSON::RunTest(const FString& Parameters)
{
	// Arrange: Valid JSON with simple CanvasPanel
	const FString ValidJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel",
			"name": "TestCanvas"
		}
	})");

	// Act: Parse JSON and create widget
	UWidget* Widget = UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), ValidJson, nullptr);

	// Assert
	TestNotNull(TEXT("Widget should be created from valid JSON"), Widget);
	TestTrue(TEXT("Widget should be CanvasPanel"), Widget->IsA<UCanvasPanel>());

	return true;
}

// Test: JSON parsing with invalid JSON
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_InvalidJSON,
	"Project.UI.WidgetLayoutLoader.InvalidJSON",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_InvalidJSON::RunTest(const FString& Parameters)
{
	// Arrange: Invalid JSON (missing closing brace)
	const FString InvalidJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel"
	})");

	// Act: Attempt to parse invalid JSON
	UWidget* Widget = UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), InvalidJson, nullptr);

	// Assert
	TestNull(TEXT("Widget should be null for invalid JSON"), Widget);

	return true;
}

// Test: Widget hierarchy creation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_Hierarchy,
	"Project.UI.WidgetLayoutLoader.Hierarchy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_Hierarchy::RunTest(const FString& Parameters)
{
	// Arrange: JSON with parent and child widgets
	const FString HierarchyJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel",
			"name": "ParentCanvas",
			"children": [
				{
					"type": "Button",
					"name": "ChildButton"
				},
				{
					"type": "TextBlock",
					"name": "ChildText",
					"text": "Hello World"
				}
			]
		}
	})");

	// Act: Create widget hierarchy
	UWidget* Widget = UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), HierarchyJson, nullptr);
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Widget);

	// Assert
	TestNotNull(TEXT("Root widget should be created"), Canvas);
	TestEqual(TEXT("Canvas should have 2 children"), Canvas->GetChildrenCount(), 2);

	UButton* Button = Cast<UButton>(Canvas->GetChildAt(0));
	UTextBlock* Text = Cast<UTextBlock>(Canvas->GetChildAt(1));

	TestNotNull(TEXT("First child should be Button"), Button);
	TestNotNull(TEXT("Second child should be TextBlock"), Text);
	TestEqual(TEXT("TextBlock should have correct text"), Text->GetText().ToString(), FString(TEXT("Hello World")));

	return true;
}

// Test: Box spacing applies as inter-item slot padding and composes with explicit slot padding
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_BoxSpacing,
	"Project.UI.WidgetLayoutLoader.BoxSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_BoxSpacing::RunTest(const FString& Parameters)
{
	const FString HorizontalJson = TEXT(R"({
		"root": {
			"type": "HorizontalBox",
			"name": "HBox",
			"spacing": 12,
			"children": [
				{
					"type": "TextBlock",
					"name": "A",
					"text": "A",
					"slot": {
						"padding": {"left": 1, "top": 2, "right": 3, "bottom": 4}
					}
				},
				{
					"type": "TextBlock",
					"name": "B",
					"text": "B"
				}
			]
		}
	})");

	UHorizontalBox* HBox = Cast<UHorizontalBox>(
		UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), HorizontalJson, nullptr));
	TestNotNull(TEXT("HorizontalBox should be created"), HBox);
	if (!HBox)
	{
		return false;
	}

	TestEqual(TEXT("HorizontalBox should have 2 children"), HBox->GetChildrenCount(), 2);

	UHorizontalBoxSlot* FirstHSlot = Cast<UHorizontalBoxSlot>(HBox->GetChildAt(0)->Slot);
	UHorizontalBoxSlot* SecondHSlot = Cast<UHorizontalBoxSlot>(HBox->GetChildAt(1)->Slot);
	TestNotNull(TEXT("First horizontal slot should exist"), FirstHSlot);
	TestNotNull(TEXT("Second horizontal slot should exist"), SecondHSlot);
	if (!FirstHSlot || !SecondHSlot)
	{
		return false;
	}

	const FMargin FirstHPadding = FirstHSlot->GetPadding();
	const FMargin SecondHPadding = SecondHSlot->GetPadding();
	TestEqual(TEXT("First horizontal slot Left padding"), FirstHPadding.Left, 1.0f);
	TestEqual(TEXT("First horizontal slot Top padding"), FirstHPadding.Top, 2.0f);
	TestEqual(TEXT("First horizontal slot Right padding includes spacing"), FirstHPadding.Right, 15.0f);
	TestEqual(TEXT("First horizontal slot Bottom padding"), FirstHPadding.Bottom, 4.0f);
	TestEqual(TEXT("Last horizontal slot Right padding should remain zero"), SecondHPadding.Right, 0.0f);

	const FString VerticalJson = TEXT(R"({
		"root": {
			"type": "VerticalBox",
			"name": "VBox",
			"spacing": 7,
			"children": [
				{
					"type": "TextBlock",
					"name": "TopItem",
					"text": "Top",
					"slot": {
						"padding": {"bottom": 1}
					}
				},
				{
					"type": "TextBlock",
					"name": "BottomItem",
					"text": "Bottom"
				}
			]
		}
	})");

	UVerticalBox* VBox = Cast<UVerticalBox>(
		UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), VerticalJson, nullptr));
	TestNotNull(TEXT("VerticalBox should be created"), VBox);
	if (!VBox)
	{
		return false;
	}

	TestEqual(TEXT("VerticalBox should have 2 children"), VBox->GetChildrenCount(), 2);

	UVerticalBoxSlot* FirstVSlot = Cast<UVerticalBoxSlot>(VBox->GetChildAt(0)->Slot);
	UVerticalBoxSlot* SecondVSlot = Cast<UVerticalBoxSlot>(VBox->GetChildAt(1)->Slot);
	TestNotNull(TEXT("First vertical slot should exist"), FirstVSlot);
	TestNotNull(TEXT("Second vertical slot should exist"), SecondVSlot);
	if (!FirstVSlot || !SecondVSlot)
	{
		return false;
	}

	const FMargin FirstVPadding = FirstVSlot->GetPadding();
	const FMargin SecondVPadding = SecondVSlot->GetPadding();
	TestEqual(TEXT("First vertical slot Bottom padding includes spacing"), FirstVPadding.Bottom, 8.0f);
	TestEqual(TEXT("Last vertical slot Bottom padding should remain zero"), SecondVPadding.Bottom, 0.0f);

	return true;
}

// Test: Widget property binding (position, size, text)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_Properties,
	"Project.UI.WidgetLayoutLoader.Properties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_Properties::RunTest(const FString& Parameters)
{
	// Arrange: JSON with widget properties
	const FString PropsJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel",
			"children": [
				{
					"type": "TextBlock",
					"name": "TestText",
					"text": "Test Message",
					"position": {"x": 100, "y": 200}
				}
			]
		}
	})");

	// Act: Create widget with properties
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(
		UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), PropsJson, nullptr)
	);

	// Assert
	TestNotNull(TEXT("Canvas should be created"), Canvas);
	TestEqual(TEXT("Canvas should have 1 child"), Canvas->GetChildrenCount(), 1);

	UTextBlock* Text = Cast<UTextBlock>(Canvas->GetChildAt(0));
	TestNotNull(TEXT("Child should be TextBlock"), Text);
	TestEqual(TEXT("Text should match JSON"), Text->GetText().ToString(), FString(TEXT("Test Message")));

	return true;
}

// Test: Theme color resolution
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_ThemeColors,
	"Project.UI.WidgetLayoutLoader.ThemeColors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_ThemeColors::RunTest(const FString& Parameters)
{
	// Arrange: Create test theme
	UProjectUIThemeData* TestTheme = NewObject<UProjectUIThemeData>(GetTransientPackage());
	TestTheme->Colors.Primary = FLinearColor::Red;
	TestTheme->Colors.Secondary = FLinearColor::Blue;

	const FString ThemeJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel",
			"children": [
				{
					"type": "TextBlock",
					"text": "Test",
					"color": "Primary"
				}
			]
		}
	})");

	// Act: Create widget with theme color reference
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(
		UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), ThemeJson, TestTheme)
	);

	// Assert
	TestNotNull(TEXT("Canvas should be created"), Canvas);
	UTextBlock* Text = Cast<UTextBlock>(Canvas->GetChildAt(0));
	TestNotNull(TEXT("TextBlock should be created"), Text);

	// Note: Actual color application testing would require more complex widget initialization
	// This test verifies the structure is created correctly

	return true;
}

// Test: Missing required fields
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_MissingFields,
	"Project.UI.WidgetLayoutLoader.MissingFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_MissingFields::RunTest(const FString& Parameters)
{
	// Arrange: JSON missing "type" field
	const FString MissingTypeJson = TEXT(R"({
		"root": {
			"name": "TestWidget"
		}
	})");

	// Act: Attempt to create widget without type
	UWidget* Widget = UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), MissingTypeJson, nullptr);

	// Assert
	TestNull(TEXT("Widget should be null when 'type' field is missing"), Widget);

	return true;
}

// Test: ProgressBar creation with percent property
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_ProgressBar,
	"Project.UI.WidgetLayoutLoader.ProgressBar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_ProgressBar::RunTest(const FString& Parameters)
{
	// Arrange: JSON with progress bar
	const FString ProgressJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel",
			"children": [
				{
					"type": "ProgressBar",
					"name": "TestProgress",
					"percent": 0.75
				}
			]
		}
	})");

	// Act: Create progress bar
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(
		UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), ProgressJson, nullptr)
	);

	// Assert
	TestNotNull(TEXT("Canvas should be created"), Canvas);
	UProgressBar* ProgressBar = Cast<UProgressBar>(Canvas->GetChildAt(0));
	TestNotNull(TEXT("ProgressBar should be created"), ProgressBar);
	TestEqual(TEXT("ProgressBar percent should be 0.75"), ProgressBar->GetPercent(), 0.75f);

	return true;
}

// Test: Multiple widget types instantiation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_MultipleTypes,
	"Project.UI.WidgetLayoutLoader.MultipleTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_MultipleTypes::RunTest(const FString& Parameters)
{
	// Arrange: JSON with various widget types
	const FString MultiTypeJson = TEXT(R"({
		"root": {
			"type": "CanvasPanel",
			"children": [
				{"type": "Button"},
				{"type": "TextBlock", "text": "Text"},
				{"type": "Image"},
				{"type": "ProgressBar"}
			]
		}
	})");

	// Act: Create widgets
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(
		UProjectWidgetLayoutLoader::LoadLayoutFromString(GetTransientPackage(), MultiTypeJson, nullptr)
	);

	// Assert
	TestNotNull(TEXT("Canvas should be created"), Canvas);
	TestEqual(TEXT("Should have 4 children"), Canvas->GetChildrenCount(), 4);

	TestNotNull(TEXT("Child 0 should be Button"), Cast<UButton>(Canvas->GetChildAt(0)));
	TestNotNull(TEXT("Child 1 should be TextBlock"), Cast<UTextBlock>(Canvas->GetChildAt(1)));
	TestNotNull(TEXT("Child 2 should be Image"), Cast<UImage>(Canvas->GetChildAt(2)));
	TestNotNull(TEXT("Child 3 should be ProgressBar"), Cast<UProgressBar>(Canvas->GetChildAt(3)));

	return true;
}

// Test: GetUIConfigPath utility function
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWidgetLayoutLoader_ConfigPath,
	"ProjectUI.WidgetLayoutLoader.ConfigPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWidgetLayoutLoader_ConfigPath::RunTest(const FString& Parameters)
{
	// Act: Get UI config path (Redirects to ProjectUI plugin data)
	FString ConfigPath = UProjectWidgetLayoutLoader::GetUIConfigPath(TEXT("TestWidget.json"));

	// Assert: Should point to .../Plugins/.../ProjectUI/Content/Data/TestWidget.json
	TestTrue(TEXT("Config path should contain 'ProjectUI/Data'"), ConfigPath.Contains(TEXT("ProjectUI/Data")));
	TestTrue(TEXT("Config path should end with 'TestWidget.json'"), ConfigPath.EndsWith(TEXT("TestWidget.json")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
