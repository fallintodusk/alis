// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPerformanceEnvelope.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"

namespace
{
	FString DynamicResolutionStatusName(EDynamicResolutionStatus Status)
	{
		switch (Status)
		{
		case EDynamicResolutionStatus::Unsupported:
			return TEXT("unsupported");
		case EDynamicResolutionStatus::Disabled:
			return TEXT("disabled");
		case EDynamicResolutionStatus::Paused:
			return TEXT("paused");
		case EDynamicResolutionStatus::Enabled:
			return TEXT("enabled");
		case EDynamicResolutionStatus::DebugForceEnabled:
			return TEXT("debug_force_enabled");
		default:
			return TEXT("unknown");
		}
	}
}

bool FProjectWorldPerformanceEnvelope::EstablishAndValidate(UWorld& World, FString& OutError)
{
	if (GEngine == nullptr)
	{
		OutError = TEXT("The engine is unavailable while establishing the performance envelope.");
		return false;
	}

	// These changes are process-local acceptance settings and are never persisted to player settings.
	GEngine->Exec(&World, TEXT("r.VSync 0"));
	GEngine->Exec(&World, TEXT("t.MaxFPS 0"));
	GEngine->Exec(&World, TEXT("r.DynamicRes.OperationMode 0"));
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	GEngine->SetDynamicResolutionUserSetting(false);

	const IConsoleVariable* VSyncVariable =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	const IConsoleVariable* MaxFpsVariable =
		IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
	const IConsoleVariable* DynamicResolutionVariable =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.OperationMode"));
	if (VSyncVariable == nullptr || MaxFpsVariable == nullptr || DynamicResolutionVariable == nullptr)
	{
		OutError = TEXT("A required Unreal performance-envelope console variable is unavailable.");
		return false;
	}

	VSync = VSyncVariable->GetInt();
	MaxFps = MaxFpsVariable->GetFloat();
	bSmoothFrameRate = GEngine->bSmoothFrameRate;
	bUseFixedFrameRate = GEngine->bUseFixedFrameRate;
	FixedFrameRate = GEngine->FixedFrameRate;
	bUseFixedTimeStep = FApp::UseFixedTimeStep();
	FixedDeltaTimeSeconds = FApp::GetFixedDeltaTime();
	bBenchmarking = FApp::IsBenchmarking();
	DynamicResolutionOperationMode = DynamicResolutionVariable->GetInt();
	FDynamicResolutionStateInfos DynamicResolutionInfos;
	GEngine->GetDynamicResolutionCurrentStateInfos(DynamicResolutionInfos);
	DynamicResolutionStatus = DynamicResolutionStatusName(DynamicResolutionInfos.Status);
	bDynamicResolutionEnabled =
		DynamicResolutionInfos.Status == EDynamicResolutionStatus::Enabled ||
		DynamicResolutionInfos.Status == EDynamicResolutionStatus::DebugForceEnabled;
	return Validate(OutError);
}

bool FProjectWorldPerformanceEnvelope::Validate(FString& OutError) const
{
	if (VSync != 0 || !FMath::IsNearlyZero(MaxFps) || bSmoothFrameRate ||
		bUseFixedFrameRate || bUseFixedTimeStep || bBenchmarking ||
		DynamicResolutionOperationMode != 0 || bDynamicResolutionEnabled ||
		(DynamicResolutionStatus != TEXT("disabled") &&
		 DynamicResolutionStatus != TEXT("unsupported")))
	{
		OutError = FString::Printf(
			TEXT("Uncapped native-resolution performance envelope rejected: ")
			TEXT("vsync=%d max_fps=%.3f smooth=%s fixed_rate=%s fixed_step=%s ")
			TEXT("benchmark=%s dynamic_mode=%d dynamic_status=%s."),
			VSync,
			MaxFps,
			bSmoothFrameRate ? TEXT("true") : TEXT("false"),
			bUseFixedFrameRate ? TEXT("true") : TEXT("false"),
			bUseFixedTimeStep ? TEXT("true") : TEXT("false"),
			bBenchmarking ? TEXT("true") : TEXT("false"),
			DynamicResolutionOperationMode,
			*DynamicResolutionStatus);
		return false;
	}
	return true;
}

void FProjectWorldPerformanceEnvelope::AppendReceiptFields(FJsonObject& Root) const
{
	Root.SetNumberField(TEXT("vsync"), VSync);
	Root.SetNumberField(TEXT("max_fps"), MaxFps);
	Root.SetBoolField(TEXT("smooth_frame_rate"), bSmoothFrameRate);
	Root.SetBoolField(TEXT("use_fixed_frame_rate"), bUseFixedFrameRate);
	Root.SetNumberField(TEXT("fixed_frame_rate"), FixedFrameRate);
	Root.SetBoolField(TEXT("use_fixed_time_step"), bUseFixedTimeStep);
	Root.SetNumberField(TEXT("fixed_delta_time_seconds"), FixedDeltaTimeSeconds);
	Root.SetBoolField(TEXT("benchmarking"), bBenchmarking);
	Root.SetNumberField(TEXT("dynamic_resolution_operation_mode"), DynamicResolutionOperationMode);
	Root.SetStringField(TEXT("dynamic_resolution_status"), DynamicResolutionStatus);
	Root.SetBoolField(TEXT("dynamic_resolution_enabled"), bDynamicResolutionEnabled);
}
