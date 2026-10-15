// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialAggregateProjection.h"
#include "ProjectMaterialExpressionUtils.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionSign.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"

namespace
{
using E=UMaterialExpression;
using namespace ProjectMaterialExpressionUtils;
E* Mask(UMaterial* M,E* A,int32 Bits)
{
    auto* N=Node<UMaterialExpressionComponentMask>(M);
    N->R=(Bits&1)!=0; N->G=(Bits&2)!=0; N->B=(Bits&4)!=0; N->A=false; N->Input.Connect(0,A); return N;
}
E* Unit(UMaterial* M,E* A) {auto* N=Node<UMaterialExpressionNormalize>(M); N->VectorInput.Connect(0,A); return N;}
E* Sample(UMaterial* M,UTexture* T,const TCHAR* Name,E* UV,E* Bias)
{
    auto* N=Node<UMaterialExpressionTextureSampleParameter2D>(M);
    N->Texture=T; N->ParameterName=Name; N->SamplerType=SAMPLERTYPE_LinearColor;
    N->MipValueMode=TMVM_MipBias; N->Coordinates.Connect(0,UV); N->MipValue.Connect(0,Bias); return N;
}
E* Sum(UMaterial* M,const TArray<E*>& Terms)
{return Binary<UMaterialExpressionAdd>(M,Binary<UMaterialExpressionAdd>(M,Terms[0],Terms[1]),Terms[2]);}
E* Direction(UMaterial* M,E* Geo,int32 Axis)
{
    auto* S=Node<UMaterialExpressionSign>(M); S->Input.Connect(0,Mask(M,Geo,1<<Axis));
    auto* Ones=Node<UMaterialExpressionConstant2Vector>(M); Ones->R=Ones->G=1;
    if (Axis==0) return Binary<UMaterialExpressionAppendVector>(M,S,Ones);
    if (Axis==2) return Binary<UMaterialExpressionAppendVector>(M,Ones,S);
    return Binary<UMaterialExpressionAppendVector>(M,Constant(M,1),Binary<UMaterialExpressionAppendVector>(M,S,Constant(M,1)));
}
bool Param(const E* N,const TCHAR* Name)
{const auto* P=Cast<UMaterialExpressionScalarParameter>(N); return P && P->ParameterName==Name;}
bool Value(const E* N,float V)
{const auto* C=Cast<UMaterialExpressionConstant>(N); return C && FMath::IsNearlyEqual(C->R,V,1.e-6f);}
bool Channel(const E* N,const E* Source,int32 Bits)
{
    const auto* C=Cast<UMaterialExpressionComponentMask>(N);
    return C && Source && C->Input.Expression==Source && C->Input.OutputIndex==0 &&
        C->R==((Bits&1)!=0) && C->G==((Bits&2)!=0) && C->B==((Bits&4)!=0) && !C->A;
}
bool Terms(const E* N,TArray<const UMaterialExpressionMultiply*>& Out)
{
    const auto* A=Cast<UMaterialExpressionAdd>(N);
    const auto* B=A ? Cast<UMaterialExpressionAdd>(A->A.Expression) : nullptr;
    if (!B) return false;
    for (const E* V : {B->A.Expression,B->B.Expression,A->B.Expression})
    {const auto* T=Cast<UMaterialExpressionMultiply>(V); if (!T) return false; Out.Add(T);}
    return true;
}
const E* Weights(const E* N,int32 Axis)
{
    const auto* C=Cast<UMaterialExpressionComponentMask>(N);
    const auto* D=C ? Cast<UMaterialExpressionDivide>(C->Input.Expression) : nullptr;
    const auto* Abs=D ? Cast<UMaterialExpressionAbs>(D->A.Expression) : nullptr;
    const auto* Dot=D ? Cast<UMaterialExpressionDotProduct>(D->B.Expression) : nullptr;
    const auto* Ones=Dot ? Cast<UMaterialExpressionConstant3Vector>(Dot->B.Expression) : nullptr;
    return D && Channel(C,D,1<<Axis) && Abs && Abs->Input.Expression &&
        Abs->Input.Expression->IsA<UMaterialExpressionVertexNormalWS>() && Dot &&
        Dot->A.Expression==Abs && Ones && Ones->Constant.Equals(FLinearColor(1,1,1),1.e-6f) ? Abs->Input.Expression : nullptr;
}
bool Metric(const UMaterialExpressionTextureSampleParameter2D* S,UTexture* T,const TCHAR* Name,const TCHAR* Scale,int32 Axis)
{
    const auto* UV=S ? Cast<UMaterialExpressionComponentMask>(S->Coordinates.Expression) : nullptr;
    const auto* D=UV ? Cast<UMaterialExpressionDivide>(UV->Input.Expression) : nullptr;
    const auto* Units=D ? Cast<UMaterialExpressionMultiply>(D->B.Expression) : nullptr;
    const int32 Bits=Axis==0 ? 6 : Axis==1 ? 5 : 3;
    return S && S->Texture==T && S->ParameterName==Name && S->SamplerType==SAMPLERTYPE_LinearColor &&
        S->MipValueMode==TMVM_MipBias && Param(S->MipValue.Expression,TEXT("TextureMipBias")) &&
        D && Channel(UV,D,Bits) && D->A.Expression && D->A.Expression->IsA<UMaterialExpressionWorldPosition>() &&
        Units && Param(Units->A.Expression,Scale) && Value(Units->B.Expression,100);
}
bool Sign(const E* N,const E* Geo,int32 Axis)
{const auto* S=Cast<UMaterialExpressionSign>(N); return S && Channel(S->Input.Expression,Geo,1<<Axis);}
bool DirectionRoute(const E* N,const E* Geo,int32 Axis)
{
    const auto* A=Cast<UMaterialExpressionAppendVector>(N);
    if (!A) return false;
    if (Axis==1)
    {
        const auto* B=Cast<UMaterialExpressionAppendVector>(A->B.Expression);
        return Value(A->A.Expression,1) && B && Sign(B->A.Expression,Geo,Axis) && Value(B->B.Expression,1);
    }
    const auto* Ones=Cast<UMaterialExpressionConstant2Vector>(Axis==0 ? A->B.Expression : A->A.Expression);
    return Ones && Ones->R==1 && Ones->G==1 && Sign(Axis==0 ? A->A.Expression : A->B.Expression,Geo,Axis);
}
const E* Unswizzle(const E* N,int32 Axis)
{
    if (Axis==2) return N;
    const auto* A=Cast<UMaterialExpressionAppendVector>(N);
    const auto* First=A ? Cast<UMaterialExpressionComponentMask>(A->A.Expression) : nullptr;
    const E* Source=First ? First->Input.Expression : nullptr;
    if (Axis==0) return Channel(First,Source,4) && Channel(A->B.Expression,Source,3) ? Source : nullptr;
    const auto* B=A ? Cast<UMaterialExpressionAppendVector>(A->B.Expression) : nullptr;
    return Channel(First,Source,1) && B && Channel(B->A.Expression,Source,4) && Channel(B->B.Expression,Source,2) ? Source : nullptr;
}
}

