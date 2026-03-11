// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ProjectValidationHelpers.h"
#include "ProjectErrorCodes.h"

/**
 * Unit tests for ProjectCore helper utilities (minimal test suite)
 */

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// FProjectValidationHelpers - Basic Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_ValidName,
	"ProjectCore.Helpers.ValidationHelpers.ValidName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_ValidName::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;
	FName ValidFName = FName(TEXT("TestName"));

	bool bIsValid = FProjectValidationHelpers::IsValidName(ValidFName, Errors);

	TestTrue(TEXT("Valid FName passes"), bIsValid);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_InvalidName,
	"ProjectCore.Helpers.ValidationHelpers.InvalidName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_InvalidName::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;
	FName InvalidFName = NAME_None;

	bool bIsValid = FProjectValidationHelpers::IsValidName(InvalidFName, Errors);

	TestFalse(TEXT("NAME_None fails"), bIsValid);
	TestEqual(TEXT("One error"), Errors.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_ValidPath,
	"ProjectCore.Helpers.ValidationHelpers.ValidPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_ValidPath::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;
	FSoftObjectPath ValidPath = FSoftObjectPath(TEXT("/Game/Maps/TestMap"));

	bool bIsValid = FProjectValidationHelpers::IsValidSoftObjectPath(ValidPath, Errors);

	TestTrue(TEXT("Valid path passes"), bIsValid);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_InRange,
	"ProjectCore.Helpers.ValidationHelpers.InRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_InRange::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;

	// Test valid range
	bool bInRange = FProjectValidationHelpers::IsInRange(50, 0, 100, Errors);
	TestTrue(TEXT("Value in range passes"), bInRange);

	// Test out of range
	Errors.Empty();
	bool bOutOfRange = FProjectValidationHelpers::IsInRange(150, 0, 100, Errors);
	TestFalse(TEXT("Value out of range fails"), bOutOfRange);
	TestTrue(TEXT("Error reported"), Errors.Num() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_ValidText,
	"ProjectCore.Helpers.ValidationHelpers.ValidText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_ValidText::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;

	// Valid text
	FText ValidText = FText::FromString(TEXT("TestText"));
	bool bIsValid = FProjectValidationHelpers::IsValidText(ValidText, Errors);
	TestTrue(TEXT("Valid FText passes"), bIsValid);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	// Empty text
	Errors.Empty();
	FText EmptyText = FText::GetEmpty();
	bool bIsInvalid = FProjectValidationHelpers::IsValidText(EmptyText, Errors);
	TestFalse(TEXT("Empty FText fails"), bIsInvalid);
	TestEqual(TEXT("One error"), Errors.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_ValidString,
	"ProjectCore.Helpers.ValidationHelpers.ValidString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_ValidString::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;

	// Valid string
	FString ValidString = TEXT("TestString");
	bool bIsValid = FProjectValidationHelpers::IsValidString(ValidString, Errors);
	TestTrue(TEXT("Valid FString passes"), bIsValid);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	// Empty string
	Errors.Empty();
	FString EmptyString = TEXT("");
	bool bIsInvalid = FProjectValidationHelpers::IsValidString(EmptyString, Errors);
	TestFalse(TEXT("Empty FString fails"), bIsInvalid);
	TestEqual(TEXT("One error"), Errors.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_ValidArray,
	"ProjectCore.Helpers.ValidationHelpers.ValidArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_ValidArray::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;

	// Valid array
	TArray<int32> ValidArray = {1, 2, 3};
	bool bIsValid = FProjectValidationHelpers::IsValidArray(ValidArray, Errors);
	TestTrue(TEXT("Valid array passes"), bIsValid);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	// Empty array
	Errors.Empty();
	TArray<int32> EmptyArray;
	bool bIsInvalid = FProjectValidationHelpers::IsValidArray(EmptyArray, Errors);
	TestFalse(TEXT("Empty array fails"), bIsInvalid);
	TestEqual(TEXT("One error"), Errors.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_IsPositive,
	"ProjectCore.Helpers.ValidationHelpers.IsPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_IsPositive::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;

	// Positive value
	bool bIsPositive = FProjectValidationHelpers::IsPositive(10, Errors);
	TestTrue(TEXT("Positive value passes"), bIsPositive);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	// Zero value
	Errors.Empty();
	bool bIsZero = FProjectValidationHelpers::IsPositive(0, Errors);
	TestFalse(TEXT("Zero fails"), bIsZero);
	TestTrue(TEXT("Error reported"), Errors.Num() > 0);

	// Negative value
	Errors.Empty();
	bool bIsNegative = FProjectValidationHelpers::IsPositive(-5, Errors);
	TestFalse(TEXT("Negative fails"), bIsNegative);
	TestTrue(TEXT("Error reported"), Errors.Num() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_ValidateArrayElements,
	"ProjectCore.Helpers.ValidationHelpers.ValidateArrayElements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_ValidateArrayElements::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;

	// Array with all valid elements
	TArray<int32> ValidElements = {1, 2, 3};
	TFunction<bool(const int32&, TArray<FText>&)> Validator = [](const int32& Value, TArray<FText>& OutErrors) -> bool
	{
		return FProjectValidationHelpers::IsPositive(Value, OutErrors);
	};

	bool bAllValid = FProjectValidationHelpers::ValidateArrayElements(ValidElements, Validator, Errors);
	TestTrue(TEXT("All valid elements pass"), bAllValid);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	// Array with some invalid elements
	Errors.Empty();
	TArray<int32> MixedElements = {1, -2, 3, 0};
	bool bHasInvalid = FProjectValidationHelpers::ValidateArrayElements(MixedElements, Validator, Errors);
	TestFalse(TEXT("Invalid elements fail"), bHasInvalid);
	TestTrue(TEXT("Errors reported"), Errors.Num() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ValidationHelpers_ErrorAccumulation,
	"ProjectCore.Helpers.ValidationHelpers.ErrorAccumulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ValidationHelpers_ErrorAccumulation::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;

	// Multiple validations with accumulation
	bool bValid1 = FProjectValidationHelpers::IsValidName(NAME_None, Errors);
	bool bValid2 = FProjectValidationHelpers::IsValidText(FText::GetEmpty(), Errors);
	bool bValid3 = FProjectValidationHelpers::IsValidString(TEXT(""), Errors);

	// All should fail
	TestFalse(TEXT("First validation fails"), bValid1);
	TestFalse(TEXT("Second validation fails"), bValid2);
	TestFalse(TEXT("Third validation fails"), bValid3);

	// All errors accumulated
	TestEqual(TEXT("All errors accumulated"), Errors.Num(), 3);

	return true;
}

// ============================================================================
// ProjectErrorCodes - Range Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ErrorCodes_ValidRange,
	"ProjectCore.Helpers.ErrorCodes.ValidRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ErrorCodes_ValidRange::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("100 valid (min)"), ProjectErrorCodes::IsValidErrorCode(100));
	TestTrue(TEXT("500 valid (mid)"), ProjectErrorCodes::IsValidErrorCode(500));
	TestTrue(TEXT("999 valid (max)"), ProjectErrorCodes::IsValidErrorCode(999));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ErrorCodes_InvalidRange,
	"ProjectCore.Helpers.ErrorCodes.InvalidRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ErrorCodes_InvalidRange::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("99 invalid (below)"), ProjectErrorCodes::IsValidErrorCode(99));
	TestFalse(TEXT("1000 invalid (above)"), ProjectErrorCodes::IsValidErrorCode(1000));
	TestFalse(TEXT("0 invalid"), ProjectErrorCodes::IsValidErrorCode(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ErrorCodes_PhaseRanges,
	"ProjectCore.Helpers.ErrorCodes.PhaseRanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ErrorCodes_PhaseRanges::RunTest(const FString& Parameters)
{
	// Verify phase ranges don't overlap
	TestTrue(TEXT("ResolveAssets in range"),
		ProjectErrorCodes::ResolveAssets::ManifestNotFound >= 100 &&
		ProjectErrorCodes::ResolveAssets::ManifestNotFound < 200);

	TestTrue(TEXT("MountContent in range"),
		ProjectErrorCodes::MountContent::DownloadFailed >= 200 &&
		ProjectErrorCodes::MountContent::DownloadFailed < 300);

	TestTrue(TEXT("PreloadAssets in range"),
		ProjectErrorCodes::PreloadAssets::AssetLoadFailed >= 300 &&
		ProjectErrorCodes::PreloadAssets::AssetLoadFailed < 400);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
