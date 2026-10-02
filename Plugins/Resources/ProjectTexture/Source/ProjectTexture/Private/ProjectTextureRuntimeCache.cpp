// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureRuntimeCache.h"

EProjectTextureCacheRequest FProjectTextureRuntimeCache::Request(
	const FString& Identity,
	int64 Bytes,
	bool bPin,
	FString& OutError)
{
	OutError.Reset();
	if (Identity.IsEmpty() || Bytes <= 0 || BudgetBytes <= 0)
	{
		OutError = TEXT("Cache request identity, bytes, and budget must be positive.");
		return EProjectTextureCacheRequest::Rejected;
	}
	if (FProjectTextureCacheEntry* Existing = Entries.Find(Identity))
	{
		Existing->LastUse = ++UseCounter;
		if (Existing->State == EProjectTextureCacheState::Ready)
		{
			if (bPin)
			{
				++Existing->Pins;
			}
			return EProjectTextureCacheRequest::Ready;
		}
		if (Existing->State == EProjectTextureCacheState::Generating)
		{
			return EProjectTextureCacheRequest::Pending;
		}
		ResidentBytes -= Existing->ResidentBytes;
		Entries.Remove(Identity);
	}

	const int64 Required = ResidentBytes + Bytes - BudgetBytes;
	if (Required > 0)
	{
		EvictUnpinned(Required);
	}
	if (ResidentBytes + Bytes > BudgetBytes)
	{
		OutError = FString::Printf(
			TEXT("Cache budget refused %lld bytes with %lld/%lld resident."),
			Bytes,
			ResidentBytes,
			BudgetBytes);
		return EProjectTextureCacheRequest::Rejected;
	}

	FProjectTextureCacheEntry Entry;
	Entry.ResidentBytes = Bytes;
	Entry.Pins = bPin ? 1 : 0;
	Entry.GenerationCount = 1;
	Entry.LastUse = ++UseCounter;
	Entries.Add(Identity, Entry);
	ResidentBytes += Bytes;
	PeakResidentBytes = FMath::Max(PeakResidentBytes, ResidentBytes);
	return EProjectTextureCacheRequest::Generate;
}

bool FProjectTextureRuntimeCache::Complete(const FString& Identity, FString& OutError)
{
	OutError.Reset();
	FProjectTextureCacheEntry* Entry = Entries.Find(Identity);
	if (Entry == nullptr || Entry->State != EProjectTextureCacheState::Generating)
	{
		OutError = TEXT("Only a generating cache entry can become ready.");
		return false;
	}
	Entry->State = EProjectTextureCacheState::Ready;
	Entry->LastUse = ++UseCounter;
	return true;
}

void FProjectTextureRuntimeCache::Fail(const FString& Identity)
{
	if (FProjectTextureCacheEntry* Entry = Entries.Find(Identity))
	{
		ResidentBytes -= Entry->ResidentBytes;
		Entry->ResidentBytes = 0;
		Entry->Pins = 0;
		Entry->State = EProjectTextureCacheState::Failed;
		Entry->LastUse = ++UseCounter;
	}
}

bool FProjectTextureRuntimeCache::Release(const FString& Identity, FString& OutError)
{
	OutError.Reset();
	FProjectTextureCacheEntry* Entry = Entries.Find(Identity);
	if (Entry == nullptr || Entry->Pins <= 0)
	{
		OutError = TEXT("Cache release requires an existing pin.");
		return false;
	}
	--Entry->Pins;
	Entry->LastUse = ++UseCounter;
	return true;
}

int32 FProjectTextureRuntimeCache::EvictUnpinned(int64 BytesToFree)
{
	int32 Evicted = 0;
	while (BytesToFree > 0)
	{
		FString Candidate;
		uint64 Oldest = MAX_uint64;
		for (const TPair<FString, FProjectTextureCacheEntry>& Pair : Entries)
		{
			if (Pair.Value.Pins == 0 && Pair.Value.State == EProjectTextureCacheState::Ready &&
				Pair.Value.ResidentBytes > 0 && Pair.Value.LastUse < Oldest)
			{
				Candidate = Pair.Key;
				Oldest = Pair.Value.LastUse;
			}
		}
		if (Candidate.IsEmpty())
		{
			break;
		}
		const int64 Freed = Entries[Candidate].ResidentBytes;
		ResidentBytes -= Freed;
		BytesToFree -= Freed;
		Entries.Remove(Candidate);
		++Evicted;
	}
	return Evicted;
}

void FProjectTextureRuntimeCache::Reset()
{
	Entries.Reset();
	ResidentBytes = 0;
	PeakResidentBytes = 0;
	UseCounter = 0;
}