ProjectMaterialAggregateProjection::FOutputs ProjectMaterialAggregateProjection::Build(
    UMaterial* M,const FProjectMaterialSurfaceRecipe& R,UTexture* SignalsTarget,UTexture* NormalTarget)
{
    E* Geo=Node<UMaterialExpressionVertexNormalWS>(M);
    auto* Abs=Node<UMaterialExpressionAbs>(M); Abs->Input.Connect(0,Geo);
    auto* Ones=Node<UMaterialExpressionConstant3Vector>(M); Ones->Constant=FLinearColor(1,1,1);
    E* W=Binary<UMaterialExpressionDivide>(M,Abs,Binary<UMaterialExpressionDotProduct>(M,Abs,Ones));
    E* Pos=Node<UMaterialExpressionWorldPosition>(M);
    E* UV=Binary<UMaterialExpressionDivide>(M,Pos,Binary<UMaterialExpressionMultiply>(M,Scalar(M,R,TEXT("TileMeters")),Constant(M,100)));
    E* MacroUV=Binary<UMaterialExpressionDivide>(M,Pos,Binary<UMaterialExpressionMultiply>(M,Scalar(M,R,TEXT("MacroMeters")),Constant(M,100)));
    E* Bias=Scalar(M,R,TEXT("TextureMipBias"));
    E* Strength=Scalar(M,R,TEXT("ReliefStrength"));
    TArray<E*> Signals,Normals,Macros;
    for (int32 I=0; I<3; ++I)
    {
        const int32 Bits=I==0 ? 6 : I==1 ? 5 : 3;
        E* Weight=Mask(M,W,1<<I); E* Coords=Mask(M,UV,Bits);
        Signals.Add(Binary<UMaterialExpressionMultiply>(M,Sample(M,SignalsTarget,TEXT("SurfaceSignals"),Coords,Bias),Weight));
        Macros.Add(Binary<UMaterialExpressionMultiply>(M,Sample(M,SignalsTarget,TEXT("SurfaceSignals"),Mask(M,MacroUV,Bits),Bias),Weight));
        E* Encoded=Sample(M,NormalTarget,TEXT("SurfaceNormal"),Coords,Bias);
        E* Decoded=Binary<UMaterialExpressionSubtract>(M,Binary<UMaterialExpressionMultiply>(M,Encoded,Constant(M,2)),Constant(M,1));
        E* Detail=Unit(M,Binary<UMaterialExpressionAppendVector>(M,Binary<UMaterialExpressionMultiply>(M,Mask(M,Decoded,3),Strength),Mask(M,Decoded,4)));
        if (I==0) Detail=Binary<UMaterialExpressionAppendVector>(M,Mask(M,Detail,4),Mask(M,Detail,3));
        else if (I==1) Detail=Binary<UMaterialExpressionAppendVector>(M,Mask(M,Detail,1),Binary<UMaterialExpressionAppendVector>(M,Mask(M,Detail,4),Mask(M,Detail,2)));
        // Encoded derivatives retain their signs; only the outward axis reverses.
        E* Oriented=Binary<UMaterialExpressionMultiply>(M,Detail,Direction(M,Geo,I));
        Normals.Add(Binary<UMaterialExpressionMultiply>(M,Oriented,Weight));
    }
    return {Sum(M,Signals),Unit(M,Sum(M,Normals)),Sum(M,Macros)};
}

