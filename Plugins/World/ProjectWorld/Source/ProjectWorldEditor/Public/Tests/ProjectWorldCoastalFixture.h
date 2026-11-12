// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldStoredSurface.h"
class FAutomationTestBase;
class UStaticMesh;
class UPackage;
class UWorld;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldCoastalFixture
{
    struct FUpper
    {
        FProjectWorldCanonicalBundle Frame;
        TArray<FProjectWorldStoredSurface> Units;
        TArray<FString> Groups;
        TArray<TArray<int32>> SourceOrdinals;
    };
    PROJECTWORLDEDITOR_API bool LoadUpper(const FString& Path,const FString& Hash,const FProjectWorldCanonicalBundle& Frame,FUpper& Out,FString& Error);
    PROJECTWORLDEDITOR_API UStaticMesh* CompileUpper(const FProjectWorldStoredSurface& Unit,FAutomationTestBase& Test,UPackage& Package);
    PROJECTWORLDEDITOR_API bool SameUpper(const FProjectWorldStoredSurface& Unit,UStaticMesh& Mesh,FAutomationTestBase& Test);
    PROJECTWORLDEDITOR_API bool ConfigureGrid(UWorld& World,const FVector& Origin,FString& Error);
    PROJECTWORLDEDITOR_API bool AddOriginalWater(const FString& Path,const FString& Hash,FProjectWorldCanonicalBundle& Bundle,FString& Error);
}
#endif
