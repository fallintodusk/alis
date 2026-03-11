// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_HUDLayout.h"
#include "Widgets/W_InteractionPrompt.h"
#include "MVVM/InteractionPromptViewModel.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "ProjectGameplayTags.h"
#include "ProjectWidgetHelpers.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Subsystems/ProjectUIRegistrySubsystem.h"
#include "Subsystems/ProjectUIFactorySubsystem.h"
#include "Subsystems/ProjectToastSubsystem.h"
#include "MVVM/ProjectViewModel.h"
#include "Widgets/ProjectUserWidget.h"
#include "HAL/PlatformTLS.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHUDLayout, Log, All);

// Thread-safe logging macro for init diagnostics
#define LOG_HUD_INIT(Format, ...) UE_LOG(LogHUDLayout, Log, TEXT("[Thread:%u] " Format), FPlatformTLS::GetCurrentThreadId(), ##__VA_ARGS__)

UW_HUDLayout::UW_HUDLayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set path to JSON layout config - required for base class to load layout
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectHUD"), TEXT("HUDLayout.json"));
}

UProjectViewModel* UW_HUDLayout::GetViewModelByClass(TSubclassOf<UProjectViewModel> ViewModelClass) const
{
	if (!ViewModelClass)
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<UProjectViewModel>* Found = ViewModelsByClass.Find(ViewModelClass.Get()))
	{
		return Found->Get();
	}

	return nullptr;
}

void UW_HUDLayout::RefreshSlot(FGameplayTag SlotTag)
{
	if (TWeakObjectPtr<UPanelWidget>* Container = SlotContainers.Find(SlotTag))
	{
		if (Container->IsValid())
		{
			RebuildSlot(SlotTag, Container->Get());
		}
	}
}

