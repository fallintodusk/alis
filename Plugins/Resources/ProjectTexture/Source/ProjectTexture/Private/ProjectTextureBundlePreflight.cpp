// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureBundlePreflight.h"
#include "ProjectTextureRuntimeCatalog.h"

bool ProjectTextureBundlePreflight::Validate(const UProjectTextureOutputSlot* Requested,
	TMap<FName, UTextureRenderTarget2D*>& OutTargets, FString& OutError, bool bRequireMaterial)
{
	OutTargets.Reset();
	auto* C = Requested ? Requested->Catalog.Get() : nullptr;
	if (!C || C->SemanticIdentity.Len() != 64 || C->Nodes.IsEmpty())
	{
		OutError = TEXT("Invalid runtime texture catalog identity."); return false;
	}
	const bool bSoil = C->OutputContract == TEXT("soil_structure_normal_masks_v1");
	if ((!bSoil && C->OutputContract != TEXT("surface_structure_rgb_v1")) ||
		(bSoil && (C->OutputSlots.Num() != 3 || C->Nodes.Num() != 3)) ||
		(!bSoil && !C->OutputSlots.IsEmpty()))
	{
		OutError = TEXT("Unsupported runtime texture bundle contract."); return false;
	}
	TSet<FName> Seen;
	TSet<FString> Identities;
	int64 PeakBytes = 0;
	for (const auto& N : C->Nodes)
	{
		if (N.NodeId.IsNone() || Seen.Contains(N.NodeId) || N.StructuralIdentity.Len() != 64 ||
			Identities.Contains(N.StructuralIdentity) || (bRequireMaterial && !N.GeneratorMaterial) || N.Width <= 0 || N.Width > 1024 ||
			N.Height != N.Width || N.Format != RTF_RGBA8 || !N.bGenerateMips || N.ParentNodeIds.Num() > 1 ||
			(!N.ParentNodeIds.IsEmpty() && (N.ParentTextureParameter.IsNone() || !Seen.Contains(N.ParentNodeIds[0]))))
		{
			OutError = TEXT("Invalid runtime DAG node, identity, parent, or resource format."); return false;
		}
		for (int32 Size = N.Width; Size > 0; Size /= 2) PeakBytes += int64(Size) * Size * 4;
		Seen.Add(N.NodeId); Identities.Add(N.StructuralIdentity);
	}
	if (PeakBytes > C->BudgetBytes || C->BudgetBytes <= 0)
	{
		OutError = TEXT("Runtime bundle peak exceeds its generation budget."); return false;
	}
	TArray<const UProjectTextureOutputSlot*> Slots;
	if (bSoil)
	{
		for (UProjectTextureOutputSlot* Slot : C->OutputSlots) Slots.Add(Slot);
	}
	else Slots.Add(Requested);
	const TArray<FName> Names = bSoil ? TArray<FName>{TEXT("Structure"), TEXT("Normal"), TEXT("Masks")}
		: TArray<FName>{TEXT("Structure")};
	bool bFound = false;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		auto* Slot = Slots[Index];
		auto* RT = Slot ? Slot->GetRenderTarget() : nullptr;
		const auto* N = Slot ? C->Nodes.FindByPredicate([Slot](const auto& Node) { return Node.NodeId == Slot->NodeId; }) : nullptr;
		if (!RT || !N || Slot->Catalog != C || Slot->SlotName != Names[Index] ||
			OutTargets.Contains(Slot->NodeId) || RT->GetOutermost() != C->GetOutermost() ||
			RT->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass()) != Slot ||
			(bSoil && (N != &C->Nodes[Index] || (Index > 0 && N->ParentNodeIds != TArray<FName>{C->Nodes[0].NodeId}))))
		{
			OutError = TEXT("Invalid or foreign runtime output slot."); return false;
		}
		OutTargets.Add(Slot->NodeId, RT);
		bFound |= Slot == Requested;
	}
	if (!bFound) { OutError = TEXT("Requested slot is not a member of the bundle."); return false; }
	return true;
}
