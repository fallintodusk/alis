// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureRuntimeCache.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTextureRuntimeCacheReuseTest,
	"Project.Texture.Runtime.CacheReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureRuntimeCacheReuseTest::RunTest(const FString& Parameters)
{
	FProjectTextureRuntimeCache Cache(1024);
	FString Error;
	TestEqual(TEXT("The first request owns generation."),
		Cache.Request(TEXT("structure-a"), 256, true, Error),
		EProjectTextureCacheRequest::Generate);
	TestEqual(TEXT("A second request cannot become a duplicate writer."),
		Cache.Request(TEXT("structure-a"), 256, true, Error),
		EProjectTextureCacheRequest::Pending);
	TestTrue(TEXT("The generated entry becomes ready."), Cache.Complete(TEXT("structure-a"), Error));
	for (int32 Index = 0; Index < 100; ++Index)
	{
		TestEqual(TEXT("Equivalent consumers reuse the ready entry."),
			Cache.Request(TEXT("structure-a"), 256, false, Error),
			EProjectTextureCacheRequest::Ready);
	}
	TestEqual(TEXT("Reuse does not multiply resident bytes."), Cache.GetResidentBytes(), int64(256));
	TestTrue(TEXT("The exported pin releases exactly once."), Cache.Release(TEXT("structure-a"), Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTextureRuntimeBudgetAndPinsTest,
	"Project.Texture.Runtime.BudgetAndPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureRuntimeBudgetAndPinsTest::RunTest(const FString& Parameters)
{
	FProjectTextureRuntimeCache Cache(100);
	FString Error;
	TestEqual(TEXT("Pinned A is admitted."), Cache.Request(TEXT("a"), 60, true, Error),
		EProjectTextureCacheRequest::Generate);
	TestTrue(TEXT("Pinned A becomes ready."), Cache.Complete(TEXT("a"), Error));
	TestEqual(TEXT("B is refused while A protects the budget."),
		Cache.Request(TEXT("b"), 60, false, Error), EProjectTextureCacheRequest::Rejected);
	TestNotNull(TEXT("Budget refusal leaves A present."), Cache.Find(TEXT("a")));
	TestEqual(TEXT("Budget refusal leaves A resident."), Cache.GetResidentBytes(), int64(60));
	TestTrue(TEXT("A can be unpinned."), Cache.Release(TEXT("a"), Error));
	TestEqual(TEXT("B is admitted after unpinned LRU eviction."),
		Cache.Request(TEXT("b"), 60, false, Error), EProjectTextureCacheRequest::Generate);
	TestNull(TEXT("A was the evicted entry."), Cache.Find(TEXT("a")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTextureRuntimeFailureLifecycleTest,
	"Project.Texture.Runtime.FailureLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureRuntimeFailureLifecycleTest::RunTest(const FString& Parameters)
{
	FProjectTextureRuntimeCache Cache(1024);
	FString Error;
	TestEqual(TEXT("A is admitted."), Cache.Request(TEXT("a"), 128, false, Error),
		EProjectTextureCacheRequest::Generate);
	TestTrue(TEXT("A becomes ready."), Cache.Complete(TEXT("a"), Error));
	TestEqual(TEXT("B is admitted."), Cache.Request(TEXT("b"), 256, false, Error),
		EProjectTextureCacheRequest::Generate);
	Cache.Fail(TEXT("b"));
	const FProjectTextureCacheEntry* A = Cache.Find(TEXT("a"));
	const FProjectTextureCacheEntry* B = Cache.Find(TEXT("b"));
	TestTrue(TEXT("An unrelated ready entry survives failure."),
		A != nullptr && A->State == EProjectTextureCacheState::Ready);
	TestTrue(TEXT("The failed entry is observable and non-resident."),
		B != nullptr && B->State == EProjectTextureCacheState::Failed && B->ResidentBytes == 0);
	TestEqual(TEXT("Only A remains resident."), Cache.GetResidentBytes(), int64(128));
	Cache.Reset();
	TestEqual(TEXT("Shutdown releases all accounting."), Cache.GetResidentBytes(), int64(0));
	TestNull(TEXT("Shutdown clears ready entries."), Cache.Find(TEXT("a")));
	return true;
}

#endif
