#include "ProjectWorldMeshTerrainTransformer.h"

#include "GameFramework/Actor.h"
#include "ProjectWorldTerrainRuntimeRole.h"

bool FProjectWorldMeshTerrainTransformer::Execute(
	UE::MeshPartition::FTransformerContext& InContext) const
{
	for (const UE::MeshPartition::FTransformerUnit& Unit : InContext.TransformerUnits)
	{
		AActor* Section = UE::MeshPartition::GetSectionChecked(Unit);
		Section->Tags.AddUnique(ProjectWorldTerrainRuntimeRole::RoleTag());
	}
	return true;
}
