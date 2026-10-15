// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureAggregateBuilder.h"
#include "ProjectTextureParticleControls.h"
#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace
{
	bool VerifyControls(const FProjectTexturePatternRecipe& R,UMaterial* M,FString& Error)
	{
		TSet<UMaterialExpression*> Visited;
		TArray<UMaterialExpression*> Queue{M->GetEditorOnlyData()->EmissiveColor.Expression};
		TSet<FName> Found;
		while (!Queue.IsEmpty())
		{
			auto* N=Queue.Pop(); if (!N || Visited.Contains(N)) continue;
			Visited.Add(N);
			if (N->Desc.StartsWith(TEXT("Structure.")))
			{
				const FName Name(*N->Desc.RightChop(10)); const double* Expected=R.StructureControls.Find(Name);
				const auto* C=Cast<UMaterialExpressionConstant>(N);
				const auto* Noise=Cast<UMaterialExpressionVectorNoise>(N);
				const auto* XYZ=Noise ? Cast<UMaterialExpressionAppendVector>(Noise->Position.Expression) : nullptr;
				const auto* Z=XYZ ? Cast<UMaterialExpressionConstant>(XYZ->B.Expression) : nullptr;
				const float Phase=Name==TEXT("PatchFrequency") ? 1.7f : Name==TEXT("CoarseFrequency") ? 2.7f : .37f;
				if (!Expected || (C ? !FMath::IsNearlyEqual(C->R,float(*Expected),1.e-6f) :
					!Noise || !Noise->bTiling || Noise->TileSize!=int32(*Expected) || !Z ||
					!FMath::IsNearlyEqual(Z->R,Phase+float(R.Seed)*.38196601125f,1.e-6f)))
				{ Error=TEXT("Connected native structural control differs from the recipe."); return false; }
				Found.Add(Name);
			}
			for (auto* Input : N->GetInputsView()) if (Input) Queue.Add(Input->Expression);
		}
		if (Found.Num()!=R.StructureControls.Num()) { Error=TEXT("A structural control is missing or disconnected."); return false; }
		return true;
	}
	using E=UMaterialExpression;
	template<class T> T* Node(UMaterial* M) { return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M,T::StaticClass())); }
	E* Constant(UMaterial* M,float V) { auto* N=Node<UMaterialExpressionConstant>(M); N->R=V; return N; }
	template<class T> E* Binary(UMaterial* M,E* A,E* B) { auto* N=Node<T>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N; }
	E* Add(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionAdd>(M,A,B); }
	E* Mul(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionMultiply>(M,A,B); }
	E* Sub(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionSubtract>(M,A,B); }
	E* Append(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionAppendVector>(M,A,B); }
	E* Mask(UMaterial* M,E* A,bool R,bool G,bool B,bool Alpha=false)
	{
		auto* N=Node<UMaterialExpressionComponentMask>(M); N->R=R; N->G=G; N->B=B; N->A=Alpha; N->Input.Connect(0,A); return N;
	}
	E* Saturate(UMaterial* M,E* A) { auto* N=Node<UMaterialExpressionSaturate>(M); N->Input.Connect(0,A); return N; }
	E* Mix(UMaterial* M,E* A,E* B,float Weight,const TCHAR* Role)
	{
		auto* W=CastChecked<UMaterialExpressionConstant>(Constant(M,Weight)); W->Desc=TEXT("Structure.")+FString(Role);
		auto* N=Node<UMaterialExpressionLinearInterpolate>(M); N->A.Connect(0,A); N->B.Connect(0,B); N->Alpha.Connect(0,W); return N;
	}
	E* Noise(UMaterial* M,E* UV,uint32 Frequency,int32 Seed,const TCHAR* Role,EVectorNoiseFunction Kind=VNF_GradientALU,float Z=.37f)
	{
		auto* N=Node<UMaterialExpressionVectorNoise>(M); N->NoiseFunction=Kind; N->Quality=3; N->bTiling=true; N->TileSize=Frequency;
		N->Desc=TEXT("Structure.")+FString(Role);
		N->Position.Connect(0,Append(M,Mul(M,UV,Constant(M,Frequency)),Constant(M,Z+float(Seed)*.38196601125f))); return N;
	}
	E* Cells(UMaterial* M,E* Voronoi,uint32 Period)
	{
		auto* N=Node<UMaterialExpressionVectorNoise>(M); N->NoiseFunction=VNF_CellnoiseALU; N->Quality=1; N->bTiling=true; N->TileSize=Period;
		N->Position.Connect(0,Mask(M,Voronoi,true,true,true)); return N;
	}
	E* Signals(UMaterial* M,const FProjectTexturePatternRecipe& R)
	{
		auto Control=[&](const TCHAR* Name)
		{
			auto* N=CastChecked<UMaterialExpressionConstant>(Constant(M,float(R.StructureControls.FindChecked(Name))));
			N->Desc=TEXT("Structure.")+FString(Name); return static_cast<E*>(N);
		};
		auto* UV=Node<UMaterialExpressionTextureCoordinate>(M);
		auto* Warp=Mask(M,Noise(M,UV,uint32(R.StructureControls.FindChecked(TEXT("WarpFrequency"))),R.Seed,TEXT("WarpFrequency"),VNF_VectorALU),true,true,false);
		auto* Warped=Add(M,UV,Mul(M,Warp,Control(TEXT("WarpAmplitude"))));
		auto* Fine=Noise(M,Warped,uint32(R.StructureControls.FindChecked(TEXT("FineFrequency"))),R.Seed,TEXT("FineFrequency"),VNF_VoronoiALU); auto* Cell=Cells(M,Fine,uint32(R.StructureControls.FindChecked(TEXT("FineFrequency"))));
		auto* Size=Add(M,Control(TEXT("GrainRadius")),Mul(M,Mask(M,Cell,false,true,false),Control(TEXT("GrainRadiusVariation"))));
		auto* Grain=Saturate(M,Sub(M,Constant(M,1),Binary<UMaterialExpressionDivide>(M,Mask(M,Fine,false,false,false,true),Size)));
		Grain=Saturate(M,Mul(M,Grain,Control(TEXT("GrainContrast"))));
		auto* Micro=Add(M,Mul(M,Mask(M,Noise(M,UV,uint32(R.StructureControls.FindChecked(TEXT("MicroFrequency"))),R.Seed,TEXT("MicroFrequency")),false,false,false,true),Constant(M,.5f)),Constant(M,.5f));
		auto* Patch=Add(M,Mul(M,Mask(M,Noise(M,UV,uint32(R.StructureControls.FindChecked(TEXT("PatchFrequency"))),R.Seed,TEXT("PatchFrequency"),VNF_GradientALU,1.7f),false,false,false,true),Constant(M,.5f)),Constant(M,.5f));
		auto* Coarse=Noise(M,UV,uint32(R.StructureControls.FindChecked(TEXT("CoarseFrequency"))),R.Seed,TEXT("CoarseFrequency"),VNF_VoronoiALU,2.7f); auto* CoarseCell=Cells(M,Coarse,uint32(R.StructureControls.FindChecked(TEXT("CoarseFrequency"))));
		auto* FineParticles=Saturate(M,Mul(M,Sub(M,Mask(M,Cell,true,false,false),Control(TEXT("FineOccupancyThreshold"))),Control(TEXT("FineOccupancyContrast"))));
		auto* CoarseParticles=Saturate(M,Mul(M,Sub(M,Mask(M,CoarseCell,true,false,false),Control(TEXT("CoarseOccupancyThreshold"))),Control(TEXT("CoarseOccupancyContrast"))));
		auto* FineRelief=Mul(M,Grain,Add(M,Control(TEXT("FineReliefBase")),Mul(M,FineParticles,Control(TEXT("FineReliefVariation")))));
		auto* Grit=Saturate(M,Sub(M,Constant(M,1),Mul(M,Mask(M,Coarse,false,false,false,true),Control(TEXT("CoarseRadiusFactor")))));
		auto* Grains=Mix(M,FineRelief,Mul(M,Grit,CoarseParticles),float(R.StructureControls.FindChecked(TEXT("CoarseMix"))),TEXT("CoarseMix"));
		auto* Height=Add(M,Mul(M,Grains,Control(TEXT("HeightAggregate"))),Mul(M,Micro,Control(TEXT("HeightMicro"))));
		auto* Pigment=Saturate(M,Add(M,Mul(M,Grains,Control(TEXT("PigmentAggregate"))),Mul(M,Patch,Control(TEXT("PigmentPatch")))));
		return Append(M,Height,Append(M,Pigment,Mix(M,Patch,Micro,float(R.StructureControls.FindChecked(TEXT("RoughnessMicroMix"))),TEXT("RoughnessMicroMix"))));
	}
	bool Normal(UMaterial* M,UTextureRenderTarget2D* Parent,int32 Size,FString& Error)
	{
		auto* Texture=Node<UMaterialExpressionTextureObjectParameter>(M); Texture->ParameterName=TEXT("HeightSource"); Texture->Texture=Parent; Texture->SamplerType=SAMPLERTYPE_LinearColor;
		auto* F=Node<UMaterialExpressionMaterialFunctionCall>(M);
		auto* Function=LoadObject<UMaterialFunction>(nullptr,TEXT("/Engine/Functions/Engine_MaterialFunctions03/Procedurals/NormalFromHeightmap.NormalFromHeightmap"));
		if (!Function || !F->SetMaterialFunction(Function)) { Error=TEXT("Native aggregate normal function is unavailable."); return false; }
		auto* Channel=Node<UMaterialExpressionConstant4Vector>(M); Channel->Constant=FLinearColor(1,0,0,0);
		const TMap<FName,E*> Inputs{{TEXT("Height Map"),Texture},{TEXT("Coordinates"),Node<UMaterialExpressionTextureCoordinate>(M)},
			{TEXT("Height Map UV Offset"),Constant(M,1.f/Size)},{TEXT("Normal Map Intensity"),Constant(M,.0004f/(.32f/Size))},{TEXT("Height Map Channel Selector"),Channel}};
		int32 Bound=0;
		for (auto& Input : F->FunctionInputs) if (E* const* Value=Inputs.Find(Input.Input.InputName)) { Input.Input.Connect(0,*Value); ++Bound; }
		if (Bound!=Inputs.Num()) { Error=TEXT("Native aggregate normal signature is unsupported."); return false; }
		auto* Unit=Node<UMaterialExpressionNormalize>(M); Unit->VectorInput.Connect(0,F);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,Add(M,Mul(M,Unit,Constant(M,.5f)),Constant(M,.5f))); return true;
	}
}

