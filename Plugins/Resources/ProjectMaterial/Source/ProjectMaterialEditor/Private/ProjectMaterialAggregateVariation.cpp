// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialAggregateVariation.h"
#include "ProjectMaterialExpressionUtils.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionWorldPosition.h"

namespace ProjectMaterialAggregateVariation
{
namespace
{
using namespace ProjectMaterialExpressionUtils;
using E=UMaterialExpression;
constexpr TCHAR Path[]=TEXT("/Engine/Functions/Engine_MaterialFunctions03/Texturing/TextureVariation.TextureVariation");
bool Unswizzled(const FExpressionInput& I)
{
    if (!I.Expression) return false;
    const auto& Outputs=I.Expression->GetOutputs();
    if (!Outputs.IsValidIndex(I.OutputIndex)) return false;
    const auto& O=Outputs[I.OutputIndex];
    return I.Mask==O.Mask && (!I.Mask ||
        (I.MaskR==O.MaskR && I.MaskG==O.MaskG && I.MaskB==O.MaskB && I.MaskA==O.MaskA));
}
const FExpressionInput* Input(const UMaterialExpressionMaterialFunctionCall* F,const TCHAR* Name)
{
    if (F) for (const auto& I : F->FunctionInputs) if (I.Input.InputName==Name) return &I.Input;
    return nullptr;
}
const E* Source(const UMaterialExpressionMaterialFunctionCall* F,const TCHAR* Name)
{const auto* I=Input(F,Name); return I && I->OutputIndex==0 && Unswizzled(*I) ? I->Expression : nullptr;}
bool Value(const E* N,float V)
{const auto* C=Cast<UMaterialExpressionConstant>(N); return C && FMath::IsNearlyEqual(C->R,V,1.e-6f);}
bool Param(const E* N,const TCHAR* Name)
{const auto* P=Cast<UMaterialExpressionScalarParameter>(N); return P && P->ParameterName==Name;}
bool Function(const UMaterialExpressionMaterialFunctionCall* F)
{return F && F->MaterialFunction && F->MaterialFunction->GetPathName()==Path;}
bool UV(const E* N)
{
    const auto* D=Cast<UMaterialExpressionDivide>(N);
    const auto* XY=D ? Cast<UMaterialExpressionComponentMask>(D->A.Expression) : nullptr;
    const auto* Units=D ? Cast<UMaterialExpressionMultiply>(D->B.Expression) : nullptr;
    return XY && XY->R && XY->G && !XY->B && !XY->A && XY->Input.Expression &&
        XY->Input.Expression->IsA<UMaterialExpressionWorldPosition>() && Units &&
        Param(Units->A.Expression,TEXT("TileMeters")) && Value(Units->B.Expression,100);
}
bool Gradient(const FExpressionInput& I,const UMaterialExpressionMaterialFunctionCall* F,int32 Index)
{
    const auto* G=Cast<UMaterialExpressionMultiply>(I.Expression);
    const auto* P=G ? Cast<UMaterialExpressionPower>(G->B.Expression) : nullptr;
    return I.OutputIndex==0 && Unswizzled(I) && G && G->A.Expression==F && G->A.OutputIndex==Index && Unswizzled(G->A) && P &&
        Value(P->Base.Expression,2) && Param(P->Exponent.Expression,TEXT("TextureMipBias"));
}
}

bool Bind(UMaterial* M,E* Coords,E* Bias,UTexture* Signals,
    UMaterialExpressionTextureSampleParameter2D* S,UMaterialExpressionTextureSampleParameter2D* N,FString& Error)
{
    auto* FunctionAsset=LoadObject<UMaterialFunction>(nullptr,Path);
    auto* F=Node<UMaterialExpressionMaterialFunctionCall>(M);
    if (!FunctionAsset || !F->SetMaterialFunction(FunctionAsset))
    {Error=TEXT("Native aggregate phase variation is unavailable."); return false;}
    auto* Object=Node<UMaterialExpressionTextureObjectParameter>(M);
    Object->ParameterName=TEXT("SurfaceSignals"); Object->Texture=Signals; Object->SamplerType=SAMPLERTYPE_LinearColor;
    auto* False=Node<UMaterialExpressionStaticBool>(M); False->Value=false;
    auto* Mask=Node<UMaterialExpressionConstant4Vector>(M); Mask->Constant=FLinearColor(1,0,0,0);
    const TMap<FName,E*> Inputs{{TEXT("Heightmap"),Object},{TEXT("UVs"),Coords},
        {TEXT("Variation Scale"),Constant(M,3.236f)},{TEXT("Variation Levels"),Constant(M,6)},
        {TEXT("Heightmap Influence"),Constant(M,.5f)},{TEXT("Mask Channel"),Mask},
        {TEXT("Use Dither"),False},{TEXT("Random Rotation and Scale"),False},{TEXT("HQ Edge Comparison"),False}};
    int32 Bound=0;
    for (auto& I : F->FunctionInputs) if (E* const* V=Inputs.Find(I.Input.InputName)) {I.Input.Connect(0,*V); ++Bound;}
    if (Bound!=Inputs.Num() || F->FunctionOutputs.Num()!=5)
    {Error=TEXT("Native aggregate phase variation signature is unsupported."); return false;}
    // Scale clean derivatives so the existing mip-bias control retains its meaning.
    auto* Scale=Node<UMaterialExpressionPower>(M); Scale->Base.Connect(0,Constant(M,2)); Scale->Exponent.Connect(0,Bias);
    auto* DX=Node<UMaterialExpressionMultiply>(M); DX->A.Connect(2,F); DX->B.Connect(0,Scale);
    auto* DY=Node<UMaterialExpressionMultiply>(M); DY->A.Connect(3,F); DY->B.Connect(0,Scale);
    for (auto* Sample : {S,N})
    {
        Sample->MipValueMode=TMVM_Derivative; Sample->Coordinates.Connect(0,F);
        Sample->CoordinatesDX.Connect(0,DX); Sample->CoordinatesDY.Connect(0,DY);
        Sample->MipValue.Expression=nullptr;
    }
    return true;
}

bool Sample(const UMaterialExpressionTextureSampleParameter2D* S,const UTexture* T)
{
    const auto* F=S ? Cast<UMaterialExpressionMaterialFunctionCall>(S->Coordinates.Expression) : nullptr;
    return S && S->Texture==T && S->SamplerType==SAMPLERTYPE_LinearColor && S->MipValueMode==TMVM_Derivative &&
        S->Coordinates.OutputIndex==0 && Unswizzled(S->Coordinates) && Function(F) && UV(Source(F,TEXT("UVs"))) &&
        Gradient(S->CoordinatesDX,F,2) && Gradient(S->CoordinatesDY,F,3) &&
        !S->MipValue.Expression;
}

bool Verify(UMaterial* M,const UTexture* Signals)
{
    int32 Calls=0,Objects=0;
    for (const auto* E : UMaterialEditingLibrary::GetMaterialExpressions(M))
    {
        if (const auto* O=Cast<UMaterialExpressionTextureObjectParameter>(E))
        {
            if (O->ParameterName!=TEXT("SurfaceSignals") || O->Texture!=Signals || O->SamplerType!=SAMPLERTYPE_LinearColor) return false;
            ++Objects;
        }
        const auto* F=Cast<UMaterialExpressionMaterialFunctionCall>(E);
        if (!Function(F)) continue;
        ++Calls;
        const auto* O=Cast<UMaterialExpressionTextureObjectParameter>(Source(F,TEXT("Heightmap")));
        const auto* Mask=Cast<UMaterialExpressionConstant4Vector>(Source(F,TEXT("Mask Channel")));
        if (!O || O->ParameterName!=TEXT("SurfaceSignals") || O->Texture!=Signals || !UV(Source(F,TEXT("UVs"))) ||
            !Value(Source(F,TEXT("Variation Scale")),3.236f) || !Value(Source(F,TEXT("Variation Levels")),6) ||
            !Value(Source(F,TEXT("Heightmap Influence")),.5f) || !Mask || !Mask->Constant.Equals(FLinearColor(1,0,0,0),1.e-6f) ||
            F->FunctionInputs.Num()!=9 || F->FunctionOutputs.Num()!=5) return false;
        for (const TCHAR* Name : {TEXT("Use Dither"),TEXT("Random Rotation and Scale"),TEXT("HQ Edge Comparison")})
        {const auto* B=Cast<UMaterialExpressionStaticBool>(Source(F,Name)); if (!B || B->Value) return false;}
    }
    return Calls==1 && Objects==1;
}
}
