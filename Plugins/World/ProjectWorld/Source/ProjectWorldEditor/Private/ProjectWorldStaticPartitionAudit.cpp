// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldStaticPartitionAudit.h"

#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldPartitionPolicy.h"
#include "ProjectWorldRuntimeProfile.h"

#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "Misc/PackageName.h"
#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

namespace ProjectWorldStaticPartitionAudit
{
	namespace
	{
		constexpr double MinimumLoadingRangeCells = 3.0;
		constexpr double ExpectedCellBoundsToleranceMeters = 0.1;
		const FName LandscapeTag(TEXT("ProjectWorld.Landscape.v1"));
		const TArray<FName> CellOwnedLayerTags{
			TEXT("ProjectWorld.Water.v1"),
			TEXT("ProjectWorld.Road.v1"),
			TEXT("ProjectWorld.Vegetation=v1"),
			TEXT("ProjectWorld.BuildingMassing.v1"),
			TEXT("ProjectWorld.BuildingMassing.v2"),
			TEXT("ProjectWorld.GameplayPlacement.v1")};

		struct FCellRange
		{
			int32 MinX = 0;
			int32 MaxX = -1;
			int32 MinY = 0;
			int32 MaxY = -1;

			int32 Count() const
			{
				const int64 X = static_cast<int64>(MaxX) - MinX + 1;
				const int64 Y = static_cast<int64>(MaxY) - MinY + 1;
				return static_cast<int32>(FMath::Min<int64>(X * Y, MAX_int32));
			}
		};

		struct FActorEntry
		{
			AActor* Actor = nullptr;
			FGuid Guid;
			FBox Bounds = FBox(ForceInit);
			FName PackageName;
			TArray<FGuid> References;
			int64 PackageBytes = 0;
			int32 DataLayerCount = 0;
			bool bSpatial = false;
			bool bLandscape = false;
			bool bLandscapeProxy = false;
			bool bExternalPackage = false;
			bool bMissingPackage = false;
		};

		struct FReferenceBundle
		{
			FBox Bounds = FBox(ForceInit);
			int32 ActorCount = 0;
			bool bLandscape = false;
		};

		bool TryCellRange(const FBox& Bounds, int32 CellSizeMeters, FCellRange& OutRange)
		{
			if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN() || CellSizeMeters <= 0 ||
				!FMath::IsFinite(Bounds.Min.X) || !FMath::IsFinite(Bounds.Min.Y) ||
				!FMath::IsFinite(Bounds.Max.X) || !FMath::IsFinite(Bounds.Max.Y))
			{
				return false;
			}
			const double CellSizeCentimeters = CellSizeMeters * 100.0;
			const double MaximumX = FMath::Max(Bounds.Min.X, Bounds.Max.X - 0.01);
			const double MaximumY = FMath::Max(Bounds.Min.Y, Bounds.Max.Y - 0.01);
			OutRange.MinX = FMath::FloorToInt(Bounds.Min.X / CellSizeCentimeters);
			OutRange.MaxX = FMath::FloorToInt(MaximumX / CellSizeCentimeters);
			OutRange.MinY = FMath::FloorToInt(Bounds.Min.Y / CellSizeCentimeters);
			OutRange.MaxY = FMath::FloorToInt(MaximumY / CellSizeCentimeters);
			return OutRange.MaxX >= OutRange.MinX && OutRange.MaxY >= OutRange.MinY;
		}

		uint64 CellKey(int32 X, int32 Y)
		{
			return (static_cast<uint64>(static_cast<uint32>(X)) << 32) |
				static_cast<uint32>(Y);
		}

		int32 FindRoot(TArray<int32>& Parents, int32 Index)
		{
			while (Parents[Index] != Index)
			{
				Parents[Index] = Parents[Parents[Index]];
				Index = Parents[Index];
			}
			return Index;
		}

		void Join(TArray<int32>& Parents, int32 Left, int32 Right)
		{
			Left = FindRoot(Parents, Left);
			Right = FindRoot(Parents, Right);
			if (Left != Right)
			{
				Parents[Right] = Left;
			}
		}

