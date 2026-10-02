// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Utilities/ProjectSha256.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCoreSha256NormalizedTextTest,
	"ProjectCore.Utilities.Sha256.NormalizedText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCoreSha256NormalizedTextTest::RunTest(const FString& Parameters)
{
	// Python's SHA-256 of the LF-only UTF-8 bytes below; the public payload composer authenticates
	// recipe sources with that same rule.
	const FString Expected = TEXT("4590a31b066b2e91797b7c69e358d3b0713a6b71c5702e07e8b0f82a75712113");
	const FString Lf = TEXT("recipe\n{\n  \"id\": \"caf\u00e9\"\n}\n");
	const TArray<TPair<FString, FString>> Equivalent = {
		{TEXT("LF"), Lf},
		{TEXT("CRLF"), Lf.Replace(TEXT("\n"), TEXT("\r\n"))},
		{TEXT("CR"), Lf.Replace(TEXT("\n"), TEXT("\r"))},
		{TEXT("UTF-8 BOM"), FString::Chr(TCHAR(0xFEFF)) + Lf}};
	for (const TPair<FString, FString>& Case : Equivalent)
	{
		FString Hash;
		TestTrue(FString::Printf(TEXT("%s text hashes."), *Case.Key),
			FProjectSha256::HashNormalizedText(Case.Value, Hash));
		TestEqual(FString::Printf(TEXT("%s text matches the Python digest."), *Case.Key), Hash, Expected);
	}

	FString Changed;
	TestTrue(TEXT("Whitespace-changed text hashes."), FProjectSha256::HashNormalizedText(Lf + TEXT(" "), Changed));
	TestNotEqual(TEXT("A whitespace change is a source change."), Changed, Expected);
	return true;
}

#endif
