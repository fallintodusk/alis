// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Commandlets/Commandlet.h"

#include "ProjectMaterialTerrainLayoutCommandlet.generated.h"

UCLASS()
class PROJECTMATERIALEDITOR_API UProjectMaterialTerrainLayoutCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectMaterialTerrainLayoutCommandlet();
	virtual int32 Main(const FString& Params) override;
};
