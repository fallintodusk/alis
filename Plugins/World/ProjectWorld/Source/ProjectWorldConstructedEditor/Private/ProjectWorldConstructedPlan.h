// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldConstructedInputs.h"
#include "MeshDescription.h"
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalConstructedSurface;
struct FProjectWorldRealizationLayer;

namespace ProjectWorldConstructedRealization
{
	inline const FName SourceFaceAttribute(TEXT("ProjectWorld.SourceFace"));
	struct FUnit
	{
		FString Id,FeatureId,CellId,ModelHash,GeometryHash,InputHash;
		FVector Origin=FVector::ZeroVector;
		FMeshDescription Description;
		const FProjectWorldCanonicalConstructedSurface* Surface=nullptr;
	};
	struct FPlan { TArray<FUnit> Units; TMap<FString,FString> CellHashes; int32 BoundaryContacts=0; };
	bool Describe(FMeshDescription& Description,FString& Hash);
	bool Qualify(const FProjectWorldCanonicalConstructedSurface& Surface,const FMeshDescription& Description,FString& Error);
	bool Plan(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldRealizationLayer& Layer,FPlan& Out,FString& Error);
#if WITH_DEV_AUTOMATION_TESTS
	int64 NativeBuildCallsForTests();
#endif
}
