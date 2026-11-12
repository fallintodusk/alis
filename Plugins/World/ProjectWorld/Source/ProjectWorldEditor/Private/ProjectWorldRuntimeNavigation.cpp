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

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldRuntimeNavigation, Log, All);

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

	bool ReconcileTerritoryData(UWorld* World, FString& OutError)
	{
		OutError.Reset();
		auto Refuse = [&OutError](const TCHAR* Message) { OutError = Message; return false; };
		auto* Navigation = World ? Cast<UNavigationSystemV1>(World->GetNavigationSystem()) : nullptr;
		if (!Navigation || !World->PersistentLevel || Navigation->GetWorld() != World || !Navigation->IsActiveTilesGenerationEnabled())
			return Refuse(TEXT("Map navigation reconciliation requires its owning invoker-only navigation system."));
		const FNavDataConfig& Config = Navigation->GetDefaultSupportedAgentConfig();
		if (!Config.IsValid() || !FMath::IsFinite(Config.AgentRadius) || !FMath::IsFinite(Config.AgentHeight) ||
			Config.AgentRadius <= 0 || Config.AgentHeight <= 0 || Config.GetNavDataClass<ANavigationData>() != ARecastNavMesh::StaticClass())
			return Refuse(TEXT("Map navigation reconciliation requires a valid supported stock Recast agent."));
		TArray<ARecastNavMesh*> Existing;
		for (TActorIterator<ARecastNavMesh> It(World); It; ++It) Existing.Add(*It);
		ANavigationData* Default = Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		if (Existing.IsEmpty())
		{
			ARecastNavMesh* Created = nullptr;
			return EnsureInternalData(World, Navigation, Created, OutError);
		}
		if (Existing.Num() != 1 || (Default && Default != Existing[0]))
			return Refuse(TEXT("Map navigation reconciliation refuses conflicting native data ownership."));
		ARecastNavMesh* Recast = Existing[0];
		if (Recast->GetClass() != ARecastNavMesh::StaticClass() || Recast->GetWorld() != World ||
			Recast->GetLevel() != World->PersistentLevel || Recast->IsPackageExternal())
			return Refuse(TEXT("Map navigation reconciliation requires one exact stock persistent internal actor."));
		const auto AgentMatches = [&]()
		{
			return Recast->GetConfig().Name == Config.Name && Recast->GetConfig().AgentRadius == Config.AgentRadius &&
				Recast->GetConfig().AgentHeight == Config.AgentHeight && Recast->AgentRadius == Config.AgentRadius &&
				Recast->AgentHeight == Config.AgentHeight;
		};
		if (Recast->IsRegistered() && !AgentMatches())
			return Refuse(TEXT("Registered map navigation has inconsistent agent bookkeeping; reconciliation refuses before mutation."));
		const auto* Defaults = GetDefault<ARecastNavMesh>();
		const float DefaultCell = Defaults->GetCellSize(ENavigationDataResolution::Default);
		const float Tile = FMath::Max(4096.f, Recast->TileSizeUU);
		if (!FMath::IsFinite(Recast->TileSizeUU) || Recast->TileSizeUU <= 0 || !FMath::IsFinite(DefaultCell) || DefaultCell < 1 || DefaultCell > 1024 ||
			Tile < FMath::Max3(float(RECAST_MIN_TILE_SIZE), 16 * DefaultCell, 4 * Config.AgentRadius) ||
			Tile > FMath::Max(float(RECAST_MIN_TILE_SIZE), 1024 * DefaultCell))
			return Refuse(TEXT("Map navigation reconciliation cannot preserve the tile under the accepted native resolution limits."));
		TArray<float> Desired;
		bool Changed = !Recast->IsRegistered() || !AgentMatches() || Recast->TileSizeUU != Tile;
		for (const auto R : {ENavigationDataResolution::Low, ENavigationDataResolution::Default, ENavigationDataResolution::High})
		{
			const float Cell = Defaults->GetCellSize(R), Height = Defaults->GetCellHeight(R);
			const float Step = Config.HasStepHeightOverride() ? Config.AgentStepHeight : Defaults->GetAgentMaxStepHeight(R);
			if (!FMath::IsFinite(Cell) || Cell < 1 || Cell > 1024 || !FMath::IsFinite(Height) || Height < 1 || Height > 1024 ||
				!FMath::IsFinite(Step) || Step < 0 || FMath::RoundToInt(Tile / Cell) < 1)
				return Refuse(TEXT("Map navigation reconciliation refuses invalid accepted resolution tiers."));
			const float Rematched = FMath::Clamp(Tile / float(FMath::RoundToInt(Tile / Cell)), 1.f, 1024.f);
			Desired.Append({Rematched, Height, Step});
			Changed |= Recast->GetCellSize(R) != Rematched || Recast->GetCellHeight(R) != Height || Recast->GetAgentMaxStepHeight(R) != Step;
		}
		FFloatProperty* TileProperty = FindFProperty<FFloatProperty>(ARecastNavMesh::StaticClass(), TEXT("TileSizeUU"));
		if (!TileProperty) return Refuse(TEXT("Map navigation reconciliation cannot access the native tile property."));
		if (!Changed)
		{
			ARecastNavMesh* Current = nullptr;
			return EnsureInternalData(World, Navigation, Current, OutError) && Current == Recast;
		}
		Navigation->UnregisterNavData(Recast);
		for (const auto R : {ENavigationDataResolution::Low, ENavigationDataResolution::Default, ENavigationDataResolution::High})
		{
			Recast->SetCellSize(R, Defaults->GetCellSize(R)); Recast->SetCellHeight(R, Defaults->GetCellHeight(R));
			Recast->SetAgentMaxStepHeight(R, Config.HasStepHeightOverride() ? Config.AgentStepHeight : Defaults->GetAgentMaxStepHeight(R));
		}
		Recast->SetConfig(Config); Recast->TileSizeUU = Tile;
		FPropertyChangedEvent Change(TileProperty); Recast->PostEditChangeProperty(Change);
		Navigation->RequestRegistrationDeferred(*Recast); Navigation->Tick(0.0f);
		ARecastNavMesh* Registered = nullptr;
		if (!EnsureInternalData(World, Navigation, Registered, OutError)) return false;
		if (Registered != Recast || !AgentMatches() || Recast->TileSizeUU != Tile)
			return Refuse(TEXT("Map navigation reconciliation did not retain the accepted native actor and agent."));
		int32 I = 0;
		for (const auto R : {ENavigationDataResolution::Low, ENavigationDataResolution::Default, ENavigationDataResolution::High})
			if (Recast->GetCellSize(R) != Desired[I++] || Recast->GetCellHeight(R) != Desired[I++] || Recast->GetAgentMaxStepHeight(R) != Desired[I++])
				return Refuse(TEXT("Map navigation reconciliation did not produce the accepted rematched resolution tiers."));
		Recast->RebuildAll(); Recast->EnsureBuildCompletion(); Recast->MarkPackageDirty();
		UE_LOG(LogProjectWorldRuntimeNavigation, Display,
			TEXT("[ReconcileTerritoryData] Reconciled map-owned navigation - world=%s actor=%s radius_cm=%g height_cm=%g tile_cm=%g"),
			*World->GetName(), *Recast->GetName(), Config.AgentRadius, Config.AgentHeight, Tile);
		return true;
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
			for (const auto& Quad : Cell.Terrain.RefinedQuads) for (const FVector& Vertex : Quad.Vertices)
			{
				if (Vertex.ContainsNaN()) { OutError = TEXT("Final terrain navigation bounds contain nonfinite refinement."); return false; }
				MinimumHeightMeters = FMath::Min(MinimumHeightMeters, Vertex.Z);
				MaximumHeightMeters = FMath::Max(MaximumHeightMeters, Vertex.Z);
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
		OutRecast = nullptr;
		OutError.Reset();
		if (!World || !Navigation || Navigation->GetWorld() != World)
		{
			OutError = TEXT("Navigation data requires its owning world and navigation system.");
			return false;
		}
		const FNavDataConfig& Config = Navigation->GetDefaultSupportedAgentConfig();
		if (!Config.IsValid() || !FMath::IsFinite(Config.AgentRadius) || !FMath::IsFinite(Config.AgentHeight) ||
			Config.AgentRadius <= 0 || Config.AgentHeight <= 0 || Config.GetNavDataClass<ANavigationData>() != ARecastNavMesh::StaticClass())
		{
			OutError = TEXT("Accepted route requires a valid supported stock Recast agent.");
			return false;
		}
		ANavigationData* Data = Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		TArray<ARecastNavMesh*> Existing;
		for (TActorIterator<ARecastNavMesh> It(World); It; ++It) Existing.Add(*It);
		if ((Data && (Existing.Num() != 1 || Existing[0] != Data)) || (!Data && !Existing.IsEmpty()))
		{
			OutError = TEXT("Navigation data census conflicts with its single map-owned default.");
			return false;
		}
		ANavigationData* Created = nullptr;
		if (!Data)
		{
			// The default creation overload uses fallback dimensions rather than the supported agent.
			Created = Navigation->CreateNavigationDataInstanceInLevel(Config, World->PersistentLevel);
			if (Created)
			{
				Navigation->RequestRegistrationDeferred(*Created);
				Navigation->Tick(0.0f);
				Data = Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
			}
		}
		ARecastNavMesh* Recast = Cast<ARecastNavMesh>(Data);
		if (!Recast || Recast->GetClass() != ARecastNavMesh::StaticClass() || !Recast->IsRegistered() || Recast->GetWorld() != World ||
			Recast->GetLevel() != World->PersistentLevel || (Created && Data != Created) ||
			Recast->GetConfig().Name != Config.Name || Recast->GetConfig().AgentRadius != Config.AgentRadius ||
			Recast->GetConfig().AgentHeight != Config.AgentHeight)
		{
			if (IsValid(Created))
			{
				Navigation->UnregisterNavData(Created);
				Created->CleanUpAndMarkPendingKill();
			}
			OutError = TEXT("Native navigation data is unregistered or differs from the supported agent.");
			return false;
		}
		if (Recast->IsPackageExternal())
		{
			Recast->SetPackageExternal(false);
		}
		OutRecast = Recast;
		return true;
	}
}
