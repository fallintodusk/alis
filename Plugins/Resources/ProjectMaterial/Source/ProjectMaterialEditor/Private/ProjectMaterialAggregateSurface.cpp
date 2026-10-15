// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialAggregateSurface.h"
#include "ProjectMaterialAggregateProjection.h"
#include "ProjectMaterialExpressionUtils.h"
#include "ProjectMaterialPatternBindings.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialSurfaceDependency.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace ProjectMaterialAggregateSurfacePrivate
{
using E=UMaterialExpression;
using namespace ProjectMaterialExpressionUtils;
constexpr TCHAR OrientPath[]=TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals.BlendAngleCorrectedNormals");

E* Mask(UMaterial* M,E* A,bool R,bool G,bool B)
{ auto* N=Node<UMaterialExpressionComponentMask>(M); N->R=R; N->G=G; N->B=B; N->A=false; N->Input.Connect(0,A); return N; }
E* Unit(UMaterial* M,E* A) { auto* N=Node<UMaterialExpressionNormalize>(M); N->VectorInput.Connect(0,A); return N; }
E* Sat(UMaterial* M,E* A) { auto* N=Node<UMaterialExpressionSaturate>(M); N->Input.Connect(0,A); return N; }
E* Sample(UMaterial* M,const TCHAR* Name,UTexture* Texture,E* UV,E* Bias)
{
	auto* N=Node<UMaterialExpressionTextureSampleParameter2D>(M); N->ParameterName=Name; N->Texture=Texture;
	N->SamplerType=SAMPLERTYPE_LinearColor; N->MipValueMode=TMVM_MipBias;
	N->Coordinates.Connect(0,UV); N->MipValue.Connect(0,Bias); return N;
}
bool Targets(const FProjectMaterialSurfaceDependency& P,TArray<UTextureRenderTarget2D*>& Out,FString& Error)
{
    TArray<ProjectMaterialPatternBindings::FBinding> Bindings;
    if (P.OutputContract!=TEXT("aggregate_signals_normal_v1") || !ProjectMaterialPatternBindings::Resolve(P,Bindings,Error)) return false;
    for (const auto& B : Bindings) Out.Add(B.Target);
    return true;
}
bool Parameter(const E* N,const TCHAR* Name)
{
	if (const auto* S=Cast<UMaterialExpressionScalarParameter>(N)) return S->ParameterName==Name;
	if (const auto* V=Cast<UMaterialExpressionVectorParameter>(N)) return V->ParameterName==Name;
	return false;
}
bool Value(const E* N,float V) { const auto* C=Cast<UMaterialExpressionConstant>(N); return C && FMath::IsNearlyEqual(C->R,V,1.e-6f); }
bool Channel(const E* N,const E* Source,bool R,bool G,bool B)
{
	const auto* C=Cast<UMaterialExpressionComponentMask>(N);
	return C && Source && C->Input.Expression==Source && C->Input.OutputIndex==0 && C->R==R && C->G==G && C->B==B && !C->A;
}
const E* TextureChannel(const E* N,UTexture* T,bool G,bool B)
{
	const auto* C=Cast<UMaterialExpressionComponentMask>(N);
	const auto* S=C ? Cast<UMaterialExpressionTextureSampleParameter2D>(C->Input.Expression) : nullptr;
	if (S && S->Texture==T && Channel(C,S,false,G,B)) return S;
	const E* Source=C ? C->Input.Expression : nullptr;
	return Channel(C,Source,false,G,B) &&
		(ProjectMaterialAggregateProjection::Signals(Source,T,TEXT("TileMeters")) ||
		 ProjectMaterialAggregateProjection::Signals(Source,T,TEXT("MacroMeters"))) ? Source : nullptr;
}
bool Metric(const E* N,const TCHAR* Scale,UTexture* Target)
{
	if (ProjectMaterialAggregateProjection::Signals(N,Target,Scale)) return true;
	const auto* S=Cast<UMaterialExpressionTextureSampleParameter2D>(N);
	const auto* D=S ? Cast<UMaterialExpressionDivide>(S->Coordinates.Expression) : nullptr;
	const auto* XY=D ? Cast<UMaterialExpressionComponentMask>(D->A.Expression) : nullptr;
	const auto* Units=D ? Cast<UMaterialExpressionMultiply>(D->B.Expression) : nullptr;
	return XY && XY->Input.Expression && XY->Input.Expression->IsA<UMaterialExpressionWorldPosition>() && Channel(XY,XY->Input.Expression,true,true,false) &&
		Units && Parameter(Units->A.Expression,Scale) && Value(Units->B.Expression,100) && Parameter(S->MipValue.Expression,TEXT("TextureMipBias"));
}
bool Routes(const UMaterialExpressionSubstrateSlabBSDF* S,const TArray<UTextureRenderTarget2D*>& T,bool bTriplanar)
{
	const auto* Diffuse=S ? Cast<UMaterialExpressionMultiply>(S->DiffuseAlbedo.Expression) : nullptr;
	const auto* Palette=Diffuse ? Cast<UMaterialExpressionLinearInterpolate>(Diffuse->A.Expression) : nullptr;
	const auto* Pigment=Palette ? TextureChannel(Palette->Alpha.Expression,T[0],true,false) : nullptr;
	const auto* Factor=Diffuse ? Cast<UMaterialExpressionAdd>(Diffuse->B.Expression) : nullptr;
	const auto* Contrast=Factor ? Cast<UMaterialExpressionMultiply>(Factor->B.Expression) : nullptr;
	const auto* Macro=Contrast ? Cast<UMaterialExpressionSubtract>(Contrast->A.Expression) : nullptr;
	const auto* MacroSample=Macro ? TextureChannel(Macro->A.Expression,T[0],false,true) : nullptr;
	const auto* Rough=S ? Cast<UMaterialExpressionSaturate>(S->Roughness.Expression) : nullptr;
	const auto* RoughAdd=Rough ? Cast<UMaterialExpressionAdd>(Rough->Input.Expression) : nullptr;
	const auto* Local=RoughAdd ? Cast<UMaterialExpressionSaturate>(RoughAdd->A.Expression) : nullptr;
	const auto* LocalAdd=Local ? Cast<UMaterialExpressionAdd>(Local->Input.Expression) : nullptr;
	const auto* Variation=LocalAdd ? Cast<UMaterialExpressionMultiply>(LocalAdd->B.Expression) : nullptr;
	const auto* Center=Variation ? Cast<UMaterialExpressionSubtract>(Variation->A.Expression) : nullptr;
	const auto* RoughMacro=RoughAdd ? Cast<UMaterialExpressionMultiply>(RoughAdd->B.Expression) : nullptr;
	if (!Palette || !Parameter(Palette->A.Expression,TEXT("BinderColor")) || !Parameter(Palette->B.Expression,TEXT("AggregateColor")) || !Metric(Pigment,TEXT("TileMeters"),T[0]) ||
		!Factor || !Value(Factor->A.Expression,1) || !Contrast || !Parameter(Contrast->B.Expression,TEXT("MacroContrast")) || !Macro || !Value(Macro->B.Expression,.5f) || !Metric(MacroSample,TEXT("MacroMeters"),T[0]) ||
		!LocalAdd || !Parameter(LocalAdd->A.Expression,TEXT("SurfaceRoughness")) || !Variation || !Parameter(Variation->B.Expression,TEXT("RoughnessVariation")) ||
		!Center || !Value(Center->B.Expression,.5f) || TextureChannel(Center->A.Expression,T[0],false,true)!=Pigment ||
		!RoughMacro || RoughMacro->A.Expression!=Macro || !Parameter(RoughMacro->B.Expression,TEXT("MacroRoughness"))) return false;
	if (bTriplanar)
	{
		const auto* F0=Cast<UMaterialExpressionConstant3Vector>(S->F0.Expression);
		return ProjectMaterialAggregateProjection::Signals(Pigment,T[0],TEXT("TileMeters")) &&
			ProjectMaterialAggregateProjection::Signals(MacroSample,T[0],TEXT("MacroMeters")) &&
			ProjectMaterialAggregateProjection::Normal(S->Normal.Expression,T[1]) &&
			F0 && F0->Constant.Equals(FLinearColor(.04f,.04f,.04f),1.e-6f);
	}
	const auto* UnitNormal=Cast<UMaterialExpressionNormalize>(S->Normal.Expression);
	const auto* Orient=UnitNormal ? Cast<UMaterialExpressionMaterialFunctionCall>(UnitNormal->VectorInput.Expression) : nullptr;
	const E* Base=nullptr; const E* Detail=nullptr;
	if (!Orient || !Orient->MaterialFunction || Orient->MaterialFunction->GetPathName()!=OrientPath) return false;
	for (const auto& I : Orient->FunctionInputs)
	{
		if (I.Input.InputName==TEXT("BaseNormal")) Base=I.Input.Expression;
		if (I.Input.InputName==TEXT("AdditionalNormal")) Detail=I.Input.Expression;
	}
	const auto* UnitDetail=Cast<UMaterialExpressionNormalize>(Detail);
	const auto* XYZ=UnitDetail ? Cast<UMaterialExpressionAppendVector>(UnitDetail->VectorInput.Expression) : nullptr;
	const auto* Strength=XYZ ? Cast<UMaterialExpressionMultiply>(XYZ->A.Expression) : nullptr;
	const auto* Z=XYZ ? Cast<UMaterialExpressionComponentMask>(XYZ->B.Expression) : nullptr;
	const auto* Decode=Z ? Cast<UMaterialExpressionSubtract>(Z->Input.Expression) : nullptr;
	const auto* Twice=Decode ? Cast<UMaterialExpressionMultiply>(Decode->A.Expression) : nullptr;
	const auto* Normal=Twice ? Cast<UMaterialExpressionTextureSampleParameter2D>(Twice->A.Expression) : nullptr;
	const auto* F0=Cast<UMaterialExpressionConstant3Vector>(S->F0.Expression);
	return Base && Base->IsA<UMaterialExpressionVertexNormalWS>() && Strength && Parameter(Strength->B.Expression,TEXT("ReliefStrength")) &&
		Decode && Value(Decode->B.Expression,1) && Twice && Value(Twice->B.Expression,2) && Channel(Strength->A.Expression,Decode,true,true,false) && Channel(Z,Decode,false,false,true) &&
		Normal && Normal->Texture==T[1] && Metric(Normal,TEXT("TileMeters"),T[1]) &&
		Cast<UMaterialExpressionTextureSampleParameter2D>(Pigment) && Normal->Coordinates.Expression==Cast<UMaterialExpressionTextureSampleParameter2D>(Pigment)->Coordinates.Expression && F0 && F0->Constant.Equals(FLinearColor(.04f,.04f,.04f),1.e-6f);
}
}

