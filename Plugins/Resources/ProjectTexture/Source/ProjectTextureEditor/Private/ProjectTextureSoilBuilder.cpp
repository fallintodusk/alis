// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureSoilBuilder.h"
#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace ProjectTextureSoilBuilderPrivate
{
using E = UMaterialExpression;

template <class T> T* Node(UMaterial* M)
{
	return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M, T::StaticClass()));
}

E* Constant(UMaterial* M, float Value)
{
	auto* N = Node<UMaterialExpressionConstant>(M);
	N->R = Value;
	return N;
}

E* Add(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionAdd>(M);
	N->A.Connect(0, A); N->B.Connect(0, B);
	return N;
}

E* Sub(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionSubtract>(M);
	N->A.Connect(0, A); N->B.Connect(0, B);
	return N;
}

E* Mul(UMaterial* M, E* A, float B)
{
	auto* N = Node<UMaterialExpressionMultiply>(M);
	N->A.Connect(0, A); N->ConstB = B;
	return N;
}

E* Clamp(UMaterial* M, E* A)
{
	auto* N = Node<UMaterialExpressionClamp>(M);
	N->Input.Connect(0, A); N->MinDefault = 0; N->MaxDefault = 1;
	return N;
}

E* Mul(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionMultiply>(M);
	N->A.Connect(0,A); N->B.Connect(0,B); return N;
}

E* Lerp(UMaterial* M, E* A, E* B, E* Weight)
{
	auto* N = Node<UMaterialExpressionLinearInterpolate>(M);
	N->A.Connect(0,A); N->B.Connect(0,B); N->Alpha.Connect(0,Weight); return N;
}

E* Power(UMaterial* M, E* A, float Exponent)
{
	auto* N = Node<UMaterialExpressionPower>(M);
	N->Base.Connect(0, A); N->ConstExponent = Exponent;
	return N;
}

E* Invert(UMaterial* M, E* A)
{
	auto* N = Node<UMaterialExpressionOneMinus>(M);
	N->Input.Connect(0, A);
	return N;
}

E* Append(UMaterial* M, E* A, E* B)
{
	auto* N = Node<UMaterialExpressionAppendVector>(M);
	N->A.Connect(0, A); N->B.Connect(0, B);
	return N;
}

E* Channel(UMaterial* M, E* A, int32 Index, int32 Output = 0)
{
	auto* N = Node<UMaterialExpressionComponentMask>(M);
	N->R = Index == 0; N->G = Index == 1; N->B = Index == 2; N->A = false;
	N->Input.Connect(Output, A);
	return N;
}

E* Vector(UMaterial* M, float X, float Y, float Z)
{
	auto* N = Node<UMaterialExpressionConstant3Vector>(M);
	N->Constant = FLinearColor(X, Y, Z);
	return N;
}

E* Noise(UMaterial* M, E* P, ENoiseFunction Kind, uint32 Frequency)
{
	// Power-of-two periods keep native NoiseTileWrap lattice hashes exact.
	check(FMath::IsPowerOfTwo(Frequency));
	auto* N = Node<UMaterialExpressionNoise>(M);
	N->Position.Connect(0, P); N->NoiseFunction = Kind;
	N->Scale = Frequency; N->RepeatSize = Frequency; N->bTiling = true;
	N->bTurbulence = false; N->Levels = 1; N->Quality = Kind == NOISEFUNCTION_VoronoiALU ? 4 : 1;
	N->OutputMin = 0; N->OutputMax = 1;
	return N;
}

E* Sample(UMaterial* M, UTextureRenderTarget2D* Parent, E* UV)
{
	auto* N = Node<UMaterialExpressionTextureSampleParameter2D>(M);
	N->ParameterName = TEXT("ParentStructure"); N->Texture = Parent;
	N->SamplerType = SAMPLERTYPE_LinearColor;
	N->Coordinates.Connect(0, UV);
	return N;
}

