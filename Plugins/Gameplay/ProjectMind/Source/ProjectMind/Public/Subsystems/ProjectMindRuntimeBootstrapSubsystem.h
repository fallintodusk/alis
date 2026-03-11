// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectMindRuntimeBootstrapSubsystem.generated.h"

class APawn;
class AController;

/**
 * Owns local pawn wiring for ProjectMind runtime.
 *
 * Keeps ProjectSinglePlayClient free from direct runtime dependency on ProjectMind.
 * Listens to pawn-controller changes and applies local pawn context to IMindRuntimeControl.
 */
UCLASS()
class PROJECTMIND_API UProjectMindRuntimeBootstrapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UFUNCTION()
	void HandlePawnControllerChanged(APawn* Pawn, AController* Controller);
	void ApplyResolvedPawn(APawn* ResolvedPawn);
	APawn* ResolvePrimaryLocalPawn() const;

private:
	TWeakObjectPtr<APawn> LastAppliedLocalPawn;
};
