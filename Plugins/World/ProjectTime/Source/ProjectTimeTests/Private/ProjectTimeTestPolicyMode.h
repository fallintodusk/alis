#pragma once
#include "GameFramework/GameModeBase.h"
#include "Interfaces/ProjectTimeView.h"
#include "ProjectTimeTestPolicyMode.generated.h"

UCLASS()
class AProjectTimeTestPolicyMode : public AGameModeBase, public IProjectTimeControlPolicy
{
	GENERATED_BODY()
public:
	bool bAllowControls = false;
	virtual bool AllowsEnvironmentalTimeControl() const override { return bAllowControls; }
};
