// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ProjectServiceLocator.h"
#include "Interfaces/IMindThoughtSource.h"
#include "Services/MindServiceImpl.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FMindThoughtCandidate MakeCandidate(
		const TCHAR* ThoughtId,
		const TCHAR* Text,
		int32 Priority,
		double CooldownSec,
		double DedupeWindowSec,
		const TCHAR* DedupeKey = TEXT(""))
	{
		FMindThoughtCandidate Candidate;
		Candidate.ThoughtId = FName(ThoughtId);
		Candidate.ThoughtText = FText::FromString(Text);
		Candidate.Priority = Priority;
		Candidate.CooldownSec = CooldownSec;
		Candidate.DedupeWindowSec = DedupeWindowSec;
		Candidate.DedupeKey = DedupeKey[0] ? FName(DedupeKey) : NAME_None;
		Candidate.Channel = EMindThoughtChannel::Toast;
		Candidate.SourceType = EMindThoughtSourceType::Vitals;
		return Candidate;
	}

	class FMockVitalsThoughtSource : public IMindVitalsThoughtSource
	{
	public:
		virtual void CollectThoughts(
			const FMindThoughtSourceContext& Context,
			TArray<FMindThoughtCandidate>& OutThoughts) const override
		{
			(void)Context;
			if (Collector)
			{
				Collector(OutThoughts);
			}
		}

		virtual FOnMindThoughtSourceChanged& OnThoughtSourceChanged() override
		{
			return ChangedDelegate;
		}

		TFunction<void(TArray<FMindThoughtCandidate>&)> Collector;
		FOnMindThoughtSourceChanged ChangedDelegate;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindAggregatorPriorityPolicyTest,
	"ProjectIntegrationTests.Unit.Mind.Aggregator.Priority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindAggregatorPriorityPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IMindVitalsThoughtSource> OriginalSource = FProjectServiceLocator::Resolve<IMindVitalsThoughtSource>();
	TSharedRef<FMockVitalsThoughtSource> MockSource = MakeShared<FMockVitalsThoughtSource>();
	MockSource->Collector = [](TArray<FMindThoughtCandidate>& Out)
	{
		Out.Add(MakeCandidate(TEXT("Mind.Test.Low"), TEXT("Low"), 10, 0.0, 0.0));
		Out.Add(MakeCandidate(TEXT("Mind.Test.High"), TEXT("High"), 100, 0.0, 0.0));
	};
	FProjectServiceLocator::Register<IMindVitalsThoughtSource>(StaticCastSharedRef<IMindVitalsThoughtSource>(MockSource));

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
	RuntimeMindService->Start();
	RuntimeMindService->PumpProvidersForTests();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Aggregator should emit exactly one thought per provider pass"), Thoughts.Num(), 1);
	if (Thoughts.Num() == 1)
	{
		TestEqual(TEXT("Highest-priority thought should win"), Thoughts[0].ThoughtId, FName(TEXT("Mind.Test.High")));
	}

	RuntimeMindService->Stop();

	FProjectServiceLocator::Unregister<IMindVitalsThoughtSource>();
	if (OriginalSource.IsValid())
	{
		FProjectServiceLocator::Register<IMindVitalsThoughtSource>(OriginalSource.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindAggregatorCooldownPolicyTest,
	"ProjectIntegrationTests.Unit.Mind.Aggregator.Cooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindAggregatorCooldownPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IMindVitalsThoughtSource> OriginalSource = FProjectServiceLocator::Resolve<IMindVitalsThoughtSource>();
	TSharedRef<FMockVitalsThoughtSource> MockSource = MakeShared<FMockVitalsThoughtSource>();
	MockSource->Collector = [](TArray<FMindThoughtCandidate>& Out)
	{
		Out.Add(MakeCandidate(TEXT("Mind.Test.Cooldown"), TEXT("Cooldown"), 50, 120.0, 0.0));
	};
	FProjectServiceLocator::Register<IMindVitalsThoughtSource>(StaticCastSharedRef<IMindVitalsThoughtSource>(MockSource));

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
	RuntimeMindService->Start();
	RuntimeMindService->PumpProvidersForTests();
	RuntimeMindService->PumpProvidersForTests();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Per-thought cooldown should suppress repeated emissions"), Thoughts.Num(), 1);

	RuntimeMindService->Stop();

	FProjectServiceLocator::Unregister<IMindVitalsThoughtSource>();
	if (OriginalSource.IsValid())
	{
		FProjectServiceLocator::Register<IMindVitalsThoughtSource>(OriginalSource.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindAggregatorDedupePolicyTest,
	"ProjectIntegrationTests.Unit.Mind.Aggregator.Dedupe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindAggregatorDedupePolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IMindVitalsThoughtSource> OriginalSource = FProjectServiceLocator::Resolve<IMindVitalsThoughtSource>();
	TSharedRef<FMockVitalsThoughtSource> MockSource = MakeShared<FMockVitalsThoughtSource>();

	int32 PumpCount = 0;
	MockSource->Collector = [&PumpCount](TArray<FMindThoughtCandidate>& Out)
	{
		if (PumpCount == 0)
		{
			Out.Add(MakeCandidate(TEXT("Mind.Test.Dedupe.First"), TEXT("First"), 90, 0.0, 120.0, TEXT("Mind.Test.Dedupe.Shared")));
		}
		else
		{
			Out.Add(MakeCandidate(TEXT("Mind.Test.Dedupe.Second"), TEXT("Second"), 80, 0.0, 120.0, TEXT("Mind.Test.Dedupe.Shared")));
		}
		++PumpCount;
	};
	FProjectServiceLocator::Register<IMindVitalsThoughtSource>(StaticCastSharedRef<IMindVitalsThoughtSource>(MockSource));

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
	RuntimeMindService->Start();
	RuntimeMindService->PumpProvidersForTests();
	RuntimeMindService->PumpProvidersForTests();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Shared dedupe key should suppress second candidate"), Thoughts.Num(), 1);
	if (Thoughts.Num() == 1)
	{
		TestEqual(TEXT("First thought should remain in feed"), Thoughts[0].ThoughtId, FName(TEXT("Mind.Test.Dedupe.First")));
	}

	RuntimeMindService->Stop();

	FProjectServiceLocator::Unregister<IMindVitalsThoughtSource>();
	if (OriginalSource.IsValid())
	{
		FProjectServiceLocator::Register<IMindVitalsThoughtSource>(OriginalSource.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindAggregatorGlobalRatePolicyTest,
	"ProjectIntegrationTests.Unit.Mind.Aggregator.GlobalRateLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindAggregatorGlobalRatePolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IMindVitalsThoughtSource> OriginalSource = FProjectServiceLocator::Resolve<IMindVitalsThoughtSource>();
	TSharedRef<FMockVitalsThoughtSource> MockSource = MakeShared<FMockVitalsThoughtSource>();

	int32 PumpCount = 0;
	MockSource->Collector = [&PumpCount](TArray<FMindThoughtCandidate>& Out)
	{
		const FString ThoughtId = FString::Printf(TEXT("Mind.Test.GlobalRate.%d"), PumpCount);
		const FString Text = FString::Printf(TEXT("Thought %d"), PumpCount);
		Out.Add(MakeCandidate(*ThoughtId, *Text, 70, 0.0, 0.0));
		++PumpCount;
	};
	FProjectServiceLocator::Register<IMindVitalsThoughtSource>(StaticCastSharedRef<IMindVitalsThoughtSource>(MockSource));

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->SetGlobalEmitCooldownForTests(60.0);
	RuntimeMindService->Start();
	RuntimeMindService->PumpProvidersForTests();
	RuntimeMindService->PumpProvidersForTests();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Global rate-limit should suppress immediate second thought"), Thoughts.Num(), 1);

	RuntimeMindService->Stop();

	FProjectServiceLocator::Unregister<IMindVitalsThoughtSource>();
	if (OriginalSource.IsValid())
	{
		FProjectServiceLocator::Register<IMindVitalsThoughtSource>(OriginalSource.ToSharedRef());
	}

	return true;
}

#endif
