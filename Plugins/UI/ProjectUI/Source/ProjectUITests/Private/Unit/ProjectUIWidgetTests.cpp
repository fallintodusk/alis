// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for ProjectUI Widget Base Classes
 *
 * Tests verify UProjectUserWidget and UProjectButton can be created.
 */

#include "Widgets/ProjectUserWidget.h"
#include "Widgets/ProjectButton.h"
#include "MVVM/ProjectViewModel.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test: Widget Creation
 * Verifies UProjectButton (concrete widget) can be created
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIWidgetCreationTest,
	"ProjectUI.Widget.Creation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIWidgetCreationTest::RunTest(const FString& Parameters)
{
	// UProjectUserWidget is Abstract, so use concrete subclass UProjectButton
	UProjectButton* Widget = NewObject<UProjectButton>();
	TestNotNull(TEXT("Widget should be created"), Widget);

	return true;
}

/**
 * Test: Widget ViewModel Binding
 * Verifies widget can bind to ViewModel
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIWidgetViewModelBindingTest,
	"ProjectUI.Widget.ViewModelBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIWidgetViewModelBindingTest::RunTest(const FString& Parameters)
{
	// UProjectUserWidget is Abstract, so use concrete subclass UProjectButton
	UProjectButton* Widget = NewObject<UProjectButton>();
	UProjectViewModel* ViewModel = NewObject<UProjectViewModel>();

	// Bind ViewModel
	Widget->SetViewModel(ViewModel);
	TestEqual(TEXT("ViewModel should be bound"), Widget->GetViewModel(), ViewModel);

	return true;
}

/**
 * Test: Widget ViewModel Unbinding
 * Verifies widget can unbind from ViewModel
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIWidgetViewModelUnbindingTest,
	"ProjectUI.Widget.ViewModelUnbinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIWidgetViewModelUnbindingTest::RunTest(const FString& Parameters)
{
	// UProjectUserWidget is Abstract, so use concrete subclass UProjectButton
	UProjectButton* Widget = NewObject<UProjectButton>();
	UProjectViewModel* ViewModel = NewObject<UProjectViewModel>();

	// Bind then unbind
	Widget->SetViewModel(ViewModel);
	Widget->SetViewModel(nullptr);
	TestNull(TEXT("ViewModel should be unbound"), Widget->GetViewModel());

	return true;
}

/**
 * Test: Button Creation
 * Verifies UProjectButton can be created
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIButtonCreationTest,
	"ProjectUI.Button.Creation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIButtonCreationTest::RunTest(const FString& Parameters)
{
	UProjectButton* Button = NewObject<UProjectButton>();
	TestNotNull(TEXT("Button should be created"), Button);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
