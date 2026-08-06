// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPresentationGate.h"

#include "Presentation/ProjectWorldPresentationSampling.h"

#include "Algo/AllOf.h"
#include "Camera/CameraActor.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "Scalability.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldPresentationGate, Log, All);

namespace
{
	constexpr double StartupTimeoutSeconds = 120.0;
	constexpr double ScreenshotTimeoutSeconds = 30.0;
	const FString PresentationRolePrefix(TEXT("ProjectWorld.PresentationRole="));
	const FString PresentationProfilePrefix(TEXT("ProjectWorld.Presentation="));
	const FString PresentationHashPrefix(TEXT("ProjectWorld.PresentationHash="));

	bool ParseValue(const TCHAR* Name, FString& OutValue, bool bShouldStopOnSeparator = true)
	{
		return FParse::Value(FCommandLine::Get(), Name, OutValue, bShouldStopOnSeparator) &&
			!OutValue.IsEmpty();
	}

	template <typename TValue>
	bool ParseNumber(const TCHAR* Name, TValue& OutValue)
	{
		FString Text;
		return ParseValue(Name, Text) && LexTryParseString(OutValue, *Text);
	}

	bool IsToken(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	bool IsSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character))
			{
				return false;
			}
		}
		return true;
	}

	bool HasTagValue(const AActor& Actor, const FString& Prefix, const FString& Expected)
	{
		return Actor.Tags.Contains(FName(*(Prefix + Expected)));
	}

	FString TagValue(const AActor& Actor, const FString& Prefix)
	{
		for (const FName& Tag : Actor.Tags)
		{
			const FString Value = Tag.ToString();
			if (Value.StartsWith(Prefix))
			{
				return Value.RightChop(Prefix.Len());
			}
		}
		return FString();
	}

	TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FString& Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}
}

FProjectWorldPresentationGate::~FProjectWorldPresentationGate()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
}

void FProjectWorldPresentationGate::StartIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldPresentationGate")))
	{
		return;
	}

	FString Error;
	if (!ParseConfig(Error))
	{
		FinishRejected(TEXT("presentation_gate_config_invalid"), Error);
		return;
	}

	Phase = EPhase::WaitingForWorld;
	PhaseStartedSeconds = FPlatformTime::Seconds();
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FProjectWorldPresentationGate::Tick));
	UE_LOG(LogProjectWorldPresentationGate, Display,
		TEXT("[FProjectWorldPresentationGate::StartIfRequested] Started - operation=%s map=%s"),
		*Config.OperationId,
		*Config.MapPackage);
}

bool FProjectWorldPresentationGate::ParseConfig(FString& OutError)
{
	FString CameraRoles;
	if (!ParseValue(TEXT("ProjectWorldGateOperation="), Config.OperationId) ||
		!ParseValue(TEXT("ProjectWorldGateResult="), Config.ResultPath) ||
		!ParseValue(TEXT("ProjectWorldGateMap="), Config.MapPackage) ||
		!ParseValue(TEXT("ProjectWorldGateMachine="), Config.MachineProfileId) ||
		!ParseValue(TEXT("ProjectWorldGatePresentation="), Config.PresentationProfileId) ||
		!ParseValue(TEXT("ProjectWorldGatePresentationHash="), Config.PresentationProfileHash) ||
		!ParseValue(TEXT("ProjectWorldGateRuntime="), Config.RuntimeProfileId) ||
		!ParseValue(TEXT("ProjectWorldGateRuntimeHash="), Config.RuntimeProfileHash) ||
		!ParseValue(TEXT("ProjectWorldGateCameras="), CameraRoles, false) ||
		!ParseNumber(TEXT("ProjectWorldGateResX="), Config.ResolutionX) ||
		!ParseNumber(TEXT("ProjectWorldGateResY="), Config.ResolutionY) ||
		!ParseNumber(TEXT("ProjectWorldGateScalability="), Config.ScalabilityLevel) ||
		!ParseNumber(TEXT("ProjectWorldGateWarmup="), Config.WarmupFrames) ||
		!ParseNumber(TEXT("ProjectWorldGateSamples="), Config.SampleFrames) ||
		!ParseNumber(TEXT("ProjectWorldGateBudgetMs="), Config.FrameTimeBudgetMs))
	{
		OutError = TEXT("A required presentation-gate argument is missing.");
		return false;
	}

	CameraRoles.ParseIntoArray(Config.CameraRoles, TEXT(","), true);
	TSet<FString> UniqueRoles;
	UniqueRoles.Append(Config.CameraRoles);
	const bool bTokensValid = IsToken(Config.OperationId) && IsToken(Config.MachineProfileId) &&
		IsToken(Config.PresentationProfileId) && IsToken(Config.RuntimeProfileId) &&
		Algo::AllOf(Config.CameraRoles, [](const FString& Role) { return IsToken(Role); });
	if (!bTokensValid || !IsSha256(Config.PresentationProfileHash) || !IsSha256(Config.RuntimeProfileHash) ||
		Config.CameraRoles.IsEmpty() || UniqueRoles.Num() != Config.CameraRoles.Num())
	{
		OutError = TEXT("Presentation-gate identities are invalid or ambiguous.");
		return false;
	}
	if (FPaths::IsRelative(Config.ResultPath) ||
		!Config.MapPackage.StartsWith(TEXT("/ProjectWorld/Generated/Representative/")))
	{
		OutError = TEXT("Presentation-gate result or map scope is invalid.");
		return false;
	}
	if (Config.ResolutionX < 640 || Config.ResolutionY < 360 ||
		Config.ScalabilityLevel < 0 || Config.ScalabilityLevel > 4 ||
		Config.WarmupFrames < 1 || Config.SampleFrames < 30 || Config.FrameTimeBudgetMs <= 0.0)
	{
		OutError = TEXT("Presentation-gate measurement settings are outside supported bounds.");
		return false;
	}
	FPaths::NormalizeFilename(Config.ResultPath);
	return true;
}

