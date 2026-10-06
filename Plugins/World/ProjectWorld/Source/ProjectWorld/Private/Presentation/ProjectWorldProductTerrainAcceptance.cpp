// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductTerrainAcceptance.h"

#include "ProjectWorldTerrainRuntimeRole.h"
#include "Utilities/ProjectSha256.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const FString ProductTerrainRuntimeRolePrefix(TEXT("ProjectWorld.RuntimeRole="));
	const FString TerritoryNavigationRole(TEXT("TerritoryNavigation"));

	bool ReadVector(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		int32 Count,
		FVector& OutValue)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || Values == nullptr || Values->Num() != Count)
		{
			return false;
		}
		OutValue = FVector::ZeroVector;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			double Number = 0.0;
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetNumber(Number) || !FMath::IsFinite(Number))
			{
				return false;
			}
			OutValue[Index] = Number;
		}
		return true;
	}

	bool HasRuntimeRole(const AActor& Actor, const FString& Role)
	{
		return Actor.Tags.Contains(FName(*(ProductTerrainRuntimeRolePrefix + Role)));
	}
}

bool ProjectWorldProductTerrainAcceptancePolicy::IsMeshComponentAccepted(
	bool bCollisionCandidate,
	bool bNonMainPassHelper,
	bool bCanEverAffectNavigation)
{
	return (!bCollisionCandidate || bCanEverAffectNavigation) &&
		(!bNonMainPassHelper || !bCanEverAffectNavigation);
}

FProjectWorldProductTerrainAcceptance::~FProjectWorldProductTerrainAcceptance()
{
	RestoreVsmCache();
	ReleaseNavigationInvoker();
}

bool FProjectWorldProductTerrainAcceptance::Initialize(
	UWorld& InWorld,
	ACharacter& InCharacter,
	APlayerController& InController,
	const FString& ContractPath,
	const FString& ExpectedContractHash,
	const FString& ExpectedMap,
	const FString& ExpectedRuntimeHash,
	bool bInForceFreshVsm,
	FString& OutError)
{
	if (!LoadContract(
		ContractPath,
		ExpectedContractHash,
		ExpectedMap,
		ExpectedRuntimeHash,
		OutError))
	{
		return false;
	}
	World = &InWorld;
	Character = &InCharacter;
	Controller = &InController;
	InitialActorRotation = InCharacter.GetActorRotation();
	InitialControlRotation = InController.GetControlRotation();
	bForceFreshVsm = bInForceFreshVsm;
	VsmCacheVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Cache"));
	if (VsmCacheVariable == nullptr)
	{
		OutError = TEXT("The VSM cache console variable is unavailable in the packaged runtime.");
		return false;
	}
	OriginalVsmCache = VsmCacheVariable->GetInt();
	if (OriginalVsmCache == 0)
	{
		OutError = TEXT("Terrain acceptance requires normal VSM caching to be enabled before either control arm starts.");
		return false;
	}
	if (!ResolveNavigation(OutError))
	{
		return false;
	}
	UNavigationSystemV1::RegisterNavigationInvoker(
		InCharacter, NavigationGenerationRadius, NavigationRemovalRadius);
	bInvokerRegistered = true;
	bEnabled = true;
	return true;
}

