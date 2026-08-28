// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductPerformanceGate.h"

#include "Presentation/ProjectWorldPerformanceMetrics.h"
#include "Presentation/ProjectWorldPlayableTourDriver.h"
#include "Presentation/ProjectWorldPlayableTourResidency.h"
#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"
#include "Presentation/ProjectWorldScreenshotValidation.h"

#include "ChartCreation.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RHI.h"
#include "RHIStats.h"
#include "Scalability.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldProductPerformanceGate, Log, All);

namespace
{
	constexpr double ProductPerformanceGateTimeoutSeconds = 600.0;
	constexpr double CorrectnessTimeoutSeconds = 300.0;
	constexpr double WarmupTimeoutSeconds = 60.0;
	constexpr double SettlementTimeoutSeconds = 45.0;
	constexpr double CsvWriteTimeoutSeconds = 60.0;
	constexpr double PerformanceScreenshotTimeoutSeconds = 30.0;
	constexpr double ResidencySampleIntervalSeconds = 0.5;
	constexpr double MemorySampleIntervalSeconds = 1.0;
	constexpr double FrameP95BudgetMilliseconds = 16.67;
	constexpr int32 RequiredReadyFrames = 5;
	constexpr int32 RequiredHighQualityLevel = 2;
	const FIntPoint RequiredResolution(2560, 1440);

	bool ParseRequiredPath(const TCHAR* Name, FString& OutPath)
	{
		if (!FParse::Value(FCommandLine::Get(), Name, OutPath) || OutPath.IsEmpty() || FPaths::IsRelative(OutPath))
		{
			return false;
		}
		FPaths::NormalizeFilename(OutPath);
		return true;
	}

	bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, FString& OutValue)
	{
		return Object.IsValid() && Object->TryGetStringField(Name, OutValue) && !OutValue.IsEmpty();
	}

	TSharedPtr<FJsonValue> VectorValue(const FVector& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Coordinates;
		Coordinates.Add(MakeShared<FJsonValueNumber>(Value.X));
		Coordinates.Add(MakeShared<FJsonValueNumber>(Value.Y));
		Coordinates.Add(MakeShared<FJsonValueNumber>(Value.Z));
		return MakeShared<FJsonValueArray>(Coordinates);
	}
}

class FProjectWorldPerformanceCollector final : public IPerformanceDataConsumer
{
public:
	virtual void StartCharting() override
	{
	}

	virtual void ProcessFrame(const FFrameData& FrameData) override
	{
		if (ActiveRoute.IsEmpty() || FrameData.TrueDeltaSeconds <= 0.0)
		{
			return;
		}
		FProjectWorldPerformanceFrame& Frame = FramesByRoute.FindOrAdd(ActiveRoute).AddDefaulted_GetRef();
		Frame.FrameMilliseconds = FrameData.TrueDeltaSeconds * 1000.0;
		Frame.GameMilliseconds = FrameData.GameThreadTimeSeconds * 1000.0;
		Frame.RenderMilliseconds = FrameData.RenderThreadTimeSeconds * 1000.0;
		Frame.GPUMilliseconds = FrameData.GPUTimeSeconds * 1000.0;
	}

	virtual void StopCharting() override
	{
		ActiveRoute.Reset();
	}

	void BeginRoute(const FString& RouteName)
	{
		ActiveRoute = RouteName;
		FramesByRoute.FindOrAdd(RouteName);
	}

	void EndRoute()
	{
		ActiveRoute.Reset();
	}

	const TArray<FProjectWorldPerformanceFrame>& FramesFor(const FString& RouteName) const
	{
		const TArray<FProjectWorldPerformanceFrame>* Frames = FramesByRoute.Find(RouteName);
		return Frames == nullptr ? EmptyFrames : *Frames;
	}

	TArray<FProjectWorldPerformanceFrame> AllFrames() const
	{
		TArray<FProjectWorldPerformanceFrame> Result;
		for (const TPair<FString, TArray<FProjectWorldPerformanceFrame>>& Pair : FramesByRoute)
		{
			Result.Append(Pair.Value);
		}
		return Result;
	}

private:
	FString ActiveRoute;
	TMap<FString, TArray<FProjectWorldPerformanceFrame>> FramesByRoute;
	TArray<FProjectWorldPerformanceFrame> EmptyFrames;
};

FProjectWorldProductPerformanceGate::~FProjectWorldProductPerformanceGate()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	if (Collector.IsValid() && GEngine != nullptr && CollectorRegistration.Consume())
	{
		GEngine->RemovePerformanceDataConsumer(Collector);
	}
	Collector.Reset();
}

