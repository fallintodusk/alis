// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldProductRouteGateConfig
{
	FString OperationId;
	FString ResultPath;
	FString MapPackage;
	FString RuntimeProfileId;
	FString RuntimeProfileHash;
	FString MachineProfileId;
	FString TerrainAcceptancePath;
	FString TerrainAcceptanceHash;
	FString BuildingReferencePath;
	FString BuildingReferenceHash;
	FVector EdgeLocation = FVector::ZeroVector;
	bool bRestorePreviewFlight = false;
	bool bRequireGameplayInteraction = true;
	bool bForceFreshVsm = false;
	bool bMaterialEvidence = false;
};

namespace ProjectWorldProductRouteConfig
{
	bool ParseCommandLine(FProjectWorldProductRouteGateConfig& OutConfig, FString& OutError);
}
