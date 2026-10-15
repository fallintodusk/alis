// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialMetricHeightSurface.h"
#include "ProjectMaterialExpressionUtils.h"
#include "ProjectMaterialPatternBindings.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialTerrainSemantics.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace ProjectMaterialMetricHeightPrivate
{
using E=UMaterialExpression;
using namespace ProjectMaterialExpressionUtils;
E* Channel(UMaterial* M,E* A,int32 I)
{
    auto* N=Node<UMaterialExpressionComponentMask>(M);
    N->R=I==0; N->G=I==1; N->B=I==2; N->A=false; N->Input.Connect(0,A); return N;
}
E* Pair(UMaterial* M,E* A,E* B)
{return Binary<UMaterialExpressionAppendVector>(M,A,B);}
E* Add(UMaterial* M,E* A,E* B) {return Binary<UMaterialExpressionAdd>(M,A,B);}
E* Sub(UMaterial* M,E* A,E* B) {return Binary<UMaterialExpressionSubtract>(M,A,B);}
E* Mul(UMaterial* M,E* A,E* B) {return Binary<UMaterialExpressionMultiply>(M,A,B);}
E* Div(UMaterial* M,E* A,E* B) {return Binary<UMaterialExpressionDivide>(M,A,B);}
E* Lerp(UMaterial* M,E* A,E* B,E* T)
{
    auto* N=Node<UMaterialExpressionLinearInterpolate>(M);
    N->A.Connect(0,A); N->B.Connect(0,B); N->Alpha.Connect(0,T); return N;
}
E* Offset(UMaterial* M,float X,float Y)
{auto* N=Node<UMaterialExpressionConstant2Vector>(M); N->R=X; N->G=Y; return N;}
E* Clamp(UMaterial* M,E* A,float Min,float Max)
{auto* N=Node<UMaterialExpressionClamp>(M); N->Input.Connect(0,A); N->MinDefault=Min; N->MaxDefault=Max; return N;}
E* Unit(UMaterial* M,E* A)
{auto* N=Node<UMaterialExpressionNormalize>(M); N->VectorInput.Connect(0,A); return N;}
E* Sample(UMaterial* M,const ProjectMaterialPatternBindings::FBinding& B,E* UV)
{
    auto* N=Node<UMaterialExpressionTextureSampleParameter2D>(M);
    N->ParameterName=B.Parameter; N->Texture=B.Target; N->SamplerType=SAMPLERTYPE_LinearColor;
    N->Coordinates.Connect(0,UV); return N;
}
E* Slopes(UMaterial* M,E* Encoded,E* Scale)
{
    E* D=Sub(M,Mul(M,Encoded,Constant(M,2)),Constant(M,1));
    E* XY=Pair(M,Channel(M,D,0),Channel(M,D,1));
    E* Z=Binary<UMaterialExpressionMax>(M,Channel(M,D,2),Constant(M,.001f));
    return Div(M,XY,Mul(M,Z,Scale));
}
bool Graph(UMaterial* M,const FProjectMaterialSurfaceRecipe& R,const FProjectMaterialSurfaceDependency& P,FString& Error,
    const TMap<FName,int32>& Channels)
{
    TArray<ProjectMaterialPatternBindings::FBinding> B;
    if (!ProjectMaterialPatternBindings::ValidateSource(R,P,Error) || !ProjectMaterialPatternBindings::Resolve(P,B,Error) || B.Num()!=3) return false;
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);
    E* XY=WorldXY(M);
    E* UV=Div(M,XY,Mul(M,Constant(M,100),Scalar(M,R,TEXT("PatternScaleMeters"))));
    E* Ratio=Scalar(M,R,TEXT("SecondaryScaleRatio"));
    E* UV2=Add(M,Div(M,UV,Ratio),Offset(M,.317f,.619f));
    E* MacroUV=Add(M,Div(M,XY,Mul(M,Constant(M,100),Scalar(M,R,TEXT("MacroScaleMeters")))),Offset(M,.273f,.619f));
    E* Macro=Channel(M,Sample(M,B[2],MacroUV),2);
    TArray<E*> A,C;
    for (const auto& Binding : B) {A.Add(Sample(M,Binding,UV)); C.Add(Sample(M,Binding,UV2));}
    E* Height=Add(M,Mul(M,Sub(M,Channel(M,C[0],0),Channel(M,A[0],0)),Constant(M,1.8f)),Constant(M,.5f));
    E* Phase=Add(M,Mul(M,Macro,Constant(M,.5f)),Constant(M,.25f));
    auto* Blend=Node<UMaterialExpressionMaterialFunctionCall>(M);
    auto* Native=LoadObject<UMaterialFunction>(nullptr,TEXT("/Engine/Functions/Engine_MaterialFunctions02/Texturing/HeightLerp.HeightLerp"));
    if (!Native || !Blend->SetMaterialFunction(Native)) {Error=TEXT("Native HeightLerp is unavailable."); return false;}
    const TMap<FName,E*> Pins={{TEXT("A"),Constant(M,0)},{TEXT("B"),Constant(M,1)},
        {TEXT("Height Texture"),Height},{TEXT("Transition Phase"),Phase},{TEXT("Contrast"),Scalar(M,R,TEXT("CompositionContrast"))}};
    int32 Bound=0;
    for (auto& Input : Blend->FunctionInputs)
        if (E* const* Value=Pins.Find(Input.Input.InputName)) {Input.Input.Connect(0,*Value); ++Bound;}
    if (Bound!=Pins.Num()) {Error=TEXT("Native HeightLerp signature differs."); return false;}
    E* Alpha=Mul(M,Channel(M,Blend,0),Scalar(M,R,TEXT("CompositionStrength")));
    E* Structure=Lerp(M,A[0],C[0],Alpha);
    E* Masks=Lerp(M,A[2],C[2],Alpha);
    E* NormalEncoded=Add(M,Mul(M,Unit(M,Pair(M,Lerp(M,Slopes(M,A[1],Constant(M,1)),Slopes(M,C[1],Ratio),Alpha),Constant(M,1))),Constant(M,.5f)),Constant(M,.5f));
    auto* Distance=Node<UMaterialExpressionDistance>(M);
    Distance->A.Connect(0,Node<UMaterialExpressionWorldPosition>(M));
    Distance->B.Connect(0,Node<UMaterialExpressionCameraPositionWS>(M));
    E* Start=Scalar(M,R,TEXT("MacroBlendStartMeters")); E* End=Scalar(M,R,TEXT("MacroBlendEndMeters"));
    E* Transition=Div(M,Sub(M,Div(M,Distance,Constant(M,100)),Start),Sub(M,End,Start));
    auto* Fade=Node<UMaterialExpressionSmoothStep>(M);
    Fade->Min.Connect(0,Constant(M,0)); Fade->Max.Connect(0,Constant(M,1)); Fade->Value.Connect(0,Transition);
    E* Weight=Lerp(M,Scalar(M,R,TEXT("NearMacroStrength")),Scalar(M,R,TEXT("FarMacroStrength")),Fade);
    E* Pigment=Lerp(M,Lerp(M,Channel(M,Masks,2),Macro,Weight),Channel(M,Structure,1),Scalar(M,R,TEXT("AggregatePigmentStrength")));
    E* Color=Lerp(M,Vector(M,R,TEXT("SoilDarkColor")),Vector(M,R,TEXT("SoilLightColor")),Pigment);
    Color=Lerp(M,Color,Vector(M,R,TEXT("GrainColor")),Mul(M,Channel(M,Structure,2),Scalar(M,R,TEXT("GrainColorStrength"))));
    if (FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(R.Archetype))
    {
        Color=ProjectMaterialTerrainSemantics::ComposeColor(M,R,Color,Channels,Error);
        if (!Color) return false;
    }
    E* Rough=Clamp(M,Add(M,Scalar(M,R,TEXT("BaseRoughness")),Mul(M,Sub(M,Channel(M,Masks,1),Constant(M,.5f)),Scalar(M,R,TEXT("RoughnessStrength")))),.05f,1);
    Rough=Clamp(M,Add(M,Rough,Mul(M,Mul(M,Sub(M,Macro,Constant(M,.5f)),Scalar(M,R,TEXT("MacroRoughnessStrength"))),Fade)),.05f,1);
    E* Decode=Sub(M,Mul(M,NormalEncoded,Constant(M,2)),Constant(M,1));
    E* Slope=Div(M,Pair(M,Channel(M,Decode,0),Channel(M,Decode,1)),Clamp(M,Channel(M,Decode,2),.02f,1));
    E* Strength=Div(M,Scalar(M,R,TEXT("BumpHeightCentimeters")),Mul(M,Scalar(M,R,TEXT("PatternScaleMeters")),Constant(M,2.5f)));
    E* Detail=Unit(M,Pair(M,Mul(M,Slope,Strength),Constant(M,1)));
    E* Oriented=WorldOrientedNormal(M,Detail,Error); if (!Oriented) return false;
    auto* Slab=Node<UMaterialExpressionSubstrateSlabBSDF>(M);
    auto* F0=Node<UMaterialExpressionConstant3Vector>(M); F0->Constant=FLinearColor(.04f,.04f,.04f);
    Slab->DiffuseAlbedo.Connect(0,Color); Slab->Roughness.Connect(0,Rough); Slab->F0.Connect(0,F0); Slab->Normal.Connect(0,Oriented);
    M->GetEditorOnlyData()->FrontMaterial.Connect(0,Slab);
    M->GetEditorOnlyData()->AmbientOcclusion.Connect(0,Lerp(M,Constant(M,1),Channel(M,Masks,0),Scalar(M,R,TEXT("AOInfluence"))));
    M->bTangentSpaceNormal=false; M->MaterialDomain=MD_Surface; M->BlendMode=BLEND_Opaque; M->SetShadingModel(MSM_DefaultLit);
    return true;
}

