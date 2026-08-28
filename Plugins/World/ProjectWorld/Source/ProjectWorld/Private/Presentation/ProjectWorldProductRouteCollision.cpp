// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductRouteCollision.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

namespace ProjectWorldProductRouteCollision
{
	int32 CountBlockingPrimitives(const AActor& Actor)
	{
		int32 Count = 0;
		TInlineComponentArray<UPrimitiveComponent*> Components;
		Actor.GetComponents(Components);
		for (const UPrimitiveComponent* Primitive : Components)
		{
			if (Primitive != nullptr && Primitive->IsRegistered() && Primitive->IsCollisionEnabled() &&
				Primitive->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				++Count;
			}
		}
		return Count;
	}
}
