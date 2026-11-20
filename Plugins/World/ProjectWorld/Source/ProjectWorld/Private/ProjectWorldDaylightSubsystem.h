#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProjectWorldDaylightSubsystem.generated.h"

class UProjectLightComponent;

/** Concrete runtime composition for the generated clock and light components. */
UCLASS()
class UProjectWorldDaylightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;
	virtual void OnWorldEndPlay(UWorld& World) override;
private:
	TWeakObjectPtr<UProjectLightComponent> Light;
};
