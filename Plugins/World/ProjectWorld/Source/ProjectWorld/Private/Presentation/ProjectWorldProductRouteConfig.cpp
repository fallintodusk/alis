// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductRouteConfig.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	bool ParseProductRouteValue(const TCHAR* Name, FString& OutValue, bool bShouldStopOnSeparator = true)
	{
		return FParse::Value(FCommandLine::Get(), Name, OutValue, bShouldStopOnSeparator) && !OutValue.IsEmpty();
	}

	bool IsProductRouteToken(const FString& Value)
	{
		if (Value.IsEmpty()) return false;
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-')) return false;
		}
		return true;
	}

	bool IsProductRouteSha256(const FString& Value)
	{
		if (Value.Len() != 64) return false;
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character)) return false;
		}
		return true;
	}
}

bool ProjectWorldProductRouteConfig::ParseCommandLine(
	FProjectWorldProductRouteGateConfig& OutConfig,
	FString& OutError)
{
	OutConfig.bRestorePreviewFlight = FParse::Param(
		FCommandLine::Get(), TEXT("ProjectWorldProductRouteRestorePreviewFlight"));
	OutConfig.bRequireGameplayInteraction = !FParse::Param(
		FCommandLine::Get(), TEXT("ProjectWorldProductRouteSkipInteraction"));
	OutConfig.bForceFreshVsm = FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldProductVsmFreshControl"));
	const bool bHasTerrainAcceptance = ParseProductRouteValue(
		TEXT("ProjectWorldProductTerrainAcceptance="), OutConfig.TerrainAcceptancePath);
	const bool bHasTerrainAcceptanceHash = ParseProductRouteValue(
		TEXT("ProjectWorldProductTerrainAcceptanceHash="), OutConfig.TerrainAcceptanceHash);
	if (bHasTerrainAcceptance != bHasTerrainAcceptanceHash ||
		(OutConfig.bForceFreshVsm && !bHasTerrainAcceptance))
	{
		OutError = TEXT("Terrain acceptance path/hash must be paired and are required by the VSM control.");
		return false;
	}
	FString EdgeText;
	if (!ParseProductRouteValue(TEXT("ProjectWorldProductOperation="), OutConfig.OperationId) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductResult="), OutConfig.ResultPath) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductMap="), OutConfig.MapPackage) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductRuntime="), OutConfig.RuntimeProfileId) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductRuntimeHash="), OutConfig.RuntimeProfileHash) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductMachine="), OutConfig.MachineProfileId) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductEdge="), EdgeText, false))
	{
		OutError = TEXT("A required product-route argument is missing.");
		return false;
	}
	TArray<FString> Coordinates;
	EdgeText.ParseIntoArray(Coordinates, TEXT(","), true);
	if (Coordinates.Num() != 3 ||
		!LexTryParseString(OutConfig.EdgeLocation.X, *Coordinates[0]) ||
		!LexTryParseString(OutConfig.EdgeLocation.Y, *Coordinates[1]) ||
		!LexTryParseString(OutConfig.EdgeLocation.Z, *Coordinates[2]))
	{
		OutError = TEXT("The product-route edge must be X,Y,Z numeric coordinates.");
		return false;
	}
	const bool bIdentityValid = IsProductRouteToken(OutConfig.OperationId) &&
		IsProductRouteToken(OutConfig.RuntimeProfileId) && IsProductRouteToken(OutConfig.MachineProfileId) &&
		IsProductRouteSha256(OutConfig.RuntimeProfileHash);
	const bool bEdgeValid = !OutConfig.EdgeLocation.ContainsNaN() &&
		!FVector2D(OutConfig.EdgeLocation.X, OutConfig.EdgeLocation.Y).IsNearlyZero();
	if (!bIdentityValid || !bEdgeValid || FPaths::IsRelative(OutConfig.ResultPath) ||
		!OutConfig.MapPackage.StartsWith(TEXT("/ProjectWorldData/Generated/")))
	{
		OutError = TEXT("The product-route identity, result path, map, or edge is outside the supported contract.");
		return false;
	}
	FPaths::NormalizeFilename(OutConfig.ResultPath);
	return true;
}
