// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWidgetHelpers.h"

class UWidget;

/**
 * Small helper for typed widget lookup from a root layout widget.
 *
 * Required lookups are collected and can be logged once as an explicit
 * construct-time validation report.
 */
class PROJECTUI_API FProjectUIWidgetBinder
{
public:
	FProjectUIWidgetBinder() = default;

	explicit FProjectUIWidgetBinder(UWidget* InRoot, const FString& InOwnerContext = FString())
		: Root(InRoot)
		, OwnerContext(InOwnerContext)
	{
	}

	void SetRoot(UWidget* InRoot)
	{
		Root = InRoot;
	}

	void SetOwnerContext(const FString& InOwnerContext)
	{
		OwnerContext = InOwnerContext;
	}

	template<typename WidgetClass>
	WidgetClass* FindOptional(FName WidgetName, bool bRecursive = true, bool bPartialMatch = false) const
	{
		if (!Root)
		{
			return nullptr;
		}

		return UProjectWidgetHelpers::FindWidgetByNameTyped<WidgetClass>(Root, WidgetName, bRecursive, bPartialMatch);
	}

	template<typename WidgetClass>
	WidgetClass* FindRequired(FName WidgetName, bool bRecursive = true, bool bPartialMatch = false)
	{
		WidgetClass* FoundWidget = FindOptional<WidgetClass>(WidgetName, bRecursive, bPartialMatch);
		if (!FoundWidget)
		{
			RecordMissingRequired(WidgetName.ToString());
		}
		return FoundWidget;
	}

	template<typename WidgetClass>
	WidgetClass* FindOptionalAny(std::initializer_list<FName> WidgetNames, bool bRecursive = true, bool bPartialMatch = false) const
	{
		for (FName WidgetName : WidgetNames)
		{
			if (WidgetClass* FoundWidget = FindOptional<WidgetClass>(WidgetName, bRecursive, bPartialMatch))
			{
				return FoundWidget;
			}
		}
		return nullptr;
	}

	template<typename WidgetClass>
	WidgetClass* FindRequiredAny(std::initializer_list<FName> WidgetNames, bool bRecursive = true, bool bPartialMatch = false)
	{
		if (WidgetClass* FoundWidget = FindOptionalAny<WidgetClass>(WidgetNames, bRecursive, bPartialMatch))
		{
			return FoundWidget;
		}

		RecordMissingRequired(FormatNameList(WidgetNames));
		return nullptr;
	}

	bool HasMissingRequired() const
	{
		return MissingRequiredWidgets.Num() > 0;
	}

	const TArray<FString>& GetMissingRequired() const
	{
		return MissingRequiredWidgets;
	}

	void LogMissingRequired(const TCHAR* Context = nullptr) const;

private:
	void RecordMissingRequired(const FString& WidgetDescriptor);
	static FString FormatNameList(std::initializer_list<FName> WidgetNames);

private:
	UWidget* Root = nullptr;
	FString OwnerContext;
	TArray<FString> MissingRequiredWidgets;
};
