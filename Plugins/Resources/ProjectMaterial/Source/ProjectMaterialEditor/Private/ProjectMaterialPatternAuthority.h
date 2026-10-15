// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ProjectMaterialSurfaceDependency.h"

class FProjectMaterialPatternAuthority final
{
public:
	static bool Resolve(
		const FString& PatternId,
		const FString& ManifestPath,
		const FString& PatternPackageRoot,
		const FString& PatternContentRoot,
		FProjectMaterialSurfaceDependency& OutDependency,
		FString& OutError);
};
