// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldGeneratedGeometry.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLandscapeRealization.h"
#include "ProjectWorldRealizationService.h"

#include "EngineUtils.h"
#include "GeoReferencingSystem.h"

namespace ProjectWorldGeneratedGeometry
{
	namespace
	{
		bool HasTagValue(const AActor& Actor, const FString& Prefix, const FString& Value)
		{
			return Actor.Tags.Contains(FName(*(Prefix + Value)));
		}

		bool IsCurrentGeneratedActor(
			const AActor& Actor,
			const FProjectWorldCanonicalBundle& Bundle,
			const FString& RuntimeProfileId,
			bool bPreserveLandscape)
		{
			if (bPreserveLandscape && ProjectWorldLandscapeRealization::IsGeneratedLandscape(&Actor))
			{
				return true;
			}
			if (Actor.IsA<AGeoReferencingSystem>())
			{
				return HasTagValue(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId) ||
					Actor.GetName().EndsWith(Bundle.GridId);
			}
			if (Actor.Tags.ContainsByPredicate([](const FName& Tag)
				{
					return Tag.ToString().StartsWith(TEXT("ProjectWorld.PresentationRole="));
				}))
			{
				return HasTagValue(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId);
			}
			if (Actor.Tags.ContainsByPredicate([](const FName& Tag)
				{
					return Tag.ToString().StartsWith(TEXT("ProjectWorld.RuntimeRole="));
				}))
			{
				return !RuntimeProfileId.IsEmpty() &&
					HasTagValue(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId) &&
					HasTagValue(Actor, TEXT("ProjectWorld.Runtime="), RuntimeProfileId);
			}
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				if (HasTagValue(Actor, TEXT("ProjectWorld.Cell="), Cell.CellId))
				{
					return true;
				}
			}
			return false;
		}

		bool DestroyActors(
			UWorld* World,
			const TArray<AActor*>& Actors,
			FProjectWorldRealizationResult& OutResult)
		{
			for (AActor* Actor : Actors)
			{
				if (!World->EditorDestroyActor(Actor, true))
				{
					return false;
				}
				++OutResult.RemovedActorCount;
			}
			return true;
		}
	}

	bool RemoveOwnedActors(
		UWorld* World,
		bool bPreserveLandscape,
		FProjectWorldRealizationResult& OutResult)
	{
		TArray<AActor*> OwnedActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(GeneratedTag) &&
				!(bPreserveLandscape && ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It)))
			{
				OwnedActors.Add(*It);
			}
			else
			{
				++OutResult.PreservedActorCount;
			}
		}
		return DestroyActors(World, OwnedActors, OutResult);
	}

	bool RemoveStaleOwnedActorsForApply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FString& RuntimeProfileId,
		bool bPreserveLandscape,
		FProjectWorldRealizationResult& OutResult)
	{
		TArray<AActor*> StaleActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Tags.Contains(GeneratedTag))
			{
				++OutResult.PreservedActorCount;
				continue;
			}
			if (!IsCurrentGeneratedActor(**It, Bundle, RuntimeProfileId, bPreserveLandscape))
			{
				StaleActors.Add(*It);
			}
		}
		return DestroyActors(World, StaleActors, OutResult);
	}
}
