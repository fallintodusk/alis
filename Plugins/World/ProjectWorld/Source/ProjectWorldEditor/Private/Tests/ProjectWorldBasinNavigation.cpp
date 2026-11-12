// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldBasinNavigation.h"
#include "ProjectWorldRuntimeNavigation.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldRealizationService.h"
#include "Components/CapsuleComponent.h"
#include "Components/BrushComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/ScopeExit.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshPath.h"
#include "NavigationData.h"
#include "NavigationInvokerComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinNavigation
{
    namespace
    {
        struct FDetail
        {
            int32 Covers=0,Unknown=0,Clamped=0;
            double Low=DBL_MAX,High=-DBL_MAX;
            TArray<NavNodeRef,TInlineAllocator<4>> CoverRefs;
        };
        FDetail DetailAt(const ARecastNavMesh& Recast,TConstArrayView<NavNodeRef> Corridor,const FVector& P,int32& Queries)
        {
            FDetail Out;const FVector From(P.X,P.Y,P.Z-1000),To(P.X,P.Y,P.Z+1000);
            if (Corridor.IsEmpty() || Corridor.Num()>4096 || From.ContainsNaN() || To.ContainsNaN() || From.Z==To.Z)
            {++Out.Unknown;return Out;}
            for (NavNodeRef Poly : Corridor)
            {
                if (Queries>1048576-2) {++Out.Unknown;return Out;}
                FVector A,B;const bool HasA=Recast.GetClosestPointOnPoly(Poly,From,A),HasB=Recast.GetClosestPointOnPoly(Poly,To,B);Queries+=2;
                if (!HasA || !HasB || A.ContainsNaN() || B.ContainsNaN()) {++Out.Unknown;continue;}
                const bool SameA=A.X==P.X && A.Y==P.Y,SameB=B.X==P.X && B.Y==P.Y;
                if (!SameA && !SameB) {++Out.Clamped;continue;}
                // Native success can leave input Z unchanged when detail height is absent.
                if (!SameA || !SameB || A!=B) {++Out.Unknown;continue;}
                ++Out.Covers;Out.CoverRefs.Add(Poly);Out.Low=FMath::Min(Out.Low,A.Z);Out.High=FMath::Max(Out.High,A.Z);
            }
            return Out;
        }
        TSharedRef<FJsonObject> Describe(const FDetail& Value,double Ground)
        {
            auto Out=MakeShared<FJsonObject>();Out->SetNumberField(TEXT("same_xy_independent_height_candidates"),Value.Covers);
            Out->SetNumberField(TEXT("unknown_candidates"),Value.Unknown);Out->SetNumberField(TEXT("xy_clamped_candidates"),Value.Clamped);
            if (Value.Covers) {Out->SetNumberField(TEXT("minimum_height_cm"),Value.Low);Out->SetNumberField(TEXT("maximum_height_cm"),Value.High);
                Out->SetNumberField(TEXT("maximum_ground_offset_cm"),FMath::Max(FMath::Abs(Value.Low-Ground),FMath::Abs(Value.High-Ground)));}
            return Out;
        }
        bool FitsGround(const FDetail& Value,double Ground)
        {
            return Value.Unknown==0 && Value.Covers>0 && FMath::IsFinite(Ground) &&
                FMath::Max(FMath::Abs(Value.Low-Ground),FMath::Abs(Value.High-Ground))<=30;
        }
    }
    bool CheckAgent(UWorld& World,ACharacter& Character,FJsonObject& Receipt,FString& Error)
    {
        Error.Reset();auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
        if (!Navigation || Character.GetWorld()!=&World) {Error=TEXT("Hero agent check has no owning native navigation system.");return false;}
        ARecastNavMesh* Resolved=nullptr;
        if (!ProjectWorldRuntimeNavigation::EnsureInternalData(&World,Navigation,Resolved,Error)) return false;
        const auto& Agent=Character.GetNavAgentPropertiesRef();
        const auto* Data=Navigation->GetNavDataForProps(Agent,Character.GetActorLocation());
        if (!Data || Data!=Resolved || !Data->IsRegistered()) {Error=TEXT("Native navigation cannot select registered data for the actual Hero agent.");return false;}
        const auto& Config=Data->GetConfig();const auto* Capsule=Character.GetCapsuleComponent();
        Receipt.SetNumberField(TEXT("navigation_agent_radius_cm"),Agent.AgentRadius);
        Receipt.SetNumberField(TEXT("navigation_agent_height_cm"),Agent.AgentHeight);
        Receipt.SetNumberField(TEXT("navigation_data_radius_cm"),Config.AgentRadius);
        Receipt.SetNumberField(TEXT("navigation_data_height_cm"),Config.AgentHeight);
        Receipt.SetStringField(TEXT("navigation_data_class"),Data->GetClass()->GetPathName());
        const auto Positive=[](double V){return FMath::IsFinite(V) && V>0;};
        if (!Positive(Agent.AgentRadius) || !Positive(Agent.AgentHeight) || !Positive(Config.AgentRadius) || !Positive(Config.AgentHeight) ||
            !Positive(Capsule->GetScaledCapsuleRadius()) || !Positive(Capsule->GetScaledCapsuleHalfHeight()) ||
            Config.AgentRadius<Capsule->GetScaledCapsuleRadius() || Config.AgentHeight<2*Capsule->GetScaledCapsuleHalfHeight())
        {Error=TEXT("Native selected navigation data does not conservatively contain the actual Hero capsule.");return false;}
        return true;
    }
    bool Prepare(ACharacter& Character,TConstArrayView<TArray<FVector>> Routes,UActorComponent*& OwnedInvoker,FString& Error)
    {
        OwnedInvoker=nullptr;Error.Reset();FBox Domain(ForceInit);
        for (const auto& Route : Routes) for (const auto& P : Route)
        {if (P.ContainsNaN()) {Error=TEXT("Navigation invoker domain is nonfinite.");return false;}Domain+=P;}
        if (!Domain.IsValid || Routes.IsEmpty()) {Error=TEXT("Navigation invoker requires explicit routes.");return false;}
        if (auto* Existing=Character.FindComponentByClass<UNavigationInvokerComponent>())
        {if (Existing->IsRegistered()) return true;Error=TEXT("Existing navigation invoker is not registered.");return false;}
        auto* Invoker=NewObject<UNavigationInvokerComponent>(&Character,NAME_None,RF_Transient);
        Character.AddInstanceComponent(Invoker);const double Radius=Domain.GetSize().Length()*2+2000;
        Invoker->SetGenerationRadii(Radius,Radius+1000);Invoker->RegisterComponent();OwnedInvoker=Invoker;
        if (!Invoker->IsRegistered()) {Error=TEXT("Cannot register the owned navigation invoker.");return false;}
        return true;
    }
    bool Verify(UWorld& World,ACharacter& Character,const FProjectWorldCanonicalBundle& Bundle,TConstArrayView<TArray<FVector>> Routes,
        TConstArrayView<UPrimitiveComponent*> Terrain,TFunctionRef<bool(const FVector&,double&)> Height,
        FJsonObject& Receipt,FString& Error,TFunction<bool(bool,FString&)> ConnectorControl)
    {
        Error.Reset();
        auto Fail=[&](const TCHAR* Message){Error=Message;return false;};
        if (World.WorldType!=EWorldType::PIE || Character.GetWorld()!=&World || !Character.IsLocallyControlled() ||
            Routes.IsEmpty() || Routes.Num()>32 || Terrain.IsEmpty()) return Fail(TEXT("Navigation requires bounded routes and actual possessed saved-scene support."));
        FBox Box(ForceInit);int32 ExpectedSegments=0;
        for (const auto& Route : Routes)
        {
            if (Route.Num()<2 || Route.Num()>256) return Fail(TEXT("Navigation route node census is invalid."));
            ExpectedSegments+=2*(Route.Num()-1);
            for (const auto& P : Route) {if (P.ContainsNaN()) return Fail(TEXT("Navigation route contains nonfinite coordinates."));Box+=P;}
        }
        TSet<UPrimitiveComponent*> Support;TArray<ECollisionEnabled::Type> Collision;
        for (auto* Component : Terrain)
        {
            if (!Component || Component->GetWorld()!=&World || !Component->IsRegistered() || Support.Contains(Component) ||
                !Component->CanEverAffectNavigation() || !Component->IsQueryCollisionEnabled())
                return Fail(TEXT("Navigation support inventory is unknown, duplicated or nonphysical."));
            Support.Add(Component);Collision.Add(Component->GetCollisionEnabled());
        }
        auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
        if (!Navigation) return Fail(TEXT("Saved PIE has no native navigation system."));
        auto* Configured=Cast<ARecastNavMesh>(Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
        if (!Configured || !Configured->IsRegistered()) return Fail(TEXT("Native approximation measurement requires the prepared registered data."));
        const float PreviousRatio=Configured->SimplificationElevationRatio;
        const float PreviousTile=Configured->TileSizeUU;
        if (!FMath::IsFinite(PreviousRatio) || PreviousRatio<0) return Fail(TEXT("The original native elevation ratio is unknown or invalid."));
        const FString RatioOption=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_NAV_ELEVATION_RATIO"));
        const FString TileOption=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_NAV_TILE_SIZE_CM"));
        const FString TerritoryProfile=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_NAV_TERRITORY_PROFILE"));
        const FString TerritoryHash=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_NAV_TERRITORY_PROFILE_SHA256"));
        const FString HeightOption=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_NAV_CELL_HEIGHT_CM"));
        const bool Territory=!TerritoryProfile.IsEmpty();
        if (!HeightOption.IsEmpty() && (!Territory || (HeightOption!=TEXT("5") && HeightOption!=TEXT("10"))))
            return Fail(TEXT("The isolated vertical-resolution comparison requires the territory helper and declared 5 or 10 cm arm."));
        FProjectWorldRuntimeProfile Profile;
        FBox TerritoryBounds(ForceInit);
        if (Territory)
        {
            FString Code;
            if ((!TileOption.IsEmpty() && TileOption!=TEXT("4180")) || !RatioOption.IsEmpty() || TerritoryHash.IsEmpty())
                return Fail(TEXT("The territory helper requires a pinned profile and only the explicitly bounded cell-aligned tile candidate."));
            if (!ProjectWorldRuntimeProfile::Load(TerritoryProfile,Profile,Code,Error)) return false;
            if (Profile.ProfileHash!=TerritoryHash) return Fail(TEXT("The production navigation-helper profile differs from its frozen pin."));
            if (!ProjectWorldRuntimeNavigation::GetTerritoryDomainBounds(Bundle,Profile,TerritoryBounds,Error)) return false;
            if (!Navigation->IsActiveTilesGenerationEnabled()) return Fail(TEXT("The production navigation-helper arm requires invoker-only generation."));
            Receipt.SetStringField(TEXT("navigation_territory_profile_id"),Profile.ProfileId);
            Receipt.SetStringField(TEXT("navigation_territory_profile_sha256"),Profile.ProfileHash);
            Receipt.SetStringField(TEXT("navigation_territory_profile_grid"),Profile.GridId);
            Receipt.SetStringField(TEXT("navigation_reduced_bundle_grid"),Bundle.GridId);
            Receipt.SetStringField(TEXT("navigation_domain_scope"),TEXT("Actual production navigation helper and exact profile padding on the reduced frozen Bundle; no complete profile/Apply, route/spawn, original cell or production territory acceptance."));
            TArray<TSharedPtr<FJsonValue>> Domain;
            for (double V : {TerritoryBounds.Min.X,TerritoryBounds.Min.Y,TerritoryBounds.Min.Z,TerritoryBounds.Max.X,TerritoryBounds.Max.Y,TerritoryBounds.Max.Z})
                Domain.Add(MakeShared<FJsonValueNumber>(V));
            Receipt.SetArrayField(TEXT("navigation_territory_bounds_cm"),Domain);
        }
        else if (!TerritoryHash.IsEmpty()) return Fail(TEXT("A territory profile pin without its input is invalid."));
        if (!RatioOption.IsEmpty() && !TileOption.IsEmpty()) return Fail(TEXT("Navigation approximation comparisons change one setting at a time."));
        if (!TileOption.IsEmpty() && TileOption!=TEXT("300") && TileOption!=TEXT("1000") && TileOption!=TEXT("4180"))
            return Fail(TEXT("The isolated tile comparison must select a declared native arm."));
        if (TileOption==TEXT("4180") && (!Territory || !HeightOption.IsEmpty() ||
            Configured->GetCellSize(ENavigationDataResolution::Default)!=5 || Configured->GetCellSize(ENavigationDataResolution::High)!=5 ||
            Configured->GetCellSize(ENavigationDataResolution::Low)!=38))
            return Fail(TEXT("The cell-aligned territory comparison requires original 5/38 cm horizontal tiers and no vertical change."));
        const float Tile=TileOption.IsEmpty() ? PreviousTile : TileOption==TEXT("300") ? 300 : TileOption==TEXT("4180") ? 4180 : 1000;
        const float Cell=Configured->GetCellSize(ENavigationDataResolution::Default),Radius=Configured->GetConfig().AgentRadius;
        if (!FMath::IsFinite(PreviousTile) || PreviousTile<=0 || !FMath::IsFinite(Cell) || Cell<=0 ||
            Tile<FMath::Max3(float(RECAST_MIN_TILE_SIZE),16*Cell,4*Radius) || Tile>FMath::Max(float(RECAST_MIN_TILE_SIZE),1024*Cell))
            return Fail(TEXT("The tile comparison is outside the native supported size bounds."));
        const float EffectiveTile=FMath::TruncToFloat(Tile/Cell)*Cell;
        float Ratio=PreviousRatio;
        if (!RatioOption.IsEmpty() && RatioOption!=TEXT("0") && RatioOption!=TEXT("1"))
            return Fail(TEXT("A disposable navigation ratio comparison must explicitly select zero or one."));
        if (!RatioOption.IsEmpty()) Ratio=RatioOption==TEXT("1") ? 1 : 0;
        Receipt.SetNumberField(TEXT("navigation_cell_size_cm"),Configured->GetCellSize(ENavigationDataResolution::Default));
        Receipt.SetStringField(TEXT("navigation_setting_observation_phase"),TEXT("Before domain-helper mutation; per-build rows own the effective built settings."));
        Receipt.SetNumberField(TEXT("navigation_cell_height_cm"),Configured->GetCellHeight(ENavigationDataResolution::Default));
        Receipt.SetNumberField(TEXT("navigation_maximum_simplification_error"),Configured->MaxSimplificationError);
        Receipt.SetNumberField(TEXT("navigation_maximum_agent_slope"),Configured->AgentMaxSlope);
        Receipt.SetNumberField(TEXT("navigation_elevation_ratio_before"),PreviousRatio);
        Receipt.SetNumberField(TEXT("navigation_tile_size_raw_before_cm"),PreviousTile);
        Receipt.SetNumberField(TEXT("navigation_tile_size_effective_before_cm"),Configured->GetTileSizeUU());
        TArray<float> Tiers;
        for (const auto R : {ENavigationDataResolution::Low,ENavigationDataResolution::Default,ENavigationDataResolution::High})
        {Tiers.Add(Configured->GetCellSize(R));Tiers.Add(Configured->GetCellHeight(R));Tiers.Add(Configured->GetAgentMaxStepHeight(R));}
        ON_SCOPE_EXIT
        {
            if (IsValid(Configured)) {Configured->SimplificationElevationRatio=PreviousRatio;Configured->TileSizeUU=PreviousTile;
                int32 I=0;
                TArray<TSharedPtr<FJsonValue>> RestoredTiers;
                for (const auto R : {ENavigationDataResolution::Low,ENavigationDataResolution::Default,ENavigationDataResolution::High})
                {
                    Configured->SetCellSize(R,Tiers[I++]);Configured->SetCellHeight(R,Tiers[I++]);Configured->SetAgentMaxStepHeight(R,Tiers[I++]);
                    for (float V : {Configured->GetCellSize(R),Configured->GetCellHeight(R),Configured->GetAgentMaxStepHeight(R)})
                        RestoredTiers.Add(MakeShared<FJsonValueNumber>(V));
                }
                Receipt.SetArrayField(TEXT("navigation_resolution_tiers_restored"),RestoredTiers);
                Receipt.SetNumberField(TEXT("navigation_elevation_ratio_restored"),Configured->SimplificationElevationRatio);
                Receipt.SetNumberField(TEXT("navigation_tile_size_raw_restored_cm"),Configured->TileSizeUU);}
            else Receipt.SetStringField(TEXT("navigation_ratio_restore_status"),TEXT("owned_data_absent"));
        };
        if (!RatioOption.IsEmpty())
        {
            Configured->SimplificationElevationRatio=Ratio;
            Receipt.SetNumberField(TEXT("navigation_candidate_elevation_ratio"),Ratio);
            Receipt.SetStringField(TEXT("navigation_candidate_scope"),TEXT("Unsaved native ratio comparison only; original actor property restored, generated tiles remain candidate until disposable World teardown."));
        }
        if (!TileOption.IsEmpty())
        {
            Configured->TileSizeUU=Tile;Receipt.SetNumberField(TEXT("navigation_candidate_tile_size_cm"),Tile);
            Receipt.SetStringField(TEXT("navigation_candidate_scope"),TEXT("Unsaved supported native tile-size comparison only; all resolution tiers and source inputs retained. Actor properties restored, generated tiles remain candidate until disposable World teardown."));
            if (Territory) Receipt.SetStringField(TEXT("navigation_candidate_tile_basis"),TEXT("4180 is the first multiple of both original 5 and 38 cm horizontal cells above the existing 4096 cm minimum. The actual territory helper retains this supported candidate; no policy adoption."));
        }
        if (!HeightOption.IsEmpty())
        {
            if (Configured->GetCellHeight(ENavigationDataResolution::Default)!=10 || Configured->GetCellHeight(ENavigationDataResolution::High)!=10 ||
                Configured->GetCellHeight(ENavigationDataResolution::Low)!=10)
                return Fail(TEXT("The vertical-resolution comparison requires the declared original 10 cm tiers."));
            const float HeightCm=HeightOption==TEXT("5") ? 5 : 10;
            Configured->SetCellHeight(ENavigationDataResolution::Default,HeightCm);Configured->SetCellHeight(ENavigationDataResolution::High,HeightCm);
            Receipt.SetNumberField(TEXT("navigation_candidate_fine_cell_height_cm"),HeightCm);
            Receipt.SetStringField(TEXT("navigation_candidate_scope"),TEXT("Unsaved vertical-resolution concept changes two fine-tier heights only. Low height, production territory helper, XY settings, agent and nominal step remain. Original actor properties restore; candidate tiles remain until disposable World teardown."));
        }
        Receipt.SetNumberField(TEXT("navigation_elevation_ratio_effective"),Configured->SimplificationElevationRatio);
        Receipt.SetNumberField(TEXT("navigation_tile_size_raw_effective_cm"),Configured->TileSizeUU);
        Receipt.SetNumberField(TEXT("navigation_tile_size_effective_cm"),Configured->GetTileSizeUU());
        auto* Bounds=World.SpawnActor<ANavMeshBoundsVolume>();
        if (!Bounds) return Fail(TEXT("Cannot create the disposable route bounds."));
        Bounds->GetBrushComponent()->SetMobility(EComponentMobility::Movable);
        auto* PreviousInvoker=Character.FindComponentByClass<UNavigationInvokerComponent>();
        if (!PreviousInvoker || !PreviousInvoker->IsRegistered() || Navigation->GetInvokerLocations().IsEmpty())
        {Bounds->Destroy();return Fail(TEXT("Navigation requires its prepared invoker after normal native frames."));}
        const float PreviousGeneration=PreviousInvoker ? PreviousInvoker->GetGenerationRadius() : 0;
        const float PreviousRemoval=PreviousInvoker ? PreviousInvoker->GetRemovalRadius() : 0;
        ON_SCOPE_EXIT
        {
            for (int32 I=0;I<Terrain.Num();++I) Terrain[I]->SetCollisionEnabled(Collision[I]);
            if (auto* Invoker=Character.FindComponentByClass<UNavigationInvokerComponent>())
            {
                if (Invoker==PreviousInvoker) Invoker->SetGenerationRadii(PreviousGeneration,PreviousRemoval);
                else Invoker->DestroyComponent();
            }
            Bounds->Destroy();
        };
        if (!Territory) {Profile.NavigationPaddingMeters=Box.GetSize().Length()*.01+10;Profile.NavigationHeightMeters=10;}
        ANavigationData* Data=nullptr;
        TArray<TSharedPtr<FJsonValue>> Builds;
        auto Build=[&]()
        {
            FProjectWorldRealizationResult Result;
            const double Started=FPlatformTime::Seconds();
            if (Territory)
            {
                ARecastNavMesh* Resolved=nullptr;
                if (!ProjectWorldRuntimeNavigation::ConfigureTerritoryDomain(&World,Bounds,TerritoryBounds,Resolved,Error)) return false;
                Data=Resolved;
                if (!Navigation->IsActiveTilesGenerationEnabled() || Navigation->GetInvokerLocations().IsEmpty())
                {Error=TEXT("Production navigation helper lost invoker-only generation or the actual prepared invoker.");return false;}
                Navigation->Build();
                Resolved->UpdateActiveTiles(Navigation->GetInvokerLocations());Resolved->EnsureBuildCompletion();
                if (!Navigation->IsActiveTilesGenerationEnabled() || Resolved->GetTileSizeUU()<4096)
                {Error=TEXT("Production navigation helper did not retain its invoker policy and minimum tile size.");return false;}
                if (!HeightOption.IsEmpty())
                {
                    const float Expected=HeightOption==TEXT("5") ? 5 : 10;
                    if (Resolved->GetCellHeight(ENavigationDataResolution::Default)!=Expected || Resolved->GetCellHeight(ENavigationDataResolution::High)!=Expected ||
                        Resolved->GetCellHeight(ENavigationDataResolution::Low)!=10)
                    {Error=TEXT("Production navigation helper changed the bound vertical-resolution arm.");return false;}
                }
            }
            else if (!ProjectWorldRuntimeNavigation::ConfigureRouteDomain(&World,&Character,Bounds,Box.Min,Box.Max,Profile,Result,Data,Error)) return false;
            if (Data!=Configured || !Data->IsRegistered() || Configured->SimplificationElevationRatio!=Ratio ||
                ((!Territory || !TileOption.IsEmpty()) && (Configured->TileSizeUU!=Tile || Configured->GetTileSizeUU()!=EffectiveTile)))
            {Error=TEXT("Native rebuild changed the bound data actor or requested effective ratio.");return false;}
            int32 I=0;
            if (!Territory || !TileOption.IsEmpty())
                for (const auto R : {ENavigationDataResolution::Low,ENavigationDataResolution::Default,ENavigationDataResolution::High})
                    if (Configured->GetCellSize(R)!=Tiers[I++] || Configured->GetCellHeight(R)!=Tiers[I++] || Configured->GetAgentMaxStepHeight(R)!=Tiers[I++])
                    {Error=TEXT("The tile comparison unexpectedly changed a resolution tier.");return false;}
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
            Row->SetBoolField(TEXT("invoker_only_after_build"),Navigation->IsActiveTilesGenerationEnabled());
            Row->SetNumberField(TEXT("tile_size_raw_cm"),Configured->TileSizeUU);Row->SetNumberField(TEXT("tile_size_effective_cm"),Configured->GetTileSizeUU());
            TArray<TSharedPtr<FJsonValue>> EffectiveTiers;
            for (const auto R : {ENavigationDataResolution::Low,ENavigationDataResolution::Default,ENavigationDataResolution::High})
                for (float V : {Configured->GetCellSize(R),Configured->GetCellHeight(R),Configured->GetAgentMaxStepHeight(R)})
                    EffectiveTiers.Add(MakeShared<FJsonValueNumber>(V));
            Row->SetArrayField(TEXT("effective_resolution_tiers"),EffectiveTiers);
            const float HeightCm=Configured->GetCellHeight(ENavigationDataResolution::Default);
            Row->SetNumberField(TEXT("derived_default_voxel_agent_height_cm"),FMath::CeilToInt(Configured->GetConfig().AgentHeight/HeightCm)*HeightCm);
            Row->SetNumberField(TEXT("derived_default_voxel_climb_cm"),FMath::CeilToInt(Configured->GetAgentMaxStepHeight(ENavigationDataResolution::Default)/HeightCm)*HeightCm);
            Row->SetNumberField(TEXT("active_tiles"),Configured->GetNumActiveTiles());Row->SetNumberField(TEXT("tile_pool_capacity"),Configured->GetNavMeshTilesCount());
            Builds.Add(MakeShared<FJsonValueObject>(Row));Receipt.SetArrayField(TEXT("navigation_build_passes"),Builds);
            if (Configured->GetNavMeshTilesCount()>4096)
            {Row->SetStringField(TEXT("polygon_census_status"),TEXT("not_measured_capacity_bound"));return true;}
            TArray<FNavTileRef> Tiles;Configured->GetAllNavMeshTiles(Tiles);int32 Polygons=0,GroundTiles=0;
            if (Tiles.Num()>4096 || Tiles.Num()>Configured->GetNavMeshTilesCount())
            {Row->SetStringField(TEXT("polygon_census_status"),TEXT("unknown_native_reference_count"));return true;}
            for (const auto Ref : Tiles) {TArray<FNavPoly> Values;if (Configured->GetPolysInTile(Ref,Values)) {++GroundTiles;Polygons+=Values.Num();}}
            Row->SetNumberField(TEXT("allocated_tile_refs"),Tiles.Num());Row->SetNumberField(TEXT("ground_tiles"),GroundTiles);
            if (GroundTiles!=Configured->GetNumActiveTiles())
            {Row->SetStringField(TEXT("polygon_census_status"),TEXT("unknown_ground_tile_coverage"));return true;}
            Row->SetNumberField(TEXT("ground_polygons"),Polygons);Row->SetStringField(TEXT("polygon_census_status"),TEXT("complete"));
            return true;
        };
        if (!Build()) return false;
        const auto& Agent=Character.GetNavAgentPropertiesRef();
        if (!CheckAgent(World,Character,Receipt,Error)) return false;
        if (Navigation->GetNavDataForProps(Agent,Character.GetActorLocation())!=Data)
            return Fail(TEXT("Built route data differs from the actual Hero's selected navigation data."));
        auto* Recast=Cast<ARecastNavMesh>(Data);
        if (!Recast) return Fail(TEXT("Actual corridor details require the selected native Recast data."));
        int32 DetailQueries=0;double MaximumChord=0;bool DetailControls=false;
        auto Paths=[&](const TCHAR* Phase,int32& Segments,int32& Samples,double& MaximumError,FString& Refusal)
        {
            Segments=0;Samples=0;MaximumError=0;DetailQueries=0;MaximumChord=0;Refusal.Reset();
            for (const auto& Route : Routes) for (int32 I=1;I<Route.Num();++I) for (int32 Direction : {1,-1})
            {
                const FVector Start=Route[Direction>0 ? I-1 : I],End=Route[Direction>0 ? I : I-1];
                FNavLocation From,To;
                const bool HasFrom=Navigation->ProjectPointToNavigation(Start,From,FVector(50,50,30),Data);
                const bool HasTo=Navigation->ProjectPointToNavigation(End,To,FVector(50,50,30),Data);
                if (!HasFrom || !HasTo)
                {
                    const FVector P=HasFrom ? End : Start;auto Witness=MakeShared<FJsonObject>();
                    Witness->SetStringField(TEXT("phase"),Phase);
                    auto Triple=[](const FVector& V) {TArray<TSharedPtr<FJsonValue>> Out;
                        for (double N : {V.X,V.Y,V.Z}) Out.Add(MakeShared<FJsonValueNumber>(N));return Out;};
                    Witness->SetArrayField(TEXT("requested_endpoint_cm"),Triple(P));
                    Witness->SetNumberField(TEXT("accepted_projection_vertical_extent_cm"),30);
                    Witness->SetNumberField(TEXT("diagnostic_projection_vertical_extent_cm"),10000);
                    FNavLocation Wide;const bool HasWide=Navigation->ProjectPointToNavigation(P,Wide,FVector(50,50,10000),Data);
                    Witness->SetBoolField(TEXT("diagnostic_projection_found"),HasWide);
                    double Z;const bool HasGround=Height(P,Z) && FMath::IsFinite(Z);
                    if (HasGround) Witness->SetNumberField(TEXT("canonical_ground_cm"),Z);
                    if (HasWide && !Wide.Location.ContainsNaN())
                    {
                        Witness->SetArrayField(TEXT("diagnostic_projected_cm"),Triple(Wide.Location));
                        Witness->SetNumberField(TEXT("diagnostic_xy_departure_cm"),FVector2D::Distance(FVector2D(P),FVector2D(Wide.Location)));
                        int32 Queries=0;const TArray<NavNodeRef> Refs={Wide.NodeRef};
                        const auto Detail=DetailAt(*Recast,Refs,P,Queries);
                        if (HasGround) Witness->SetObjectField(TEXT("diagnostic_one_ref_same_xy_detail"),Describe(Detail,Z));
                    }
                    Witness->SetStringField(TEXT("scope"),TEXT("Diagnostic only; the original 30 cm projection refusal remains. A wider query does not satisfy route acceptance."));
                    Receipt.SetObjectField(TEXT("navigation_projection_refusal_diagnostic"),Witness);
                    Refusal=TEXT("projection_missing");return false;
                }
                if (From.Location.ContainsNaN() || To.Location.ContainsNaN()) {Refusal=TEXT("nonfinite_projection");return false;}
                FPathFindingQuery Query(&Character,*Data,From.Location,To.Location);Query.SetAllowPartialPaths(false);
                const auto Found=Navigation->FindPathSync(Agent,Query);
                if (Found.Result==ENavigationQueryResult::Fail)
                {Refusal=TEXT("complete_path_missing");return false;}
                if (!Found.IsSuccessful() || !Found.Path.IsValid() || Found.Path->IsPartial() || Found.Path->GetPathPoints().Num()<2)
                {Refusal=TEXT("invalid_path_query_or_result");return false;}
                const auto& Points=Found.Path->GetPathPoints();
                for (const auto& P : Points) if (P.Location.ContainsNaN()) {Refusal=TEXT("nonfinite_path");return false;}
                const auto* Corridor=Found.Path->CastPath<FNavMeshPath>();
                if (!Corridor || Corridor->PathCorridor.IsEmpty() || Corridor->ContainsAnyCustomLink())
                {Refusal=TEXT("unsupported_or_missing_ground_corridor");return false;}
                for (const auto& P : Points) if (FNavMeshNodeFlags(P.Flags).IsNavLink())
                {Refusal=TEXT("unsupported_offmesh_path");return false;}
                for (int32 J=1;J<Points.Num();++J)
                {
                    const int32 Count=FMath::CeilToInt(FVector::Distance(Points[J-1].Location,Points[J].Location)/25.);
                    if (Count<1 || Samples+Count>32768) {Refusal=TEXT("path_sampling_budget");return false;}
                    for (int32 K=0;K<=Count;++K)
                    {
                        const FVector P=FMath::Lerp(Points[J-1].Location,Points[J].Location,double(K)/Count);double Z;
                        if (!Height(P,Z) || !FMath::IsFinite(Z)) {Refusal=TEXT("unknown_canonical_support");return false;}
                        MaximumChord=FMath::Max(MaximumChord,FMath::Abs(P.Z-Z));
                        const FDetail Detail=DetailAt(*Recast,Corridor->PathCorridor,P,DetailQueries);
                        if (Detail.Covers) MaximumError=FMath::Max(MaximumError,FMath::Max(FMath::Abs(Detail.Low-Z),FMath::Abs(Detail.High-Z)));
                        if (!DetailControls && FitsGround(Detail,Z))
                        {
                            const TArray<NavNodeRef> Empty,Invalid={0};
                            const auto Vacuous=DetailAt(*Recast,Empty,P,DetailQueries),Bad=DetailAt(*Recast,Invalid,P,DetailQueries);
                            const auto Far=DetailAt(*Recast,Corridor->PathCorridor,P+FVector(10000000,0,0),DetailQueries);
                            if (FitsGround(Vacuous,Z) || Vacuous.Unknown==0 || FitsGround(Bad,Z) || Bad.Unknown==0 || Bad.Covers!=0 ||
                                FitsGround(Far,Z) || Far.Covers!=0 || Far.Unknown!=0 || Far.Clamped==0 || FitsGround(Detail,Detail.High+31))
                            {Refusal=TEXT("detail_measurement_control_failure");return false;}
                            Receipt.SetNumberField(TEXT("navigation_invalid_ref_unknown_candidates"),Bad.Unknown);
                            Receipt.SetNumberField(TEXT("navigation_far_xy_detail_covers"),Far.Covers);
                            Receipt.SetNumberField(TEXT("navigation_displaced_reference_offset_cm"),31);DetailControls=true;
                        }
                        FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ProjectWorldBasinNavigation),true);Q.AddIgnoredActor(&Character);
                        const bool HitFound=World.LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Z+100),FVector(P.X,P.Y,Z-100),ECC_Pawn,Q);
                        if (!FitsGround(Detail,Z) || !HitFound ||
                            !Hit.bBlockingHit || !Support.Contains(Hit.GetComponent()) || Hit.ImpactPoint.ContainsNaN() || FMath::Abs(Hit.ImpactPoint.Z-Z)>.01)
                        {
                            auto Witness=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Position;
                            Witness->SetStringField(TEXT("phase"),Phase);
                            for (double V : {P.X,P.Y,P.Z}) Position.Add(MakeShared<FJsonValueNumber>(V));
                            Witness->SetArrayField(TEXT("navigation_point_cm"),Position);Witness->SetNumberField(TEXT("canonical_ground_cm"),Z);
                            Witness->SetNumberField(TEXT("maximum_navigation_ground_offset_cm"),MaximumError);
                            Witness->SetNumberField(TEXT("maximum_waypoint_chord_ground_offset_cm"),MaximumChord);
                            Witness->SetBoolField(TEXT("ray_found"),HitFound);Witness->SetBoolField(TEXT("blocking_hit"),Hit.bBlockingHit);
                            Witness->SetBoolField(TEXT("expected_terrain_component"),Support.Contains(Hit.GetComponent()));
                            if (HitFound && Hit.bBlockingHit && !Hit.ImpactPoint.ContainsNaN()) Witness->SetNumberField(TEXT("physics_ground_cm"),Hit.ImpactPoint.Z);
                            else Witness->SetStringField(TEXT("physics_ground_status"),TEXT("not_observed_finite"));
                            Witness->SetStringField(TEXT("hit_component"),GetPathNameSafe(Hit.GetComponent()));
                            Witness->SetNumberField(TEXT("completed_segments"),Segments);Witness->SetNumberField(TEXT("completed_samples"),Samples);
                            Witness->SetObjectField(TEXT("actual_corridor_detail"),Describe(Detail,Z));
                            TArray<TSharedPtr<FJsonValue>> Polygons,QueriedPolygons;
                            for (NavNodeRef Ref : Corridor->PathCorridor)
                            {
                                TArray<FVector> Vertices;auto Polygon=MakeShared<FJsonObject>();
                                Polygon->SetStringField(TEXT("native_ref"),FString::Printf(TEXT("%llu"),static_cast<uint64>(Ref)));
                                const bool Covers=Detail.CoverRefs.Contains(Ref);Polygon->SetBoolField(TEXT("same_xy_detail_cover"),Covers);
                                for (int32 Side=0;Side<2;++Side)
                                {
                                    FVector Closest;const bool Known=Recast->GetClosestPointOnPoly(Ref,P+FVector(0,0,Side==0 ? -1000 : 1000),Closest);
                                    const FString Name=Side==0 ? TEXT("lower_query") : TEXT("upper_query");
                                    Polygon->SetBoolField(Name+TEXT("_returned"),Known);
                                    if (Known && !Closest.ContainsNaN())
                                    {
                                        TArray<TSharedPtr<FJsonValue>> Triple;
                                        for (double N : {Closest.X,Closest.Y,Closest.Z}) Triple.Add(MakeShared<FJsonValueNumber>(N));
                                        Polygon->SetArrayField(Name+TEXT("_closest_cm"),Triple);
                                    }
                                }
                                if (Recast->GetPolyVerts(Ref,Vertices))
                                {
                                    TArray<TSharedPtr<FJsonValue>> Encoded;
                                    for (const FVector& V : Vertices) {TArray<TSharedPtr<FJsonValue>> Triple;
                                        for (double N : {V.X,V.Y,V.Z}) Triple.Add(MakeShared<FJsonValueNumber>(N));Encoded.Add(MakeShared<FJsonValueArray>(Triple));}
                                    Polygon->SetArrayField(TEXT("vertices_cm"),Encoded);
                                }
                                else Polygon->SetStringField(TEXT("vertices_status"),TEXT("unavailable"));
                                QueriedPolygons.Add(MakeShared<FJsonValueObject>(Polygon));
                                if (Covers) Polygons.Add(MakeShared<FJsonValueObject>(Polygon));
                            }
                            Witness->SetArrayField(TEXT("actual_covering_polygons"),Polygons);
                            Witness->SetArrayField(TEXT("actual_queried_corridor_polygons"),QueriedPolygons);
                            Receipt.SetObjectField(TEXT("navigation_support_failure_witness"),Witness);
                            Refusal=TEXT("canonical_collision_support_mismatch");return false;
                        }
                        ++Samples;
                    }
                }
                ++Segments;
            }
            return Segments==ExpectedSegments;
        };
        int32 Segments=0,Samples=0;double MaximumError=0;FString Refusal;
        if (!Paths(TEXT("initial_support"),Segments,Samples,MaximumError,Refusal)) {Error=TEXT("Native navigation: ")+Refusal;return false;}
        Receipt.SetNumberField(TEXT("navigation_segments"),Segments);Receipt.SetNumberField(TEXT("navigation_support_samples"),Samples);
        Receipt.SetNumberField(TEXT("navigation_maximum_ground_offset_cm"),MaximumError);
        Receipt.SetNumberField(TEXT("navigation_detail_queries"),DetailQueries);
        Receipt.SetNumberField(TEXT("navigation_maximum_waypoint_chord_ground_offset_cm"),MaximumChord);
        Receipt.SetStringField(TEXT("navigation_waypoint_chord_property_result"),MaximumChord>30 ? TEXT("refused_proxy_property") : TEXT("within_bound_diagnostic_only"));
        for (auto* Component : Terrain) Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (!Build()) return false;
        const bool Bad=Paths(TEXT("removed_support_control"),Segments,Samples,MaximumError,Refusal);
        Receipt.SetStringField(TEXT("navigation_disabled_support_refusal"),Refusal);
        if (Bad || (Refusal!=TEXT("projection_missing") && Refusal!=TEXT("complete_path_missing")))
            return Fail(TEXT("Native navigation did not discriminate removed Terrain support from nonphysical Water."));
        for (int32 I=0;I<Terrain.Num();++I) Terrain[I]->SetCollisionEnabled(Collision[I]);
        if (!Build() || !Paths(TEXT("restored_support"),Segments,Samples,MaximumError,Refusal)) {Error=TEXT("Restored navigation: ")+Error+Refusal;return false;}
        Receipt.SetNumberField(TEXT("navigation_restored_segments"),Segments);
        Receipt.SetNumberField(TEXT("navigation_restored_support_samples"),Samples);
        Receipt.SetNumberField(TEXT("navigation_restored_maximum_ground_offset_cm"),MaximumError);
        if (ConnectorControl)
        {
            if (!ConnectorControl(true,Error) || !Build()) return false;
            const bool Connected=Paths(TEXT("removed_connector_control"),Segments,Samples,MaximumError,Refusal);
            Receipt.SetStringField(TEXT("navigation_removed_connector_refusal"),Refusal);
            if (Connected || Refusal!=TEXT("complete_path_missing"))
                return Fail(TEXT("Removing the connector did not preserve projected deck endpoints while breaking complete navigation."));
            if (!ConnectorControl(false,Error) || !Build() ||
                !Paths(TEXT("restored_connector"),Segments,Samples,MaximumError,Refusal))
            {Error=TEXT("Restored connector navigation: ")+Error+Refusal;return false;}
            Receipt.SetNumberField(TEXT("navigation_connector_restored_segments"),Segments);
            Receipt.SetNumberField(TEXT("navigation_connector_restored_samples"),Samples);
            Receipt.SetNumberField(TEXT("navigation_connector_restored_maximum_ground_offset_cm"),MaximumError);
        }
        Receipt.SetStringField(TEXT("navigation_scope"),TEXT("Actual PIE selected Hero agent; native ground paths both directions, <=25 cm XY stations query all actual corridor detail refs at two independent Z inputs with exact unchanged XY, finite unambiguous height and unchanged 30 cm Ground bound. Correct saved Terrain collision, removed-support/rebuild/restoration. Waypoint chord Z remains a separate diagnostic; no saved/cooked Nav or regional access claim."));
        return true;
    }
}
#endif