bool FProjectWorldPresentationGate::Tick(float DeltaSeconds)
{
	if (Phase == EPhase::Finished)
	{
		return false;
	}
	if (Phase == EPhase::WaitingForWorld)
	{
		if (TryPrepareWorld())
		{
			BeginViewpoint();
		}
		else if (FPlatformTime::Seconds() - PhaseStartedSeconds > StartupTimeoutSeconds)
		{
			const FString Detail = LastReadinessError.IsEmpty()
				? TEXT("The packaged map, cameras, or viewport did not become ready.")
				: LastReadinessError;
			FinishRejected(TEXT("presentation_gate_world_timeout"), Detail);
		}
		return Phase != EPhase::Finished;
	}
	if (Phase == EPhase::Warmup || Phase == EPhase::Sampling)
	{
		// Ownership is inspected on every measured tick, not only at the
		// warmup boundary: World Partition may stream a runtime actor in at
		// any frame, and stale or duplicated role ownership must reject the
		// run even when it first appears mid-sampling. The scan cost lands
		// inside the measured window on purpose - it biases p95 upward, the
		// fail-safe direction.
		APlayerController* Controller = PlayerController.Get();
		if (Controller == nullptr || Controller->GetWorld() == nullptr)
		{
			FinishRejected(TEXT("presentation_gate_camera_lost"),
				TEXT("The player controller became unavailable during measurement."));
			return false;
		}
		FString Error;
		if (!InspectRuntimeRoles(*Controller->GetWorld(), Error))
		{
			FinishRejected(TEXT("presentation_gate_runtime_route_invalid"), Error);
			return false;
		}
	}
	if (Phase == EPhase::Warmup)
	{
		if (--RemainingWarmupFrames <= 0)
		{
			const TArray<FString> Missing =
				ProjectWorldPresentation::MissingRequiredRoles(ObservedRuntimeRoles);
			if (CurrentViewpoint + 1 >= Cameras.Num() && !Missing.IsEmpty())
			{
				if (FPlatformTime::Seconds() - PhaseStartedSeconds <= 30.0)
				{
					RemainingWarmupFrames = 1;
					return true;
				}
				FinishRejected(TEXT("presentation_gate_runtime_route_timeout"),
					FString::Printf(
						TEXT("Required runtime roles were never observed across the viewpoint sequence; missing: %s."),
						*FString::Join(Missing, TEXT(","))));
				return false;
			}
			FrameTimesMs.Reset(Config.SampleFrames);
			Phase = EPhase::Sampling;
		}
		return true;
	}
	if (Phase == EPhase::Sampling)
	{
		// Every rendered frame in the fixed window is evidence: stalls of any
		// magnitude are appended, and a timing value that is not a real frame
		// duration rejects the run instead of silently extending the window,
		// so sample_count always means frames observed.
		const double FrameTimeMs = static_cast<double>(FApp::GetDeltaTime()) * 1000.0;
		if (!ProjectWorldPresentation::IsValidSampleFrameMs(FrameTimeMs))
		{
			FinishRejected(TEXT("presentation_gate_frame_time_invalid"),
				FString::Printf(
					TEXT("Camera '%s' frame %d reported invalid frame time %f ms."),
					*Config.CameraRoles[CurrentViewpoint],
					FrameTimesMs.Num() + 1,
					FrameTimeMs));
			return false;
		}
		FrameTimesMs.Add(FrameTimeMs);
		if (FrameTimesMs.Num() >= Config.SampleFrames)
		{
			RequestViewpointScreenshot();
		}
		return true;
	}
	if (Phase == EPhase::WaitingForScreenshot)
	{
		const FString& Screenshot = ViewpointEvidence.Last().ScreenshotPath;
		if (IFileManager::Get().FileSize(*Screenshot) > 0)
		{
			CompleteViewpoint();
		}
		else if (FPlatformTime::Seconds() - PhaseStartedSeconds > ScreenshotTimeoutSeconds)
		{
			FinishRejected(TEXT("presentation_gate_screenshot_timeout"),
				TEXT("A fixed-camera screenshot was not written before the timeout."));
		}
	}
	return Phase != EPhase::Finished;
}

