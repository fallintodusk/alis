// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMaterialReferencePipeline.h"
#include "ProjectWorldLayerDirtyInput.h"
#include "ProjectWorldLayerProducerRegistry.h"

bool ProjectWorldMaterialReferencePipeline::Apply(UWorld& World, const FProjectWorldCanonicalBundle& Bundle,
    const FProjectWorldRealizationProfile& Profile, const FProjectWorldAuthoredOverlaySet& Overlays,
    const FProjectWorldLayerDirtyInput& DirtyInput, const TMap<FName,UMaterialInterface*>& Materials,
    FProjectWorldRealizationResult& Result, FString& Error)
{
    if (Result.ExecutionLayerIds.IsEmpty() || DirtyInput.MaterialReferenceArtifacts.Num()!=Result.ExecutionLayerIds.Num())
    {Error=TEXT("Material-only operation requires exact selected prior membership."); return false;}
    for (const auto& Pair : DirtyInput.MaterialReferenceArtifacts)
        if (!Result.ExecutionLayerIds.Contains(Pair.Key)) {Error=TEXT("Unselected material membership is forbidden."); return false;}
    TSet<FName> Roles;
    for (const FString& Id : Result.ExecutionLayerIds)
    {
        const auto* Layer=Profile.Layers.FindByPredicate([&Id](const auto& L){return L.LayerId==Id;});
        if (!Layer) {Error=TEXT("Selected material layer is absent."); return false;}
        const auto* Producer=ProjectWorldLayerProducerRegistry::Find(Layer->GeneratorId,Layer->GeneratorVersion,Error);
        if (!Producer) return false;
        Roles.Add(Producer->GetDeclaration().PresentationMaterialRole);
    }
    for (FName Role : DirtyInput.MaterialReferenceChangedRoles)
        if (!Roles.Contains(Role)) {Error=TEXT("Changed material declaration targets an unselected owner."); return false;}
    // Validate every selected owner before the first package mutation. The existing
    // outer transaction restores persisted packages if a later save fails.
    for (bool bApply : {false,true})
    {
        for (const FString& Id : Result.ExecutionLayerIds)
        {
            const auto* Layer=Profile.Layers.FindByPredicate([&Id](const auto& L){return L.LayerId==Id;});
            auto* Inventory=Result.LayerInventories.FindByPredicate([&Id](const auto& L){return L.LayerId==Id;});
            const auto* Accepted=DirtyInput.MaterialReferenceArtifacts.Find(Id);
            if (!Layer || !Inventory || !Accepted) {Error=TEXT("Material owner or accepted membership is missing."); return false;}
            const auto* Producer=ProjectWorldLayerProducerRegistry::Find(Layer->GeneratorId,Layer->GeneratorVersion,Error);
            if (!Producer) return false;
            const FProjectWorldLayerInputs Inputs{Bundle,Profile,*Layer,Overlays};
            const FProjectWorldLayerApplyContext Context{World,Inputs,Inventory->FinalDirtyUnits,Materials};
            if (!Producer->UpdateMaterialReferences(Context,*Accepted,*Inventory,Result,bApply,Error)) return false;
        }
    }
    return true;
}