E* Offset(UMaterial* M, E* UV, float X, float Y)
{
	return Append(M, Add(M, Channel(M, UV, 0), Constant(M, X)),
		Add(M, Channel(M, UV, 1), Constant(M, Y)));
}

E* Structure(UMaterial* M, int32 Seed)
{
	auto* UV = Node<UMaterialExpressionTextureCoordinate>(M);
	E* P = Add(M, Append(M, UV, Constant(M, 0)),
		Vector(M, Seed % 17, Seed % 29, (Seed % 31) * .17f));
	E* WarpX = Mul(M, Sub(M, Noise(M, P, NOISEFUNCTION_GradientALU, 2), Constant(M, .5f)), .035f);
	E* WarpY = Mul(M, Sub(M, Noise(M, Add(M, P, Vector(M, 0, 0, 8.17f)),
		NOISEFUNCTION_GradientALU, 2), Constant(M, .5f)), .035f);
	E* Warped = Add(M, P, Append(M, Append(M, WarpX, WarpY), Constant(M, 0)));
	// Patch weights mix fixed periodic populations; changing frequency with position
	// would break the native wrap contract. Small warp avoids long stretched ridges.
	E* Loose = Clamp(M,Mul(M,Sub(M,Noise(M,P,NOISEFUNCTION_GradientALU,4),Constant(M,.28f)),2.2f));
	E* Clump = Power(M, Clamp(M, Sub(M, Mul(M,
		Invert(M, Noise(M, Warped, NOISEFUNCTION_VoronoiALU, 32)), 1.65f), Constant(M, .25f))), 2.8f);
	E* FineClump = Power(M, Invert(M, Noise(M, Warped, NOISEFUNCTION_VoronoiALU, 64)), 3.5f);
	E* Grain = Power(M, Invert(M, Noise(M, P, NOISEFUNCTION_VoronoiALU, 128)), 6.0f);
	Grain = Mul(M,Grain,Clamp(M,Mul(M,Sub(M,Noise(M,P,NOISEFUNCTION_ValueALU,16),Constant(M,.30f)),2.0f)));
	E* Pore = Power(M, Clamp(M, Mul(M, Sub(M,
		Noise(M, Warped, NOISEFUNCTION_ValueALU, 128), Constant(M, .63f)), 2.7f)), 2);
	E* Aggregates = Lerp(M,Mul(M,FineClump,.32f),Add(M,Mul(M,Clump,.56f),Mul(M,FineClump,.30f)),Loose);
	E* Height = Clamp(M,Sub(M,Add(M,Add(M,Aggregates,Mul(M,Grain,.32f)),Mul(M,Loose,.06f)),Mul(M,Pore,.10f)));
	return Append(M,Append(M,Height,Mul(M,Clump,Loose)),Clamp(M,Add(M,Grain,Mul(M,Pore,.6f))));
}

