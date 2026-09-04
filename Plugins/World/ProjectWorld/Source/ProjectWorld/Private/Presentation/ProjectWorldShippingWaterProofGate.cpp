// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldShippingWaterProofGate.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"
#include "RHI.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WorldPartition/WorldPartition.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldShippingWaterProof, Log, All);

namespace
{
	constexpr double ShippingWaterOverallTimeoutSeconds = 420.0;
	constexpr double ShippingWaterWorldTimeoutSeconds = 120.0;
	constexpr double ShippingWaterSettlementTimeoutSeconds = 30.0;
	constexpr double ShippingWaterRepeatDelaySeconds = 2.0;
	constexpr int32 ShippingWaterRequiredStableFrames = 3;
	constexpr double ShippingWaterMaximumStableSpeedCentimetersPerSecond = 5.0;
	constexpr double ShippingWaterMaximumRepeatLocationDriftCentimeters = 1.0;
	constexpr double ShippingWaterTravelNumericToleranceCentimeters = 1.0;
	constexpr float ShippingWaterCaptureOrthoWidthCentimeters = 20000.0f;
	constexpr float ShippingWaterFinalColorFieldOfViewDegrees = 70.0f;
	const FVector ShippingWaterFinalColorCameraOffsetCentimeters(16000.0, 0.0, 12000.0);

	bool ParseShippingWaterRequiredValue(
		const TCHAR* Key,
		FString& OutValue,
		bool bShouldStopOnSeparator = true)
	{
		return FParse::Value(
			FCommandLine::Get(), Key, OutValue, bShouldStopOnSeparator) && !OutValue.IsEmpty();
	}

	bool IsShippingWaterAsciiToken(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			const bool bValid =
				(Character >= TEXT('A') && Character <= TEXT('Z')) ||
				(Character >= TEXT('a') && Character <= TEXT('z')) ||
				(Character >= TEXT('0') && Character <= TEXT('9')) ||
				Character == TEXT('_') || Character == TEXT('-');
			if (!bValid)
			{
				return false;
			}
		}
		return true;
	}

	bool IsShippingWaterSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!((Character >= TEXT('0') && Character <= TEXT('9')) ||
				(Character >= TEXT('a') && Character <= TEXT('f'))))
			{
				return false;
			}
		}
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> ShippingWaterVectorValue(const FVector& Value)
	{
		return
		{
			MakeShared<FJsonValueNumber>(Value.X),
			MakeShared<FJsonValueNumber>(Value.Y),
			MakeShared<FJsonValueNumber>(Value.Z)
		};
	}
}

bool ProjectWorldShippingWaterProofContract::ValidateTargetRelativeTravel(
	double RequestedTargetDistanceCentimeters,
	double TargetXYErrorCentimeters,
	double HorizontalDisplacementCentimeters,
	FString& OutError)
{
	if (!FMath::IsFinite(RequestedTargetDistanceCentimeters) ||
		!FMath::IsFinite(TargetXYErrorCentimeters) ||
		!FMath::IsFinite(HorizontalDisplacementCentimeters) ||
		RequestedTargetDistanceCentimeters < 0.0 || TargetXYErrorCentimeters < 0.0 ||
		HorizontalDisplacementCentimeters < 0.0)
	{
		OutError = TEXT("Shipping Water travel evidence contains an invalid distance.");
		return false;
	}
	if (RequestedTargetDistanceCentimeters <= MaximumTargetXYErrorCentimeters)
	{
		OutError = TEXT("The Shipping Water target does not require travel beyond its arrival radius.");
		return false;
	}
	if (TargetXYErrorCentimeters > MaximumTargetXYErrorCentimeters)
	{
		OutError = TEXT("The real-input character did not reach the Shipping Water arrival radius.");
		return false;
	}
	const double MinimumRequiredTravel =
		RequestedTargetDistanceCentimeters - MaximumTargetXYErrorCentimeters;
	if (HorizontalDisplacementCentimeters + ShippingWaterTravelNumericToleranceCentimeters <
		MinimumRequiredTravel)
	{
		OutError = TEXT("The accumulated real-input travel is shorter than the target-relative minimum.");
		return false;
	}
	return true;
}

FProjectWorldShippingWaterProofGate::~FProjectWorldShippingWaterProofGate()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Driver.ReleaseInputs();
	WaterCaptureSession.Reset();
	FinalColorCaptureSession.Reset();
}

