#pragma once

#include "Interfaces/ProjectTimeView.h"
#include "ProjectLightTestTimeView.generated.h"

UCLASS()
class UProjectLightTestTimeView : public UObject, public IProjectTimeView
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UWorld> World;
	FProjectTimeSnapshot Snapshot;
	FSimpleMulticastDelegate Changed;
	virtual UWorld* GetTimeWorld() const override { return World; }
	virtual bool TryReadTime(FProjectTimeSnapshot& Out) override { Out = Snapshot; return true; }
	virtual FSimpleMulticastDelegate& TimeChanged() override { return Changed; }
};