E* Derived(UMaterial* M, UTextureRenderTarget2D* Parent, int32 Size, bool bNormal, int32 Seed=0)
{
	auto* UV = Node<UMaterialExpressionTextureCoordinate>(M);
	E* Center = Sample(M, Parent, UV);
	const float Step = 1.0f / Size;
	E* Left = Channel(M, Sample(M, Parent, Offset(M, UV, -Step, 0)), 0);
	E* Right = Channel(M, Sample(M, Parent, Offset(M, UV, Step, 0)), 0);
	E* Down = Channel(M, Sample(M, Parent, Offset(M, UV, 0, -Step)), 0);
	E* Up = Channel(M, Sample(M, Parent, Offset(M, UV, 0, Step)), 0);
	if (bNormal)
	{
		// Reference height/tile ratio .025 keeps slopes representable in RGB8.
		// The material reconstructs slopes and scales by its actual physical ratio.
		E* X = Mul(M, Sub(M, Left, Right), .025f / (2 * Step));
		E* Y = Mul(M, Sub(M, Down, Up), .025f / (2 * Step));
		auto* N = Node<UMaterialExpressionNormalize>(M);
		N->VectorInput.Connect(0, Append(M, Append(M, X, Y), Constant(M, 1)));
		return Add(M, Mul(M, N, .5f), Constant(M, .5f));
	}
	E* H = Channel(M, Center, 0);
	// Positive neighbor-height excess approximates local cavity, never cast shadows.
	E* Excess = Constant(M, 0);
	for (E* Neighbor : {Left, Right, Down, Up})
	{
		Excess = Add(M, Excess, Clamp(M, Sub(M, Neighbor, H)));
	}
	E* AO = Invert(M, Clamp(M, Mul(M, Excess, 3.0f)));
	// Surface composition varies independently of height while retaining grain response.
	E* P = Add(M,Append(M,UV,Constant(M,0)),Vector(M,Seed%17,Seed%29,(Seed%31)*.17f));
	E* Compaction = Noise(M,Add(M,P,Vector(M,0,0,15.31f)),NOISEFUNCTION_GradientALU,8);
	E* Roughness = Clamp(M,Add(M,Mul(M,Compaction,.65f),Mul(M,Channel(M,Center,2),.35f)));
	E* Pigment = Clamp(M,Add(M,Mul(M,Noise(M,Add(M,P,Vector(M,0,0,23.73f)),NOISEFUNCTION_GradientALU,4),.75f),
		Mul(M,Noise(M,P,NOISEFUNCTION_ValueALU,32),.25f)));
	return Append(M, Append(M, AO, Roughness), Pigment);
}

bool Finish(UMaterial* M, E* RGB, FString& Error)
{
	M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
	M->GetEditorOnlyData()->Opacity.Connect(0, Constant(M, 1));
	M->MaterialDomain = MD_UI; M->BlendMode = BLEND_AlphaComposite;
	M->SetShadingModel(MSM_Unlit); M->PostEditChange();
	const auto Errors = UMaterialEditingLibrary::RecompileMaterial(M);
	if (!Errors.IsEmpty()) { Error = FString::Join(Errors, TEXT(" | ")); return false; }
	M->MarkPackageDirty();
	return true;
}
}

bool ProjectTextureSoilBuilder::BuildDerivedMaterial(UMaterial* Material,
	UTextureRenderTarget2D* Parent, int32 Size, bool bNormal, FString& OutError)
{
	using namespace ProjectTextureSoilBuilderPrivate;
	return Finish(Material, Derived(Material, Parent, Size, bNormal), OutError);
}

