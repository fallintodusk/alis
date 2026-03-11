// Copyright ALIS. All Rights Reserved.

#include "MVVM/MindJournalViewModel.h"
#include "Interfaces/IMindService.h"
#include "ProjectServiceLocator.h"

DEFINE_LOG_CATEGORY_STATIC(LogMindJournalVM, Log, All);

namespace
{
constexpr int32 MaxImportantThoughts = 40;
constexpr int32 MaxRecentThoughts = 30;

struct FCollapsedThoughtLine
{
	FMindThoughtView Thought;
	int32 Count = 0;
	bool bResolved = false;
};

bool ShouldAppearInImportant(const FMindThoughtView& Thought)
{
	return Thought.Channel == EMindThoughtChannel::Journal
		|| Thought.Channel == EMindThoughtChannel::ToastAndJournal;
}

bool ShouldAppearInRecent(const FMindThoughtView& Thought)
{
	return Thought.Channel == EMindThoughtChannel::Toast;
}

FString ResolveSourceLabel(EMindThoughtSourceType SourceType)
{
	switch (SourceType)
	{
	case EMindThoughtSourceType::Dialogue:
		return TEXT("Dialogue");
	case EMindThoughtSourceType::Vitals:
		return TEXT("Vitals");
	case EMindThoughtSourceType::Inventory:
		return TEXT("Inventory");
	case EMindThoughtSourceType::Scan:
		return TEXT("Scan");
	case EMindThoughtSourceType::Quest:
		return TEXT("Quest");
	case EMindThoughtSourceType::Beacon:
		return TEXT("Beacon");
	case EMindThoughtSourceType::System:
		return TEXT("System");
	default:
		return TEXT("Mind");
	}
}

FString ResolveTimeLabel(const FMindThoughtView& Thought)
{
	if (Thought.CreatedAtUtc != FDateTime())
	{
		return Thought.CreatedAtUtc.ToString(TEXT("%H:%M"));
	}

	return TEXT("--:--");
}

FString FormatImportantLine(const FCollapsedThoughtLine& Entry)
{
	const FString TimeLabel = ResolveTimeLabel(Entry.Thought);
	const FString Marker = Entry.bResolved
		? TEXT("[x]")
		: (Entry.Thought.Priority >= 100 ? TEXT("[!]") : TEXT("[*]"));
	return FString::Printf(TEXT("%s [%s] %s"), *Marker, *TimeLabel, *Entry.Thought.Text.ToString());
}

FString FormatRecentLine(const FMindThoughtView& Thought)
{
	const FString TimeLabel = ResolveTimeLabel(Thought);
	const FString SourceLabel = ResolveSourceLabel(Thought.SourceType);
	return FString::Printf(TEXT("[%s] (%s) %s"), *TimeLabel, *SourceLabel, *Thought.Text.ToString());
}

FString FormatCollapsedLine(const FString& BaseLine, int32 Count)
{
	if (Count <= 1)
	{
		return BaseLine;
	}

	return FString::Printf(TEXT("%s x%d"), *BaseLine, Count);
}

FString BuildThoughtCollapseKey(const FMindThoughtView& Thought)
{
	const FString ThoughtId = Thought.ThoughtId.IsNone() ? TEXT("None") : Thought.ThoughtId.ToString();
	return FString::Printf(
		TEXT("%s|%s|%d"),
		*ThoughtId,
		*Thought.Text.ToString(),
		static_cast<int32>(Thought.SourceType));
}
}

void UMindJournalViewModel::Initialize(UObject* Context)
{
	Super::Initialize(Context);

	UpdatebPanelVisible(false);
	UpdatebJournalTabActive(true);
	UpdateJournalText(FText::FromString(TEXT("No important records yet.")));
	UpdateRecentText(FText::FromString(TEXT("No recent activity yet.")));

	SubscribeToService();
}

void UMindJournalViewModel::Shutdown()
{
	StopServiceRetry();
	UnsubscribeFromService();
	Super::Shutdown();
}

bool UMindJournalViewModel::GetBoolProperty(FName PropertyName) const
{
	static const FName PanelVisibleProp(TEXT("bPanelVisible"));
	static const FName JournalTabProp(TEXT("bJournalTabActive"));

	if (PropertyName == PanelVisibleProp)
	{
		return GetbPanelVisible();
	}

	if (PropertyName == JournalTabProp)
	{
		return GetbJournalTabActive();
	}

	return Super::GetBoolProperty(PropertyName);
}

void UMindJournalViewModel::TogglePanel()
{
	UpdatebPanelVisible(!GetbPanelVisible());
}

void UMindJournalViewModel::ShowPanel()
{
	UpdatebPanelVisible(true);
}

void UMindJournalViewModel::HidePanel()
{
	UpdatebPanelVisible(false);
}

void UMindJournalViewModel::SetJournalTabActive(bool bActive)
{
	UpdatebJournalTabActive(bActive);
}

void UMindJournalViewModel::SubscribeToService()
{
	CachedService = FProjectServiceLocator::Resolve<IMindService>();
	if (!CachedService.IsValid())
	{
		if (!ServiceRetryHandle.IsValid())
		{
			ServiceRetryHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &UMindJournalViewModel::RetrySubscribeToService),
				0.5f);
		}

		UE_LOG(LogMindJournalVM, Verbose, TEXT("SubscribeToService: IMindService not available, waiting"));
		return;
	}

	ThoughtAddedHandle = CachedService->OnThoughtAdded().AddUObject(
		this, &UMindJournalViewModel::HandleThoughtAdded);
	FeedChangedHandle = CachedService->OnMindFeedChanged().AddUObject(
		this, &UMindJournalViewModel::HandleMindFeedChanged);

	RefreshFromService();
}