void FProjectWorldProductPerformanceGate::StartIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldProductPerformanceGate")))
	{
		return;
	}

	FString Error;
	if (!ParseConfig(Error))
	{
		Finish(TEXT("rejected"), TEXT("performance_config_invalid"), Error);
		return;
	}
	GateStartedSeconds = FPlatformTime::Seconds();
	SetPhase(EPhase::WaitingForCorrectness);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FProjectWorldProductPerformanceGate::Tick));
	UE_LOG(LogProjectWorldProductPerformanceGate, Display,
		TEXT("[FProjectWorldProductPerformanceGate::StartIfRequested] Started - result=%s"),
		*ResultPath);
}

bool FProjectWorldProductPerformanceGate::ParseConfig(FString& OutError)
{
	bPlayableTourRequested = FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldPlayableTour"));
	if (!ParseRequiredPath(TEXT("ProjectWorldPerformanceResult="), ResultPath) ||
		!ParseRequiredPath(TEXT("ProjectWorldPerformanceCorrectness="), CorrectnessResultPath) ||
		!ParseRequiredPath(TEXT("ProjectWorldPerformanceCsv="), RequestedCsvPath))
	{
		OutError = TEXT("Absolute result, correctness, and CSV paths are required.");
		return false;
	}
	if (ResultPath == CorrectnessResultPath || ResultPath == RequestedCsvPath)
	{
		OutError = TEXT("Performance result, correctness result, and CSV paths must be distinct.");
		return false;
	}
	if (bPlayableTourRequested &&
		!ParseRequiredPath(TEXT("ProjectWorldPerformanceScreenshot="), ScreenshotPath))
	{
		OutError = TEXT("Playable-tour acceptance requires an absolute screenshot path.");
		return false;
	}
	return true;
}

bool FProjectWorldProductPerformanceGate::Tick(float DeltaSeconds)
{
	if (Phase == EPhase::Finished)
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - GateStartedSeconds > ProductPerformanceGateTimeoutSeconds)
	{
		Finish(TEXT("rejected"), TEXT("performance_gate_timeout"),
			TEXT("The bounded packaged performance gate timed out."));
		return Phase != EPhase::Finished;
	}

	if (Phase == EPhase::WaitingForCorrectness)
	{
		FString Error;
		if (TryReadCorrectnessResult(Error))
		{
			if (!TryAcquireProductWorld(Error) || !ConfigureCapture(Error))
			{
				Finish(TEXT("rejected"), TEXT("performance_start_invalid"), Error);
			}
		}
		else if (!Error.IsEmpty())
		{
			Finish(TEXT("rejected"), TEXT("performance_correctness_invalid"), Error);
		}
		else if (Now - PhaseStartedSeconds > CorrectnessTimeoutSeconds)
		{
			Finish(TEXT("rejected"), TEXT("performance_correctness_timeout"),
				TEXT("The accepted product-route receipt did not arrive."));
		}
		return Phase != EPhase::Finished;
	}

	if (Phase == EPhase::WaitingForCsv)
	{
		if (CsvWriteFuture.IsValid() && CsvWriteFuture.IsReady())
		{
			WrittenCsvPath = CsvWriteFuture.Get();
			if (WrittenCsvPath.IsEmpty() || IFileManager::Get().FileSize(*WrittenCsvPath) <= 0)
			{
				PendingStatus = TEXT("rejected");
				PendingErrorCode = TEXT("performance_csv_missing");
				PendingErrorMessage = TEXT("Native UE CSV capture completed without a readable file.");
			}
			WriteResult(PendingStatus, PendingErrorCode, PendingErrorMessage);
			SetPhase(EPhase::Finished);
			const int32 ExitStatus = PendingStatus == TEXT("accepted") ? 0 : 10;
			FPlatformMisc::RequestExitWithStatus(false, ExitStatus,
				TEXT("ProjectWorldProductPerformanceGate.Finished"));
		}
		else if (Now - PhaseStartedSeconds > CsvWriteTimeoutSeconds)
		{
			PendingStatus = TEXT("rejected");
			PendingErrorCode = TEXT("performance_csv_timeout");
			PendingErrorMessage = TEXT("Native UE CSV output did not finish within the bounded timeout.");
			WriteResult(PendingStatus, PendingErrorCode, PendingErrorMessage);
			SetPhase(EPhase::Finished);
			FPlatformMisc::RequestExitWithStatus(false, 10,
				TEXT("ProjectWorldProductPerformanceGate.CsvTimeout"));
		}
		return Phase != EPhase::Finished;
	}
	if (Phase == EPhase::WaitingForScreenshot)
	{
		TickScreenshot();
		return Phase != EPhase::Finished;
	}

	if (!ProductWorld.IsValid() || !PlayerCharacter.IsValid())
	{
		Finish(TEXT("rejected"), TEXT("performance_product_ownership_lost"),
			TEXT("The accepted product world or possessed production character became unavailable."));
		return Phase != EPhase::Finished;
	}

	SampleResidency(DeltaSeconds);
	if (Phase == EPhase::Warmup)
	{
		TickWarmup();
	}
	else if (Phase == EPhase::Traversing)
	{
		TickRoute(DeltaSeconds);
	}
	else if (Phase == EPhase::Settling)
	{
		TickSettlement();
	}
	return Phase != EPhase::Finished;
}

