// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/UILayerTypes.h"
#include "Data/ProjectUIDefinitions.h"
#include "ProjectUILayerHostSubsystem.generated.h"

class APlayerController;
class UProjectViewModel;
class UUserWidget;
class UW_LayerStack;

/**
 * Hosts HUD layout and UI layers, applies effective input from the topmost layer.
 */
UCLASS()
class PROJECTUI_API UProjectUILayerHostSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Ensure HUD and layer stack for a player controller.
	 * @param bInitializeHUD If false, skips HUD layout creation (use for main menu).
	 */
	void InitializeForPlayer(APlayerController* PlayerController, bool bInitializeHUD = true);

	/** Show a definition by Id (creates if needed). */
	UUserWidget* ShowDefinition(FName DefinitionId);

	/** Hide a definition by Id (collapses or destroys based on spawn policy). */
	void HideDefinition(FName DefinitionId);

	/** Access the active HUD layout widget. */
	UUserWidget* GetHUDLayout() const { return HUDLayout.Get(); }

	/** Access shared ViewModels from factory. */
	UProjectViewModel* GetSharedViewModel(TSubclassOf<UProjectViewModel> ViewModelClass) const;

private:
	struct FActiveWidgetEntry
	{
		TWeakObjectPtr<UUserWidget> Widget;
		FGameplayTag LayerTag;
		EProjectWidgetInputMode RequestedInput = EProjectWidgetInputMode::Default;
		int32 Priority = 0;
		EProjectUISpawnPolicy SpawnPolicy = EProjectUISpawnPolicy::Persistent;
	};

	void InitializeDefaultLayers();
	void EnsureHUDLayout(APlayerController* PlayerController);
	void EnsureLayerStack(APlayerController* PlayerController);
	void PreloadDefinitions(APlayerController* PlayerController);
	void SetupAutoVisibilityBindings();
	void TeardownAutoVisibilityBindings();
	void ApplyInputForActiveLayers();
	EProjectWidgetInputMode GetRequestedInputForLayer(FGameplayTag LayerTag) const;
	void RefreshHUDSlot(FGameplayTag SlotTag) const;

	void ApplyViewportSizing(UUserWidget* Widget, const FProjectUIDefinition& Definition) const;

private:
	TWeakObjectPtr<APlayerController> PrimaryPlayerController;
	TWeakObjectPtr<UUserWidget> HUDLayout;
	TWeakObjectPtr<UW_LayerStack> LayerStack;

	TMap<FName, FActiveWidgetEntry> ActiveWidgets;
	TSet<FGameplayTag> ActiveLayers;
	TMap<FGameplayTag, FUILayerConfig> LayerConfigs;

	struct FAutoVisibilityBinding
	{
		TWeakObjectPtr<UProjectViewModel> ViewModel;
		FName DefinitionId;
		FName PropertyName;
		FDelegateHandle Handle;
	};

	TArray<FAutoVisibilityBinding> AutoVisibilityBindings;
	bool bPreloadComplete = false;
};
