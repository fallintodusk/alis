// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ProjectWorldRealizeCommandlet.generated.h"

UCLASS()
class PROJECTWORLDEDITOR_API UProjectWorldRealizeCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectWorldRealizeCommandlet();
	virtual int32 Main(const FString& Params) override;
};