bool FProjectWorldPresentationGate::TryPrepareWorld()
{
    if (GDynamicRHI == nullptr || RHIGetInterfaceType() == ERHIInterfaceType::Null)
    {
        LastReadinessError = TEXT("The packaged process has no rendered RHI.");
        return false;
    }
    if (GEngine == nullptr || GEngine->GameViewport == nullptr || GEngine->GameViewport->Viewport == nullptr)
    {
        LastReadinessError = TEXT("The packaged game viewport is unavailable.");
        return false;
    }
    UWorld* World = GEngine->GameViewport->GetWorld();
    if (World == nullptr)
    {
        LastReadinessError = TEXT("The packaged game viewport has no world.");
        return false;
    }
	const FString LoadedMapPackage = World->GetPackage()->GetName();
	if (LoadedMapPackage != Config.MapPackage)
	{
		if (!bMapTravelRequested)
		{
			bMapTravelRequested = true;
			LastReadinessError = FString::Printf(
				TEXT("Travelling from packaged startup map '%s' to requested map '%s'."),
				*LoadedMapPackage,
				*Config.MapPackage);
			UGameplayStatics::OpenLevel(World, FName(*Config.MapPackage), true);
		}
		else
		{
			LastReadinessError = FString::Printf(
				TEXT("Requested map '%s' but the packaged viewport remained on '%s' after travel."),
				*Config.MapPackage,
				*LoadedMapPackage);
		}
		return false;
	}
	FString Error;
	if (!ValidateWorldIdentity(*World, Error) || !FindOwnedCameras(*World, Error))
	{
		LastReadinessError = Error;
		return false;
	}
    APlayerController* Controller = World->GetFirstPlayerController();
    if (Controller == nullptr)
    {
        LastReadinessError = TEXT("The packaged world has no player controller.");
        return false;
    }
	PlayerController = Controller;
	Scalability::FQualityLevels Quality;
	Quality.SetFromSingleQualityLevel(Config.ScalabilityLevel);
	Scalability::SetQualityLevels(Quality, true);
	GEngine->Exec(World, TEXT("r.VSync 0"));
	GEngine->Exec(World, TEXT("t.MaxFPS 0"));
	const FIntPoint ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
	if (ViewportSize.X != Config.ResolutionX || ViewportSize.Y != Config.ResolutionY)
	{
		FinishRejected(TEXT("presentation_gate_resolution_mismatch"),
			FString::Printf(TEXT("Expected %dx%d but rendered %dx%d."),
				Config.ResolutionX,
				Config.ResolutionY,
				ViewportSize.X,
				ViewportSize.Y));
		return false;
	}
	return true;
}

bool FProjectWorldPresentationGate::ValidateWorldIdentity(UWorld& World, FString& OutError) const
{
	if (World.GetPackage()->GetName() != Config.MapPackage)
	{
		OutError = TEXT("The loaded packaged world does not match the requested map.");
		return false;
	}
	return true;
}

