// Copyright ALIS. All Rights Reserved.

#include "ProjectWidgetHelpers.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWidgetHelpers, Log, All);

UWidget* UProjectWidgetHelpers::FindWidgetByName(UWidget* Root, FName WidgetName, bool bRecursive, bool bPartialMatch)
{
	if (!Root)
	{
		UE_LOG(LogProjectWidgetHelpers, Warning, TEXT("FindWidgetByName: Root widget is null"));
		return nullptr;
	}

	if (WidgetName.IsNone())
	{
		UE_LOG(LogProjectWidgetHelpers, Warning, TEXT("FindWidgetByName: WidgetName is None"));
		return nullptr;
	}

	const FString SearchName = WidgetName.ToString();

	// Check root widget first
	if (DoesWidgetNameMatch(Root->GetName(), SearchName, bPartialMatch))
	{
		return Root;
	}

	// If recursive, search entire tree
	if (bRecursive)
	{
		return FindWidgetByNameRecursive(Root, WidgetName, bPartialMatch);
	}

	// Non-recursive: only search immediate children
	UPanelWidget* PanelWidget = Cast<UPanelWidget>(Root);
	if (PanelWidget)
	{
		for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
		{
			UWidget* Child = PanelWidget->GetChildAt(i);
			if (Child && DoesWidgetNameMatch(Child->GetName(), SearchName, bPartialMatch))
			{
				return Child;
			}
		}
	}

	return nullptr;
}

UWidget* UProjectWidgetHelpers::FindWidgetByNameRecursive(UWidget* Current, FName WidgetName, bool bPartialMatch)
{
	if (!Current)
	{
		return nullptr;
	}

	UPanelWidget* PanelWidget = Cast<UPanelWidget>(Current);
	if (!PanelWidget)
	{
		return nullptr;
	}

	const FString SearchName = WidgetName.ToString();

	// Search children
	for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
	{
		UWidget* Child = PanelWidget->GetChildAt(i);
		if (!Child)
		{
			continue;
		}

		// Check if this child matches
		if (DoesWidgetNameMatch(Child->GetName(), SearchName, bPartialMatch))
		{
			return Child;
		}

		// Recurse into child's children
		UWidget* Found = FindWidgetByNameRecursive(Child, WidgetName, bPartialMatch);
		if (Found)
		{
			return Found;
		}
	}

	return nullptr;
}

bool UProjectWidgetHelpers::DoesWidgetNameMatch(const FString& WidgetName, const FString& SearchName, bool bPartialMatch)
{
	if (bPartialMatch)
	{
		return WidgetName.Contains(SearchName);
	}
	else
	{
		return WidgetName.Equals(SearchName);
	}
}