bool ProjectTextureAggregateBuilder::Build(const FProjectTexturePatternRecipe& R,const FString& PackageName,UObject*& OutAsset,FString& Error)
{
	OutAsset=nullptr;
	if (R.Nodes.Num()!=2 || R.Exports.Num()!=2 || R.Seed<0 || R.Seed>255 || R.ResolutionClass!=TEXT("medium") ||
		!ProjectTextureParticleControls::Validate(R.StructureControls,Error))
	{ if (Error.IsEmpty()) Error=TEXT("Particle source requires its bounded two-node 512-square contract."); return false; }
	UPackage* P=CreatePackage(*PackageName);
	auto* C=NewObject<UProjectTextureRuntimeCatalog>(P,*R.CatalogId,RF_Public|RF_Standalone);
	C->PatternId=FName(R.PatternId); C->OutputContract=FName(R.OutputContract); C->BudgetBytes=16*1024*1024;
	C->SemanticIdentity=FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(R,PackageName+TEXT(".")+R.Exports[0].DescriptorId);
	TArray<UTextureRenderTarget2D*> Targets;
	for (const auto& Export : R.Exports)
	{
		auto* RT=NewObject<UTextureRenderTarget2D>(P,*Export.DescriptorId,RF_Public|RF_Standalone);
		RT->RenderTargetFormat=RTF_RGBA8; RT->SRGB=false; RT->bForceLinearGamma=true; RT->bAutoGenerateMips=true;
		RT->AddressX=RT->AddressY=RT->MipsAddressU=RT->MipsAddressV=TA_Wrap; RT->Filter=RT->MipsSamplerFilter=TF_Trilinear; RT->InitAutoFormat(512,512);
		auto* Slot=NewObject<UProjectTextureOutputSlot>(RT); Slot->Catalog=C; Slot->NodeId=FName(Export.NodeId); Slot->SlotName=FName(Export.Slot);
		RT->AddAssetUserData(Slot); C->OutputSlots.Add(Slot); Targets.Add(RT); FAssetRegistryModule::AssetCreated(RT);
		P->GetMetaData().SetValue(RT,TEXT("ProjectGenerationOwner"),TEXT("ProjectTexture")); P->GetMetaData().SetValue(RT,TEXT("ProjectPatternId"),*R.PatternId);
	}
	for (int32 Index=0; Index<2; ++Index)
	{
		auto* M=NewObject<UMaterial>(P,*FString::Printf(TEXT("M_ProjectAggregateGenerator_%d"),Index),RF_Public|RF_Standalone);
		if (!Index) M->GetEditorOnlyData()->EmissiveColor.Connect(0,Signals(M,R)); else if (!Normal(M,Targets[0],512,Error)) return false;
		M->GetEditorOnlyData()->Opacity.Connect(0,Constant(M,1)); M->MaterialDomain=MD_UI; M->BlendMode=BLEND_AlphaComposite; M->SetShadingModel(MSM_Unlit); M->PostEditChange();
		const auto Errors=UMaterialEditingLibrary::RecompileMaterial(M); if (!Errors.IsEmpty()) { Error=FString::Join(Errors,TEXT(" | ")); return false; }
		FAssetRegistryModule::AssetCreated(M); P->GetMetaData().SetValue(M,TEXT("ProjectGenerationOwner"),TEXT("ProjectTexture")); P->GetMetaData().SetValue(M,TEXT("ProjectPatternId"),*R.PatternId);
		auto& N=C->Nodes.AddDefaulted_GetRef(); N.NodeId=FName(R.Nodes[Index].NodeId); N.StructuralIdentity=R.Nodes[Index].StructuralIdentity;
		for (const FString& Parent : R.Nodes[Index].Parents) N.ParentNodeIds.Add(FName(Parent));
		N.GeneratorMaterial=M; N.ParentTextureParameter=Index ? FName(TEXT("HeightSource")) : NAME_None; N.Width=N.Height=512;
	}
	FAssetRegistryModule::AssetCreated(C); P->GetMetaData().SetValue(C,TEXT("ProjectGenerationOwner"),TEXT("ProjectTexture")); P->GetMetaData().SetValue(C,TEXT("ProjectPatternId"),*R.PatternId);
	P->MarkPackageDirty(); OutAsset=Targets[0]; return true;
}

