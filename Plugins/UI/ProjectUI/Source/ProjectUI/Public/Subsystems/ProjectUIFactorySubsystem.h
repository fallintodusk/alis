// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ProjectUIDefinitions.h"
#include "ProjectUIFactorySubsystem.generated.h"

class UProjectViewModel;
class UUserWidget;
class UPanelWidget;

/**
 * Result of a widget creation request.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectUIWidgetCreateResult
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UUserWidget> Widget = nullptr;

	UPROPERTY()
	TObjectPtr<UProjectViewModel> ViewModel = nullptr;
};

/**
 * Factory responsible for creating widgets and injecting ViewModels.
 * All CreateWidget calls live here.
 */
UCLASS()
class PROJECTUI_API UProjectUIFactorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Create a widget by class (no definition). */
	UUserWidget* CreateWidgetByClass(UObject* WorldContext, TSubclassOf<UUserWidget> WidgetClass);

	/** Create a widget from a definition and inject a ViewModel if configured. */
	FProjectUIWidgetCreateResult CreateWidgetForDefinition(const FProjectUIDefinition& Definition, UObject* WorldContext);

	/** Attach a widget to a slot container with sizing policy. */
	bool AttachWidgetToSlot(UPanelWidget* Container, UUserWidget* Widget, const FProjectUIDefinition& Definition);

	/** Access shared ViewModels (Global policy). */
	UProjectViewModel* GetSharedViewModel(TSubclassOf<UProjectViewModel> ViewModelClass) const;

	/** Create a Global ViewModel eagerly (without widget). Used by auto_visibility. */
	UProjectViewModel* EnsureGlobalViewModel(const FProjectUIDefinition& Definition);

private:
	UProjectViewModel* CreateOrResolveViewModel(const FProjectUIDefinition& Definition, UObject* Context, UUserWidget* WidgetOuter, bool& bOutShouldInitialize);
	UProjectViewModel* ResolveViewModelPropertyPath(UObject* Context, const FString& PropertyPath) const;
	void InjectViewModel(UUserWidget* Widget, UProjectViewModel* ViewModel, UObject* Context, bool bShouldInitialize) const;

private:
	UPROPERTY()
	TMap<UClass*, TObjectPtr<UProjectViewModel>> SharedViewModels;
};
