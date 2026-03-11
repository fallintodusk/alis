// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_MindJournalPanel.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UScrollBox;
class UMindJournalViewModel;

/**
 * Menu-layer panel that presents thought history in two tabs.
 */
UCLASS()
class PROJECTMINDUI_API UW_MindJournalPanel : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_MindJournalPanel(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void BindCallbacks() override;
	virtual void RefreshFromViewModel_Implementation() override;

private:
	UFUNCTION()
	void HandleBackdropClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleImportantTabClicked();

	UFUNCTION()
	void HandleRecentTabClicked();

private:
	UPROPERTY()
	TObjectPtr<UButton> BackdropButton;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	UPROPERTY()
	TObjectPtr<UButton> ImportantTabButton;

	UPROPERTY()
	TObjectPtr<UButton> RecentTabButton;

	UPROPERTY()
	TObjectPtr<UBorder> ImportantContent;

	UPROPERTY()
	TObjectPtr<UBorder> RecentContent;

	UPROPERTY()
	TObjectPtr<UTextBlock> ImportantHistoryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RecentHistoryText;

	UPROPERTY()
	TObjectPtr<UScrollBox> ImportantScroll;

	UPROPERTY()
	TObjectPtr<UScrollBox> RecentScroll;
};
