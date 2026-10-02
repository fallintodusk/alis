// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Commandlets/Commandlet.h"
#include "ProjectTextureGenerateCommandlet.generated.h"

UCLASS()
class PROJECTTEXTUREEDITOR_API UProjectTextureGenerateCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectTextureGenerateCommandlet();
	virtual int32 Main(const FString& Params) override;
};
