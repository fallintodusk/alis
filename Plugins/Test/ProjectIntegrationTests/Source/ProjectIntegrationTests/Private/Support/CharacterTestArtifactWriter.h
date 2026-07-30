// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"

namespace ProjectCharacterTest
{
	inline bool SaveArtifact(
		FAutomationTestBase& Test,
		const FString& Contents,
		const FString& FilePath)
	{
		if (FFileHelper::SaveStringToFile(
			Contents,
			*FilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return true;
		}

		Test.AddError(FString::Printf(
			TEXT("Character capture artifact write failed: %s"),
			*FilePath));
		return false;
	}
}
