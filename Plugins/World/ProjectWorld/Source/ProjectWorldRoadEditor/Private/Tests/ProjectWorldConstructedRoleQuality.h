// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
class FJsonValue;
struct FMeshDescription;

namespace ProjectWorldConstructedRoleQuality
{
	struct FRegion
	{
		FString Role;
		double MaximumNorm=0,MaximumGrade=0,MaximumCrossSlope=0;
	};
	struct FPolicy
	{
		FString Hash;
		TArray<FRegion> Regions;
		TArray<int32> RegionBySource;
	};
	bool Parse(const FJsonObject& Document,const FString& AssemblyHash,const TArray<FString>& SourceRoles,FPolicy& Out,FString& Error);
	bool Load(const FJsonObject& Fixture,const FString& AssemblyHash,const TArray<FString>& SourceRoles,FPolicy& Out,FString& Error);
	bool Validate(const FPolicy& Policy,const FMeshDescription& Mesh,const TArray<int32>& SourceOrdinals,
		const FVector2D& AlongAxis,TArray<TSharedPtr<FJsonValue>>& Receipts,FString& Error);
}