bool FProjectWorldProductTerrainAcceptance::LoadContract(
	const FString& ContractPath,
	const FString& ExpectedContractHash,
	const FString& ExpectedMap,
	const FString& ExpectedRuntimeHash,
	FString& OutError)
{
	if (FPaths::IsRelative(ContractPath) || ExpectedContractHash.Len() != 64)
	{
		OutError = TEXT("Terrain acceptance requires an absolute contract path and SHA-256 identity.");
		return false;
	}
	FString ActualHash;
	FString Payload;
	if (!FProjectSha256::HashFile(ContractPath, ActualHash) ||
		!ActualHash.Equals(ExpectedContractHash, ESearchCase::IgnoreCase) ||
		!FFileHelper::LoadFileToString(Payload, *ContractPath))
	{
		OutError = TEXT("The terrain acceptance contract is missing or failed SHA-256 authentication.");
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Payload), Root) || !Root.IsValid())
	{
		OutError = TEXT("The terrain acceptance contract is not valid JSON.");
		return false;
	}
	FString Schema;
	FString Map;
	FString RuntimeHash;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) ||
		Schema != TEXT("project-world-product-terrain-acceptance:v1") ||
		!Root->TryGetStringField(TEXT("map_package"), Map) || Map != ExpectedMap ||
		!Root->TryGetStringField(TEXT("runtime_profile_sha256"), RuntimeHash) ||
		!RuntimeHash.Equals(ExpectedRuntimeHash, ESearchCase::IgnoreCase) ||
		!Root->TryGetStringField(TEXT("terrain_generator_id"), TerrainGeneratorId))
	{
		OutError = TEXT("The terrain acceptance contract identity does not match the requested product route.");
		return false;
	}
	const TSharedPtr<FJsonObject>* Expectations = nullptr;
	FString CollisionExpectation;
	FString HelperExpectation;
	FString EndpointExpectation;
	if (!Root->TryGetObjectField(TEXT("terrain_expectations"), Expectations) ||
		Expectations == nullptr || !Expectations->IsValid() || (*Expectations)->Values.Num() != 3 ||
		!(*Expectations)->TryGetStringField(TEXT("collision_components"), CollisionExpectation) ||
		!(*Expectations)->TryGetStringField(TEXT("non_main_pass_helpers"), HelperExpectation) ||
		!(*Expectations)->TryGetStringField(TEXT("route_endpoints"), EndpointExpectation) ||
		CollisionExpectation != TEXT("navigation_relevant_pawn_blocking") ||
		HelperExpectation != TEXT("required_navigation_irrelevant") ||
		EndpointExpectation != TEXT("navigation_relevant_pawn_blocking"))
	{
		OutError = FString::Printf(TEXT("Terrain generator %s has no supported runtime acceptance expectations."),
			*TerrainGeneratorId);
		return false;
	}

	const TSharedPtr<FJsonObject>* NavigationObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("navigation"), NavigationObject) || NavigationObject == nullptr ||
		!ReadVector(*NavigationObject, TEXT("start_unreal_cm"), 3, NavigationStart) ||
		!ReadVector(*NavigationObject, TEXT("end_unreal_cm"), 3, NavigationEnd) ||
		!(*NavigationObject)->TryGetStringField(TEXT("start_cell_id"), NavigationStartCell) ||
		!(*NavigationObject)->TryGetStringField(TEXT("end_cell_id"), NavigationEndCell) ||
		!(*NavigationObject)->TryGetStringField(TEXT("crossed_boundary"), NavigationBoundary))
	{
		OutError = TEXT("The terrain acceptance navigation contract is incomplete.");
		return false;
	}
	double GenerationRadius = 0.0;
	double RemovalRadius = 0.0;
	if (!(*NavigationObject)->TryGetNumberField(TEXT("generation_radius_cm"), GenerationRadius) ||
		!(*NavigationObject)->TryGetNumberField(TEXT("removal_radius_cm"), RemovalRadius) ||
		GenerationRadius <= 0.0 || RemovalRadius <= GenerationRadius ||
		NavigationStartCell.IsEmpty() || NavigationEndCell.IsEmpty() ||
		NavigationStartCell == NavigationEndCell || NavigationBoundary.IsEmpty())
	{
		OutError = TEXT("The navigation contract must cross a canonical boundary with bounded invoker radii.");
		return false;
	}
	NavigationGenerationRadius = static_cast<float>(GenerationRadius);
	NavigationRemovalRadius = static_cast<float>(RemovalRadius);

	const TArray<TSharedPtr<FJsonValue>>* ProbeValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("height_probes"), ProbeValues) || ProbeValues == nullptr ||
		ProbeValues->Num() < 5 || ProbeValues->Num() > 9)
	{
		OutError = TEXT("The terrain acceptance contract requires five through nine height probes.");
		return false;
	}
	TSet<FString> Ids;
	TSet<FString> Kinds;
	for (const TSharedPtr<FJsonValue>& ProbeValue : *ProbeValues)
	{
		const TSharedPtr<FJsonObject> ProbeObject = ProbeValue.IsValid() ? ProbeValue->AsObject() : nullptr;
		FHeightProbe Probe;
		if (!ProbeObject.IsValid() ||
			!ProbeObject->TryGetStringField(TEXT("id"), Probe.Id) ||
			!ProbeObject->TryGetStringField(TEXT("kind"), Probe.Kind) ||
			!ProbeObject->TryGetStringField(TEXT("stage"), Probe.Stage) ||
			!ProbeObject->TryGetStringField(TEXT("cell_id"), Probe.CellId) ||
			!ReadVector(ProbeObject, TEXT("canonical_m"), 3, Probe.CanonicalMeters) ||
			!ReadVector(ProbeObject, TEXT("unreal_cm"), 3, Probe.UnrealCentimeters) ||
			!ProbeObject->TryGetNumberField(TEXT("tolerance_cm"), Probe.ToleranceCentimeters) ||
			Probe.Id.IsEmpty() || Probe.CellId.IsEmpty() ||
			(Probe.Stage != TEXT("center") && Probe.Stage != TEXT("edge")) ||
			Probe.ToleranceCentimeters <= 0.0 || Ids.Contains(Probe.Id))
		{
			OutError = TEXT("A terrain height probe is invalid or duplicates an existing probe identity.");
			return false;
		}
		Ids.Add(Probe.Id);
		Kinds.Add(Probe.Kind);
		HeightProbes.Add(MoveTemp(Probe));
	}
	for (const FString& RequiredKind : {
		TEXT("center"), TEXT("perimeter"), TEXT("cell_boundary"),
		TEXT("high"), TEXT("low"), TEXT("hydro_transition")})
	{
		if (!Kinds.Contains(RequiredKind))
		{
			OutError = FString::Printf(TEXT("The terrain acceptance contract has no '%s' height probe."), *RequiredKind);
			return false;
		}
	}
	ContractHash = ActualHash.ToLower();
	return true;
}