bool FProjectWorldProductPerformanceGate::TryReadCorrectnessResult(FString& OutError)
{
	if (!FPaths::FileExists(CorrectnessResultPath))
	{
		return false;
	}
	FString Payload;
	if (!FFileHelper::LoadFileToString(Payload, *CorrectnessResultPath))
	{
		OutError = TEXT("The product-route receipt could not be read.");
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
		!ReadString(Root, TEXT("status"), CorrectnessStatus))
	{
		OutError = TEXT("The product-route receipt is malformed.");
		return false;
	}
	if (CorrectnessStatus != TEXT("accepted"))
	{
		OutError = TEXT("Packaged performance cannot start from a rejected product route.");
		return false;
	}
	const bool bComplete = ReadString(Root, TEXT("operation_id"), OperationId) &&
		ReadString(Root, TEXT("runtime_profile"), RuntimeProfileId) &&
		ReadString(Root, TEXT("runtime_profile_sha256"), RuntimeProfileHash) &&
		ReadString(Root, TEXT("machine_profile_id"), MachineProfileId) &&
		ReadString(Root, TEXT("map_package"), MapPackage) &&
		ReadString(Root, TEXT("executable"), CorrectnessExecutable) &&
		ReadString(Root, TEXT("engine_version"), CorrectnessEngineVersion) &&
		ReadString(Root, TEXT("gpu_adapter"), CorrectnessGpuAdapter) &&
		ReadString(Root, TEXT("gpu_driver"), CorrectnessGpuDriver) &&
		ReadString(Root, TEXT("rhi"), CorrectnessRhi);
	if (!bComplete)
	{
		OutError = TEXT("The accepted product-route receipt lacks required identity fields.");
		return false;
	}
	if (bPlayableTourRequested)
	{
		Root->TryGetStringField(TEXT("correctness_contract"), CorrectnessContract);
		const bool bScenarioContract = CorrectnessContract == TEXT("single-play-scenario-v1");
		const bool bCorrectnessComplete =
			Root->TryGetBoolField(TEXT("gameplay_interaction"), bCorrectnessGameplayInteraction) &&
			Root->TryGetBoolField(TEXT("terrain_collision"), bCorrectnessTerrainCollision) &&
			Root->TryGetBoolField(TEXT("road_collision"), bCorrectnessRoadCollision) &&
			Root->TryGetBoolField(TEXT("building_collision"), bCorrectnessBuildingCollision) &&
			ReadString(Root, TEXT("screenshot"), CorrectnessScreenshotPath);
		if (!bCorrectnessComplete || !bCorrectnessGameplayInteraction ||
			(!bScenarioContract && (!bCorrectnessTerrainCollision || !bCorrectnessRoadCollision ||
				!bCorrectnessBuildingCollision)))
		{
			OutError = TEXT("Playable tour requires accepted interaction and actor-scoped collision evidence.");
			return false;
		}
	}
	return true;
}

bool FProjectWorldProductPerformanceGate::TryAcquireProductWorld(FString& OutError)
{
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		OutError = TEXT("The normal game viewport is unavailable after product-route acceptance.");
		return false;
	}
	UWorld* World = GEngine->GameViewport->GetWorld();
	APlayerController* Controller = World == nullptr ? nullptr : World->GetFirstPlayerController();
	ACharacter* Character = Controller == nullptr ? nullptr : Cast<ACharacter>(Controller->GetPawn());
	if (World == nullptr || Controller == nullptr || Character == nullptr ||
		World->GetPackage()->GetName() != MapPackage ||
		FCString::Strcmp(World->URL.GetOption(TEXT("ProjectLoadingRoute="), TEXT("")), TEXT("1")) != 0)
	{
		OutError = TEXT("The live world no longer matches the accepted menu-to-ProjectLoading product route.");
		return false;
	}
	if (bPlayableTourRequested)
	{
		const FString Traversal = World->URL.GetOption(TEXT("Traversal="), TEXT(""));
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Traversal != TEXT("PreviewFlight") || Movement == nullptr || !Movement->IsFlying() ||
			!Character->GetClass()->GetPathName().Contains(TEXT("DefinitionCharacter")))
		{
			OutError = TEXT("Playable tour did not acquire PreviewFlight on the possessed DefinitionCharacter.");
			return false;
		}
	}
	ProductWorld = World;
	PlayerController = Controller;
	PlayerCharacter = Character;
	CenterLocation = Character->GetActorLocation();
	return true;
}