bool ProjectMaterialAggregateSurface::Build(UMaterial* M,const FProjectMaterialSurfaceRecipe& R,
	const FProjectMaterialSurfaceDependency& P,TArray<FString>& Errors,FString& Error)
{
	using namespace ProjectMaterialAggregateSurfacePrivate;
	TArray<UTextureRenderTarget2D*> T; if (!Targets(P,T,Error)) return false;
	UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);
	E* Signals=nullptr; E* OrientedNormal=nullptr; E* MacroSignals=nullptr;
	if (R.ProjectionFrame==EProjectMaterialProjectionFrame::WorldMetricTriplanar2D)
	{
		const auto Projection=ProjectMaterialAggregateProjection::Build(M,R,T[0],T[1]);
		Signals=Projection.Signals; OrientedNormal=Projection.Normal; MacroSignals=Projection.Macro;
	}
	else
	{
		E* XY=WorldXY(M);
		E* Bias=Scalar(M,R,TEXT("TextureMipBias"));
		E* UV=Binary<UMaterialExpressionDivide>(M,XY,Binary<UMaterialExpressionMultiply>(M,Scalar(M,R,TEXT("TileMeters")),Constant(M,100)));
		Signals=Sample(M,TEXT("SurfaceSignals"),T[0],UV,Bias);
		E* Normal=Sample(M,TEXT("SurfaceNormal"),T[1],UV,Bias);
		E* Decoded=Binary<UMaterialExpressionSubtract>(M,Binary<UMaterialExpressionMultiply>(M,Normal,Constant(M,2)),Constant(M,1));
		E* Detail=ProjectMaterialAggregateSurfacePrivate::Unit(M,Binary<UMaterialExpressionAppendVector>(M,Binary<UMaterialExpressionMultiply>(M,Mask(M,Decoded,true,true,false),Scalar(M,R,TEXT("ReliefStrength"))),Mask(M,Decoded,false,false,true)));
		OrientedNormal=WorldOrientedNormal(M,Detail,Error);
		if (!OrientedNormal) return false;
		E* MacroUV=Binary<UMaterialExpressionDivide>(M,XY,Binary<UMaterialExpressionMultiply>(M,Scalar(M,R,TEXT("MacroMeters")),Constant(M,100)));
		MacroSignals=Sample(M,TEXT("SurfaceSignals"),T[0],MacroUV,Bias);
	}
	auto* Color=Node<UMaterialExpressionLinearInterpolate>(M);
	Color->A.Connect(0,Vector(M,R,TEXT("BinderColor"))); Color->B.Connect(0,Vector(M,R,TEXT("AggregateColor"))); Color->Alpha.Connect(0,Mask(M,Signals,false,true,false));
	E* Rough=Sat(M,Binary<UMaterialExpressionAdd>(M,Scalar(M,R,TEXT("SurfaceRoughness")),Binary<UMaterialExpressionMultiply>(M,Binary<UMaterialExpressionSubtract>(M,Mask(M,Signals,false,false,true),Constant(M,.5f)),Scalar(M,R,TEXT("RoughnessVariation")))));
	E* Macro=Binary<UMaterialExpressionSubtract>(M,Mask(M,MacroSignals,false,false,true),Constant(M,.5f));
	E* Diffuse=Binary<UMaterialExpressionMultiply>(M,Color,Binary<UMaterialExpressionAdd>(M,Constant(M,1),Binary<UMaterialExpressionMultiply>(M,Macro,Scalar(M,R,TEXT("MacroContrast")))));
	Rough=Sat(M,Binary<UMaterialExpressionAdd>(M,Rough,Binary<UMaterialExpressionMultiply>(M,Macro,Scalar(M,R,TEXT("MacroRoughness")))));
	auto* F0=Node<UMaterialExpressionConstant3Vector>(M); F0->Constant=FLinearColor(.04f,.04f,.04f);
	auto* Slab=Node<UMaterialExpressionSubstrateSlabBSDF>(M);
	Slab->DiffuseAlbedo.Connect(0,Diffuse); Slab->Roughness.Connect(0,Rough); Slab->F0.Connect(0,F0); Slab->Normal.Connect(0,OrientedNormal);
	M->GetEditorOnlyData()->FrontMaterial.Connect(0,Slab); M->bTangentSpaceNormal=false;
	M->MaterialDomain=MD_Surface; M->BlendMode=BLEND_Opaque; M->SetShadingModel(MSM_DefaultLit);
	M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ProjectGenerationOwner"),TEXT("ProjectMaterial"));
	M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ProjectSurfaceArchetype"),*R.Archetype);
	M->PostEditChange(); Errors=UMaterialEditingLibrary::RecompileMaterial(M);
	if (!Errors.IsEmpty()) { Error=FString::Join(Errors,TEXT(" | ")); return false; }
	M->MarkPackageDirty(); return true;
}

