// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureRuntimeCatalog.h"
#include "ProjectTextureRuntimeSubsystem.h"

#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "RHITypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectTextureRuntimeGenerationTests
{
float RgbDelta(const FLinearColor& A, const FLinearColor& B)
{
	return (FMath::Abs(A.R - B.R) + FMath::Abs(A.G - B.G) +
		FMath::Abs(A.B - B.B)) / 3.0f;
}

float ChannelRange(const TArray<FLinearColor>& Pixels, int32 Channel)
{
	float Minimum = 1.0f;
	float Maximum = 0.0f;
	for (const FLinearColor& Pixel : Pixels)
	{
		const float Value = Pixel.Component(Channel);
		Minimum = FMath::Min(Minimum, Value);
		Maximum = FMath::Max(Maximum, Value);
	}
	return Maximum - Minimum;
}

float MeanChannelDifference(
	const TArray<FLinearColor>& Pixels,
	int32 LeftChannel,
	int32 RightChannel)
{
	if (Pixels.IsEmpty())
	{
		return 0.0f;
	}
	float Difference = 0.0f;
	for (const FLinearColor& Pixel : Pixels)
	{
		Difference += FMath::Abs(Pixel.Component(LeftChannel) - Pixel.Component(RightChannel));
	}
	return Difference / Pixels.Num();
}

bool ReadMip(UTextureRenderTarget2D* Target, int32 Mip, TArray<FLinearColor>& OutPixels)
{
	if (Target == nullptr || Target->GameThread_GetRenderTargetResource() == nullptr)
	{
		return false;
	}
	const int32 Width = FMath::Max(1, Target->SizeX >> Mip);
	const int32 Height = FMath::Max(1, Target->SizeY >> Mip);
	FReadSurfaceDataFlags Flags(RCM_MinMax, CubeFace_MAX);
	Flags.SetLinearToGamma(false);
	Flags.SetMip(static_cast<uint8>(Mip));
	return Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(
		OutPixels, Flags, FIntRect(0, 0, Width, Height));
}

bool IsStructuredAndSeamSafe(
	const TArray<FLinearColor>& Pixels,
	int32 Width,
	int32 Height,
	float& OutSeamDelta,
	float& OutInteriorDelta)
{
	if (Pixels.Num() != Width * Height || Width < 4 || Height < 4)
	{
		return false;
	}
	float Minimum = 1.0f;
	float Maximum = 0.0f;
	OutSeamDelta = 0.0f;
	OutInteriorDelta = 0.0f;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		OutSeamDelta += RgbDelta(Pixels[Y * Width], Pixels[Y * Width + Width - 1]);
	}
	for (int32 X = 0; X < Width; ++X)
	{
		OutSeamDelta += RgbDelta(Pixels[X], Pixels[(Height - 1) * Width + X]);
	}
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FLinearColor& Pixel = Pixels[Y * Width + X];
			const float PixelMinimum = FMath::Min(Pixel.R, FMath::Min(Pixel.G, Pixel.B));
			const float PixelMaximum = FMath::Max(Pixel.R, FMath::Max(Pixel.G, Pixel.B));
			Minimum = FMath::Min(Minimum, PixelMinimum);
			Maximum = FMath::Max(Maximum, PixelMaximum);
			if (X + 1 < Width)
			{
				OutInteriorDelta += RgbDelta(Pixel, Pixels[Y * Width + X + 1]);
			}
		}
	}
	OutSeamDelta /= Width + Height;
	OutInteriorDelta /= Height * (Width - 1);
	return Maximum - Minimum > 0.05f &&
		OutSeamDelta <= FMath::Max(0.02f, OutInteriorDelta * 4.0f);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTextureRuntimeTwoNodeGenerationTest,
	"Project.Texture.Runtime.TwoNodeGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureRuntimeTwoNodeGenerationTest::RunTest(const FString& Parameters)
{
	UTextureRenderTarget2D* Target = LoadObject<UTextureRenderTarget2D>(
		nullptr,
		TEXT("/ProjectTexture/Patterns/Terrain/DA_ProjectTerrainStructureCatalog.RT_ProjectTerrainStructure"));
	if (!TestNotNull(TEXT("The stable Structure output slot loads."), Target))
	{
		return false;
	}
	TestFalse(TEXT("The Structure output is not tagged as sRGB color data."), Target->SRGB);
	TestFalse(TEXT("The Structure render target does not create an sRGB resource."), Target->IsSRGB());
	UProjectTextureOutputSlot* Slot = Cast<UProjectTextureOutputSlot>(
		Target->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass()));
	if (!TestNotNull(TEXT("The output descriptor carries ProjectTexture slot metadata."), Slot))
	{
		return false;
	}
	const FProjectTextureRuntimeNodeDescriptor* ChildNode = Slot->Catalog != nullptr
		? Slot->Catalog->Nodes.FindByPredicate([](const FProjectTextureRuntimeNodeDescriptor& Node)
		{
			return !Node.ParentTextureParameter.IsNone();
		}) : nullptr;
	const UMaterial* ChildGenerator = ChildNode != nullptr
		? Cast<UMaterial>(ChildNode->GeneratorMaterial) : nullptr;
	if (!TestNotNull(TEXT("The cached child generator exists."), ChildGenerator))
	{
		return false;
	}
	int32 ParentSampleCount = 0;
	for (UMaterialExpression* Expression : UMaterialEditingLibrary::GetMaterialExpressions(
		const_cast<UMaterial*>(ChildGenerator)))
	{
		const UMaterialExpressionTextureSampleParameter2D* ParentSample =
			Cast<UMaterialExpressionTextureSampleParameter2D>(Expression);
		if (ParentSample != nullptr && ParentSample->ParameterName == ChildNode->ParentTextureParameter)
		{
			++ParentSampleCount;
			TestTrue(TEXT("DAG structural data uses the linear-color sampler."),
				ParentSample->SamplerType == SAMPLERTYPE_LinearColor);
			TestTrue(TEXT("The packaged DAG sample defaults to the stable Structure output."),
				ParentSample->Texture == Target);
		}
	}
	TestEqual(TEXT("The DAG child has one matching parent-data sample."), ParentSampleCount, 1);
	UProjectTextureRuntimeSubsystem* Runtime = GEngine != nullptr
		? GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>() : nullptr;
	UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("The rendering client owns a ProjectTexture runtime subsystem."), Runtime) ||
		!TestNotNull(TEXT("The editor provides a render-capable world context."), World))
	{
		return false;
	}

	FString PackageFilename;
	TestTrue(TEXT("The output package resolves to a stable file."),
		FPackageName::DoesPackageExist(Target->GetOutermost()->GetName(), &PackageFilename));
	TArray<uint8> BeforeBytes;
	TestTrue(TEXT("The stable output package is readable before runtime generation."),
		FFileHelper::LoadFileToArray(BeforeBytes, *PackageFilename));

	const FProjectTextureRuntimeTelemetry Before = Runtime->GetTelemetry();
	FString Error;
	TestTrue(TEXT("The two-node runtime DAG generates the stable Structure output."),
		Runtime->RequestOutput(World, Slot, Error));
	TestTrue(TEXT("The exported Structure slot becomes ready."), Runtime->IsOutputReady(Slot));
	const FProjectTextureRuntimeTelemetry Generated = Runtime->GetTelemetry();
	TestEqual(TEXT("The basis and its exported child each generate once."),
		Generated.Generations - Before.Generations, 2);
	TestEqual(TEXT("Only the pinned exported 512 RGBA8 mip chain remains resident."),
		Generated.ResidentBytes, int64(1398100));
	TestEqual(TEXT("The transient parent and exported child fit the declared peak."),
		Generated.PeakResidentBytes, int64(2796200));

	for (int32 Consumer = 0; Consumer < 100; ++Consumer)
	{
		TestTrue(TEXT("Equivalent consumers reuse the one ready output."),
			Runtime->RequestOutput(World, Slot, Error));
	}
	const FProjectTextureRuntimeTelemetry Reused = Runtime->GetTelemetry();
	TestEqual(TEXT("One hundred consumers add no generation."),
		Reused.Generations, Generated.Generations);
	TestEqual(TEXT("One hundred consumers hit the shared output."),
		Reused.CacheHits - Generated.CacheHits, 100);

	for (const int32 Mip : {0, 4})
	{
		TArray<FLinearColor> Pixels;
		TestTrue(FString::Printf(TEXT("Mip %d is readable after publication."), Mip),
			ProjectTextureRuntimeGenerationTests::ReadMip(Target, Mip, Pixels));
		float SeamDelta = 0.0f;
		float InteriorDelta = 0.0f;
		const int32 Size = FMath::Max(1, Target->SizeX >> Mip);
		TestTrue(FString::Printf(TEXT("Mip %d is structured and wrap-continuous."), Mip),
			ProjectTextureRuntimeGenerationTests::IsStructuredAndSeamSafe(
				Pixels, Size, Size, SeamDelta, InteriorDelta));
		for (int32 Channel = 0; Channel < 3; ++Channel)
		{
			TestTrue(
				FString::Printf(TEXT("Mip %d declared RGB signal %d is non-constant."),
					Mip, Channel),
				ProjectTextureRuntimeGenerationTests::ChannelRange(Pixels, Channel) > 0.01f);
		}
		TestTrue(FString::Printf(TEXT("Mip %d MacroVariation and CoverPatch remain distinct."), Mip),
			ProjectTextureRuntimeGenerationTests::MeanChannelDifference(Pixels, 0, 1) > 0.01f);
		TestTrue(FString::Printf(TEXT("Mip %d MacroVariation and GroundDetail remain distinct."), Mip),
			ProjectTextureRuntimeGenerationTests::MeanChannelDifference(Pixels, 0, 2) > 0.01f);
		TestTrue(FString::Printf(TEXT("Mip %d CoverPatch and GroundDetail remain distinct."), Mip),
			ProjectTextureRuntimeGenerationTests::MeanChannelDifference(Pixels, 1, 2) > 0.01f);
		AddInfo(FString::Printf(TEXT("Mip %d seam=%f interior=%f"),
			Mip, SeamDelta, InteriorDelta));
	}

	TArray<uint8> AfterBytes;
	TestTrue(TEXT("The stable output package remains readable after generation."),
		FFileHelper::LoadFileToArray(AfterBytes, *PackageFilename));
	TestTrue(TEXT("Runtime generation does not mutate output package bytes."),
		AfterBytes == BeforeBytes);
	return true;
}

#endif
