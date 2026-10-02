#include "ProjectWorldTerrainRuntimeRole.h"

#include "GameFramework/Actor.h"

namespace ProjectWorldTerrainRuntimeRole
{
	const FName& RoleTag()
	{
		static const FName Value(TEXT("ProjectWorld.Terrain.v1"));
		return Value;
	}

	bool HasRole(const AActor& Actor)
	{
		return Actor.ActorHasTag(RoleTag());
	}
}
