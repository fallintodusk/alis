// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UWorld;
class FJsonObject;
struct FProjectWorldCanonicalBundle;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionalEvidence
{
    PROJECTWORLDEDITOR_API bool Configure(UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
        const FString& ProfilePath,const FString& ProfileHash,FJsonObject& Receipt,FString& Error);
    PROJECTWORLDEDITOR_API bool Capture(UWorld& World,const FString& PlanPath,const FString& PlanHash,
        const FString& Root,FJsonObject& Receipt,FString& Error);
}
#endif