bool FProjectWorldProductTerrainAcceptance::ResolveNavigation(FString& OutError)
{
	UWorld* ActiveWorld = World.Get();
	UNavigationSystemV1* ActiveNavigation = ActiveWorld != nullptr
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(ActiveWorld)
		: nullptr;
	ARecastNavMesh* ActiveRecast = ActiveNavigation != nullptr
		? Cast<ARecastNavMesh>(ActiveNavigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
		: nullptr;
	int32 TerritoryBoundsCount = 0;
	if (ActiveWorld != nullptr)
	{
		for (TActorIterator<ANavMeshBoundsVolume> It(ActiveWorld); It; ++It)
		{
			TerritoryBoundsCount += HasRuntimeRole(**It, TerritoryNavigationRole) ? 1 : 0;
		}
	}
	if (ActiveNavigation == nullptr || ActiveRecast == nullptr || TerritoryBoundsCount != 1 ||
		!ActiveNavigation->IsActiveTilesGenerationEnabled() ||
		ActiveRecast->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic ||
		ActiveRecast->IsPackageExternal())
	{
		OutError = FString::Printf(
			TEXT("Product navigation requires one neutral bounds domain, map-owned dynamic Recast, and invoker-only generation (bounds=%d nav=%d recast=%d invoker_only=%d dynamic=%d map_owned=%d)."),
			TerritoryBoundsCount,
			ActiveNavigation != nullptr ? 1 : 0,
			ActiveRecast != nullptr ? 1 : 0,
			ActiveNavigation != nullptr && ActiveNavigation->IsActiveTilesGenerationEnabled() ? 1 : 0,
			ActiveRecast != nullptr && ActiveRecast->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic ? 1 : 0,
			ActiveRecast != nullptr && !ActiveRecast->IsPackageExternal() ? 1 : 0);
		return false;
	}
	Navigation = ActiveNavigation;
	Recast = ActiveRecast;
	return true;
}

FProjectWorldProductTerrainAcceptance::EProbeStatus
FProjectWorldProductTerrainAcceptance::ProbeCenter(FString& OutError)
{
	const EProbeStatus HeightStatus = ProbeHeightStage(TEXT("center"), OutError);
	if (HeightStatus != EProbeStatus::Accepted)
	{
		return HeightStatus;
	}
	if (!InspectMeshNavigationComponents(OutError))
	{
		return EProbeStatus::Rejected;
	}
	const EProbeStatus NavigationStatus = ProbeNavigation(false, OutError);
	bCenterAccepted = NavigationStatus == EProbeStatus::Accepted;
	return NavigationStatus;
}

FProjectWorldProductTerrainAcceptance::EProbeStatus
FProjectWorldProductTerrainAcceptance::ProbeEdge(FString& OutError)
{
	const EProbeStatus Status = ProbeHeightStage(TEXT("edge"), OutError);
	bEdgeAccepted = Status == EProbeStatus::Accepted;
	return Status;
}

FProjectWorldProductTerrainAcceptance::EProbeStatus
FProjectWorldProductTerrainAcceptance::ProbeAfterReload(FString& OutError)
{
	const EProbeStatus NavigationStatus = ProbeNavigation(true, OutError);
	if (NavigationStatus != EProbeStatus::Accepted)
	{
		return NavigationStatus;
	}
	ACharacter* ActiveCharacter = Character.Get();
	APlayerController* ActiveController = Controller.Get();
	if (ActiveCharacter == nullptr || ActiveController == nullptr)
	{
		OutError = TEXT("The fixed acceptance camera owner became unavailable after reload.");
		return EProbeStatus::Rejected;
	}
	ActiveCharacter->SetActorRotation(InitialActorRotation);
	ActiveController->SetControlRotation(InitialControlRotation);
	bReloadAccepted = true;
	return EProbeStatus::Accepted;
}

FProjectWorldProductTerrainAcceptance::EProbeStatus
FProjectWorldProductTerrainAcceptance::ProbeHeightStage(const FString& Stage, FString& OutError)
{
	bool bPending = false;
	for (FHeightProbe& Probe : HeightProbes)
	{
		if (Probe.Stage != Stage || Probe.bAccepted)
		{
			continue;
		}
		UPrimitiveComponent* Primitive = nullptr;
		if (!TraceTerrain(
			Probe.UnrealCentimeters,
			Probe.UnrealCentimeters.Z,
			Probe.Actor,
			Probe.Component,
			Probe.SectionPackage,
			Probe.ActualZCentimeters,
			Primitive))
		{
			bPending = true;
			continue;
		}
		Probe.ErrorCentimeters = FMath::Abs(Probe.ActualZCentimeters - Probe.UnrealCentimeters.Z);
		if (Probe.ErrorCentimeters > Probe.ToleranceCentimeters)
		{
			OutError = FString::Printf(
				TEXT("Height probe '%s' missed the canonical final surface by %.3f cm (allowed %.3f cm)."),
				*Probe.Id, Probe.ErrorCentimeters, Probe.ToleranceCentimeters);
			return EProbeStatus::Rejected;
		}
		Probe.bAccepted = true;
	}
	return bPending ? EProbeStatus::NotReady : EProbeStatus::Accepted;
}

FProjectWorldProductTerrainAcceptance::EProbeStatus
FProjectWorldProductTerrainAcceptance::ProbeNavigation(bool bAfterReload, FString& OutError)
{
	UWorld* ActiveWorld = World.Get();
	UNavigationSystemV1* ActiveNavigation = Navigation.Get();
	ARecastNavMesh* ActiveRecast = Recast.Get();
	if (ActiveWorld == nullptr || ActiveNavigation == nullptr || ActiveRecast == nullptr)
	{
		OutError = TEXT("The product navigation authority became unavailable.");
		return EProbeStatus::Rejected;
	}
	FNavLocation ProjectedStart;
	FNavLocation ProjectedEnd;
	const FVector QueryExtent(2000.0, 2000.0, 10000.0);
	if (!ActiveNavigation->ProjectPointToNavigation(
		NavigationStart, ProjectedStart, QueryExtent, ActiveRecast) ||
		!ActiveNavigation->ProjectPointToNavigation(
		NavigationEnd, ProjectedEnd, QueryExtent, ActiveRecast))
	{
		return EProbeStatus::NotReady;
	}
	double PathLengthCentimeters = 0.0;
	if (UNavigationSystemV1::GetPathLength(
		ActiveWorld,
		ProjectedStart.Location,
		ProjectedEnd.Location,
		PathLengthCentimeters,
		ActiveRecast) != ENavigationQueryResult::Success || PathLengthCentimeters <= 0.0)
	{
		return EProbeStatus::NotReady;
	}
	FString StartPackage;
	FString EndPackage;
	double StartZ = 0.0;
	double EndZ = 0.0;
	UPrimitiveComponent* StartPrimitive = nullptr;
	UPrimitiveComponent* EndPrimitive = nullptr;
	if (!TraceTerrain(
		NavigationStart,
		NavigationStart.Z,
		NavigationStartActor,
		NavigationStartComponent,
		StartPackage,
		StartZ,
		StartPrimitive) ||
		!TraceTerrain(
			NavigationEnd,
			NavigationEnd.Z,
			NavigationEndActor,
			NavigationEndComponent,
			EndPackage,
			EndZ,
			EndPrimitive))
	{
		return EProbeStatus::NotReady;
	}
	if (!ValidateMeshRoutePrimitive(StartPrimitive, TEXT("start"), OutError) ||
		!ValidateMeshRoutePrimitive(EndPrimitive, TEXT("end"), OutError))
	{
		return EProbeStatus::Rejected;
	}
	const double PathMeters = PathLengthCentimeters * 0.01;
	if (bAfterReload)
	{
		NavigationPathAfterReloadMeters = PathMeters;
		NavigationTilesAfterReload = ActiveRecast->GetNumActiveTiles();
	}
	else
	{
		NavigationPathBeforeReloadMeters = PathMeters;
		NavigationTilesBeforeReload = ActiveRecast->GetNumActiveTiles();
	}
	return EProbeStatus::Accepted;
}

bool FProjectWorldProductTerrainAcceptance::TraceTerrain(
	const FVector& Point,
	double ExpectedZ,
	FString& OutActor,
	FString& OutComponent,
	FString& OutPackage,
	double& OutActualZ,
	UPrimitiveComponent*& OutPrimitive) const
{
	UWorld* ActiveWorld = World.Get();
	if (ActiveWorld == nullptr)
	{
		return false;
	}
	const FVector Start(Point.X, Point.Y, ExpectedZ + 100000.0);
	const FVector End(Point.X, Point.Y, ExpectedZ - 100000.0);
	double BestError = TNumericLimits<double>::Max();
	for (TActorIterator<AActor> It(ActiveWorld); It; ++It)
	{
		if (!ProjectWorldTerrainRuntimeRole::HasRole(**It))
		{
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectWorldProductTerrainHeight), true);
		FHitResult Hit;
		if (!It->ActorLineTraceSingle(Hit, Start, End, ECC_Visibility, Params) || !Hit.bBlockingHit)
		{
			continue;
		}
		const double Error = FMath::Abs(Hit.ImpactPoint.Z - ExpectedZ);
		if (Error < BestError)
		{
			BestError = Error;
			OutActor = It->GetPathName();
			OutComponent = Hit.GetComponent() != nullptr ? Hit.GetComponent()->GetPathName() : FString();
			OutPackage = It->GetPackage()->GetName();
			OutActualZ = Hit.ImpactPoint.Z;
			OutPrimitive = Hit.GetComponent();
		}
	}
	return BestError < TNumericLimits<double>::Max();
}

