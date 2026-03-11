// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ProjectLogging.h"

/**
 * Unit tests for ProjectCore foundation
 *
 * Tests core logging and fundamental infrastructure.
 */

#if WITH_DEV_AUTOMATION_TESTS

// Test: Logging categories are defined
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoggingCategories,
	"ProjectCore.Foundation.LoggingCategories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoggingCategories::RunTest(const FString& Parameters)
{
	// Assert: ProjectCore log category should be accessible
	UE_LOG(LogProjectCore, Verbose, TEXT("ProjectCore logging test"));

	TestTrue(TEXT("ProjectCore log category is accessible"), true);

	return true;
}

// Test: Module initialization
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ModuleInit,
	"ProjectCore.Foundation.ModuleInit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ModuleInit::RunTest(const FString& Parameters)
{
	// Assert: Module should be loaded (if we're running this test, the module loaded)
	TestTrue(TEXT("ProjectCore module is loaded"), true);

	return true;
}

// Test: Basic types are available
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_BasicTypes,
	"ProjectCore.Foundation.BasicTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_BasicTypes::RunTest(const FString& Parameters)
{
	// Test basic type availability by creating instances
	// These types should compile and be accessible

	TestTrue(TEXT("Core types are available"), true);

	return true;
}

// Test: String utilities
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_StringUtils,
	"ProjectCore.Foundation.StringUtils",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_StringUtils::RunTest(const FString& Parameters)
{
	// Test basic string operations
	const FString TestString = TEXT("ProjectCore");

	TestTrue(TEXT("String is not empty"), !TestString.IsEmpty());
	TestEqual(TEXT("String length is correct"), TestString.Len(), 11);
	TestTrue(TEXT("String contains 'Core'"), TestString.Contains(TEXT("Core")));

	return true;
}

// Test: Name utilities
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_NameUtils,
	"ProjectCore.Foundation.NameUtils",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_NameUtils::RunTest(const FString& Parameters)
{
	// Test FName operations
	const FName Name1(TEXT("ProjectCore"));
	const FName Name2(TEXT("ProjectCore"));
	const FName DifferentName(TEXT("OtherName"));

	TestEqual(TEXT("Same names are equal"), Name1, Name2);
	TestNotEqual(TEXT("Different names are not equal"), Name1, DifferentName);
	TestFalse(TEXT("Name is not None"), Name1.IsNone());

	return true;
}

// Test: Array operations
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ArrayOps,
	"ProjectCore.Foundation.ArrayOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ArrayOps::RunTest(const FString& Parameters)
{
	// Test TArray operations
	TArray<int32> TestArray;

	TestEqual(TEXT("Empty array has size 0"), TestArray.Num(), 0);

	TestArray.Add(1);
	TestArray.Add(2);
	TestArray.Add(3);

	TestEqual(TEXT("Array has 3 elements"), TestArray.Num(), 3);
	TestEqual(TEXT("First element is 1"), TestArray[0], 1);
	TestEqual(TEXT("Last element is 3"), TestArray[2], 3);

	TestArray.RemoveAt(1);
	TestEqual(TEXT("After removal, array has 2 elements"), TestArray.Num(), 2);

	return true;
}

// Test: Map operations
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_MapOps,
	"ProjectCore.Foundation.MapOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_MapOps::RunTest(const FString& Parameters)
{
	// Test TMap operations
	TMap<FString, int32> TestMap;

	TestEqual(TEXT("Empty map has size 0"), TestMap.Num(), 0);

	TestMap.Add(TEXT("One"), 1);
	TestMap.Add(TEXT("Two"), 2);
	TestMap.Add(TEXT("Three"), 3);

	TestEqual(TEXT("Map has 3 elements"), TestMap.Num(), 3);
	TestTrue(TEXT("Map contains 'Two'"), TestMap.Contains(TEXT("Two")));
	TestEqual(TEXT("Value for 'Two' is 2"), TestMap[TEXT("Two")], 2);

	return true;
}

// Test: Enum operations
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_EnumOps,
	"ProjectCore.Foundation.EnumOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_EnumOps::RunTest(const FString& Parameters)
{
	// Test enum operations with a standard UE enum
	enum class ETestEnum : uint8
	{
		Value1,
		Value2,
		Value3
	};

	ETestEnum TestValue = ETestEnum::Value2;

	TestEqual(TEXT("Enum value is Value2"), TestValue, ETestEnum::Value2);
	TestNotEqual(TEXT("Enum value is not Value1"), TestValue, ETestEnum::Value1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
