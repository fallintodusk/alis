#pragma once

#include "CoreMinimal.h"

class AActor;

namespace ProjectWorldTerrainRuntimeRole
{
	PROJECTWORLD_API const FName& RoleTag();
	PROJECTWORLD_API bool HasRole(const AActor& Actor);
}