void FProjectWorldShippingWaterProofGate::StartIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldShippingWaterProof")))
	{
		return;
	}
	FString Error;
	if (!ParseConfig(Error))
	{
		StartupError = Error;
	}
	GateStartedSeconds = FPlatformTime::Seconds();
	SetPhase(EPhase::WaitingForWorld);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FProjectWorldShippingWaterProofGate::Tick));
}

bool FProjectWorldShippingWaterProofGate::ParseConfig(FString& OutError)
{
	FString TargetText;
	if (!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterOperation="), Config.OperationId) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterResult="), Config.ResultPath) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterReference="), Config.ReferencePath) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterRepeat="), Config.RepeatPath) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterProduct="), Config.ProductPath) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterMap="), Config.MapPackage) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterRuntime="), Config.RuntimeProfileId) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterRuntimeHash="), Config.RuntimeProfileHash) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterMachine="), Config.MachineProfileId) ||
		!ParseShippingWaterRequiredValue(TEXT("ProjectWorldShippingWaterTarget="), TargetText, false))
	{
		OutError = TEXT("A required Shipping Water proof argument is missing.");
		return false;
	}

	TArray<FString> Coordinates;
	TargetText.ParseIntoArray(Coordinates, TEXT(","), true);
	if (Coordinates.Num() != 3 ||
		!LexTryParseString(Config.TargetLocation.X, *Coordinates[0]) ||
		!LexTryParseString(Config.TargetLocation.Y, *Coordinates[1]) ||
		!LexTryParseString(Config.TargetLocation.Z, *Coordinates[2]))
	{
		OutError = TEXT("The Shipping Water target must be X,Y,Z numeric coordinates.");
		return false;
	}

	FPaths::NormalizeFilename(Config.ResultPath);
	FPaths::NormalizeFilename(Config.ReferencePath);
	FPaths::NormalizeFilename(Config.RepeatPath);
	FPaths::NormalizeFilename(Config.ProductPath);
	const bool bPathsValid = !FPaths::IsRelative(Config.ResultPath) &&
		!FPaths::IsRelative(Config.ReferencePath) && !FPaths::IsRelative(Config.RepeatPath) &&
		!FPaths::IsRelative(Config.ProductPath) &&
		Config.ResultPath != Config.ReferencePath && Config.ResultPath != Config.RepeatPath &&
		Config.ResultPath != Config.ProductPath && Config.ReferencePath != Config.RepeatPath &&
		Config.ReferencePath != Config.ProductPath && Config.RepeatPath != Config.ProductPath;
	if (!bPathsValid || !IsShippingWaterAsciiToken(Config.OperationId) ||
		!IsShippingWaterAsciiToken(Config.RuntimeProfileId) ||
		!IsShippingWaterAsciiToken(Config.MachineProfileId) ||
		!IsShippingWaterSha256(Config.RuntimeProfileHash) || Config.TargetLocation.ContainsNaN() ||
		FVector2D(Config.TargetLocation).IsNearlyZero() ||
		!Config.MapPackage.StartsWith(TEXT("/ProjectWorldData/Generated/Territory/")))
	{
		OutError = TEXT("The Shipping Water proof identity, paths, map, hash, or target are invalid.");
		return false;
	}
	return true;
}

