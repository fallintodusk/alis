// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSoilBuilder.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialPatternAuthority.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace ProjectMaterialSoilBuilderPrivate
{
using E = UMaterialExpression;
template <class T> T* Node(UMaterial* M)
{
	return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M, T::StaticClass()));
}
E* Scalar(UMaterial* M, const FProjectMaterialSurfaceRecipe& R, const TCHAR* Name)
{
	auto* N = Node<UMaterialExpressionScalarParameter>(M);
	N->ParameterName = Name; N->DefaultValue = R.Scalars[FName(Name)]; return N;
}
E* Vector(UMaterial* M, const FProjectMaterialSurfaceRecipe& R, const TCHAR* Name)
{
	auto* N = Node<UMaterialExpressionVectorParameter>(M);
	N->ParameterName = Name; N->DefaultValue = R.Vectors[FName(Name)]; return N;
}
E* Constant(UMaterial* M, float V)
{
	auto* N = Node<UMaterialExpressionConstant>(M); N->R = V; return N;
}
E* Channel(UMaterial* M, E* A, int32 C)
{
	auto* N = Node<UMaterialExpressionComponentMask>(M);
	N->R = C == 0; N->G = C == 1; N->B = C == 2; N->A = false;
	N->Input.Connect(0, A); return N;
}
E* Mul(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionMultiply>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N;
}
E* Add(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionAdd>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N;
}
E* Sub(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionSubtract>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N;
}
E* Div(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionDivide>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N;
}
E* Lerp(UMaterial* M, E* A, E* B, E* Alpha)
{
	auto* N = Node<UMaterialExpressionLinearInterpolate>(M);
	N->A.Connect(0,A); N->B.Connect(0,B); N->Alpha.Connect(0,Alpha); return N;
}
E* Clamp(UMaterial* M, E* A, float Min, float Max)
{
	auto* N = Node<UMaterialExpressionClamp>(M); N->Input.Connect(0,A); N->MinDefault=Min; N->MaxDefault=Max; return N;
}
E* Append(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionAppendVector>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N;
}

bool IsChannel(const E* Expression, const E* Source, int32 Index)
{
	const auto* C=Cast<UMaterialExpressionComponentMask>(Expression);
	return C && Source && C->Input.Expression==Source && C->Input.OutputIndex==0 &&
		C->R==(Index==0) && C->G==(Index==1) && C->B==(Index==2) && !C->A;
}

bool IsTextureChannel(const E* Expression, const UTexture* Texture, int32 Index)
{
	const auto* C=Cast<UMaterialExpressionComponentMask>(Expression);
	const auto* Sample=C ? Cast<UMaterialExpressionTextureSample>(C->Input.Expression) : nullptr;
	return Sample && Sample->Texture==Texture && IsChannel(C,Sample,Index);
}