bool ProjectTextureSoilBuilder::VerifyDerivedMaterial(const UMaterial* M,const UTextureRenderTarget2D* Parent,
	int32 Size,bool bNormal,FString& Error)
{
	using namespace ProjectTextureSoilBuilderPrivate;
	if (!M || !Parent || Size<=0) { Error=TEXT("Derived material has no bounded parent."); return false; }
	auto IsConstant=[](const E* N,float V)
	{
		const auto* C=Cast<UMaterialExpressionConstant>(N); return C && FMath::IsNearlyEqual(C->R,V,1.e-6f);
	};
	auto IsScaled=[](const UMaterialExpressionMultiply* N,float V)
	{
		return N && N->A.Expression && !N->B.Expression && FMath::IsNearlyEqual(N->ConstB,V,1.e-6f);
	};
	E* Root=M->GetEditorOnlyData()->EmissiveColor.Expression;
	if (bNormal)
	{
		const auto* Add=Cast<UMaterialExpressionAdd>(Root);
		const auto* Half=Add ? Cast<UMaterialExpressionMultiply>(Add->A.Expression) : nullptr;
		const auto* Unit=Half ? Cast<UMaterialExpressionNormalize>(Half->A.Expression) : nullptr;
		const auto* XYZ=Unit ? Cast<UMaterialExpressionAppendVector>(Unit->VectorInput.Expression) : nullptr;
		const auto* XY=XYZ ? Cast<UMaterialExpressionAppendVector>(XYZ->A.Expression) : nullptr;
		const auto* X=XY ? Cast<UMaterialExpressionMultiply>(XY->A.Expression) : nullptr;
		const auto* Y=XY ? Cast<UMaterialExpressionMultiply>(XY->B.Expression) : nullptr;
		if (!Add || !IsConstant(Add->B.Expression,.5f) || !IsScaled(Half,.5f) || !XYZ || !XY ||
			!IsConstant(XYZ->B.Expression,1) || !IsScaled(X,.025f*Size/2) || !IsScaled(Y,.025f*Size/2) ||
			!Cast<UMaterialExpressionSubtract>(X->A.Expression) || !Cast<UMaterialExpressionSubtract>(Y->A.Expression))
		{ Error=TEXT("Derived normal is not the coherent encoded central-difference route."); return false; }
	}
	else
	{
		const auto* RGB=Cast<UMaterialExpressionAppendVector>(Root);
		const auto* RG=RGB ? Cast<UMaterialExpressionAppendVector>(RGB->A.Expression) : nullptr;
		const auto* AO=RG ? Cast<UMaterialExpressionOneMinus>(RG->A.Expression) : nullptr;
		const auto* Cavity=AO ? Cast<UMaterialExpressionClamp>(AO->Input.Expression) : nullptr;
		const auto* Excess=Cavity ? Cast<UMaterialExpressionMultiply>(Cavity->Input.Expression) : nullptr;
		const auto* Rough=RG ? Cast<UMaterialExpressionClamp>(RG->B.Expression) : nullptr;
		const auto* Sum=Rough ? Cast<UMaterialExpressionAdd>(Rough->Input.Expression) : nullptr;
		const auto* Compaction=Sum ? Cast<UMaterialExpressionMultiply>(Sum->A.Expression) : nullptr;
		const auto* Grain=Sum ? Cast<UMaterialExpressionMultiply>(Sum->B.Expression) : nullptr;
		const auto* Pigment=RGB ? Cast<UMaterialExpressionClamp>(RGB->B.Expression) : nullptr;
		const auto* PigmentSum=Pigment ? Cast<UMaterialExpressionAdd>(Pigment->Input.Expression) : nullptr;
		const auto* Macro=PigmentSum ? Cast<UMaterialExpressionMultiply>(PigmentSum->A.Expression) : nullptr;
		const auto* Micro=PigmentSum ? Cast<UMaterialExpressionMultiply>(PigmentSum->B.Expression) : nullptr;
		if (!RG || !IsScaled(Excess,3) || !Cast<UMaterialExpressionAdd>(Excess->A.Expression) ||
			!IsScaled(Compaction,.65f) || !IsScaled(Grain,.35f) || !IsScaled(Macro,.75f) || !IsScaled(Micro,.25f))
		{ Error=TEXT("Derived masks do not preserve independent cavity, roughness and pigment routes."); return false; }
	}
	TSet<E*> Visited; TArray<E*> Queue{Root}; int32 Samples=0;
	while (!Queue.IsEmpty())
	{
		auto* N=Queue.Pop(); if (!N || Visited.Contains(N)) continue; Visited.Add(N);
		if (const auto* S=Cast<UMaterialExpressionTextureSampleParameter2D>(N))
		{
			if (S->ParameterName!=TEXT("ParentStructure") || S->Texture!=Parent || S->SamplerType!=SAMPLERTYPE_LinearColor || !S->Coordinates.Expression)
			{ Error=TEXT("Derived output samples a foreign or disconnected filtered parent."); return false; }
			++Samples;
		}
		for (FExpressionInputIterator It{N};It;++It) Queue.Add(It->Expression);
	}
	if (Samples!=(bNormal ? 4 : 5)) { Error=TEXT("Derived output has incomplete neighboring-height coverage."); return false; }
	return true;
}

