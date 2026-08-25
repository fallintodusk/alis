// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldEvidenceCapture.h"
#include "ProjectWorldEvidenceReadiness.h"

#include "AssetCompilingManager.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldEvidenceCommand, Log, All);

namespace
{
	constexpr double CaptureReadinessTimeoutSeconds = 180.0;

	struct FPendingEvidenceCapture
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<UWorldPartition> WorldPartition;
		UWorldPartitionEditorLoaderAdapter* LoaderAdapter = nullptr;
		TArray<FProjectWorldCaptureVantage> Vantages;
		FProjectWorldCaptureResult Result;
		FProjectWorldEvidenceReadiness Readiness;
		FString OutputDirectory;
		FString ReceiptPath;
		double RequestedSeconds = 0.0;
		int32 Width = 0;
		int32 Height = 0;
	};

	TUniquePtr<FPendingEvidenceCapture> GPendingCapture;

	void ReleaseLoader(FPendingEvidenceCapture& Pending)
	{
		if (Pending.LoaderAdapter != nullptr && Pending.WorldPartition.IsValid())
		{
			Pending.WorldPartition->ReleaseEditorLoaderAdapter(Pending.LoaderAdapter);
		}
		Pending.LoaderAdapter = nullptr;
	}

	void WriteResultAndExit(FPendingEvidenceCapture& Pending)
	{
		ReleaseLoader(Pending);
		FString ReceiptError;
		if (!ProjectWorldEvidenceCapture::WriteReceipt(Pending.Result, Pending.ReceiptPath, ReceiptError))
		{
			UE_LOG(LogProjectWorldEvidenceCommand, Error, TEXT("[ProjectWorld.CaptureEvidence] %s"), *ReceiptError);
		}
		else
		{
			UE_LOG(
				LogProjectWorldEvidenceCommand,
				Display,
				TEXT("[ProjectWorld.CaptureEvidence] status=%s views=%d receipt=%s"),
				*Pending.Result.Status,
				Pending.Result.Views.Num(),
				*Pending.ReceiptPath);
		}
		if (FApp::IsUnattended())
		{
			FPlatformMisc::RequestExit(false);
		}
	}

	bool TickPendingCapture(float DeltaSeconds)
	{
		if (!GPendingCapture.IsValid())
		{
			return false;
		}
		FPendingEvidenceCapture& Pending = *GPendingCapture;
		UWorld* World = Pending.World.Get();
		if (World == nullptr)
		{
			Pending.Result.Status = TEXT("rejected");
			Pending.Result.Message = TEXT("The editor world was released while evidence capture was settling.");
			WriteResultAndExit(Pending);
			GPendingCapture.Reset();
			return false;
		}

		const int32 RemainingCompilations = FAssetCompilingManager::Get().GetNumRemainingAssets();
		if (!Pending.Readiness.Advance(GFrameCounter, RemainingCompilations))
		{
			if (FPlatformTime::Seconds() - Pending.RequestedSeconds <= CaptureReadinessTimeoutSeconds)
			{
				return true;
			}
			Pending.Result.Status = TEXT("rejected");
			Pending.Result.Message = FString::Printf(
				TEXT("Rendering did not settle within %.0f seconds; remaining compilations=%d."),
				CaptureReadinessTimeoutSeconds,
				RemainingCompilations);
			WriteResultAndExit(Pending);
			GPendingCapture.Reset();
			return false;
		}

		FString Error;
		const bool bCaptured = ProjectWorldEvidenceCapture::CaptureVantages(
			World,
			Pending.Vantages,
			Pending.Width,
			Pending.Height,
			Pending.OutputDirectory,
			Pending.Result,
			Error);
		if (!bCaptured)
		{
			Pending.Result.Status = TEXT("rejected");
			Pending.Result.Message = Error;
		}
		else if (!Pending.Result.bViewsPairwiseDistinct)
		{
			Pending.Result.Status = TEXT("rejected");
			Pending.Result.Message = TEXT("Two vantages produced identical bytes, which is the signature of a stale frame.");
		}
		else
		{
			Pending.Result.Status = TEXT("accepted");
			Pending.Result.Message = FString::Printf(
				TEXT("Captured %d operator views (repeat-pose control %s)."),
				Pending.Result.Views.Num(),
				Pending.Result.bControlMatches ? TEXT("byte-identical") : TEXT("differs; temporal state, not gated"));
		}
		WriteResultAndExit(Pending);
		GPendingCapture.Reset();
		return false;
	}

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
		if (GPendingCapture.IsValid())
		{
			UE_LOG(LogProjectWorldEvidenceCommand, Error, TEXT("[ProjectWorld.CaptureEvidence] A capture is already pending."));
			return;
		}
		UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World == nullptr)
		{
			UE_LOG(LogProjectWorldEvidenceCommand, Error, TEXT("[ProjectWorld.CaptureEvidence] No editor world."));
			return;
		}

		TUniquePtr<FPendingEvidenceCapture> Pending = MakeUnique<FPendingEvidenceCapture>();
		Pending->World = World;
		Pending->Result.MapPackage = World->GetPackage() != nullptr ? World->GetPackage()->GetName() : FString();
		Pending->OutputDirectory = Arguments[1];
		Pending->ReceiptPath = Arguments[2];
		Pending->RequestedSeconds = FPlatformTime::Seconds();
		FString Error;
		if (!ProjectWorldEvidenceCapture::LoadVantagePlan(
				Arguments[0],
				Pending->Width,
				Pending->Height,
				Pending->Vantages,
				Pending->Result.VantagePlanSha256,
				Error))
		{
			UE_LOG(LogProjectWorldEvidenceCommand, Error, TEXT("[ProjectWorld.CaptureEvidence] %s"), *Error);
			return;
		}

		// A World Partition editor world loads cells on demand, so an unattended editor may hold
		// none of them. Load the whole editor bounds explicitly: identical frames must mean a
		// defect, never an unloaded world.
		UWorldPartition* WorldPartition = World->GetWorldPartition();
		Pending->WorldPartition = WorldPartition;
		if (WorldPartition != nullptr)
		{
			FBox EditorBounds = WorldPartition->GetEditorWorldBounds();
			if (!EditorBounds.IsValid)
			{
				EditorBounds = WorldPartition->GetRuntimeWorldBounds();
			}
			if (EditorBounds.IsValid)
			{
				Pending->LoaderAdapter = WorldPartition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(
					World, EditorBounds, TEXT("ProjectWorld evidence capture"));
				Pending->LoaderAdapter->GetLoaderAdapter()->Load();
				World->UpdateWorldComponents(false, false);
				World->SendAllEndOfFrameUpdates();
			}
		}
		GPendingCapture = MoveTemp(Pending);
		UE_LOG(
			LogProjectWorldEvidenceCommand,
			Display,
			TEXT("[ProjectWorld.CaptureEvidence] Loaded evidence bounds; waiting for three settled editor frames."));
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickPendingCapture));
	}

	FAutoConsoleCommand GCaptureEvidenceCommand(
		TEXT("ProjectWorld.CaptureEvidence"),
		TEXT("Render the planned operator vantages of the open generated world. Args: <vantage-plan> <output-dir> <receipt>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CaptureEvidenceCommand));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, ProjectWorldEditor)
