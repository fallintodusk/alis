// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTexturePatternBuilder.h"
#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureRuntimeCatalog.h"

#include "AssetCompilingManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectTextureOutputContractClosedTest,
	"Project.Texture.Generation.OutputContractClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectTextureOutputContractClosedTest::RunTest(const FString& Parameters)
{
	const FString RecipeRoot = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Resources/ProjectTexture/Data/Patterns"));
	const FString RecipePath = FPaths::Combine(RecipeRoot, TEXT("Terrain/project_terrain_structure.pattern.json"));
	FString Json;
	FString Error;
	FProjectTexturePatternRecipe Recipe;
	if (!TestTrue(TEXT("The production pattern recipe is readable."), FFileHelper::LoadFileToString(Json, *RecipePath)) ||
		!TestTrue(TEXT("The production pattern recipe parses."),
			FProjectTexturePatternRecipeContract::Parse(Json, RecipePath, RecipeRoot, Recipe, Error)))
	{
		return false;
	}
	FString PackageName;
	FString ObjectPath;
	UObject* Asset = nullptr;
	if (!TestTrue(TEXT("The output identity resolves under a transient root."),
			FProjectTexturePatternRecipeContract::ResolveOutputIdentity(
				Recipe, TEXT("/Temp/ProjectTextureOutputContract"), PackageName, ObjectPath, Error)) ||
		!TestTrue(TEXT("The pattern builds in memory."),
			ProjectTexturePatternBuilder::Build(Recipe, PackageName, Asset, Error)))
	{
		return false;
	}
	UTextureRenderTarget2D* Target = Cast<UTextureRenderTarget2D>(Asset);
	UProjectTextureOutputSlot* Slot = Target != nullptr
		? Cast<UProjectTextureOutputSlot>(Target->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass()))
		: nullptr;
	UProjectTextureRuntimeCatalog* Catalog = Slot != nullptr ? Slot->Catalog : nullptr;
	if (!TestNotNull(TEXT("The built output carries its contract slot and catalog."), Catalog))
	{
		return false;
	}
	TestTrue(TEXT("The built output satisfies surface_structure_rgb_v1."),
		ProjectTexturePatternBuilder::Verify(Recipe, Asset, Error));

	// Each consumer-visible render-target condition Verify enforces is broken once, so a producer
	// cannot change it while keeping the same output_contract. Channel meaning is not checkable here.
	auto Rejects = [&](const TCHAR* Property, TFunctionRef<void()> Break, TFunctionRef<void()> Restore)
	{
		Break();
		FString BreakError;
		TestFalse(FString::Printf(TEXT("Verify rejects a changed %s under the same contract."), Property),
			ProjectTexturePatternBuilder::Verify(Recipe, Asset, BreakError));
		Restore();
	};
	Rejects(TEXT("color space"), [&] { Target->SRGB = true; }, [&] { Target->SRGB = false; });
	Rejects(TEXT("format"), [&] { Target->RenderTargetFormat = RTF_RGBA16f; },
		[&] { Target->RenderTargetFormat = RTF_RGBA8; });
	Rejects(TEXT("U addressing"), [&] { Target->AddressX = TA_Clamp; }, [&] { Target->AddressX = TA_Wrap; });
	Rejects(TEXT("V addressing"), [&] { Target->AddressY = TA_Clamp; }, [&] { Target->AddressY = TA_Wrap; });
	Rejects(TEXT("U mip addressing"), [&] { Target->MipsAddressU = TA_Clamp; },
		[&] { Target->MipsAddressU = TA_Wrap; });
	Rejects(TEXT("V mip addressing"), [&] { Target->MipsAddressV = TA_Clamp; },
		[&] { Target->MipsAddressV = TA_Wrap; });
	Rejects(TEXT("mip generation"), [&] { Target->bAutoGenerateMips = false; },
		[&] { Target->bAutoGenerateMips = true; });
	Rejects(TEXT("filter"), [&] { Target->Filter = TF_Nearest; }, [&] { Target->Filter = TF_Trilinear; });
	Rejects(TEXT("mip filter"), [&] { Target->MipsSamplerFilter = TF_Nearest; },
		[&] { Target->MipsSamplerFilter = TF_Trilinear; });
	Rejects(TEXT("slot"), [&] { Slot->SlotName = TEXT("Other"); }, [&] { Slot->SlotName = TEXT("Structure"); });
	Rejects(TEXT("contract name"), [&] { Catalog->OutputContract = TEXT("surface_structure_rgb_v2"); },
		[&] { Catalog->OutputContract = FName(Recipe.OutputContract); });
	TestTrue(TEXT("The restored output satisfies the contract again."),
		ProjectTexturePatternBuilder::Verify(Recipe, Asset, Error));

	FAssetCompilingManager::Get().FinishAllCompilation();
	if (UPackage* Package = FindPackage(nullptr, *PackageName))
	{
		ForEachObjectWithPackage(Package, [](UObject* Object)
		{
			Object->ClearFlags(RF_Public | RF_Standalone);
			Object->MarkAsGarbage();
			return true;
		});
		Package->MarkAsGarbage();
	}
	return true;
}

#endif