bool ProjectMaterialAggregateProjection::Signals(const E* N,UTexture* T,const TCHAR* Scale)
{
    TArray<const UMaterialExpressionMultiply*> Parts;
    if (!Terms(N,Parts)) return false;
    const E* Geo=nullptr;
    for (int32 I=0; I<3; ++I)
    {
        const E* Current=Weights(Parts[I]->B.Expression,I);
        if (!Current || (Geo && Current!=Geo) || !Metric(Cast<UMaterialExpressionTextureSampleParameter2D>(Parts[I]->A.Expression),T,TEXT("SurfaceSignals"),Scale,I)) return false;
        Geo=Current;
    }
    return true;
}

bool ProjectMaterialAggregateProjection::Normal(const E* N,UTexture* T)
{
    const auto* UnitNormal=Cast<UMaterialExpressionNormalize>(N);
    TArray<const UMaterialExpressionMultiply*> Parts;
    if (!UnitNormal || !Terms(UnitNormal->VectorInput.Expression,Parts)) return false;
    const E* Geo=nullptr;
    for (int32 I=0; I<3; ++I)
    {
        const E* Current=Weights(Parts[I]->B.Expression,I);
        const auto* Oriented=Cast<UMaterialExpressionMultiply>(Parts[I]->A.Expression);
        if (!Current || (Geo && Current!=Geo) || !Oriented || !DirectionRoute(Oriented->B.Expression,Current,I)) return false;
        Geo=Current;
        const auto* U=Cast<UMaterialExpressionNormalize>(Unswizzle(Oriented->A.Expression,I));
        const auto* XY=U ? Cast<UMaterialExpressionAppendVector>(U->VectorInput.Expression) : nullptr;
        const auto* Strength=XY ? Cast<UMaterialExpressionMultiply>(XY->A.Expression) : nullptr;
        const auto* Z=XY ? Cast<UMaterialExpressionComponentMask>(XY->B.Expression) : nullptr;
        const auto* Decode=Z ? Cast<UMaterialExpressionSubtract>(Z->Input.Expression) : nullptr;
        const auto* Twice=Decode ? Cast<UMaterialExpressionMultiply>(Decode->A.Expression) : nullptr;
        if (!XY || !Strength || !Param(Strength->B.Expression,TEXT("ReliefStrength")) || !Decode || !Twice ||
            !Channel(Strength->A.Expression,Decode,3) || !Channel(Z,Decode,4) || !Value(Decode->B.Expression,1) || !Value(Twice->B.Expression,2) ||
            !Metric(Cast<UMaterialExpressionTextureSampleParameter2D>(Twice->A.Expression),T,TEXT("SurfaceNormal"),TEXT("TileMeters"),I)) return false;
    }
    return true;
}
