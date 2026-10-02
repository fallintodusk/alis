// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

enum class EProjectTextureCacheRequest : uint8
{
	Generate,
	Pending,
	Ready,
	Rejected
};

enum class EProjectTextureCacheState : uint8
{
	Generating,
	Ready,
	Failed
};

struct FProjectTextureCacheEntry
{
	int64 ResidentBytes = 0;
	int32 Pins = 0;
	int32 GenerationCount = 0;
	uint64 LastUse = 0;
	EProjectTextureCacheState State = EProjectTextureCacheState::Generating;
};

class FProjectTextureRuntimeCache final
{
public:
	explicit FProjectTextureRuntimeCache(int64 InBudgetBytes = 0)
		: BudgetBytes(InBudgetBytes)
	{
	}

	void SetBudgetBytes(int64 InBudgetBytes) { BudgetBytes = InBudgetBytes; }
	EProjectTextureCacheRequest Request(
		const FString& Identity,
		int64 Bytes,
		bool bPin,
		FString& OutError);
	bool Complete(const FString& Identity, FString& OutError);
	void Fail(const FString& Identity);
	bool Release(const FString& Identity, FString& OutError);
	int32 EvictUnpinned(int64 BytesToFree);
	void Reset();

	const FProjectTextureCacheEntry* Find(const FString& Identity) const
	{
		return Entries.Find(Identity);
	}

	int64 GetResidentBytes() const { return ResidentBytes; }
	int64 GetPeakResidentBytes() const { return PeakResidentBytes; }

private:
	TMap<FString, FProjectTextureCacheEntry> Entries;
	int64 BudgetBytes = 0;
	int64 ResidentBytes = 0;
	int64 PeakResidentBytes = 0;
	uint64 UseCounter = 0;
};
