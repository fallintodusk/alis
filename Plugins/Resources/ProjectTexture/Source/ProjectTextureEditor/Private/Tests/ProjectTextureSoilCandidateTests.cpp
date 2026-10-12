// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureSoilBuilder.h"
#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "ProjectTextureRuntimeSubsystem.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectTextureSoilCandidateTests
{
using E = UMaterialExpression;
static TArray<TStrongObjectPtr<UObject>> PreviewObjects;

template<class T> T* Node(UMaterial* M)
{
	return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M,T::StaticClass()));
}
E* Constant(UMaterial* M,float Value)
{
	auto* N=Node<UMaterialExpressionConstant>(M); N->R=Value; return N;
}
E* Scalar(UMaterial* M,const TCHAR* Name,float Value)
{
	auto* N=Node<UMaterialExpressionScalarParameter>(M); N->ParameterName=Name; N->DefaultValue=Value; return N;
}
template<class T> E* Binary(UMaterial* M,E* A,E* B)
{
	auto* N=Node<T>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N;
}
E* Add(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionAdd>(M,A,B); }
E* Sub(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionSubtract>(M,A,B); }
E* Mul(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionMultiply>(M,A,B); }
E* Div(UMaterial* M,E* A,E* B) { return Binary<UMaterialExpressionDivide>(M,A,B); }
E* Invert(UMaterial* M,E* A)
{
	auto* N=Node<UMaterialExpressionOneMinus>(M); N->Input.Connect(0,A); return N;
}
E* Clamp(UMaterial* M,E* A)
{
	auto* N=Node<UMaterialExpressionClamp>(M); N->Input.Connect(0,A); return N;
}
E* Power(UMaterial* M,E* A,float Value)
{
	auto* N=Node<UMaterialExpressionPower>(M); N->Base.Connect(0,A); N->ConstExponent=Value; return N;
}
E* Append(UMaterial* M,E* A,E* B)
{
	auto* N=Node<UMaterialExpressionAppendVector>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N;
}
E* Channel(UMaterial* M,E* A,int32 Index)
{
	auto* N=Node<UMaterialExpressionComponentMask>(M);
	N->R=Index==0; N->G=Index==1; N->B=Index==2; N->A=Index==3; N->Input.Connect(0,A); return N;
}
E* RGB(UMaterial* M,E* A)
{
	auto* N=Node<UMaterialExpressionComponentMask>(M);
	N->R=N->G=N->B=true; N->A=false; N->Input.Connect(0,A); return N;
}
E* Lerp(UMaterial* M,E* A,E* B,E* Alpha)
{
	auto* N=Node<UMaterialExpressionLinearInterpolate>(M);
	N->A.Connect(0,A); N->B.Connect(0,B); N->Alpha.Connect(0,Alpha); return N;
}
E* Noise(UMaterial* M,E* P,int32 Period)
{
	auto* N=Node<UMaterialExpressionNoise>(M); N->Position.Connect(0,P);
	N->NoiseFunction=NOISEFUNCTION_GradientALU; N->Scale=Period; N->RepeatSize=Period;
	N->bTiling=true; N->bTurbulence=false; N->Levels=1; N->Quality=1; N->OutputMin=0; N->OutputMax=1; return N;
}
E* Population(UMaterial* M,E* P,int32 Period,E* Presence,E* Radius,E*& OutMask)
{
	auto* V=Node<UMaterialExpressionVectorNoise>(M);
	V->NoiseFunction=VNF_VoronoiALU; V->Quality=4; V->bTiling=true; V->TileSize=Period;
	V->Position.Connect(0,Mul(M,P,Constant(M,Period)));
	// Voronoi returns an unwrapped nearest-seed position. Wrap it before Cellnoise
	// so per-cell properties remain equal on both sides of a repeating tile.
	auto* Wrap=Node<UMaterialExpressionFrac>(M); Wrap->Input.Connect(0,Div(M,RGB(M,V),Constant(M,Period)));
	auto* Cell=Node<UMaterialExpressionVectorNoise>(M); Cell->NoiseFunction=VNF_CellnoiseALU;
	Cell->Position.Connect(0,Mul(M,Wrap,Constant(M,Period)));
	E* Present=Clamp(M,Mul(M,Sub(M,Presence,Channel(M,Cell,0)),Constant(M,12)));
	E* CellRadius=Mul(M,Radius,Add(M,Constant(M,.35f),Mul(M,Channel(M,Cell,2),Constant(M,.65f))));
	E* Shape=Power(M,Clamp(M,Invert(M,Div(M,Channel(M,V,3),CellRadius))),2);
	OutMask=Mul(M,Shape,Present);
	return Mul(M,OutMask,Add(M,Constant(M,.35f),Mul(M,Channel(M,Cell,1),Constant(M,.65f))));
}
UMaterial* BuildStructure()
{
	auto* M=NewObject<UMaterial>(GetTransientPackage(),TEXT("M_ProjectSoilNativeExperiment"));
	// Native shader compilation may collect transient UObjects while waiting.
	PreviewObjects.Emplace(M);
	auto* UV=Node<UMaterialExpressionTextureCoordinate>(M);
	E* P=Append(M,UV,Constant(M,1.17f));
	E* Warp=Scalar(M,TEXT("WarpStrength"),.025f);
	E* DX=Mul(M,Sub(M,Noise(M,P,4),Constant(M,.5f)),Warp);
	auto* Offset=Node<UMaterialExpressionConstant3Vector>(M); Offset->Constant=FLinearColor(0,0,7.13f);
	E* DY=Mul(M,Sub(M,Noise(M,Add(M,P,Offset),4),Constant(M,.5f)),Warp);
	E* Warped=Add(M,P,Append(M,Append(M,DX,DY),Constant(M,0)));
	E* Patch=Clamp(M,Add(M,Mul(M,Sub(M,Noise(M,P,4),Constant(M,.5f)),Constant(M,2)),Scalar(M,TEXT("LooseCoverage"),.5f)));
	E* AggregateMask=nullptr;
	E* Aggregate=Population(M,Warped,16,Scalar(M,TEXT("AggregatePresence"),.7f),Scalar(M,TEXT("AggregateRadius"),.7f),AggregateMask);
	E* GrainMask=nullptr;
	E* Grain=Population(M,P,128,Scalar(M,TEXT("GrainDensity"),.5f),Scalar(M,TEXT("GrainRadius"),.6f),GrainMask);
	E* Fine=Mul(M,Noise(M,P,64),Scalar(M,TEXT("FineStrength"),.06f));
	E* Base=Add(M,Mul(M,Noise(M,P,8),Constant(M,.12f)),Fine);
	E* Pore=Mul(M,Power(M,Clamp(M,Sub(M,Noise(M,P,128),Constant(M,.65f))),2),Invert(M,AggregateMask));
	E* Relief=Mul(M,Mul(M,Aggregate,Scalar(M,TEXT("AggregateHeight"),.5f)),Add(M,Constant(M,.2f),Mul(M,Patch,Constant(M,.8f))));
	E* Height=Clamp(M,Sub(M,Add(M,Add(M,Base,Relief),Mul(M,Grain,Scalar(M,TEXT("GrainHeight"),.25f))),Mul(M,Pore,Scalar(M,TEXT("PoreStrength"),.15f))));
	M->GetEditorOnlyData()->EmissiveColor.Connect(0,Append(M,Append(M,Height,AggregateMask),GrainMask));
	M->GetEditorOnlyData()->Opacity.Connect(0,Constant(M,1));
	M->MaterialDomain=MD_UI; M->BlendMode=BLEND_AlphaComposite; M->SetShadingModel(MSM_Unlit); M->PostEditChange();
	return M;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureSoilNativeCandidatesTest,
	"Project.Texture.Soil.NativeCandidates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter|
	EAutomationTestFlags::RequiresUser)
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTextureSoilNativeCandidatesTest,
	"Project.Texture.Soil.NativeCandidates","[Slow][Integration][ProjectTexture]")

