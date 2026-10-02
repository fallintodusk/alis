// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectMaterialPatternDependency
{
	FString PatternId;
	FString OutputContract;
	FString OutputObjectPath;
	FString SemanticIdentity;
	FString PackageSha256;
};

class FProjectMaterialPatternAuthority final
{
public:
	static bool Resolve(
		const FString& PatternId,
		const FString& ManifestPath,
		const FString& PatternPackageRoot,
		const FString& PatternContentRoot,
		FProjectMaterialPatternDependency& OutDependency,
		FString& OutError);
};
