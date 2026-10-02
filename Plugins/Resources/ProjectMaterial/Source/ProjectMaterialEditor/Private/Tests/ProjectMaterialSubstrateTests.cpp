// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MaterialEditingLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialSubstrateMinimalCompileTest,
	"Project.Material.Substrate.MinimalCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectMaterialSubstrateMinimalCompileTest::RunTest(const FString& Parameters)
{
	const IConsoleVariable* Substrate = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Substrate"));
	const IConsoleVariable* GBufferFormat =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.Substrate.ProjectGBufferFormat"));
	TestNotNull(TEXT("The Substrate project setting exists."), Substrate);
	TestNotNull(TEXT("The Substrate GBuffer project setting exists."), GBufferFormat);
	if (Substrate == nullptr || GBufferFormat == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Substrate is enabled for the project."), Substrate->GetInt(), 1);
	TestEqual(TEXT("Substrate uses the Blendable GBuffer."), GBufferFormat->GetInt(), 0);
	if (Substrate->GetInt() != 1 || GBufferFormat->GetInt() != 0)
	{
		return false;
	}

	TStrongObjectPtr<UMaterial> Material(
		NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
	UMaterialExpressionSubstrateSlabBSDF* Slab =
		Cast<UMaterialExpressionSubstrateSlabBSDF>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material.Get(), UMaterialExpressionSubstrateSlabBSDF::StaticClass(), -200, 0));
	TestNotNull(TEXT("The minimal Substrate slab expression is created."), Slab);
	if (Slab == nullptr)
	{
		return false;
	}

	UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
	TestNotNull(TEXT("The transient material has editor graph data."), EditorData);
	if (EditorData == nullptr)
	{
		return false;
	}
	EditorData->FrontMaterial.Connect(0, Slab);
	Material->BlendMode = BLEND_Opaque;
	Material->MaterialDomain = MD_Surface;
	Material->PostEditChange();

	const TArray<FString> CompileErrors = UMaterialEditingLibrary::RecompileMaterial(Material.Get());
	TestEqual(TEXT("The minimal Substrate slab compiles without errors."), CompileErrors.Num(), 0);
	TestTrue(
		TEXT("The compiled material retains its explicit Substrate front material."),
		Material->HasSubstrateFrontMaterialConnected());
	return CompileErrors.IsEmpty() && Material->HasSubstrateFrontMaterialConnected();
}

#endif