bool FProjectWorldShippingWaterProofGate::Tick(float DeltaSeconds)
{
	if (Phase == EPhase::Finished)
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	if (!StartupError.IsEmpty())
	{
		FinishRejected(TEXT("shipping_water_config_invalid"), StartupError);
		return false;
	}
	if (Now - GateStartedSeconds > ShippingWaterOverallTimeoutSeconds)
	{
		FinishRejected(TEXT("shipping_water_timeout"), TEXT("The bounded Shipping Water proof timed out."));
		return false;
	}

	if (Phase == EPhase::WaitingForWorld)
	{
		FString Error;
		if (!TryAcquireProductWorld(Error) && Now - PhaseStartedSeconds > ShippingWaterWorldTimeoutSeconds)
		{
			FinishRejected(TEXT("shipping_water_world_timeout"), Error);
		}
		return Phase != EPhase::Finished;
	}

	if (!ProductWorld.IsValid() || !PlayerController.IsValid() || !PlayerCharacter.IsValid())
	{
		FinishRejected(TEXT("shipping_water_ownership_lost"),
			TEXT("The Shipping product world, controller, or character became unavailable."));
		return false;
	}

	if (Phase == EPhase::Touring)
	{
		FString Error;
		const EProjectWorldPlayableTourResult Result = Driver.Tick(DeltaSeconds, Error);
		if (Result == EProjectWorldPlayableTourResult::Rejected)
		{
			FinishRejected(TEXT("shipping_water_input_failed"), Error);
		}
		else if (!bReferenceCaptured && Driver.GetEvidence().WaypointsReached >= 1)
		{
			SetPhase(EPhase::SettlingAtWater);
		}
		else if (Result == EProjectWorldPlayableTourResult::Accepted)
		{
			if (!bReferenceCaptured || !bRepeatCaptured || !bFinalColorCaptured)
			{
				FinishRejected(TEXT("shipping_water_capture_missing"),
					TEXT("The real-input tour completed without the required Water evidence images."));
			}
			else
			{
				FinishAccepted();
			}
		}
		return Phase != EPhase::Finished;
	}

	if (Phase == EPhase::SettlingAtWater)
	{
		const UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
		const bool bStable = Movement != nullptr &&
			Movement->Velocity.Size() <= ShippingWaterMaximumStableSpeedCentimetersPerSecond &&
			IsStreamingComplete();
		StableFrames = bStable ? StableFrames + 1 : 0;
		if (StableFrames >= ShippingWaterRequiredStableFrames)
		{
			FString Error;
			CapturePlayerLocation = PlayerCharacter->GetActorLocation();
			if (!InitializeWaterCapture(Error) || !CaptureWater(Config.ReferencePath, Error))
			{
				FinishRejected(TEXT("shipping_water_reference_failed"), Error);
			}
			else
			{
				bReferenceCaptured = true;
				ReferenceCaptureSessionId = WaterCaptureSession.GetSessionId();
				SetPhase(EPhase::WaitingForRepeat);
			}
		}
		else if (Now - PhaseStartedSeconds > ShippingWaterSettlementTimeoutSeconds)
		{
			FinishRejected(TEXT("shipping_water_settlement_failed"),
				TEXT("The real-input Water viewpoint did not become stationary and streaming-complete."));
		}
		return Phase != EPhase::Finished;
	}

	if (Phase == EPhase::WaitingForRepeat &&
		Now - PhaseStartedSeconds >= ShippingWaterRepeatDelaySeconds)
	{
		const double Drift = FVector::Distance(CapturePlayerLocation, PlayerCharacter->GetActorLocation());
		if (Drift > ShippingWaterMaximumRepeatLocationDriftCentimeters)
		{
			FinishRejected(TEXT("shipping_water_pose_drift"),
				FString::Printf(TEXT("The repeated Water pose drifted %.3f cm."), Drift));
			return false;
		}
		FString Error;
		if (!CaptureWater(Config.RepeatPath, Error))
		{
			FinishRejected(TEXT("shipping_water_repeat_failed"), Error);
		}
		else
		{
			bRepeatCaptured = true;
			RepeatCaptureSessionId = WaterCaptureSession.GetSessionId();
			if (!CaptureFinalColorWater(Error))
			{
				FinishRejected(TEXT("shipping_water_final_color_failed"), Error);
			}
			else
			{
				bFinalColorCaptured = true;
				FinishAccepted();
			}
		}
	}
	return Phase != EPhase::Finished;
}

