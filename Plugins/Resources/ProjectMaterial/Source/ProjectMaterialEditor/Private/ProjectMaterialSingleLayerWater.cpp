// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialSingleLayerWater.h"
#include "ProjectMaterialExpressionUtils.h"
#include "ProjectMaterialPatternBindings.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialSurfaceDependency.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Engine/Texture.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectMaterialSingleLayerWaterPrivate
{
	using E = UMaterialExpression;
using namespace ProjectMaterialExpressionUtils;
	constexpr TCHAR RippleTexture[] = TEXT("/Water/Textures/Normals/T_Water_TilingNormal_With_Height_02_Softened.T_Water_TilingNormal_With_Height_02_Softened");
	constexpr TCHAR WavesFunction[] = TEXT("/Water/Materials/Functions/WaterTextureWaves.WaterTextureWaves");
	constexpr TCHAR DeriveFunction[] = TEXT("/Engine/Functions/Engine_MaterialFunctions02/Math/DeriveNormalZ_Function.DeriveNormalZ_Function");
	constexpr TCHAR OrientFunction[] = TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals.BlendAngleCorrectedNormals");





	E* Mask(UMaterial* M, E* A, bool R, bool G, bool B)
	{
		auto* N=Node<UMaterialExpressionComponentMask>(M); N->R=R; N->G=G; N->B=B; N->A=false; N->Input.Connect(0,A); return N;
	}
	E* Normalize(UMaterial* M, E* A) { auto* N=Node<UMaterialExpressionNormalize>(M); N->VectorInput.Connect(0,A); return N; }
	UMaterialExpressionMaterialFunctionCall* Function(UMaterial* M, const TCHAR* Path)
	{
		auto* F=LoadObject<UMaterialFunction>(nullptr,Path);
		auto* N=Node<UMaterialExpressionMaterialFunctionCall>(M);
		return F && N->SetMaterialFunction(F) ? N : nullptr;
	}
	bool Bind(UMaterialExpressionMaterialFunctionCall* F, const TCHAR* Name, E* Value)
	{
		if (!F) return false;
		for (auto& Input : F->FunctionInputs)
		{
			if (Input.Input.InputName == Name) { Input.Input.Connect(0,Value); return true; }
		}
		return false;
	}
	const E* Input(const UMaterialExpressionMaterialFunctionCall* F, const TCHAR* Name)
	{
		if (F) for (const auto& I : F->FunctionInputs) if (I.Input.InputName==Name) return I.Input.Expression;
		return nullptr;
	}
	bool IsConstant(const E* N, float Value)
	{
		const auto* C=Cast<UMaterialExpressionConstant>(N); return C && FMath::IsNearlyEqual(C->R,Value,1.e-6f);
	}
	bool IsMask(const E* N, const E* Source, bool R, bool G, bool B)
	{
		const auto* C=Cast<UMaterialExpressionComponentMask>(N);
		return C && C->Input.Expression==Source && C->R==R && C->G==G && C->B==B && !C->A;
	}
}

bool ProjectMaterialSingleLayerWater::ResolveResources(FProjectMaterialSurfaceDependency& OutResources, FString& OutError)
{
	using namespace ProjectMaterialSingleLayerWaterPrivate;
	OutResources = {};
	for (const TCHAR* Path : {RippleTexture,WavesFunction,DeriveFunction,OrientFunction})
	{
		FString Filename, Hash;
		UObject* Resource=StaticLoadObject(UObject::StaticClass(),nullptr,Path);
		if (!Resource || Resource->GetOutermost()->IsDirty())
		{
			OutError=FString::Printf(TEXT("Native river resource is unavailable or unsaved: %s"),Path); return false;
		}
		if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(Path)), &Filename) ||
			!FProjectSha256::HashFile(Filename,Hash))
		{
			OutError=FString::Printf(TEXT("Native river resource cannot be authenticated: %s"),Path); return false;
		}
		OutResources.NativeResourceHashes.Add(Path,Hash);
	}
	return true;
}

