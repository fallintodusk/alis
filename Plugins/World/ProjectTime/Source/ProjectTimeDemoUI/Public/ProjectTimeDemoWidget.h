#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "ProjectTimeDemoWidget.generated.h"

class UTextBlock;
class USlider;

UCLASS()
class PROJECTTIMEDEMOUI_API UProjectTimeDemoHintWidget : public UProjectUserWidget
{
	GENERATED_BODY()
};

UCLASS()
class PROJECTTIMEDEMOUI_API UProjectTimeDemoWidget : public UProjectUserWidget
{
	GENERATED_BODY()
public:
	UProjectTimeDemoWidget(const FObjectInitializer& Initializer);
	virtual void RefreshFromViewModel_Implementation() override;
protected:
	virtual void BindCallbacks() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	UFUNCTION() void OnOffset(float Value);
	UFUNCTION() void OnRate(float Value);
	UFUNCTION() void OnScrubBegin();
	UFUNCTION() void OnReset();
	UFUNCTION() void OnClose();
	UPROPERTY() TObjectPtr<UTextBlock> TimeLabel;
	UPROPERTY() TObjectPtr<USlider> OffsetSlider;
	UPROPERTY() TObjectPtr<USlider> RateSlider;
	bool bRefreshing = false;
};
