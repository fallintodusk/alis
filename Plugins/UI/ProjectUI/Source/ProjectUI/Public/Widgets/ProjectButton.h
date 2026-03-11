// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "ProjectButton.generated.h"

/**
 * Button style variants
 */
UENUM(BlueprintType)
enum class EProjectButtonStyle : uint8
{
	/** Primary action button (filled, primary color) */
	Primary UMETA(DisplayName = "Primary"),

	/** Secondary action button (outlined, secondary color) */
	Secondary UMETA(DisplayName = "Secondary"),

	/** Success button (filled, success color) */
	Success UMETA(DisplayName = "Success"),

	/** Warning button (filled, warning color) */
	Warning UMETA(DisplayName = "Warning"),

	/** Error/danger button (filled, error color) */
	Error UMETA(DisplayName = "Error"),

	/** Text-only button (no background) */
	Text UMETA(DisplayName = "Text")
};

/**
 * Button size variants
 */
UENUM(BlueprintType)
enum class EProjectButtonSize : uint8
{
	Small UMETA(DisplayName = "Small"),
	Medium UMETA(DisplayName = "Medium"),
	Large UMETA(DisplayName = "Large")
};

/**
 * Reusable themed button widget
 *
 * Features:
 * - Multiple style variants (Primary, Secondary, Success, Warning, Error, Text)
 * - Multiple size variants (Small, Medium, Large)
 * - Automatic theme integration
 * - Hover/pressed/disabled states
 * - Optional icon support
 *
 * Usage in C++:
 * @code
 * UProjectButton* Button = CreateWidget<UProjectButton>(this);
 * Button->SetButtonStyle(EProjectButtonStyle::Primary);
 * Button->SetButtonSize(EProjectButtonSize::Large);
 * Button->SetButtonText(FText::FromString("Click Me"));
 * Button->OnButtonClicked.AddDynamic(this, &UMyClass::HandleButtonClicked);
 * @endcode
 *
 * Usage in UMG:
 * 1. Create widget blueprint derived from UProjectButton
 * 2. Add Button component bound to "ButtonWidget"
 * 3. Add TextBlock component bound to "TextWidget"
 * 4. Configure style and size in blueprint details
 */
UCLASS()
class PROJECTUI_API UProjectButton : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UProjectButton(const FObjectInitializer& ObjectInitializer);

	/** UUserWidget lifecycle */
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	/**
	 * Set button style variant
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Button")
	void SetButtonStyle(EProjectButtonStyle NewStyle);

	/**
	 * Get current button style
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Button")
	EProjectButtonStyle GetButtonStyle() const { return ButtonStyle; }

	/**
	 * Set button size variant
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Button")
	void SetButtonSize(EProjectButtonSize NewSize);

	/**
	 * Get current button size
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Button")
	EProjectButtonSize GetButtonSize() const { return ButtonSize; }

	/**
	 * Set button text
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Button")
	void SetButtonText(const FText& NewText);

	/**
	 * Get button text
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Button")
	FText GetButtonText() const;

	/**
	 * Set button enabled state
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Button")
	void SetButtonEnabled(bool bEnabled);

	/**
	 * Check if button is enabled
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Button")
	bool IsButtonEnabled() const;

	/** Delegate fired when button is clicked */
	UPROPERTY(BlueprintAssignable, Category = "Project UI|Button")
	FOnButtonClickedEvent OnButtonClicked;

protected:
	/** Override to apply theme colors based on button style */
	virtual void OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme) override;

	/**
	 * Apply styling to button based on current style, size, and theme
	 */
	void ApplyStyling();

	/**
	 * Get color for current button style from theme
	 */
	FLinearColor GetStyleColor(UProjectUIThemeData* Theme) const;

	/** Handle button click from underlying UButton */
	UFUNCTION()
	void HandleButtonClicked();

protected:
	/** Button style variant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project UI|Button")
	EProjectButtonStyle ButtonStyle = EProjectButtonStyle::Primary;

	/** Button size variant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project UI|Button")
	EProjectButtonSize ButtonSize = EProjectButtonSize::Medium;

	/** UMG Button component (bind in blueprint) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ButtonWidget;

	/** UMG TextBlock component (bind in blueprint) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TextWidget;
};
