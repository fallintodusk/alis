// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectMindComponent.generated.h"

/**
 * Local player context bridge for ProjectMind runtime.
 *
 * Attached to a locally controlled pawn by single-player controller bootstrap.
 * Registers/clears local pawn context in IMindRuntimeControl.
 */
UCLASS(ClassGroup = (ProjectMind), meta = (BlueprintSpawnableComponent))
class PROJECTMIND_API UProjectMindComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProjectMindComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
