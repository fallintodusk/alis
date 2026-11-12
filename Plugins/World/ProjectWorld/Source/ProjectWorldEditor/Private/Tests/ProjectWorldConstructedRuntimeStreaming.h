// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UWorld;
class ACharacter;
class FAutomationTestBase;
class IAutomationLatentCommand;
class UWorldPartitionEditorLoaderAdapter;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	struct FInput;
	struct FSavedCandidate;
	struct FPawnSelection;
	struct FStreamingScene
	{
		TArray<FString> Packages;
		TMap<FString,FString> ExternalMounts;
		TWeakObjectPtr<UWorld> EditorWorld;
		TWeakObjectPtr<UWorldPartitionEditorLoaderAdapter> Loader;
		FVector ControlOrigin=FVector::ZeroVector,ControlPoint=FVector::ZeroVector,Far=FVector::ZeroVector;
		FString ControlMesh;
		int32 LoadingRange=0;
		~FStreamingScene();
		bool SaveAndReload(UWorld*& World,const FInput& Input,const FSavedCandidate& Saved,FAutomationTestBase& Test);
		void ReleaseLoader();
		void UnloadPackages(FAutomationTestBase& Test);
	};
	TSharedRef<IAutomationLatentCommand> StreamingReturn(
		FAutomationTestBase& Test,const FInput& Input,const FPawnSelection& Selection,FStreamingScene& Scene,
		TFunction<bool(UWorld*,ACharacter*)> Measure);
}
#endif
