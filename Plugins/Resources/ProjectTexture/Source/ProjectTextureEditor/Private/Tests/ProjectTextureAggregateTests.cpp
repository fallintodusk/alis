// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureAggregateBuilder.h"
#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "ProjectTextureRuntimeSubsystem.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureAggregateTest,
	"Project.Texture.Aggregate.AuthenticatedSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureAggregateTest::RunTest(const FString& Parameters)
{
	const FString Fixture=FPaths::Combine(FPaths::ProjectPluginsDir(),TEXT("Resources/ProjectTexture/Data/TestFixtures"));
	const FString Path=FPaths::Combine(Fixture,TEXT("Aggregate/project_asphalt_fixture.pattern.json"));
	FString Json,Error; FProjectTexturePatternRecipe Recipe;
	if (!TestTrue(TEXT("Read the tracked source contract"),FFileHelper::LoadFileToString(Json,*Path)) ||
		!TestTrue(TEXT("Parse the bounded aggregate recipe"),FProjectTexturePatternRecipeContract::Parse(Json,Path,Fixture,Recipe,Error)))
	{ AddError(Error); return false; }
	FProjectTexturePatternRecipe Rejected;
	TestTrue(TEXT("Implemented native seed variation is admitted"),FProjectTexturePatternRecipeContract::Parse(
		Json.Replace(TEXT("\"seed\": 0"),TEXT("\"seed\": 1")),Path,Fixture,Rejected,Error));
	TestNotEqual(TEXT("Seed variation changes structural identity"),Rejected.Nodes[0].StructuralIdentity,Recipe.Nodes[0].StructuralIdentity);
	const FString Package=TEXT("/Temp/ProjectAggregate_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UObject* Asset=nullptr;
	if (!TestTrue(TEXT("Build the native two-output bundle"),ProjectTextureAggregateBuilder::Build(Recipe,Package,Asset,Error)))
	{ AddError(Error); return false; }
	auto* Target=CastChecked<UTextureRenderTarget2D>(Asset);
	auto* Slot=CastChecked<UProjectTextureOutputSlot>(Target->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass()));
	TStrongObjectPtr<UProjectTextureRuntimeCatalog> Owner(Slot->Catalog);
	auto* Catalog=Owner.Get();
	TestTrue(TEXT("The generated catalog authenticates"),ProjectTextureAggregateBuilder::Verify(Recipe,Asset,Error));
	for (const auto& Expression : CastChecked<UMaterial>(Catalog->Nodes[0].GeneratorMaterial)->GetExpressionCollection().Expressions)
		if (auto* C=Cast<UMaterialExpressionConstant>(Expression.Get()); C && C->Desc==TEXT("Structure.GrainRadius"))
		{
			const float Original=C->R; C->R=.95f;
			TestFalse(TEXT("Connected native controls authenticate actual graph values"),ProjectTextureAggregateBuilder::Verify(Recipe,Asset,Error));
			C->R=Original;
		}
	FProjectTexturePatternRecipe Invalid=Recipe; Invalid.StructureControls.FindChecked(TEXT("WarpAmplitude"))=1;
	const FString RefusedPackage=Package+TEXT("_Refused"); UObject* Refused=nullptr;
	TestFalse(TEXT("Out-of-envelope structure refuses before allocation"),ProjectTextureAggregateBuilder::Build(Invalid,RefusedPackage,Refused,Error));
	TestNull(TEXT("Refusal creates no output asset"),Refused);
	TestNull(TEXT("Refusal creates no package"),FindPackage(nullptr,*RefusedPackage));
	const FName ParentParameter=Catalog->Nodes[1].ParentTextureParameter;
	Catalog->Nodes[1].ParentTextureParameter=TEXT("WrongHeightChannel");
	TestFalse(TEXT("A disconnected runtime parent contract is rejected"),ProjectTextureAggregateBuilder::Verify(Recipe,Asset,Error));
	Catalog->Nodes[1].ParentTextureParameter=ParentParameter;
	auto* Runtime=GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
	auto* World=GEditor->GetEditorWorldContext().World();
	const auto Before=Runtime->GetTelemetry();
	if (!TestTrue(TEXT("The existing runtime generates both admitted outputs"),Runtime->RequestOutput(World,Slot,Error)))
	{ AddError(Error); return false; }
	const auto Cold=Runtime->GetTelemetry();
	TestEqual(TEXT("Exactly two source nodes execute"),Cold.Generations-Before.Generations,2);
	for (int32 Index=0; Index<20; ++Index)
		TestTrue(TEXT("Either consumer reuses the same bundle"),Runtime->RequestOutput(World,Catalog->OutputSlots[Index%2],Error));
	const auto Warm=Runtime->GetTelemetry();
	TestEqual(TEXT("Warm consumers do not redraw"),Warm.Generations,Cold.Generations);
	TestEqual(TEXT("All twenty warm requests hit the same cache"),Warm.CacheHits-Cold.CacheHits,20);
	const FName NormalSlot=Catalog->OutputSlots[1]->SlotName;
	Catalog->OutputSlots[1]->SlotName=TEXT("Masks");
	TestFalse(TEXT("A soil ABI cannot masquerade as an aggregate output"),Runtime->RequestOutput(World,Slot,Error));
	TestEqual(TEXT("Rejected output identity cannot draw"),Runtime->GetTelemetry().Generations,Warm.Generations);
	Catalog->OutputSlots[1]->SlotName=NormalSlot;
	FlushRenderingCommands();
	FString Output=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/texture/verification/particle_bundle"));
	FString RequestedOutput;
	if (FParse::Value(FCommandLine::Get(),TEXT("ProjectTextureParticleEvidenceRoot="),RequestedOutput))
	{
		if (FPaths::IsRelative(RequestedOutput)) RequestedOutput=FPaths::Combine(FPaths::ProjectDir(),RequestedOutput);
		Output=FPaths::ConvertRelativePathToFull(RequestedOutput);
		if (!FPaths::IsUnderDirectory(Output,FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp"))))
		{ AddError(TEXT("Particle evidence root must remain under project tmp.")); return false; }
	}
	IFileManager::Get().MakeDirectory(*Output,true);
	TArray<TArray<FColor>> Reference;
	for (int32 Index=0; Index<2; ++Index)
	{
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_MinMax); Flags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Read actual generated linear pixels"),Catalog->OutputSlots[Index]->GetRenderTarget()->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))) return false;
		TestEqual(TEXT("The frozen source resolution is 512 square"),Pixels.Num(),512*512);
		TArray<uint8> RGB; RGB.Reserve(Pixels.Num()*3);
		for (const FColor& P : Pixels)
		{
			RGB.Append({P.R,P.G,P.B});
			if (Index==1)
			{
				const FVector N(P.R/127.5-1,P.G/127.5-1,P.B/127.5-1);
				if (!TestTrue(TEXT("Generated normals stay finite and unit length"),!N.ContainsNaN() && FMath::Abs(N.Size()-1)<.025)) return false;
			}
		}
		TestTrue(TEXT("Publish diagnostic pixels for independent exact-source comparison"),FFileHelper::SaveArrayToFile(RGB,*FPaths::Combine(Output,Index ? TEXT("Asphalt-Normal.rgb") : TEXT("Asphalt-Signals.rgb"))));
		Reference.Add(MoveTemp(Pixels));
	}
	TSharedPtr<FJsonObject> VariantObject;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),VariantObject);
	VariantObject->SetNumberField(TEXT("seed"),17);
	VariantObject->GetObjectField(TEXT("structure"))->SetNumberField(TEXT("CoarseMix"),.44);
	FString VariantJson; FJsonSerializer::Serialize(VariantObject.ToSharedRef(),TJsonWriterFactory<>::Create(&VariantJson));
	FProjectTexturePatternRecipe Variant;
	if (!FProjectTexturePatternRecipeContract::Parse(VariantJson,Path,Fixture,Variant,Error)) return false;
	UObject* VariantAsset=nullptr;
	if (!ProjectTextureAggregateBuilder::Build(Variant,Package+TEXT("_Variant"),VariantAsset,Error)) return false;
	auto* VariantSlot=CastChecked<UProjectTextureOutputSlot>(CastChecked<UTextureRenderTarget2D>(VariantAsset)->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass()));
	TStrongObjectPtr<UProjectTextureRuntimeCatalog> VariantOwner(VariantSlot->Catalog);
	if (!TestTrue(TEXT("Nondefault structure uses the existing runtime owner"),Runtime->RequestOutput(World,VariantSlot,Error))) return false;
	for (int32 I=0; I<2; ++I)
	{
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_MinMax); Flags.SetLinearToGamma(false);
		if (!VariantOwner->OutputSlots[I]->GetRenderTarget()->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)) return false;
		int32 Changed=0; TArray<uint8> RGB;
		for (int32 P=0; P<Pixels.Num(); ++P)
		{
			RGB.Append({Pixels[P].R,Pixels[P].G,Pixels[P].B});
			if (Pixels[P]!=Reference[I][P]) ++Changed;
			if (I==1)
			{
				const FVector N(Pixels[P].R/127.5-1,Pixels[P].G/127.5-1,Pixels[P].B/127.5-1);
				if (!TestTrue(TEXT("Varied structure retains coherent finite unit normals"),!N.ContainsNaN() && FMath::Abs(N.Size()-1)<.025)) return false;
			}
		}
		TestTrue(TEXT("JSON-only structure changes produce different maps"),Changed>Pixels.Num()/10);
		TestTrue(TEXT("Record the nondefault map evidence"),FFileHelper::SaveArrayToFile(RGB,*FPaths::Combine(Output,I ? TEXT("Variant-Normal.rgb") : TEXT("Variant-Signals.rgb"))));
	}
	return true;
}
#endif