bool FProjectWorldShippingWaterProofGate::TryAcquireProductWorld(FString& OutError)
{
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		OutError = TEXT("The normal Shipping viewport is not ready.");
		return false;
	}
	UWorld* World = GEngine->GameViewport->GetWorld();
	if (World == nullptr || !World->IsGameWorld() || World->GetPackage()->GetName() != Config.MapPackage)
	{
		OutError = TEXT("Waiting for the normal menu/loading route to enter the requested World.");
		return false;
	}
	if (FCString::Strcmp(World->URL.GetOption(TEXT("ProjectLoadingRoute="), TEXT("")), TEXT("1")) != 0 ||
		FCString::Strcmp(World->URL.GetOption(TEXT("Traversal="), TEXT("")), TEXT("PreviewFlight")) != 0)
	{
		OutError = TEXT("Shipping Water proof requires ProjectLoading provenance and PreviewFlight.");
		return false;
	}
	AGameModeBase* GameMode = World->GetAuthGameMode();
	APlayerController* Controller = World->GetFirstPlayerController();
	ACharacter* Character = Controller == nullptr ? nullptr : Cast<ACharacter>(Controller->GetPawn());
	UCharacterMovementComponent* Movement = Character == nullptr ? nullptr : Character->GetCharacterMovement();
	if (GameMode == nullptr ||
		GameMode->GetClass()->GetPathName() != TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode") ||
		Controller == nullptr || Character == nullptr || Movement == nullptr ||
		Character->GetClass()->GetPathName() != TEXT("/Script/ProjectCharacter.DefinitionCharacter") ||
		Character->GetController() != Controller || !Movement->IsFlying())
	{
		OutError = TEXT("Waiting for the normal possessed PreviewFlight DefinitionCharacter.");
		return false;
	}

	ProductWorld = World;
	PlayerController = Controller;
	PlayerCharacter = Character;
	const FVector Center = Character->GetActorLocation();
	StartPlayerLocation = Center;
	FVector Target = Config.TargetLocation;
	Target.Z = Center.Z;
	RequestedTargetDistanceCentimeters = FVector2D::Distance(FVector2D(Center), FVector2D(Target));
	const TArray<FVector> Waypoints = {Center, Target, Center};
	if (!Driver.Initialize(*Controller, *Character, Waypoints, OutError))
	{
		ProductWorld.Reset();
		PlayerController.Reset();
		PlayerCharacter.Reset();
		return false;
	}
	SetPhase(EPhase::Touring);
	return true;
}

bool FProjectWorldShippingWaterProofGate::IsStreamingComplete() const
{
	const UWorld* World = ProductWorld.Get();
	UWorldPartition* Partition = World == nullptr ? nullptr : World->GetWorldPartition();
	if (Partition == nullptr)
	{
		return false;
	}
	const TArray<FWorldPartitionStreamingSource>& Sources = Partition->GetStreamingSources();
	return !Sources.IsEmpty() && Partition->IsStreamingCompleted(&Sources);
}

bool FProjectWorldShippingWaterProofGate::InitializeWaterCapture(FString& OutError)
{
	if (!ProductWorld.IsValid() || !PlayerCharacter.IsValid())
	{
		OutError = TEXT("The Shipping Water capture lost its product world or character.");
		return false;
	}
	CaptureCameraLocation = FVector(
		Config.TargetLocation.X,
		Config.TargetLocation.Y,
		FMath::Max(PlayerCharacter->GetActorLocation().Z, Config.TargetLocation.Z + 10000.0));
	ProjectWorldRuntimeScreenshotCapture::FCaptureSpec Spec;
	Spec.CameraLocation = CaptureCameraLocation;
	Spec.CameraRotation = FRotator(-90.0f, 0.0f, 0.0f);
	Spec.CaptureSource = SCS_BaseColor;
	Spec.SourceIdentity = TEXT("canonical_water_base_color");
	Spec.OrthographicWidthCentimeters = ShippingWaterCaptureOrthoWidthCentimeters;
	Spec.bIsolateBaseColor = true;
	return WaterCaptureSession.Initialize(*ProductWorld.Get(), Spec, OutError) &&
		WaterCaptureSession.WarmUp(3, OutError);
}

bool FProjectWorldShippingWaterProofGate::CaptureWater(const FString& Path, FString& OutError)
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	IFileManager::Get().Delete(*Path, false, true, true);
	return WaterCaptureSession.Capture(Path, OutError);
}