bool VerifyRoutes(const UMaterial* M, const UMaterialExpressionSubstrateSlabBSDF* Slab,
	const TArray<UTextureRenderTarget2D*>& Targets)
{
	// The closed graph's meaning is its connected routes, not expression membership.
	const auto* Color=Cast<UMaterialExpressionLinearInterpolate>(Slab->DiffuseAlbedo.Expression);
	const auto* Palette=Color ? Cast<UMaterialExpressionLinearInterpolate>(Color->A.Expression) : nullptr;
	const auto* Grain=Color ? Cast<UMaterialExpressionMultiply>(Color->Alpha.Expression) : nullptr;
	const auto* Rough=Cast<UMaterialExpressionClamp>(Slab->Roughness.Expression);
	const auto* RoughAdd=Rough ? Cast<UMaterialExpressionAdd>(Rough->Input.Expression) : nullptr;
	const auto* RoughWeight=RoughAdd ? Cast<UMaterialExpressionMultiply>(RoughAdd->B.Expression) : nullptr;
	const auto* RoughCenter=RoughWeight ? Cast<UMaterialExpressionSubtract>(RoughWeight->A.Expression) : nullptr;
	const auto* AO=Cast<UMaterialExpressionLinearInterpolate>(M->GetEditorOnlyData()->AmbientOcclusion.Expression);
	const auto* Normal=Cast<UMaterialExpressionNormalize>(Slab->Normal.Expression);
	const auto* XYZ=Normal ? Cast<UMaterialExpressionAppendVector>(Normal->VectorInput.Expression) : nullptr;
	const auto* Strength=XYZ ? Cast<UMaterialExpressionMultiply>(XYZ->A.Expression) : nullptr;
	const auto* Slope=Strength ? Cast<UMaterialExpressionDivide>(Strength->A.Expression) : nullptr;
	const auto* XY=Slope ? Cast<UMaterialExpressionAppendVector>(Slope->A.Expression) : nullptr;
	const auto* Z=Slope ? Cast<UMaterialExpressionClamp>(Slope->B.Expression) : nullptr;
	const auto* X=XY ? Cast<UMaterialExpressionComponentMask>(XY->A.Expression) : nullptr;
	const auto* Decode=X ? Cast<UMaterialExpressionSubtract>(X->Input.Expression) : nullptr;
	const auto* DecodeScale=Decode ? Cast<UMaterialExpressionMultiply>(Decode->A.Expression) : nullptr;
	const auto* NormalSample=DecodeScale ? Cast<UMaterialExpressionTextureSample>(DecodeScale->A.Expression) : nullptr;
	return Palette && Grain && RoughCenter && AO && XY && Z && NormalSample &&
		IsTextureChannel(Palette->Alpha.Expression,Targets[2],2) &&
		IsTextureChannel(Grain->A.Expression,Targets[0],2) &&
		IsTextureChannel(RoughCenter->A.Expression,Targets[2],1) &&
		IsTextureChannel(AO->B.Expression,Targets[2],0) &&
		NormalSample->Texture==Targets[1] && DecodeScale->A.OutputIndex==0 && IsChannel(XY->A.Expression,Decode,0) &&
		IsChannel(XY->B.Expression,Decode,1) && IsChannel(Z->Input.Expression,Decode,2);
}

bool Resolve(const FProjectMaterialPatternDependency& P, TArray<UTextureRenderTarget2D*>& Out, FString& Error)
{
	if (P.OutputContract != TEXT("soil_structure_normal_masks_v1"))
	{
		Error = TEXT("Soil material requires its reviewed three-output contract."); return false;
	}
	auto* Primary = LoadObject<UTextureRenderTarget2D>(nullptr, *P.OutputObjectPath);
	auto* Slot = Primary ? Cast<UProjectTextureOutputSlot>(Primary->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())) : nullptr;
	auto* C = Slot ? Slot->Catalog.Get() : nullptr;
	if (!C || C->OutputContract != FName(P.OutputContract) || C->SemanticIdentity != P.SemanticIdentity ||
		C->PatternId != FName(P.PatternId) || C->OutputSlots.Num() != 3 || C->Nodes.Num() != 3 ||
		C->GetOutermost() != Primary->GetOutermost() || Primary->GetOutermost()->IsDirty())
	{
		Error = TEXT("Loaded soil bundle is dirty, foreign, or does not match authenticated identity."); return false;
	}
	const TArray<FName> Names = {TEXT("Structure"),TEXT("Normal"),TEXT("Masks")};
	for (int32 Index=0; Index<3; ++Index)
	{
		auto* S = C->OutputSlots[Index].Get(); auto* RT = S ? S->GetRenderTarget() : nullptr;
		if (!RT || S->Catalog != C || S->SlotName != Names[Index] || S->NodeId != C->Nodes[Index].NodeId ||
			RT->GetOutermost() != Primary->GetOutermost() || RT->RenderTargetFormat != RTF_RGBA8 || RT->IsSRGB() ||
			RT->SizeX != Primary->SizeX || RT->SizeY != Primary->SizeY || !RT->bAutoGenerateMips ||
			RT->AddressX != TA_Wrap || RT->AddressY != TA_Wrap || RT->Filter != TF_Trilinear)
		{
			Error = TEXT("Soil sibling slot or native texture contract is invalid."); return false;
		}
		Out.Add(RT);
	}
	if (Out[0] != Primary) { Error = TEXT("Soil primary is not the Structure output."); return false; }
	return true;
}
}