bool FProjectWorldProductPerformanceGate::ConfigureCapture(FString& OutError)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")))
	{
		OutError = TEXT("Automated 1440p capture requires UE RenderOffScreen to avoid monitor-size clamping.");
		return false;
	}
	FSystemResolution::RequestResolutionChange(
		RequiredResolution.X,
		RequiredResolution.Y,
		EWindowMode::Windowed);
	GEngine->Exec(ProductWorld.Get(), TEXT("r.VSync 0"));
	GEngine->Exec(ProductWorld.Get(), TEXT("t.MaxFPS 0"));
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	Scalability::FQualityLevels HighQuality;
	HighQuality.SetFromSingleQualityLevel(RequiredHighQualityLevel);
	// This gate owns a one-shot process; late teardown must never reconfigure a shut-down renderer.
	Scalability::SetQualityLevels(HighQuality, true);
	HighQualityLevel = Scalability::GetQualityLevels().GetSingleQualityLevel();
	CapturedResolution = GEngine->GameViewport->Viewport->GetSizeXY();
	if (HighQualityLevel != RequiredHighQualityLevel || CapturedResolution != RequiredResolution)
	{
		OutError = FString::Printf(
			TEXT("Primary capture requires High 2560x1440; observed quality=%d resolution=%dx%d."),
			HighQualityLevel,
			CapturedResolution.X,
			CapturedResolution.Y);
		return false;
	}
	if (!BuildRoutes(OutError))
	{
		return false;
	}
	if (bPlayableTourRequested)
	{
		PlayableTourDriver = MakeUnique<FProjectWorldPlayableTourDriver>();
		if (!PlayableTourDriver->Initialize(
				*PlayerController.Get(),
				*PlayerCharacter.Get(),
				Routes[0].Points,
				OutError))
		{
			return false;
		}
	}

#if CSV_PROFILER
	FCsvProfiler* CsvProfiler = FCsvProfiler::Get();
	if (CsvProfiler == nullptr || CsvProfiler->IsCapturing() || CsvProfiler->IsWritingFile())
	{
		OutError = TEXT("Native UE CSV profiler is unavailable or already busy.");
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(RequestedCsvPath), true);
	IFileManager::Get().Delete(*RequestedCsvPath, false, true, true);
	FCsvProfiler::SetNonPersistentMetadata(TEXT("ProjectWorldProfile"), *RuntimeProfileId);
	FCsvProfiler::SetNonPersistentMetadata(TEXT("ProjectWorldProfileSha256"), *RuntimeProfileHash);
	FCsvProfiler::SetNonPersistentMetadata(TEXT("ProjectWorldMachine"), *MachineProfileId);
	CsvProfiler->BeginCapture(
		-1,
		FPaths::GetPath(RequestedCsvPath),
		FPaths::GetCleanFilename(RequestedCsvPath));
	bCsvCaptureStarted = true;
#else
	OutError = TEXT("This Shipping build does not include UE CSV profiler support.");
	return false;
#endif

	Collector = MakeShared<FProjectWorldPerformanceCollector>();
	GEngine->AddPerformanceDataConsumer(Collector);
	CollectorRegistration.MarkRegistered();
	LastResidencySampleSeconds = FPlatformTime::Seconds();
	LastMemorySampleSeconds = LastResidencySampleSeconds;
	SetPhase(EPhase::Warmup);
	return true;
}

