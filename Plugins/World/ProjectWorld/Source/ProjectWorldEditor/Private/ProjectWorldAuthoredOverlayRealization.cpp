// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAuthoredOverlayRealization.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationService.h"

#include "EngineUtils.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldAuthoredOverlayRealization, Log, All);

namespace ProjectWorldAuthoredOverlayRealization
{
	namespace
	{
		FName ValueTag(const TCHAR* Prefix, const FString& Value)
		{
			return FName(*(FString(Prefix) + Value));
		}

		FString OverlayIdFromActor(const AActor& Actor)
		{
			const FString Prefix = TEXT("ProjectWorld.AuthoredOverlay=");
			for (const FName& Tag : Actor.Tags)
			{
				const FString Value = Tag.ToString();
				if (Value.StartsWith(Prefix))
				{
					return Value.RightChop(Prefix.Len());
				}
			}
			return FString();
		}

		bool IsCurrentAnchorActor(
			const ALevelInstance& Actor,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldAuthoredOverlaySet& Set,
			const FProjectWorldAuthoredAnchorEvidence& Evidence)
		{
			const FTransform ExpectedTransform(Evidence.WorldRotation, Evidence.WorldLocation);
			return Actor.GetActorGuid() == ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|authored-overlay|") + Evidence.OverlayId) &&
				Actor.IsPackageExternal() &&
				Actor.GetActorTransform().Equals(ExpectedTransform, 0.01) &&
				Actor.GetWorldAssetPackage() == Evidence.AuthoredPackage &&
				Actor.Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag) &&
				Actor.Tags.Contains(ValueTag(TEXT("ProjectWorld.Grid="), Bundle.GridId)) &&
				Actor.Tags.Contains(ValueTag(TEXT("ProjectWorld.AuthoredOverlay="), Evidence.OverlayId)) &&
				Actor.Tags.Contains(ValueTag(TEXT("ProjectWorld.AuthoredPackage="), Evidence.AuthoredPackage)) &&
				Actor.Tags.Contains(ValueTag(TEXT("ProjectWorld.AuthoredOverlaySet="), Set.SetHash)) &&
				Actor.Tags.Contains(ValueTag(
					TEXT("ProjectWorld.Resolver="), FString::FromInt(Set.ResolverVersion))) &&
				Actor.GetIsSpatiallyLoaded() && !Actor.bEnableAutoLODGeneration;
		}

		bool SaveExternalActor(AActor* Actor)
		{
			UPackage* Package = Actor != nullptr ? Actor->GetExternalPackage() : nullptr;
			if (Package == nullptr)
			{
				return false;
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Arguments;
			Arguments.SaveFlags = SAVE_NoError;
			return UPackage::SavePackage(Package, nullptr, *Filename, Arguments);
		}
	}

	bool Resolve(
		const FProjectWorldCanonicalBundle& Bundle,
		const FString& ProfilePath,
		FProjectWorldAuthoredOverlaySet& OutSet,
		FProjectWorldRealizationResult& OutResult,
		FString& OutErrorCode,
		FString& OutError)
	{
		if (!ProjectWorldAuthoredOverlay::Load(
			ProfilePath, OutSet, OutErrorCode, OutError))
		{
			return false;
		}

		OutResult.AuthoredOverlaySetId = OutSet.OverlaySetId;
		OutResult.AuthoredOverlaySetHash = OutSet.SetHash;
		for (const FProjectWorldAuthoredOverlay& Overlay : OutSet.Overlays)
		{
			FProjectWorldAnchorResolution Resolution;
			if (!ProjectWorldAuthoredOverlay::Resolve(
				Bundle, OutSet, Overlay, Resolution, OutError))
			{
				++OutResult.AuthoredAnchorRefusedCount;
				OutErrorCode = TEXT("authored-overlay-resolution");
				return false;
			}
			if (Resolution.bPlaces && !FPackageName::DoesPackageExist(Overlay.AuthoredPackage))
			{
				++OutResult.AuthoredAnchorRefusedCount;
				OutErrorCode = TEXT("authored-overlay-package");
				OutError = FString::Printf(
					TEXT("Authored package does not exist: %s"), *Overlay.AuthoredPackage);
				return false;
			}

			FProjectWorldAuthoredAnchorEvidence& Evidence = OutResult.AuthoredAnchors.AddDefaulted_GetRef();
			Evidence.OverlayId = Overlay.OverlayId;
			Evidence.AuthoredPackage = Overlay.AuthoredPackage;
			Evidence.WorldLocation = Resolution.WorldLocation;
			Evidence.WorldRotation = Resolution.WorldRotation;
			Evidence.DriftMeters = Resolution.DriftMeters;
			Evidence.HorizontalTotalErrorMeters = Resolution.HorizontalTotalErrorMeters;
			Evidence.VerticalTotalErrorMeters = Resolution.VerticalTotalErrorMeters;
			Evidence.bSurfaceSnapped = Resolution.bSurfaceSnapped;
			Evidence.bPlaces = Resolution.bPlaces;
			++OutResult.AuthoredAnchorResolvedCount;
			OutResult.AuthoredAnchorMaximumDriftMeters = FMath::Max(
				OutResult.AuthoredAnchorMaximumDriftMeters, Resolution.DriftMeters);
			if (Resolution.bPlaces)
			{
				++OutResult.AuthoredAnchorPlacedCount;
			}
			else
			{
				++OutResult.AuthoredMaskCount;
			}
		}
		OutErrorCode.Reset();
		return true;
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldAuthoredOverlaySet& Set,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		TMap<FString, ALevelInstance*> Existing;
		TArray<AActor*> Stale;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FString OverlayId = OverlayIdFromActor(**It);
			if (OverlayId.IsEmpty() ||
				!It->Tags.Contains(ValueTag(TEXT("ProjectWorld.Grid="), Bundle.GridId)))
			{
				continue;
			}
			ALevelInstance* LevelInstance = Cast<ALevelInstance>(*It);
			if (LevelInstance == nullptr || Existing.Contains(OverlayId))
			{
				OutError = FString::Printf(
					TEXT("Generated authored-overlay identity is invalid or duplicated: %s"), *OverlayId);
				return false;
			}
			Existing.Add(OverlayId, LevelInstance);
		}

