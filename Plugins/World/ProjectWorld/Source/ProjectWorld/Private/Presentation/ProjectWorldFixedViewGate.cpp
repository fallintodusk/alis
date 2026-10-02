// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldFixedViewGate.h"

#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"

#include "Components/ActorComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	bool ApplyScalarOverrides(
		UMaterialInstanceDynamic* DynamicMaterial,
		const TArray<FProjectWorldFixedViewScalarOverride>& Overrides,
		FString& OutError)
	{
		if (DynamicMaterial == nullptr)
		{
			OutError = TEXT("The transient material instance could not be created.");
			return false;
		}
		for (const FProjectWorldFixedViewScalarOverride& Override : Overrides)
		{
			const FName ParameterName(*Override.ParameterName);
			DynamicMaterial->SetScalarParameterValue(ParameterName, Override.Value);
			if (!FMath::IsNearlyEqual(
				DynamicMaterial->K2_GetScalarParameterValue(ParameterName), Override.Value, UE_SMALL_NUMBER))
			{
				OutError = FString::Printf(
					TEXT("The transient material did not retain scalar parameter %s."), *Override.ParameterName);
				return false;
			}
		}
		return true;
	}

	bool MaterialContainsPath(const UMaterialInterface* Material, const FString& TargetPath)
	{
		for (const UMaterialInterface* Current = Material; Current != nullptr;)
		{
			if (Current->GetPathName() == TargetPath)
			{
				return true;
			}
			const UMaterialInstance* Instance = Cast<UMaterialInstance>(Current);
			Current = Instance != nullptr ? Instance->Parent : nullptr;
		}
		return false;
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
				Record->SetArrayField(TEXT("world_scale_xyz"), JsonVector(SceneComponent->GetComponentScale()));
			}
			if (const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Record->SetBoolField(TEXT("hidden_in_game"), Primitive->bHiddenInGame);
				Record->SetBoolField(TEXT("should_render"), Primitive->ShouldRender());
				Record->SetArrayField(TEXT("bounds_origin_cm"), JsonVector(Primitive->Bounds.Origin));
				Record->SetArrayField(TEXT("bounds_extent_cm"), JsonVector(Primitive->Bounds.BoxExtent));
			}
			if (const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
			{
				if (const UStaticMesh* Mesh = MeshComponent->GetStaticMesh())
				{
					const FBox LocalBounds = Mesh->GetBoundingBox();
					Record->SetStringField(TEXT("static_mesh"), Mesh->GetPathName());
					Record->SetArrayField(TEXT("local_bounds_size_cm"), JsonVector(LocalBounds.GetSize()));
					Record->SetArrayField(TEXT("local_bounds_extent_cm"), JsonVector(LocalBounds.GetExtent()));
				}
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

bool ProjectWorldFixedViewContract::ParseScalarOverrides(
	const FString& Text,
	TArray<FProjectWorldFixedViewScalarOverride>& OutOverrides,
	FString& OutError)
{
	OutOverrides.Reset();
	TArray<FString> Entries;
	Text.ParseIntoArray(Entries, TEXT(";"), true);
	TSet<FName> ParameterNames;
	for (const FString& Entry : Entries)
	{
		TArray<FString> Parts;
		Entry.ParseIntoArray(Parts, TEXT(":"), false);
		FProjectWorldFixedViewScalarOverride Override;
		if (Parts.Num() != 2 || !IsFixedViewToken(Parts[0]) ||
			!LexTryParseString(Override.Value, *Parts[1]) || !FMath::IsFinite(Override.Value))
		{
			OutOverrides.Reset();
			OutError = TEXT("Each scalar override must be a finite ParameterName:Value pair.");
			return false;
		}
		Override.ParameterName = MoveTemp(Parts[0]);
		const FName ParameterKey(*Override.ParameterName);
		if (ParameterNames.Contains(ParameterKey))
		{
			OutOverrides.Reset();
			OutError = FString::Printf(TEXT("Scalar parameter %s is specified more than once."), *Override.ParameterName);
			return false;
		}
		ParameterNames.Add(ParameterKey);
		OutOverrides.Add(MoveTemp(Override));
	}
	if (OutOverrides.IsEmpty())
	{
		OutError = TEXT("At least one scalar override is required.");
		return false;
	}
	return true;
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
	if (Config.HasFixture() &&
		(Config.FixtureMeshPath.IsEmpty() || Config.FixtureMaterialPath.IsEmpty() ||
		 !Config.FixtureMeshPath.StartsWith(TEXT("/")) ||
		 !Config.FixtureMaterialPath.StartsWith(TEXT("/")) ||
		 Config.SubjectClassPath != TEXT("/Script/Engine.StaticMeshActor") ||
		 Config.FixtureStartLocation.ContainsNaN() || Config.FixtureStartRotation.ContainsNaN() ||
		 Config.FixtureFinalRotation.ContainsNaN() || Config.FixtureScale.ContainsNaN() ||
		 !FMath::IsFinite(Config.FixtureScale.X) || !FMath::IsFinite(Config.FixtureScale.Y) ||
		 !FMath::IsFinite(Config.FixtureScale.Z) || Config.FixtureScale.X <= 0.0 ||
		 Config.FixtureScale.Y <= 0.0 || Config.FixtureScale.Z <= 0.0))
	{
		OutError = TEXT("The optional fixed-view fixture contract is incomplete or invalid.");
		return false;
	}
	if (Config.HasScalarOverride() && Config.ScalarTargetMaterialPath.IsEmpty() &&
		!Config.HasFixture())
	{
		OutError = TEXT("A scalar override requires either a fixture or target material.");
		return false;
	}
	TSet<FName> ScalarParameterNames;
	for (const FProjectWorldFixedViewScalarOverride& Override : Config.ScalarOverrides)
	{
		const FName ParameterName(*Override.ParameterName);
		if (!IsFixedViewToken(Override.ParameterName) || !FMath::IsFinite(Override.Value) ||
			ScalarParameterNames.Contains(ParameterName))
		{
			OutError = TEXT("Scalar overrides must have unique parameter names and finite values.");
			return false;
		}
		ScalarParameterNames.Add(ParameterName);
	}
	if (!Config.ScalarTargetMaterialPath.IsEmpty() &&
		(!Config.ScalarTargetMaterialPath.StartsWith(TEXT("/")) ||
		 !Config.HasScalarOverride() || !FMath::IsFinite(Config.ScalarTargetRadiusCentimeters) ||
		 Config.ScalarTargetRadiusCentimeters <= 0.0f))
	{
		OutError = TEXT("The world material scalar override target or search radius is invalid.");
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
	Config.bIsolateBaseColor = FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldFixedViewBaseColor"));
	FParse::Value(FCommandLine::Get(), TEXT("ProjectWorldFixedViewFixtureMesh="), Config.FixtureMeshPath);
	FParse::Value(FCommandLine::Get(), TEXT("ProjectWorldFixedViewFixtureMaterial="), Config.FixtureMaterialPath);
	FString FixtureScaleText;
	FParse::Value(FCommandLine::Get(), TEXT("ProjectWorldFixedViewFixtureScale="), FixtureScaleText, false);
	if (!FixtureScaleText.IsEmpty() &&
		!ProjectWorldFixedViewContract::ParseVector(FixtureScaleText, Config.FixtureScale))
	{
		OutError = TEXT("The optional fixture scale is malformed.");
		return false;
	}
	FString ScalarOverridesText;
	if (FParse::Value(FCommandLine::Get(), TEXT("ProjectWorldFixedViewScalars="), ScalarOverridesText, false) &&
		!ProjectWorldFixedViewContract::ParseScalarOverrides(ScalarOverridesText, Config.ScalarOverrides, OutError))
	{
		return false;
	}
	FParse::Value(FCommandLine::Get(), TEXT("ProjectWorldFixedViewScalarMaterial="), Config.ScalarTargetMaterialPath);
	FParse::Value(FCommandLine::Get(), TEXT("ProjectWorldFixedViewScalarRadius="), Config.ScalarTargetRadiusCentimeters);
	if (Config.HasFixture())
	{
		FString FixtureStartText;
		FString FixtureStartRotationText;
		FString FixtureFinalRotationText;
		FVector FixtureStartRotation;
		FVector FixtureFinalRotation;
		if (!ParseRequiredValue(TEXT("ProjectWorldFixedViewFixtureStart="), FixtureStartText, false) ||
			!ParseRequiredValue(TEXT("ProjectWorldFixedViewFixtureStartRotation="), FixtureStartRotationText, false) ||
			!ParseRequiredValue(TEXT("ProjectWorldFixedViewFixtureFinalRotation="), FixtureFinalRotationText, false) ||
			!ProjectWorldFixedViewContract::ParseVector(FixtureStartText, Config.FixtureStartLocation) ||
			!ProjectWorldFixedViewContract::ParseVector(FixtureStartRotationText, FixtureStartRotation) ||
			!ProjectWorldFixedViewContract::ParseVector(FixtureFinalRotationText, FixtureFinalRotation))
		{
			OutError = TEXT("The optional fixed-view fixture transforms are missing or malformed.");
			return false;
		}
		Config.FixtureStartRotation = FRotator(
			FixtureStartRotation.X, FixtureStartRotation.Y, FixtureStartRotation.Z);
		Config.FixtureFinalRotation = FRotator(
			FixtureFinalRotation.X, FixtureFinalRotation.Y, FixtureFinalRotation.Z);
	}
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
			if (Config.HasFixture() && !SubjectActor.IsValid())
			{
				FString Error;
				if (!SpawnFixture(Error))
				{
					FinishRejected(TEXT("fixed_view_fixture_failed"), Error);
					return false;
				}
			}
			if (!SubjectActor.IsValid())
			{
				SubjectActor = FindSubject();
			}
			if (!SubjectActor.IsValid())
			{
				FinishRejected(TEXT("fixed_view_subject_missing"), TEXT("No matching subject actor exists at the requested location."));
				return false;
			}
			FString Error;
			if (!ApplyFixtureFinalTransform(Error))
			{
				FinishRejected(TEXT("fixed_view_fixture_transform_failed"), Error);
				return false;
			}
			if (!ApplyScalarOverride(Error))
			{
				FinishRejected(TEXT("fixed_view_scalar_override_failed"), Error);
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

bool FProjectWorldFixedViewGate::SpawnFixture(FString& OutError)
{
	UWorld* World = ProductWorld.Get();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Config.FixtureMeshPath);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Config.FixtureMaterialPath);
	if (World == nullptr || Mesh == nullptr || Material == nullptr)
	{
		OutError = TEXT("The requested fixture mesh or material could not be loaded.");
		return false;
	}
	FActorSpawnParameters Parameters;
	Parameters.Name = TEXT("ProjectWorldFixedViewFixture");
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Fixture = World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), Config.FixtureStartLocation,
		Config.FixtureStartRotation, Parameters);
	if (Fixture == nullptr || Fixture->GetStaticMeshComponent() == nullptr)
	{
		OutError = TEXT("The fixed-view fixture actor could not be spawned.");
		return false;
	}
	Fixture->SetMobility(EComponentMobility::Movable);
	Fixture->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	if (Config.HasScalarOverride() && Config.ScalarTargetMaterialPath.IsEmpty())
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, Fixture);
		if (!ApplyScalarOverrides(DynamicMaterial, Config.ScalarOverrides, OutError))
		{
			return false;
		}
		Fixture->GetStaticMeshComponent()->SetMaterial(0, DynamicMaterial);
	}
	else
	{
		Fixture->GetStaticMeshComponent()->SetMaterial(0, Material);
	}
	SubjectActor = Fixture;
	return true;
}

