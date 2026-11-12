// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldCanonicalBundle.h"
class FJsonObject;
namespace ProjectWorldIdentifiedBasinParsing
{
	struct FRetainedInput
	{
		FProjectWorldCanonicalBasinArtifact Artifact;
		FString SourceFeatureHash, CanonicalCrs;
		TSharedPtr<FJsonObject> Document;
		TMap<FString,TSet<FIntPoint>> Selected;
		TMap<FString,double> Parameters;
		int32 MaximumTriangles=0;
	};
	using FRetainedInputs=TMap<FString,FRetainedInput>;
	bool ReadFunction(const TSharedPtr<FJsonObject>& Function,FProjectWorldCanonicalWaterSurface& Surface);
	bool ReadInput(const TSharedPtr<FJsonObject>& Operation,const TSharedPtr<FJsonObject>& Document,
		const FProjectWorldCanonicalBasinArtifact& Artifact,FRetainedInputs& Inputs);
	bool ReadTerrain(const TSharedPtr<FJsonObject>& Document,FProjectWorldCanonicalTerrain& Terrain);
	bool ValidateSources(const TSharedPtr<FJsonObject>& Document,const FRetainedInputs& Inputs,FProjectWorldCanonicalValidation& Validation);
	bool ValidateBundle(const FProjectWorldCanonicalBundle& Bundle,const FRetainedInputs& Inputs,
		const TMap<FString,FProjectWorldCanonicalBasinArtifact>& LoadedTerrain,
		TFunctionRef<bool(const FString&,const FString&,int64,bool)> IsVerifiedOutput,
		FProjectWorldCanonicalValidation& Validation);
}
