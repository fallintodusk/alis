#pragma once

#include "CoreMinimal.h"
#include "MVVM/ProjectViewModel.h"
#include "ProjectTimeDemoViewModel.generated.h"

class UProjectTimeSubsystem;

UCLASS()
class PROJECTTIMEDEMOUI_API UProjectTimeDemoViewModel : public UProjectViewModel
{
	GENERATED_BODY()
public:
	virtual void Initialize(UObject* Context) override;
	virtual void Shutdown() override;
	void Refresh();
	void BeginScrub();
	void SeekHours(float Hours);
	void ChangeRate(float Rate);
	void Reset();
	void Close() { CloseRequested.Broadcast(); }
	const FText& GetDateText() const { return DateText; }
	float GetRate() const { return RateValue; }
	float GetOffset() const { return OffsetHours; }
	bool CanControl() const;
	UWorld* GetBoundWorld() const;
	FSimpleMulticastDelegate CloseRequested;

private:
	TWeakObjectPtr<UProjectTimeSubsystem> Time;
	FDateTime ScrubAnchor;
	FText DateText;
	float RateValue = 1, OffsetHours = 0;
	bool bScrubAnchor = false;
};
