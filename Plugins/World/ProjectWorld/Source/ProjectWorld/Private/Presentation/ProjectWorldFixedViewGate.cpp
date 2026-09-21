// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldFixedViewGate.h"

#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"

#include "Components/ActorComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartition.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldFixedViewGate, Log, All);

namespace
{
	constexpr double GateTimeoutSeconds = 180.0;
	constexpr double FixedViewStreamingTimeoutSeconds = 120.0;
	constexpr int32 RequiredStableStreamingFrames = 3;
	constexpr int32 RequiredSettleFrames = 30;

	bool ParseRequiredValue(const TCHAR* Name, FString& OutValue, bool bStopOnSeparator = true)
	{
		return FParse::Value(FCommandLine::Get(), Name, OutValue, bStopOnSeparator) && !OutValue.IsEmpty();
	}

	bool IsFixedViewToken(const FString& Value)
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

	TArray<TSharedPtr<FJsonValue>> JsonVector(const FVector& Value)
	{
		return {
			MakeShared<FJsonValueNumber>(Value.X),
			MakeShared<FJsonValueNumber>(Value.Y),
			MakeShared<FJsonValueNumber>(Value.Z)};
	}

	TArray<TSharedPtr<FJsonValue>> JsonRotator(const FRotator& Value)
	{
		return {
			MakeShared<FJsonValueNumber>(Value.Pitch),
			MakeShared<FJsonValueNumber>(Value.Yaw),
			MakeShared<FJsonValueNumber>(Value.Roll)};
	}

	TArray<TSharedPtr<FJsonValue>> DirectComponentReferences(const AActor& Actor)
	{
		TArray<TSharedPtr<FJsonValue>> Components;
		TInlineComponentArray<UActorComponent*> ActorComponents;
		Actor.GetComponents(ActorComponents);
		for (const UActorComponent* Component : ActorComponents)
		{
			if (Component == nullptr)
			{
				continue;
			}
			TSet<FString> References;
			for (TFieldIterator<FObjectPropertyBase> Property(Component->GetClass()); Property; ++Property)
			{
				const UObject* Value = Property->GetObjectPropertyValue_InContainer(Component);
				if (Value != nullptr && Value->IsAsset())
				{
					References.Add(Value->GetPathName());
				}
			}
			TArray<FString> SortedReferences = References.Array();
			SortedReferences.Sort();
			TArray<TSharedPtr<FJsonValue>> ReferenceValues;
			for (const FString& Reference : SortedReferences)
			{
				ReferenceValues.Add(MakeShared<FJsonValueString>(Reference));
			}
			TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
			Record->SetStringField(TEXT("name"), Component->GetName());
			Record->SetStringField(TEXT("class"), Component->GetClass()->GetPathName());
			Record->SetBoolField(TEXT("registered"), Component->IsRegistered());
			if (const USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				Record->SetBoolField(TEXT("visible"), SceneComponent->IsVisible());
				Record->SetArrayField(TEXT("world_location_cm"), JsonVector(SceneComponent->GetComponentLocation()));
			}
			if (const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Record->SetBoolField(TEXT("hidden_in_game"), Primitive->bHiddenInGame);
				Record->SetBoolField(TEXT("should_render"), Primitive->ShouldRender());
				Record->SetArrayField(TEXT("bounds_origin_cm"), JsonVector(Primitive->Bounds.Origin));
				Record->SetArrayField(TEXT("bounds_extent_cm"), JsonVector(Primitive->Bounds.BoxExtent));
			}
			Record->SetArrayField(TEXT("asset_references"), ReferenceValues);
			Components.Add(MakeShared<FJsonValueObject>(Record));
		}
		return Components;
	}
}

bool ProjectWorldFixedViewContract::ParseVector(const FString& Text, FVector& OutValue)
{
	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT(","), true);
	return Parts.Num() == 3 && LexTryParseString(OutValue.X, *Parts[0]) &&
		LexTryParseString(OutValue.Y, *Parts[1]) && LexTryParseString(OutValue.Z, *Parts[2]) &&
		!OutValue.ContainsNaN();
}

