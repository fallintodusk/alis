// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Modules/ModuleManager.h"
#include "ProjectMaterialSurfacePreview.h"
#include "ProjectMaterialSurfaceGenerationService.h"
#include "ProjectMaterialFixtureDomain.h"
#include "Components/MeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMaterialPreview, Log, All);

namespace ProjectMaterialPreviewConsole
{
FProjectMaterialSurfacePreview Session;
FAutoConsoleCommand GenerateFixture(TEXT("project.material.fixture.generate"),
	TEXT("Generate an isolated graph fixture: <tmp/material/generation/run-root> <layout-receipt>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		FString Error;
		FProjectMaterialSurfaceGenerationRequest Request;
		FProjectMaterialSurfaceGenerationResult Result;
		if (Session.IsActive() || Args.Num() != 2 ||
			!FProjectMaterialSurfaceGenerationService::MakeTestRequest(Args[0], Args[1], Request, Error))
		{
			UE_LOG(LogProjectMaterialPreview, Warning, TEXT("REFUSED: fixture generation requires a confined root and no active preview. %s"), *Error);
			return;
		}
		const bool bAccepted = FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Request, Result);
		UE_LOG(LogProjectMaterialPreview, Display, TEXT("%s: generated=%d skipped=%d compiles=%d detail=%s"),
			bAccepted ? TEXT("FIXTURE_READY") : TEXT("REFUSED"), Result.Generated, Result.Skipped, Result.ShaderCompiles, *Result.Error);
	}));
FAutoConsoleCommand ApplyFixture(TEXT("project.material.preview.fixture"),
	TEXT("Transient terrain fixture: <candidate> <candidate-root> <fixture-root> <component> <slot> <layout>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		FString Error;
		int32 Slot = INDEX_NONE;
		if (Args.Num() != 6 || !LexTryParseString(Slot, *Args[4]))
		{
			UE_LOG(LogProjectMaterialPreview, Warning, TEXT("REFUSED: expected candidate, candidate root, fixture root, component, slot and layout."));
			return;
		}
		UMeshComponent* Component = FindObject<UMeshComponent>(nullptr, *Args[3]);
		const bool bApplied = Session.ApplyFixtureRecipeFile(Args[0], Args[1], Args[2], Args[5], Component, Slot, Error);
		UE_LOG(LogProjectMaterialPreview, Display, TEXT("%s: fixture=%s component=%s slot=%d detail=%s"),
			bApplied ? TEXT("APPLIED_FIXTURE") : TEXT("REFUSED"), *Args[2], *Args[3], Slot, *Error);
	}));
FAutoConsoleCommand Apply(TEXT("project.material.preview"),
	TEXT("Transient instance preview: <recipe> <recipe-root> <component-path> <slot> <layout-receipt>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		FString Error;
		int32 Slot = INDEX_NONE;
		if (Args.Num() != 5 || !LexTryParseString(Slot, *Args[3]))
		{
			UE_LOG(LogProjectMaterialPreview, Warning, TEXT("REFUSED: expected recipe, root, component, slot, layout receipt."));
			return;
		}
		UMeshComponent* Component = FindObject<UMeshComponent>(nullptr, *Args[2]);
		const bool bApplied = Session.ApplyRecipeFile(Args[0], Args[1], Args[4], Component, Slot, Error);
		UE_LOG(LogProjectMaterialPreview, Display, TEXT("%s: recipe=%s component=%s slot=%d detail=%s"),
			bApplied ? TEXT("APPLIED") : TEXT("REFUSED"), *Args[0], *Args[2], Slot, *Error);
	}));
FAutoConsoleCommand Revert(TEXT("project.material.preview.revert"), TEXT("Restore the original live binding."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		FString Error;
		const bool bRestored = Session.Revert(Error);
		UE_LOG(LogProjectMaterialPreview, Display, TEXT("%s: %s"),
			bRestored ? TEXT("RESTORED") : TEXT("REFUSED"), *Error);
	}));
}

class FProjectMaterialEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		PreExitHandle = FCoreDelegates::OnEnginePreExit.AddStatic(&ProjectMaterialFixtureDomain::Shutdown);
	}
	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnEnginePreExit.Remove(PreExitHandle);
		FString Error;
		if (!ProjectMaterialPreviewConsole::Session.Revert(Error))
		{
			UE_LOG(LogProjectMaterialPreview, Warning, TEXT("Preview shutdown recovery: %s"), *Error);
		}
		ProjectMaterialFixtureDomain::Shutdown();
	}
private:
	FDelegateHandle PreExitHandle;
};

IMPLEMENT_MODULE(FProjectMaterialEditorModule, ProjectMaterialEditor)
