// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dialogs/ProjectDialogTypes.h"
#include "ProjectDialogWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UButton;
class UProjectUIThemeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogButtonClicked, const FString&, Action);

/**
 * Universal dialog widget with overlay background and content box.
 *
 * Features:
 * - Semi-transparent overlay that greys out background
 * - Centered content box with title, message, and buttons
 * - Any number of buttons with configurable text, action, and variant
 * - Data-driven via JSON "dialog" configuration
 * - Theme-aware colors and fonts
 *
 * JSON Usage:
 * {
 *   "type": "Dialog",
 *   "name": "MyDialog",
 *   "visible": false,
 *   "dialog": {
 *     "title": "Dialog Title",
 *     "message": "Dialog message text",
 *     "overlayOpacity": 0.85,
 *     "buttons": [
 *       { "text": "OK", "action": "Confirm", "variant": "Primary" },
 *       { "text": "Cancel", "action": "Cancel", "variant": "Secondary" }
 *     ]
 *   }
 * }
 *
 * Button actions are mapped via the owning widget's BindCallbacks system.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Dialog"))
class PROJECTUI_API UProjectDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UProjectDialogWidget(const FObjectInitializer& ObjectInitializer);

	// UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	// ==========================================================================
	// Dialog Configuration
	// ==========================================================================

	/** Apply dialog parameters (called during JSON loading) */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Dialog")
	void ApplyParams(const FProjectDialogParams& InParams);

	/** Get current dialog parameters */
	UFUNCTION(BlueprintPure, Category = "Project UI|Dialog")
	const FProjectDialogParams& GetParams() const { return Params; }

	/** Set dialog title at runtime */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Dialog")
	void SetTitle(const FText& InTitle);

	/** Set dialog message at runtime */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Dialog")
	void SetMessage(const FText& InMessage);

	/** Set theme for styling */
	void SetTheme(UProjectUIThemeData* InTheme) { Theme = InTheme; }

	// ==========================================================================
	// Dialog Control
	// ==========================================================================

	/** Show the dialog */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Dialog")
	void ShowDialog();

	/** Hide the dialog */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Dialog")
	void HideDialog();

	/** Check if dialog is currently visible */
	UFUNCTION(BlueprintPure, Category = "Project UI|Dialog")
	bool IsDialogVisible() const;

	// ==========================================================================
	// Events
	// ==========================================================================

	/** Called when any button is clicked (passes action string) */
	UPROPERTY(BlueprintAssignable, Category = "Project UI|Dialog")
	FOnDialogButtonClicked OnButtonClicked;

	/** Get all button widgets (for external binding) */
	UFUNCTION(BlueprintPure, Category = "Project UI|Dialog")
	TArray<UButton*> GetButtons() const { return ButtonWidgets; }

protected:
	/** Build the dialog UI structure */
	void BuildDialogUI();

	/** Create a button widget from config */
	UButton* CreateButton(const FProjectDialogButton& ButtonConfig, int32 Index);

	/** Handle overlay click */
	UFUNCTION()
	void OnOverlayClicked();

	/** Resolve theme color by name */
	FLinearColor ResolveColor(const FString& ColorName) const;

	/** Resolve theme font by name */
	FSlateFontInfo ResolveFont(const FString& FontName) const;

	/** Apply button variant styling */
	void ApplyButtonVariant(UButton* Button, const FString& Variant);

protected:
	/** Dialog configuration */
	UPROPERTY()
	FProjectDialogParams Params;

	/** Theme for styling */
	UPROPERTY()
	TObjectPtr<UProjectUIThemeData> Theme;

	/** Root canvas panel */
	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	/** Overlay border (grey background) */
	UPROPERTY()
	TObjectPtr<UBorder> OverlayBorder;

	/** Content box border */
	UPROPERTY()
	TObjectPtr<UBorder> ContentBox;

	/** Title text block */
	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	/** Message text block */
	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;

	/** Button container */
	UPROPERTY()
	TObjectPtr<UHorizontalBox> ButtonContainer;

	/** Created button widgets */
	UPROPERTY()
	TArray<TObjectPtr<UButton>> ButtonWidgets;

	/** Button to action mapping for dynamic click handling */
	TMap<UButton*, FString> ButtonActions;

	/** Flag to track if UI has been built */
	bool bUIBuilt = false;
};
