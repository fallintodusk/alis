// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectUIFactorySubsystem.h"
#include "MVVM/ProjectViewModel.h"
#include "Widgets/ProjectUserWidget.h"
#include "Widgets/ProjectActivatableWidget.h"
#include "Interfaces/ProjectViewModelConsumer.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectUIFactory, Log, All);

static TAutoConsoleVariable<int32> CVarUIForceCreateInstance(
	TEXT("ui.vm.force_create_instance"),
	0,
	TEXT("Force CreateInstance ViewModel policy (ignores Global/PropertyPath)."),
	ECVF_Default);

UUserWidget* UProjectUIFactorySubsystem::CreateWidgetByClass(UObject* WorldContext, TSubclassOf<UUserWidget> WidgetClass)
{
	if (!WorldContext || !WidgetClass)
	{
		return nullptr;
	}

	// Use a supported owning object for CreateWidget.
	if (APlayerController* PlayerController = Cast<APlayerController>(WorldContext))
	{
		return CreateWidget<UUserWidget>(PlayerController, WidgetClass);
	}

	if (UUserWidget* OwningWidget = Cast<UUserWidget>(WorldContext))
	{
		if (APlayerController* OwningPlayer = OwningWidget->GetOwningPlayer())
		{
			return CreateWidget<UUserWidget>(OwningPlayer, WidgetClass);
		}
	}

	if (UGameInstance* GameInstance = Cast<UGameInstance>(WorldContext))
	{
		return CreateWidget<UUserWidget>(GameInstance, WidgetClass);
	}

	if (UWorld* World = Cast<UWorld>(WorldContext))
	{
		return CreateWidget<UUserWidget>(World, WidgetClass);
	}

	if (UWidget* WidgetOwner = Cast<UWidget>(WorldContext))
	{
		return CreateWidget<UUserWidget>(WidgetOwner, WidgetClass);
	}

	if (UWidgetTree* WidgetTree = Cast<UWidgetTree>(WorldContext))
	{
		return CreateWidget<UUserWidget>(WidgetTree, WidgetClass);
	}

	if (UWorld* World = WorldContext->GetWorld())
	{
		return CreateWidget<UUserWidget>(World, WidgetClass);
	}

	UE_LOG(LogProjectUIFactory, Warning, TEXT("CreateWidgetByClass: Unsupported WorldContext type %s"),
		*WorldContext->GetClass()->GetName());
	return nullptr;
}

FProjectUIWidgetCreateResult UProjectUIFactorySubsystem::CreateWidgetForDefinition(const FProjectUIDefinition& Definition, UObject* WorldContext)
{
	FProjectUIWidgetCreateResult Result;

	if (!WorldContext)
	{
		UE_LOG(LogProjectUIFactory, Warning, TEXT("CreateWidgetForDefinition: Missing WorldContext for %s"),
			*Definition.Id.ToString());
		return Result;
	}

	UClass* WidgetClass = Definition.WidgetClassPath.TryLoadClass<UUserWidget>();
	if (!WidgetClass)
	{
		UE_LOG(LogProjectUIFactory, Error, TEXT("CreateWidgetForDefinition: WidgetClass failed to load (%s)"),
			*Definition.WidgetClassPath.ToString());
		return Result;
	}

	UUserWidget* Widget = CreateWidgetByClass(WorldContext, WidgetClass);
	if (!Widget)
	{
		UE_LOG(LogProjectUIFactory, Error, TEXT("CreateWidgetForDefinition: CreateWidget failed for %s"),
			*Definition.Id.ToString());
		return Result;
	}

	// Apply layout path override for ProjectUserWidget types.
	if (UProjectUserWidget* ProjectWidget = Cast<UProjectUserWidget>(Widget))
	{
		if (!Definition.LayoutJson.IsEmpty())
		{
			const FString LayoutPath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(
				Definition.SourcePluginName, Definition.LayoutJson);
			ProjectWidget->SetConfigFilePath(LayoutPath);
		}
	}

	bool bShouldInitialize = false;
	UProjectViewModel* ViewModel = CreateOrResolveViewModel(Definition, WorldContext, Widget, bShouldInitialize);
	if (ViewModel)
	{
		InjectViewModel(Widget, ViewModel, WorldContext, bShouldInitialize);
	}

	Result.Widget = Widget;
	Result.ViewModel = ViewModel;
	return Result;
}

