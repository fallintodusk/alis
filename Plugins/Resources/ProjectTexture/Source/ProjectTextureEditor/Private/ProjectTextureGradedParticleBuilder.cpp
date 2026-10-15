// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureGradedParticleBuilder.h"
#include "ProjectTextureGradedParticleControls.h"
#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureSoilBuilder.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionStep.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace ProjectTextureGradedParticleBuilderPrivate
{
using E=UMaterialExpression;
template<class T> T* Node(UMaterial* M) { return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M,T::StaticClass())); }
E* C(UMaterial* M,float V) { auto* N=Node<UMaterialExpressionConstant>(M); N->R=V; return N; }
E* Control(UMaterial* M,const FProjectTexturePatternRecipe& R,const FString& Name)
{
    auto* N=Node<UMaterialExpressionScalarParameter>(M); N->ParameterName=FName(Name);
    N->DefaultValue=R.StructureControls.FindChecked(FName(Name)); N->Desc=TEXT("Structure.")+Name; return N;
}
template<class T> E* Binary(UMaterial* M,E* A,E* B) { auto* N=Node<T>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N; }
E* Add(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionAdd>(M,A,B); }
E* Sub(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionSubtract>(M,A,B); }
E* Mul(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionMultiply>(M,A,B); }
E* Div(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionDivide>(M,A,B); }
E* Append(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionAppendVector>(M,A,B); }
E* Mask(UMaterial* M,E* A,bool R,bool G,bool B)
{
    auto* N=Node<UMaterialExpressionComponentMask>(M); N->R=R; N->G=G; N->B=B; N->A=false; N->Input.Connect(0,A); return N;
}
template<class T> E* Unary(UMaterial* M,E* A) { auto* N=Node<T>(M); N->Input.Connect(0,A); return N; }
E* Lerp(UMaterial* M,E* A,E* B,E* Alpha)
{
    auto* N=Node<UMaterialExpressionLinearInterpolate>(M); N->A.Connect(0,A); N->B.Connect(0,B); N->Alpha.Connect(0,Alpha); return N;
}
E* Noise(UMaterial* M,E* P,const FProjectTexturePatternRecipe& R,const TCHAR* Name)
{
    auto* N=Node<UMaterialExpressionNoise>(M); const int32 Frequency=R.StructureControls.FindChecked(Name);
    N->NoiseFunction=NOISEFUNCTION_GradientALU; N->Scale=Frequency; N->Quality=1; N->Levels=1;
    N->bTiling=true; N->RepeatSize=Frequency; N->OutputMin=0; N->OutputMax=1; N->bTurbulence=false;
    N->Position.Connect(0,P); N->Desc=TEXT("Structure.")+FString(Name); return N;
}
E* Raw(UMaterial* M,const FProjectTexturePatternRecipe& R)
{
    E* UV=Node<UMaterialExpressionTextureCoordinate>(M);
    auto* Phase=CastChecked<UMaterialExpressionConstant>(C(M,.19f+float(R.Seed)*.38196601125f)); Phase->Desc=TEXT("StructureSeed");
    E* P=Append(M,UV,Phase); E* Patch=Noise(M,P,R,TEXT("PatchFrequency")); E* Micro=Noise(M,P,R,TEXT("MicroFrequency"));
    E* Height=Mul(M,Micro,Control(M,R,TEXT("BaseMicroHeight"))); E* Occupancy=C(M,0); E* Grain=nullptr;
    for (const TCHAR* Band : {TEXT("Large"),TEXT("Medium"),TEXT("Small"),TEXT("Grain")})
    {
        const FString Prefix(Band); const int32 Frequency=R.StructureControls.FindChecked(FName(Prefix+TEXT("Frequency")));
        auto* Period=CastChecked<UMaterialExpressionConstant>(C(M,Frequency)); Period->Desc=TEXT("Structure.")+Prefix+TEXT("Frequency");
        E* Scaled=Mul(M,P,Period);
        auto* Voronoi=Node<UMaterialExpressionVectorNoise>(M); Voronoi->NoiseFunction=VNF_VoronoiALU;
        Voronoi->Quality=4; Voronoi->bTiling=true; Voronoi->TileSize=Frequency; Voronoi->Position.Connect(0,Scaled);
        E* Seed=Mask(M,Voronoi,true,true,true);
        E* Wrapped=Unary<UMaterialExpressionFrac>(M,Div(M,Seed,Period));
        auto* Cell=Node<UMaterialExpressionVectorNoise>(M); Cell->NoiseFunction=VNF_CellnoiseALU; Cell->Position.Connect(0,Mul(M,Wrapped,Period));
        E* Delta=Sub(M,Mask(M,Scaled,true,true,false),Mask(M,Seed,true,true,false));
        E* Distance=Binary<UMaterialExpressionDistance>(M,Mask(M,Scaled,true,true,false),Mask(M,Seed,true,true,false));
        E* Sin=Unary<UMaterialExpressionSine>(M,Mask(M,Cell,false,false,true));
        E* Cos=Unary<UMaterialExpressionCosine>(M,Mask(M,Cell,false,false,true));
        E* X=Mask(M,Delta,true,false,false); E* Y=Mask(M,Delta,false,true,false);
        E* LocalX=Sub(M,Mul(M,X,Cos),Mul(M,Y,Sin)); E* LocalY=Add(M,Mul(M,X,Sin),Mul(M,Y,Cos));
        E* Stretch=Lerp(M,C(M,.78f),C(M,1.28f),Mask(M,Cell,true,false,false));
        E* Faceted=Mul(M,Binary<UMaterialExpressionMax>(M,Unary<UMaterialExpressionAbs>(M,Mul(M,LocalX,Stretch)),Unary<UMaterialExpressionAbs>(M,Div(M,LocalY,Stretch))),C(M,1.15f));
        Distance=Lerp(M,Distance,Faceted,Control(M,R,TEXT("FacetStrength")));
        E* Radius=Lerp(M,C(M,.20f),C(M,.55f),Mask(M,Cell,true,false,false));
        E* Profile=Unary<UMaterialExpressionClamp>(M,Sub(M,C(M,1),Div(M,Distance,Radius)));
        auto* Shaped=Node<UMaterialExpressionPower>(M); Shaped->Base.Connect(0,Profile); Shaped->Exponent.Connect(0,Control(M,R,Prefix+TEXT("Profile")));
        E* Density=Mul(M,Control(M,R,Prefix+TEXT("Density")),Lerp(M,C(M,.45f),C(M,1.25f),Patch));
        auto* Selected=Node<UMaterialExpressionStep>(M); Selected->Y.Connect(0,Mask(M,Cell,false,true,false)); Selected->X.Connect(0,Density);
        E* Occupied=Mul(M,Shaped,Selected);
        E* Relief=Mul(M,Occupied,Mul(M,Control(M,R,Prefix+TEXT("Height")),Lerp(M,C(M,.55f),C(M,1),Mask(M,Cell,false,false,true))));
        Height=Binary<UMaterialExpressionMax>(M,Height,Relief); Occupancy=Binary<UMaterialExpressionMax>(M,Occupancy,Occupied); Grain=Occupied;
    }
    E* Edges=Mul(M,Mul(M,Occupancy,Sub(M,C(M,1),Occupancy)),C(M,4));
    E* EdgeGrains=Mul(M,Micro,Mul(M,Edges,Mul(M,Patch,Control(M,R,TEXT("EdgeMicroHeight")))));
    Height=Unary<UMaterialExpressionClamp>(M,Add(M,C(M,.20f),Add(M,Height,EdgeGrains)));
    E* SurfaceGrain=Mul(M,Micro,Mul(M,Control(M,R,TEXT("AggregateSurfaceGrain")),Mul(M,Patch,Occupancy)));
    Height=Unary<UMaterialExpressionClamp>(M,Add(M,Height,SurfaceGrain));
    Height=Unary<UMaterialExpressionClamp>(M,Add(M,Height,Mul(M,Grain,Control(M,R,TEXT("RaisedGrainHeight")))));
    return Append(M,Append(M,Height,Unary<UMaterialExpressionClamp>(M,Occupancy)),Grain);
}
bool Finish(UMaterial* M,E* RGB,FString& Error)
{
    M->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); M->GetEditorOnlyData()->Opacity.Connect(0,C(M,1));
    M->MaterialDomain=MD_UI; M->BlendMode=BLEND_AlphaComposite; M->SetShadingModel(MSM_Unlit); M->PostEditChange();
    const auto Errors=UMaterialEditingLibrary::RecompileMaterial(M);
    if (!Errors.IsEmpty()) { Error=FString::Join(Errors,TEXT(" | ")); return false; }
    M->MarkPackageDirty(); return true;
}
bool Filter(UMaterial* M,UTextureRenderTarget2D* RawTarget,FString& Error)
{
    auto* Texture=Node<UMaterialExpressionTextureObjectParameter>(M); Texture->ParameterName=TEXT("RawStructure"); Texture->Texture=RawTarget; Texture->SamplerType=SAMPLERTYPE_LinearColor;
    auto* Function=Node<UMaterialExpressionMaterialFunctionCall>(M);
    auto* Native=LoadObject<UMaterialFunction>(nullptr,ProjectTextureGradedParticleControls::FilterFunctionPath);
    if (!Native || Native->GetOutermost()->IsDirty() || !Function->SetMaterialFunction(Native))
    { Error=TEXT("Native graded-particle filter is absent or dirty."); return false; }
    TMap<FName,E*> Inputs={{TEXT("TextureObject"),Texture},{TEXT("UV"),Node<UMaterialExpressionTextureCoordinate>(M)},
        {TEXT("Distance Steps"),C(M,2)},{TEXT("Radial Steps"),C(M,8)},{TEXT("Distance"),C(M,.35f/512)},
        {TEXT("Radial Offset"),C(M,0)},{TEXT("Kernel Power"),C(M,1)}};
    int32 Bound=0;
    for (auto& I : Function->FunctionInputs)
        if (auto* Value=Inputs.Find(I.Input.InputName)) { I.Input.Connect(0,*Value); ++Bound; }
    if (Bound!=Inputs.Num()) { Error=TEXT("Native filter signature is unsupported."); return false; }
    return Finish(M,Function,Error);
}
bool Shape(const FProjectTexturePatternRecipe& R,FString& Error)
{
    if (R.Family!=TEXT("graded_particles") || R.OutputContract!=TEXT("soil_structure_normal_masks_v1") ||
        R.ResolutionClass!=TEXT("medium") || R.Seed<0 || R.Seed>255 || R.Nodes.Num()!=4 || R.Exports.Num()!=3 ||
        !ProjectTextureGradedParticleControls::Validate(R.StructureControls,Error))
    { if (Error.IsEmpty()) Error=TEXT("Graded-particle source has an unsupported bounded shape."); return false; }
    return true;
}
bool VerifyRaw(const FProjectTexturePatternRecipe& R,UMaterial* M,FString& Error)
{
    TSet<E*> Visited; TSet<FName> Found; TArray<E*> Queue{M->GetEditorOnlyData()->EmissiveColor.Expression};
    int32 VoronoiCount=0,CellCount=0; bool PhaseFound=false;
    while (!Queue.IsEmpty())
    {
        auto* N=Queue.Pop(); if (!N || Visited.Contains(N)) continue; Visited.Add(N);
        if (N->Desc.StartsWith(TEXT("Structure.")))
        {
            const FName Name(*N->Desc.RightChop(10)); const double* Expected=R.StructureControls.Find(Name);
            const auto* Scalar=Cast<UMaterialExpressionScalarParameter>(N);
            const auto* Constant=Cast<UMaterialExpressionConstant>(N);
            const auto* Gradient=Cast<UMaterialExpressionNoise>(N);
            const float Value=Scalar ? Scalar->DefaultValue : Constant ? Constant->R : Gradient ? Gradient->Scale : -1;
            if (!Expected || !FMath::IsNearlyEqual(Value,float(*Expected),1.e-6f) ||
                (Gradient && (!Gradient->bTiling || Gradient->RepeatSize!=int32(*Expected) || Gradient->Levels!=1 || Gradient->NoiseFunction!=NOISEFUNCTION_GradientALU)))
            { Error=TEXT("Connected graded-particle control differs: ")+Name.ToString(); return false; }
            Found.Add(Name);
        }
        if (N->Desc==TEXT("StructureSeed"))
        {
            const auto* Phase=Cast<UMaterialExpressionConstant>(N);
            if (!Phase || !FMath::IsNearlyEqual(Phase->R,.19f+float(R.Seed)*.38196601125f,1.e-6f))
            { Error=TEXT("Connected graded-particle seed differs."); return false; }
            PhaseFound=true;
        }
        if (const auto* V=Cast<UMaterialExpressionVectorNoise>(N))
        {
            if (V->NoiseFunction==VNF_VoronoiALU)
            {
                const auto* Scale=Cast<UMaterialExpressionMultiply>(V->Position.Expression);
                const auto* Period=Scale ? Cast<UMaterialExpressionConstant>(Scale->B.Expression) : nullptr;
                if (!Period || !Period->Desc.StartsWith(TEXT("Structure.")) || !V->bTiling || V->Quality!=4 || V->TileSize!=int32(Period->R))
                { Error=TEXT("Native Voronoi frequency and position must consume the same period."); return false; }
                ++VoronoiCount;
            }
            else if (V->NoiseFunction==VNF_CellnoiseALU) ++CellCount;
            else { Error=TEXT("Unsupported vector-noise kernel."); return false; }
        }
        for (FExpressionInputIterator It{N};It;++It) Queue.Add(It->Expression);
    }
    if (Found.Num()!=R.StructureControls.Num() || !PhaseFound || VoronoiCount!=4 || CellCount!=4)
    { Error=TEXT("Graded-particle inputs or native populations are disconnected."); return false; }
    return true;
}
bool VerifyFilter(const UMaterial* M,const UTextureRenderTarget2D* Structure,FString& Error)
{
    const auto* F=Cast<UMaterialExpressionMaterialFunctionCall>(M->GetEditorOnlyData()->EmissiveColor.Expression);
    if (!F || !F->MaterialFunction || F->MaterialFunction->GetPathName()!=ProjectTextureGradedParticleControls::FilterFunctionPath)
    { Error=TEXT("Graded particles do not consume the admitted native filter."); return false; }
    const TMap<FName,float> Values={{TEXT("Distance Steps"),2},{TEXT("Radial Steps"),8},{TEXT("Distance"),.35f/512},
        {TEXT("Radial Offset"),0},{TEXT("Kernel Power"),1}};
    int32 Found=0;
    for (const auto& I : F->FunctionInputs)
    {
        if (const auto* Expected=Values.Find(I.Input.InputName))
        {
            const auto* V=Cast<UMaterialExpressionConstant>(I.Input.Expression);
            if (!V || !FMath::IsNearlyEqual(V->R,*Expected,1.e-7f)) { Error=TEXT("Native filter controls differ."); return false; }
            ++Found;
        }
        else if (I.Input.InputName==TEXT("UV"))
        {
            if (!Cast<UMaterialExpressionTextureCoordinate>(I.Input.Expression)) { Error=TEXT("Native filter UV differs."); return false; }
            ++Found;
        }
        else if (I.Input.InputName==TEXT("TextureObject"))
        {
            const auto* T=Cast<UMaterialExpressionTextureObjectParameter>(I.Input.Expression);
            if (!T || T->ParameterName!=TEXT("RawStructure") || T->Texture!=Structure || T->SamplerType!=SAMPLERTYPE_LinearColor)
            { Error=TEXT("Native filter raw input is foreign or disconnected."); return false; }
            ++Found;
        }
    }
    if (Found!=7) { Error=TEXT("Native filter signature differs."); return false; }
    return true;
}
}

