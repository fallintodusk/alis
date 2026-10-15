// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialPatternBindings.h"
#include "ProjectMaterialSurfaceDependency.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Utilities/ProjectSha256.h"

FString ProjectMaterialPatternBindings::Contract(const FProjectMaterialSurfaceDependency& P)
{
	return P.NativeResourceHashes.IsEmpty() ? P.OutputContract : TEXT("native_single_layer_water_v1");
}

bool ProjectMaterialPatternBindings::Resolve(const FProjectMaterialSurfaceDependency& P,
	TArray<FBinding>& Out, FString& Error)
{
	Out.Reset();
	Error.Reset();
	if (!P.NativeResourceHashes.IsEmpty()) return true;
	TArray<FName> Slots, Parameters;
	if (P.OutputContract == TEXT("aggregate_signals_normal_v1"))
	{ Slots={TEXT("Signals"),TEXT("Normal")}; Parameters={TEXT("SurfaceSignals"),TEXT("SurfaceNormal")}; }
	else if (P.OutputContract == TEXT("soil_structure_normal_masks_v1"))
	{ Slots={TEXT("Structure"),TEXT("Normal"),TEXT("Masks")}; Parameters={TEXT("SurfaceStructure"),TEXT("SurfaceNormal"),TEXT("SurfaceMasks")}; }
	else if (P.OutputContract == TEXT("surface_structure_rgb_v1"))
	{ Slots={TEXT("Structure")}; Parameters={TEXT("PatternStructure")}; }
	else { Error=TEXT("Unsupported material pattern output ABI."); return false; }
	auto* Primary=LoadObject<UTextureRenderTarget2D>(nullptr,*P.OutputObjectPath);
	auto* First=Primary ? Cast<UProjectTextureOutputSlot>(Primary->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())) : nullptr;
	auto* C=First ? First->Catalog.Get() : nullptr;
	FString Filename, Hash;
	if (!C || C->SemanticIdentity!=P.SemanticIdentity || C->PatternId!=FName(P.PatternId) ||
		C->OutputContract!=FName(P.OutputContract) || C->GetOutermost()!=Primary->GetOutermost() ||
		C->GetOutermost()->IsDirty() || !FPackageName::DoesPackageExist(C->GetOutermost()->GetName(), &Filename) ||
		!FProjectSha256::HashFile(Filename,Hash) || Hash!=P.PackageSha256 ||
		!FPaths::IsSamePath(Filename,C->GetOutermost()->GetLoadedPath().GetLocalFullPath()))
	{ Error=TEXT("Pattern source is dirty, foreign or differs from its authenticated package."); return false; }
	const bool Filtered=P.OutputContract==TEXT("soil_structure_normal_masks_v1") && C->Nodes.Num()==4;
	const int32 ExportStart=Filtered ? 1 : 0;
	if (Slots.Num()>1 && (C->OutputSlots.Num()!=Slots.Num() || C->Nodes.Num()!=Slots.Num()+(Filtered ? 1 : 0) || C->OutputSlots[0]!=First))
	{ Error=TEXT("Pattern source bundle cardinality or primary slot is invalid."); return false; }
	if (Filtered && (!C->Nodes[0].ParentNodeIds.IsEmpty() || C->Nodes[1].ParentNodeIds!=TArray<FName>{C->Nodes[0].NodeId}))
	{ Error=TEXT("Pattern filter has no coherent private raw parent."); return false; }
	for (int32 I=0; I<Slots.Num(); ++I)
	{
		auto* S=Slots.Num()==1 ? First : C->OutputSlots[I].Get();
		auto* T=S ? S->GetRenderTarget() : nullptr;
		const auto* N=S ? C->Nodes.FindByPredicate([S](const auto& Node){return Node.NodeId==S->NodeId;}) : nullptr;
		if (!T || !N || S->Catalog!=C || S->SlotName!=Slots[I] || T->GetOutermost()!=Primary->GetOutermost() ||
			T->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())!=S ||
			N->Width<=0 || N->Width>1024 || N->Height!=N->Width || N->Format!=RTF_RGBA8 || !N->bGenerateMips ||
			T->SizeX!=N->Width || T->SizeY!=N->Height || T->RenderTargetFormat!=RTF_RGBA8 || T->IsSRGB() ||
			!T->bAutoGenerateMips || T->AddressX!=TA_Wrap || T->AddressY!=TA_Wrap ||
			T->MipsAddressU!=TA_Wrap || T->MipsAddressV!=TA_Wrap || T->Filter!=TF_Trilinear || T->MipsSamplerFilter!=TF_Trilinear ||
			(Slots.Num()>1 && (N!=&C->Nodes[I+ExportStart] || (I>0 && N->ParentNodeIds!=TArray<FName>{C->Nodes[ExportStart].NodeId}))))
		{ Error=TEXT("Pattern output descriptor or membership is incompatible."); Out.Reset(); return false; }
		Out.Add({Parameters[I],T});
	}
	return true;
}

bool ProjectMaterialPatternBindings::ValidateSource(const FProjectMaterialSurfaceRecipe& R,
	const FProjectMaterialSurfaceDependency& P, FString& Error)
{
	const FString Expected=R.Archetype==TEXT("aggregate_metric") ? TEXT("aggregate_signals_normal_v1") :
		(R.Archetype==TEXT("height_masks_uv") || FProjectMaterialSurfaceRecipeContract::IsMetricHeightArchetype(R.Archetype)) ? TEXT("soil_structure_normal_masks_v1") :
		R.Archetype==TEXT("single_layer_water") ? TEXT("native_single_layer_water_v1") : TEXT("surface_structure_rgb_v1");
	if (Contract(P)!=Expected)
	{ Error=TEXT("Recipe shader family and pattern output ABI are incompatible."); return false; }
	TArray<FBinding> Bindings;
	return Resolve(P,Bindings,Error);
}

