// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Commandlets/Commandlet.h"
#include "ProjectMaterialSurfaceGenerateCommandlet.generated.h"

UCLASS()
class PROJECTMATERIALEDITOR_API UProjectMaterialSurfaceGenerateCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectMaterialSurfaceGenerateCommandlet();
	virtual int32 Main(const FString& Params) override;
};
