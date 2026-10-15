// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UStaticMesh;
class IProjectWorldLayerProducer;
struct FProjectWorldLayerApplyContext;
struct FProjectWorldLayerArtifactInventory;
struct FProjectWorldLayerInventory;
struct FProjectWorldRealizationResult;

namespace ProjectWorldMaterialReferenceUpdate
{
bool HashGeometry(UStaticMesh& Mesh, FString& Hash, FString& Error);
bool Update(const IProjectWorldLayerProducer& Producer, const FProjectWorldLayerApplyContext& Context,
    const TArray<FProjectWorldLayerArtifactInventory>& Accepted, FProjectWorldLayerInventory& Inventory,
    FProjectWorldRealizationResult& Result, bool bApply, FString& Error);
}