bool ProjectTextureSoilBuilder::Build(const FProjectTexturePatternRecipe& Recipe,
	const FString& PackageName, UObject*& OutAsset, FString& OutError)
{
	using namespace ProjectTextureSoilBuilderPrivate;
	OutAsset = nullptr;
	if (Recipe.Nodes.Num() != 3 || Recipe.Exports.Num() != 3) { OutError = TEXT("Invalid soil bundle."); return false; }
	UPackage* Package = CreatePackage(*PackageName);
	auto* Catalog = NewObject<UProjectTextureRuntimeCatalog>(Package, *Recipe.CatalogId, RF_Public | RF_Standalone);
	const int32 Size = Recipe.ResolutionClass == TEXT("low") ? 256 : Recipe.ResolutionClass == TEXT("high") ? 1024 : 512;
	Catalog->PatternId = FName(Recipe.PatternId);
	Catalog->OutputContract = FName(Recipe.OutputContract);
	Catalog->BudgetBytes = 32 * 1024 * 1024;
	Catalog->SemanticIdentity = FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(
		Recipe, PackageName + TEXT(".") + Recipe.Exports[0].DescriptorId);
	TArray<UTextureRenderTarget2D*> Targets;
	for (const auto& Export : Recipe.Exports)
	{
		auto* Target = NewObject<UTextureRenderTarget2D>(Package, *Export.DescriptorId, RF_Public | RF_Standalone);
		Target->RenderTargetFormat = RTF_RGBA8; Target->SRGB = false; Target->bForceLinearGamma = true;
		Target->AddressX = Target->AddressY = Target->MipsAddressU = Target->MipsAddressV = TA_Wrap;
		Target->Filter = Target->MipsSamplerFilter = TF_Trilinear; Target->bAutoGenerateMips = true;
		Target->InitAutoFormat(Size, Size);
		auto* Slot = NewObject<UProjectTextureOutputSlot>(Target);
		Slot->Catalog = Catalog; Slot->NodeId = FName(Export.NodeId); Slot->SlotName = FName(Export.Slot);
		Target->AddAssetUserData(Slot); Catalog->OutputSlots.Add(Slot); Targets.Add(Target);
		FAssetRegistryModule::AssetCreated(Target);
		Package->GetMetaData().SetValue(Target, TEXT("ProjectGenerationOwner"), TEXT("ProjectTexture"));
		Package->GetMetaData().SetValue(Target, TEXT("ProjectPatternId"), *Recipe.PatternId);
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* M = NewObject<UMaterial>(Package,
			*FString::Printf(TEXT("M_ProjectSoilGenerator_%d"), Index), RF_Public | RF_Standalone);
		E* RGB = Index == 0 ? Structure(M, Recipe.Seed) : Derived(M, Targets[0], Size, Index == 1, Recipe.Seed);
		if (!Finish(M, RGB, OutError)) return false;
		FAssetRegistryModule::AssetCreated(M);
		Package->GetMetaData().SetValue(M, TEXT("ProjectGenerationOwner"), TEXT("ProjectTexture"));
		Package->GetMetaData().SetValue(M, TEXT("ProjectPatternId"), *Recipe.PatternId);
		auto& N = Catalog->Nodes.AddDefaulted_GetRef();
		N.NodeId = FName(Recipe.Nodes[Index].NodeId); N.StructuralIdentity = Recipe.Nodes[Index].StructuralIdentity;
		for (const auto& Parent : Recipe.Nodes[Index].Parents) N.ParentNodeIds.Add(FName(Parent));
		N.GeneratorMaterial = M; N.ParentTextureParameter = Index == 0 ? NAME_None : FName(TEXT("ParentStructure"));
		N.Width = N.Height = Size;
	}
	FAssetRegistryModule::AssetCreated(Catalog);
	Package->GetMetaData().SetValue(Catalog, TEXT("ProjectGenerationOwner"), TEXT("ProjectTexture"));
	Package->GetMetaData().SetValue(Catalog, TEXT("ProjectPatternId"), *Recipe.PatternId);
	Package->MarkPackageDirty(); OutAsset = Targets[0];
	return true;
}

