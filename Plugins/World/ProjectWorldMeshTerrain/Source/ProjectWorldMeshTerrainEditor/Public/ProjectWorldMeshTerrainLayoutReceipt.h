// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;

struct FProjectWorldMeshTerrainLayoutReceipt
{
	FString CanonicalSurfaceContractId;
	int32 CanonicalSurfaceContractVersion = 0;
	FString CanonicalSurfaceContractSha256;
	FString EngineIdentity;
	FString LayoutSha256;
	FString SharedDefinitionPackageSha256;
	FString BuildPolicySha256;
	FString ReceiptPayload;
	FString ReceiptSha256;
};

class PROJECTWORLDMESHTERRAINEDITOR_API FProjectWorldMeshTerrainLayoutReceiptContract final
{
public:
	static bool Build(
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldMeshTerrainLayoutReceipt& OutReceipt,
		FString& OutError);

	static bool Save(
		const FProjectWorldMeshTerrainLayoutReceipt& Receipt,
		const FString& OutputPath,
		FString& OutError);

	static FString GetLayoutPayload();
	static FString GetBuildPolicyPayload();
};