void UW_HUDLayout::NativeConstruct()
{
	LOG_HUD_INIT("NativeConstruct START - Widget=%s", *GetName());
	Super::NativeConstruct();

	// CRITICAL CHECK: Is this HUD in viewport?
	LOG_HUD_INIT("NativeConstruct: IsInViewport=%s (must be YES for layout to work)",
		IsInViewport() ? TEXT("YES") : TEXT("NO - HUD NOT ADDED TO VIEWPORT!"));

	// Log THIS widget's (W_HUDLayout's) own geometry
	LOG_HUD_INIT("NativeConstruct: === W_HUDLayout SELF-DIAGNOSTICS ===");
	FVector2D OwnDesired = GetDesiredSize();
	FVector2D OwnCached = GetCachedGeometry().GetLocalSize();
	LOG_HUD_INIT("NativeConstruct: W_HUDLayout DesiredSize=(%.0f,%.0f) CachedSize=(%.0f,%.0f)",
		OwnDesired.X, OwnDesired.Y, OwnCached.X, OwnCached.Y);

	// Check our slot (if we're a child of something)
	if (Slot)
	{
		LOG_HUD_INIT("NativeConstruct: W_HUDLayout has Slot type: %s", *Slot->GetClass()->GetName());
	}
	else
	{
		LOG_HUD_INIT("NativeConstruct: W_HUDLayout has NO Slot (is viewport root - EXPECTED)");
	}

	// Schedule deferred self-check after Slate has time to complete layout (0.1s delay)
	TWeakObjectPtr<UW_HUDLayout> WeakThis = this;
	if (UWorld* World = GetWorld())
	{
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle, [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				FVector2D DeferredCached = WeakThis->GetCachedGeometry().GetLocalSize();
				bool bInViewport = WeakThis->IsInViewport();

				// Check if Slate geometry is valid (not just timing issue)
				TSharedPtr<SWidget> SlateWidget = WeakThis->GetCachedWidget();
				bool bSlateValid = SlateWidget.IsValid();
				FVector2D SlateSize = bSlateValid ? SlateWidget->GetTickSpaceGeometry().GetLocalSize() : FVector2D::ZeroVector;

				// Only log errors if Slate has geometry but UMG doesn't
				if (!bInViewport)
				{
					UE_LOG(LogHUDLayout, Error, TEXT("W_HUDLayout: NOT in viewport!"));
				}
				else if (DeferredCached.IsNearlyZero() && SlateSize.IsNearlyZero())
				{
					UE_LOG(LogHUDLayout, Warning, TEXT("W_HUDLayout: Size pending (Slate=%.0fx%.0f)"), SlateSize.X, SlateSize.Y);
				}
				else if (!DeferredCached.IsNearlyZero())
				{
					UE_LOG(LogHUDLayout, Log, TEXT("W_HUDLayout: OK (%.0fx%.0f)"), DeferredCached.X, DeferredCached.Y);
				}
			}
		}, 0.1f, false);
	}
	LOG_HUD_INIT("NativeConstruct: === END W_HUDLayout SELF-DIAGNOSTICS ===");

	// Find slot containers by name (JSON-built, no BindWidget)
	// IMPORTANT: Search from RootWidget (the JSON-loaded tree), NOT from 'this'
	if (RootWidget)
	{
		VitalsMiniSlot = UProjectWidgetHelpers::FindWidgetByNameTyped<UPanelWidget>(
			RootWidget, TEXT("VitalsMiniSlot"));
		StatusIconsSlot = UProjectWidgetHelpers::FindWidgetByNameTyped<UPanelWidget>(
			RootWidget, TEXT("StatusIconsSlot"));
		InteractionPromptContainer = UProjectWidgetHelpers::FindWidgetByNameTyped<UPanelWidget>(
			RootWidget, TEXT("InteractionPromptContainer"));
		MindThoughtSlot = UProjectWidgetHelpers::FindWidgetByNameTyped<UPanelWidget>(
			RootWidget, TEXT("MindThoughtSlot"));
		ToastContainer = UProjectWidgetHelpers::FindWidgetByNameTyped<UPanelWidget>(
			RootWidget, TEXT("ToastContainer"));
	}
	else
	{
		UE_LOG(LogHUDLayout, Warning, TEXT("[Thread:%u] NativeConstruct: RootWidget is null, cannot find slots"), FPlatformTLS::GetCurrentThreadId());
	}

	LOG_HUD_INIT("NativeConstruct: RootWidget=%s VitalsMiniSlot=%s StatusIconsSlot=%s InteractionPromptContainer=%s MindThoughtSlot=%s ToastContainer=%s",
		RootWidget ? *RootWidget->GetName() : TEXT("null"),
		VitalsMiniSlot.IsValid() ? TEXT("found") : TEXT("NOT FOUND"),
		StatusIconsSlot.IsValid() ? TEXT("found") : TEXT("NOT FOUND"),
		InteractionPromptContainer.IsValid() ? TEXT("found") : TEXT("NOT FOUND"),
		MindThoughtSlot.IsValid() ? TEXT("found") : TEXT("NOT FOUND"),
		ToastContainer.IsValid() ? TEXT("found") : TEXT("NOT FOUND"));

	SlotContainers.Empty();
	if (VitalsMiniSlot.IsValid())
	{
		SlotContainers.Add(ProjectTags::HUD_Slot_VitalsMini, VitalsMiniSlot);
	}
	if (StatusIconsSlot.IsValid())
	{
		SlotContainers.Add(ProjectTags::HUD_Slot_StatusIcons, StatusIconsSlot);
	}
	if (MindThoughtSlot.IsValid())
	{
		SlotContainers.Add(ProjectTags::HUD_Slot_MindThought, MindThoughtSlot);
	}

	// Initial rebuild
	if (VitalsMiniSlot.IsValid())
	{
		LOG_HUD_INIT("NativeConstruct: Rebuilding VitalsMiniSlot");
		RebuildSlot(ProjectTags::HUD_Slot_VitalsMini, VitalsMiniSlot.Get());
	}
	if (StatusIconsSlot.IsValid())
	{
		LOG_HUD_INIT("NativeConstruct: Rebuilding StatusIconsSlot");
		RebuildSlot(ProjectTags::HUD_Slot_StatusIcons, StatusIconsSlot.Get());
	}
	if (MindThoughtSlot.IsValid())
	{
		LOG_HUD_INIT("NativeConstruct: Rebuilding MindThoughtSlot");
		RebuildSlot(ProjectTags::HUD_Slot_MindThought, MindThoughtSlot.Get());
	}

	// Create interaction prompt widget (fixed, not slot-managed)
	// Architecture: Widget only reads from ViewModel, ViewModel handles service subscription
	if (InteractionPromptContainer.IsValid())
	{
		LOG_HUD_INIT("NativeConstruct: Creating InteractionPrompt widget");

		// Create widget with explicit OwningPlayer to ensure proper player context
		UW_InteractionPrompt* Prompt = CreateWidget<UW_InteractionPrompt>(GetOwningPlayer());
		if (Prompt)
		{
			// Create ViewModel and initialize with widget context
			// ViewModel resolves local pawn from widget's ownership chain
			UInteractionPromptViewModel* PromptVM = NewObject<UInteractionPromptViewModel>(Prompt);
			if (PromptVM)
			{
				PromptVM->Initialize(this);
				Prompt->SetInteractionViewModel(PromptVM);
				LOG_HUD_INIT("NativeConstruct: InteractionPromptViewModel created and bound");
			}

			InteractionPromptContainer->AddChild(Prompt);
			InteractionPrompt = Prompt;
			LOG_HUD_INIT("NativeConstruct: InteractionPrompt created and added to container");
		}
		else
		{
			UE_LOG(LogHUDLayout, Warning, TEXT("NativeConstruct: Failed to create InteractionPrompt widget"));
		}
	}

	// Initialize toast system (sets up toast container with subsystem)
	InitializeToastSystem();

	LOG_HUD_INIT("NativeConstruct END - ViewModelsByClass.Num=%d", ViewModelsByClass.Num());
}

