// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
class ACharacter;
class UPrimitiveComponent;
class UActorComponent;
class FJsonObject;
struct FProjectWorldCanonicalBundle;
namespace ProjectWorldBasinNavigation
{
    PROJECTWORLDEDITOR_API bool CheckAgent(UWorld& World,ACharacter& Character,FJsonObject& Receipt,FString& Error);
    PROJECTWORLDEDITOR_API bool Prepare(ACharacter& Character,TConstArrayView<TArray<FVector>> Routes,
        UActorComponent*& OwnedInvoker,FString& Error);
    PROJECTWORLDEDITOR_API bool Verify(UWorld& World,ACharacter& Character,const FProjectWorldCanonicalBundle& Bundle,
        TConstArrayView<TArray<FVector>> Routes,TConstArrayView<UPrimitiveComponent*> Terrain,
        TFunctionRef<bool(const FVector&,double&)> Height,FJsonObject& Receipt,FString& Error,
        TFunction<bool(bool,FString&)> ConnectorControl={});
}
#endif
