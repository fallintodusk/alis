// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Commandlets/Commandlet.h"
#include "ProjectMaterialGenerateCommandlet.generated.h"

UCLASS()
class PROJECTMATERIALEDITOR_API UProjectMaterialGenerateCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectMaterialGenerateCommandlet();
	virtual int32 Main(const FString& Params) override;
};
