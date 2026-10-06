// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimeRealization.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRuntimeNavigation.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldTerritoryRuntimeAcceptance.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "UObject/UObjectGlobals.h"

namespace ProjectWorldRuntimeRealization
{
	namespace
	{
		const FString RuntimeRolePrefix(TEXT("ProjectWorld.RuntimeRole="));
		const FString RuntimeProfilePrefix(TEXT("ProjectWorld.Runtime="));
		const FString RuntimeProfileHashPrefix(TEXT("ProjectWorld.RuntimeHash="));
		const FString GridPrefix(TEXT("ProjectWorld.Grid="));
		const FString RoutePrefix(TEXT("ProjectWorld.Route="));

		void SetTagValue(AActor& Actor, const FString& Prefix, const FString& Value)
		{
			Actor.Tags.RemoveAll([&Prefix](const FName& Tag)
			{
				return Tag.ToString().StartsWith(Prefix);
			});
			Actor.Tags.Add(FName(*(Prefix + Value)));
		}

		FString RuntimeRole(const AActor& Actor)
		{
			for (const FName& Tag : Actor.Tags)
			{
				const FString Value = Tag.ToString();
				if (Value.StartsWith(RuntimeRolePrefix))
				{
					return Value.RightChop(RuntimeRolePrefix.Len());
				}
			}
			return FString();
		}

		bool HasSingleTagValue(
			const AActor& Actor,
			const FString& Prefix,
			const FString& Value)
		{
			int32 MatchCount = 0;
			for (const FName& Tag : Actor.Tags)
			{
				const FString TagValue = Tag.ToString();
				if (TagValue.StartsWith(Prefix))
				{
					++MatchCount;
					if (TagValue != Prefix + Value)
					{
						return false;
					}
				}
			}
			return MatchCount == 1;
		}

