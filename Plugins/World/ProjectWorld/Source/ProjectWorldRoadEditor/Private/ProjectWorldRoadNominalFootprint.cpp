// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadNominalFootprint.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldCanonicalBundle.h"

bool ProjectWorldRoadNominalFootprint::Read(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldCanonicalCell& Cell,
	const FProjectWorldRealizationLayer& Layer,const TSet<FString>& FeatureIds,FProjectWorldRoadNominalFootprint& Out,FString& Error)
{
	Out={}; Error.Reset();
	if (FeatureIds.IsEmpty() || FeatureIds.Num()>4096)
	{Error=TEXT("Nominal Road footprint read requires a bounded explicit feature selection."); return false;}
	for (const auto& Id : FeatureIds)
	{
		const auto* Feature=Bundle.Features.Find(Id);
		if (!Feature || Feature->FeatureClass!=TEXT("road"))
		{Error=TEXT("Nominal Road selection has a missing or non-Road feature."); return false;}
	}
	return ProjectWorldRoadRealization::BuildNominalFootprint(Bundle,Cell,Layer,FeatureIds,Out,Error);
}
