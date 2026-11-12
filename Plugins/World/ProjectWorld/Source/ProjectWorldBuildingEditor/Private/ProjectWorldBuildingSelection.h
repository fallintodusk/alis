#pragma once
#include "ProjectWorldBuildingAssembly.h"
#include "ProjectWorldLayerProducerRegistry.h"

namespace ProjectWorldBuildingSelection
{
    struct FSelected
    {
        FString FeatureId, CellId, KitHash;
        ProjectBuildingAssembly::FKit Kit;
    };
    struct FSet {TArray<FSelected> Buildings;bool bConfigured=false;};
    bool ValidateSetting(const FString& Value);
    bool Load(const FProjectWorldLayerInputs& Inputs,FSet& Out,FString& Error);
    bool HashCell(const FProjectWorldLayerInputs& Inputs,const FSet& Set,
        const FProjectWorldCanonicalCell& Cell,FString& Hash,FString& Error);
    bool BuildCell(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldCanonicalCell& Cell,
        const FProjectWorldAuthoredOverlaySet& Overlays,const FProjectWorldBuildingSettings& Settings,
        const FSet& Set,FProjectWorldBuildingMeshBuildResult& Out,FString& Error);
    bool Prepare(const FProjectWorldLayerInputs& Inputs,const FSelected& Selection,
        ProjectWorldBuildingAssembly::FPrepared& Out,FString& Error);
    bool Apply(const FProjectWorldLayerApplyContext& Context,const FSet& Set,
        FProjectWorldLayerInventory& Inventory,FProjectWorldRealizationResult& Result,FString& Error);
    bool Capture(const FProjectWorldLayerApplyContext& Context,const FSet& Set,
        FProjectWorldLayerInventory& Inventory,FProjectWorldRealizationResult& Result,FString& Error);
}
