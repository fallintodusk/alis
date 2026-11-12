// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "Tests/ProjectWorldCoastalFixture.h"
namespace UE::MeshPartition { class UMeshPartitionDefinition; class FMeshData; }
namespace ProjectWorldTerrainStorageFixture { struct FSplitSections; }
#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldCoastalSceneTest
{
    inline const FName UpperTag(TEXT("ProjectWorld.Test.CoastalUpper"));
    bool ValidateInput(FAutomationTestBase& Test,const FJsonObject& Document,const FProjectWorldCanonicalBundle& Bundle);
    bool Prepare(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
        UE::MeshPartition::UMeshPartitionDefinition& Definition,const UE::MeshPartition::UMeshPartitionDefinition& Shared);
    bool ApplyWater(FAutomationTestBase& Test,UWorld& World,const FJsonObject& Document,ProjectWorldBasinSceneTest::FInput& Input);
    bool CheckWater(FAutomationTestBase& Test,UWorld& World,const ProjectWorldBasinSceneTest::FInput& Input,FJsonObject& Receipt);
    bool InstallUpper(FAutomationTestBase& Test,UWorld& World,const FJsonObject& Document,const FProjectWorldCanonicalBundle& Bundle,
        const FString& Mount,TArray<FString>& Packages,FJsonObject& Receipt);
    bool BindNativeParts(FAutomationTestBase& Test,const FProjectWorldCanonicalBundle& Bundle,
        const UE::MeshPartition::FMeshData& Native,const FTransform& Parent,ProjectWorldTerrainStorageFixture::FSplitSections& Split);
    bool Queue(FAutomationTestBase& Test,ProjectWorldBasinSceneTest::FInput&& Input,const FJsonObject& Document);
}
#endif
