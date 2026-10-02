#pragma once

#include "MeshPartitionTransformerPipeline.h"

#include "ProjectWorldMeshTerrainBuildPipeline.generated.h"

UCLASS()
class UProjectWorldMeshTerrainBuildPipeline final : public UE::MeshPartition::UTransformerPipeline
{
	GENERATED_BODY()

public:
	virtual bool IsEditorOnly() const override { return true; }
};
