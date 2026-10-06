// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductPerformanceGate.h"
#include "Presentation/ProjectWorldPerformanceCollector.h"
#include "Presentation/ProjectWorldPerformanceMetrics.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProperties.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TSharedPtr<FJsonValue> VectorValue(const FVector& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Coordinates;
		Coordinates.Add(MakeShared<FJsonValueNumber>(Value.X));
		Coordinates.Add(MakeShared<FJsonValueNumber>(Value.Y));
		Coordinates.Add(MakeShared<FJsonValueNumber>(Value.Z));
		return MakeShared<FJsonValueArray>(Coordinates);
	}
}

void FProjectWorldProductPerformanceGate::WriteResult(
	const FString& Status,
	const FString& ErrorCode,
	const FString& ErrorMessage)
{
	const FProjectWorldPerformanceStatistics Overall = Collector.IsValid() ?
		ProjectWorldPerformanceMetrics::Calculate(Collector->AllFrames()) :
		FProjectWorldPerformanceStatistics();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"),
		TEXT("https://alis.world/schemas/world-performance/product-performance-result-v1.json"));
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("operation_id"), OperationId);
	Root->SetStringField(TEXT("status"), Status);
	Root->SetStringField(TEXT("map_package"), MapPackage);
	Root->SetStringField(TEXT("runtime_profile"), RuntimeProfileId);
	Root->SetStringField(TEXT("runtime_profile_sha256"), RuntimeProfileHash);
	Root->SetStringField(TEXT("machine_profile_id"), MachineProfileId);
	Root->SetStringField(TEXT("correctness_status"), CorrectnessStatus);
	Root->SetStringField(TEXT("correctness_receipt"), CorrectnessResultPath);
	Root->SetStringField(TEXT("executable"), CorrectnessExecutable);
	Root->SetStringField(TEXT("build_configuration"), LexToString(FApp::GetBuildConfiguration()));
	Root->SetBoolField(TEXT("requires_cooked_data"), FPlatformProperties::RequiresCookedData());
	Root->SetBoolField(TEXT("play_in_editor"), ProductWorld.IsValid() && ProductWorld->WorldType == EWorldType::PIE);
	Root->SetStringField(TEXT("engine_version"), CorrectnessEngineVersion);
	Root->SetStringField(TEXT("gpu_adapter"), CorrectnessGpuAdapter);
	Root->SetStringField(TEXT("gpu_driver"), CorrectnessGpuDriver);
	Root->SetStringField(TEXT("rhi"), CorrectnessRhi);
	Root->SetBoolField(TEXT("render_offscreen"),
		FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")));
	Root->SetStringField(TEXT("quality_preset"), TEXT("High"));
	Root->SetNumberField(TEXT("quality_level"), HighQualityLevel);
	Root->SetNumberField(TEXT("resolution_x"), CapturedResolution.X);
	Root->SetNumberField(TEXT("resolution_y"), CapturedResolution.Y);
	PerformanceEnvelope.AppendReceiptFields(*Root);
	Root->SetNumberField(TEXT("frame_p95_budget_ms"), FrameP95BudgetMilliseconds);
	Root->SetNumberField(TEXT("time_to_ready_seconds"), CaptureReadySeconds);
	Root->SetNumberField(TEXT("streaming_failures"), TotalStreamingFailures);
	Root->SetNumberField(TEXT("activation_transitions"), TotalActivationTransitions);
	Root->SetNumberField(TEXT("unloaded_cell_count"), PlayableTourResidency.GetUnloadedCount());
	Root->SetNumberField(TEXT("reloaded_cell_count"), PlayableTourResidency.GetReloadedCount());
	Root->SetNumberField(TEXT("peak_process_physical_bytes"), static_cast<double>(PeakProcessPhysicalBytes));
	Root->SetNumberField(TEXT("peak_gpu_local_bytes"), static_cast<double>(PeakGpuLocalBytes));
	Root->SetStringField(TEXT("csv_capture"), WrittenCsvPath);
	Root->SetStringField(TEXT("measurement_backend"),
		bNativeSteadyRequested ? TEXT("native_performance_consumer") : TEXT("csv_and_native_consumer"));
	Root->SetStringField(TEXT("raw_sample_capture"), RawSamplePath);
	Root->SetNumberField(TEXT("sample_count"), Overall.SampleCount);
	Root->SetNumberField(TEXT("frame_p50_ms"), Overall.FrameP50Milliseconds);
	Root->SetNumberField(TEXT("frame_p95_ms"), Overall.FrameP95Milliseconds);
	Root->SetNumberField(TEXT("frame_p99_ms"), Overall.FrameP99Milliseconds);
	Root->SetNumberField(TEXT("frame_max_ms"), Overall.FrameMaxMilliseconds);
	Root->SetNumberField(TEXT("game_p50_ms"), Overall.GameP50Milliseconds);
	Root->SetNumberField(TEXT("game_p95_ms"), Overall.GameP95Milliseconds);
	Root->SetNumberField(TEXT("game_p99_ms"), Overall.GameP99Milliseconds);
	Root->SetNumberField(TEXT("render_p50_ms"), Overall.RenderP50Milliseconds);
	Root->SetNumberField(TEXT("render_p95_ms"), Overall.RenderP95Milliseconds);
	Root->SetNumberField(TEXT("render_p99_ms"), Overall.RenderP99Milliseconds);
	Root->SetNumberField(TEXT("gpu_p50_ms"), Overall.GPUP50Milliseconds);
	Root->SetNumberField(TEXT("gpu_p95_ms"), Overall.GPUP95Milliseconds);
	Root->SetNumberField(TEXT("gpu_p99_ms"), Overall.GPUP99Milliseconds);
	Root->SetNumberField(TEXT("rhi_p50_ms"), Overall.RHIP50Milliseconds);
	Root->SetNumberField(TEXT("rhi_p95_ms"), Overall.RHIP95Milliseconds);
	Root->SetNumberField(TEXT("rhi_p99_ms"), Overall.RHIP99Milliseconds);
	Root->SetBoolField(TEXT("rhi_timing_available"), Overall.RHIP95Milliseconds > 0.0);
	Root->SetStringField(TEXT("acceptance_reason"), AcceptanceReason);
	Root->SetBoolField(TEXT("playable_tour"), bPlayableTourRequested);
	Root->SetStringField(TEXT("playable_tour_screenshot"), ScreenshotPath);
	Root->SetStringField(TEXT("correctness_screenshot"), CorrectnessScreenshotPath);
	Root->SetBoolField(TEXT("gameplay_interaction"), bCorrectnessGameplayInteraction);
	// Authenticate the policy, so an omitted interaction is a recorded decision rather than
	// something a reader has to infer from a false value.
	Root->SetBoolField(TEXT("gameplay_interaction_required"), bRequireGameplayInteraction);
	Root->SetBoolField(TEXT("terrain_collision"), bCorrectnessTerrainCollision);
	Root->SetBoolField(TEXT("road_collision"), bCorrectnessRoadCollision);
	Root->SetBoolField(TEXT("building_collision"), bCorrectnessBuildingCollision);
	if (PlayableTourDriver.IsValid())
	{
		Root->SetNumberField(TEXT("final_center_arrival_radius_cm"),
			PlayableTourDriver->GetFinalCenterArrivalRadiusCentimeters());
		PlayableTourDriver->AppendReceiptFields(*Root);
		PlayableTourResidency.AppendReceiptFields(*Root);
	}

	TArray<TSharedPtr<FJsonValue>> RouteValues;
	for (const FProjectWorldPerformanceRoute& Route : Routes)
	{
		const FProjectWorldPerformanceStatistics RouteStatistics = Collector.IsValid() ?
			ProjectWorldPerformanceMetrics::Calculate(Collector->FramesFor(Route.Name)) :
			FProjectWorldPerformanceStatistics();
		TSharedRef<FJsonObject> RouteObject = MakeShared<FJsonObject>();
		RouteObject->SetStringField(TEXT("route"), Route.Name);
		RouteObject->SetNumberField(TEXT("duration_seconds"), Route.DurationSeconds);
		RouteObject->SetNumberField(TEXT("ready_wait_seconds"), Route.ReadyWaitSeconds);
		RouteObject->SetNumberField(TEXT("sample_count"), RouteStatistics.SampleCount);
		RouteObject->SetNumberField(TEXT("frame_p95_ms"), RouteStatistics.FrameP95Milliseconds);
		RouteObject->SetNumberField(TEXT("frame_p99_ms"), RouteStatistics.FrameP99Milliseconds);
		RouteObject->SetNumberField(TEXT("frame_max_ms"), RouteStatistics.FrameMaxMilliseconds);
		RouteObject->SetNumberField(TEXT("game_p95_ms"), RouteStatistics.GameP95Milliseconds);
		RouteObject->SetNumberField(TEXT("render_p95_ms"), RouteStatistics.RenderP95Milliseconds);
		RouteObject->SetNumberField(TEXT("gpu_p95_ms"), RouteStatistics.GPUP95Milliseconds);
		RouteObject->SetNumberField(TEXT("peak_loaded_cells"), Route.PeakLoadedCells);
		RouteObject->SetNumberField(TEXT("peak_activated_cells"), Route.PeakActivatedCells);
		RouteObject->SetNumberField(TEXT("loaded_cell_seconds"), Route.LoadedCellSeconds);
		RouteObject->SetNumberField(TEXT("activated_cell_seconds"), Route.ActivatedCellSeconds);
		RouteObject->SetNumberField(TEXT("activation_transitions"), Route.ActivationTransitions);
		RouteObject->SetNumberField(TEXT("streaming_failures"), Route.StreamingFailures);
		TArray<TSharedPtr<FJsonValue>> PointValues;
		for (const FVector& Point : Route.Points)
		{
			PointValues.Add(VectorValue(Point));
		}
		RouteObject->SetArrayField(TEXT("points"), PointValues);
		RouteValues.Add(MakeShared<FJsonValueObject>(RouteObject));
	}
	Root->SetArrayField(TEXT("routes"), RouteValues);

	TArray<TSharedPtr<FJsonValue>> Errors;
	if (!ErrorCode.IsEmpty())
	{
		TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("code"), ErrorCode);
		Error->SetStringField(TEXT("message"), ErrorMessage);
		Errors.Add(MakeShared<FJsonValueObject>(Error));
	}
	Root->SetArrayField(TEXT("errors"), Errors);

	FString Payload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true);
	const FString Staging = ResultPath + TEXT(".tmp");
	if (FFileHelper::SaveStringToFile(
		Payload + TEXT("\n"),
		*Staging,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		IFileManager::Get().Move(*ResultPath, *Staging, true, true);
	}
}

