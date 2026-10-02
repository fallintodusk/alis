// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class PROJECTMATERIALEDITOR_API FProjectMaterialCompilerIdentity final
{
public:
	static FString ComputeSha256(const TArrayView<const uint8> Bytes);
	static FString ComputeStringSha256(const FString& Value);
	static FString GetCompilerFingerprint();
};
