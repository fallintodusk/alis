// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
class UWorld;
class ACharacter;
class APlayerController;
class FAutomationTestBase;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	struct FPawnSelection
	{
		UClass* GameModeClass=nullptr;
		UClass* PawnClass=nullptr;
		FPrimaryAssetId Definition;
	};
	PROJECTWORLDEDITOR_API bool ConfigurePawn(UWorld* World,const FVector& Start,FAutomationTestBase& Test,FPawnSelection& Out);
	PROJECTWORLDEDITOR_API bool ReadPawnSelection(FAutomationTestBase& Test,FPawnSelection& Out);
	PROJECTWORLDEDITOR_API bool ConfiguredPawnRequested();
	PROJECTWORLDEDITOR_API bool SpawnPawn(UWorld* World,const FVector& Start,const FPawnSelection& Selection,FAutomationTestBase& Test,ACharacter*& Out,APlayerController*& Controller);
}
#endif
