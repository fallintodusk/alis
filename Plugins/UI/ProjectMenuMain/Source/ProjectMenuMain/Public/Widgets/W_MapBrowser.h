// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_MapBrowser.generated.h"

class UProjectMapListViewModel;

/**
 * Map browser widget - loads layout from Config/UI/MapBrowser.json
 *
 * Uses base class JSON layout loading (no duplication needed):
 * - Set ConfigFilePath in constructor
 * - Override BindCallbacks() to wire button handlers
 * - Hot reload automatic in editor
 *
 * Layout: Search box, map list, selected map details, Load/Back buttons
 */
UCLASS()
class PROJECTMENUMAIN_API UW_MapBrowser : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_MapBrowser(const FObjectInitializer& ObjectInitializer);

protected:
	// UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Focus handling (for TextInputFramework.dll crash debugging)
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	// UProjectUserWidget interface - bind button handlers
	virtual void BindCallbacks() override;

	/**
	 * Update the map list UI from ViewModel data.
	 */
	void UpdateMapList();

	/**
	 * Update the selected map details panel.
	 */
	void UpdateSelectedMapDetails();

	// Search callbacks
	UFUNCTION()
	void OnSearchTextChanged(const FText& NewText);

	// Focus callbacks (for TextInputFramework.dll crash debugging)
	UFUNCTION()
	void OnSearchBoxFocusReceived();

	UFUNCTION()
	void OnSearchBoxFocusLost();

	// Map selection callbacks
	UFUNCTION()
	void OnMapSelected(int32 MapIndex);

	// Navigation callbacks
	UFUNCTION()
	void OnLoadClicked();

	UFUNCTION()
	void OnBackClicked();

	// ViewModel event handlers
	UFUNCTION()
	void OnMapListChanged();

	UFUNCTION()
	void OnSelectedMapChanged();

protected:
	/** ViewModel for map list and selection state */
	UPROPERTY(Transient)
	TObjectPtr<UProjectMapListViewModel> MapListViewModel;

	/** Current search filter text */
	FString CurrentSearchFilter;

	/** Cached reference to search box for focus management */
	UPROPERTY(Transient)
	TObjectPtr<class UEditableTextBox> CachedSearchBox;
};