bool ProjectTextureGradedParticleBuilder::Build(const FProjectTexturePatternRecipe& R,const FString& PackageName,UObject*& Asset,FString& Error)
{
    using namespace ProjectTextureGradedParticleBuilderPrivate;
    Asset=nullptr; if (!Shape(R,Error)) return false;
    auto* Package=CreatePackage(*PackageName);
    auto* Catalog=NewObject<UProjectTextureRuntimeCatalog>(Package,*R.CatalogId,RF_Public|RF_Standalone);
    Catalog->PatternId=FName(R.PatternId); Catalog->OutputContract=FName(R.OutputContract); Catalog->BudgetBytes=32*1024*1024;
    const FString ObjectPath=PackageName+TEXT(".")+R.Exports[0].DescriptorId;
    Catalog->SemanticIdentity=FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(R,ObjectPath);
    TArray<UTextureRenderTarget2D*> Targets;
    for (int32 I=0;I<3;++I)
    {
        const FString Name=R.Exports[I].DescriptorId;
        auto* T=NewObject<UTextureRenderTarget2D>(Package,*Name,RF_Public|RF_Standalone);
        T->RenderTargetFormat=RTF_RGBA8; T->SRGB=false; T->bForceLinearGamma=true; T->bAutoGenerateMips=true;
        T->AddressX=T->AddressY=T->MipsAddressU=T->MipsAddressV=TA_Wrap; T->Filter=T->MipsSamplerFilter=TF_Trilinear;
        T->InitAutoFormat(512,512); Targets.Add(T);
        auto* Slot=NewObject<UProjectTextureOutputSlot>(T); Slot->Catalog=Catalog; Slot->NodeId=FName(R.Exports[I].NodeId);
        Slot->SlotName=FName(R.Exports[I].Slot); T->AddAssetUserData(Slot); Catalog->OutputSlots.Add(Slot);
        FAssetRegistryModule::AssetCreated(T);
    }
    for (int32 I=0;I<4;++I)
    {
        auto* M=NewObject<UMaterial>(Package,*FString::Printf(TEXT("M_ProjectGradedGenerator_%d"),I),RF_Public|RF_Standalone);
        const bool Built=I==0 ? Finish(M,Raw(M,R),Error) : I==1 ? Filter(M,Targets[0],Error) :
            ProjectTextureSoilBuilder::BuildDerivedMaterial(M,Targets[0],512,I==2,Error);
        if (!Built) return false;
        auto& N=Catalog->Nodes.AddDefaulted_GetRef(); N.NodeId=FName(R.Nodes[I].NodeId); N.StructuralIdentity=R.Nodes[I].StructuralIdentity;
        for (const auto& Parent : R.Nodes[I].Parents) N.ParentNodeIds.Add(FName(Parent));
        N.GeneratorMaterial=M; N.ParentTextureParameter=I==0 ? NAME_None : I==1 ? FName(TEXT("RawStructure")) : FName(TEXT("ParentStructure"));
        N.Width=N.Height=512; FAssetRegistryModule::AssetCreated(M);
        Package->GetMetaData().SetValue(M,TEXT("ProjectGenerationOwner"),TEXT("ProjectTexture"));
        Package->GetMetaData().SetValue(M,TEXT("ProjectPatternId"),*R.PatternId);
    }
    FAssetRegistryModule::AssetCreated(Catalog);
    for (UObject* O : TArray<UObject*>{Catalog,Targets[0],Targets[1],Targets[2]})
    {
        Package->GetMetaData().SetValue(O,TEXT("ProjectGenerationOwner"),TEXT("ProjectTexture"));
        Package->GetMetaData().SetValue(O,TEXT("ProjectPatternId"),*R.PatternId);
    }
    Package->MarkPackageDirty(); Asset=Targets[0]; return true;
}

