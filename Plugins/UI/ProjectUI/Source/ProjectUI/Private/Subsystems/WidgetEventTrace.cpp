// Copyright ALIS. All Rights Reserved.
// WidgetEventTrace.cpp - Widget lifecycle event ring buffer implementation
// Extracted from ProjectUIDebugSubsystem.cpp for SOLID compliance

#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

// =============================================================================
// FProjectUIWidgetEventEntry
// =============================================================================

FString FProjectUIWidgetEventEntry::ToString() const
{
	const TCHAR* EventNames[] = {
		TEXT("Created"),
		TEXT("AddedToParent"),
		TEXT("RemovedFromParent"),
		TEXT("VisibilityChanged"),
		TEXT("SlotChanged"),
		TEXT("GeometryChanged"),
		TEXT("ViewModelBound"),
		TEXT("ViewModelUnbound"),
		TEXT("Destroyed")
	};

	const TCHAR* EventName = (static_cast<uint8>(EventType) < UE_ARRAY_COUNT(EventNames))
		? EventNames[static_cast<uint8>(EventType)]
		: TEXT("Unknown");

	return FString::Printf(TEXT("[%.3f F%lld] %s (%s) %s%s%s"),
		Timestamp,
		FrameNumber,
		*WidgetName,
		*WidgetClass,
		EventName,
		Details.IsEmpty() ? TEXT("") : TEXT(": "),
		*Details);
}

// =============================================================================
// Event Trace Control
// =============================================================================

void UProjectUIDebugSubsystem::SetEventTraceEnabled(bool bEnabled)
{
	if (bEnabled == bEventTraceEnabled)
	{
		return;
	}

	bEventTraceEnabled = bEnabled;

	if (bEnabled)
	{
		EventTraceBuffer.SetNum(EventTraceCapacity);
		EventTraceWriteIndex = 0;
		EventTraceCount = 0;
		UE_LOG(LogProjectUIDebug, Display, TEXT("Event trace enabled (capacity: %d)"), EventTraceCapacity);
	}
	else
	{
		UE_LOG(LogProjectUIDebug, Display, TEXT("Event trace disabled (recorded %d events)"), EventTraceCount);
	}
}

void UProjectUIDebugSubsystem::RecordWidgetEvent(
	EProjectUIWidgetEvent EventType,
	UWidget* Widget,
	const FString& Details)
{
	if (!bEventTraceEnabled || !Widget)
	{
		return;
	}

	FProjectUIWidgetEventEntry& Entry = EventTraceBuffer[EventTraceWriteIndex];
	Entry.Timestamp = FPlatformTime::Seconds();
	Entry.FrameNumber = GFrameNumber;
	Entry.EventType = EventType;
	Entry.WidgetName = Widget->GetName();
	Entry.WidgetClass = Widget->GetClass()->GetName();
	Entry.Details = Details;

	if (UPanelWidget* Parent = Widget->GetParent())
	{
		Entry.ParentName = Parent->GetName();
	}
	else
	{
		Entry.ParentName.Empty();
	}

	EventTraceWriteIndex = (EventTraceWriteIndex + 1) % EventTraceCapacity;
	++EventTraceCount;
}

// =============================================================================
// Event Trace Retrieval
// =============================================================================

TArray<FProjectUIWidgetEventEntry> UProjectUIDebugSubsystem::GetEventTrace() const
{
	TArray<FProjectUIWidgetEventEntry> Result;

	if (EventTraceCount == 0)
	{
		return Result;
	}

	const int32 NumEvents = FMath::Min(EventTraceCount, EventTraceCapacity);
	Result.Reserve(NumEvents);

	int32 ReadIndex = (EventTraceCount >= EventTraceCapacity)
		? EventTraceWriteIndex
		: 0;

	for (int32 i = 0; i < NumEvents; ++i)
	{
		Result.Add(EventTraceBuffer[ReadIndex]);
		ReadIndex = (ReadIndex + 1) % EventTraceCapacity;
	}

	return Result;
}