bool FProjectWorldPresentationGate::FindOwnedCameras(UWorld& World, FString& OutError)
{
	TMap<FString, ACameraActor*> ByRole;
	for (TActorIterator<ACameraActor> It(&World); It; ++It)
	{
		const FString Role = TagValue(**It, PresentationRolePrefix);
		if (!Role.StartsWith(TEXT("Capture_")))
		{
			continue;
		}
		if (!HasTagValue(**It, PresentationProfilePrefix, Config.PresentationProfileId) ||
			!HasTagValue(**It, PresentationHashPrefix, Config.PresentationProfileHash) || ByRole.Contains(Role))
		{
			OutError = TEXT("A generated capture camera has stale or ambiguous ownership.");
			return false;
		}
		ByRole.Add(Role, *It);
	}
	Cameras.Reset(Config.CameraRoles.Num());
	for (const FString& CameraRole : Config.CameraRoles)
	{
		ACameraActor* const* Camera = ByRole.Find(TEXT("Capture_") + CameraRole);
		if (Camera == nullptr)
		{
			OutError = TEXT("A required generated capture camera is absent.");
			return false;
		}
		Cameras.Add(*Camera);
	}
	return true;
}

bool FProjectWorldPresentationGate::InspectRuntimeRoles(UWorld& World, FString& OutError)
{
	const ProjectWorldPresentation::FRuntimeRoleScan Scan = ProjectWorldPresentation::ScanRuntimeRoles(
		World,
		Config.RuntimeProfileId,
		Config.RuntimeProfileHash);
	if (!Scan.bValid)
	{
		OutError = Scan.Error;
		return false;
	}
	ObservedRuntimeRoles.Append(Scan.LoadedRoles);
	return true;
}

void FProjectWorldPresentationGate::BeginViewpoint()
{
	ACameraActor* Camera = Cameras[CurrentViewpoint].Get();
	APlayerController* Controller = PlayerController.Get();
	if (Camera == nullptr || Controller == nullptr)
	{
		FinishRejected(TEXT("presentation_gate_camera_lost"),
			TEXT("The fixed capture camera or player controller became unavailable."));
		return;
	}
	Controller->SetViewTarget(Camera);
	RemainingWarmupFrames = Config.WarmupFrames;
	PhaseStartedSeconds = FPlatformTime::Seconds();
	Phase = EPhase::Warmup;
}

void FProjectWorldPresentationGate::RequestViewpointScreenshot()
{
	const FString CaptureRoot = FPaths::Combine(FPaths::GetPath(Config.ResultPath), TEXT("captures"));
	IFileManager::Get().MakeDirectory(*CaptureRoot, true);
	const FString Screenshot = FPaths::Combine(CaptureRoot, Config.CameraRoles[CurrentViewpoint] + TEXT(".png"));
	IFileManager::Get().Delete(*Screenshot, false, true, true);
	FProjectWorldPresentationViewpointEvidence Evidence;
	Evidence.CameraRole = Config.CameraRoles[CurrentViewpoint];
	Evidence.ScreenshotPath = Screenshot;
	Evidence.SampleCount = FrameTimesMs.Num();
	Evidence.P95FrameTimeMs = P95(FrameTimesMs);
	ViewpointEvidence.Add(MoveTemp(Evidence));
	FScreenshotRequest::RequestScreenshot(Screenshot, false, false, false, FIntRect(), true);
	PhaseStartedSeconds = FPlatformTime::Seconds();
	Phase = EPhase::WaitingForScreenshot;
}

void FProjectWorldPresentationGate::CompleteViewpoint()
{
	++CurrentViewpoint;
	if (CurrentViewpoint >= Cameras.Num())
	{
		FinishAccepted();
		return;
	}
	BeginViewpoint();
}

void FProjectWorldPresentationGate::FinishAccepted()
{
	double WorstP95 = 0.0;
	for (const FProjectWorldPresentationViewpointEvidence& Evidence : ViewpointEvidence)
	{
		WorstP95 = FMath::Max(WorstP95, Evidence.P95FrameTimeMs);
	}
	if (WorstP95 > Config.FrameTimeBudgetMs)
	{
		FinishRejected(TEXT("presentation_gate_frame_budget_exceeded"),
			FString::Printf(TEXT("Worst fixed-camera p95 %.3f ms exceeds %.3f ms."),
				WorstP95,
				Config.FrameTimeBudgetMs));
		return;
	}
	WriteResult(TEXT("accepted"), FString(), FString());
	Phase = EPhase::Finished;
	FPlatformMisc::RequestExitWithStatus(false, 0, TEXT("ProjectWorldPresentationGate.Accepted"));
}

