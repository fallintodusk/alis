// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "ViewModels/ProjectMenuViewModel.h"
#include "Overlay/ProjectUIPopupPresenter.h"
#include "W_MainMenu.generated.h"

class UCanvasPanel;
class UUserWidget;

/**
 * Main menu widget - loads layout from Config/UI/MainMenu.json
 *
 * Uses base class JSON layout loading (no duplication needed):
 * - Set ConfigFilePath in constructor
 * - Override BindCallbacks() to wire button handlers
 * - Hot reload automatic in editor
 *
 * Layout buttons: Play, Settings, Credits, Quit
 */
UCLASS()
class PROJECTMENUMAIN_API UW_MainMenu : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_MainMenu(const FObjectInitializer& ObjectInitializer);

protected:
	// UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// UProjectUserWidget interface - bind button handlers
	virtual void BindCallbacks() override;

	// Navigation callbacks (bound to buttons in JSON)
	UFUNCTION()
	void NavigateToMapBrowser();

	UFUNCTION()
	void NavigateToSettings();

	UFUNCTION()
	void NavigateToCredits();

	UFUNCTION()
	void BackToMain();

	UFUNCTION()
	void SelectMapCity17();

	UFUNCTION()
	void RequestQuit();

	UFUNCTION()
	void ConfirmQuit();

	UFUNCTION()
	void CancelQuit();

	/** Handle dialog button clicks - routes action strings to appropriate handlers */
	UFUNCTION()
	void OnDialogButtonClicked(const FString& Action);

	// ViewModel event handlers
	UFUNCTION()
	void OnCurrentScreenChanged();

	UFUNCTION()
	void OnQuitConfirmationChanged();

	/** Update left rail button highlighting based on current screen */
	void UpdateMenuButtonHighlight(EMenuScreen CurrentScreen);

protected:
	/** ViewModel for menu state and navigation */
	UPROPERTY(Transient)
	TObjectPtr<UProjectMenuViewModel> MenuViewModel;

private:
	/** Ensure ProjectSettingsUI widget is created and hosted via shared ProjectUI presenter. */
	void EnsureSettingsPopupInitialized();

	/** Show/hide settings popup content for Settings screen. */
	void SetSettingsPopupVisible(bool bVisible);

	/** Lazily created settings widget hosted from ProjectSettingsUI */
	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> SettingsContent;

	/** Host canvas resolved from SettingsPanel in JSON layout. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UCanvasPanel> SettingsHostCanvas;

	/** Shared popup presenter for non-inventory settings content hosting (Phase 6 reuse). */
	FProjectUIPopupPresenter SettingsPopupPresenter;

	/** One-time diagnostic flag for post-layout size check */
	bool bPendingDiagnosticTick = true;
};
