// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class AActor;
class ACharacter;
class APlayerController;
class ARecastNavMesh;
class FJsonObject;
class IConsoleVariable;
class UNavigationSystemV1;
class UPrimitiveComponent;
class UWorld;

namespace ProjectWorldProductTerrainAcceptancePolicy
{
	bool IsMeshComponentAccepted(
		bool bCollisionCandidate,
		bool bNonMainPassHelper,
		bool bCanEverAffectNavigation);
}

class FProjectWorldProductTerrainAcceptance final
{
public:
	enum class EProbeStatus : uint8
	{
		NotReady,
		Accepted,
		Rejected
	};

	~FProjectWorldProductTerrainAcceptance();

	bool Initialize(
		UWorld& World,
		ACharacter& Character,
		APlayerController& Controller,
		const FString& ContractPath,
		const FString& ExpectedContractHash,
		const FString& ExpectedMap,
		const FString& ExpectedRuntimeHash,
		bool bForceFreshVsm,
		FString& OutError);

	EProbeStatus ProbeCenter(FString& OutError);
	EProbeStatus ProbeEdge(FString& OutError);
	EProbeStatus ProbeAfterReload(FString& OutError);
	void BeginStreamingControl();
	bool CompleteStreamingControl(FString& OutError);
	void AppendReceipt(FJsonObject& Root) const;
	bool IsAccepted() const;

private:
	struct FHeightProbe
	{
		FString Id;
		FString Kind;
		FString Stage;
		FString CellId;
		FVector CanonicalMeters = FVector::ZeroVector;
		FVector UnrealCentimeters = FVector::ZeroVector;
		double ToleranceCentimeters = 0.0;
		FString Actor;
		FString Component;
		FString SectionPackage;
		double ActualZCentimeters = 0.0;
		double ErrorCentimeters = 0.0;
		bool bAccepted = false;
	};

	bool LoadContract(
		const FString& ContractPath,
		const FString& ExpectedContractHash,
		const FString& ExpectedMap,
		const FString& ExpectedRuntimeHash,
		FString& OutError);
	bool ResolveNavigation(FString& OutError);
	EProbeStatus ProbeHeightStage(const FString& Stage, FString& OutError);
	EProbeStatus ProbeNavigation(bool bAfterReload, FString& OutError);
	bool TraceTerrain(
		const FVector& Point,
		double ExpectedZ,
		FString& OutActor,
		FString& OutComponent,
		FString& OutPackage,
		double& OutActualZ,
		UPrimitiveComponent*& OutPrimitive) const;
	bool InspectMeshNavigationComponents(FString& OutError);
	bool ValidateMeshRoutePrimitive(
		const UPrimitiveComponent* Primitive,
		const TCHAR* Endpoint,
		FString& OutError) const;
	void ReleaseNavigationInvoker();
	void RestoreVsmCache();

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<UNavigationSystemV1> Navigation;
	TWeakObjectPtr<ARecastNavMesh> Recast;
	TArray<FHeightProbe> HeightProbes;
	FVector NavigationStart = FVector::ZeroVector;
	FVector NavigationEnd = FVector::ZeroVector;
	FString NavigationStartCell;
	FString NavigationEndCell;
	FString NavigationBoundary;
	FString ContractHash;
	FString TerrainGeneratorId;
	FString NavigationStartActor;
	FString NavigationStartComponent;
	FString NavigationEndActor;
	FString NavigationEndComponent;
	FRotator InitialActorRotation = FRotator::ZeroRotator;
	FRotator InitialControlRotation = FRotator::ZeroRotator;
	IConsoleVariable* VsmCacheVariable = nullptr;
	double NavigationPathBeforeReloadMeters = 0.0;
	double NavigationPathAfterReloadMeters = 0.0;
	int32 NavigationTilesBeforeReload = 0;
	int32 NavigationTilesAfterReload = 0;
	int32 OriginalVsmCache = 0;
	int32 VsmCacheValueDuringStreaming = INDEX_NONE;
	int32 VsmCacheValueAfterStreaming = INDEX_NONE;
	int32 MeshCollisionComponentCount = 0;
	int32 MeshHelperComponentCount = 0;
	int32 MeshNavigationPolicyOffenderCount = 0;
	float NavigationGenerationRadius = 0.0f;
	float NavigationRemovalRadius = 0.0f;
	bool bEnabled = false;
	bool bInvokerRegistered = false;
	bool bCenterAccepted = false;
	bool bEdgeAccepted = false;
	bool bReloadAccepted = false;
	bool bForceFreshVsm = false;
	bool bVsmCacheDisabledDuringStreaming = false;
	bool bVsmCacheRestored = false;
	bool bMeshCollisionNavigationRelevant = false;
	bool bMeshHelperNavigationIrrelevant = false;
};
