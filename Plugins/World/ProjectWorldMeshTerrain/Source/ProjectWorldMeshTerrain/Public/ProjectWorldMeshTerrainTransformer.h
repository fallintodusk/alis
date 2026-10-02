#pragma once

#include "CoreMinimal.h"
#include "MeshPartitionTransformer.h"

#include "ProjectWorldMeshTerrainTransformer.generated.h"

USTRUCT()
struct PROJECTWORLDMESHTERRAIN_API FProjectWorldMeshTerrainTransformer final
	: public UE::MeshPartition::FTransformer
{
	GENERATED_BODY()

	virtual bool Execute(UE::MeshPartition::FTransformerContext& InContext) const override;
};