bool FProjectWorldProductPerformanceGate::BuildRoutes(FString& OutError)
{
	UWorldPartition* Partition = ProductWorld->GetWorldPartition();
	if (Partition == nullptr || Partition->RuntimeHash == nullptr)
	{
		OutError = TEXT("The accepted Kazan world has no runtime partition hash.");
		return false;
	}
	FBox2D Bounds(EForceInit::ForceInit);
	Partition->RuntimeHash->ForEachStreamingCells([&Bounds](const UWorldPartitionRuntimeCell* Cell)
	{
		if (Cell != nullptr && !Cell->IsAlwaysLoaded())
		{
			const FBox CellBounds = Cell->GetStreamingBounds();
			if (CellBounds.IsValid)
			{
				Bounds += FVector2D(CellBounds.Min.X, CellBounds.Min.Y);
				Bounds += FVector2D(CellBounds.Max.X, CellBounds.Max.Y);
			}
		}
		return true;
	});
	if (!Bounds.bIsValid || Bounds.GetSize().GetMin() < 100000.0)
	{
		OutError = TEXT("Runtime cell bounds cannot define a territory-scale traversal.");
		return false;
	}

	const FVector2D Size = Bounds.GetSize();
	const FVector2D Minimum = Bounds.Min + Size * 0.12;
	const FVector2D Maximum = Bounds.Max - Size * 0.12;
	const double DenseRadius = FMath::Min(Size.GetMin() * 0.04, 50000.0);
	const double Z = CenterLocation.Z;
	const auto Point = [Z](const FVector2D& XY) { return FVector(XY.X, XY.Y, Z); };
	const FVector Center(CenterLocation.X, CenterLocation.Y, Z);
	const FVector MinPoint = Point(Minimum);
	const FVector MaxPoint = Point(Maximum);
	const FVector TopLeft(Minimum.X, Maximum.Y, Z);
	const FVector BottomRight(Maximum.X, Minimum.Y, Z);

	Routes.Reset();
	if (bPlayableTourRequested)
	{
		const FVector DensePoint = Center + FVector(DenseRadius, 0.0, 0.0);
		Routes.Add({
			TEXT("playable_tour"),
			{Center, DensePoint, MinPoint, DensePoint, Center},
			360.0});
		return true;
	}
	Routes.Add({
		TEXT("dense_centre"),
		{Center, Center + FVector(DenseRadius, 0.0, 0.0),
		 Center + FVector(0.0, DenseRadius, 0.0),
		 Center - FVector(DenseRadius, 0.0, 0.0), Center},
		12.0});
	Routes.Add({TEXT("long_diagonal"), {Center, MinPoint, MaxPoint, Center}, 24.0});
	Routes.Add({TEXT("perimeter"), {Center, MinPoint, TopLeft, MaxPoint, BottomRight, MinPoint, Center}, 30.0});
	Routes.Add({TEXT("backtrack"), {Center, MaxPoint, Center}, 20.0});
	Routes.Add({TEXT("higher_speed_stress"), {Center, MinPoint, MaxPoint, Center}, 8.0});
	return true;
}

bool FProjectWorldProductPerformanceGate::TickWarmup()
{
	if (IsStreamingCompleted())
	{
		++StableReadyFrames;
		if (StableReadyFrames >= RequiredReadyFrames)
		{
			CaptureReadySeconds = FPlatformTime::Seconds() - GateStartedSeconds;
			if (bPlayableTourRequested)
			{
				FString Error;
				UWorldPartition* Partition = ProductWorld->GetWorldPartition();
				if (Partition == nullptr ||
					!PlayableTourResidency.FreezeCenterCells(*Partition, CenterLocation, Error))
				{
					Finish(TEXT("rejected"), TEXT("playable_tour_center_census_failed"), Error);
					return false;
				}
			}
			BeginRoute();
			return true;
		}
	}
	else
	{
		StableReadyFrames = 0;
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > WarmupTimeoutSeconds)
	{
		Finish(TEXT("rejected"), TEXT("performance_warmup_timeout"),
			TEXT("Kazan did not reach a stable streaming-complete state before traversal."));
	}
	return false;
}

void FProjectWorldProductPerformanceGate::BeginRoute()
{
	++CurrentRouteIndex;
	if (!Routes.IsValidIndex(CurrentRouteIndex))
	{
		if (bPlayableTourRequested)
		{
			FString Error;
			if (!RequestPlayableTourScreenshot(Error))
			{
				Finish(TEXT("rejected"), TEXT("playable_tour_screenshot_failed"), Error);
			}
		}
		else
		{
			FinishFromMetrics();
		}
		return;
	}

	FProjectWorldPerformanceRoute& Route = Routes[CurrentRouteIndex];
	RouteStartedSeconds = FPlatformTime::Seconds();
	StableReadyFrames = 0;
	Collector->BeginRoute(Route.Name);
	CSV_EVENT_GLOBAL(TEXT("ProjectWorldRouteBegin:%s"), *Route.Name);
	SetPhase(EPhase::Traversing);
}

