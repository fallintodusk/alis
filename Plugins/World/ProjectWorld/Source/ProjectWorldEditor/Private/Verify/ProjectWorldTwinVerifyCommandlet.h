// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ProjectWorldTwinVerifyCommandlet.generated.h"

UCLASS()
class UProjectWorldTwinVerifyCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectWorldTwinVerifyCommandlet();
	virtual int32 Main(const FString& Params) override;
};