void UMindJournalViewModel::UnsubscribeFromService()
{
	if (!CachedService.IsValid())
	{
		return;
	}

	if (ThoughtAddedHandle.IsValid())
	{
		CachedService->OnThoughtAdded().Remove(ThoughtAddedHandle);
		ThoughtAddedHandle.Reset();
	}

	if (FeedChangedHandle.IsValid())
	{
		CachedService->OnMindFeedChanged().Remove(FeedChangedHandle);
		FeedChangedHandle.Reset();
	}

	CachedService.Reset();
}

bool UMindJournalViewModel::RetrySubscribeToService(float /*DeltaTime*/)
{
	if (CachedService.IsValid())
	{
		StopServiceRetry();
		return false;
	}

	SubscribeToService();
	if (CachedService.IsValid())
	{
		StopServiceRetry();
		return false;
	}

	return true;
}

void UMindJournalViewModel::StopServiceRetry()
{
	if (ServiceRetryHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ServiceRetryHandle);
		ServiceRetryHandle.Reset();
	}
}

void UMindJournalViewModel::RefreshFromService()
{
	if (!CachedService.IsValid())
	{
		UpdateJournalText(FText::FromString(TEXT("No important records yet.")));
		UpdateRecentText(FText::FromString(TEXT("No recent activity yet.")));
		return;
	}

	TArray<FMindThoughtView> ThoughtHistory;
	CachedService->GetThoughtHistory(ThoughtHistory);

	TArray<FString> JournalLines;
	TArray<FString> RecentLines;
	TArray<FCollapsedThoughtLine> JournalCollapsed;
	TArray<FCollapsedThoughtLine> RecentCollapsed;
	TMap<FString, int32> JournalCollapseIndices;
	TMap<FString, int32> RecentCollapseIndices;
	TSet<FName> SeenImportantRecordIds;

	for (int32 Index = ThoughtHistory.Num() - 1; Index >= 0; --Index)
	{
		const FMindThoughtView& Thought = ThoughtHistory[Index];
		if (Thought.Text.IsEmpty())
		{
			continue;
		}

		if (ShouldAppearInImportant(Thought))
		{
			if (!Thought.RecordId.IsNone())
			{
				// Durable records are represented by their latest state/message only.
				if (!SeenImportantRecordIds.Contains(Thought.RecordId) && JournalCollapsed.Num() < MaxImportantThoughts)
				{
					SeenImportantRecordIds.Add(Thought.RecordId);

					FCollapsedThoughtLine Entry;
					Entry.Thought = Thought;
					Entry.Count = 1;
					Entry.bResolved = (Thought.RecordState == EMindRecordState::Resolved);
					JournalCollapsed.Add(MoveTemp(Entry));
				}
			}
			else
			{
				const FString CollapseKey = BuildThoughtCollapseKey(Thought);
				if (const int32* ExistingIndex = JournalCollapseIndices.Find(CollapseKey))
				{
					++JournalCollapsed[*ExistingIndex].Count;
				}
				else if (JournalCollapsed.Num() < MaxImportantThoughts)
				{
					FCollapsedThoughtLine Entry;
					Entry.Thought = Thought;
					Entry.Count = 1;
					Entry.bResolved = false;
					const int32 AddedIndex = JournalCollapsed.Add(MoveTemp(Entry));
					JournalCollapseIndices.Add(CollapseKey, AddedIndex);
				}
			}
		}

		if (ShouldAppearInRecent(Thought))
		{
			const FString CollapseKey = BuildThoughtCollapseKey(Thought);
			if (const int32* ExistingIndex = RecentCollapseIndices.Find(CollapseKey))
			{
				++RecentCollapsed[*ExistingIndex].Count;
			}
			else if (RecentCollapsed.Num() < MaxRecentThoughts)
			{
				FCollapsedThoughtLine Entry;
				Entry.Thought = Thought;
				Entry.Count = 1;
				const int32 AddedIndex = RecentCollapsed.Add(MoveTemp(Entry));
				RecentCollapseIndices.Add(CollapseKey, AddedIndex);
			}
		}

		if (JournalCollapsed.Num() >= MaxImportantThoughts && RecentCollapsed.Num() >= MaxRecentThoughts)
		{
			break;
		}
	}

	for (const FCollapsedThoughtLine& Entry : JournalCollapsed)
	{
		if (!Entry.bResolved)
		{
			JournalLines.Add(FormatCollapsedLine(FormatImportantLine(Entry), Entry.Count));
		}
	}

	for (const FCollapsedThoughtLine& Entry : JournalCollapsed)
	{
		if (Entry.bResolved)
		{
			JournalLines.Add(FormatCollapsedLine(FormatImportantLine(Entry), Entry.Count));
		}
	}

	for (const FCollapsedThoughtLine& Entry : RecentCollapsed)
	{
		RecentLines.Add(FormatCollapsedLine(FormatRecentLine(Entry.Thought), Entry.Count));
	}

	if (JournalLines.Num() == 0)
	{
		JournalLines.Add(TEXT("No important records yet."));
	}

	if (RecentLines.Num() == 0)
	{
		RecentLines.Add(TEXT("No recent activity yet."));
	}

	UpdateJournalText(FText::FromString(FString::Join(JournalLines, TEXT("\n"))));
	UpdateRecentText(FText::FromString(FString::Join(RecentLines, TEXT("\n"))));
}

void UMindJournalViewModel::HandleThoughtAdded(const FMindThoughtView& /*Thought*/)
{
	RefreshFromService();
}

void UMindJournalViewModel::HandleMindFeedChanged()
{
	RefreshFromService();
}