bool ProjectWorldFixedViewContract::ValidateConfig(
	const FProjectWorldFixedViewConfig& Config,
	FString& OutError)
{
	if (!IsFixedViewToken(Config.OperationId) || FPaths::IsRelative(Config.ResultPath) ||
		FPaths::IsRelative(Config.ScreenshotPath) || !Config.MapPackage.StartsWith(TEXT("/")) ||
		!Config.SubjectClassPath.StartsWith(TEXT("/Script/")) || Config.PlayerLocation.ContainsNaN() ||
		Config.LookAtLocation.ContainsNaN() || Config.SubjectLocation.ContainsNaN() ||
		Config.PlayerLocation.Equals(Config.LookAtLocation) || Config.SubjectToleranceCentimeters <= 0.0f)
	{
		OutError = TEXT("The fixed-view identity, paths, map, class, or coordinates are invalid.");
		return false;
	}
	return true;
}

FProjectWorldFixedViewGate::~FProjectWorldFixedViewGate()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
}

void FProjectWorldFixedViewGate::StartIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldFixedViewGate")))
	{
		return;
	}
	FString Error;
	if (!ParseConfig(Error))
	{
		FinishRejected(TEXT("fixed_view_config_invalid"), Error);
		return;
	}
	GateStartedSeconds = FPlatformTime::Seconds();
	SetPhase(EPhase::WaitingForWorld);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FProjectWorldFixedViewGate::Tick));
}

bool FProjectWorldFixedViewGate::ParseConfig(FString& OutError)
{
	FString PlayerText;
	FString LookAtText;
	FString SubjectText;
	if (!ParseRequiredValue(TEXT("ProjectWorldFixedViewOperation="), Config.OperationId) ||
		!ParseRequiredValue(TEXT("ProjectWorldFixedViewResult="), Config.ResultPath) ||
		!ParseRequiredValue(TEXT("ProjectWorldFixedViewScreenshot="), Config.ScreenshotPath) ||
		!ParseRequiredValue(TEXT("ProjectWorldFixedViewMap="), Config.MapPackage) ||
		!ParseRequiredValue(TEXT("ProjectWorldFixedViewSubjectClass="), Config.SubjectClassPath) ||
		!ParseRequiredValue(TEXT("ProjectWorldFixedViewPlayer="), PlayerText, false) ||
		!ParseRequiredValue(TEXT("ProjectWorldFixedViewLookAt="), LookAtText, false) ||
		!ParseRequiredValue(TEXT("ProjectWorldFixedViewSubject="), SubjectText, false) ||
		!FParse::Value(FCommandLine::Get(), TEXT("ProjectWorldFixedViewTolerance="), Config.SubjectToleranceCentimeters) ||
		!ProjectWorldFixedViewContract::ParseVector(PlayerText, Config.PlayerLocation) ||
		!ProjectWorldFixedViewContract::ParseVector(LookAtText, Config.LookAtLocation) ||
		!ProjectWorldFixedViewContract::ParseVector(SubjectText, Config.SubjectLocation))
	{
		OutError = TEXT("A required fixed-view argument is missing or malformed.");
		return false;
	}
	FPaths::NormalizeFilename(Config.ResultPath);
	FPaths::NormalizeFilename(Config.ScreenshotPath);
	return ProjectWorldFixedViewContract::ValidateConfig(Config, OutError);
}

