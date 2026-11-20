#include "ProjectTimeDemoViewModel.h"
#include "ProjectTimeSubsystem.h"
#include "Engine/World.h"

void UProjectTimeDemoViewModel::Initialize(UObject* Context)
{
	Super::Initialize(Context);
	Time = Context && Context->GetWorld() ? Context->GetWorld()->GetSubsystem<UProjectTimeSubsystem>() : nullptr;
	Refresh();
}

bool UProjectTimeDemoViewModel::CanControl() const
{
	return Time.IsValid() && Time->CanControlTime();
}

UWorld* UProjectTimeDemoViewModel::GetBoundWorld() const
{
	return Time.IsValid() ? Time->GetWorld() : nullptr;
}

void UProjectTimeDemoViewModel::Refresh()
{
	FProjectTimeSnapshot Value;
	if (Time.IsValid() && Time->TryReadTime(Value))
	{
		RateValue = Value.Rate;
		DateText = FText::FromString(FString::Printf(TEXT("%s  |  %s  %.1fx"),
			*Time->GetLocalTimeText().ToString(), Value.bDemo ? TEXT("Demo") : TEXT("Live"), Value.Rate));
	}
	else DateText = NSLOCTEXT("ProjectTime", "Unavailable", "Time unavailable");
	NotifyPropertyChanged(TEXT("Time"));
}

void UProjectTimeDemoViewModel::BeginScrub()
{
	FProjectTimeSnapshot Value;
	bScrubAnchor = CanControl() && Time->TryReadTime(Value);
	if (bScrubAnchor) ScrubAnchor = Value.Utc - FTimespan::FromHours(OffsetHours);
}

void UProjectTimeDemoViewModel::SeekHours(float Hours)
{
	if (!FMath::IsFinite(Hours) || Hours < -12 || Hours > 12 || !CanControl()) return;
	if (!bScrubAnchor) BeginScrub();
	const int64 Delta = FTimespan::FromHours(Hours).GetTicks();
	if (!bScrubAnchor || ScrubAnchor.GetTicks() + Delta < 0 ||
		ScrubAnchor.GetTicks() + Delta > FDateTime::MaxValue().GetTicks()) return;
	if (Time->SeekUtc(FDateTime(ScrubAnchor.GetTicks() + Delta))) OffsetHours = Hours;
	Refresh();
}

void UProjectTimeDemoViewModel::ChangeRate(float Rate)
{
	if (CanControl()) Time->SetRate(Rate);
	Refresh();
}

void UProjectTimeDemoViewModel::Reset()
{
	if (CanControl() && Time->ResetToPcTime())
	{
		OffsetHours = 0;
		bScrubAnchor = false;
	}
	Refresh();
}

void UProjectTimeDemoViewModel::Shutdown()
{
	CloseRequested.Clear();
	Time.Reset();
	Super::Shutdown();
}