void FProjectWorldProductPerformanceGate::TickRoute(float DeltaSeconds)
{
	FProjectWorldPerformanceRoute& Route = Routes[CurrentRouteIndex];
	if (bPlayableTourRequested)
	{
		FString Error;
		const EProjectWorldPlayableTourResult Result = PlayableTourDriver->Tick(DeltaSeconds, Error);
		if (Result == EProjectWorldPlayableTourResult::Rejected)
		{
			Finish(TEXT("rejected"), TEXT("playable_tour_input_failed"), Error);
		}
		else if (Result == EProjectWorldPlayableTourResult::Accepted)
		{
			Route.DurationSeconds = PlayableTourDriver->GetEvidence().DurationSeconds;
			SettlementStartedSeconds = FPlatformTime::Seconds();
			StableReadyFrames = 0;
			SetPhase(EPhase::Settling);
		}
		return;
	}
	const double Alpha = FMath::Clamp(
		(FPlatformTime::Seconds() - RouteStartedSeconds) / Route.DurationSeconds,
		0.0,
		1.0);
	const FVector PriorLocation = PlayerCharacter->GetActorLocation();
	const FVector Destination = RoutePosition(Route, Alpha);
	const FVector Direction = Destination - PriorLocation;
	if (!Direction.IsNearlyZero())
	{
		PlayerController->SetControlRotation(Direction.Rotation());
	}
	if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Flying);
		Movement->Velocity = Direction / FMath::Max(DeltaSeconds, UE_SMALL_NUMBER);
	}
	PlayerCharacter->SetActorLocation(Destination, false, nullptr, ETeleportType::None);
	if (Alpha >= 1.0)
	{
		SettlementStartedSeconds = FPlatformTime::Seconds();
		StableReadyFrames = 0;
		SetPhase(EPhase::Settling);
	}
}

void FProjectWorldProductPerformanceGate::TickSettlement()
{
	FProjectWorldPerformanceRoute& Route = Routes[CurrentRouteIndex];
	if (IsStreamingCompleted())
	{
		++StableReadyFrames;
		if (StableReadyFrames >= RequiredReadyFrames)
		{
			Route.ReadyWaitSeconds = FPlatformTime::Seconds() - SettlementStartedSeconds;
			Collector->EndRoute();
			CSV_EVENT_GLOBAL(TEXT("ProjectWorldRouteEnd:%s"), *Route.Name);
			BeginRoute();
		}
		return;
	}
	StableReadyFrames = 0;
	if (FPlatformTime::Seconds() - SettlementStartedSeconds > SettlementTimeoutSeconds)
	{
		++Route.StreamingFailures;
		++TotalStreamingFailures;
		Route.ReadyWaitSeconds = FPlatformTime::Seconds() - SettlementStartedSeconds;
		Collector->EndRoute();
		CSV_EVENT_GLOBAL(TEXT("ProjectWorldRouteStreamingFailure:%s"), *Route.Name);
		BeginRoute();
	}
}

void FProjectWorldProductPerformanceGate::SampleResidency(float DeltaSeconds)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - LastResidencySampleSeconds >= ResidencySampleIntervalSeconds)
	{
		const double SampleDuration = Now - LastResidencySampleSeconds;
		LastResidencySampleSeconds = Now;
		int32 LoadedCells = 0;
		int32 ActivatedCells = 0;
		UWorldPartition* Partition = ProductWorld->GetWorldPartition();
		if (Partition != nullptr && Partition->RuntimeHash != nullptr)
		{
			Partition->RuntimeHash->ForEachStreamingCells(
				[this, &LoadedCells, &ActivatedCells](const UWorldPartitionRuntimeCell* Cell)
			{
				if (Cell == nullptr)
				{
					return true;
				}
				const uint8 State = static_cast<uint8>(Cell->GetCurrentState());
				LoadedCells += State >= static_cast<uint8>(EWorldPartitionRuntimeCellState::Loaded) ? 1 : 0;
				ActivatedCells += State == static_cast<uint8>(EWorldPartitionRuntimeCellState::Activated) ? 1 : 0;
				if (bPlayableTourRequested && PlayableTourDriver.IsValid())
				{
					PlayableTourResidency.ObserveCell(
						*Cell,
						PlayableTourDriver->HasReachedEdge(),
						PlayableTourDriver->HasReturnedToCenter());
				}
				if (const uint8* PriorState = PriorCellStates.Find(Cell->GetGuid());
					PriorState != nullptr && *PriorState != State)
				{
					if (Routes.IsValidIndex(CurrentRouteIndex))
					{
						++Routes[CurrentRouteIndex].ActivationTransitions;
						++TotalActivationTransitions;
					}
				}
				PriorCellStates.Add(Cell->GetGuid(), State);
				return true;
			});
		}
		if (Routes.IsValidIndex(CurrentRouteIndex))
		{
			FProjectWorldPerformanceRoute& Route = Routes[CurrentRouteIndex];
			Route.LoadedCellSeconds += LoadedCells * SampleDuration;
			Route.ActivatedCellSeconds += ActivatedCells * SampleDuration;
			Route.PeakLoadedCells = FMath::Max(Route.PeakLoadedCells, LoadedCells);
			Route.PeakActivatedCells = FMath::Max(Route.PeakActivatedCells, ActivatedCells);
		}
	}

	if (Now - LastMemorySampleSeconds >= MemorySampleIntervalSeconds)
	{
		LastMemorySampleSeconds = Now;
		const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
		PeakProcessPhysicalBytes = FMath::Max(PeakProcessPhysicalBytes, Memory.PeakUsedPhysical);
		if (GDynamicRHI != nullptr)
		{
			FRHIMemoryStats GpuMemory;
			RHIGetMemoryStats(GpuMemory);
			PeakGpuLocalBytes = FMath::Max(PeakGpuLocalBytes, GpuMemory.UsedLocal);
		}
	}
}