bool ProjectTextureAggregateBuilder::Verify(const FProjectTexturePatternRecipe& R,UObject* Asset,FString& Error)
{
	auto* RT=Cast<UTextureRenderTarget2D>(Asset); auto* S=RT ? Cast<UProjectTextureOutputSlot>(RT->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())) : nullptr;
	auto* C=S ? S->Catalog.Get() : nullptr;
	if (!C || R.Nodes.Num()!=2 || R.Exports.Num()!=2 || C->PatternId!=FName(R.PatternId) || C->OutputContract!=FName(R.OutputContract) || C->Nodes.Num()!=2 || C->OutputSlots.Num()!=2 || C->SemanticIdentity!=FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(R,Asset->GetPathName()))
	{
		Error=TEXT("Reloaded aggregate source identity is invalid."); return false;
	}
	for (int32 I=0; I<2; ++I)
	{
		const auto* Slot=C->OutputSlots[I].Get(); auto* Target=Slot ? Slot->GetRenderTarget() : nullptr; const auto& N=C->Nodes[I]; const auto* M=Cast<UMaterial>(N.GeneratorMaterial);
		TArray<FName> Parents; for (const FString& Parent : R.Nodes[I].Parents) Parents.Add(FName(Parent));
		if (!Target || !M || Target->GetOutermost()!=Asset->GetOutermost() || M->GetOutermost()!=Asset->GetOutermost() || Slot->Catalog!=C || Slot->NodeId!=N.NodeId || Slot->SlotName!=FName(R.Exports[I].Slot) ||
			N.NodeId!=FName(R.Nodes[I].NodeId) || N.ParentNodeIds!=Parents || N.ParentTextureParameter!=(I ? FName(TEXT("HeightSource")) : NAME_None) || Target->GetName()!=R.Exports[I].DescriptorId ||
			Target->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())!=Slot ||
			N.StructuralIdentity!=R.Nodes[I].StructuralIdentity || N.Width!=512 || N.Height!=512 || Target->SizeX!=512 || Target->SizeY!=512 || Target->RenderTargetFormat!=RTF_RGBA8 || Target->IsSRGB() || !Target->bAutoGenerateMips ||
			Target->AddressX!=TA_Wrap || Target->AddressY!=TA_Wrap || Target->MipsAddressU!=TA_Wrap || Target->MipsAddressV!=TA_Wrap || Target->Filter!=TF_Trilinear || Target->MipsSamplerFilter!=TF_Trilinear ||
			M->MaterialDomain!=MD_UI || M->BlendMode!=BLEND_AlphaComposite || !M->GetEditorOnlyData()->EmissiveColor.Expression)
		{
			Error=TEXT("Reloaded aggregate slot or generator contract is invalid."); return false;
		}
	}
	if (!VerifyControls(R,CastChecked<UMaterial>(C->Nodes[0].GeneratorMaterial),Error)) return false;
	return true;
}