bool ProjectMaterialSingleLayerWater::Build(UMaterial* M, const FProjectMaterialSurfaceRecipe& R,
	TArray<FString>& OutCompileErrors, FString& OutError)
{
	using namespace ProjectMaterialSingleLayerWaterPrivate;
	UTexture* Texture=LoadObject<UTexture>(nullptr,RippleTexture);
	if (!Texture) { OutError=TEXT("Native river normal texture cannot be loaded."); return false; }
	UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);
	auto* Water=Node<UMaterialExpressionSubstrateSingleLayerWaterBSDF>(M);
	auto* Albedo=Vector(M,R,TEXT("WaterAlbedo")); auto* Extinction=Vector(M,R,TEXT("WaterExtinction"));
	auto* Phase=Scalar(M,R,TEXT("WaterPhaseG")); auto* Behind=Vector(M,R,TEXT("BehindWaterScale"));
	Water->BaseColor.Connect(0,Vector(M,R,TEXT("WaterSurfaceColor")));
	Water->Roughness.Connect(0,Scalar(M,R,TEXT("SurfaceRoughness")));
	Water->Specular.Connect(0,Scalar(M,R,TEXT("WaterSpecular")));
	Water->WaterAlbedo.Connect(0,Albedo); Water->WaterExtinction.Connect(0,Extinction);
	Water->WaterPhaseG.Connect(0,Phase); Water->ColorScaleBehindWater.Connect(0,Behind);
	auto* Volume=Node<UMaterialExpressionSingleLayerWaterMaterialOutput>(M);
	Volume->ScatteringCoefficients.Connect(0,Binary<UMaterialExpressionMultiply>(M,Albedo,Extinction));
	auto* Absorption=Node<UMaterialExpressionOneMinus>(M); Absorption->Input.Connect(0,Albedo);
	Volume->AbsorptionCoefficients.Connect(0,Binary<UMaterialExpressionMultiply>(M,Absorption,Extinction));
	Volume->PhaseG.Connect(0,Phase); Volume->ColorScaleBehindWater.Connect(0,Behind);
	auto* XY=Binary<UMaterialExpressionDivide>(M,WorldXY(M),Constant(M,100));
	auto* Waves=Function(M,WavesFunction);
	auto* Tex=Node<UMaterialExpressionTextureObjectParameter>(M);
	Tex->ParameterName=TEXT("RippleNormal"); Tex->Texture=Texture; Tex->SamplerType=SAMPLERTYPE_LinearColor;
	bool Bound=Bind(Waves,TEXT("Texture"),Tex);
	Bound &= Bind(Waves,TEXT("Coordinates"),Binary<UMaterialExpressionDivide>(M,XY,Scalar(M,R,TEXT("RippleTileMeters"))));
	Bound &= Bind(Waves,TEXT("Divisor"),Constant(M,1.0f/3.0f));
	Bound &= Bind(Waves,TEXT("Speed"),Scalar(M,R,TEXT("RippleSpeed")));
	Bound &= Bind(Waves,TEXT("Time"),Binary<UMaterialExpressionAdd>(M,
		Binary<UMaterialExpressionMultiply>(M,Node<UMaterialExpressionTime>(M),Scalar(M,R,TEXT("LiveTimeEnabled"))),Scalar(M,R,TEXT("PreviewSeconds"))));
	if (!Bound) { OutError=TEXT("Native WaterTextureWaves signature is unsupported."); return false; }
	auto* NormalXY=Binary<UMaterialExpressionSubtract>(M,
		Binary<UMaterialExpressionMultiply>(M,Mask(M,Waves,true,true,false),Constant(M,2)),Constant(M,1));
	auto* Derive=Function(M,DeriveFunction);
	if (!Bind(Derive,TEXT("XY vector"),NormalXY)) { OutError=TEXT("Native normal derivation signature is unsupported."); return false; }
	auto* Detail=Normalize(M,Binary<UMaterialExpressionAppendVector>(M,
		Binary<UMaterialExpressionMultiply>(M,Mask(M,Derive,true,true,false),Scalar(M,R,TEXT("RippleStrength"))),Mask(M,Derive,false,false,true)));
	E* OrientedNormal=WorldOrientedNormal(M,Detail,OutError);
	if (!OrientedNormal) return false;
	Water->Normal.Connect(0,OrientedNormal);
	M->GetEditorOnlyData()->FrontMaterial.Connect(0,Water);
	M->bTangentSpaceNormal=false; M->BlendMode=BLEND_Opaque; M->MaterialDomain=MD_Surface; M->SetShadingModel(MSM_SingleLayerWater);
	M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ProjectGenerationOwner"),TEXT("ProjectMaterial"));
	M->GetOutermost()->GetMetaData().SetValue(M,TEXT("ProjectSurfaceArchetype"),*R.Archetype);
	M->PostEditChange(); OutCompileErrors=UMaterialEditingLibrary::RecompileMaterial(M);
	if (!OutCompileErrors.IsEmpty()) { OutError=FString::Join(OutCompileErrors,TEXT(" | ")); return false; }
	M->MarkPackageDirty(); return true;
}

