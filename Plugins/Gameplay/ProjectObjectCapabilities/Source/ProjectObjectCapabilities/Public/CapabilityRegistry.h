// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Reserved scope name for per-actor capabilities (vs per-mesh). */
inline const FName NAME_CapabilityScope_Actor(TEXT("actor"));

/**
 * Registry for capability components (CDO scan pattern).
 *
 * Maps stable capability IDs to UClass* for data-driven object spawning.
 * Components register by overriding GetPrimaryAssetId() to return
 * FPrimaryAssetId("CapabilityComponent", "MyCapabilityId").
 *
 * For capability validation, see FCapabilityValidationRegistry in ProjectCore.
 *
 * Built lazily on first lookup. Rebuilt on module reload.
 *
 * Architecture: [flexible_path.md] - Pattern B (C++ Classes)
 */
class PROJECTOBJECTCAPABILITIES_API FCapabilityRegistry
{
public:
	/**
	 * Get component class by capability ID.
	 * Builds registry on first call (lazy initialization).
	 *
	 * @param CapabilityId Stable ID (e.g., "Lockable", "Hinged", "Sliding")
	 * @return UClass* or nullptr if not found
	 */
	static UClass* GetCapabilityClass(FName CapabilityId);

	/**
	 * Check if capability ID exists.
	 */
	static bool HasCapability(FName CapabilityId);

	/**
	 * Force registry rebuild (call on hot reload / module load).
	 */
	static void RebuildRegistry();

	/**
	 * Check if a capability type is a motion capability (needs Movable mesh).
	 */
	static bool IsMotionCapability(FName CapabilityId);

private:
	static void EnsureBuilt();
	static void Build();

	static TMap<FName, UClass*> Registry;
	static bool bIsBuilt;
};
