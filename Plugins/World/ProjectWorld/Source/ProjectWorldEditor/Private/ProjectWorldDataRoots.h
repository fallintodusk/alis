// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldDataRoots
{
	FString PluginName;
	FString MountRoot;
	FString GeneratedPackageRoot;
	FString AuthoredPackageRoot;
	FString ContentRoot;
	FString DataRoot;
	FString ManifestRoot;

	bool IsGeneratedPackage(const FString& PackagePath) const;
	bool IsAuthoredPackage(const FString& PackagePath) const;

	static bool Resolve(
		const FString& PluginName,
		FProjectWorldDataRoots& OutRoots,
		FString& OutError);
};
