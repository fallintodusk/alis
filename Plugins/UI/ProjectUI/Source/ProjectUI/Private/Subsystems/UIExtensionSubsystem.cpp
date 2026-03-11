// Copyright ALIS. All Rights Reserved.

#include "Subsystems/UIExtensionSubsystem.h"
#include "Widgets/W_LayerStack.h"

DEFINE_LOG_CATEGORY_STATIC(LogUIExtension, Log, All);

FUIExtensionHandle UUIExtensionSubsystem::RegisterSlotExtension(const FUISlotExtensionData& Data)
{
	FUIExtensionHandle Handle;
	Handle.Id = FGuid::NewGuid();

	FStoredSlotExtension Stored;
	Stored.Data = Data;
	Stored.RegistrationOrder = NextRegistrationOrder++;

	SlotExtensions.Add(Handle.Id, Stored);

	UE_LOG(LogUIExtension, Log, TEXT("RegisterSlotExtension: Slot=%s Widget=%s Priority=%d Order=%llu Handle=%s"),
		*Data.SlotTag.ToString(),
		Data.WidgetClass ? *Data.WidgetClass->GetName() : TEXT("null"),
		Data.Priority,
		Stored.RegistrationOrder,
		*Handle.Id.ToString());

	OnSlotExtensionChanged.Broadcast(Data.SlotTag);

	return Handle;
}

void UUIExtensionSubsystem::UnregisterExtension(FUIExtensionHandle Handle)
{
	if (!Handle.IsValid())
	{
		UE_LOG(LogUIExtension, Warning, TEXT("UnregisterExtension: Invalid handle"));
		return;
	}

	// Try slot extensions first
	if (FStoredSlotExtension* Stored = SlotExtensions.Find(Handle.Id))
	{
		const FGameplayTag SlotTag = Stored->Data.SlotTag;

		UE_LOG(LogUIExtension, Log, TEXT("UnregisterExtension: Slot=%s Handle=%s"),
			*SlotTag.ToString(),
			*Handle.Id.ToString());

		SlotExtensions.Remove(Handle.Id);
		OnSlotExtensionChanged.Broadcast(SlotTag);
		return;
	}

	// Try layer extensions
	if (FStoredLayerExtension* Stored = LayerExtensions.Find(Handle.Id))
	{
		const FGameplayTag LayerTag = Stored->Data.LayerTag;

		UE_LOG(LogUIExtension, Log, TEXT("UnregisterExtension: Layer=%s Handle=%s"),
			*LayerTag.ToString(),
			*Handle.Id.ToString());

		LayerExtensions.Remove(Handle.Id);
		OnLayerExtensionChanged.Broadcast(LayerTag);
		return;
	}

	UE_LOG(LogUIExtension, Warning, TEXT("UnregisterExtension: Handle not found: %s"),
		*Handle.Id.ToString());
}

TArray<FUISlotExtensionData> UUIExtensionSubsystem::GetExtensionsForSlot(FGameplayTag SlotTag) const
{
	TArray<const FStoredSlotExtension*> Matching;
	for (const auto& Pair : SlotExtensions)
	{
		if (Pair.Value.Data.SlotTag == SlotTag)
		{
			Matching.Add(&Pair.Value);
		}
	}

	// Sort by Priority descending, then by RegistrationOrder ascending for stable ordering
	// TArray<T*>::Sort uses TDereferenceWrapper - dereferences pointers, lambda receives references
	Matching.Sort([](const FStoredSlotExtension& A, const FStoredSlotExtension& B)
	{
		if (A.Data.Priority != B.Data.Priority)
		{
			return A.Data.Priority > B.Data.Priority; // Higher priority first
		}
		return A.RegistrationOrder < B.RegistrationOrder; // Earlier registered first (stable)
	});

	TArray<FUISlotExtensionData> Result;
	Result.Reserve(Matching.Num());
	for (const FStoredSlotExtension* Stored : Matching)
	{
		Result.Add(Stored->Data);
	}

	UE_LOG(LogUIExtension, Verbose, TEXT("GetExtensionsForSlot: Slot=%s Count=%d"),
		*SlotTag.ToString(), Result.Num());

	return Result;
}

// =============================================================================
// Layer Extensions (Phase 10.5)
// =============================================================================

FUIExtensionHandle UUIExtensionSubsystem::RegisterLayerExtension(const FUILayerExtensionData& Data)
{
	FUIExtensionHandle Handle;
	Handle.Id = FGuid::NewGuid();

	FStoredLayerExtension Stored;
	Stored.Data = Data;
	Stored.RegistrationOrder = NextRegistrationOrder++;

	LayerExtensions.Add(Handle.Id, Stored);

	UE_LOG(LogUIExtension, Log, TEXT("RegisterLayerExtension: Layer=%s Widget=%s Priority=%d Order=%llu Handle=%s"),
		*Data.LayerTag.ToString(),
		Data.WidgetClass ? *Data.WidgetClass->GetName() : TEXT("null"),
		Data.Priority,
		Stored.RegistrationOrder,
		*Handle.Id.ToString());

	OnLayerExtensionChanged.Broadcast(Data.LayerTag);

	return Handle;
}

TArray<FUILayerExtensionData> UUIExtensionSubsystem::GetExtensionsForLayer(FGameplayTag LayerTag) const
{
	TArray<const FStoredLayerExtension*> Matching;
	for (const auto& Pair : LayerExtensions)
	{
		if (Pair.Value.Data.LayerTag == LayerTag)
		{
			Matching.Add(&Pair.Value);
		}
	}

	// Sort by Priority descending, then by RegistrationOrder ascending for stable ordering
	// TArray<T*>::Sort uses TDereferenceWrapper - dereferences pointers, lambda receives references
	Matching.Sort([](const FStoredLayerExtension& A, const FStoredLayerExtension& B)
	{
		if (A.Data.Priority != B.Data.Priority)
		{
			return A.Data.Priority > B.Data.Priority; // Higher priority first
		}
		return A.RegistrationOrder < B.RegistrationOrder; // Earlier registered first (stable)
	});

	TArray<FUILayerExtensionData> Result;
	Result.Reserve(Matching.Num());
	for (const FStoredLayerExtension* Stored : Matching)
	{
		Result.Add(Stored->Data);
	}

	UE_LOG(LogUIExtension, Verbose, TEXT("GetExtensionsForLayer: Layer=%s Count=%d"),
		*LayerTag.ToString(), Result.Num());

	return Result;
}

void UUIExtensionSubsystem::SetLayerStackWidget(UW_LayerStack* InLayerStack)
{
	LayerStackWidget = InLayerStack;

	UE_LOG(LogUIExtension, Log, TEXT("SetLayerStackWidget: %s"),
		InLayerStack ? *InLayerStack->GetName() : TEXT("null"));
}

UW_LayerStack* UUIExtensionSubsystem::GetLayerStackWidget() const
{
	return LayerStackWidget.Get();
}