bool ProjectMaterialSingleLayerWater::Verify(const FProjectMaterialSurfaceRecipe& R, const UObject* Asset, FString& OutError)
{
	using namespace ProjectMaterialSingleLayerWaterPrivate;
	const UMaterial* M=Cast<UMaterial>(Asset);
	int32 Waters=0,Volumes=0,Textures=0,Functions=0;
	if (!M || M->bTangentSpaceNormal || M->BlendMode!=BLEND_Opaque) { OutError=TEXT("Native river material frame is invalid."); return false; }
	const UMaterialExpressionSubstrateSingleLayerWaterBSDF* Water=nullptr;
	const UMaterialExpressionSingleLayerWaterMaterialOutput* Volume=nullptr;
	const UMaterialExpressionTextureObjectParameter* Texture=nullptr;
	TMap<FString,const UMaterialExpressionMaterialFunctionCall*> Calls;
	TMap<FName,const E*> Parameters;
	for (const E* N : UMaterialEditingLibrary::GetMaterialExpressions(M))
	{
		Waters += N->IsA<UMaterialExpressionSubstrateSingleLayerWaterBSDF>() ? 1 : 0;
		Volumes += N->IsA<UMaterialExpressionSingleLayerWaterMaterialOutput>() ? 1 : 0;
		if (const auto* W=Cast<UMaterialExpressionSubstrateSingleLayerWaterBSDF>(N)) Water=W;
		if (const auto* V=Cast<UMaterialExpressionSingleLayerWaterMaterialOutput>(N)) Volume=V;
		if (const auto* Tex=Cast<UMaterialExpressionTextureObjectParameter>(N))
		{
			Texture=Tex; Textures += Tex->Texture && Tex->Texture->GetPathName()==RippleTexture && Tex->SamplerType==SAMPLERTYPE_LinearColor ? 1 : 100;
		}
		if (const auto* F=Cast<UMaterialExpressionMaterialFunctionCall>(N))
		{
			const FString Path=F->MaterialFunction ? F->MaterialFunction->GetPathName() : FString();
			Functions += Path==WavesFunction || Path==DeriveFunction || Path==OrientFunction ? 1 : 100;
			if (Calls.Contains(Path)) { OutError=TEXT("Native river functions must be distinct."); return false; }
			Calls.Add(Path,F);
		}
		FName Name;
		if (const auto* S=Cast<UMaterialExpressionScalarParameter>(N))
		{
			Name=S->ParameterName; const double* Expected=R.Scalars.Find(Name);
			if (!Expected || !FMath::IsNearlyEqual(S->DefaultValue,static_cast<float>(*Expected),1.e-6f))
			{
				OutError=TEXT("Native river scalar defaults differ from the recipe."); return false;
			}
		}
		if (const auto* V=Cast<UMaterialExpressionVectorParameter>(N))
		{
			Name=V->ParameterName; const FLinearColor* Expected=R.Vectors.Find(Name);
			if (!Expected || !V->DefaultValue.Equals(*Expected,1.e-6f))
			{
				OutError=TEXT("Native river vector defaults differ from the recipe."); return false;
			}
		}
		if (!Name.IsNone())
		{
			if (Parameters.Contains(Name)) { OutError=TEXT("Native river parameter identity is duplicated."); return false; }
			Parameters.Add(Name,N);
		}
	}
	if (Waters!=1 || Volumes!=1 || Textures!=1 || Functions!=3 || Parameters.Num()!=R.Scalars.Num()+R.Vectors.Num() ||
		M->GetEditorOnlyData()->FrontMaterial.Expression!=Water)
	{
		OutError=TEXT("Reloaded river graph does not match the closed native resource shape."); return false;
	}
	auto Parameter=[&Parameters](const TCHAR* Name){return Parameters.FindRef(FName(Name));};
	const E* Albedo=Parameter(TEXT("WaterAlbedo")); const E* Extinction=Parameter(TEXT("WaterExtinction"));
	const auto* Scatter=Cast<UMaterialExpressionMultiply>(Volume->ScatteringCoefficients.Expression);
	const auto* Absorb=Cast<UMaterialExpressionMultiply>(Volume->AbsorptionCoefficients.Expression);
	const auto* OneMinus=Absorb ? Cast<UMaterialExpressionOneMinus>(Absorb->A.Expression) : nullptr;
	if (Water->BaseColor.Expression!=Parameter(TEXT("WaterSurfaceColor")) || Water->WaterAlbedo.Expression!=Albedo ||
		Water->WaterExtinction.Expression!=Extinction || Water->Roughness.Expression!=Parameter(TEXT("SurfaceRoughness")) ||
		Water->Specular.Expression!=Parameter(TEXT("WaterSpecular")) || Water->WaterPhaseG.Expression!=Parameter(TEXT("WaterPhaseG")) ||
		Volume->PhaseG.Expression!=Water->WaterPhaseG.Expression || Water->ColorScaleBehindWater.Expression!=Parameter(TEXT("BehindWaterScale")) ||
		Volume->ColorScaleBehindWater.Expression!=Water->ColorScaleBehindWater.Expression ||
		!Scatter || Scatter->A.Expression!=Albedo || Scatter->B.Expression!=Extinction ||
		!Absorb || !OneMinus || OneMinus->Input.Expression!=Albedo || Absorb->B.Expression!=Extinction)
	{
		OutError=TEXT("Native river optical routes differ from the closed graph."); return false;
	}
	const auto* Waves=Calls.FindRef(WavesFunction); const auto* Derive=Calls.FindRef(DeriveFunction); const auto* Orient=Calls.FindRef(OrientFunction);
	const auto* Normal=Cast<UMaterialExpressionNormalize>(Water->Normal.Expression);
	const auto* Detail=Cast<UMaterialExpressionNormalize>(Input(Orient,TEXT("AdditionalNormal")));
	const auto* Append=Detail ? Cast<UMaterialExpressionAppendVector>(Detail->VectorInput.Expression) : nullptr;
	const auto* Strength=Append ? Cast<UMaterialExpressionMultiply>(Append->A.Expression) : nullptr;
	const auto* Decode=Cast<UMaterialExpressionSubtract>(Input(Derive,TEXT("XY vector")));
	const auto* Expand=Decode ? Cast<UMaterialExpressionMultiply>(Decode->A.Expression) : nullptr;
	const auto* Tile=Cast<UMaterialExpressionDivide>(Input(Waves,TEXT("Coordinates")));
	const auto* Meters=Tile ? Cast<UMaterialExpressionDivide>(Tile->A.Expression) : nullptr;
	const auto* WorldXY=Meters ? Cast<UMaterialExpressionComponentMask>(Meters->A.Expression) : nullptr;
	const auto* Time=Cast<UMaterialExpressionAdd>(Input(Waves,TEXT("Time")));
	const auto* Clock=Time ? Cast<UMaterialExpressionMultiply>(Time->A.Expression) : nullptr;
	if (!Waves || !Derive || !Orient || !Normal || Normal->VectorInput.Expression!=Orient ||
		!Cast<UMaterialExpressionVertexNormalWS>(Input(Orient,TEXT("BaseNormal"))) || !Append || !Strength ||
		!IsMask(Strength->A.Expression,Derive,true,true,false) || Strength->B.Expression!=Parameter(TEXT("RippleStrength")) ||
		!IsMask(Append->B.Expression,Derive,false,false,true) || !Decode || !Expand ||
		!IsMask(Expand->A.Expression,Waves,true,true,false) || !IsConstant(Expand->B.Expression,2) || !IsConstant(Decode->B.Expression,1) ||
		Input(Waves,TEXT("Texture"))!=Texture || !IsConstant(Input(Waves,TEXT("Divisor")),1.0f/3.0f) ||
		Input(Waves,TEXT("Speed"))!=Parameter(TEXT("RippleSpeed")) || !Tile || Tile->B.Expression!=Parameter(TEXT("RippleTileMeters")) ||
		!Meters || !IsConstant(Meters->B.Expression,100) || !WorldXY || !IsMask(WorldXY,WorldXY->Input.Expression,true,true,false) ||
		!Cast<UMaterialExpressionWorldPosition>(WorldXY->Input.Expression) || !Time || Time->B.Expression!=Parameter(TEXT("PreviewSeconds")) ||
		!Clock || Clock->B.Expression!=Parameter(TEXT("LiveTimeEnabled")) || !Cast<UMaterialExpressionTime>(Clock->A.Expression))
	{
		OutError=TEXT("Native river normal, coordinate or time route differs from the closed graph."); return false;
	}
	return true;
}