bool ProjectTextureSoilBuilder::Verify(const FProjectTexturePatternRecipe& Recipe, UObject* Asset, FString& OutError)
{
	auto* Target = Cast<UTextureRenderTarget2D>(Asset);
	auto* Primary = Target ? Cast<UProjectTextureOutputSlot>(Target->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())) : nullptr;
	auto* Catalog = Primary ? Primary->Catalog.Get() : nullptr;
	if (!Catalog || Catalog->OutputContract != FName(Recipe.OutputContract) || Catalog->Nodes.Num() != 3 ||
		Catalog->OutputSlots.Num() != 3 || Catalog->SemanticIdentity != FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(Recipe, Asset->GetPathName()))
	{
		OutError = TEXT("Reloaded soil catalog identity or shape is invalid."); return false;
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const auto* Slot = Catalog->OutputSlots[Index].Get();
		const auto* RT = Slot ? Slot->GetRenderTarget() : nullptr;
		const auto& N = Catalog->Nodes[Index];
		const auto* M = Cast<UMaterial>(N.GeneratorMaterial);
		const auto* Opacity = M ? Cast<UMaterialExpressionConstant>(M->GetEditorOnlyData()->Opacity.Expression) : nullptr;
		const int32 ExpectedSize = Recipe.ResolutionClass == TEXT("low") ? 256 : Recipe.ResolutionClass == TEXT("high") ? 1024 : 512;
		TArray<FName> Parents;
		for (const auto& Parent : Recipe.Nodes[Index].Parents) Parents.Add(FName(Parent));
		if (!RT || Slot->Catalog != Catalog || Slot->SlotName != FName(Recipe.Exports[Index].Slot) ||
			Slot->NodeId != N.NodeId || N.StructuralIdentity != Recipe.Nodes[Index].StructuralIdentity ||
			N.ParentNodeIds != Parents || N.Width != ExpectedSize || N.Height != ExpectedSize ||
			N.ParentTextureParameter != (Index == 0 ? NAME_None : FName(TEXT("ParentStructure"))) ||
			RT->GetOutermost() != Asset->GetOutermost() || Catalog->GetOutermost() != Asset->GetOutermost() ||
			RT->GetName() != Recipe.Exports[Index].DescriptorId || RT->SizeX != N.Width || RT->SizeY != N.Height ||
			RT->RenderTargetFormat != RTF_RGBA8 || RT->IsSRGB() || !RT->bAutoGenerateMips ||
			RT->AddressX != TA_Wrap || RT->AddressY != TA_Wrap || RT->MipsAddressU != TA_Wrap || RT->MipsAddressV != TA_Wrap ||
			RT->Filter != TF_Trilinear || RT->MipsSamplerFilter != TF_Trilinear || !M ||
			M->GetOutermost() != Asset->GetOutermost() || M->MaterialDomain != MD_UI || M->BlendMode != BLEND_AlphaComposite ||
			!Opacity || !FMath::IsNearlyEqual(Opacity->R, 1.f) || !M->GetEditorOnlyData()->EmissiveColor.Expression)
		{
			OutError = TEXT("Reloaded soil slot, format, generator, or owner is invalid."); return false;
		}
		for (const UObject* Object : {static_cast<const UObject*>(RT), static_cast<const UObject*>(M), static_cast<const UObject*>(Catalog)})
		{
			auto& Metadata = Asset->GetOutermost()->GetMetaData();
			if (Metadata.GetValue(Object, TEXT("ProjectGenerationOwner")) != TEXT("ProjectTexture") ||
				Metadata.GetValue(Object, TEXT("ProjectPatternId")) != Recipe.PatternId)
			{
				OutError = TEXT("Reloaded soil ownership metadata is invalid."); return false;
			}
		}
		if (Index && !VerifyDerivedMaterial(M,Catalog->OutputSlots[0]->GetRenderTarget(),ExpectedSize,Index==1,OutError)) return false;
	}
	return true;
}