bool ProjectMaterialAggregateSurface::Verify(const FProjectMaterialSurfaceRecipe& R,const FProjectMaterialSurfaceDependency& P,const UObject* Asset,FString& Error)
{
	using namespace ProjectMaterialAggregateSurfacePrivate;
	const auto* M=Cast<UMaterial>(Asset); TArray<UTextureRenderTarget2D*> T;
	const bool bTriplanar=R.ProjectionFrame==EProjectMaterialProjectionFrame::WorldMetricTriplanar2D;
	if (!M || !Targets(P,T,Error) || M->bTangentSpaceNormal || M->MaterialDomain!=MD_Surface || M->BlendMode!=BLEND_Opaque)
	{ Error=TEXT("Asphalt material or source contract is invalid."); return false; }
	const auto* Slab=Cast<UMaterialExpressionSubstrateSlabBSDF>(M->GetEditorOnlyData()->FrontMaterial.Expression);
	const auto* Normal=Slab ? Cast<UMaterialExpressionNormalize>(Slab->Normal.Expression) : nullptr;
	const auto* Orient=Normal ? Cast<UMaterialExpressionMaterialFunctionCall>(Normal->VectorInput.Expression) : nullptr;
	if (!Slab || !Slab->DiffuseAlbedo.Expression || !Slab->Roughness.Expression ||
		(!bTriplanar && (!Orient || !Orient->MaterialFunction || Orient->MaterialFunction->GetPathName()!=OrientPath)) || !Routes(Slab,T,bTriplanar))
	{ Error=TEXT("Asphalt connected surface routes are invalid."); return false; }
	TSet<FName> Scalars,Vectors; int32 SignalSamples=0,NormalSamples=0,Slabs=0;
	for (const auto* N : UMaterialEditingLibrary::GetMaterialExpressions(const_cast<UMaterial*>(M)))
	{
		if (const auto* S=Cast<UMaterialExpressionScalarParameter>(N))
		{
			const double* Expected=R.Scalars.Find(S->ParameterName);
			if (!Expected || !FMath::IsNearlyEqual(S->DefaultValue,float(*Expected),1.e-6f)) { Error=TEXT("Asphalt scalar defaults differ from the recipe."); return false; }
			Scalars.Add(S->ParameterName);
		}
		if (const auto* V=Cast<UMaterialExpressionVectorParameter>(N))
		{
			const FLinearColor* Expected=R.Vectors.Find(V->ParameterName);
			if (!Expected || !V->DefaultValue.Equals(*Expected,1.e-6f)) { Error=TEXT("Asphalt palette defaults differ from the recipe."); return false; }
			Vectors.Add(V->ParameterName);
		}
		if (const auto* S=Cast<UMaterialExpressionTextureSampleParameter2D>(N))
		{
			if (S->SamplerType!=SAMPLERTYPE_LinearColor || S->MipValueMode!=TMVM_MipBias || !S->Coordinates.Expression || !S->MipValue.Expression)
			{ Error=TEXT("Asphalt sampling contract is invalid."); return false; }
			if (S->Texture==T[0] && S->ParameterName==TEXT("SurfaceSignals")) ++SignalSamples;
			else if (S->Texture==T[1] && S->ParameterName==TEXT("SurfaceNormal")) ++NormalSamples;
			else { Error=TEXT("Asphalt has a foreign source texture."); return false; }
		}
		Slabs+=N->IsA<UMaterialExpressionSubstrateSlabBSDF>();
	}
	if (Scalars.Num()!=R.Scalars.Num() || Vectors.Num()!=R.Vectors.Num() || Slabs!=1 || SignalSamples!=(bTriplanar ? 6 : 2) || NormalSamples!=(bTriplanar ? 3 : 1))
	{ Error=TEXT("Asphalt parameters or native samples are incomplete."); return false; }
	return true;
}
