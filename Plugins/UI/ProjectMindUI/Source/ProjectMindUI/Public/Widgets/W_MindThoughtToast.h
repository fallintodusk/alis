// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_MindThoughtToast.generated.h"

class UTextBlock;

/**
 * Temporary top-right thought toast driven by UMindThoughtViewModel.
 */
UCLASS()
class PROJECTMINDUI_API UW_MindThoughtToast : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_MindThoughtToast(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshFromViewModel_Implementation() override;

private:
	void HandleAutoHideExpired();

private:
	TWeakObjectPtr<UTextBlock> ThoughtText;
	TWeakObjectPtr<UTextBlock> ThoughtMetaText;
	FTimerHandle AutoHideTimerHandle;
	FSlateFontInfo DefaultThoughtFont;
	bool bDefaultFontCached = false;
};
