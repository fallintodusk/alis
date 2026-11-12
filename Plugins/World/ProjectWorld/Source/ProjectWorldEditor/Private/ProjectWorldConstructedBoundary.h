// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"
#include "ProjectWorldConstructedSurface.h"

namespace ProjectWorldConstructedBoundary
{
	class FConversion
	{
	public:
		using FDomain=TFunction<ProjectWorldConstructedSurface::ESampleResult(const FVector2f&,FString&)>;
		explicit FConversion(FDomain InDomain) : Domain(MoveTemp(InDomain)) {}
		bool Select(const FVector2f& Nearest,FVector2f& Out,FString& Error);
	private:
		TMap<FVector2f,FVector2f> Nodes;
		FDomain Domain;
	};
}
