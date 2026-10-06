// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

namespace ProjectWorldSchemaReference
{
	PROJECTWORLDEDITOR_API bool ResolvesToCanonical(
		const FString& DocumentPath,
		const FString& DeclaredReference,
		const TCHAR* ExpectedSchemaFilename,
		FString& OutError);
}
