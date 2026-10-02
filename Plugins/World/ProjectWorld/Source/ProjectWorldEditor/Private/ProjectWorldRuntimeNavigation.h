// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

class ARecastNavMesh;
class ANavigationData;
class ANavMeshBoundsVolume;
class AActor;
class UNavigationSystemV1;
class UWorld;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldRealizationResult;
struct FProjectWorldRuntimeProfile;

namespace ProjectWorldRuntimeNavigation
{
	bool GetTerritoryDomainBounds(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile& Profile,
		FBox& OutBounds,
		FString& OutError);

	bool ConfigureTerritoryDomain(
		UWorld* World,
		ANavMeshBoundsVolume* NavBounds,
		const FBox& DomainBounds,
		ARecastNavMesh*& OutRecast,
		FString& OutError);

	bool EnsureTerritoryDomain(
		UWorld* World,
		ANavMeshBoundsVolume* NavBounds,
		ARecastNavMesh*& OutRecast,
		FString& OutError);

	bool ConfigureRouteDomain(
		UWorld* World,
		AActor* InvokerOwner,
		ANavMeshBoundsVolume* NavBounds,
		const FVector& Start,
		const FVector& End,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		ANavigationData*& OutNavigationData,
		FString& OutError);

	bool EnsureInternalData(
		UWorld* World,
		UNavigationSystemV1* Navigation,
		ARecastNavMesh*& OutRecast,
		FString& OutError);
}