bool SameGraph(const E* A,const E* B,TMap<const E*,const E*>& Seen,int32 Depth=0)
{
    if (!A || !B) return A==B;
    if (Depth>512 || A->GetClass()!=B->GetClass()) return false;
    if (const E* const* Prior=Seen.Find(A)) return *Prior==B;
    Seen.Add(A,B);
    // Native editable values define the closed shader. Object names, graph positions and GUIDs do not.
    for (TFieldIterator<FProperty> I(A->GetClass()); I; ++I)
    {
        const auto* P=*I;
        if (!P->HasAnyPropertyFlags(CPF_Edit) || P->GetOwnerClass()==UMaterialExpression::StaticClass()) continue;
        if (const auto* S=CastField<FStructProperty>(P))
            if (S->Struct->GetFName()==TEXT("Guid") || S->Struct->GetFName()==TEXT("ExpressionInput") || S->Struct->GetName().EndsWith(TEXT("MaterialInput"))) continue;
        if (P->GetFName()==TEXT("FunctionInputs") || P->GetFName()==TEXT("FunctionOutputs")) continue;
        if (!P->Identical_InContainer(A,B)) return false;
    }
    for (int32 I=0;; ++I)
    {
        const FExpressionInput* X=A->GetInput(I); const FExpressionInput* Y=B->GetInput(I);
        if (!X || !Y) return X==Y;
        if (A->GetInputName(I)!=B->GetInputName(I) || X->OutputIndex!=Y->OutputIndex ||
            X->Mask!=Y->Mask || X->MaskR!=Y->MaskR || X->MaskG!=Y->MaskG || X->MaskB!=Y->MaskB || X->MaskA!=Y->MaskA ||
            !SameGraph(X->Expression,Y->Expression,Seen,Depth+1)) return false;
    }
}
}