bool FProjectWorldFixedViewGate::Tick(float DeltaSeconds)
{
	if (Phase == EPhase::Finished)
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - GateStartedSeconds > GateTimeoutSeconds)
	{
		FinishRejected(TEXT("fixed_view_timeout"), TEXT("The packaged fixed-view proof exceeded its bounded timeout."));
		return false;
	}
	if (Phase == EPhase::WaitingForWorld)
	{
		FString Error;
		if (TryAcquireWorld(Error))
		{
			SetPhase(EPhase::WaitingForStreaming);
		}
		else if (Now - PhaseStartedSeconds > FixedViewStreamingTimeoutSeconds)
		{
			FinishRejected(TEXT("fixed_view_world_unavailable"), Error);
		}
		return Phase != EPhase::Finished;
	}
	if (!ProductWorld.IsValid() || !PlayerController.IsValid())
	{
		FinishRejected(TEXT("fixed_view_ownership_lost"), TEXT("The packaged world or player controller became unavailable."));
		return false;
	}
	if (Phase == EPhase::WaitingForStreaming)
	{
		StableStreamingFrames = IsStreamingComplete() ? StableStreamingFrames + 1 : 0;
		if (StableStreamingFrames >= RequiredStableStreamingFrames)
		{
			SubjectActor = FindSubject();
			if (!SubjectActor.IsValid())
			{
				FinishRejected(TEXT("fixed_view_subject_missing"), TEXT("No matching subject actor exists at the requested location."));
				return false;
			}
			SetPhase(EPhase::Settling);
		}
		else if (Now - PhaseStartedSeconds > FixedViewStreamingTimeoutSeconds)
		{
			FinishRejected(TEXT("fixed_view_streaming_timeout"), TEXT("World Partition did not settle around the requested vantage."));
		}
		return Phase != EPhase::Finished;
	}
	if (Phase == EPhase::Settling && StableStreamingFrames++ >= RequiredSettleFrames)
	{
		FString Error;
		if (!Capture(Error))
		{
			FinishRejected(TEXT("fixed_view_capture_failed"), Error);
		}
		else
		{
			FinishAccepted();
		}
	}
	return Phase != EPhase::Finished;
}

bool FProjectWorldFixedViewGate::TryAcquireWorld(FString& OutError)
{
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		OutError = TEXT("The packaged game viewport is not ready.");
		return false;
	}
	UWorld* World = GEngine->GameViewport->GetWorld();
	if (World == nullptr || !World->IsGameWorld() || World->GetPackage()->GetName() != Config.MapPackage)
	{
		OutError = TEXT("The normal menu/loading route has not entered the requested map.");
		return false;
	}
	if (FCString::Strcmp(World->URL.GetOption(TEXT("ProjectLoadingRoute="), TEXT("")), TEXT("1")) != 0)
	{
		OutError = TEXT("The requested map was not entered through ProjectLoading.");
		return false;
	}
	APlayerController* Controller = World->GetFirstPlayerController();
	ACharacter* Character = Controller == nullptr ? nullptr : Cast<ACharacter>(Controller->GetPawn());
	if (Controller == nullptr || Character == nullptr || Controller->PlayerCameraManager == nullptr)
	{
		OutError = TEXT("The normal possessed character and camera are not ready.");
		return false;
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);
	}
	if (!Character->SetActorLocation(Config.PlayerLocation, false, nullptr, ETeleportType::TeleportPhysics))
	{
		OutError = TEXT("The player could not move to the requested fixed-view location.");
		return false;
	}
	Controller->SetControlRotation((Config.LookAtLocation - Config.PlayerLocation).Rotation());
	ProductWorld = World;
	PlayerController = Controller;
	return true;
}

bool FProjectWorldFixedViewGate::IsStreamingComplete() const
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

AActor* FProjectWorldFixedViewGate::FindSubject() const
{
	UWorld* World = ProductWorld.Get();
	AActor* Nearest = nullptr;
	double NearestDistanceSquared = FMath::Square(Config.SubjectToleranceCentimeters);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass()->GetPathName() != Config.SubjectClassPath)
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(It->GetActorLocation(), Config.SubjectLocation);
		if (DistanceSquared <= NearestDistanceSquared)
		{
			Nearest = *It;
			NearestDistanceSquared = DistanceSquared;
		}
	}
	return Nearest;
}

