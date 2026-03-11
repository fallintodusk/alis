// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectDialogTypes.generated.h"

/**
 * Configuration for a single dialog button.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectDialogButton
{
	GENERATED_BODY()

	/** Button display text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText Text;

	/** Action identifier (used by BindCallbacks to map to handler) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString Action;

	/** Button style variant (Primary, Secondary, Error, Warning, Success, Text) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString Variant = TEXT("Secondary");

	/** Optional icon (FontAwesome unicode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString Icon;

	FProjectDialogButton()
		: Text(FText::GetEmpty())
		, Action(TEXT(""))
		, Variant(TEXT("Secondary"))
		, Icon(TEXT(""))
	{
	}

	FProjectDialogButton(const FText& InText, const FString& InAction, const FString& InVariant = TEXT("Secondary"))
		: Text(InText)
		, Action(InAction)
		, Variant(InVariant)
		, Icon(TEXT(""))
	{
	}
};

/**
 * Configuration for a dialog widget.
 * Parsed from JSON "dialog" object.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectDialogParams
{
	GENERATED_BODY()

	/** Dialog title text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText Title;

	/** Dialog message/body text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText Message;

	/** Buttons to display (in order) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	TArray<FProjectDialogButton> Buttons;

	/** Overlay background opacity (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverlayOpacity = 0.85f;

	/** Overlay color (theme reference or explicit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString OverlayColor = TEXT("Background");

	/** Dialog box width in pixels (0 = auto) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	float Width = 400.0f;

	/** Dialog box background color (theme reference or explicit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString BoxColor = TEXT("Surface");

	/** Border color around dialog box (theme reference or explicit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString BorderColor = TEXT("Border");

	/** Border width in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	float BorderWidth = 2.0f;

	/** Padding inside dialog box */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	float Padding = 24.0f;

	/** Title font style (theme reference) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString TitleFont = TEXT("HeadingSmall");

	/** Message font style (theme reference) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString MessageFont = TEXT("BodyLarge");

	/** Close dialog when clicking overlay (outside dialog box) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	bool bCloseOnOverlayClick = false;

	/** Action to trigger when overlay is clicked (if bCloseOnOverlayClick) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FString OverlayClickAction;

	FProjectDialogParams()
		: Title(FText::GetEmpty())
		, Message(FText::GetEmpty())
		, OverlayOpacity(0.85f)
		, OverlayColor(TEXT("Background"))
		, Width(400.0f)
		, BoxColor(TEXT("Surface"))
		, BorderColor(TEXT("Border"))
		, BorderWidth(2.0f)
		, Padding(24.0f)
		, TitleFont(TEXT("HeadingSmall"))
		, MessageFont(TEXT("BodyLarge"))
		, bCloseOnOverlayClick(false)
		, OverlayClickAction(TEXT(""))
	{
	}
};