bool UProjectUIFactorySubsystem::AttachWidgetToSlot(UPanelWidget* Container, UUserWidget* Widget, const FProjectUIDefinition& Definition)
{
	UE_LOG(LogProjectUIFactory, Log, TEXT("AttachWidgetToSlot ENTRY: Container=%s (Class=%s) Widget=%s Definition=%s"),
		Container ? *Container->GetName() : TEXT("null"),
		Container ? *Container->GetClass()->GetName() : TEXT("null"),
		Widget ? *Widget->GetName() : TEXT("null"),
		*Definition.Id.ToString());

	if (!Container || !Widget)
	{
		return false;
	}

	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Container))
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
		if (!CanvasSlot)
		{
			return false;
		}

		// Centralized sizing policy for HUD slots.
		switch (Definition.SizePolicy)
		{
		case EProjectUISizePolicy::Fill:
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
			CanvasSlot->SetSize(FVector2D::ZeroVector);
			CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
			CanvasSlot->SetAutoSize(false);
			break;
		case EProjectUISizePolicy::AutoSize:
			CanvasSlot->SetAutoSize(true);
			break;
		case EProjectUISizePolicy::Desired:
		default:
			CanvasSlot->SetAutoSize(true);
			break;
		}

		CanvasSlot->SetZOrder(Definition.Priority);
		return true;
	}

	if (UOverlay* Overlay = Cast<UOverlay>(Container))
	{
		UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Widget);
		if (!OverlaySlot)
		{
			UE_LOG(LogProjectUIFactory, Warning, TEXT("AttachWidgetToSlot: AddChildToOverlay returned null"));
			return false;
		}

		// UE5 UOverlaySlot race condition: OnSlotAdded() calls BuildSlot() IMMEDIATELY
		// when the parent Slate widget exists. BuildSlot() creates the Slate slot with
		// default Left/Top alignment BEFORE we can call SetHorizontalAlignment().
		//
		// Fix: Set alignment properties, then call SynchronizeProperties() to force
		// the Slate slot to re-sync with the UPROPERTY values.
		// Reference: https://forums.unrealengine.com/t/umg-how-to-maintain-anchoring-of-widget-when-added-as-child-to-another-widget/460577

		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);

		// CRITICAL: SynchronizeProperties() re-applies UPROPERTY values to the Slate slot.
		// This is the UE5 best practice for dynamically added overlay children.
		OverlaySlot->SynchronizeProperties();

		UE_LOG(LogProjectUIFactory, Log, TEXT("AttachWidgetToSlot: Overlay %s <- %s (Fill alignment + SynchronizeProperties)"),
			*Container->GetName(), *Widget->GetName());

		return true;
	}

	Container->AddChild(Widget);
	return true;
}

UProjectViewModel* UProjectUIFactorySubsystem::GetSharedViewModel(TSubclassOf<UProjectViewModel> ViewModelClass) const
{
	if (!ViewModelClass)
	{
		return nullptr;
	}

	if (const TObjectPtr<UProjectViewModel>* Found = SharedViewModels.Find(ViewModelClass.Get()))
	{
		return Found->Get();
	}

	return nullptr;
}

// Decouples VM lifecycle from widget lifecycle: VM exists before ShowDefinition.
// Used by auto_visibility so LayerHost can watch VM state before widget exists.
UProjectViewModel* UProjectUIFactorySubsystem::EnsureGlobalViewModel(const FProjectUIDefinition& Definition)
{
	if (Definition.ViewModelCreationPolicy != EProjectUIViewModelCreationPolicy::Global)
	{
		return nullptr;
	}

	if (!Definition.ViewModelClassPath.IsValid())
	{
		return nullptr;
	}

	UClass* ViewModelClass = Definition.ViewModelClassPath.TryLoadClass<UProjectViewModel>();
	if (!ViewModelClass)
	{
		UE_LOG(LogProjectUIFactory, Warning, TEXT("EnsureGlobalViewModel: Failed to load class %s"),
			*Definition.ViewModelClassPath.ToString());
		return nullptr;
	}

	if (TObjectPtr<UProjectViewModel>* Found = SharedViewModels.Find(ViewModelClass))
	{
		return Found->Get();
	}

	UProjectViewModel* VM = NewObject<UProjectViewModel>(GetGameInstance(), ViewModelClass);
	SharedViewModels.Add(ViewModelClass, VM);
	VM->Initialize(GetGameInstance());
	UE_LOG(LogProjectUIFactory, Log, TEXT("EnsureGlobalViewModel: Created %s for definition %s"),
		*ViewModelClass->GetName(), *Definition.Id.ToString());
	return VM;
}