bool ProjectMaterialSoilBuilder::Build(UMaterial* M, const FProjectMaterialSurfaceRecipe& R,
	const FProjectMaterialPatternDependency& P, TArray<FString>& OutErrors, FString& OutError)
{
	using namespace ProjectMaterialSoilBuilderPrivate;
	TArray<UTextureRenderTarget2D*> Targets;
	if (!Resolve(P, Targets, OutError)) return false;
	UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);
	// BasicShapes plane is 100 cm wide. A UV unit spans one metre in this fixture.
	E* UV = Div(M, Node<UMaterialExpressionTextureCoordinate>(M), Scalar(M,R,TEXT("PatternScaleMeters")));
	TArray<E*> Samples;
	for (auto* RT : Targets)
	{
		auto* S = Node<UMaterialExpressionTextureSample>(M); S->Texture = RT;
		S->SamplerType = SAMPLERTYPE_LinearColor; S->Coordinates.Connect(0,UV); Samples.Add(S);
	}
	E* Color = Lerp(M,Vector(M,R,TEXT("SoilDarkColor")),Vector(M,R,TEXT("SoilLightColor")),Channel(M,Samples[2],2));
	Color = Lerp(M,Color,Vector(M,R,TEXT("GrainColor")),Mul(M,Channel(M,Samples[0],2),Scalar(M,R,TEXT("GrainColorStrength"))));
	E* Roughness = Clamp(M,Add(M,Scalar(M,R,TEXT("BaseRoughness")),
		Mul(M,Sub(M,Channel(M,Samples[2],1),Constant(M,.5f)),Scalar(M,R,TEXT("RoughnessStrength")))),.05f,1);
	E* Decoded = Sub(M,Mul(M,Samples[1],Constant(M,2)),Constant(M,1));
	E* XY = Append(M,Channel(M,Decoded,0),Channel(M,Decoded,1));
	E* Z = Clamp(M,Channel(M,Decoded,2),.02f,1);
	E* Ratio = Div(M,Scalar(M,R,TEXT("BumpHeightCentimeters")),
		Mul(M,Scalar(M,R,TEXT("PatternScaleMeters")),Constant(M,2.5f)));
	auto* Normal = Node<UMaterialExpressionNormalize>(M);
	Normal->VectorInput.Connect(0,Append(M,Mul(M,Div(M,XY,Z),Ratio),Constant(M,1)));
	auto* F0 = Node<UMaterialExpressionConstant3Vector>(M); F0->Constant=FLinearColor(.04f,.04f,.04f);
	auto* Slab = Node<UMaterialExpressionSubstrateSlabBSDF>(M);
	Slab->DiffuseAlbedo.Connect(0,Color); Slab->Roughness.Connect(0,Roughness);
	Slab->F0.Connect(0,F0); Slab->Normal.Connect(0,Normal);
	M->GetEditorOnlyData()->FrontMaterial.Connect(0,Slab);
	M->GetEditorOnlyData()->AmbientOcclusion.Connect(0,Lerp(M,Constant(M,1),Channel(M,Samples[2],0),Scalar(M,R,TEXT("AOInfluence"))));
	// UE's Substrate compiler transforms this requested tangent basis to world itself.
	M->bTangentSpaceNormal=true; M->MaterialDomain=MD_Surface; M->BlendMode=BLEND_Opaque;
	M->SetShadingModel(MSM_DefaultLit);
	M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ProjectGenerationOwner"),TEXT("ProjectMaterial"));
	M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ProjectSurfaceArchetype"),*R.Archetype);
	M->PostEditChange(); OutErrors=UMaterialEditingLibrary::RecompileMaterial(M);
	if (!OutErrors.IsEmpty()) { OutError=FString::Join(OutErrors,TEXT(" | ")); return false; }
	M->MarkPackageDirty(); return true;
}

