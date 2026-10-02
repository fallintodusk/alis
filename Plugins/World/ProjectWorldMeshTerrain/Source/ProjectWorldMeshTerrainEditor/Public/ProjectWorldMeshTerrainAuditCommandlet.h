// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ProjectWorldMeshTerrainAuditCommandlet.generated.h"

UCLASS()
class PROJECTWORLDMESHTERRAINEDITOR_API UProjectWorldMeshTerrainAuditCommandlet final
	: public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectWorldMeshTerrainAuditCommandlet();
	virtual int32 Main(const FString& Params) override;

	static bool IsSupportedMapPackagePath(const FString& MapPackagePath);
};
