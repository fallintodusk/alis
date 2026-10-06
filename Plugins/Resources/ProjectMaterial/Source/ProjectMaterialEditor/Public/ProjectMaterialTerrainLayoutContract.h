// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectMaterialTerrainLayoutContract
{
	FString LayoutId;
	int32 LayoutVersion = 0;
	FString LayoutSha256;
	FString ReceiptSha256;
	TMap<FName, int32> SemanticChannels;
};

class PROJECTMATERIALEDITOR_API FProjectMaterialTerrainLayoutCompiler final
{
public:
	static bool CompileFile(
		const FString& ReceiptPath,
		const FString& ExpectedLayoutSha256,
		FProjectMaterialTerrainLayoutContract& OutContract,
		FString& OutError);

	static bool CompileJson(
		const FString& Json,
		const FString& ExpectedLayoutSha256,
		FProjectMaterialTerrainLayoutContract& OutContract,
		FString& OutError);
};
