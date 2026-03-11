// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for UProjectViewModel
 *
 * Tests verify MVVM base class functionality: property notifications,
 * initialization lifecycle, and data binding.
 */

#include "MVVM/ProjectViewModel.h"
#include "Misc/AutomationTest.h"
#include "TestViewModel.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test: ViewModel Initialization
 * Verifies that ViewModel initializes correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectViewModelInitTest,
	"ProjectUI.ViewModel.Initialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectViewModelInitTest::RunTest(const FString& Parameters)
{
	// Create ViewModel
	UTestViewModel* ViewModel = NewObject<UTestViewModel>();
	TestNotNull(TEXT("ViewModel should be created"), ViewModel);

	// Initialize and shutdown
	ViewModel->Initialize(nullptr);
	ViewModel->Shutdown();

	return true;
}

/**
 * Test: Property Change Notification
 * Verifies that property changes trigger notifications
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectViewModelPropertyNotificationTest,
	"ProjectUI.ViewModel.PropertyNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectViewModelPropertyNotificationTest::RunTest(const FString& Parameters)
{
	UTestViewModel* ViewModel = NewObject<UTestViewModel>();
	ViewModel->Initialize(nullptr);

	// Track property change notifications
	FName LastChangedProperty = NAME_None;
	int32 NotificationCount = 0;

	// Bind to native delegate (lambda-friendly)
	ViewModel->OnPropertyChangedNative.AddLambda([&](FName PropertyName)
	{
		LastChangedProperty = PropertyName;
		NotificationCount++;
	});

	// Change Title property
	ViewModel->SetTitle(FText::FromString(TEXT("Test Title")));
	TestEqual(TEXT("Notification should be fired for Title"),
		LastChangedProperty, FName("Title"));
	TestEqual(TEXT("One notification should be fired"), NotificationCount, 1);

	// Change ItemCount property
	ViewModel->SetItemCount(42);
	TestEqual(TEXT("Notification should be fired for ItemCount"),
		LastChangedProperty, FName("ItemCount"));
	TestEqual(TEXT("Two notifications should be fired"), NotificationCount, 2);

	// Change bIsActive property
	ViewModel->SetbIsActive(true);
	TestEqual(TEXT("Notification should be fired for bIsActive"),
		LastChangedProperty, FName("bIsActive"));
	TestEqual(TEXT("Three notifications should be fired"), NotificationCount, 3);

	ViewModel->Shutdown();
	return true;
}

/**
 * Test: Property Getter/Setter
 * Verifies that property getters and setters work correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectViewModelPropertyAccessTest,
	"ProjectUI.ViewModel.PropertyAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectViewModelPropertyAccessTest::RunTest(const FString& Parameters)
{
	UTestViewModel* ViewModel = NewObject<UTestViewModel>();
	ViewModel->Initialize(nullptr);

	// Test FText property
	FText TestTitle = FText::FromString(TEXT("My Title"));
	ViewModel->SetTitle(TestTitle);
	TestEqual(TEXT("Title getter should return set value"),
		ViewModel->GetTitle().ToString(), TestTitle.ToString());

	// Test int32 property
	ViewModel->SetItemCount(123);
	TestEqual(TEXT("ItemCount getter should return set value"),
		ViewModel->GetItemCount(), 123);

	// Test bool property
	ViewModel->SetbIsActive(true);
	TestTrue(TEXT("bIsActive getter should return true"),
		ViewModel->GetbIsActive());

	ViewModel->SetbIsActive(false);
	TestFalse(TEXT("bIsActive getter should return false"),
		ViewModel->GetbIsActive());

	ViewModel->Shutdown();
	return true;
}

/**
 * Test: No Notification on Same Value
 * Verifies that setting the same value doesn't trigger notification
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectViewModelNoChangeNoNotificationTest,
	"ProjectUI.ViewModel.NoChangeNoNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectViewModelNoChangeNoNotificationTest::RunTest(const FString& Parameters)
{
	UTestViewModel* ViewModel = NewObject<UTestViewModel>();
	ViewModel->Initialize(nullptr);

	int32 NotificationCount = 0;
	ViewModel->OnPropertyChangedNative.AddLambda([&](FName PropertyName)
	{
		NotificationCount++;
	});

	// Set initial value
	ViewModel->SetItemCount(42);
	TestEqual(TEXT("First set should trigger notification"), NotificationCount, 1);

	// Set same value again
	ViewModel->SetItemCount(42);
	TestEqual(TEXT("Setting same value should not trigger notification"), NotificationCount, 1);

	// Set different value
	ViewModel->SetItemCount(100);
	TestEqual(TEXT("Setting different value should trigger notification"), NotificationCount, 2);

	ViewModel->Shutdown();
	return true;
}

/**
 * Test: Multiple Property Changes
 * Verifies that multiple property changes are tracked correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectViewModelMultipleChangesTest,
	"ProjectUI.ViewModel.MultipleChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectViewModelMultipleChangesTest::RunTest(const FString& Parameters)
{
	UTestViewModel* ViewModel = NewObject<UTestViewModel>();
	ViewModel->Initialize(nullptr);

	TArray<FName> ChangedProperties;
	ViewModel->OnPropertyChangedNative.AddLambda([&](FName PropertyName)
	{
		ChangedProperties.Add(PropertyName);
	});

	// Change multiple properties
	ViewModel->SetTitle(FText::FromString(TEXT("Title 1")));
	ViewModel->SetItemCount(10);
	ViewModel->SetbIsActive(true);
	ViewModel->SetTitle(FText::FromString(TEXT("Title 2")));

	// Verify all changes were tracked
	TestEqual(TEXT("Should have 4 property changes"), ChangedProperties.Num(), 4);
	TestEqual(TEXT("First change should be Title"),
		ChangedProperties[0], FName("Title"));
	TestEqual(TEXT("Second change should be ItemCount"),
		ChangedProperties[1], FName("ItemCount"));
	TestEqual(TEXT("Third change should be bIsActive"),
		ChangedProperties[2], FName("bIsActive"));
	TestEqual(TEXT("Fourth change should be Title again"),
		ChangedProperties[3], FName("Title"));

	ViewModel->Shutdown();
	return true;
}

/**
 * Test: Initialize/Shutdown Lifecycle
 * Verifies that initialize and shutdown work correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectViewModelLifecycleTest,
	"ProjectUI.ViewModel.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectViewModelLifecycleTest::RunTest(const FString& Parameters)
{
	UTestViewModel* ViewModel = NewObject<UTestViewModel>();

	// Initialize
	ViewModel->Initialize(nullptr);

	// Can set properties when initialized
	ViewModel->SetTitle(FText::FromString(TEXT("Test")));
	TestEqual(TEXT("Property should be set"), ViewModel->GetTitle().ToString(), FString(TEXT("Test")));

	// Shutdown
	ViewModel->Shutdown();

	return true;
}

/**
 * Test: Multiple Delegates
 * Verifies that multiple listeners can bind to property changes
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectViewModelMultipleDelegatesTest,
	"ProjectUI.ViewModel.MultipleDelegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectViewModelMultipleDelegatesTest::RunTest(const FString& Parameters)
{
	UTestViewModel* ViewModel = NewObject<UTestViewModel>();
	ViewModel->Initialize(nullptr);

	int32 Listener1Count = 0;
	int32 Listener2Count = 0;
	int32 Listener3Count = 0;

	// Bind multiple listeners
	ViewModel->OnPropertyChangedNative.AddLambda([&](FName PropertyName) { Listener1Count++; });
	ViewModel->OnPropertyChangedNative.AddLambda([&](FName PropertyName) { Listener2Count++; });
	ViewModel->OnPropertyChangedNative.AddLambda([&](FName PropertyName) { Listener3Count++; });

	// Change property
	ViewModel->SetTitle(FText::FromString(TEXT("Test")));

	// All listeners should be notified
	TestEqual(TEXT("Listener 1 should be notified"), Listener1Count, 1);
	TestEqual(TEXT("Listener 2 should be notified"), Listener2Count, 1);
	TestEqual(TEXT("Listener 3 should be notified"), Listener3Count, 1);

	ViewModel->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
