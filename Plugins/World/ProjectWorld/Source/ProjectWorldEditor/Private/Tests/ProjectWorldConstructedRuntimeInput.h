// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldStoredSurface.h"
class FJsonValue;
class FJsonObject;

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	struct FSourceGroup { FString Id,Role; TArray<FProjectWorldSurfaceTriangle> Field; };
	struct FRoute { FString Id; double WidthMeters=0; TArray<FVector> Points; };
	struct FInput
	{
		TArray<FProjectWorldStoredSurface> Units;
		TArray<TArray<int32>> UnitSources;
		TSet<int32> ConnectorSources;
		TArray<FString> UnitGroups;
		TArray<FSourceGroup> SourceGroups;
		TArray<FRoute> Routes;
		TMap<FString,double> DeckHeightsAboveWater;
		FString ConnectorGroup,ConnectorRoute;
		FVector Start=FVector(-200,0,135),End=FVector(2200,0,370);
		FString Hash;
		FProjectWorldCanonicalBundle Frame;
		TArray<FProjectWorldSurfaceTriangle> Field;
		double Lift=0;
		bool Approach=false;
		bool Grouped=false;
		FVector CoreStart=FVector::ZeroVector,CoreEnd=FVector::ZeroVector;
		ProjectWorldConstructedMesh::FSlopeMetrics Slopes;
	};
	bool Vector(const TSharedPtr<FJsonValue>& Value,FVector& Out,int32 Dimensions=3);
	bool Load(FInput& Out,FString& Error,const FString& Path,const FString& Expected);
	bool LoadGrouped(const FJsonObject& Root,const FString& Hash,FInput& Out,FString& Error);
	bool CanonicalHeight(const FInput& Input,const FVector2D& Point,double& Out,FString& Error);
	bool ExpectedHeight(const FInput& Input,const FVector& Point,double& Height);
	// Measures top-face elevation above representative Water, never underside clearance.
	bool MeasureDeckWater(const FInput& Input,double WaterMeters,double MinimumDeckHeightMeters,TMap<FString,double>& Out,FString& Error);
}
#endif
