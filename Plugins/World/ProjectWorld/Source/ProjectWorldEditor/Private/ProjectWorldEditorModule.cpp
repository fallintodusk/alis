// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldEvidenceCapture.h"

#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldEvidenceCommand, Log, All);

namespace
{
	// Evidence capture runs in the LIVE EDITOR, not in a commandlet.
	//
	// The commandlet envelope was tried and abandoned on measured evidence: with a real D3D12
	// SM6 RHI, a valid World->Scene, 378 loaded actors, 566 registered visible primitives, a
	// DirectionalLight, re-registered world components and an explicit
	// CommandletHelpers::TickEngine frame, USceneCaptureComponent2D::CaptureScene still wrote
	// nothing but the render target's clear colour. A commandlet is a deliberately raw host; the
	// editor runs the ordinary frame loop the renderer expects. Same capture code, different
	// host - this command is only a trigger.
	void CaptureEvidenceCommand(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() < 3)
		{
			UE_LOG(
				LogProjectWorldEvidenceCommand,
				Error,
				TEXT("[ProjectWorld.CaptureEvidence] Usage - ProjectWorld.CaptureEvidence <vantage-plan> <output-dir> <receipt>"));
			return;
		}
		UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World == nullptr)
		{
			UE_LOG(LogProjectWorldEvidenceCommand, Error, TEXT("[ProjectWorld.CaptureEvidence] No editor world."));
			return;
		}

		FProjectWorldCaptureResult Result;
		Result.MapPackage = World->GetPackage() != nullptr ? World->GetPackage()->GetName() : FString();
		FString Error;
		int32 Width = 0;
		int32 Height = 0;
		TArray<FProjectWorldCaptureVantage> Vantages;
		if (!ProjectWorldEvidenceCapture::LoadVantagePlan(
				Arguments[0], Width, Height, Vantages, Result.VantagePlanSha256, Error))
		{
			UE_LOG(LogProjectWorldEvidenceCommand, Error, TEXT("[ProjectWorld.CaptureEvidence] %s"), *Error);
			return;
		}

		// A World Partition editor world loads cells on demand, so an unattended editor may hold
		// none of them. Load the whole editor bounds explicitly: identical frames must mean a
		// defect, never an unloaded world.
		UWorldPartition* WorldPartition = World->GetWorldPartition();
		UWorldPartitionEditorLoaderAdapter* LoaderAdapter = nullptr;
		if (WorldPartition != nullptr)
		{
			FBox EditorBounds = WorldPartition->GetEditorWorldBounds();
			if (!EditorBounds.IsValid)
			{
				EditorBounds = WorldPartition->GetRuntimeWorldBounds();
			}
			if (EditorBounds.IsValid)
			{
				LoaderAdapter = WorldPartition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(
					World, EditorBounds, TEXT("ProjectWorld evidence capture"));
				LoaderAdapter->GetLoaderAdapter()->Load();
				World->UpdateWorldComponents(false, false);
				World->SendAllEndOfFrameUpdates();
			}
		}

		const bool bCaptured = ProjectWorldEvidenceCapture::CaptureVantages(
			World, Vantages, Width, Height, Arguments[1], Result, Error);
		if (!bCaptured)
		{
			Result.Status = TEXT("rejected");
			Result.Message = Error;
		}
		else if (!Result.bViewsPairwiseDistinct)
		{
			Result.Status = TEXT("rejected");
			Result.Message = TEXT("Two vantages produced identical bytes, which is the signature of a stale frame.");
		}
		else
		{
			Result.Status = TEXT("accepted");
			// The repeated-pose control is REPORTED, not gated. UE carries temporal rendering
			// state between frames - auto exposure alone reads the previous frame - so demanding
			// byte-identical repeats would fail on healthy imagery. The property that actually
			// matters is that distinct poses do not return the same frame, and that is gated.
			Result.Message = FString::Printf(
				TEXT("Captured %d operator views (repeat-pose control %s)."),
				Result.Views.Num(),
				Result.bControlMatches ? TEXT("byte-identical") : TEXT("differs; temporal state, not gated"));
		}

		if (LoaderAdapter != nullptr && WorldPartition != nullptr)
		{
			WorldPartition->ReleaseEditorLoaderAdapter(LoaderAdapter);
		}

		FString ReceiptError;
		if (!ProjectWorldEvidenceCapture::WriteReceipt(Result, Arguments[2], ReceiptError))
		{
			UE_LOG(LogProjectWorldEvidenceCommand, Error, TEXT("[ProjectWorld.CaptureEvidence] %s"), *ReceiptError);
			return;
		}
		// Nothing is ever saved: evidence production must not modify the territory it documents.
		UE_LOG(
			LogProjectWorldEvidenceCommand,
			Display,
			TEXT("[ProjectWorld.CaptureEvidence] status=%s views=%d receipt=%s"),
			*Result.Status,
			Result.Views.Num(),
			*Arguments[2]);
		// An unattended capture host exists only to produce this receipt. Quitting here rather
		// than through a trailing "; Quit" in -ExecCmds keeps the semicolon out of the console
		// argument list, which otherwise ends up appended to the receipt path.
		if (FApp::IsUnattended())
		{
			FPlatformMisc::RequestExit(false);
		}
	}

	FAutoConsoleCommand GCaptureEvidenceCommand(
		TEXT("ProjectWorld.CaptureEvidence"),
		TEXT("Render the planned operator vantages of the open generated world. Args: <vantage-plan> <output-dir> <receipt>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CaptureEvidenceCommand));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, ProjectWorldEditor)
