// Copyright ALIS. All Rights Reserved.

#pragma once

#include "MVVM/ProjectViewModel.h"
#include "TestViewModel.generated.h"

/**
 * Lightweight test double for MVVM property validation.
 * Provides explicit setters so unit tests can drive change notifications.
 */
UCLASS()
class UTestViewModel : public UProjectViewModel
{
	GENERATED_BODY()

public:
	VIEWMODEL_PROPERTY(FText, Title);
	VIEWMODEL_PROPERTY(int32, ItemCount);
	VIEWMODEL_PROPERTY(bool, bIsActive);

	void SetTitle(const FText& InTitle)
	{
		if (Title.EqualTo(InTitle))
		{
			return;
		}

		Title = InTitle;
		NotifyPropertyChanged(FName(TEXT("Title")));
	}

	void SetItemCount(int32 InCount)
	{
		if (ItemCount == InCount)
		{
			return;
		}

		ItemCount = InCount;
		NotifyPropertyChanged(FName(TEXT("ItemCount")));
	}

	void SetbIsActive(bool bInIsActive)
	{
		if (bIsActive == bInIsActive)
		{
			return;
		}

		bIsActive = bInIsActive;
		NotifyPropertyChanged(FName(TEXT("bIsActive")));
	}
};