bool FProjectWorldProductTerrainAcceptance::InspectMeshNavigationComponents(FString& OutError)
{
	UWorld* ActiveWorld = World.Get();
	if (ActiveWorld == nullptr)
	{
		OutError = TEXT("The product world is unavailable for terrain navigation inspection.");
		return false;
	}
	MeshCollisionComponentCount = 0;
	MeshHelperComponentCount = 0;
	MeshNavigationPolicyOffenderCount = 0;
	for (TActorIterator<AActor> It(ActiveWorld); It; ++It)
	{
		if (!ProjectWorldTerrainRuntimeRole::HasRole(**It))
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive == nullptr)
			{
				continue;
			}
			const bool bCollisionCandidate = Primitive->IsCollisionEnabled() &&
				Primitive->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
			bool bNonMainPassHelper = false;
			if (const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Primitive))
			{
				bNonMainPassHelper = !StaticMesh->bRenderInMainPass;
			}
			MeshCollisionComponentCount += bCollisionCandidate ? 1 : 0;
			MeshHelperComponentCount += bNonMainPassHelper ? 1 : 0;
			if ((bCollisionCandidate || bNonMainPassHelper) &&
				!ProjectWorldProductTerrainAcceptancePolicy::IsMeshComponentAccepted(
					bCollisionCandidate,
					bNonMainPassHelper,
					Primitive->CanEverAffectNavigation()))
			{
				++MeshNavigationPolicyOffenderCount;
			}
		}
	}
	bMeshCollisionNavigationRelevant = MeshCollisionComponentCount > 0 && MeshNavigationPolicyOffenderCount == 0;
	bMeshHelperNavigationIrrelevant = MeshHelperComponentCount > 0 && MeshNavigationPolicyOffenderCount == 0;
	if (!bMeshCollisionNavigationRelevant || !bMeshHelperNavigationIrrelevant)
	{
		OutError = FString::Printf(
			TEXT("Every loaded terrain collision/helper component must obey navigation policy (collision=%d helpers=%d offenders=%d)."),
			MeshCollisionComponentCount,
			MeshHelperComponentCount,
			MeshNavigationPolicyOffenderCount);
		return false;
	}
	return true;
}