TArray<FProjectUIWidgetEventEntry> UProjectUIDebugSubsystem::GetEventTraceFiltered(const FString& WidgetNamePattern) const
{
	TArray<FProjectUIWidgetEventEntry> AllEvents = GetEventTrace();
	TArray<FProjectUIWidgetEventEntry> Filtered;

	for (const FProjectUIWidgetEventEntry& Entry : AllEvents)
	{
		if (Entry.WidgetName.Contains(WidgetNamePattern))
		{
			Filtered.Add(Entry);
		}
	}

	return Filtered;
}

void UProjectUIDebugSubsystem::ClearEventTrace()
{
	EventTraceWriteIndex = 0;
	EventTraceCount = 0;
	UE_LOG(LogProjectUIDebug, Display, TEXT("Event trace cleared"));
}

// =============================================================================
// Event Trace Output
// =============================================================================

bool UProjectUIDebugSubsystem::DumpEventTrace(const FString& OutPath) const
{
	TArray<FProjectUIWidgetEventEntry> Events = GetEventTrace();

	if (Events.Num() == 0)
	{
		UE_LOG(LogProjectUIDebug, Display, TEXT("Event trace is empty"));
		return true;
	}

	TArray<FString> Lines;
	Lines.Add(TEXT("==== WIDGET EVENT TRACE ===="));
	Lines.Add(FString::Printf(TEXT("Total events: %d (capacity: %d)"), EventTraceCount, EventTraceCapacity));
	Lines.Add(TEXT("Format: [Timestamp Frame] WidgetName (Class) Event: Details"));
	Lines.Add(TEXT("----------------------------"));

	for (const FProjectUIWidgetEventEntry& Entry : Events)
	{
		Lines.Add(Entry.ToString());
	}

	Lines.Add(TEXT("----------------------------"));

	const bool bToFile = !OutPath.IsEmpty();
	if (bToFile)
	{
		FString FullPath = OutPath;
		if (FPaths::IsRelative(FullPath))
		{
			FullPath = FPaths::ProjectSavedDir() / FullPath;
		}
		FPaths::NormalizeFilename(FullPath);

		FString Dir = FPaths::GetPath(FullPath);
		IFileManager::Get().MakeDirectory(*Dir, true);

		FString Output = FString::Join(Lines, TEXT("\n"));
		if (FFileHelper::SaveStringToFile(Output, *FullPath))
		{
			UE_LOG(LogProjectUIDebug, Display, TEXT("Event trace saved to: %s"), *FullPath);
			return true;
		}
		else
		{
			UE_LOG(LogProjectUIDebug, Error, TEXT("Failed to write event trace to: %s"), *FullPath);
			return false;
		}
	}
	else
	{
		for (const FString& Line : Lines)
		{
			UE_LOG(LogProjectUIDebug, Display, TEXT("%s"), *Line);
		}
		return true;
	}
}

void UProjectUIDebugSubsystem::SetEventTraceCapacity(int32 NewCapacity)
{
	NewCapacity = FMath::Max(16, NewCapacity);

	if (NewCapacity == EventTraceCapacity)
	{
		return;
	}

	if (bEventTraceEnabled)
	{
		TArray<FProjectUIWidgetEventEntry> OldEvents = GetEventTrace();
		EventTraceCapacity = NewCapacity;
		EventTraceBuffer.SetNum(EventTraceCapacity);
		EventTraceWriteIndex = 0;
		EventTraceCount = 0;

		const int32 CopyCount = FMath::Min(OldEvents.Num(), EventTraceCapacity);
		const int32 StartIndex = OldEvents.Num() - CopyCount;
		for (int32 i = 0; i < CopyCount; ++i)
		{
			EventTraceBuffer[i] = OldEvents[StartIndex + i];
		}
		EventTraceWriteIndex = CopyCount % EventTraceCapacity;
		EventTraceCount = CopyCount;
	}
	else
	{
		EventTraceCapacity = NewCapacity;
	}

	UE_LOG(LogProjectUIDebug, Display, TEXT("Event trace capacity set to %d"), EventTraceCapacity);
}
