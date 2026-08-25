// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ProjectWorldStaticPartitionAuditCommandlet.generated.h"

UCLASS()
class PROJECTWORLDEDITOR_API UProjectWorldStaticPartitionAuditCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectWorldStaticPartitionAuditCommandlet();
	virtual int32 Main(const FString& Params) override;
};
