// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ProjectViewModelConsumer.generated.h"

class UProjectViewModel;

/**
 * Interface for widgets that consume ViewModels injected by HUD hosts.
 *
 * In the Lyra-style UI extension system, the HUD layout creates widgets
 * dynamically and needs to pass ViewModels to them. Widgets that need
 * ViewModels should implement this interface.
 *
 * The HUD host (e.g., W_HUDLayout) creates the ViewModel instance and
 * passes it to the widget via SetViewModel(). This ensures consistent
 * ViewModel lifecycle management and avoids the footgun of some widgets
 * creating their own VMs while others don't.
 *
 * Usage:
 * @code
 * // Widget declaration
 * class UW_VitalsHUD : public UProjectUserWidget, public IProjectViewModelConsumer
 * {
 *     virtual void SetViewModel(UProjectViewModel* ViewModel) override
 *     {
 *         if (UVitalsViewModel* VitalsVM = Cast<UVitalsViewModel>(ViewModel))
 *         {
 *             CachedViewModel = VitalsVM;
 *             // Initialize ViewModel with context (e.g., ASC)
 *             // Bind to ViewModel properties
 *         }
 *     }
 * };
 * @endcode
 *
 * SOT: todo/current/gas_ui_mechanics.md (Phase 10)
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UProjectViewModelConsumer : public UInterface
{
	GENERATED_BODY()
};

class PROJECTUI_API IProjectViewModelConsumer
{
	GENERATED_BODY()

public:
	/**
	 * Called by HUD host to inject a ViewModel into the widget.
	 *
	 * The widget should:
	 * 1. Cast to the expected ViewModel type
	 * 2. Initialize the ViewModel with any required context (e.g., ASC)
	 * 3. Bind to ViewModel properties for data display
	 *
	 * @param ViewModel - The ViewModel instance created by the HUD host.
	 *                    The ViewModel's Outer is set to this widget for
	 *                    proper lifecycle management.
	 */
	virtual void SetViewModel(UProjectViewModel* ViewModel) = 0;
};
