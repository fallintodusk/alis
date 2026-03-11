// Copyright ALIS. All Rights Reserved.

#include "MVVM/MindThoughtViewModel.h"
#include "Interfaces/IMindService.h"
#include "ProjectServiceLocator.h"

DEFINE_LOG_CATEGORY_STATIC(LogMindThoughtVM, Log, All);

namespace
{
bool ShouldAppearInToast(const FMindThoughtView& Thought)
{
	return Thought.Channel == EMindThoughtChannel::Toast
		|| Thought.Channel == EMindThoughtChannel::ToastAndJournal;
}

FText BuildThoughtMetaText(const FMindThoughtView& Thought)
{
	FString SourceLabel;
	switch (Thought.SourceType)
	{
	case EMindThoughtSourceType::Dialogue:
		SourceLabel = TEXT("Dialogue");
		break;
	case EMindThoughtSourceType::Vitals:
		SourceLabel = TEXT("Vitals");
		break;
	case EMindThoughtSourceType::Inventory:
		SourceLabel = TEXT("Inventory");
		break;
	case EMindThoughtSourceType::Scan:
		SourceLabel = TEXT("Scan");
		break;
	case EMindThoughtSourceType::Quest:
		SourceLabel = TEXT("Quest");
		break;
	case EMindThoughtSourceType::Beacon:
		SourceLabel = TEXT("POI");
		break;
	case EMindThoughtSourceType::System:
		SourceLabel = TEXT("System");
		break;
	default:
		SourceLabel = TEXT("Mind");
		break;
	}

	if (Thought.Priority >= 100)
	{
		SourceLabel = FString::Printf(TEXT("%s | Critical"), *SourceLabel);
	}

	return FText::FromString(SourceLabel);
}
}

void UMindThoughtViewModel::Initialize(UObject* Context)
{
	Super::Initialize(Context);
	SubscribeToService();
}

void UMindThoughtViewModel::Shutdown()
{
	StopServiceRetry();
	UnsubscribeFromService();
	Super::Shutdown();
}

bool UMindThoughtViewModel::GetBoolProperty(FName PropertyName) const
{
	static const FName HasThoughtProp(TEXT("bHasThought"));
	if (PropertyName == HasThoughtProp)
	{
		return GetbHasThought();
	}

	return Super::GetBoolProperty(PropertyName);
}

void UMindThoughtViewModel::ClearCurrentThought()
{
	UpdatebHasThought(false);
	UpdateThoughtText(FText::GetEmpty());
	UpdateThoughtMetaText(FText::GetEmpty());
}

void UMindThoughtViewModel::SubscribeToService()
{
	CachedService = FProjectServiceLocator::Resolve<IMindService>();
	if (!CachedService.IsValid())
	{
		if (!ServiceRetryHandle.IsValid())
		{
			ServiceRetryHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &UMindThoughtViewModel::RetrySubscribeToService),
				0.5f);
		}

		UE_LOG(LogMindThoughtVM, Verbose, TEXT("SubscribeToService: IMindService not available, waiting"));
		return;
	}

	ThoughtAddedHandle = CachedService->OnThoughtAdded().AddUObject(
		this, &UMindThoughtViewModel::HandleThoughtAdded);

	TArray<FMindThoughtView> ThoughtHistory;
	CachedService->GetThoughtHistory(ThoughtHistory);

	for (int32 Index = ThoughtHistory.Num() - 1; Index >= 0; --Index)
	{
		if (ShouldAppearInToast(ThoughtHistory[Index]))
		{
			ApplyThought(ThoughtHistory[Index], true);
			return;
		}
	}

	ClearCurrentThought();
}

void UMindThoughtViewModel::UnsubscribeFromService()
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

	CachedService.Reset();
}

bool UMindThoughtViewModel::RetrySubscribeToService(float /*DeltaTime*/)
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

void UMindThoughtViewModel::StopServiceRetry()
{
	if (ServiceRetryHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ServiceRetryHandle);
		ServiceRetryHandle.Reset();
	}
}

void UMindThoughtViewModel::ApplyThought(const FMindThoughtView& Thought, bool bRespectAge)
{
	float EffectiveTtlSec = Thought.TimeToLiveSec;

	if (bRespectAge)
	{
		double AgeSec = 0.0;

		if (Thought.CreatedAtUtc != FDateTime())
		{
			const FTimespan AgeSpan = FDateTime::UtcNow() - Thought.CreatedAtUtc;
			AgeSec = FMath::Max(0.0, AgeSpan.GetTotalSeconds());
		}
		else
		{
			const double NowSec = FPlatformTime::Seconds();
			AgeSec = FMath::Max(0.0, NowSec - static_cast<double>(Thought.CreatedAtSec));
		}

		EffectiveTtlSec = static_cast<float>(FMath::Max(0.0, static_cast<double>(Thought.TimeToLiveSec) - AgeSec));
	}

	if (EffectiveTtlSec <= 0.0f || Thought.Text.IsEmpty())
	{
		ClearCurrentThought();
		return;
	}

	UpdateThoughtText(Thought.Text);
	UpdateThoughtMetaText(BuildThoughtMetaText(Thought));
	UpdateThoughtTimeToLiveSec(EffectiveTtlSec);
	UpdatebHasThought(true);
}

void UMindThoughtViewModel::HandleThoughtAdded(const FMindThoughtView& Thought)
{
	if (!ShouldAppearInToast(Thought))
	{
		return;
	}

	ApplyThought(Thought, false);
}