UProjectViewModel* UProjectUIFactorySubsystem::CreateOrResolveViewModel(const FProjectUIDefinition& Definition, UObject* Context, UUserWidget* WidgetOuter, bool& bOutShouldInitialize)
{
	bOutShouldInitialize = false;

	if (!Definition.ViewModelClassPath.IsValid())
	{
		return nullptr;
	}

	EProjectUIViewModelCreationPolicy Policy = Definition.ViewModelCreationPolicy;
	if (CVarUIForceCreateInstance.GetValueOnGameThread() != 0)
	{
		Policy = EProjectUIViewModelCreationPolicy::CreateInstance;
	}

	UClass* ViewModelClass = Definition.ViewModelClassPath.TryLoadClass<UProjectViewModel>();
	if (!ViewModelClass)
	{
		UE_LOG(LogProjectUIFactory, Warning, TEXT("CreateOrResolveViewModel: Failed to load ViewModelClass (%s)"),
			*Definition.ViewModelClassPath.ToString());
		return nullptr;
	}

	switch (Policy)
	{
	case EProjectUIViewModelCreationPolicy::Global:
		{
			if (TObjectPtr<UProjectViewModel>* Found = SharedViewModels.Find(ViewModelClass))
			{
				return Found->Get();
			}

			UProjectViewModel* SharedVM = NewObject<UProjectViewModel>(GetGameInstance(), ViewModelClass);
			SharedViewModels.Add(ViewModelClass, SharedVM);
			bOutShouldInitialize = true;
			return SharedVM;
		}
	case EProjectUIViewModelCreationPolicy::PropertyPath:
		{
			UProjectViewModel* Resolved = ResolveViewModelPropertyPath(Context, Definition.ViewModelPropertyPath);
			if (Resolved)
			{
				return Resolved;
			}

			UE_LOG(LogProjectUIFactory, Warning, TEXT("PropertyPath failed for %s, falling back to CreateInstance"),
				*Definition.Id.ToString());
			bOutShouldInitialize = true;
			return NewObject<UProjectViewModel>(WidgetOuter, ViewModelClass);
		}
	case EProjectUIViewModelCreationPolicy::CreateInstance:
	default:
		bOutShouldInitialize = true;
		return NewObject<UProjectViewModel>(WidgetOuter, ViewModelClass);
	}
}

UProjectViewModel* UProjectUIFactorySubsystem::ResolveViewModelPropertyPath(UObject* Context, const FString& PropertyPath) const
{
	if (!Context || PropertyPath.IsEmpty())
	{
		return nullptr;
	}

	// KISS: support simple UObject property chains (no function calls).
	UObject* Current = Context;
	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);

	for (const FString& Segment : Segments)
	{
		if (!Current)
		{
			return nullptr;
		}

		FProperty* Prop = Current->GetClass()->FindPropertyByName(FName(*Segment));
		if (!Prop)
		{
			UE_LOG(LogProjectUIFactory, Warning, TEXT("PropertyPath segment not found: %s"), *Segment);
			return nullptr;
		}

		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
		{
			Current = ObjProp->GetObjectPropertyValue_InContainer(Current);
		}
		else
		{
			UE_LOG(LogProjectUIFactory, Warning, TEXT("PropertyPath segment is not UObject: %s"), *Segment);
			return nullptr;
		}
	}

	return Cast<UProjectViewModel>(Current);
}

void UProjectUIFactorySubsystem::InjectViewModel(UUserWidget* Widget, UProjectViewModel* ViewModel, UObject* Context, bool bShouldInitialize) const
{
	if (!Widget || !ViewModel)
	{
		return;
	}

	// Widgets implementing IProjectViewModelConsumer receive initialized ViewModel.
	// Initialize with widget as context so ViewModel can resolve player from ownership chain.
	if (IProjectViewModelConsumer* Consumer = Cast<IProjectViewModelConsumer>(Widget))
	{
		if (bShouldInitialize)
		{
			ViewModel->Initialize(Widget);
		}
		Consumer->SetViewModel(ViewModel);
		return;
	}

	// Default injection path for Project widgets.
	if (UProjectActivatableWidget* Activatable = Cast<UProjectActivatableWidget>(Widget))
	{
		if (bShouldInitialize)
		{
			ViewModel->Initialize(Context);
		}
		Activatable->SetViewModel(ViewModel);
		return;
	}

	if (UProjectUserWidget* ProjectWidget = Cast<UProjectUserWidget>(Widget))
	{
		if (bShouldInitialize)
		{
			ViewModel->Initialize(Context);
		}
		ProjectWidget->SetViewModel(ViewModel);
		return;
	}
}
