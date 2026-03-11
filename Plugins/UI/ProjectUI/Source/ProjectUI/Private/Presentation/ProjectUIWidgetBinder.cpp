// Copyright ALIS. All Rights Reserved.

#include "Presentation/ProjectUIWidgetBinder.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectUIWidgetBinder, Log, All);

void FProjectUIWidgetBinder::LogMissingRequired(const TCHAR* Context) const
{
	if (!HasMissingRequired())
	{
		return;
	}

	const FString EffectiveContext = (Context != nullptr && FCString::Strlen(Context) > 0)
		? FString(Context)
		: OwnerContext;

	for (const FString& MissingWidget : MissingRequiredWidgets)
	{
		if (!EffectiveContext.IsEmpty())
		{
			UE_LOG(LogProjectUIWidgetBinder, Error, TEXT("[%s] Missing required widget: %s"), *EffectiveContext, *MissingWidget);
		}
		else
		{
			UE_LOG(LogProjectUIWidgetBinder, Error, TEXT("Missing required widget: %s"), *MissingWidget);
		}
	}
}

void FProjectUIWidgetBinder::RecordMissingRequired(const FString& WidgetDescriptor)
{
	if (WidgetDescriptor.IsEmpty())
	{
		return;
	}

	MissingRequiredWidgets.AddUnique(WidgetDescriptor);
}

FString FProjectUIWidgetBinder::FormatNameList(std::initializer_list<FName> WidgetNames)
{
	FString Result;
	bool bFirst = true;
	for (FName WidgetName : WidgetNames)
	{
		if (!bFirst)
		{
			Result += TEXT(" | ");
		}
		Result += WidgetName.ToString();
		bFirst = false;
	}
	return Result;
}

