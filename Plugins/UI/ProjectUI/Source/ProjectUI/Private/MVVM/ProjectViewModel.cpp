// Copyright ALIS. All Rights Reserved.

#include "MVVM/ProjectViewModel.h"

void UProjectViewModel::Initialize(UObject* Context)
{
	if (bInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("ViewModel %s already initialized"), *GetName());
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("Initializing ViewModel: %s"), *GetName());
	bInitialized = true;
}

void UProjectViewModel::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("Shutting down ViewModel: %s"), *GetName());

	// Clear all delegate bindings
	OnPropertyChanged.Clear();
	OnPropertyChangedNative.Clear();

	bInitialized = false;
}

bool UProjectViewModel::GetBoolProperty(FName PropertyName) const
{
	return false;
}

void UProjectViewModel::NotifyPropertyChanged(FName PropertyName)
{
	if (!bInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("NotifyPropertyChanged called on uninitialized ViewModel: %s"), *GetName());
		return;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("ViewModel %s: Property changed: %s"), *GetName(), *PropertyName.ToString());

	// Broadcast to all bound views
	OnPropertyChanged.Broadcast(PropertyName);
	OnPropertyChangedNative.Broadcast(PropertyName);
}