bool ProjectMaterialPatternBindings::ValidateParent(const FProjectMaterialSurfaceRecipe& R,
	const FProjectMaterialSurfaceDependency& P, UMaterialInterface* Parent, TArray<FBinding>& Out, FString& Error)
{
	if (!Parent || Parent->GetOutermost()->IsDirty() ||
		Parent->GetOutermost()->GetMetaData().GetValue(Parent,TEXT("ProjectSurfaceArchetype"))!=R.Archetype ||
		Parent->GetOutermost()->GetMetaData().GetValue(Parent,TEXT("ProjectPatternOutputContract"))!=Contract(P))
	{ Error=TEXT("Instance parent is dirty or has an incompatible shader family/output ABI."); return false; }
	if (!Resolve(P,Out,Error)) return false;
	for (const auto& B : Out)
	{
		UTexture* Existing=nullptr;
		if (!Parent->GetTextureParameterValue(FMaterialParameterInfo(B.Parameter),Existing))
		{ Error=TEXT("Instance parent does not expose its declared pattern texture parameter."); return false; }
	}
	return true;
}

bool ProjectMaterialPatternBindings::VerifyBindings(const FProjectMaterialSurfaceRecipe& R,
	const FProjectMaterialSurfaceDependency& P, const UMaterialInstanceConstant* I, FString& Error)
{
	TArray<FBinding> Bindings;
	// The one-output triplanar family also has real accepted parents with inline
	// samples. Their inherited source can be verified but cannot be retargeted.
	if (I && I->Parent && P.OutputContract==TEXT("surface_structure_rgb_v1") &&
		(FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(R.Archetype) || R.Archetype==TEXT("object_metric")) &&
		I->Parent->GetOutermost()->GetMetaData().GetValue(I->Parent,TEXT("ProjectPatternOutputContract")).IsEmpty())
	{
		if (I->Parent->GetOutermost()->IsDirty() ||
			I->Parent->GetOutermost()->GetMetaData().GetValue(I->Parent,TEXT("ProjectSurfaceArchetype"))!=R.Archetype ||
			!Resolve(P,Bindings,Error)) return false;
		int32 Count=0;
		for (const auto& N : I->Parent->GetMaterial()->GetExpressionCollection().Expressions)
			if (const auto* S=Cast<UMaterialExpressionTextureSample>(N.Get()))
			{
				if (S->Texture!=Bindings[0].Target) { Error=TEXT("Inherited parent source differs from the authenticated recipe."); return false; }
				++Count;
			}
		if (Count!=(R.Archetype==TEXT("terrain_detail_metric") ? 6 : 3))
		{ Error=TEXT("Inherited triplanar source is incomplete."); return false; }
		return true;
	}
	if (!I || !ValidateParent(R,P,I->Parent,Bindings,Error)) return false;
	for (const auto& B : Bindings)
	{
		UTexture* Actual=nullptr;
		if (!I->GetTextureParameterValue(FMaterialParameterInfo(B.Parameter),Actual) || Actual!=B.Target)
		{ Error=TEXT("Reloaded instance does not consume its authenticated pattern bundle."); return false; }
	}
	if (!P.NativeResourceHashes.IsEmpty())
	{
		UTexture* Expected=nullptr; UTexture* Actual=nullptr;
		if (!I->Parent->GetTextureParameterValue(FMaterialParameterInfo(TEXT("RippleNormal")),Expected) || !Expected ||
			!P.NativeResourceHashes.Contains(Expected->GetPathName()) ||
			!I->GetTextureParameterValue(FMaterialParameterInfo(TEXT("RippleNormal")),Actual) || Actual!=Expected)
		{ Error=TEXT("Reloaded water instance has an unauthenticated native texture binding."); return false; }
	}
	return true;
}

bool ProjectMaterialPatternBindings::VerifyInstance(const FProjectMaterialSurfaceRecipe& R,
	const FProjectMaterialSurfaceDependency& P,const UMaterialInstanceConstant* I,FString& Error)
{
	if (FProjectMaterialSurfaceRecipeContract::RequiresDirectNaniteUsage(R.Archetype) && (!I || !I->Parent ||
		!I->GetUsageByFlag(MATUSAGE_Nanite) || !I->Parent->GetUsageByFlag(MATUSAGE_Nanite)))
	{ Error=TEXT("Geometry instance or parent lacks its authenticated Nanite usage."); return false; }
	if (!VerifyBindings(R,P,I,Error)) return false;
	for (const auto& V : R.Scalars)
	{
		float Actual=0;
		if (!I->GetScalarParameterValue(FMaterialParameterInfo(V.Key),Actual) || !FMath::IsNearlyEqual(Actual,float(V.Value),1.e-6f))
		{ Error=TEXT("Reloaded instance scalar differs from its recipe."); return false; }
	}
	for (const auto& V : R.Vectors)
	{
		FLinearColor Actual;
		if (!I->GetVectorParameterValue(FMaterialParameterInfo(V.Key),Actual) || !Actual.Equals(V.Value,1.e-6f))
		{ Error=TEXT("Reloaded instance vector differs from its recipe."); return false; }
	}
	return true;
}