bool FProjectTextureSoilNativeCandidatesTest::RunTest(const FString& Parameters)
{
	using namespace ProjectTextureSoilCandidateTests;
	if (!TestTrue(TEXT("The visual candidate fixture requires a renderer"),FApp::CanEverRender())) return false;
	UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestTrue(TEXT("Candidate generation uses an isolated template world"),World && World->GetOutermost()->GetName().StartsWith(TEXT("/Engine/Maps/Templates/")))) return false;
	if (!TestTrue(TEXT("The bounded candidate fixture runs once per Editor lifetime"),PreviewObjects.IsEmpty())) return false;
	auto* Runtime=GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
	if (!TestNotNull(TEXT("Existing ProjectTexture runtime owns generation"),Runtime)) return false;
	auto* Structure=BuildStructure();
	const auto Errors=UMaterialEditingLibrary::RecompileMaterial(Structure);
	if (!TestTrue(TEXT("Native VectorNoise graph compiles"),Errors.IsEmpty())) { for(const auto& Error:Errors) AddError(Error); return false; }
	const TArray<FString> Names={TEXT("Compacted"),TEXT("LooseAggregates"),TEXT("Gravelly")};
	const TArray<FName> Slots={TEXT("Structure"),TEXT("Normal"),TEXT("Masks")};
	const TArray<FName> Controls={TEXT("LooseCoverage"),TEXT("AggregatePresence"),TEXT("AggregateRadius"),TEXT("AggregateHeight"),TEXT("GrainDensity"),TEXT("GrainRadius"),TEXT("GrainHeight"),TEXT("FineStrength"),TEXT("PoreStrength"),TEXT("WarpStrength")};
	const float Values[3][10]={{.12f,.3f,.65f,.25f,.38f,.48f,.14f,.09f,.12f,.025f},
		{.88f,.9f,.9f,.8f,.6f,.6f,.26f,.05f,.3f,.035f},
		{.5f,.6f,.75f,.4f,.85f,.95f,.6f,.045f,.18f,.02f}};
	auto Receipt=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Candidates;
	const FString RunRoot=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/world/surface_presentation/t1d_native"));
	IFileManager::Get().MakeDirectory(*FPaths::Combine(RunRoot,TEXT("receipts")),true);
	for(int32 Candidate=0;Candidate<3;++Candidate)
	{
		auto Entry=MakeShared<FJsonObject>(); auto ParameterJson=MakeShared<FJsonObject>();
		FString IdentityInput=Names[Candidate]+TEXT("|native-voronoi-cellnoise-v1|512|seed=1.17");
		auto* MID=UMaterialInstanceDynamic::Create(Structure,GetTransientPackage(),*FString::Printf(TEXT("MID_ProjectSoilShape_%d"),Candidate));
		PreviewObjects.Emplace(MID);
		for(int32 I=0;I<Controls.Num();++I)
		{
			MID->SetScalarParameterValue(Controls[I],Values[Candidate][I]); ParameterJson->SetNumberField(Controls[I].ToString(),Values[Candidate][I]);
			IdentityInput+=FString::Printf(TEXT("|%s=%.9g"),*Controls[I].ToString(),Values[Candidate][I]);
		}
		auto* Catalog=NewObject<UProjectTextureRuntimeCatalog>(GetTransientPackage(),*FString::Printf(TEXT("DA_ProjectSoilCandidate_%d"),Candidate));
		PreviewObjects.Emplace(Catalog);
		Catalog->PatternId=FName(*Names[Candidate]); Catalog->OutputContract=TEXT("soil_structure_normal_masks_v1");
		Catalog->SemanticIdentity=FProjectTexturePatternRecipeContract::ComputeStringSha256(IdentityInput);
		Catalog->BudgetBytes=32*1024*1024;
		TArray<UTextureRenderTarget2D*> Targets; TArray<TSharedPtr<FJsonValue>> Paths;
		for(int32 I=0;I<3;++I)
		{
			auto* RT=NewObject<UTextureRenderTarget2D>(GetTransientPackage(),*FString::Printf(TEXT("RT_ProjectSoil_%d_%d"),Candidate,I));
			RT->RenderTargetFormat=RTF_RGBA8; RT->SRGB=false; RT->bForceLinearGamma=true; RT->bAutoGenerateMips=true;
			RT->AddressX=RT->AddressY=RT->MipsAddressU=RT->MipsAddressV=TA_Wrap; RT->Filter=RT->MipsSamplerFilter=TF_Trilinear; RT->InitAutoFormat(512,512);
			auto* Slot=NewObject<UProjectTextureOutputSlot>(RT); Slot->Catalog=Catalog; Slot->NodeId=Slot->SlotName=Slots[I]; RT->AddAssetUserData(Slot);
			Catalog->OutputSlots.Add(Slot); Targets.Add(RT); Paths.Add(MakeShared<FJsonValueString>(RT->GetPathName())); PreviewObjects.Emplace(RT);
		}
		for(int32 I=0;I<3;++I)
		{
			auto& NodeDesc=Catalog->Nodes.AddDefaulted_GetRef(); NodeDesc.NodeId=Slots[I];
			NodeDesc.StructuralIdentity=FProjectTexturePatternRecipeContract::ComputeStringSha256(IdentityInput+Slots[I].ToString());
			NodeDesc.Width=NodeDesc.Height=512;
			if(I==0) NodeDesc.GeneratorMaterial=MID;
			else
			{
				auto* Derived=NewObject<UMaterial>(GetTransientPackage()); FString Error;
				PreviewObjects.Emplace(Derived);
				if (!TestTrue(TEXT("Existing ProjectTexture derives coherent normal/masks"),ProjectTextureSoilBuilder::BuildDerivedMaterial(Derived,Targets[0],512,I==1,Error))) { AddError(Error); return false; }
				NodeDesc.GeneratorMaterial=Derived; NodeDesc.ParentNodeIds.Add(Slots[0]); NodeDesc.ParentTextureParameter=TEXT("ParentStructure");
			}
		}
		const auto Before=Runtime->GetTelemetry(); const double Start=FPlatformTime::Seconds(); FString Error;
		if (!TestTrue(TEXT("Complete candidate bundle generates through existing runtime"),Runtime->RequestOutput(World,Catalog->OutputSlots[0],Error))) { AddError(Error); return false; }
		const double ColdMilliseconds=(FPlatformTime::Seconds()-Start)*1000; const auto Cold=Runtime->GetTelemetry();
		TestEqual(TEXT("The candidate generates exactly three owner-local nodes"),Cold.Generations-Before.Generations,3);
		for(int32 I=0;I<100;++I) if(!Runtime->RequestOutput(World,Catalog->OutputSlots[0],Error)) { AddError(Error); return false; }
		const auto Warm=Runtime->GetTelemetry();
		TestEqual(TEXT("One hundred requests reuse frozen pixels"),Warm.Generations,Cold.Generations);
		TestEqual(TEXT("One hundred requests hit the owner cache"),Warm.CacheHits-Cold.CacheHits,100);
		Entry->SetStringField(TEXT("name"),Names[Candidate]); Entry->SetObjectField(TEXT("parameters"),ParameterJson);
		Entry->SetArrayField(TEXT("outputs"),Paths); Entry->SetStringField(TEXT("semantic_identity"),Catalog->SemanticIdentity);
		Entry->SetStringField(TEXT("shape_mid"),MID->GetPathName()); Entry->SetNumberField(TEXT("cpu_request_to_ready_ms"),ColdMilliseconds);
		Entry->SetNumberField(TEXT("resident_logical_bytes_delta"),Cold.ResidentBytes-Before.ResidentBytes);
		Entry->SetNumberField(TEXT("warm_generations"),Warm.Generations-Cold.Generations); Entry->SetNumberField(TEXT("warm_hits"),Warm.CacheHits-Cold.CacheHits);
		Candidates.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Receipt->SetArrayField(TEXT("candidates"),Candidates); Receipt->SetStringField(TEXT("envelope"),TEXT("Editor diagnostic; 512-square linear RGBA8 full mips; no production manifests; CPU timing is not GPU completion"));
	FString Json; FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Json));
	TestTrue(TEXT("Candidate parameters and identities are reviewable"),FFileHelper::SaveStringToFile(Json,*FPaths::Combine(RunRoot,TEXT("receipts/candidates.json"))));
	return true;
}
#endif
