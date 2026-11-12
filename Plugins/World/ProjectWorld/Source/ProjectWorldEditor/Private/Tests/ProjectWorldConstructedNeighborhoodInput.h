// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldConstructedRuntimeInput.h"
class FJsonObject;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedNeighborhood
{
	struct FGroup
	{
		FString Id,Role;
		TArray<FString> RequiredCells;
		TArray<FProjectWorldSurfaceTriangle> Field;
	};
	struct FScene
	{
		FProjectWorldCanonicalBundle Frame;
		TArray<FGroup> Groups;
		ProjectWorldConstructedRuntimeTest::FInput Body;
		TArray<FString> BodyCells;
		FString Hash,GeometryHash,OutputPath;
		bool SharedPartition=false;
		TSharedPtr<FJsonObject> BodyQuality;
		FVector2D BodyAxis=FVector2D::ZeroVector;
	};
	struct FUnit
	{
		FString GroupId,CellId;
		FProjectWorldStoredSurface Surface;
		TArray<int32> Sources;
	};
	struct FExpectedUnit { FString ContentHash; int32 Faces=0; };
	using FExpectedOutputs=TMap<FString,FExpectedUnit>;
	// Capture only from successful Build output or authenticated prior Build output.
	// The digest binds later data to that coverage/provenance proof; it cannot establish one.
	bool CaptureBuildOutput(const FUnit& Unit,FExpectedOutputs& Out,FString& Error);
	bool ParseGroups(const FJsonObject& Manifest,const FJsonObject& Surfaces,
		const FProjectWorldCanonicalBundle& Frame,TArray<FGroup>& Out,FString& Error);
	bool Load(const FString& Path,const FString& Expected,FScene& Out,FString& Error);
	bool ValidateCensus(const FScene& Scene,const TArray<FUnit>& Units,const FExpectedOutputs& Expected,FString& Error);
}
#endif
