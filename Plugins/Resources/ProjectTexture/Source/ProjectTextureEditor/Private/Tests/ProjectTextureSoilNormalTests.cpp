// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureSoilBuilder.h"
#include "MaterialEditingLibrary.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Misc/AutomationTest.h"
#include "PackageTools.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureSoilNormalTest,
	"Project.Texture.Soil.NormalControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureSoilNormalTest::RunTest(const FString& Parameters)
{
	constexpr int32 Size = 64;
	TStrongObjectPtr<UTextureRenderTarget2D> HeightOwner(NewObject<UTextureRenderTarget2D>());
	TStrongObjectPtr<UTextureRenderTarget2D> NormalOwner(NewObject<UTextureRenderTarget2D>());
	TStrongObjectPtr<UTextureRenderTarget2D> MasksOwner(NewObject<UTextureRenderTarget2D>());
	auto* Height=HeightOwner.Get(); auto* Normal=NormalOwner.Get(); auto* Masks=MasksOwner.Get();
	for (auto* RT : {Height, Normal, Masks})
	{
		RT->RenderTargetFormat = RTF_RGBA8; RT->bForceLinearGamma = true; RT->SRGB = false;
		RT->AddressX = RT->AddressY = TA_Wrap; RT->InitAutoFormat(Size, Size); RT->UpdateResourceImmediate(true);
	}
	TStrongObjectPtr<UMaterial> NormalMaterial(NewObject<UMaterial>());
	TStrongObjectPtr<UMaterial> MaskMaterial(NewObject<UMaterial>());
	auto* N=NormalMaterial.Get(); auto* A=MaskMaterial.Get();
	FString Error;
	if (!TestTrue(TEXT("Build native normal kernel"), ProjectTextureSoilBuilder::BuildDerivedMaterial(N,Height,Size,true,Error)) ||
		!TestTrue(TEXT("Build native cavity kernel"), ProjectTextureSoilBuilder::BuildDerivedMaterial(A,Height,Size,false,Error)))
	{
		AddError(Error); return false;
	}
	for (int32 Axis=-1; Axis<2; ++Axis)
	{
		TStrongObjectPtr<UMaterial> SourceOwner(NewObject<UMaterial>());
		auto* Source=SourceOwner.Get();
		auto Make=[Source](UClass* Class) { return UMaterialEditingLibrary::CreateMaterialExpression(Source,Class); };
		auto* Flat=CastChecked<UMaterialExpressionConstant3Vector>(Make(UMaterialExpressionConstant3Vector::StaticClass()));
		Flat->Constant=FLinearColor(.5,.5,.5);
		UMaterialExpression* RGB=Flat;
		if (Axis>=0)
		{
			auto* UV=Make(UMaterialExpressionTextureCoordinate::StaticClass());
			auto* C=CastChecked<UMaterialExpressionComponentMask>(Make(UMaterialExpressionComponentMask::StaticClass()));
			C->R=Axis==0; C->G=Axis==1; C->B=C->A=false; C->Input.Connect(0,UV);
			auto* Half=CastChecked<UMaterialExpressionConstant>(Make(UMaterialExpressionConstant::StaticClass())); Half->R=.5;
			auto* RG=CastChecked<UMaterialExpressionAppendVector>(Make(UMaterialExpressionAppendVector::StaticClass()));
			RG->A.Connect(0,C); RG->B.Connect(0,Half);
			auto* Packed=CastChecked<UMaterialExpressionAppendVector>(Make(UMaterialExpressionAppendVector::StaticClass()));
			Packed->A.Connect(0,RG); Packed->B.Connect(0,Half); RGB=Packed;
		}
		auto* Opacity=CastChecked<UMaterialExpressionConstant>(Make(UMaterialExpressionConstant::StaticClass())); Opacity->R=1;
		Source->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); Source->GetEditorOnlyData()->Opacity.Connect(0,Opacity);
		Source->MaterialDomain=MD_UI; Source->BlendMode=BLEND_AlphaComposite; Source->SetShadingModel(MSM_Unlit);
		Source->PostEditChange();
		TestTrue(TEXT("Control shader compiles"), UMaterialEditingLibrary::RecompileMaterial(Source).IsEmpty());
		UPackageTools::FlushAsyncCompilation({Source->GetOutermost()});
		auto* World=GEditor->GetEditorWorldContext().World();
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(World,Height,Source);
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(World,Normal,N);
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(World,Masks,A);
		FlushRenderingCommands();
		TArray<FColor> Pixels,AO;
		FReadSurfaceDataFlags Flags(RCM_MinMax); Flags.SetLinearToGamma(false);
		TestTrue(TEXT("Read native normal pixels"),Normal->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
		TestTrue(TEXT("Read cavity pixels"),Masks->GameThread_GetRenderTargetResource()->ReadPixels(AO,Flags));
		if (Pixels.Num()!=Size*Size || AO.Num()!=Size*Size) return false;
		const auto P=Pixels[(Size/2)*Size+Size/2].ReinterpretAsLinear();
		const FVector V(P.R*2-1,P.G*2-1,P.B*2-1);
		TestTrue(TEXT("Encoded vector is finite and nearly unit"),!V.ContainsNaN() && FMath::Abs(V.Size()-1)<.025);
		if (Axis<0)
		{
			TestTrue(TEXT("Flat height yields flat normal"),FMath::Abs(V.X)<.01 && FMath::Abs(V.Y)<.01 && V.Z>.99);
			TestTrue(TEXT("Flat height has no cavity occlusion"),AO[(Size/2)*Size+Size/2].R==255);
		}
		else
		{
			TestTrue(TEXT("Rising axis points its normal toward negative UV"),V[Axis]<-.015 && V[Axis]>-.04);
			TestTrue(TEXT("Orthogonal axis remains flat"),FMath::Abs(V[1-Axis])<.01);
		}
	}
	return true;
}
#endif
