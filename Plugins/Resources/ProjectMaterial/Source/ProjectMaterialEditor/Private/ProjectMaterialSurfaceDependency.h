#pragma once
#include "CoreMinimal.h"

struct FProjectMaterialSurfaceDependency
{
	FString PatternId;
	FString OutputContract;
	FString OutputObjectPath;
	FString SemanticIdentity;
	FString PackageSha256;
	TMap<FString, FString> NativeResourceHashes;
};