bool FProjectWorldShippingWaterProofGate::CaptureFinalColorWater(FString& OutError)
{
	if (!ProductWorld.IsValid())
	{
		OutError = TEXT("The Shipping Water final-color capture lost its product world.");
		return false;
	}
	FinalColorCameraLocation = Config.TargetLocation + ShippingWaterFinalColorCameraOffsetCentimeters;
	FinalColorCameraRotation = (Config.TargetLocation - FinalColorCameraLocation).Rotation();
	ProjectWorldRuntimeScreenshotCapture::FCaptureSpec Spec;
	Spec.CameraLocation = FinalColorCameraLocation;
	Spec.CameraRotation = FinalColorCameraRotation;
	Spec.CaptureSource = SCS_FinalColorLDR;
	Spec.SourceIdentity = TEXT("canonical_water_final_color");
	Spec.FieldOfViewDegrees = ShippingWaterFinalColorFieldOfViewDegrees;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Config.ProductPath), true);
	IFileManager::Get().Delete(*Config.ProductPath, false, true, true);
	return FinalColorCaptureSession.Initialize(*ProductWorld.Get(), Spec, OutError) &&
		FinalColorCaptureSession.WarmUp(3, OutError) &&
		FinalColorCaptureSession.Capture(Config.ProductPath, OutError);
}

void FProjectWorldShippingWaterProofGate::FinishAccepted()
{
	Driver.ReleaseInputs();
	const FProjectWorldPlayableTourEvidence& Evidence = Driver.GetEvidence();
	FString Error;
	const double TargetXYError = FVector2D::Distance(
		FVector2D(Config.TargetLocation), FVector2D(CapturePlayerLocation));
	if (Evidence.WaypointsReached < 1 || Evidence.InputEventCount < 1 ||
		WaterCaptureSession.GetWrittenCaptureCount() != 2 ||
		FinalColorCaptureSession.GetWrittenCaptureCount() != 1 ||
		!ProjectWorldShippingWaterProofContract::ValidateTargetRelativeTravel(
			RequestedTargetDistanceCentimeters,
			TargetXYError,
			Evidence.HorizontalDisplacementCentimeters,
			Error))
	{
		FinishRejected(
			TEXT("shipping_water_acceptance_contract_failed"),
			Error.IsEmpty() ? TEXT("The Shipping Water input or capture evidence is incomplete.") : Error);
		return;
	}
	WriteResult(TEXT("accepted"), FString(), FString());
	SetPhase(EPhase::Finished);
	WaterCaptureSession.Reset();
	FinalColorCaptureSession.Reset();
	FPlatformMisc::RequestExitWithStatus(false, 0, TEXT("ProjectWorldShippingWaterProof.Accepted"));
}

void FProjectWorldShippingWaterProofGate::FinishRejected(const FString& Code, const FString& Message)
{
	Driver.ReleaseInputs();
	WriteResult(TEXT("rejected"), Code, Message);
	SetPhase(EPhase::Finished);
	WaterCaptureSession.Reset();
	FinalColorCaptureSession.Reset();
	UE_LOG(LogProjectWorldShippingWaterProof, Error,
		TEXT("[FProjectWorldShippingWaterProofGate::FinishRejected] Rejected - code=%s message=%s"),
		*Code,
		*Message);
	FPlatformMisc::RequestExitWithStatus(false, 9, TEXT("ProjectWorldShippingWaterProof.Rejected"));
}