		void AddFailure(TArray<TSharedPtr<FJsonValue>>& Errors, const FString& Message)
		{
			Errors.Add(MakeShared<FJsonValueString>(Message));
		}

		bool HasCellIdentity(const AActor* Actor)
		{
			return Actor->Tags.ContainsByPredicate([](const FName& Tag)
			{
				const FString Value = Tag.ToString();
				return Value.StartsWith(TEXT("ProjectWorld.Cell=")) ||
					Value.StartsWith(TEXT("ProjectWorld.WaterCell=")) ||
					Value.StartsWith(TEXT("ProjectWorld.RoadCell=")) ||
					Value.StartsWith(TEXT("ProjectWorld.VegetationCell=")) ||
					Value.StartsWith(TEXT("ProjectWorld.BuildingCell="));
			});
		}

		bool IsCellOwnedLayerActor(const AActor* Actor)
		{
			return CellOwnedLayerTags.ContainsByPredicate([Actor](const FName& Tag)
			{
				return Actor->Tags.Contains(Tag);
			});
		}
	}

	int32 CountIntersectedCells(const FBox& Bounds, int32 CellSizeMeters)
	{
		FCellRange Range;
		return TryCellRange(Bounds, CellSizeMeters, Range) ? Range.Count() : 0;
	}

	bool Capture(
		UWorld* World,
		const TArray<FProjectWorldRuntimeProfile>& Profiles,
		const FString& SelectedProfileId,
		TSharedPtr<FJsonObject>& OutReceipt,
		FString& OutError)
	{
		if (World == nullptr || !World->IsPartitionedWorld() || Profiles.IsEmpty())
		{
			OutError = TEXT("Static partition audit requires a partitioned world and at least one runtime profile.");
			return false;
		}
		UWorldPartition* WorldPartition = World->GetWorldPartition();
		UActorDescContainerInstance* Container = WorldPartition != nullptr
			? WorldPartition->GetActorDescContainerInstance()
			: nullptr;
		if (Container == nullptr)
		{
			OutError = TEXT("World Partition has no initialized actor descriptor container.");
			return false;
		}

		TArray<FActorEntry> Entries;
		TMap<FGuid, int32> IndexByGuid;
		TArray<ALandscape*> Landscapes;
		TArray<ALandscapeStreamingProxy*> LandscapeProxies;
		int32 HlodProxyActorCount = 0;
		int32 HlodEligibleGeneratedActorCount = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			HlodProxyActorCount += Actor->IsA<AWorldPartitionHLOD>() ? 1 : 0;
			if (!Actor->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag))
			{
				continue;
			}
			HlodEligibleGeneratedActorCount +=
				Actor->bEnableAutoLODGeneration || Actor->GetHLODLayer() != nullptr ? 1 : 0;
			if (ALandscape* Landscape = Cast<ALandscape>(Actor))
			{
				Landscapes.Add(Landscape);
			}
			if (ALandscapeStreamingProxy* Proxy = Cast<ALandscapeStreamingProxy>(Actor))
			{
				LandscapeProxies.Add(Proxy);
			}

			FActorEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Actor = Actor;
			Entry.Guid = Actor->GetActorGuid();
			Entry.bSpatial = Actor->GetIsSpatiallyLoaded();
			Entry.bLandscape = Actor->IsA<ALandscapeProxy>();
			Entry.bLandscapeProxy = Actor->IsA<ALandscapeStreamingProxy>();
			Entry.bExternalPackage = Actor->IsPackageExternal();
			if (const FWorldPartitionActorDescInstance* Desc = Container->GetActorDescInstance(Entry.Guid))
			{
				Entry.Bounds = Desc->GetRuntimeBounds();
				Entry.PackageName = Desc->GetActorPackage();
				Entry.References = Desc->GetReferences();
				Entry.DataLayerCount = Desc->GetDataLayerInstanceNames().Num();
			}
			else if (Entry.bSpatial)
			{
				Entry.Bounds = Actor->GetComponentsBoundingBox(true);
			}
			if (Entry.bExternalPackage)
			{
				if (Entry.PackageName.IsNone())
				{
					Entry.PackageName = Actor->GetPackage()->GetFName();
				}
				FString Filename;
				if (FPackageName::DoesPackageExist(Entry.PackageName.ToString(), &Filename))
				{
					Entry.PackageBytes = IFileManager::Get().FileSize(*Filename);
				}
				else
				{
					Entry.PackageBytes = -1;
				}
				Entry.bMissingPackage = Entry.PackageBytes < 0;
			}
			IndexByGuid.Add(Entry.Guid, Entries.Num() - 1);
		}

		int32 LandscapeOwnershipFailureCount = Landscapes.Num() == 1 ? 0 : 1;
		ALandscape* LandscapeOwner = Landscapes.Num() == 1 ? Landscapes[0] : nullptr;
		if (LandscapeOwner != nullptr && LandscapeOwner->GetIsSpatiallyLoaded())
		{
			++LandscapeOwnershipFailureCount;
		}
		for (ALandscapeStreamingProxy* Proxy : LandscapeProxies)
		{
			if (!Proxy->GetIsSpatiallyLoaded() || !Proxy->IsPackageExternal() ||
				Proxy->GetLandscapeActor() != LandscapeOwner ||
				!Proxy->Tags.Contains(LandscapeTag))
			{
				++LandscapeOwnershipFailureCount;
			}
		}
		FVector2D ExpectedCanonicalCellSizeMeters = FVector2D::ZeroVector;
		for (const FActorEntry& Entry : Entries)
		{
			if (Entry.bLandscapeProxy && Entry.Bounds.IsValid)
			{
				const FVector SizeMeters = Entry.Bounds.GetSize() * 0.01;
				ExpectedCanonicalCellSizeMeters.X = FMath::Max(ExpectedCanonicalCellSizeMeters.X, SizeMeters.X);
				ExpectedCanonicalCellSizeMeters.Y = FMath::Max(ExpectedCanonicalCellSizeMeters.Y, SizeMeters.Y);
			}
		}
		if (ExpectedCanonicalCellSizeMeters.X <= 0.0 || ExpectedCanonicalCellSizeMeters.Y <= 0.0)
		{
			++LandscapeOwnershipFailureCount;
		}

		TArray<int32> Parents;
		Parents.SetNumUninitialized(Entries.Num());
		for (int32 Index = 0; Index < Parents.Num(); ++Index)
		{
			Parents[Index] = Index;
		}
		int32 GeneratedReferenceCount = 0;
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			if (!Entries[Index].bSpatial)
			{
				continue;
			}
			for (const FGuid& Reference : Entries[Index].References)
			{
				if (const int32* Target = IndexByGuid.Find(Reference); Target != nullptr && Entries[*Target].bSpatial)
				{
					++GeneratedReferenceCount;
					Join(Parents, Index, *Target);
				}
			}
		}

		TMap<int32, FReferenceBundle> Bundles;
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const FActorEntry& Entry = Entries[Index];
			if (!Entry.bSpatial || !Entry.Bounds.IsValid)
			{
				continue;
			}
			FReferenceBundle& Bundle = Bundles.FindOrAdd(FindRoot(Parents, Index));
			Bundle.Bounds += Entry.Bounds;
			++Bundle.ActorCount;
			Bundle.bLandscape |= Entry.bLandscape;
		}

		TArray<TSharedPtr<FJsonValue>> ProfileValues;
		bool bFoundSelectedProfile = false;
		bool bSelectedProfileAccepted = false;
		for (const FProjectWorldRuntimeProfile& Profile : Profiles)
		{
			TSharedRef<FJsonObject> ProfileObject = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Errors;
			TMap<uint64, TSet<FName>> CellPackages;
			TMap<FName, int64> UniquePackageBytes;
			int32 InvalidBoundsCount = 0;
			int32 MissingPackageCount = 0;
			int32 DataLayerMembershipCount = 0;
			int32 CellOwnedActorBoundsOverhangCount = 0;
			int32 CellOwnedActorMissingIdentityCount = 0;
			FString MaximumBoundsOverhangActorName;
			FVector MaximumBoundsOverhangActorSize = FVector::ZeroVector;
			TArray<FName> MaximumBoundsOverhangActorTags;
			FString FirstMissingIdentityActorName;
			TArray<FName> FirstMissingIdentityActorTags;
			int32 MaximumActorCellSpan = 0;
			int32 MaximumNonLandscapeActorCellSpan = 0;
			FString MaximumNonLandscapeActorName;
			FString MaximumNonLandscapeActorClass;
			FVector MaximumNonLandscapeActorSize = FVector::ZeroVector;
			TArray<FName> MaximumNonLandscapeActorTags;
			int32 SpatialActorCellAssignments = 0;
			for (const FActorEntry& Entry : Entries)
			{
				DataLayerMembershipCount += Entry.DataLayerCount;
				MissingPackageCount += Entry.bMissingPackage ? 1 : 0;
				if (Entry.bExternalPackage && Entry.PackageBytes >= 0)
				{
					UniquePackageBytes.FindOrAdd(Entry.PackageName) = Entry.PackageBytes;
				}
				if (!Entry.bSpatial)
				{
					continue;
				}
				FCellRange Range;
				if (!TryCellRange(Entry.Bounds, Profile.RuntimeCellSizeMeters, Range))
				{
					++InvalidBoundsCount;
					continue;
				}
				const int32 Span = Range.Count();
				MaximumActorCellSpan = FMath::Max(MaximumActorCellSpan, Span);
				if (!Entry.bLandscape)
				{
					if (Span > MaximumNonLandscapeActorCellSpan)
					{
						MaximumNonLandscapeActorCellSpan = Span;
						MaximumNonLandscapeActorName = Entry.Actor->GetName();
						MaximumNonLandscapeActorClass = Entry.Actor->GetClass()->GetPathName();
						MaximumNonLandscapeActorSize = Entry.Bounds.GetSize() * 0.01;
						MaximumNonLandscapeActorTags = Entry.Actor->Tags;
					}
					if (IsCellOwnedLayerActor(Entry.Actor))
					{
						if (!HasCellIdentity(Entry.Actor))
						{
							++CellOwnedActorMissingIdentityCount;
							if (FirstMissingIdentityActorName.IsEmpty())
							{
								FirstMissingIdentityActorName = Entry.Actor->GetName();
								FirstMissingIdentityActorTags = Entry.Actor->Tags;
							}
						}
						const FVector SizeMeters = Entry.Bounds.GetSize() * 0.01;
						const bool bHasBoundsOverhang =
							SizeMeters.X > ExpectedCanonicalCellSizeMeters.X + ExpectedCellBoundsToleranceMeters ||
							SizeMeters.Y > ExpectedCanonicalCellSizeMeters.Y + ExpectedCellBoundsToleranceMeters;
						if (bHasBoundsOverhang)
						{
							++CellOwnedActorBoundsOverhangCount;
							if (SizeMeters.X * SizeMeters.Y >
								MaximumBoundsOverhangActorSize.X * MaximumBoundsOverhangActorSize.Y)
							{
								MaximumBoundsOverhangActorName = Entry.Actor->GetName();
								MaximumBoundsOverhangActorSize = SizeMeters;
								MaximumBoundsOverhangActorTags = Entry.Actor->Tags;
							}
						}
					}
				}
				SpatialActorCellAssignments += Span;
				if (Entry.bExternalPackage && !Entry.bMissingPackage && Span <= 4096)
				{
					for (int32 X = Range.MinX; X <= Range.MaxX; ++X)
					{
						for (int32 Y = Range.MinY; Y <= Range.MaxY; ++Y)
						{
							CellPackages.FindOrAdd(CellKey(X, Y)).Add(Entry.PackageName);
						}
					}
				}
			}

			int32 ReferenceBundleCount = 0;
			int32 NonLandscapeReferenceBundleCount = 0;
			int32 LandscapeReferenceBundleCount = 0;
			int32 MaximumReferenceBundleActorCount = 0;
			int32 MaximumNonLandscapeReferenceBundleCellSpan = 0;
			int32 MaximumLandscapeReferenceBundleCellSpan = 0;
			for (const TPair<int32, FReferenceBundle>& Pair : Bundles)
			{
				const FReferenceBundle& Bundle = Pair.Value;
				if (Bundle.ActorCount <= 1)
				{
					continue;
				}
				++ReferenceBundleCount;
				MaximumReferenceBundleActorCount = FMath::Max(MaximumReferenceBundleActorCount, Bundle.ActorCount);
				const int32 Span = CountIntersectedCells(Bundle.Bounds, Profile.RuntimeCellSizeMeters);
				if (Bundle.bLandscape)
				{
					++LandscapeReferenceBundleCount;
					MaximumLandscapeReferenceBundleCellSpan = FMath::Max(MaximumLandscapeReferenceBundleCellSpan, Span);
				}
				else
				{
					++NonLandscapeReferenceBundleCount;
					MaximumNonLandscapeReferenceBundleCellSpan =
						FMath::Max(MaximumNonLandscapeReferenceBundleCellSpan, Span);
				}
			}

			int64 TotalExternalPackageBytes = 0;
			int64 MaximumExternalPackageBytes = 0;
			for (const TPair<FName, int64>& Pair : UniquePackageBytes)
			{
				TotalExternalPackageBytes += Pair.Value;
				MaximumExternalPackageBytes = FMath::Max(MaximumExternalPackageBytes, Pair.Value);
			}
			int64 MaximumCellPackageBytes = 0;
			for (const TPair<uint64, TSet<FName>>& Pair : CellPackages)
			{
				int64 CellBytes = 0;
				for (const FName PackageName : Pair.Value)
				{
					CellBytes += UniquePackageBytes.FindRef(PackageName);
				}
				MaximumCellPackageBytes = FMath::Max(MaximumCellPackageBytes, CellBytes);
			}

			const int32 HlodLayerReferenceCount = ProjectWorldPartitionPolicy::CountHLODLayerReferences(World);
			const double LoadingRangeCells =
				static_cast<double>(Profile.RuntimeLoadingRangeMeters) / Profile.RuntimeCellSizeMeters;
			if (InvalidBoundsCount > 0)
			{
				AddFailure(Errors, FString::Printf(TEXT("%d spatial generated actors have invalid runtime bounds."), InvalidBoundsCount));
			}
			if (CellOwnedActorMissingIdentityCount > 0)
			{
				AddFailure(Errors, FString::Printf(
					TEXT("%d generated layer actors have no canonical-cell identity."),
					CellOwnedActorMissingIdentityCount));
			}
			if (NonLandscapeReferenceBundleCount > 0)
			{
				AddFailure(Errors, FString::Printf(
					TEXT("%d non-Landscape generated actor reference bundles violate cell-local ownership."),
					NonLandscapeReferenceBundleCount));
			}
			if (MissingPackageCount > 0)
			{
				AddFailure(Errors, FString::Printf(TEXT("%d generated external actor packages are missing."), MissingPackageCount));
			}
			if (DataLayerMembershipCount > 0)
			{
				AddFailure(Errors, FString::Printf(
					TEXT("Current no-Data-Layer candidates contain %d generated actor memberships."),
					DataLayerMembershipCount));
			}
			if (LandscapeOwnershipFailureCount > 0 || LandscapeProxies.IsEmpty())
			{
				AddFailure(Errors, FString::Printf(
					TEXT("Landscape ownership failed: failures=%d roots=%d proxies=%d."),
					LandscapeOwnershipFailureCount, Landscapes.Num(), LandscapeProxies.Num()));
			}
			if (HlodProxyActorCount > 0 || HlodLayerReferenceCount > 0 || HlodEligibleGeneratedActorCount > 0)
			{
				AddFailure(Errors, TEXT("The production zero-HLOD contract is not satisfied."));
			}
			if (LoadingRangeCells < MinimumLoadingRangeCells)
			{
				AddFailure(Errors, FString::Printf(
					TEXT("Loading range covers %.3f cells; minimum source-speed static coverage is %.1f."),
					LoadingRangeCells, MinimumLoadingRangeCells));
			}

			const bool bAccepted = Errors.IsEmpty();
			ProfileObject->SetStringField(TEXT("profile_id"), Profile.ProfileId);
			ProfileObject->SetStringField(TEXT("profile_sha256"), Profile.ProfileHash);
			ProfileObject->SetStringField(TEXT("status"), bAccepted ? TEXT("accepted") : TEXT("rejected"));
			ProfileObject->SetNumberField(TEXT("cell_size_m"), Profile.RuntimeCellSizeMeters);
			ProfileObject->SetNumberField(TEXT("loading_range_m"), Profile.RuntimeLoadingRangeMeters);
			ProfileObject->SetNumberField(TEXT("loading_range_cells"), LoadingRangeCells);
			ProfileObject->SetArrayField(TEXT("expected_canonical_cell_size_m"), {
				MakeShared<FJsonValueNumber>(ExpectedCanonicalCellSizeMeters.X),
				MakeShared<FJsonValueNumber>(ExpectedCanonicalCellSizeMeters.Y)});
			ProfileObject->SetNumberField(TEXT("generated_actor_count"), Entries.Num());
			ProfileObject->SetNumberField(TEXT("spatial_actor_cell_assignments"), SpatialActorCellAssignments);
			ProfileObject->SetNumberField(TEXT("invalid_bounds_count"), InvalidBoundsCount);
			ProfileObject->SetNumberField(
				TEXT("cell_owned_actor_bounds_overhang_count"), CellOwnedActorBoundsOverhangCount);
			ProfileObject->SetNumberField(
				TEXT("cell_owned_actor_missing_identity_count"), CellOwnedActorMissingIdentityCount);
			ProfileObject->SetStringField(
				TEXT("maximum_bounds_overhang_actor_name"), MaximumBoundsOverhangActorName);
			ProfileObject->SetArrayField(TEXT("maximum_bounds_overhang_actor_size_m"), {
				MakeShared<FJsonValueNumber>(MaximumBoundsOverhangActorSize.X),
				MakeShared<FJsonValueNumber>(MaximumBoundsOverhangActorSize.Y),
				MakeShared<FJsonValueNumber>(MaximumBoundsOverhangActorSize.Z)});
			TArray<TSharedPtr<FJsonValue>> BoundsOverhangActorTags;
			for (const FName Tag : MaximumBoundsOverhangActorTags)
			{
				BoundsOverhangActorTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
			ProfileObject->SetArrayField(
				TEXT("maximum_bounds_overhang_actor_tags"), BoundsOverhangActorTags);
			ProfileObject->SetStringField(
				TEXT("first_missing_identity_actor_name"), FirstMissingIdentityActorName);
			TArray<TSharedPtr<FJsonValue>> MissingIdentityActorTags;
			for (const FName Tag : FirstMissingIdentityActorTags)
			{
				MissingIdentityActorTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
			ProfileObject->SetArrayField(
				TEXT("first_missing_identity_actor_tags"), MissingIdentityActorTags);
			ProfileObject->SetNumberField(TEXT("maximum_actor_cell_span"), MaximumActorCellSpan);
			ProfileObject->SetNumberField(
				TEXT("maximum_non_landscape_actor_cell_span"), MaximumNonLandscapeActorCellSpan);
			ProfileObject->SetStringField(
				TEXT("maximum_non_landscape_actor_name"), MaximumNonLandscapeActorName);
			ProfileObject->SetStringField(
				TEXT("maximum_non_landscape_actor_class"), MaximumNonLandscapeActorClass);
			ProfileObject->SetArrayField(TEXT("maximum_non_landscape_actor_size_m"), {
				MakeShared<FJsonValueNumber>(MaximumNonLandscapeActorSize.X),
				MakeShared<FJsonValueNumber>(MaximumNonLandscapeActorSize.Y),
				MakeShared<FJsonValueNumber>(MaximumNonLandscapeActorSize.Z)});
			TArray<TSharedPtr<FJsonValue>> MaximumActorTags;
			for (const FName Tag : MaximumNonLandscapeActorTags)
			{
				MaximumActorTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
			ProfileObject->SetArrayField(TEXT("maximum_non_landscape_actor_tags"), MaximumActorTags);
			ProfileObject->SetNumberField(TEXT("generated_reference_count"), GeneratedReferenceCount);
			ProfileObject->SetNumberField(TEXT("reference_bundle_count"), ReferenceBundleCount);
			ProfileObject->SetNumberField(
				TEXT("non_landscape_reference_bundle_count"), NonLandscapeReferenceBundleCount);
			ProfileObject->SetNumberField(
				TEXT("landscape_reference_bundle_count"), LandscapeReferenceBundleCount);
			ProfileObject->SetNumberField(
				TEXT("maximum_reference_bundle_actor_count"), MaximumReferenceBundleActorCount);
			ProfileObject->SetNumberField(
				TEXT("maximum_non_landscape_reference_bundle_cell_span"),
				MaximumNonLandscapeReferenceBundleCellSpan);
			ProfileObject->SetNumberField(
				TEXT("maximum_landscape_reference_bundle_cell_span"), MaximumLandscapeReferenceBundleCellSpan);
			ProfileObject->SetNumberField(TEXT("external_actor_package_count"), UniquePackageBytes.Num());
			ProfileObject->SetNumberField(TEXT("external_actor_package_bytes"), TotalExternalPackageBytes);
			ProfileObject->SetNumberField(TEXT("maximum_external_actor_package_bytes"), MaximumExternalPackageBytes);
			ProfileObject->SetNumberField(TEXT("runtime_cells_with_packages"), CellPackages.Num());
			ProfileObject->SetNumberField(TEXT("maximum_runtime_cell_package_bytes"), MaximumCellPackageBytes);
			ProfileObject->SetNumberField(TEXT("generated_data_layer_membership_count"), DataLayerMembershipCount);
			ProfileObject->SetNumberField(TEXT("landscape_root_count"), Landscapes.Num());
			ProfileObject->SetNumberField(TEXT("landscape_proxy_count"), LandscapeProxies.Num());
			ProfileObject->SetNumberField(TEXT("landscape_ownership_failure_count"), LandscapeOwnershipFailureCount);
			ProfileObject->SetNumberField(TEXT("hlod_proxy_actor_count"), HlodProxyActorCount);
			ProfileObject->SetNumberField(TEXT("hlod_layer_reference_count"), HlodLayerReferenceCount);
			ProfileObject->SetNumberField(
				TEXT("hlod_eligible_generated_actor_count"), HlodEligibleGeneratedActorCount);
			ProfileObject->SetArrayField(TEXT("errors"), Errors);
			ProfileValues.Add(MakeShared<FJsonValueObject>(ProfileObject));

			if (Profile.ProfileId == SelectedProfileId)
			{
				bFoundSelectedProfile = true;
				bSelectedProfileAccepted = bAccepted;
			}
		}

		if (!bFoundSelectedProfile)
		{
			OutError = FString::Printf(TEXT("Selected runtime profile was not audited: %s"), *SelectedProfileId);
			return false;
		}
		OutReceipt = MakeShared<FJsonObject>();
		OutReceipt->SetStringField(
			TEXT("$schema"), TEXT("https://alis.world/schemas/world-static-partition-audit/result-v1.json"));
		OutReceipt->SetNumberField(TEXT("schema_version"), 1);
		OutReceipt->SetStringField(TEXT("status"), bSelectedProfileAccepted ? TEXT("accepted") : TEXT("rejected"));
		OutReceipt->SetStringField(TEXT("map_package"), World->GetPackage()->GetName());
		OutReceipt->SetStringField(TEXT("selected_profile_id"), SelectedProfileId);
		OutReceipt->SetNumberField(
			TEXT("expected_cell_bounds_tolerance_m"), ExpectedCellBoundsToleranceMeters);
		OutReceipt->SetNumberField(TEXT("minimum_loading_range_cells"), MinimumLoadingRangeCells);
		OutReceipt->SetArrayField(TEXT("profiles"), ProfileValues);
		return bSelectedProfileAccepted;
	}
}
