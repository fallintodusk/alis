#include "ProjectWorldMeshTerrainPartition.h"

#if WITH_EDITOR
#include "MeshPartitionComponent.h"

void AProjectWorldMeshTerrainPartition::PostLoad()
{
	Super::PostLoad();
	RestoreEditorComponent();
}

void AProjectWorldMeshTerrainPartition::PostActorCreated()
{
	Super::PostActorCreated();
	RestoreEditorComponent();
}

void AProjectWorldMeshTerrainPartition::RestoreEditorComponent()
{
	if (GetMeshPartitionComponent() != nullptr)
	{
		return;
	}
	UClass* ComponentClass = LoadClass<UE::MeshPartition::UMeshPartitionComponent>(
		nullptr, TEXT("/Script/MeshPartitionEditor.MeshPartitionEditorComponent"));
	if (ComponentClass == nullptr)
	{
		return;
	}
	auto* Component = NewObject<UE::MeshPartition::UMeshPartitionComponent>(
		this, ComponentClass, TEXT("MeshPartitionEditorComponent"), RF_Transactional);
	SetMeshPartitionComponent(Component);
}
#endif
