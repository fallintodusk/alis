// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Tests/ProjectWorldConstructedRuntimePawn.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
class FAutomationTestBase;
class FJsonObject;
class FRHIBuffer;
class AActor;
class UStaticMesh;
class ACharacter;
class IAutomationLatentCommand;
namespace ProjectWorldCompositeTerrainTests
{
    bool ReadBuffer(FRHIBuffer* Buffer,uint32 Bytes,TArray<uint8>& Out);
}
namespace ProjectWorldBasinSceneTest
{
    struct FInput
    {
        FString Root,Mount,Directory,PackageName,MapSha256;
        bool PluginManaged=false;
        bool NavigationOnly=false;
        bool StreamingOnly=false;
        TArray<FString> Packages;
        FProjectWorldCanonicalBundle Bundle;
        ProjectWorldConstructedRuntimeTest::FPawnSelection Pawn;
        TSharedPtr<FJsonObject> Expected;
        FVector Start;
        FName AdditionalSpatialTag;
        TArray<FVector> AdditionalSupportWorldPoints;
        TOptional<FVector> ReturnWalkTargetWorld;
    };
    // Takes the entire disposable package/mount lease, including on failure.
    bool Queue(FAutomationTestBase& Test,FInput&& Input);
    bool Replay(FAutomationTestBase& Test,const FString& Root,FProjectWorldCanonicalBundle Bundle,const FJsonObject& Document,const FString& FixtureHash,const FString& DefinitionHash,bool Coastal=false);
    TSharedRef<IAutomationLatentCommand> StreamingReturn(FAutomationTestBase& Test,const FInput& Input,
        const TArray<TArray<FVector>>& Routes,TSharedRef<FJsonObject> Receipt,ACharacter& Pawn,
        TFunction<bool(const FVector&,double&)> Height,TFunction<bool(UWorld*,ACharacter*)> Measure);
    bool BuildWithNativeCommandlet(FAutomationTestBase& Test,const FInput& Input);
    bool PrepareNativePlugin(FAutomationTestBase& Test,const FInput& Input);
    void DescribePackage(FAutomationTestBase& Test,const FString& Name);
    bool OwnsSavedActor(const AActor* Actor,const UWorld* World,const FInput& Input,const FString& ExpectedPackage);
    bool ApplyJointWater(FAutomationTestBase& Test,UWorld* World,const FJsonObject& Document,FInput& Input);
    bool MeasureJointWater(FAutomationTestBase& Test,UWorld* World,const FInput& Input,const FString& Prefix,FJsonObject& Receipt);
    void TrackWaterInstances(UWorld* World,TArray<TWeakObjectPtr<UObject>>& Instances);
    TStrongObjectPtr<UStaticMesh> CompileEncodingCandidate(FAutomationTestBase& Test,UStaticMesh* Source,int32 Precision,FJsonObject& Metrics);
}
#endif