void FProjectWorldPresentationGate::FinishRejected(const FString& Code, const FString& Message)
{
	WriteResult(TEXT("rejected"), Code, Message);
	Phase = EPhase::Finished;
	UE_LOG(LogProjectWorldPresentationGate, Error,
		TEXT("[FProjectWorldPresentationGate::FinishRejected] Rejected - code=%s message=%s"),
		*Code,
		*Message);
	FPlatformMisc::RequestExitWithStatus(false, 9, TEXT("ProjectWorldPresentationGate.Rejected"));
}

void FProjectWorldPresentationGate::WriteResult(
	const FString& Status,
	const FString& ErrorCode,
	const FString& ErrorMessage)
{
	if (Config.ResultPath.IsEmpty())
	{
		return;
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"),
		TEXT("https://alis.world/schemas/world-presentation/presentation-result-v1.json"));
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("operation_id"), Config.OperationId);
	Root->SetStringField(TEXT("status"), Status);
	Root->SetStringField(TEXT("map_package"), Config.MapPackage);
	Root->SetStringField(TEXT("presentation_profile"), Config.PresentationProfileId);
	Root->SetStringField(TEXT("presentation_profile_sha256"), Config.PresentationProfileHash);
	Root->SetStringField(TEXT("runtime_profile"), Config.RuntimeProfileId);
	Root->SetStringField(TEXT("runtime_profile_sha256"), Config.RuntimeProfileHash);
	Root->SetStringField(TEXT("executable"), FPlatformProcess::ExecutablePath());
	Root->SetStringField(TEXT("build_configuration"), LexToString(FApp::GetBuildConfiguration()));
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("machine_profile_id"), Config.MachineProfileId);
	Root->SetStringField(TEXT("gpu_adapter"), GRHIAdapterName);
	const FGPUDriverInfo Driver = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
	Root->SetStringField(TEXT("gpu_driver"), Driver.UserDriverVersion);
	Root->SetStringField(TEXT("rhi"), GDynamicRHI == nullptr ? TEXT("unavailable") : GDynamicRHI->GetName());
	Root->SetNumberField(TEXT("resolution_x"), Config.ResolutionX);
	Root->SetNumberField(TEXT("resolution_y"), Config.ResolutionY);
	Root->SetNumberField(TEXT("scalability_level"), Config.ScalabilityLevel);
	Root->SetNumberField(TEXT("warmup_frames"), Config.WarmupFrames);
	Root->SetNumberField(TEXT("sample_frames_per_camera"), Config.SampleFrames);
	Root->SetNumberField(TEXT("p95_frame_time_budget_ms"), Config.FrameTimeBudgetMs);
	double WorstP95 = 0.0;
	TArray<TSharedPtr<FJsonValue>> Viewpoints;
	for (const FProjectWorldPresentationViewpointEvidence& Evidence : ViewpointEvidence)
	{
		TSharedRef<FJsonObject> Viewpoint = MakeShared<FJsonObject>();
		Viewpoint->SetStringField(TEXT("camera_role"), Evidence.CameraRole);
		Viewpoint->SetStringField(TEXT("screenshot"), Evidence.ScreenshotPath);
		Viewpoint->SetNumberField(TEXT("sample_count"), Evidence.SampleCount);
		Viewpoint->SetNumberField(TEXT("p95_frame_time_ms"), Evidence.P95FrameTimeMs);
		Viewpoints.Add(MakeShared<FJsonValueObject>(Viewpoint));
		WorstP95 = FMath::Max(WorstP95, Evidence.P95FrameTimeMs);
	}
	Root->SetArrayField(TEXT("viewpoints"), Viewpoints);
	Root->SetNumberField(TEXT("worst_p95_frame_time_ms"), WorstP95);
	Root->SetArrayField(TEXT("requested_camera_roles"), Strings(Config.CameraRoles));
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
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Config.ResultPath), true);
	const FString Staging = Config.ResultPath + TEXT(".tmp");
	if (FFileHelper::SaveStringToFile(Payload + TEXT("\n"), *Staging, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		IFileManager::Get().Move(*Config.ResultPath, *Staging, true, true);
	}
}

double FProjectWorldPresentationGate::P95(const TArray<double>& Values) const
{
	if (Values.IsEmpty())
	{
		return 0.0;
	}
	TArray<double> Sorted = Values;
	Sorted.Sort();
	const int32 Index = FMath::Clamp(FMath::CeilToInt(0.95 * Sorted.Num()) - 1, 0, Sorted.Num() - 1);
	return Sorted[Index];
}
