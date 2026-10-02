// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimeNavigation.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRuntimeProfile.h"

#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Components/BrushComponent.h"
#include "EngineUtils.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationData.h"
#include "NavigationInvokerComponent.h"
#include "NavigationSystem.h"
#include "UObject/UnrealType.h"

namespace ProjectWorldRuntimeNavigation
{
	namespace
	{
		bool ConfigureVolume(
			ANavMeshBoundsVolume* NavBounds,
			const FVector& Center,
			const FRotator& Rotation,
			const FVector& Extent,
			FString& OutError)
		{
			if (NavBounds == nullptr || Extent.GetMin() <= 0.0)
			{
				OutError = TEXT("Navigation domain requires valid bounds and a bounds volume.");
				return false;
			}
			NavBounds->SetActorLocation(Center);
			NavBounds->SetActorRotation(Rotation);
			UCubeBuilder* Builder = NewObject<UCubeBuilder>();
			Builder->X = Extent.X * 2.0;
			Builder->Y = Extent.Y * 2.0;
			Builder->Z = Extent.Z * 2.0;
			Builder->Hollow = false;
			Builder->Tessellated = false;
			UActorFactory::CreateBrushForVolumeActor(NavBounds, Builder);
			if (UBrushComponent* Brush = NavBounds->GetBrushComponent())
			{
				Brush->UpdateBounds();
			}
			NavBounds->ReregisterAllComponents();
			return NavBounds->GetComponentsBoundingBox(true).IsValid != 0;
		}

		bool PrepareNavigation(
			UWorld* World,
			ANavMeshBoundsVolume* NavBounds,
			float MinimumTileSizeUU,
			bool bRequireInvokerOnly,
			UNavigationSystemV1*& OutNavigation,
			ARecastNavMesh*& OutRecast,
			FString& OutError)
		{
			OutNavigation = World != nullptr
				? Cast<UNavigationSystemV1>(World->GetNavigationSystem())
				: nullptr;
			if (OutNavigation == nullptr)
			{
				OutError = TEXT("World has no NavigationSystemV1 for the accepted navigation domain.");
				return false;
			}
			if (bRequireInvokerOnly && !OutNavigation->IsActiveTilesGenerationEnabled())
			{
				OutError = TEXT("Territory navigation requires invoker-only tile generation.");
				return false;
			}
			if (!EnsureInternalData(World, OutNavigation, OutRecast, OutError))
			{
				return false;
			}
			if (MinimumTileSizeUU > OutRecast->GetTileSizeUU())
			{
				FFloatProperty* TileSizeProperty = FindFProperty<FFloatProperty>(
					ARecastNavMesh::StaticClass(), TEXT("TileSizeUU"));
				if (TileSizeProperty == nullptr)
				{
					OutError = TEXT("Cannot configure the map-owned Recast tile size.");
					return false;
				}
				TileSizeProperty->SetPropertyValue_InContainer(OutRecast, MinimumTileSizeUU);
				FPropertyChangedEvent ChangeEvent(TileSizeProperty);
				OutRecast->PostEditChangeProperty(ChangeEvent);
				OutRecast->MarkPackageDirty();
			}
			if (OutNavigation->IsNavigationBuildingLocked(ENavigationBuildLock::AsyncLoadLock))
			{
				OutNavigation->RemoveNavigationBuildLock(
					ENavigationBuildLock::AsyncLoadLock,
					UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
			}
			OutNavigation->OnNavigationBoundsUpdated(NavBounds);
			OutNavigation->InitializeLevelCollisions();
			const FBox NavigationBox = NavBounds->GetComponentsBoundingBox(true);
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (*It != NavBounds && It->GetComponentsBoundingBox(true).Intersect(NavigationBox))
				{
					UNavigationSystemV1::UpdateActorAndComponentsInNavOctree(**It);
				}
			}
			OutNavigation->Tick(0.0f);
			return true;
		}
	}

