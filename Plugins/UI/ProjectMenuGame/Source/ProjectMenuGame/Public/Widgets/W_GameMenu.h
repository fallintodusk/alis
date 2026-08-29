// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Widgets/ProjectUserWidget.h"
#include "W_GameMenu.generated.h"

/** Minimal in-game pause menu hosted by ProjectUI. */
UCLASS()
class PROJECTMENUGAME_API UW_GameMenu : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_GameMenu(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BindCallbacks() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void ResumeGame();

	UFUNCTION()
	void ReturnToMainMenu();

	UFUNCTION()
	void QuitGame();
};
