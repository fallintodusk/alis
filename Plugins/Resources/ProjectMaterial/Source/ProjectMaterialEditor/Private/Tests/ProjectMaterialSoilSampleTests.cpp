// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialSurfaceGenerationService.h"
#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialFixtureDomain.h"
#include "ProjectMaterialSoilBuilder.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectTextureFixtureDomain.h"
#include "ProjectTextureGenerationService.h"
#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "ProjectTextureRuntimeSubsystem.h"
#include "AssetReferencingPolicySettings.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MaterialEditingLibrary.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialSoilSampleTest,
	"Project.Material.Soil.AuthenticatedSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialSoilSampleTest::RunTest(const FString& Parameters)
{
	const FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString TextureRoot=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/texture/generation/soil_"))+Id;
	const FString MaterialRoot=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/material/generation/soil_"))+Id;
	const FString SourceTexture=FPaths::Combine(FPaths::ProjectDir(),TEXT("Plugins/Resources/ProjectTexture/Data/TestFixtures/Soil/project_soil_fixture.pattern.json"));
	const FString SourceMaterial=FPaths::Combine(FPaths::ProjectDir(),TEXT("Plugins/Resources/ProjectMaterial/Data/TestFixtures/Soil"));
	auto* Settings=GetMutableDefault<UAssetReferencingPolicySettings>();
	const auto PolicyBefore=Settings->AdditionalDomains;
	const bool HadTexture=PolicyBefore.ContainsByPredicate([](const auto& R){return R.DomainName==TEXT("ProjectTextureFixture");});
	const bool HadMaterial=PolicyBefore.ContainsByPredicate([](const auto& R){return R.DomainName==TEXT("ProjectMaterialFixture");});
	ON_SCOPE_EXIT
	{
		const uint64 AfterFrame=GFrameCounter+2;
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,Settings,PolicyBefore,HadTexture,HadMaterial,AfterFrame]()
		{
			if (GFrameCounter<AfterFrame) return false;
			if (!HadMaterial) ProjectMaterialFixtureDomain::Shutdown();
			if (!HadTexture) ProjectTextureFixtureDomain::Shutdown();
			bool Same=Settings->AdditionalDomains.Num()==PolicyBefore.Num();
			for (int32 I=0; Same && I<PolicyBefore.Num(); ++I)
				Same=FARPDomainDefinitionByContentRoot::StaticStruct()->CompareScriptStruct(&Settings->AdditionalDomains[I],&PolicyBefore[I],0);
			TestTrue(TEXT("Native save validators complete before exact fixture policy restoration"),Same);
			return true;
		}));
	};
	FProjectTextureGenerationRequest Texture;
	Texture.RecipeRoot=FPaths::Combine(TextureRoot,TEXT("recipes"));
	Texture.OutputPackageRoot=TEXT("/ProjectTextureTest/Patterns");
	Texture.OutputContentRoot=FPaths::Combine(TextureRoot,TEXT("content/Patterns"));
	Texture.ManifestRoot=FPaths::Combine(TextureRoot,TEXT("manifests"));
	const FString TextureFile=FPaths::Combine(Texture.RecipeRoot,TEXT("Soil/project_soil_fixture.pattern.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(TextureFile),true);
	FString Json,Error;
	if (!TestTrue(TEXT("Read self-contained texture fixture"),FFileHelper::LoadFileToString(Json,*SourceTexture)) ||
		!TestTrue(TEXT("Copy isolated recipe"),FFileHelper::SaveStringToFile(Json,*TextureFile))) return false;
	TSharedPtr<FJsonObject> Reordered;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Reordered);
	auto Nodes=Reordered->GetArrayField(TEXT("nodes")); Nodes.Swap(1,2);
	Reordered->SetArrayField(TEXT("nodes"),Nodes);
	FString BadJson;
	FJsonSerializer::Serialize(Reordered.ToSharedRef(),TJsonWriterFactory<>::Create(&BadJson));
	FProjectTexturePatternRecipe BadRecipe;
	TestFalse(TEXT("Unsupported node order refuses at parsing before assets"),FProjectTexturePatternRecipeContract::Parse(BadJson,TextureFile,Texture.RecipeRoot,BadRecipe,Error));
	FPackageName::RegisterMountPoint(TEXT("/ProjectTextureTest/"),FPaths::Combine(TextureRoot,TEXT("content/")));
	FProjectTextureGenerationResult Generated;
	if (!TestTrue(TEXT("Producer compiler generates and reload-verifies bundle"),FProjectTextureGenerationService::RegenerateTestMount(Texture,Generated)))
	{
		AddError(Generated.Error); return false;
	}
	TestEqual(TEXT("One bundle generated"),Generated.Generated,1);
	FProjectTextureGenerationResult WarmCompile;
	TestTrue(TEXT("Unchanged bundle generation skips"),FProjectTextureGenerationService::RegenerateTestMount(Texture,WarmCompile));
	TestEqual(TEXT("No second bundle write"),WarmCompile.Generated,0);
	// A mount retarget must not reuse UObjects whose saved bytes belong to the first root.
	const FString OtherRoot=TextureRoot+TEXT("_other");
	auto Other=Texture;
	Other.RecipeRoot=FPaths::Combine(OtherRoot,TEXT("recipes"));
	Other.OutputContentRoot=FPaths::Combine(OtherRoot,TEXT("content/Patterns"));
	Other.ManifestRoot=FPaths::Combine(OtherRoot,TEXT("manifests"));
	const FString OtherRecipe=FPaths::Combine(Other.RecipeRoot,TEXT("Soil/project_soil_fixture.pattern.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OtherRecipe),true);
	if (!FFileHelper::LoadFileToString(Json,*TextureFile) || !FFileHelper::SaveStringToFile(Json,*OtherRecipe)) return false;
	const FString FirstPackage=FPackageName::LongPackageNameToFilename(
		FPackageName::ObjectPathToPackageName(Generated.OutputObjectPaths[0]),FPackageName::GetAssetPackageExtension());
	TArray<uint8> PackageBefore,ManifestBefore,BytesAfter;
	const FString FirstManifest=FPaths::Combine(Texture.ManifestRoot,TEXT("accepted.pattern-manifest.json"));
	if (!FFileHelper::LoadFileToArray(PackageBefore,*FirstPackage) || !FFileHelper::LoadFileToArray(ManifestBefore,*FirstManifest)) return false;
	FProjectTextureGenerationResult OriginResult;
	TestFalse(TEXT("Mismatched physical output mount refuses before writes"),FProjectTextureGenerationService::RegenerateTestMount(Other,OriginResult));
	{
		FPackageName::RegisterMountPoint(TEXT("/ProjectTextureTest/"),FPaths::Combine(OtherRoot,TEXT("content/")));
		ON_SCOPE_EXIT {FPackageName::UnRegisterMountPoint(TEXT("/ProjectTextureTest/"),FPaths::Combine(OtherRoot,TEXT("content/")));};
		TestFalse(TEXT("Second scratch root refuses already loaded first-root package"),FProjectTextureGenerationService::RegenerateTestMount(Other,OriginResult));
		TestEqual(TEXT("Origin refusal generates no output"),OriginResult.Generated,0);
		TestFalse(TEXT("Origin refusal promotes no second manifest"),FPaths::FileExists(FPaths::Combine(Other.ManifestRoot,TEXT("accepted.pattern-manifest.json"))));
		TestFalse(TEXT("Origin refusal writes no second package"),FPaths::DirectoryExists(Other.OutputContentRoot));
		TestTrue(TEXT("First-root package bytes retained"),FFileHelper::LoadFileToArray(BytesAfter,*FirstPackage) && BytesAfter==PackageBefore);
		TestTrue(TEXT("First-root manifest bytes retained"),FFileHelper::LoadFileToArray(BytesAfter,*FirstManifest) && BytesAfter==ManifestBefore);
		const FString OtherPackage=FPackageName::LongPackageNameToFilename(
			FPackageName::ObjectPathToPackageName(Generated.OutputObjectPaths[0]),FPackageName::GetAssetPackageExtension());
		const FString OtherManifest=FPaths::Combine(Other.ManifestRoot,TEXT("accepted.pattern-manifest.json"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OtherPackage),true);
		IFileManager::Get().MakeDirectory(*Other.ManifestRoot,true);
		if (!FFileHelper::SaveArrayToFile(PackageBefore,*OtherPackage) || !FFileHelper::SaveArrayToFile(ManifestBefore,*OtherManifest)) return false;
		TestFalse(TEXT("Copied accepted bytes cannot validate a foreign loaded package"),FProjectTextureGenerationService::Validate(Other,OriginResult));
		TestTrue(TEXT("Read-only origin refusal retains copied package bytes"),FFileHelper::LoadFileToArray(BytesAfter,*OtherPackage) && BytesAfter==PackageBefore);
		TestTrue(TEXT("Read-only origin refusal retains copied manifest bytes"),FFileHelper::LoadFileToArray(BytesAfter,*OtherManifest) && BytesAfter==ManifestBefore);
	}
	const FString PrimaryPath=Generated.OutputObjectPaths[0];
	auto* RT=LoadObject<UTextureRenderTarget2D>(nullptr,*PrimaryPath);
	auto* Slot=RT ? Cast<UProjectTextureOutputSlot>(RT->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass())) : nullptr;
	if (!TestNotNull(TEXT("Reloaded primary output"),Slot)) return false;
	auto* C=Slot->Catalog.Get();
	auto* Runtime=GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
	auto* World=GEditor->GetEditorWorldContext().World();
	const auto Before=Runtime->GetTelemetry();
	const int64 Budget=C->BudgetBytes;
	C->BudgetBytes=1;
	TestFalse(TEXT("Insufficient budget refuses before drawing"),Runtime->RequestOutput(World,Slot,Error));
	TestEqual(TEXT("Budget refusal generated nothing"),Runtime->GetTelemetry().Generations,Before.Generations);
	for (UProjectTextureOutputSlot* S : C->OutputSlots) TestFalse(TEXT("No partial output readiness"),Runtime->IsOutputReady(S));
	C->BudgetBytes=Budget;
	const auto Original=C->OutputSlots[2];
	C->OutputSlots[2]=C->OutputSlots[1];
	TestFalse(TEXT("Duplicate sibling slot refuses"),Runtime->RequestOutput(World,Slot,Error));
	TestEqual(TEXT("Malformed sibling generated nothing"),Runtime->GetTelemetry().Generations,Before.Generations);
	C->OutputSlots[2]=Original;
	if (!TestTrue(TEXT("Native runtime generates coherent soil bundle"),Runtime->RequestOutput(World,Slot,Error))) {AddError(Error); return false;}
	TestEqual(TEXT("Each of the three nodes generated once"),Runtime->GetTelemetry().Generations-Before.Generations,3);
	const int32 Generations=Runtime->GetTelemetry().Generations;
	for (UProjectTextureOutputSlot* S : C->OutputSlots)
	{
		TestTrue(TEXT("Each sibling is ready"),Runtime->IsOutputReady(S));
		TestTrue(TEXT("Each sibling warm request reuses pixels"),Runtime->RequestOutput(World,S,Error));
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags(RCM_MinMax); Flags.SetLinearToGamma(false);
		TestTrue(TEXT("Read actual generated map pixels"),S->GetRenderTarget()->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
		if (Pixels.Num()!=RT->SizeX*RT->SizeY) return false;
		float Min=1,Max=0;
		for (const auto& P : Pixels) {Min=FMath::Min(Min,P.R/255.f);Max=FMath::Max(Max,P.R/255.f);}
		TestTrue(TEXT("Generated map has actual structure"),Max-Min>.05f);
		if (S->SlotName==TEXT("Structure"))
		{
			// Detect the observed full-height/width stripes on actual linear byte output.
			double MaximumMeanStep=0;
			for (int32 Edge=1; Edge<RT->SizeX; ++Edge)
			{
				double X=0,Y=0;
				for (int32 I=0; I<RT->SizeX; ++I)
				{
					X+=FMath::Abs(int32(Pixels[I*RT->SizeX+Edge].R)-Pixels[I*RT->SizeX+Edge-1].R);
					Y+=FMath::Abs(int32(Pixels[Edge*RT->SizeX+I].R)-Pixels[(Edge-1)*RT->SizeX+I].R);
				}
				MaximumMeanStep=FMath::Max(MaximumMeanStep,FMath::Max(X,Y)/RT->SizeX);
			}
			AddInfo(FString::Printf(TEXT("Soil height maximum mean adjacent row/column step=%.6f / 255"),MaximumMeanStep));
			TestTrue(TEXT("No stripe discontinuity in generated height"),MaximumMeanStep<6.0);
		}
		if (S->SlotName==TEXT("Normal"))
		{
			for (int32 I=0; I<Pixels.Num(); I+=193)
			{
				const auto P=Pixels[I].ReinterpretAsLinear(); const FVector N(P.R*2-1,P.G*2-1,P.B*2-1);
				if (!TestTrue(TEXT("RGB8 decoded normal nearly unit and finite"),!N.ContainsNaN() && FMath::Abs(N.Size()-1)<.025)) return false;
			}
		}
	}
	TestEqual(TEXT("Warm requests never regenerate nodes"),Runtime->GetTelemetry().Generations,Generations);
	FProjectMaterialSurfaceGenerationRequest Material;
	const FString Layout=FPaths::Combine(FPaths::ProjectDir(),TEXT("Plugins/World/ProjectWorldMeshTerrain/Data/TestFixtures/mesh-terrain-layout-receipt.json"));
	if (!TestTrue(TEXT("Existing isolated material fixture admission"),FProjectMaterialSurfaceGenerationService::MakeTestRequest(MaterialRoot,Layout,Material,Error))) return false;
	Material.PatternPackageRoot=TEXT("/ProjectTextureTest");
	Material.PatternContentRoot=FPaths::Combine(TextureRoot,TEXT("content"));
	Material.PatternManifestPath=FPaths::Combine(Texture.ManifestRoot,TEXT("accepted.pattern-manifest.json"));
	for (const auto* Name : {TEXT("M_ProjectSoilFixture"),TEXT("MI_ProjectSoilFixture")})
	{
		const FString Source=FPaths::Combine(SourceMaterial,FString(Name)+TEXT(".surface.json"));
		const FString File=FPaths::Combine(Material.RecipeRoot,TEXT("Soil"),FString(Name)+TEXT(".surface.json"));
		if (!FFileHelper::LoadFileToString(Json,*Source)) return false;
		Json.ReplaceInline(TEXT("/ProjectMaterial/Surfaces/"),TEXT("/ProjectMaterialTest/Surfaces/"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
		if (!FFileHelper::SaveStringToFile(Json,*File)) return false;
	}
	FProjectMaterialSurfaceGenerationResult Result;
	if (!TestTrue(TEXT("Consumer compiler authenticates, generates, saves and reloads soil material"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Material,Result)))
	{
		AddError(Result.Error); return false;
	}
	TestEqual(TEXT("Parent and instance generated"),Result.Generated,2);
	FProjectMaterialSurfaceGenerationResult Repeat;
	TestTrue(TEXT("Unchanged sample material skips"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Material,Repeat));
	TestEqual(TEXT("No second material write"),Repeat.Generated,0);
	FProjectMaterialPatternDependency P;
	TestTrue(TEXT("Exact package bytes authenticated"),FProjectMaterialPatternAuthority::Resolve(TEXT("project_soil_fixture"),Material.PatternManifestPath,
		Material.PatternPackageRoot,Material.PatternContentRoot,P,Error));
	FProjectMaterialSurfaceRecipe Recipe;
	const FString ParentFile=FPaths::Combine(Material.RecipeRoot,TEXT("Soil/M_ProjectSoilFixture.surface.json"));
	FFileHelper::LoadFileToString(Json,*ParentFile);
	TestTrue(TEXT("Parse sample recipe"),FProjectMaterialSurfaceRecipeContract::Parse(Json,ParentFile,Material.RecipeRoot,Recipe,Error));
	UObject* Parent=LoadObject<UObject>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/Soil/M_ProjectSoilFixture.M_ProjectSoilFixture"));
	RT->GetOutermost()->SetDirtyFlag(true);
	TestFalse(TEXT("Dirty loaded source refuses consumer verification"),ProjectMaterialSoilBuilder::Verify(Recipe,P,Parent,Error));
	RT->GetOutermost()->SetDirtyFlag(false);
	TestTrue(TEXT("Original authenticated source verifies"),ProjectMaterialSoilBuilder::Verify(Recipe,P,Parent,Error));
	auto* ParentMaterial=CastChecked<UMaterial>(Parent);
	auto* Slab=CastChecked<UMaterialExpressionSubstrateSlabBSDF>(ParentMaterial->GetEditorOnlyData()->FrontMaterial.Expression);
	UMaterialExpressionTextureSample* NormalSample=nullptr;
	UMaterialExpressionTextureSample* MasksSample=nullptr;
	UMaterialExpressionComponentMask* RoughnessChannel=nullptr;
	for (auto* Expression : UMaterialEditingLibrary::GetMaterialExpressions(ParentMaterial))
	{
		if (auto* Sample=Cast<UMaterialExpressionTextureSample>(Expression))
		{
			if (Sample->Texture==C->OutputSlots[1]->GetRenderTarget()) NormalSample=Sample;
			if (Sample->Texture==C->OutputSlots[2]->GetRenderTarget()) MasksSample=Sample;
		}
	}
	if (!TestNotNull(TEXT("Normal route sample"),NormalSample) || !TestNotNull(TEXT("Masks route sample"),MasksSample)) return false;
	Swap(NormalSample->Texture,MasksSample->Texture);
	TestFalse(TEXT("Swapped normal and masks bindings refuse semantic verification"),ProjectMaterialSoilBuilder::Verify(Recipe,P,Parent,Error));
	Swap(NormalSample->Texture,MasksSample->Texture);
	for (auto* Expression : UMaterialEditingLibrary::GetMaterialExpressions(ParentMaterial))
		if (auto* Channel=Cast<UMaterialExpressionComponentMask>(Expression); Channel && Channel->G && Channel->Input.Expression==MasksSample) RoughnessChannel=Channel;
	if (!TestNotNull(TEXT("Roughness channel route"),RoughnessChannel)) return false;
	RoughnessChannel->R=true; RoughnessChannel->G=false;
	TestFalse(TEXT("Cavity routed as roughness refuses semantic verification"),ProjectMaterialSoilBuilder::Verify(Recipe,P,Parent,Error));
	RoughnessChannel->R=false; RoughnessChannel->G=true;
	UMaterialExpressionMultiply* NormalDecode=nullptr;
	for (auto* Expression : UMaterialEditingLibrary::GetMaterialExpressions(ParentMaterial))
		if (auto* Multiply=Cast<UMaterialExpressionMultiply>(Expression); Multiply && Multiply->A.Expression==NormalSample) NormalDecode=Multiply;
	if (!TestNotNull(TEXT("Normal RGB decode route"),NormalDecode)) return false;
	NormalDecode->A.OutputIndex=1;
	TestFalse(TEXT("Single texture channel cannot masquerade as normal RGB"),ProjectMaterialSoilBuilder::Verify(Recipe,P,Parent,Error));
	NormalDecode->A.OutputIndex=0;
	TestTrue(TEXT("Restored semantic routes verify"),ProjectMaterialSoilBuilder::Verify(Recipe,P,Parent,Error));
	for (FExpressionInput* Input : {&Slab->Normal,&Slab->DiffuseAlbedo,&Slab->Roughness,&Slab->F0})
	{
		const auto Saved=*Input;
		Input->Expression=nullptr;
		TestFalse(TEXT("Disconnected native PBR input refuses reload verification"),ProjectMaterialSoilBuilder::Verify(Recipe,P,Parent,Error));
		*Input=Saved;
	}
	TestTrue(TEXT("Restored native PBR connections verify"),ProjectMaterialSoilBuilder::Verify(Recipe,P,Parent,Error));
	// A resource replacement has the same object and semantic identity but empty pixels.
	// It must invalidate every sibling's warm hint rather than report a false cache hit.
	auto* Replaced=C->OutputSlots[1]->GetRenderTarget();
	Replaced->UpdateResourceImmediate(true);
	Replaced->UpdateResource();
	FlushRenderingCommands();
	for (UProjectTextureOutputSlot* S : C->OutputSlots)
		TestFalse(TEXT("Any recreated sibling invalidates the complete bundle readiness"),Runtime->IsOutputReady(S));
	const int32 BeforeReplacement=Runtime->GetTelemetry().Generations;
	TestTrue(TEXT("Resource replacement regenerates coherent bundle"),Runtime->RequestOutput(World,Slot,Error));
	TestEqual(TEXT("All three dependent maps regenerate after resource replacement"),Runtime->GetTelemetry().Generations-BeforeReplacement,3);
	const FString Receipt=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/world/surface_presentation/soil_native/fixture.txt"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Receipt),true);
	TestTrue(TEXT("Write disposable visual continuation receipt"),FFileHelper::SaveStringToFile(
		TextureRoot+TEXT("\n")+MaterialRoot+TEXT("\n")+PrimaryPath+TEXT("\n"),*Receipt));
	AddInfo(FString::Printf(TEXT("Soil sample prepared; native runtime generations=%d, resident_bytes=%lld, peak_bytes=%lld"),
		Generations,Runtime->GetTelemetry().ResidentBytes,Runtime->GetTelemetry().PeakResidentBytes));
	return true;
}
#endif
