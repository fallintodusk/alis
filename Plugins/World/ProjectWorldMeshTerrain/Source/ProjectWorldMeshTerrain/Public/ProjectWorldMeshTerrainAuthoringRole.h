#pragma once
#include "CoreMinimal.h"

namespace ProjectWorldMeshTerrainAuthoringRole
{
	inline const FName& Tag()
	{
		static const FName Value(TEXT("ProjectWorld.MeshTerrain.Authoring.v1"));
		return Value;
	}
	inline const FString& CellPrefix()
	{
		static const FString Value(TEXT("ProjectWorld.MeshTerrain.Cell="));
		return Value;
	}
}
