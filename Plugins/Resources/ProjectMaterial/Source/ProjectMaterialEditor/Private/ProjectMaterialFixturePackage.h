// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once

#include "Misc/PackagePath.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace ProjectMaterialFixturePackage
{
// Mount selection does not change the file origin of already loaded packages.
inline bool CheckLoadedOrigin(const FString& PackageName, const FString& ExpectedFile, FString& OutError)
{
	if (const UPackage* Package = FindPackage(nullptr, *PackageName))
	{
		const FString LoadedFile = Package->GetLoadedPath().GetLocalFullPath();
		if (Package->IsDirty() || LoadedFile.IsEmpty() || !FPaths::IsSamePath(
			FPaths::ConvertRelativePathToFull(LoadedFile), FPaths::ConvertRelativePathToFull(ExpectedFile)))
		{
			OutError = FString::Printf(TEXT("Fixture package is dirty or already loaded from a different or unknown file: %s"), *PackageName);
			return false;
		}
	}
	return true;
}
}