bool FProjectWorldProductPerformanceGate::RequestPlayableTourScreenshot(FString& OutError)
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
	IFileManager::Get().Delete(*ScreenshotPath, false, true, true);
	if (!ProjectWorldRuntimeScreenshotCapture::CapturePlayerContext(
			*ProductWorld.Get(),
			*PlayerController.Get(),
			ScreenshotPath,
			OutError))
	{
		return false;
	}
	SetPhase(EPhase::WaitingForScreenshot);
	return true;
}

void FProjectWorldProductPerformanceGate::TickScreenshot()
{
	if (IFileManager::Get().FileSize(*ScreenshotPath) > 0)
	{
		FString Error;
		if (!ProjectWorldScreenshotValidation::ValidateFile(ScreenshotPath, Error))
		{
			Finish(TEXT("rejected"), TEXT("playable_tour_screenshot_invalid"), Error);
			return;
		}
		FinishFromMetrics();
		return;
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > PerformanceScreenshotTimeoutSeconds)
	{
		Finish(TEXT("rejected"), TEXT("playable_tour_screenshot_timeout"),
			TEXT("Playable-tour screenshot did not arrive within the bounded timeout."));
	}
}

void FProjectWorldProductPerformanceGate::FinishFromMetrics()
{
	const FProjectWorldPerformanceStatistics Statistics =
		ProjectWorldPerformanceMetrics::Calculate(Collector->AllFrames());
	bool bAccepted = ProjectWorldPerformanceMetrics::IsAccepted(
		Statistics,
		TotalStreamingFailures,
		FrameP95BudgetMilliseconds,
		AcceptanceReason);
	if (bAccepted && bPlayableTourRequested && !PlayableTourResidency.HasCompleteCycle())
	{
		bAccepted = false;
		AcceptanceReason = TEXT(
			"Playable tour did not prove the same initial center cell unloaded at the edge and reloaded after return.");
	}
	Finish(
		bAccepted ? TEXT("accepted") : TEXT("rejected"),
		bAccepted ? FString() : TEXT("performance_hard_gate_failed"),
		AcceptanceReason);
}

FVector FProjectWorldProductPerformanceGate::RoutePosition(
	const FProjectWorldPerformanceRoute& Route,
	double Alpha) const
{
	if (Route.Points.Num() < 2)
	{
		return CenterLocation;
	}
	TArray<double> SegmentLengths;
	double TotalLength = 0.0;
	for (int32 Index = 1; Index < Route.Points.Num(); ++Index)
	{
		const double Length = FVector::Distance(Route.Points[Index - 1], Route.Points[Index]);
		SegmentLengths.Add(Length);
		TotalLength += Length;
	}
	const double TargetDistance = TotalLength * Alpha;
	double PriorDistance = 0.0;
	for (int32 Index = 0; Index < SegmentLengths.Num(); ++Index)
	{
		const double NextDistance = PriorDistance + SegmentLengths[Index];
		if (TargetDistance <= NextDistance || Index == SegmentLengths.Num() - 1)
		{
			const double SegmentAlpha = SegmentLengths[Index] <= UE_SMALL_NUMBER ? 1.0 :
				(TargetDistance - PriorDistance) / SegmentLengths[Index];
			return FMath::Lerp(Route.Points[Index], Route.Points[Index + 1], SegmentAlpha);
		}
		PriorDistance = NextDistance;
	}
	return Route.Points.Last();
}