bool ProjectMaterialSoilBuilder::Verify(const FProjectMaterialSurfaceRecipe& R,
	const FProjectMaterialPatternDependency& P, const UObject* Asset, FString& OutError)
{
	using namespace ProjectMaterialSoilBuilderPrivate;
	TArray<UTextureRenderTarget2D*> Targets;
	if (!Resolve(P,Targets,OutError)) return false;
	const auto* M=Cast<UMaterial>(Asset);
	if (!M || !M->bTangentSpaceNormal || !M->HasSubstrateFrontMaterialConnected() ||
		!M->GetEditorOnlyData()->AmbientOcclusion.Expression)
	{
		OutError=TEXT("Reloaded soil material lacks native PBR connections."); return false;
	}
	TSet<FName> Parameters;
	TSet<const UTexture*> Textures;
	int32 Samples=0,Slabs=0;
	const UMaterialExpressionSubstrateSlabBSDF* Slab=nullptr;
	for (const auto* E : UMaterialEditingLibrary::GetMaterialExpressions(M))
	{
		if (const auto* S=Cast<UMaterialExpressionTextureSample>(E))
		{
			if (S->SamplerType!=SAMPLERTYPE_LinearColor || !S->Coordinates.Expression)
			{
				OutError=TEXT("Reloaded soil sampler or UV contract differs."); return false;
			}
			Textures.Add(S->Texture); ++Samples;
		}
		if (const auto* S=Cast<UMaterialExpressionScalarParameter>(E))
		{
			const auto* Value=R.Scalars.Find(S->ParameterName);
			if (!Value || !FMath::IsNearlyEqual(S->DefaultValue,float(*Value),1.e-5f))
			{
				OutError=TEXT("Reloaded soil scalar name or default differs."); return false;
			}
			Parameters.Add(S->ParameterName);
		}
		if (const auto* V=Cast<UMaterialExpressionVectorParameter>(E))
		{
			const auto* Value=R.Vectors.Find(V->ParameterName);
			if (!Value || !V->DefaultValue.Equals(*Value,1.e-6f))
			{
				OutError=TEXT("Reloaded soil vector name or default differs."); return false;
			}
			Parameters.Add(V->ParameterName);
		}
		if (const auto* S=Cast<UMaterialExpressionSubstrateSlabBSDF>(E)) {Slab=S; ++Slabs;}
	}
	if (Samples!=3 || Slabs!=1 || Textures.Num()!=3 || Parameters.Num()!=R.Scalars.Num()+R.Vectors.Num())
	{
		OutError=TEXT("Reloaded soil shader does not match its closed graph."); return false;
	}
	const auto* Normal=Cast<UMaterialExpressionNormalize>(Slab->Normal.Expression);
	const auto* F0=Cast<UMaterialExpressionConstant3Vector>(Slab->F0.Expression);
	if (M->GetEditorOnlyData()->FrontMaterial.Expression!=Slab || !Normal || !Normal->VectorInput.Expression ||
		!Cast<UMaterialExpressionClamp>(Slab->Roughness.Expression) ||
		!Cast<UMaterialExpressionLinearInterpolate>(Slab->DiffuseAlbedo.Expression) ||
		!Cast<UMaterialExpressionLinearInterpolate>(M->GetEditorOnlyData()->AmbientOcclusion.Expression) ||
		!F0 || !F0->Constant.Equals(FLinearColor(.04f,.04f,.04f),1.e-6f))
	{
		OutError=TEXT("Reloaded soil PBR input is disconnected or has the wrong native expression."); return false;
	}
	for (auto* RT : Targets) if (!Textures.Contains(RT)) { OutError=TEXT("Reloaded soil texture binding differs."); return false; }
	if (!VerifyRoutes(M,Slab,Targets))
	{
		OutError=TEXT("Reloaded soil texture channels do not follow their native PBR semantic routes."); return false;
	}
	return true;
}