		APlayerStart* FindCurrentProductPlayerStart(
			UWorld* World,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			const FVector& ProductLocation)
		{
			const FString Role(TEXT("PlayerStart"));
			const FGuid ExpectedGuid = ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|runtime|") + Role);
			const FName ExpectedName(TEXT("ProjectWorld_PlayerStart"));
			TSet<AActor*> IdentityActors;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (RuntimeRole(**It) == Role || It->GetActorGuid() == ExpectedGuid ||
					It->GetFName() == ExpectedName)
				{
					IdentityActors.Add(*It);
				}
			}
			if (UObject* NamedObject = StaticFindObjectFast(nullptr, World->PersistentLevel, ExpectedName))
			{
				if (AActor* NamedActor = Cast<AActor>(NamedObject))
				{
					IdentityActors.Add(NamedActor);
				}
			}
			if (IdentityActors.Num() != 1)
			{
				return nullptr;
			}
			APlayerStart* PlayerStart = Cast<APlayerStart>(*IdentityActors.CreateConstIterator());
			const FRotator ExpectedRotation(
				Profile.ProductSpawnPitchDegrees,
				Profile.ProductSpawnYawDegrees,
				0.0);
			return PlayerStart != nullptr && PlayerStart->GetClass() == APlayerStart::StaticClass() &&
				PlayerStart->GetActorGuid() == ExpectedGuid && PlayerStart->GetFName() == ExpectedName &&
				PlayerStart->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag) &&
				HasSingleTagValue(*PlayerStart, RuntimeRolePrefix, Role) &&
				HasSingleTagValue(*PlayerStart, RuntimeProfilePrefix, Profile.ProfileId) &&
				HasSingleTagValue(*PlayerStart, RuntimeProfileHashPrefix, Profile.ProfileHash) &&
				HasSingleTagValue(*PlayerStart, GridPrefix, Bundle.GridId) &&
				HasSingleTagValue(*PlayerStart, RoutePrefix, Profile.RouteId) &&
				PlayerStart->GetActorLocation().Equals(ProductLocation, 0.01) &&
				PlayerStart->GetActorRotation().Equals(ExpectedRotation, 0.01) &&
				!PlayerStart->GetIsSpatiallyLoaded() && !PlayerStart->bEnableAutoLODGeneration
				? PlayerStart
				: nullptr;
		}

		ANavMeshBoundsVolume* FindCurrentTerritoryNavigation(
			UWorld* World,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			const FBox& ExpectedBounds)
		{
			const FString Role(TEXT("TerritoryNavigation"));
			const FGuid ExpectedGuid = ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|runtime|") + Role);
			const FName ExpectedName(TEXT("ProjectWorld_TerritoryNavigation"));
			TSet<AActor*> IdentityActors;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (RuntimeRole(**It) == Role || It->GetActorGuid() == ExpectedGuid ||
					It->GetFName() == ExpectedName)
				{
					IdentityActors.Add(*It);
				}
			}
			if (UObject* NamedObject = StaticFindObjectFast(nullptr, World->PersistentLevel, ExpectedName))
			{
				if (AActor* NamedActor = Cast<AActor>(NamedObject))
				{
					IdentityActors.Add(NamedActor);
				}
			}
			if (IdentityActors.Num() != 1)
			{
				return nullptr;
			}
			ANavMeshBoundsVolume* NavBounds = Cast<ANavMeshBoundsVolume>(*IdentityActors.CreateConstIterator());
			const FBox ActualBounds = NavBounds != nullptr
				? NavBounds->GetComponentsBoundingBox(true)
				: FBox(ForceInit);
			return NavBounds != nullptr && NavBounds->GetClass() == ANavMeshBoundsVolume::StaticClass() &&
				NavBounds->GetActorGuid() == ExpectedGuid && NavBounds->GetFName() == ExpectedName &&
				NavBounds->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag) &&
				HasSingleTagValue(*NavBounds, RuntimeRolePrefix, Role) &&
				HasSingleTagValue(*NavBounds, RuntimeProfilePrefix, Profile.ProfileId) &&
				HasSingleTagValue(*NavBounds, RuntimeProfileHashPrefix, Profile.ProfileHash) &&
				HasSingleTagValue(*NavBounds, GridPrefix, Bundle.GridId) &&
				HasSingleTagValue(*NavBounds, RoutePrefix, Profile.RouteId) &&
				ActualBounds.IsValid && ActualBounds.Min.Equals(ExpectedBounds.Min, 1.0) &&
				ActualBounds.Max.Equals(ExpectedBounds.Max, 1.0) &&
				!NavBounds->GetIsSpatiallyLoaded() && !NavBounds->bEnableAutoLODGeneration
				? NavBounds
				: nullptr;
		}

		const FProjectWorldCanonicalFeature* RouteFeature(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			FString& OutError)
		{
			if (Bundle.GridId != Profile.GridId)
			{
				OutError = FString::Printf(
					TEXT("Runtime profile grid %s does not match canonical grid %s."),
					*Profile.GridId,
					*Bundle.GridId);
				return nullptr;
			}
			const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(Profile.RouteFeatureId);
			if (Feature == nullptr || Feature->FeatureClass != TEXT("road") ||
				Feature->GeometryType != TEXT("LineString") || Feature->GeometryPoints.Num() < 2)
			{
				OutError = FString::Printf(
					TEXT("Runtime route must pin an accepted canonical LineString road: %s."),
					*Profile.RouteFeatureId);
				return nullptr;
			}
			return Feature;
		}

		FVector2D InsetEndpoint(
			const TArray<FVector2D>& Points,
			bool bFromStart,
			double InsetMeters)
		{
			const FVector2D Endpoint = bFromStart ? Points[0] : Points.Last();
			const FVector2D Neighbor = bFromStart ? Points[1] : Points[Points.Num() - 2];
			const FVector2D Direction = (Neighbor - Endpoint).GetSafeNormal();
			return Endpoint + Direction * InsetMeters;
		}

		const FProjectWorldCanonicalCell* CellAt(
			const FProjectWorldCanonicalBundle& Bundle,
			const FVector2D& Point)
		{
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				if (Point.X >= Cell.Bounds.X && Point.X <= Cell.Bounds.Z &&
					Point.Y >= Cell.Bounds.Y && Point.Y <= Cell.Bounds.W)
				{
					return &Cell;
				}
			}
			return nullptr;
		}

		bool RouteLocations(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			const FProjectWorldCanonicalFeature& Feature,
			FVector& OutStart,
			FVector& OutEnd,
			FString& OutError)
		{
			const FVector2D Start = InsetEndpoint(Feature.GeometryPoints, true, Profile.EndpointInsetMeters);
			const FVector2D End = InsetEndpoint(Feature.GeometryPoints, false, Profile.EndpointInsetMeters);
			const FProjectWorldCanonicalCell* StartCell = CellAt(Bundle, Start);
			const FProjectWorldCanonicalCell* EndCell = CellAt(Bundle, End);
			if (StartCell == nullptr || EndCell == nullptr)
			{
				OutError = TEXT("Runtime route endpoints must remain inside accepted canonical cells.");
				return false;
			}
			OutStart = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(Start, ProjectWorldGeneratedGeometry::SampleTerrain(*StartCell, Start.X, Start.Y) + Profile.RouteSurfaceOffsetMeters));
			OutEnd = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(End, ProjectWorldGeneratedGeometry::SampleTerrain(*EndCell, End.X, End.Y) + Profile.RouteSurfaceOffsetMeters));
			return true;
		}

		bool ProductSpawnLocation(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			FVector& OutLocation,
			FString& OutError)
		{
			if (Profile.ProductSpawnAnchor != TEXT("engine_georeference_origin"))
			{
				OutError = TEXT("The product spawn must use the engine georeference origin anchor.");
				return false;
			}
			const FVector2D Anchor = Bundle.EngineGeoreferenceOriginMeters;
			const FProjectWorldCanonicalCell* Cell = CellAt(Bundle, Anchor);
			if (Cell == nullptr)
			{
				OutError = TEXT("The product spawn anchor must remain inside an accepted canonical cell.");
				return false;
			}

			const double TerrainHeight = ProjectWorldGeneratedGeometry::SampleTerrain(*Cell, Anchor.X, Anchor.Y);
			OutLocation = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				FVector(Anchor, TerrainHeight + Profile.ProductSpawnHeightAboveTerrainMeters));
			return true;
		}

		AActor* ReuseOrSpawn(
			UWorld* World,
			UClass* ActorClass,
			const FString& Role,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			FProjectWorldRealizationResult& OutResult)
		{
			const FGuid ExpectedGuid = ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|runtime|") + Role);
			const FName ExpectedName(*FString::Printf(TEXT("ProjectWorld_%s"), *Role));
			TSet<AActor*> IdentityActors;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (RuntimeRole(**It) == Role || It->GetActorGuid() == ExpectedGuid ||
					It->GetFName() == ExpectedName)
				{
					IdentityActors.Add(*It);
				}
			}
			if (UObject* NamedObject = StaticFindObjectFast(nullptr, World->PersistentLevel, ExpectedName))
			{
				AActor* NamedActor = Cast<AActor>(NamedObject);
				if (NamedActor == nullptr)
				{
					return nullptr;
				}
				IdentityActors.Add(NamedActor);
			}
			AActor* Existing = nullptr;
			for (AActor* Candidate : IdentityActors)
			{
				if (Candidate->GetClass() == ActorClass && Candidate->GetActorGuid() == ExpectedGuid)
				{
					if (Existing != nullptr)
					{
						return nullptr;
					}
					Existing = Candidate;
				}
			}
			if (Existing != nullptr && IdentityActors.Num() == 1)
			{
				IdentityActors.Remove(Existing);
			}
			else
			{
				Existing = nullptr;
			}
			for (AActor* Retired : IdentityActors)
			{
				const FName RetiredName = MakeUniqueObjectName(
					Retired->GetOuter(), Retired->GetClass(), TEXT("ProjectWorld_RetiredRuntime"));
				if (!Retired->Rename(
					*RetiredName.ToString(),
					Retired->GetOuter(),
					REN_DontCreateRedirectors | REN_NonTransactional) ||
					!World->EditorDestroyActor(Retired, true))
				{
					return nullptr;
				}
				++OutResult.RemovedActorCount;
			}
			if (!IdentityActors.IsEmpty())
			{
				CollectGarbage(RF_NoFlags);
			}
			if (Existing != nullptr)
			{
				if (Existing->GetFName() != ExpectedName && !Existing->Rename(
					*ExpectedName.ToString(),
					Existing->GetOuter(),
					REN_DontCreateRedirectors | REN_NonTransactional))
				{
					return nullptr;
				}
				++OutResult.UpdatedActorCount;
				return Existing;
			}

			FActorSpawnParameters Parameters;
			Parameters.Name = ExpectedName;
			Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
			Parameters.OverrideActorGuid = ExpectedGuid;
			AActor* Actor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, Parameters);
			if (Actor != nullptr)
			{
				++OutResult.CreatedActorCount;
			}
			return Actor;
		}

		void SetIdentity(
			AActor& Actor,
			const FString& Role,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldRuntimeProfile& Profile,
			bool bSpatiallyLoaded)
		{
			Actor.Tags.AddUnique(ProjectWorldGeneratedGeometry::GeneratedTag);
			SetTagValue(Actor, RuntimeRolePrefix, Role);
			SetTagValue(Actor, RuntimeProfilePrefix, Profile.ProfileId);
			SetTagValue(Actor, RuntimeProfileHashPrefix, Profile.ProfileHash);
			SetTagValue(Actor, GridPrefix, Bundle.GridId);
			SetTagValue(Actor, RoutePrefix, Profile.RouteId);
			Actor.SetActorLabel(FString::Printf(TEXT("ProjectWorld_%s"), *Role), false);
			Actor.bEnableAutoLODGeneration = false;
			if (Actor.CanChangeIsSpatiallyLoadedFlag())
			{
				Actor.SetIsSpatiallyLoaded(bSpatiallyLoaded);
			}
			Actor.MarkPackageDirty();
		}
	}

	bool IsRuntimeRoleActor(const AActor& Actor)
	{
		return !RuntimeRole(Actor).IsEmpty();
	}

	bool IsCurrentRuntimeActorForApply(
		const AActor& Actor,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bRuntimeRequested)
	{
		return bRuntimeRequested && Actor.Tags.Contains(FName(*(GridPrefix + Bundle.GridId)));
	}

	bool Validate(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile& Profile,
		FString& OutError)
	{
		if (Profile.ProfileKind != TEXT("territory_product"))
		{
			OutError = TEXT("Only territory product runtime profiles are supported.");
			return false;
		}
		const FProjectWorldCanonicalFeature* Feature = RouteFeature(Bundle, Profile, OutError);
		FVector Start;
		FVector End;
		if (Feature == nullptr || !RouteLocations(Bundle, Profile, *Feature, Start, End, OutError))
		{
			return false;
		}
		return ProductSpawnLocation(Bundle, Profile, Start, OutError);
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		if (Profile.ProfileKind != TEXT("territory_product"))
		{
			OutError = TEXT("Only territory product runtime profiles are supported.");
			return false;
		}
		const FProjectWorldCanonicalFeature* Feature = RouteFeature(Bundle, Profile, OutError);
		FVector Start;
		FVector End;
		if (Feature == nullptr || !RouteLocations(Bundle, Profile, *Feature, Start, End, OutError))
		{
			return false;
		}

		FVector ProductLocation;
		if (!ProductSpawnLocation(Bundle, Profile, ProductLocation, OutError))
		{
			return false;
		}
		if (FindCurrentProductPlayerStart(World, Bundle, Profile, ProductLocation) != nullptr)
		{
			++OutResult.PreservedActorCount;
		}
		else
		{
			APlayerStart* PlayerStart = Cast<APlayerStart>(ReuseOrSpawn(
				World, APlayerStart::StaticClass(), TEXT("PlayerStart"), Bundle, Profile, OutResult));
			if (PlayerStart == nullptr)
			{
				OutError = TEXT("Cannot create the unique territory PlayerStart.");
				return false;
			}
			PlayerStart->SetActorLocation(ProductLocation);
			PlayerStart->SetActorRotation(FRotator(
				Profile.ProductSpawnPitchDegrees,
				Profile.ProductSpawnYawDegrees,
				0.0));
			SetIdentity(*PlayerStart, TEXT("PlayerStart"), Bundle, Profile, false);
		}

		FBox TerritoryNavigationBounds;
		if (!ProjectWorldRuntimeNavigation::GetTerritoryDomainBounds(
			Bundle, Profile, TerritoryNavigationBounds, OutError))
		{
			return false;
		}
		ARecastNavMesh* Recast = nullptr;
		if (ANavMeshBoundsVolume* CurrentNavigation = FindCurrentTerritoryNavigation(
			World, Bundle, Profile, TerritoryNavigationBounds))
		{
			++OutResult.PreservedActorCount;
			if (!ProjectWorldRuntimeNavigation::EnsureTerritoryDomain(
				World, CurrentNavigation, Recast, OutError))
			{
				return false;
			}
		}
		else
		{
			ANavMeshBoundsVolume* NavBounds = Cast<ANavMeshBoundsVolume>(ReuseOrSpawn(
				World,
				ANavMeshBoundsVolume::StaticClass(),
				TEXT("TerritoryNavigation"),
				Bundle,
				Profile,
				OutResult));
			if (NavBounds == nullptr || !ProjectWorldRuntimeNavigation::ConfigureTerritoryDomain(
				World, NavBounds, TerritoryNavigationBounds, Recast, OutError))
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("Cannot create the unique territory navigation domain.");
				}
				return false;
			}
			SetIdentity(*NavBounds, TEXT("TerritoryNavigation"), Bundle, Profile, false);
		}
		return true;
	}

	bool CaptureAndCheckStructuralBudgets(
		UWorld* World,
		const FProjectWorldRuntimeProfile& Profile,
		bool bVegetationLayerSelected,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		if (Profile.ProfileKind != TEXT("territory_product"))
		{
			OutError = TEXT("Only territory product runtime profiles are supported.");
			return false;
		}
		return ProjectWorldTerritoryRuntimeAcceptance::CaptureAndCheck(
			World, Profile, bVegetationLayerSelected, OutResult, OutError);
	}
}