bool FProjectWorldProductPerformanceGate::IsStreamingCompleted() const
{
	UWorldPartition* Partition = ProductWorld.IsValid() ? ProductWorld->GetWorldPartition() : nullptr;
	if (Partition == nullptr)
	{
		return false;
	}
	const TArray<FWorldPartitionStreamingSource>& Sources = Partition->GetStreamingSources();
	return !Sources.IsEmpty() && Partition->IsStreamingCompleted(&Sources);
}

void FProjectWorldProductPerformanceGate::EndCapture()
{
	if (Collector.IsValid() && GEngine != nullptr && CollectorRegistration.Consume())
	{
		Collector->EndRoute();
		GEngine->RemovePerformanceDataConsumer(Collector);
	}
#if CSV_PROFILER
	CsvWriteFuture = FCsvProfiler::Get()->EndCapture();
#endif
	bCsvCaptureStarted = false;
	SetPhase(EPhase::WaitingForCsv);
}

void FProjectWorldProductPerformanceGate::Finish(
	const FString& Status,
	const FString& ErrorCode,
	const FString& ErrorMessage)
{
	PendingStatus = Status;
	PendingErrorCode = ErrorCode;
	PendingErrorMessage = ErrorMessage;
	if (bCsvCaptureStarted)
	{
		EndCapture();
		return;
	}
	WriteResult(Status, ErrorCode, ErrorMessage);
	SetPhase(EPhase::Finished);
	FPlatformMisc::RequestExitWithStatus(
		false,
		Status == TEXT("accepted") ? 0 : 10,
		TEXT("ProjectWorldProductPerformanceGate.Finished"));
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
	Root->SetNumberField(TEXT("frame_p95_budget_ms"), FrameP95BudgetMilliseconds);
	Root->SetNumberField(TEXT("time_to_ready_seconds"), CaptureReadySeconds);
	Root->SetNumberField(TEXT("streaming_failures"), TotalStreamingFailures);
	Root->SetNumberField(TEXT("activation_transitions"), TotalActivationTransitions);
	Root->SetNumberField(TEXT("unloaded_cell_count"), PlayableTourResidency.GetUnloadedCount());
	Root->SetNumberField(TEXT("reloaded_cell_count"), PlayableTourResidency.GetReloadedCount());
	Root->SetNumberField(TEXT("peak_process_physical_bytes"), static_cast<double>(PeakProcessPhysicalBytes));
	Root->SetNumberField(TEXT("peak_gpu_local_bytes"), static_cast<double>(PeakGpuLocalBytes));
	Root->SetStringField(TEXT("csv_capture"), WrittenCsvPath);
	Root->SetNumberField(TEXT("sample_count"), Overall.SampleCount);
	Root->SetNumberField(TEXT("frame_p95_ms"), Overall.FrameP95Milliseconds);
	Root->SetNumberField(TEXT("frame_p99_ms"), Overall.FrameP99Milliseconds);
	Root->SetNumberField(TEXT("frame_max_ms"), Overall.FrameMaxMilliseconds);
	Root->SetNumberField(TEXT("game_p95_ms"), Overall.GameP95Milliseconds);
	Root->SetNumberField(TEXT("render_p95_ms"), Overall.RenderP95Milliseconds);
	Root->SetNumberField(TEXT("gpu_p95_ms"), Overall.GPUP95Milliseconds);
	Root->SetStringField(TEXT("acceptance_reason"), AcceptanceReason);
	Root->SetBoolField(TEXT("playable_tour"), bPlayableTourRequested);
	Root->SetStringField(TEXT("playable_tour_screenshot"), ScreenshotPath);
	Root->SetStringField(TEXT("correctness_screenshot"), CorrectnessScreenshotPath);
	Root->SetBoolField(TEXT("gameplay_interaction"), bCorrectnessGameplayInteraction);
	Root->SetBoolField(TEXT("terrain_collision"), bCorrectnessTerrainCollision);
	Root->SetBoolField(TEXT("road_collision"), bCorrectnessRoadCollision);
	Root->SetBoolField(TEXT("building_collision"), bCorrectnessBuildingCollision);
	if (PlayableTourDriver.IsValid())
	{
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

void FProjectWorldProductPerformanceGate::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStartedSeconds = FPlatformTime::Seconds();
}
