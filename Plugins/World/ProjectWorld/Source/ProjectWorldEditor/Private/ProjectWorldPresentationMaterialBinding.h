// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once
#include "CoreMinimal.h"

struct FProjectWorldPresentationResources;
class UMaterialInterface;

struct FProjectWorldMaterialReference
{
	UMaterialInterface* Material=nullptr;
	FString ObjectPath;
	FString PackageSha256;
	FString SemanticIdentity;
	FString ManifestSha256;
};

namespace ProjectWorldPresentationMaterialBinding
{
	bool Resolve(const FString& BindingId,FProjectWorldMaterialReference& OutReference,FString& OutError);
	bool ResolveTerrain(
		FProjectWorldPresentationResources& InOutResources,
		FString& OutError);
}
