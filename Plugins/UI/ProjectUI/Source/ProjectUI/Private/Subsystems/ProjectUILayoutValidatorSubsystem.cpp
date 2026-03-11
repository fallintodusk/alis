// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectUILayoutValidatorSubsystem.h"
#include "GameplayTagsManager.h"
#include "MVVM/ProjectViewModel.h"
#include "Blueprint/UserWidget.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectUIValidator, Log, All);

bool UProjectUILayoutValidatorSubsystem::ValidateLayoutFile(const FProjectUIDefinition& Definition, FString& OutError) const
{
	if (Definition.LayoutJsonPath.IsEmpty())
	{
		return true;
	}

	if (!FPaths::FileExists(Definition.LayoutJsonPath))
	{
		OutError = FString::Printf(TEXT("Layout JSON missing: %s"), *Definition.LayoutJsonPath);
		return false;
	}

	return true;
}

bool UProjectUILayoutValidatorSubsystem::ValidateDefinition(const FProjectUIDefinition& Definition, FString& OutError) const
{
	if (Definition.Id.IsNone())
	{
		OutError = TEXT("Definition Id is empty");
		return false;
	}

	if (!Definition.LayerTag.IsValid() && !Definition.SlotTag.IsValid())
	{
		OutError = FString::Printf(TEXT("Definition %s must define layer or slot"), *Definition.Id.ToString());
		return false;
	}

	if (!Definition.WidgetClassPath.IsValid())
	{
		OutError = FString::Printf(TEXT("Definition %s missing widget_class"), *Definition.Id.ToString());
		return false;
	}

	// Resolve widget class type
	if (UClass* WidgetClass = Definition.WidgetClassPath.TryLoadClass<UUserWidget>())
	{
		if (!WidgetClass->IsChildOf(UUserWidget::StaticClass()))
		{
			OutError = FString::Printf(TEXT("Definition %s widget_class is not a UUserWidget"), *Definition.Id.ToString());
			return false;
		}
	}
	else
	{
		OutError = FString::Printf(TEXT("Definition %s widget_class could not be loaded: %s"),
			*Definition.Id.ToString(), *Definition.WidgetClassPath.ToString());
		return false;
	}

	// Resolve optional ViewModel class type
	if (Definition.ViewModelClassPath.IsValid())
	{
		UClass* ViewModelClass = Definition.ViewModelClassPath.TryLoadClass<UProjectViewModel>();
		if (!ViewModelClass || !ViewModelClass->IsChildOf(UProjectViewModel::StaticClass()))
		{
			OutError = FString::Printf(TEXT("Definition %s viewmodel_class is invalid: %s"),
				*Definition.Id.ToString(), *Definition.ViewModelClassPath.ToString());
			return false;
		}
	}

	// Validate layout file existence
	if (!ValidateLayoutFile(Definition, OutError))
	{
		return false;
	}

	// Validate tags are registered
	if (Definition.LayerTag.IsValid())
	{
		FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(Definition.LayerTag.GetTagName(), false);
		if (!Tag.IsValid())
		{
			OutError = FString::Printf(TEXT("Definition %s layer tag not registered: %s"),
				*Definition.Id.ToString(), *Definition.LayerTag.ToString());
			return false;
		}
	}

	if (Definition.SlotTag.IsValid())
	{
		FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(Definition.SlotTag.GetTagName(), false);
		if (!Tag.IsValid())
		{
			OutError = FString::Printf(TEXT("Definition %s slot tag not registered: %s"),
				*Definition.Id.ToString(), *Definition.SlotTag.ToString());
			return false;
		}
	}

	return true;
}

bool UProjectUILayoutValidatorSubsystem::ValidateAndLog(const FProjectUIDefinition& Definition) const
{
	FString Error;
	if (!ValidateDefinition(Definition, Error))
	{
		UE_LOG(LogProjectUIValidator, Error, TEXT("UI definition invalid (%s): %s"),
			*Definition.Id.ToString(), *Error);
		return false;
	}

	return true;
}