bool ProjectTextureGradedParticleBuilder::Verify(const FProjectTexturePatternRecipe& R,UObject* Asset,FString& Error)
{
    using namespace ProjectTextureGradedParticleBuilderPrivate;
    if (!Shape(R,Error)) return false;
    auto* Target=Cast<UTextureRenderTarget2D>(Asset);
    auto* Slot=Target ? Cast<UProjectTextureOutputSlot>(Target->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())) : nullptr;
    auto* Catalog=Slot ? Slot->Catalog.Get() : nullptr;
    if (!Catalog || Catalog->Nodes.Num()!=4 || Catalog->OutputSlots.Num()!=3 || Catalog->GetOutermost()!=Asset->GetOutermost() ||
        Catalog->SemanticIdentity!=FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(R,Asset->GetPathName()))
    { Error=TEXT("Graded-particle catalog differs from its admitted identity."); return false; }
    for (int32 I=0;I<4;++I)
    {
        const auto& N=Catalog->Nodes[I]; TArray<FName> Parents;
        for (const auto& P : R.Nodes[I].Parents) Parents.Add(FName(P));
        auto* M=Cast<UMaterial>(N.GeneratorMaterial);
        if (!M || M->GetOutermost()!=Asset->GetOutermost() || N.NodeId!=FName(R.Nodes[I].NodeId) ||
            N.StructuralIdentity!=R.Nodes[I].StructuralIdentity || N.ParentNodeIds!=Parents || N.Width!=512 || N.Height!=512 ||
            N.ParentTextureParameter!=(I==0 ? NAME_None : I==1 ? FName(TEXT("RawStructure")) : FName(TEXT("ParentStructure"))) ||
            M->MaterialDomain!=MD_UI || M->BlendMode!=BLEND_AlphaComposite || !M->GetEditorOnlyData()->EmissiveColor.Expression)
        { Error=TEXT("Graded-particle generator or runtime DAG differs."); return false; }
        const auto* Opacity=Cast<UMaterialExpressionConstant>(M->GetEditorOnlyData()->Opacity.Expression);
        if (!Opacity || !FMath::IsNearlyEqual(Opacity->R,1.f) || !M->GetShadingModels().HasShadingModel(MSM_Unlit) ||
            Asset->GetOutermost()->GetMetaData().GetValue(M,TEXT("ProjectGenerationOwner"))!=TEXT("ProjectTexture") ||
            Asset->GetOutermost()->GetMetaData().GetValue(M,TEXT("ProjectPatternId"))!=R.PatternId)
        { Error=TEXT("Graded-particle generator owner or unlit output differs."); return false; }
        if ((!I && !VerifyRaw(R,M,Error)) || (I==1 && !VerifyFilter(M,Target,Error))) return false;
        if (I>=2 && !ProjectTextureSoilBuilder::VerifyDerivedMaterial(M,Target,512,I==2,Error)) return false;
        if (I)
        {
            const auto* S=Catalog->OutputSlots[I-1].Get(); auto* T=S ? S->GetRenderTarget() : nullptr;
            if (!T || S->Catalog!=Catalog || S->NodeId!=N.NodeId || S->SlotName!=FName(R.Exports[I-1].Slot) ||
                T->GetName()!=R.Exports[I-1].DescriptorId || T->GetOutermost()!=Asset->GetOutermost() || T->SizeX!=512 || T->SizeY!=512 ||
                T->IsSRGB() || !T->bAutoGenerateMips || T->RenderTargetFormat!=RTF_RGBA8 || T->AddressX!=TA_Wrap || T->AddressY!=TA_Wrap ||
                T->MipsAddressU!=TA_Wrap || T->MipsAddressV!=TA_Wrap || T->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())!=S ||
                T->Filter!=TF_Trilinear || T->MipsSamplerFilter!=TF_Trilinear)
            { Error=TEXT("Graded-particle export format or ownership differs."); return false; }
        }
    }
    return true;
}