bool FProjectWorldProductTerrainAcceptance::ValidateMeshRoutePrimitive(
	const UPrimitiveComponent* Primitive,
	const TCHAR* Endpoint,
	FString& OutError) const
{
	const bool bAccepted = Primitive != nullptr && Primitive->IsCollisionEnabled() &&
		Primitive->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block &&
		Primitive->CanEverAffectNavigation();
	if (!bAccepted)
	{
		OutError = FString::Printf(
			TEXT("The terrain %s route endpoint did not trace to navigation-relevant Pawn-blocking collision."),
			Endpoint);
	}
	return bAccepted;
}

void FProjectWorldProductTerrainAcceptance::BeginStreamingControl()
{
	if (bForceFreshVsm && VsmCacheVariable != nullptr)
	{
		VsmCacheVariable->Set(0, ECVF_SetByCode);
		VsmCacheValueDuringStreaming = VsmCacheVariable->GetInt();
		bVsmCacheDisabledDuringStreaming = OriginalVsmCache != 0 && VsmCacheValueDuringStreaming == 0;
	}
}

bool FProjectWorldProductTerrainAcceptance::CompleteStreamingControl(FString& OutError)
{
	RestoreVsmCache();
	if (bForceFreshVsm && (!bVsmCacheDisabledDuringStreaming || !bVsmCacheRestored))
	{
		OutError = TEXT("The forced-fresh VSM control did not disable caching only across streaming and restore it afterward.");
		return false;
	}
	return true;
}