bool ProjectMaterialMetricHeightSurface::Build(UMaterial* M,const FProjectMaterialSurfaceRecipe& R,
    const FProjectMaterialSurfaceDependency& P,TArray<FString>& Errors,FString& Error,const TMap<FName,int32>& Channels)
{
    if (!ProjectMaterialMetricHeightPrivate::Graph(M,R,P,Error,Channels)) return false;
    M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ProjectGenerationOwner"),TEXT("ProjectMaterial"));
    M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ProjectSurfaceArchetype"),*R.Archetype);
    M->PostEditChange(); Errors=UMaterialEditingLibrary::RecompileMaterial(M);
    if (!Errors.IsEmpty()) {Error=FString::Join(Errors,TEXT(" | ")); return false;}
    M->MarkPackageDirty(); return true;
}

bool ProjectMaterialMetricHeightSurface::Verify(const FProjectMaterialSurfaceRecipe& R,
    const FProjectMaterialSurfaceDependency& P,const UObject* Asset,FString& Error,const TMap<FName,int32>& Channels)
{
    using namespace ProjectMaterialMetricHeightPrivate;
    const auto* M=Cast<UMaterial>(Asset);
    if (!M || M->bTangentSpaceNormal || M->MaterialDomain!=MD_Surface || M->BlendMode!=BLEND_Opaque ||
        M->GetShadingModels()!=FMaterialShadingModelField(MSM_DefaultLit))
    {Error=TEXT("Metric height material uses an incompatible surface or normal frame."); return false;}
    // Construct only expressions for comparison; verification does not compile or save shaders.
    TStrongObjectPtr<UMaterial> Expected(NewObject<UMaterial>(GetTransientPackage(),NAME_None,RF_Transient));
    if (!Graph(Expected.Get(),R,P,Error,Channels)) return false;
    TMap<const E*,const E*> Seen;
    if (UMaterialEditingLibrary::GetMaterialExpressions(M).Num()!=UMaterialEditingLibrary::GetMaterialExpressions(Expected.Get()).Num() ||
        !SameGraph(M->GetEditorOnlyData()->FrontMaterial.Expression,Expected->GetEditorOnlyData()->FrontMaterial.Expression,Seen) ||
        !SameGraph(M->GetEditorOnlyData()->AmbientOcclusion.Expression,Expected->GetEditorOnlyData()->AmbientOcclusion.Expression,Seen))
    {Error=TEXT("Metric height shader differs from its connected native expression contract."); return false;}
    return true;
}