bool FProjectWorldFixedViewGate::Capture(FString& OutError)
{
	APlayerCameraManager* Camera = PlayerController->PlayerCameraManager;
	ActualCameraLocation = Camera->GetCameraLocation();
	ActualCameraRotation = Camera->GetCameraRotation();
	ProjectWorldRuntimeScreenshotCapture::FCaptureSpec Spec;
	Spec.CameraLocation = ActualCameraLocation;
	Spec.CameraRotation = ActualCameraRotation;
	Spec.SourceIdentity = TEXT("packaged_fixed_view");
	ProjectWorldRuntimeScreenshotCapture::FCaptureSession Session;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Config.ScreenshotPath), true);
	IFileManager::Get().Delete(*Config.ScreenshotPath, false, true, true);
	return Session.Initialize(*ProductWorld.Get(), Spec, OutError) &&
		Session.WarmUp(3, OutError) && Session.Capture(Config.ScreenshotPath, OutError);
}

void FProjectWorldFixedViewGate::FinishAccepted()
{
	WriteResult(TEXT("accepted"), FString(), FString());
	SetPhase(EPhase::Finished);
	FPlatformMisc::RequestExitWithStatus(false, 0, TEXT("ProjectWorldFixedViewGate.Accepted"));
}

void FProjectWorldFixedViewGate::FinishRejected(const FString& Code, const FString& Message)
{
	WriteResult(TEXT("rejected"), Code, Message);
	SetPhase(EPhase::Finished);
	UE_LOG(LogProjectWorldFixedViewGate, Error, TEXT("Fixed-view proof rejected - code=%s message=%s"), *Code, *Message);
	FPlatformMisc::RequestExitWithStatus(false, 9, TEXT("ProjectWorldFixedViewGate.Rejected"));
}

void FProjectWorldFixedViewGate::WriteResult(
	const FString& Status,
	const FString& ErrorCode,
	const FString& ErrorMessage) const
{
	if (Config.ResultPath.IsEmpty())
	{
		return;
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("contract"), TEXT("project-world-packaged-fixed-view-v1"));
	Root->SetStringField(TEXT("status"), Status);
	Root->SetStringField(TEXT("operation_id"), Config.OperationId);
	Root->SetStringField(TEXT("map_package"), Config.MapPackage);
	Root->SetStringField(TEXT("screenshot"), Config.ScreenshotPath);
	Root->SetStringField(TEXT("error_code"), ErrorCode);
	Root->SetStringField(TEXT("error_message"), ErrorMessage);
	Root->SetArrayField(TEXT("requested_player_location_cm"), JsonVector(Config.PlayerLocation));
	Root->SetArrayField(TEXT("requested_look_at_location_cm"), JsonVector(Config.LookAtLocation));
	Root->SetArrayField(TEXT("actual_camera_location_cm"), JsonVector(ActualCameraLocation));
	Root->SetArrayField(TEXT("actual_camera_rotation_deg"), JsonRotator(ActualCameraRotation));
	if (SubjectActor.IsValid())
	{
		Root->SetStringField(TEXT("subject_actor"), SubjectActor->GetName());
		Root->SetStringField(TEXT("subject_class"), SubjectActor->GetClass()->GetPathName());
		Root->SetBoolField(TEXT("subject_hidden"), SubjectActor->IsHidden());
		Root->SetArrayField(TEXT("subject_location_cm"), JsonVector(SubjectActor->GetActorLocation()));
		Root->SetArrayField(TEXT("subject_components"), DirectComponentReferences(*SubjectActor.Get()));
	}
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Config.ResultPath), true);
	FFileHelper::SaveStringToFile(Json, *Config.ResultPath);
}

void FProjectWorldFixedViewGate::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStartedSeconds = FPlatformTime::Seconds();
	StableStreamingFrames = 0;
}
