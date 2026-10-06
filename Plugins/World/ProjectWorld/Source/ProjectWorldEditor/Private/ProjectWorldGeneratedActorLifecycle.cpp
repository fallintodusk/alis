// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldGeneratedGeometry.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRuntimeRealization.h"

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

		bool HasCurrentCellTag(
			const AActor& Actor,
			const FProjectWorldCanonicalBundle& Bundle,
			const FString& Prefix)
		{
			if (!HasTagValue(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId))
			{
				return false;
			}
			return Bundle.Cells.ContainsByPredicate([&Actor, &Prefix](const FProjectWorldCanonicalCell& Cell)
			{
				return HasTagValue(Actor, Prefix, Cell.CellId);
			});
		}

		bool IsCurrentGeneratedActor(
			const AActor& Actor,
			const FProjectWorldCanonicalBundle& Bundle,
			const FString& RuntimeProfileId)
		{
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
			if (ProjectWorldRuntimeRealization::IsRuntimeRoleActor(Actor))
			{
				return ProjectWorldRuntimeRealization::IsCurrentRuntimeActorForApply(
					Actor, Bundle, !RuntimeProfileId.IsEmpty());
			}
			if (Actor.Tags.ContainsByPredicate([](const FName& Tag)
				{
					return Tag.ToString().StartsWith(TEXT("ProjectWorld.AuthoredOverlay="));
				}))
			{
				return HasTagValue(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId);
			}
			const IProjectWorldLayerProducer* Owner = nullptr;
			FString OwnerError;
			if (!ProjectWorldLayerProducerRegistry::FindActorOwner(Actor, Owner, OwnerError))
			{
				return false;
			}
			if (Owner != nullptr)
			{
				const FString& Prefix = Owner->GetDeclaration().OwnedActors.CellTagPrefix;
				return Prefix.IsEmpty()
					? HasTagValue(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId)
					: HasCurrentCellTag(Actor, Bundle, Prefix);
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

	bool IsCoreGeneratedActor(const AActor& Actor)
	{
		return Actor.IsA<AGeoReferencingSystem>() ||
			ProjectWorldRuntimeRealization::IsRuntimeRoleActor(Actor) ||
			Actor.Tags.ContainsByPredicate([](const FName& Tag)
			{
				const FString Value = Tag.ToString();
				return Value.StartsWith(TEXT("ProjectWorld.PresentationRole=")) ||
					Value.StartsWith(TEXT("ProjectWorld.AuthoredOverlay="));
			});
	}

	bool RemoveOwnedActors(
		UWorld* World,
		FProjectWorldRealizationResult& OutResult)
	{
		TArray<AActor*> OwnedActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(GeneratedTag))
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
		FProjectWorldRealizationResult& OutResult,
		FString* OutError)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(GeneratedTag))
			{
				const IProjectWorldLayerProducer* Owner = nullptr;
				FString OwnerError;
				if (!ProjectWorldLayerProducerRegistry::FindActorOwner(**It, Owner, OwnerError))
				{
					if (OutError != nullptr) *OutError = OwnerError;
					return false;
				}
			}
		}
		TArray<AActor*> StaleActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Tags.Contains(GeneratedTag))
			{
				++OutResult.PreservedActorCount;
				continue;
			}
			if (!IsCurrentGeneratedActor(**It, Bundle, RuntimeProfileId))
			{
				StaleActors.Add(*It);
			}
		}
		return DestroyActors(World, StaleActors, OutResult);
	}
}
