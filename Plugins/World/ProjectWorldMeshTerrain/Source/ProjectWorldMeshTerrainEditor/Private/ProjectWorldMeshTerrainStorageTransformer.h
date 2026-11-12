#pragma once

#include "CoreMinimal.h"
#include "MeshPartitionTransformer.h"
#include "ProjectWorldMeshTerrainStorageTransformer.generated.h"

/** Stores compiled positions locally while retaining their parent-space world geometry. */
USTRUCT()
struct FProjectWorldMeshTerrainStorageTransformer : public UE::MeshPartition::FTransformer
{
	GENERATED_BODY()

	virtual bool Execute(UE::MeshPartition::FTransformerContext& Context) const override;
	virtual void GatherDependencies(UE::MeshPartition::IDependencyInterface& Dependencies) const override;
};
