// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectWidgetHelpers.generated.h"

class UWidget;
class UPanelWidget;

/**
 * Widget Utility Helpers
 *
 * Static utility functions for widget tree manipulation and queries.
 * Used by W_ base widgets to find and manipulate child widgets loaded from JSON.
 */
UCLASS()
class PROJECTUI_API UProjectWidgetHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Find a widget by name in the widget tree
	 *
	 * @param Root - Root widget to search from
	 * @param WidgetName - Name to search for (case-sensitive)
	 * @param bRecursive - If true, search recursively through entire tree. If false, only search immediate children.
	 * @param bPartialMatch - If true, match widgets whose name contains WidgetName. If false, require exact match.
	 * @return First widget found with matching name, or nullptr if not found
	 *
	 * Usage:
	 * @code
	 * UProgressBar* ProgressBar = Cast<UProgressBar>(
	 *     UProjectWidgetHelpers::FindWidgetByName(RootWidget, TEXT("ProgressBar_Loading"), true, false)
	 * );
	 * if (ProgressBar)
	 * {
	 *     ProgressBar->SetPercent(0.5f);
	 * }
	 * @endcode
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Widget Utilities")
	static UWidget* FindWidgetByName(UWidget* Root, FName WidgetName, bool bRecursive = true, bool bPartialMatch = false);

	/**
	 * Find a widget by name and cast to specific type
	 *
	 * Template version for convenience - automatically casts result.
	 *
	 * @param Root - Root widget to search from
	 * @param WidgetName - Name to search for
	 * @param bRecursive - If true, search recursively
	 * @param bPartialMatch - If true, allow partial name matches
	 * @return Widget cast to WidgetClass, or nullptr if not found or wrong type
	 *
	 * Usage:
	 * @code
	 * UProgressBar* ProgressBar = UProjectWidgetHelpers::FindWidgetByNameTyped<UProgressBar>(
	 *     RootWidget, TEXT("ProgressBar_Loading")
	 * );
	 * @endcode
	 */
	template<typename WidgetClass>
	static WidgetClass* FindWidgetByNameTyped(UWidget* Root, FName WidgetName, bool bRecursive = true, bool bPartialMatch = false)
	{
		return Cast<WidgetClass>(FindWidgetByName(Root, WidgetName, bRecursive, bPartialMatch));
	}

private:
	/**
	 * Recursive helper for FindWidgetByName
	 */
	static UWidget* FindWidgetByNameRecursive(UWidget* Current, FName WidgetName, bool bPartialMatch);

	/**
	 * Check if widget name matches search criteria
	 */
	static bool DoesWidgetNameMatch(const FString& WidgetName, const FString& SearchName, bool bPartialMatch);
};