	bool GetTerritoryDomainBounds(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile& Profile,
		FBox& OutBounds,
		FString& OutError)
	{
		OutBounds = FBox(ForceInit);
		if (Bundle.Cells.IsEmpty())
		{
			OutError = TEXT("Territory navigation requires canonical terrain cells.");
			return false;
		}
		double MinimumHeightMeters = TNumericLimits<double>::Max();
		double MaximumHeightMeters = TNumericLimits<double>::Lowest();
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			for (const double HeightMeters : Cell.Terrain.HeightsMeters)
			{
				MinimumHeightMeters = FMath::Min(MinimumHeightMeters, HeightMeters);
				MaximumHeightMeters = FMath::Max(MaximumHeightMeters, HeightMeters);
			}
			OutBounds += FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle, FVector(Cell.Bounds.X, Cell.Bounds.Y, 0.0));
			OutBounds += FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle, FVector(Cell.Bounds.Z, Cell.Bounds.W, 0.0));
		}
		if (!FMath::IsFinite(MinimumHeightMeters) || !FMath::IsFinite(MaximumHeightMeters))
		{
			OutError = TEXT("Territory navigation cannot derive a finite terrain height range.");
			return false;
		}
		const double MinimumZ = (MinimumHeightMeters - Bundle.HeightOriginMeters) * 100.0;
		const double MaximumZ = (MaximumHeightMeters - Bundle.HeightOriginMeters) * 100.0;
		const double HorizontalPadding = Profile.NavigationPaddingMeters * 100.0;
		const double VerticalPadding = FMath::Max(
			Profile.NavigationPaddingMeters * 100.0,
			Profile.NavigationHeightMeters * 50.0);
		OutBounds.Min -= FVector(HorizontalPadding, HorizontalPadding, 0.0);
		OutBounds.Max += FVector(HorizontalPadding, HorizontalPadding, 0.0);
		OutBounds.Min.Z = MinimumZ - VerticalPadding;
		OutBounds.Max.Z = MaximumZ + VerticalPadding;
		return OutBounds.IsValid != 0;
	}

	bool ConfigureTerritoryDomain(
		UWorld* World,
		ANavMeshBoundsVolume* NavBounds,
		const FBox& DomainBounds,
		ARecastNavMesh*& OutRecast,
		FString& OutError)
	{
		if (!DomainBounds.IsValid || !ConfigureVolume(
			NavBounds,
			DomainBounds.GetCenter(),
			FRotator::ZeroRotator,
			DomainBounds.GetExtent(),
			OutError))
		{
			return false;
		}
		return EnsureTerritoryDomain(World, NavBounds, OutRecast, OutError);
	}

	bool EnsureTerritoryDomain(
		UWorld* World,
		ANavMeshBoundsVolume* NavBounds,
		ARecastNavMesh*& OutRecast,
		FString& OutError)
	{
		UNavigationSystemV1* Navigation = nullptr;
		return PrepareNavigation(World, NavBounds, 4096.0f, true, Navigation, OutRecast, OutError);
	}

	bool ConfigureRouteDomain(
		UWorld* World,
		AActor* InvokerOwner,
		ANavMeshBoundsVolume* NavBounds,
		const FVector& Start,
		const FVector& End,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		ANavigationData*& OutNavigationData,
		FString& OutError)
	{
		if (InvokerOwner == nullptr)
		{
			OutError = TEXT("Route navigation requires an invoker owner.");
			return false;
		}
		UNavigationInvokerComponent* Invoker = InvokerOwner != nullptr
			? InvokerOwner->FindComponentByClass<UNavigationInvokerComponent>()
			: nullptr;
		if (Invoker == nullptr)
		{
			Invoker = NewObject<UNavigationInvokerComponent>(
				InvokerOwner, TEXT("RouteNavigationInvoker"), RF_Transactional);
			InvokerOwner->AddInstanceComponent(Invoker);
			Invoker->RegisterComponent();
		}
		const float GenerationRadius = FVector::Distance(Start, End) +
			Profile.NavigationPaddingMeters * 100.0;
		Invoker->SetGenerationRadii(
			GenerationRadius,
			GenerationRadius + Profile.NavigationPaddingMeters * 100.0);

		const FVector RouteCenter = (Start + End) * 0.5;
		const FVector2D RouteDelta(End.X - Start.X, End.Y - Start.Y);
		const double RouteYawDegrees = FMath::RadiansToDegrees(FMath::Atan2(RouteDelta.Y, RouteDelta.X));
		const FVector RouteExtent(
			RouteDelta.Size() * 0.5 + Profile.NavigationPaddingMeters * 100.0,
			Profile.NavigationPaddingMeters * 100.0,
			Profile.NavigationHeightMeters * 50.0);
		if (!ConfigureVolume(
			NavBounds,
			RouteCenter,
			FRotator(0.0, RouteYawDegrees, 0.0),
			RouteExtent,
			OutError))
		{
			return false;
		}
		OutResult.RuntimeRouteVolumeYawDegrees = RouteYawDegrees;

		UNavigationSystemV1* Navigation = nullptr;
		ARecastNavMesh* Recast = nullptr;
		if (!PrepareNavigation(World, NavBounds, 0.0f, false, Navigation, Recast, OutError))
		{
			return false;
		}
		Invoker->RegisterWithNavigationSystem(*Navigation);
		FBoolProperty* InvokerOnlyProperty = FindFProperty<FBoolProperty>(
			UNavigationSystemV1::StaticClass(),
			TEXT("bGenerateNavigationOnlyAroundNavigationInvokers"));
		if (InvokerOnlyProperty == nullptr)
		{
			OutError = TEXT("Cannot access the UE invoker-only navigation setting.");
			return false;
		}
		const bool bInvokerOnly = Navigation->IsActiveTilesGenerationEnabled();
		if (bInvokerOnly)
		{
			InvokerOnlyProperty->SetPropertyValue_InContainer(Navigation, false);
		}
		Navigation->Build();
		Recast->EnsureBuildCompletion();
		if (bInvokerOnly)
		{
			InvokerOnlyProperty->SetPropertyValue_InContainer(Navigation, true);
			Recast->UpdateActiveTiles(Navigation->GetInvokerLocations());
		}
		TArray<FBox> RegisteredNavigationBounds;
		Navigation->GetNavigationBoundsForNavData(*Recast, RegisteredNavigationBounds);
		if (RegisteredNavigationBounds.IsEmpty() || Navigation->GetInvokerLocations().IsEmpty())
		{
			OutError = TEXT("Navigation did not retain the generated route domain and invoker.");
			return false;
		}
		OutNavigationData = Recast;
		return true;
	}

	bool EnsureInternalData(
		UWorld* World,
		UNavigationSystemV1* Navigation,
		ARecastNavMesh*& OutRecast,
		FString& OutError)
	{
		OutRecast = Cast<ARecastNavMesh>(
			Navigation->GetDefaultNavDataInstance(FNavigationSystem::Create));
		if (OutRecast == nullptr)
		{
			OutError = TEXT("Accepted route requires stock Recast navigation data.");
			return false;
		}
		if (OutRecast->IsPackageExternal())
		{
			OutRecast->SetPackageExternal(false);
		}
		return true;
	}
}
