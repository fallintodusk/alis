#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/ProjectTimeView.h"
#include "ProjectTimeClock.h"
#include "ProjectTimeSubsystem.generated.h"

class UProjectTimeWorldContextComponent;

UCLASS()
class PROJECTTIME_API UProjectTimeSubsystem : public UWorldSubsystem, public IProjectTimeView
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	virtual void OnWorldEndPlay(UWorld& World) override;
	virtual UWorld* GetTimeWorld() const override { return GetWorld(); }
	virtual bool TryReadTime(FProjectTimeSnapshot& Out) override;
	virtual FSimpleMulticastDelegate& TimeChanged() override { return Changed; }

	bool BindContext(UProjectTimeWorldContextComponent& InContext, FString& OutError);
	void UnbindContext();
	bool CanControlTime() const;
	bool SeekUtc(FDateTime Utc);
	bool SetRate(double Rate);
	bool ResetToPcTime();
	FText GetLocalTimeText();

private:
	void RefreshPermission();
	void ObserveDemoPermission();
	FTimerHandle PermissionTimer;
	TWeakObjectPtr<UProjectTimeWorldContextComponent> Context;
	FString BoundIntegrity;
	FProjectTimeClock Clock;
	FSimpleMulticastDelegate Changed;
};
