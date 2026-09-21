// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

/** Reserved scope name for per-actor capabilities (vs per-mesh). */
inline const FName NAME_CapabilityScope_Actor(TEXT("actor"));

/**
 * Registry for all capability components (objects AND skeletal actors).
 *
 * Maps stable capability IDs to UClass* for data-driven actor composition.
 * Components register by overriding GetPrimaryAssetId() to return
 * FPrimaryAssetId("CapabilityComponent", "MyCapabilityId").
 *
 * Works for any actor type:
 * - Simple objects (doors, chests): capabilities attached at spawn, no lifecycle
 * - Skeletal actors (hero, NPC): same capabilities, optionally managed by
 *   USkeletalAssemblyComponent when lifecycle orchestration is needed
 *
 * Built lazily on first lookup. Uses FRegisteredClassScan kernel from
 * ProjectCore for CDO scan mechanics.
 *
 * All public methods must be called from the game thread.
 *
 * For capability validation, see FCapabilityValidationRegistry in ProjectCore.
 *
 * Architecture: Plugins/Foundation/ProjectCore/docs/architecture/asset_identity.md
 */
class PROJECTOBJECTCAPABILITIES_API FCapabilityRegistry
{
public:
	/**
	 * Get component class by capability ID.
	 * Builds registry on first call (lazy initialization).
	 *
	 * @param CapabilityId Stable ID (e.g., "Lockable", "Hinged", "MotionMatching")
	 * @return UClass* or nullptr if not found
	 */
	static UClass* GetCapabilityClass(FName CapabilityId);

	/**
	 * Check if capability ID exists.
	 */
	static bool HasCapability(FName CapabilityId);

	/** True once every enabled provider has completed normal plugin startup. */
	static bool IsReady(FString* OutReason = nullptr);

	/**
	 * Force registry rebuild (call on hot reload / module load).
	 */
	static void RebuildRegistry();

	/**
	 * Check if a capability type is a motion capability (needs Movable mesh).
	 */
	static bool IsMotionCapability(FName CapabilityId);

	/** Enumerate all registered entries. */
	static void ForEach(TFunctionRef<void(FName Id, UClass* Class)> Func);

	/** Log all registered entries for diagnostics. */
	static void DumpToLog();

	/** Number of registered capabilities. */
	static int32 Num();

private:
	static bool EnsureBuilt();
	static bool Build();

	static TMap<FName, UClass*> Registry;
	static bool bIsBuilt;
	static uint32 BuiltProviderGeneration;
};