bool FProjectWorldFixedViewGate::ApplyFixtureFinalTransform(FString& OutError)
{
	if (!Config.HasFixture() || bFixtureFinalTransformApplied)
	{
		return true;
	}
	AActor* Fixture = SubjectActor.Get();
	if (Fixture == nullptr || !Fixture->SetActorLocationAndRotation(
		Config.SubjectLocation, Config.FixtureFinalRotation, false, nullptr,
		ETeleportType::TeleportPhysics))
	{
		OutError = TEXT("The fixed-view fixture could not apply its final transform.");
		return false;
	}
	Fixture->SetActorScale3D(Config.FixtureScale);
	bFixtureFinalTransformApplied = true;
	return true;
}

bool FProjectWorldFixedViewGate::ApplyScalarOverride(FString& OutError)
{
	if (Config.ScalarTargetMaterialPath.IsEmpty())
	{
		return true;
	}
	UWorld* World = ProductWorld.Get();
	if (World == nullptr)
	{
		OutError = TEXT("The scalar override world is unavailable.");
		return false;
	}
	const double RadiusSquared = FMath::Square(static_cast<double>(Config.ScalarTargetRadiusCentimeters));
	TSet<FString> AvailableMaterialPaths;
	TSet<UPrimitiveComponent*> OverriddenComponents;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UActorComponent*> Components;
		ActorIt->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (Primitive == nullptr || !Primitive->ShouldRender() ||
				FVector::DistSquared(Primitive->GetComponentLocation(), Config.SubjectLocation) > RadiusSquared)
			{
				continue;
			}
			for (int32 MaterialIndex = 0; MaterialIndex < Primitive->GetNumMaterials(); ++MaterialIndex)
			{
				UMaterialInterface* Material = Primitive->GetMaterial(MaterialIndex);
				if (Material == nullptr)
				{
					continue;
				}
				for (const UMaterialInterface* Current = Material; Current != nullptr;)
				{
					AvailableMaterialPaths.Add(Current->GetPathName());
					const UMaterialInstance* Instance = Cast<UMaterialInstance>(Current);
					Current = Instance != nullptr ? Instance->Parent : nullptr;
				}
				if (!MaterialContainsPath(Material, Config.ScalarTargetMaterialPath))
				{
					continue;
				}
				UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(Material, Primitive);
				if (!ApplyScalarOverrides(DynamicMaterial, Config.ScalarOverrides, OutError))
				{
					return false;
				}
				Primitive->SetMaterial(MaterialIndex, DynamicMaterial);
				OverriddenComponents.Add(Primitive);
				++ScalarOverrideSlotCount;
			}
		}
	}
	if (ScalarOverrideSlotCount == 0)
	{
		TArray<FString> AvailablePaths = AvailableMaterialPaths.Array();
		AvailablePaths.Sort();
		AvailablePaths.SetNum(FMath::Min(AvailablePaths.Num(), 8));
		const FString PathSample = FString::Join(AvailablePaths, TEXT(", "));
		OutError = FString::Printf(
			TEXT("No rendered material slot matched %s within %.0f cm of the requested view. Observed: %s"),
			*Config.ScalarTargetMaterialPath, Config.ScalarTargetRadiusCentimeters, *PathSample);
		return false;
	}
	ScalarOverrideComponentCount = OverriddenComponents.Num();
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
	Spec.bIsolateBaseColor = Config.bIsolateBaseColor;
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
	Root->SetBoolField(TEXT("base_color_isolated"), Config.bIsolateBaseColor);
	if (Config.HasScalarOverride())
	{
		Root->SetStringField(TEXT("scalar_override_source"), TEXT("fixed_view_gate"));
		Root->SetStringField(TEXT("scalar_target_material"), Config.ScalarTargetMaterialPath);
		Root->SetNumberField(TEXT("scalar_override_component_count"), ScalarOverrideComponentCount);
		Root->SetNumberField(TEXT("scalar_override_slot_count"), ScalarOverrideSlotCount);
		TArray<TSharedPtr<FJsonValue>> ScalarValues;
		for (const FProjectWorldFixedViewScalarOverride& Override : Config.ScalarOverrides)
		{
			TSharedRef<FJsonObject> ScalarValue = MakeShared<FJsonObject>();
			ScalarValue->SetStringField(TEXT("parameter"), Override.ParameterName);
			ScalarValue->SetNumberField(TEXT("value"), Override.Value);
			ScalarValues.Add(MakeShared<FJsonValueObject>(ScalarValue));
		}
		Root->SetArrayField(TEXT("scalar_overrides"), ScalarValues);
		if (!Config.ScalarTargetMaterialPath.IsEmpty())
		{
			Root->SetNumberField(TEXT("scalar_target_radius_cm"), Config.ScalarTargetRadiusCentimeters);
		}
	}
	if (Config.HasFixture())
	{
		Root->SetStringField(TEXT("fixture_mesh"), Config.FixtureMeshPath);
		Root->SetStringField(TEXT("fixture_material"), Config.FixtureMaterialPath);
		Root->SetArrayField(TEXT("fixture_start_location_cm"), JsonVector(Config.FixtureStartLocation));
		Root->SetArrayField(TEXT("fixture_start_rotation_deg"), JsonRotator(Config.FixtureStartRotation));
		Root->SetArrayField(TEXT("fixture_final_location_cm"), JsonVector(Config.SubjectLocation));
		Root->SetArrayField(TEXT("fixture_final_rotation_deg"), JsonRotator(Config.FixtureFinalRotation));
		Root->SetArrayField(TEXT("fixture_scale_xyz"), JsonVector(Config.FixtureScale));
		Root->SetBoolField(TEXT("fixture_moved"), !Config.FixtureStartLocation.Equals(Config.SubjectLocation));
		Root->SetBoolField(TEXT("fixture_rotated"), !Config.FixtureStartRotation.Equals(Config.FixtureFinalRotation));
	}
	if (SubjectActor.IsValid())
	{
		Root->SetStringField(TEXT("subject_actor"), SubjectActor->GetName());
		Root->SetStringField(TEXT("subject_class"), SubjectActor->GetClass()->GetPathName());
		Root->SetBoolField(TEXT("subject_hidden"), SubjectActor->IsHidden());
		Root->SetArrayField(TEXT("subject_location_cm"), JsonVector(SubjectActor->GetActorLocation()));
		Root->SetArrayField(TEXT("subject_scale_xyz"), JsonVector(SubjectActor->GetActorScale3D()));
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
