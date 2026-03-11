// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectVitalsUIBootstrap.generated.h"

/**
 * Bootstrap subsystem for ProjectVitalsUI.
 *
 * Bootstraps VitalsUI data that is not covered by ui_definitions.json.
 * Currently this registers vitals state tag texts with ProjectTagTextSubsystem.
 *
 * Key points:
 * - Uses InitializeDependency to ensure ProjectTagTextSubsystem is ready
 * - No widget registration here (ProjectUI loads ui_definitions.json)
 *
 * SOT: todo/current/gas_ui_mechanics.md (Phase 10)
 */
UCLASS()
class PROJECTVITALSUI_API UProjectVitalsUIBootstrap : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	/** Register vitals state tag texts with ProjectTagTextSubsystem */
	void RegisterTagTexts();
};