		TSet<FString> PlacedIds;
		for (const FProjectWorldAuthoredAnchorEvidence& Evidence : OutResult.AuthoredAnchors)
		{
			if (!Evidence.bPlaces)
			{
				continue;
			}
			PlacedIds.Add(Evidence.OverlayId);
			ALevelInstance* Actor = Existing.FindRef(Evidence.OverlayId);
			if (Actor == nullptr)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = FName(*FString::Printf(
					TEXT("ProjectWorld_AuthoredOverlay_%s"), *Evidence.OverlayId));
				SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				SpawnParameters.OverrideActorGuid = ProjectWorldGeneratedGeometry::StableGuid(
					Bundle.GridId + TEXT("|authored-overlay|") + Evidence.OverlayId);
				Actor = World->SpawnActor<ALevelInstance>(
					ALevelInstance::StaticClass(),
					FTransform(Evidence.WorldRotation, Evidence.WorldLocation),
					SpawnParameters);
				if (Actor == nullptr)
				{
					OutError = FString::Printf(
						TEXT("Cannot create authored-overlay anchor actor: %s"), *Evidence.OverlayId);
					return false;
				}
				++OutResult.CreatedActorCount;
			}
			else
			{
				if (IsCurrentAnchorActor(*Actor, Bundle, Set, Evidence))
				{
					++OutResult.PreservedActorCount;
					continue;
				}
				++OutResult.UpdatedActorCount;
			}

			const FString AssetName = FPackageName::GetLongPackageAssetName(Evidence.AuthoredPackage);
			const TSoftObjectPtr<UWorld> WorldAsset(
				FSoftObjectPath(Evidence.AuthoredPackage + TEXT(".") + AssetName));
			Actor->SetPackageExternal(true);
			Actor->Modify();
			Actor->SetActorTransform(FTransform(Evidence.WorldRotation, Evidence.WorldLocation));
			if (!Actor->SetWorldAsset(WorldAsset))
			{
				OutError = FString::Printf(
					TEXT("Cannot bind authored package %s to overlay %s."),
					*Evidence.AuthoredPackage, *Evidence.OverlayId);
				return false;
			}
			Actor->Tags.Reset();
			Actor->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
			Actor->Tags.Add(ValueTag(TEXT("ProjectWorld.Grid="), Bundle.GridId));
			Actor->Tags.Add(ValueTag(TEXT("ProjectWorld.AuthoredOverlay="), Evidence.OverlayId));
			Actor->Tags.Add(ValueTag(TEXT("ProjectWorld.AuthoredPackage="), Evidence.AuthoredPackage));
			Actor->Tags.Add(ValueTag(TEXT("ProjectWorld.AuthoredOverlaySet="), Set.SetHash));
			Actor->Tags.Add(ValueTag(TEXT("ProjectWorld.Resolver="), FString::FromInt(Set.ResolverVersion)));
			Actor->SetActorLabel(FString::Printf(TEXT("Authored Overlay - %s"), *Evidence.OverlayId));
			Actor->SetIsSpatiallyLoaded(true);
			Actor->bEnableAutoLODGeneration = false;
			Actor->MarkPackageDirty();
			const bool bPersistentMap =
				!World->PersistentLevel->GetPackage()->GetName().StartsWith(TEXT("/Temp/"));
			if (bPersistentMap && !SaveExternalActor(Actor))
			{
				OutError = FString::Printf(
					TEXT("Cannot save authored-overlay anchor actor: %s"), *Evidence.OverlayId);
				return false;
			}
			if (bPersistentMap)
			{
				++OutResult.SelfSavedActorMutationCount;
			}
			UE_LOG(
				LogProjectWorldAuthoredOverlayRealization,
				Display,
				TEXT("[ProjectWorldAuthoredOverlayRealization::Apply] Prepared anchor - overlay=%s external=%d package=%s persisted=%d"),
				*Evidence.OverlayId,
				Actor->IsPackageExternal() ? 1 : 0,
				Actor->GetExternalPackage() ? *Actor->GetExternalPackage()->GetName() : TEXT("none"),
				bPersistentMap ? 1 : 0);
		}

		for (const TPair<FString, ALevelInstance*>& Pair : Existing)
		{
			if (!PlacedIds.Contains(Pair.Key))
			{
				Stale.Add(Pair.Value);
			}
		}
		for (AActor* Actor : Stale)
		{
			if (!World->EditorDestroyActor(Actor, true))
			{
				OutError = TEXT("Cannot remove a stale authored-overlay anchor actor.");
				return false;
			}
			++OutResult.RemovedActorCount;
		}
		return true;
	}
}
