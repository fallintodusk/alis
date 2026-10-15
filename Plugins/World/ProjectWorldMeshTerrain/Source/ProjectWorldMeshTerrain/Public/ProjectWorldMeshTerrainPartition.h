#pragma once

#include "CoreMinimal.h"
#include "MeshPartition.h"

#include "ProjectWorldMeshTerrainPartition.generated.h"

UCLASS(NotPlaceable)
class PROJECTWORLDMESHTERRAIN_API AProjectWorldMeshTerrainPartition final
	: public UE::MeshPartition::AMeshPartition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostActorCreated() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
#endif

private:
#if WITH_EDITOR
	void RestoreEditorComponent();
	void SuppressGameAuthoring(AActor* Actor);
	FDelegateHandle AuthoringRegistrationHandle;
#endif
};