void FProjectWorldProductTerrainAcceptance::AppendReceipt(FJsonObject& Root) const
{
	Root.SetBoolField(TEXT("terrain_acceptance_enabled"), bEnabled);
	if (!bEnabled)
	{
		return;
	}
	Root.SetStringField(TEXT("terrain_acceptance_contract_sha256"), ContractHash);
	Root.SetStringField(TEXT("terrain_generator_id"), TerrainGeneratorId);
	Root.SetBoolField(TEXT("navigation_invoker_temporary"), bInvokerRegistered);
	Root.SetNumberField(TEXT("navigation_generation_radius_cm"), NavigationGenerationRadius);
	Root.SetNumberField(TEXT("navigation_removal_radius_cm"), NavigationRemovalRadius);
	Root.SetStringField(TEXT("navigation_start_cell_id"), NavigationStartCell);
	Root.SetStringField(TEXT("navigation_end_cell_id"), NavigationEndCell);
	Root.SetStringField(TEXT("navigation_crossed_boundary"), NavigationBoundary);
	Root.SetNumberField(TEXT("navigation_path_before_reload_m"), NavigationPathBeforeReloadMeters);
	Root.SetNumberField(TEXT("navigation_path_after_reload_m"), NavigationPathAfterReloadMeters);
	Root.SetNumberField(TEXT("navigation_tiles_before_reload"), NavigationTilesBeforeReload);
	Root.SetNumberField(TEXT("navigation_tiles_after_reload"), NavigationTilesAfterReload);
	Root.SetStringField(TEXT("navigation_start_terrain_actor"), NavigationStartActor);
	Root.SetStringField(TEXT("navigation_start_terrain_component"), NavigationStartComponent);
	Root.SetStringField(TEXT("navigation_end_terrain_actor"), NavigationEndActor);
	Root.SetStringField(TEXT("navigation_end_terrain_component"), NavigationEndComponent);
	Root.SetBoolField(TEXT("mesh_collision_navigation_relevant"), bMeshCollisionNavigationRelevant);
	Root.SetBoolField(TEXT("mesh_helper_navigation_irrelevant"), bMeshHelperNavigationIrrelevant);
	Root.SetNumberField(TEXT("mesh_collision_component_count"), MeshCollisionComponentCount);
	Root.SetNumberField(TEXT("mesh_helper_component_count"), MeshHelperComponentCount);
	Root.SetNumberField(TEXT("mesh_navigation_policy_offender_count"), MeshNavigationPolicyOffenderCount);
	Root.SetBoolField(TEXT("vsm_forced_fresh_control"), bForceFreshVsm);
	Root.SetNumberField(TEXT("vsm_cache_value_before_streaming"), OriginalVsmCache);
	Root.SetNumberField(TEXT("vsm_cache_value_during_streaming"), VsmCacheValueDuringStreaming);
	Root.SetNumberField(TEXT("vsm_cache_value_after_streaming"), VsmCacheValueAfterStreaming);
	Root.SetBoolField(TEXT("vsm_cache_disabled_during_streaming"), bVsmCacheDisabledDuringStreaming);
	Root.SetBoolField(TEXT("vsm_cache_restored_after_streaming"), bVsmCacheRestored);
	TArray<TSharedPtr<FJsonValue>> ProbeValues;
	for (const FHeightProbe& Probe : HeightProbes)
	{
		TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("id"), Probe.Id);
		Value->SetStringField(TEXT("kind"), Probe.Kind);
		Value->SetStringField(TEXT("stage"), Probe.Stage);
		Value->SetStringField(TEXT("cell_id"), Probe.CellId);
		Value->SetArrayField(TEXT("canonical_m"), {
			MakeShared<FJsonValueNumber>(Probe.CanonicalMeters.X),
			MakeShared<FJsonValueNumber>(Probe.CanonicalMeters.Y),
			MakeShared<FJsonValueNumber>(Probe.CanonicalMeters.Z)});
		Value->SetArrayField(TEXT("unreal_cm"), {
			MakeShared<FJsonValueNumber>(Probe.UnrealCentimeters.X),
			MakeShared<FJsonValueNumber>(Probe.UnrealCentimeters.Y),
			MakeShared<FJsonValueNumber>(Probe.UnrealCentimeters.Z)});
		Value->SetNumberField(TEXT("expected_z_cm"), Probe.UnrealCentimeters.Z);
		Value->SetNumberField(TEXT("actual_z_cm"), Probe.ActualZCentimeters);
		Value->SetNumberField(TEXT("absolute_error_cm"), Probe.ErrorCentimeters);
		Value->SetNumberField(TEXT("tolerance_cm"), Probe.ToleranceCentimeters);
		Value->SetStringField(TEXT("terrain_actor"), Probe.Actor);
		Value->SetStringField(TEXT("terrain_component"), Probe.Component);
		Value->SetStringField(TEXT("terrain_section_package"), Probe.SectionPackage);
		Value->SetBoolField(TEXT("accepted"), Probe.bAccepted);
		ProbeValues.Add(MakeShared<FJsonValueObject>(Value));
	}
	Root.SetArrayField(TEXT("height_probes"), ProbeValues);
}

bool FProjectWorldProductTerrainAcceptance::IsAccepted() const
{
	return !bEnabled || (bCenterAccepted && bEdgeAccepted && bReloadAccepted &&
		HeightProbes.ContainsByPredicate([](const FHeightProbe& Probe) { return !Probe.bAccepted; }) == false &&
		(!bForceFreshVsm || (bVsmCacheDisabledDuringStreaming && bVsmCacheRestored)));
}

void FProjectWorldProductTerrainAcceptance::ReleaseNavigationInvoker()
{
	if (bInvokerRegistered)
	{
		if (ACharacter* ActiveCharacter = Character.Get())
		{
			UNavigationSystemV1::UnregisterNavigationInvoker(*ActiveCharacter);
		}
		bInvokerRegistered = false;
	}
}

void FProjectWorldProductTerrainAcceptance::RestoreVsmCache()
{
	if (bForceFreshVsm && VsmCacheVariable != nullptr && bVsmCacheDisabledDuringStreaming)
	{
		VsmCacheVariable->Set(OriginalVsmCache, ECVF_SetByCode);
		VsmCacheValueAfterStreaming = VsmCacheVariable->GetInt();
		bVsmCacheRestored = VsmCacheValueAfterStreaming == OriginalVsmCache;
	}
}
