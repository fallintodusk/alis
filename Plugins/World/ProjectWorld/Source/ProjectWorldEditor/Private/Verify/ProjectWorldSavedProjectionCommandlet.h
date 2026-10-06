// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ProjectWorldSavedProjectionCommandlet.generated.h"

UCLASS()
class UProjectWorldSavedProjectionCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectWorldSavedProjectionCommandlet();
	virtual int32 Main(const FString& Params) override;
};
