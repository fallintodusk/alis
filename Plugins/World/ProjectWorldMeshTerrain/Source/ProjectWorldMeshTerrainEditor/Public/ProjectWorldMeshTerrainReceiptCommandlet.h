// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ProjectWorldMeshTerrainReceiptCommandlet.generated.h"

UCLASS()
class PROJECTWORLDMESHTERRAINEDITOR_API UProjectWorldMeshTerrainReceiptCommandlet final
	: public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectWorldMeshTerrainReceiptCommandlet();
	virtual int32 Main(const FString& Params) override;
};