void UW_HUDLayout::NativeDestruct()
{
	LOG_HUD_INIT("NativeDestruct START");
	Super::NativeDestruct();
	LOG_HUD_INIT("NativeDestruct END");
}

void UW_HUDLayout::RebuildSlot(FGameplayTag SlotTag, UPanelWidget* Container)
{
	LOG_HUD_INIT("RebuildSlot START - Slot=%s", *SlotTag.ToString());

	if (!Container)
	{
		UE_LOG(LogHUDLayout, Warning, TEXT("[Thread:%u] RebuildSlot: Container is null for slot %s"),
			FPlatformTLS::GetCurrentThreadId(), *SlotTag.ToString());
		return;
	}

	// Diagnose the SLOT CONTAINER itself - this might be the problem
	LOG_HUD_INIT("RebuildSlot: === SLOT CONTAINER DIAGNOSTICS ===");
	LOG_HUD_INIT("RebuildSlot: Container=%s Class=%s",
		*Container->GetName(), *Container->GetClass()->GetName());
	LOG_HUD_INIT("RebuildSlot: Container IsVisible=%s",
		Container->IsVisible() ? TEXT("YES") : TEXT("NO"));

	FVector2D ContainerSize = Container->GetCachedGeometry().GetLocalSize();
	LOG_HUD_INIT("RebuildSlot: Container Size=%.0fx%.0f %s",
		ContainerSize.X, ContainerSize.Y,
		ContainerSize.IsNearlyZero() ? TEXT("[ZERO - CONTAINER HIDDEN!]") : TEXT(""));

	// Check container's slot
	if (UCanvasPanelSlot* ContainerSlot = Cast<UCanvasPanelSlot>(Container->Slot))
	{
		FVector2D SlotSize = ContainerSlot->GetSize();
		LOG_HUD_INIT("RebuildSlot: Container in CanvasSlot AutoSize=%s SlotSize=(%.0f,%.0f) ZOrder=%d",
			ContainerSlot->GetAutoSize() ? TEXT("true") : TEXT("false"),
			SlotSize.X, SlotSize.Y,
			ContainerSlot->GetZOrder());

		FAnchors Anchors = ContainerSlot->GetAnchors();
		LOG_HUD_INIT("RebuildSlot: Container Anchors Min(%.2f,%.2f) Max(%.2f,%.2f)",
			Anchors.Minimum.X, Anchors.Minimum.Y,
			Anchors.Maximum.X, Anchors.Maximum.Y);

		FVector2D Offset = ContainerSlot->GetPosition();
		LOG_HUD_INIT("RebuildSlot: Container Offset=(%.0f, %.0f)",
			Offset.X, Offset.Y);
	}
	LOG_HUD_INIT("RebuildSlot: === END SLOT CONTAINER DIAGNOSTICS ===");

	// 1. Clear existing widgets from this slot
	ClearSlot(SlotTag, Container);

	// 2. Query fresh extension list (sorted by Priority desc)
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogHUDLayout, Warning, TEXT("[Thread:%u] RebuildSlot: No GameInstance"), FPlatformTLS::GetCurrentThreadId());
		return;
	}

	UProjectUIRegistrySubsystem* Registry = GI->GetSubsystem<UProjectUIRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogHUDLayout, Warning, TEXT("[Thread:%u] RebuildSlot: ProjectUIRegistrySubsystem not found"),
			FPlatformTLS::GetCurrentThreadId());
		return;
	}

	UProjectUIFactorySubsystem* Factory = GI->GetSubsystem<UProjectUIFactorySubsystem>();
	if (!Factory)
	{
		UE_LOG(LogHUDLayout, Warning, TEXT("[Thread:%u] RebuildSlot: ProjectUIFactorySubsystem not found"),
			FPlatformTLS::GetCurrentThreadId());
		return;
	}

	TArray<const FProjectUIDefinition*> SlotDefinitions;
	Registry->GetDefinitionsForSlot(SlotTag, SlotDefinitions);

	LOG_HUD_INIT("RebuildSlot: Slot=%s Definitions=%d", *SlotTag.ToString(), SlotDefinitions.Num());

	// Create and add widgets (definitions already sorted by priority).
	TArray<TWeakObjectPtr<UUserWidget>>& Widgets = SlotWidgets.FindOrAdd(SlotTag);
	for (const FProjectUIDefinition* Definition : SlotDefinitions)
	{
		if (!Definition)
		{
			continue;
		}

		LOG_HUD_INIT("RebuildSlot: Creating widget for %s", *Definition->Id.ToString());
		// Pass 'this' as context - factory handles player resolution internally
		FProjectUIWidgetCreateResult Result = Factory->CreateWidgetForDefinition(*Definition, this);
		UUserWidget* NewWidget = Result.Widget;
		if (!NewWidget)
		{
			UE_LOG(LogHUDLayout, Warning, TEXT("[Thread:%u] RebuildSlot: Failed to create widget for %s"),
				FPlatformTLS::GetCurrentThreadId(), *Definition->Id.ToString());
			continue;
		}

		if (!Factory->AttachWidgetToSlot(Container, NewWidget, *Definition))
		{
			UE_LOG(LogHUDLayout, Warning, TEXT("[Thread:%u] RebuildSlot: Failed to attach widget for %s"),
				FPlatformTLS::GetCurrentThreadId(), *Definition->Id.ToString());
		}

		Widgets.Add(NewWidget);

		if (Result.ViewModel)
		{
			SlotViewModels.FindOrAdd(SlotTag).Add(Result.ViewModel);
			ViewModelsByClass.Add(Result.ViewModel->GetClass(), Result.ViewModel);
			LOG_HUD_INIT("RebuildSlot: Registered ViewModel %s in ViewModelsByClass (total=%d)",
				*Result.ViewModel->GetClass()->GetName(), ViewModelsByClass.Num());
		}

		// Auto-diagnose after Slate layout (0.1s delay)
		if (UProjectUserWidget* ProjectWidget = Cast<UProjectUserWidget>(NewWidget))
		{
			TWeakObjectPtr<UProjectUserWidget> WeakWidget = ProjectWidget;
			if (UWorld* World = GetWorld())
			{
				FTimerHandle TimerHandle;
				World->GetTimerManager().SetTimer(TimerHandle, [WeakWidget]()
				{
					if (WeakWidget.IsValid())
					{
						// Some HUD widgets (e.g., Mind toast) are intentionally collapsed until runtime data arrives.
						// Skip zero-size warning for non-visible widgets to avoid false positives.
						if (!WeakWidget->IsVisible())
						{
							return;
						}

						FVector2D Size = WeakWidget->GetCachedGeometry().GetLocalSize();
						if (Size.IsNearlyZero())
						{
							// Only log if still zero after delay - might be real issue
							UE_LOG(LogHUDLayout, Warning, TEXT("%s: Size still 0x0 after layout"), *WeakWidget->GetName());
						}
					}
				}, 0.1f, false);
			}
		}
	}
	LOG_HUD_INIT("RebuildSlot END - Slot=%s Widgets=%d", *SlotTag.ToString(), Widgets.Num());
}