void FProjectWorldShippingWaterProofGate::WriteResult(
	const FString& Status,
	const FString& ErrorCode,
	const FString& ErrorMessage) const
{
	if (Config.ResultPath.IsEmpty())
	{
		return;
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("status"), Status);
	Root->SetStringField(TEXT("operation_id"), Config.OperationId);
	Root->SetStringField(TEXT("map_package"), Config.MapPackage);
	Root->SetStringField(TEXT("runtime_profile"), Config.RuntimeProfileId);
	Root->SetStringField(TEXT("runtime_profile_sha256"), Config.RuntimeProfileHash);
	Root->SetStringField(TEXT("machine_profile_id"), Config.MachineProfileId);
	Root->SetStringField(TEXT("executable"), FPlatformProcess::ExecutablePath());
	Root->SetStringField(TEXT("build_configuration"), LexToString(FApp::GetBuildConfiguration()));
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("gpu_adapter"), GRHIAdapterName);
	Root->SetStringField(TEXT("rhi"), GDynamicRHI == nullptr ? TEXT("unavailable") : GDynamicRHI->GetName());
	Root->SetBoolField(TEXT("project_loading_provenance"), ProductWorld.IsValid());
	Root->SetBoolField(TEXT("preview_flight"), PlayerCharacter.IsValid() &&
		PlayerCharacter->GetCharacterMovement() != nullptr &&
		PlayerCharacter->GetCharacterMovement()->IsFlying());
	Root->SetStringField(TEXT("pawn_class"), PlayerCharacter.IsValid() ?
		PlayerCharacter->GetClass()->GetPathName() : FString());
	Root->SetStringField(TEXT("reference_screenshot"), Config.ReferencePath);
	Root->SetStringField(TEXT("repeat_screenshot"), Config.RepeatPath);
	Root->SetStringField(TEXT("final_color_screenshot"), Config.ProductPath);
	Root->SetArrayField(TEXT("requested_water_target_cm"), ShippingWaterVectorValue(Config.TargetLocation));
	Root->SetArrayField(TEXT("start_player_location_cm"), ShippingWaterVectorValue(StartPlayerLocation));
	Root->SetNumberField(TEXT("requested_target_distance_cm"), RequestedTargetDistanceCentimeters);
	Root->SetNumberField(TEXT("allowed_target_xy_error_cm"),
		ProjectWorldShippingWaterProofContract::MaximumTargetXYErrorCentimeters);
	Root->SetArrayField(TEXT("capture_player_location_cm"), ShippingWaterVectorValue(CapturePlayerLocation));
	Root->SetNumberField(TEXT("target_xy_error_cm"), FVector2D::Distance(
		FVector2D(Config.TargetLocation), FVector2D(CapturePlayerLocation)));
	Root->SetArrayField(TEXT("capture_camera_location_cm"), ShippingWaterVectorValue(CaptureCameraLocation));
	Root->SetArrayField(TEXT("capture_camera_rotation_deg"),
		ShippingWaterVectorValue(FVector(-90.0, 0.0, 0.0)));
	Root->SetStringField(TEXT("capture_source"), TEXT("SCS_BaseColor"));
	Root->SetStringField(TEXT("capture_projection"), TEXT("orthographic"));
	Root->SetNumberField(TEXT("capture_ortho_width_cm"), ShippingWaterCaptureOrthoWidthCentimeters);
	Root->SetStringField(TEXT("capture_session_id"), WaterCaptureSession.GetSessionId());
	Root->SetStringField(TEXT("reference_capture_session_id"), ReferenceCaptureSessionId);
	Root->SetStringField(TEXT("repeat_capture_session_id"), RepeatCaptureSessionId);
	Root->SetBoolField(TEXT("same_capture_session"),
		!ReferenceCaptureSessionId.IsEmpty() && ReferenceCaptureSessionId == RepeatCaptureSessionId);
	Root->SetNumberField(TEXT("capture_session_written_image_count"),
		WaterCaptureSession.GetWrittenCaptureCount());
	Root->SetNumberField(TEXT("repeat_delay_seconds"), ShippingWaterRepeatDelaySeconds);
	Root->SetBoolField(TEXT("reference_captured"), bReferenceCaptured);
	Root->SetBoolField(TEXT("repeat_captured"), bRepeatCaptured);
	Root->SetStringField(TEXT("final_color_capture_source"), TEXT("SCS_FinalColorLDR"));
	Root->SetStringField(TEXT("final_color_capture_projection"), TEXT("perspective"));
	Root->SetArrayField(TEXT("final_color_camera_location_cm"),
		ShippingWaterVectorValue(FinalColorCameraLocation));
	Root->SetArrayField(TEXT("final_color_camera_rotation_deg"), ShippingWaterVectorValue(FVector(
		FinalColorCameraRotation.Pitch, FinalColorCameraRotation.Yaw, FinalColorCameraRotation.Roll)));
	Root->SetNumberField(TEXT("final_color_field_of_view_degrees"),
		ShippingWaterFinalColorFieldOfViewDegrees);
	Root->SetStringField(TEXT("final_color_capture_session_id"), FinalColorCaptureSession.GetSessionId());
	Root->SetNumberField(TEXT("final_color_capture_session_written_image_count"),
		FinalColorCaptureSession.GetWrittenCaptureCount());
	Root->SetBoolField(TEXT("final_color_captured"), bFinalColorCaptured);
	Driver.AppendReceiptFields(*Root);
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
	if (FFileHelper::SaveStringToFile(
		Payload + TEXT("\n"), *Staging, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		IFileManager::Get().Move(*Config.ResultPath, *Staging, true, true);
	}
}

void FProjectWorldShippingWaterProofGate::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStartedSeconds = FPlatformTime::Seconds();
	StableFrames = 0;
}