void UW_HUDLayout::ClearSlot(FGameplayTag SlotTag, UPanelWidget* Container)
{
	if (!Container)
	{
		return;
	}

	// Remove tracked widgets for this slot
	if (TArray<TWeakObjectPtr<UUserWidget>>* Widgets = SlotWidgets.Find(SlotTag))
	{
		for (TWeakObjectPtr<UUserWidget>& WeakWidget : *Widgets)
		{
			if (UUserWidget* Widget = WeakWidget.Get())
			{
				Widget->RemoveFromParent();
			}
		}
		Widgets->Empty();

		UE_LOG(LogHUDLayout, Verbose, TEXT("ClearSlot: Cleared slot %s"), *SlotTag.ToString());
	}

	if (TArray<TWeakObjectPtr<UProjectViewModel>>* ViewModels = SlotViewModels.Find(SlotTag))
	{
		for (TWeakObjectPtr<UProjectViewModel>& WeakVM : *ViewModels)
		{
			if (UProjectViewModel* VM = WeakVM.Get())
			{
				UClass* VMClass = VM->GetClass();
				if (const TWeakObjectPtr<UProjectViewModel>* Existing = ViewModelsByClass.Find(VMClass))
				{
					if (Existing->Get() == VM)
					{
						ViewModelsByClass.Remove(VMClass);
					}
				}
			}
		}

		ViewModels->Empty();
	}
}

void UW_HUDLayout::InitializeToastSystem()
{
	// Set up toast container with subsystem
	if (ToastContainer.IsValid())
	{
		UGameInstance* GI = GetGameInstance();
		if (GI)
		{
			UProjectToastSubsystem* ToastSub = GI->GetSubsystem<UProjectToastSubsystem>();
			if (ToastSub)
			{
				ToastSub->SetToastContainer(ToastContainer.Get(), GetOwningPlayer());
				LOG_HUD_INIT("Toast subsystem initialized with container");
			}
		}
	}
}
