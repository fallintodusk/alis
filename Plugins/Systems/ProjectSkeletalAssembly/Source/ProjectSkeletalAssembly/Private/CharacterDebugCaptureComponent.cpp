// Copyright ALIS. All Rights Reserved.

#include "CharacterDebugCaptureComponent.h"
#include "ProjectSkeletalAssemblyModule.h"
#include "SkeletalAssemblyComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h"
#include "UnrealClient.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"

namespace
{
	// Key bones for first-person parity comparison
	const TArray<FName> DiagnosticBones = {
		FName("root"),
		FName("pelvis"),
		FName("spine_03"),
		FName("spine_05"),
		FName("neck_01"),
		FName("head")
	};

	FString TransformToCompactString(const FTransform& T)
	{
		const FVector& L = T.GetLocation();
		const FRotator R = T.GetRotation().Rotator();
		return FString::Printf(TEXT("L=(%.1f,%.1f,%.1f) R=(%.1f,%.1f,%.1f)"),
			L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll);
	}
}

UCharacterDebugCaptureComponent::UCharacterDebugCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCharacterDebugCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogSkeletalAssembly, Verbose,
		TEXT("[%s] CharacterDebugCapture ready"),
		*GetOwner()->GetName());
}

void UCharacterDebugCaptureComponent::SetOverlayEnabled(bool bEnabled)
{
	bShowOverlay = bEnabled;
	SetComponentTickEnabled(bEnabled);

	UE_LOG(LogSkeletalAssembly, Log,
		TEXT("[%s] Debug overlay %s"),
		*GetOwner()->GetName(),
		bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void UCharacterDebugCaptureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bShowOverlay || !GEngine)
	{
		return;
	}

	TSharedPtr<FJsonObject> State = CollectDiagnosticState();
	FString OverlayText = FormatOverlayText(State);
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, OverlayText);
}

void UCharacterDebugCaptureComponent::CaptureSnapshot(const FString& Label)
{
	TSharedPtr<FJsonObject> State = CollectDiagnosticState();
	if (!State.IsValid())
	{
		UE_LOG(LogSkeletalAssembly, Warning, TEXT("CaptureSnapshot: failed to collect state"));
		return;
	}

	// Build filename
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S"));
	const FString SystemId = State->GetStringField(TEXT("system"));
	const FString PawnClass = State->GetStringField(TEXT("pawnClass"));
	const FString BaseName = Label.IsEmpty()
		? FString::Printf(TEXT("%s_%s_%s"), *Timestamp, *SystemId, *PawnClass)
		: FString::Printf(TEXT("%s_%s_%s_%s"), *Timestamp, *SystemId, *PawnClass, *Label);

	const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("Validation") / TEXT("CharacterDebug");
	IFileManager::Get().MakeDirectory(*OutputDir, true);

	// Write JSON sidecar
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(State.ToSharedRef(), Writer);

	const FString JsonPath = OutputDir / (BaseName + TEXT(".json"));
	FFileHelper::SaveStringToFile(JsonString, *JsonPath);

	// Capture screenshot from detached debug viewpoint behind/above the character.
	// Uses SceneCaptureComponent2D to render from a known offset without
	// affecting the player's view. The character continues ticking normally.
	const FString ScreenshotPath = OutputDir / (BaseName + TEXT(".png"));
	{
		AActor* OwnerActor = GetOwner();
		const FVector CharLocation = OwnerActor->GetActorLocation();
		const FRotator CharRotation = OwnerActor->GetActorRotation();
		const FVector Offset = CharRotation.RotateVector(FVector(-300.f, 0.f, 150.f));
		const FVector CaptureLocation = CharLocation + Offset;
		const FRotator CaptureRotation = (CharLocation + FVector(0, 0, 50) - CaptureLocation).Rotation();

		USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(OwnerActor);
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->SetWorldLocationAndRotation(CaptureLocation, CaptureRotation);

		const int32 Width = 1280;
		const int32 Height = 720;

		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
		RenderTarget->InitAutoFormat(Width, Height);
		RenderTarget->ClearColor = FLinearColor::Black;
		Capture->TextureTarget = RenderTarget;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->RegisterComponent();
		Capture->CaptureScene();

		// Read pixels and save to PNG
		TArray<FColor> Pixels;
		FRenderTarget* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
		if (RTResource && RTResource->ReadPixels(Pixels))
		{
			TArray64<uint8> PngData;
			FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), PngData);
			FFileHelper::SaveArrayToFile(PngData, *ScreenshotPath);

			UE_LOG(LogSkeletalAssembly, Log,
				TEXT("CharacterDebugCapture: screenshot saved to %s"), *ScreenshotPath);
		}
		else
		{
			UE_LOG(LogSkeletalAssembly, Warning,
				TEXT("CharacterDebugCapture: failed to read render target pixels"));
		}

		Capture->DestroyComponent();
		RenderTarget->ConditionalBeginDestroy();
	}

	UE_LOG(LogSkeletalAssembly, Log,
		TEXT("CharacterDebugCapture: JSON saved to %s"), *JsonPath);

	// Console summary
	FString Summary = FormatOverlayText(State);
	UE_LOG(LogSkeletalAssembly, Log, TEXT("--- Capture Summary ---\n%s"), *Summary);
}

TSharedPtr<FJsonObject> UCharacterDebugCaptureComponent::CollectDiagnosticState() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

	// System identification
	ACharacter* Character = Cast<ACharacter>(Owner);
	const FString ClassName = Owner->GetClass()->GetName();

	// Detect system type from class name
	FString SystemId = TEXT("unknown");
	if (ClassName.Contains(TEXT("Definition")))
	{
		SystemId = TEXT("modular");
	}
	else if (ClassName.Contains(TEXT("ProjectCharacter")) || ClassName.Contains(TEXT("BP_Hero")))
	{
		SystemId = TEXT("legacy");
	}

	Root->SetStringField(TEXT("system"), SystemId);
	Root->SetStringField(TEXT("pawnClass"), ClassName);
	Root->SetStringField(TEXT("actorName"), Owner->GetName());

	// Assembly state
	if (USkeletalAssemblyComponent* Assembly = Owner->FindComponentByClass<USkeletalAssemblyComponent>())
	{
		Root->SetStringField(TEXT("assemblyState"), StaticEnum<ESkeletalAssemblyState>()->GetNameStringByValue(
			static_cast<int64>(Assembly->GetAssemblyState())));
	}
	else
	{
		Root->SetStringField(TEXT("assemblyState"), TEXT("none"));
	}

	// Capability list
	TArray<TSharedPtr<FJsonValue>> CapArray;
	TArray<UActorComponent*> AllComps;
	Owner->GetComponents(AllComps);
	for (UActorComponent* Comp : AllComps)
	{
		FPrimaryAssetId AssetId = Comp->GetPrimaryAssetId();
		if (AssetId.IsValid() && AssetId.PrimaryAssetType == FPrimaryAssetType(TEXT("CapabilityComponent")))
		{
			TSharedPtr<FJsonObject> CapObj = MakeShared<FJsonObject>();
			CapObj->SetStringField(TEXT("id"), AssetId.PrimaryAssetName.ToString());
			CapObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
			CapObj->SetBoolField(TEXT("active"), Comp->IsActive());
			CapArray.Add(MakeShared<FJsonValueObject>(CapObj));
		}
	}
	Root->SetArrayField(TEXT("capabilities"), CapArray);

	// Camera
	UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
	if (Camera)
	{
		TSharedPtr<FJsonObject> CamObj = MakeShared<FJsonObject>();
		CamObj->SetStringField(TEXT("parent"), Camera->GetAttachParent() ? Camera->GetAttachParent()->GetName() : TEXT("none"));
		CamObj->SetStringField(TEXT("socket"), Camera->GetAttachSocketName().ToString());
		CamObj->SetStringField(TEXT("worldTransform"), TransformToCompactString(Camera->GetComponentTransform()));
		CamObj->SetStringField(TEXT("relativeTransform"), TransformToCompactString(Camera->GetRelativeTransform()));
		CamObj->SetNumberField(TEXT("fov"), Camera->FieldOfView);
		CamObj->SetBoolField(TEXT("usePawnControlRotation"), Camera->bUsePawnControlRotation);
		Root->SetObjectField(TEXT("camera"), CamObj);
	}

	// Movement
	if (Character)
	{
		UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
		if (CMC)
		{
			TSharedPtr<FJsonObject> MoveObj = MakeShared<FJsonObject>();
			MoveObj->SetNumberField(TEXT("maxWalkSpeed"), CMC->MaxWalkSpeed);
			MoveObj->SetNumberField(TEXT("maxWalkSpeedCrouched"), CMC->MaxWalkSpeedCrouched);
			MoveObj->SetNumberField(TEXT("jumpZVelocity"), CMC->JumpZVelocity);
			MoveObj->SetNumberField(TEXT("groundFriction"), CMC->GroundFriction);
			MoveObj->SetNumberField(TEXT("brakingDeceleration"), CMC->BrakingDecelerationWalking);
			MoveObj->SetBoolField(TEXT("isFalling"), CMC->IsFalling());
			MoveObj->SetBoolField(TEXT("isCrouching"), Character->bIsCrouched);
			MoveObj->SetBoolField(TEXT("orientRotationToMovement"), CMC->bOrientRotationToMovement);
			MoveObj->SetBoolField(TEXT("useControllerDesiredRotation"), CMC->bUseControllerDesiredRotation);
			MoveObj->SetStringField(TEXT("velocity"), Character->GetVelocity().ToCompactString());
			Root->SetObjectField(TEXT("movement"), MoveObj);
		}

		// Capsule
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		if (Capsule)
		{
			TSharedPtr<FJsonObject> CapsuleObj = MakeShared<FJsonObject>();
			float Radius, HalfHeight;
			Capsule->GetScaledCapsuleSize(Radius, HalfHeight);
			CapsuleObj->SetNumberField(TEXT("radius"), Radius);
			CapsuleObj->SetNumberField(TEXT("halfHeight"), HalfHeight);
			Root->SetObjectField(TEXT("capsule"), CapsuleObj);
		}
	}

	// Skeletal meshes -- collect all
	TArray<USkeletalMeshComponent*> SkeletalMeshes;
	Owner->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);

	TArray<TSharedPtr<FJsonValue>> MeshArray;
	for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
	{
		TSharedPtr<FJsonObject> MeshObj = MakeShared<FJsonObject>();
		CollectMeshInfo(MeshObj, TEXT(""), Mesh);
		MeshArray.Add(MakeShared<FJsonValueObject>(MeshObj));
	}
	Root->SetArrayField(TEXT("meshes"), MeshArray);

	// Bone transforms from the primary mesh (for parity comparison)
	if (Character && Character->GetMesh())
	{
		CollectBoneTransforms(Root, Character->GetMesh());
	}

	// Per-role bone transforms for animation chain diagnostics.
	// Comparing DriverBody vs BodyCustomization bones tells us whether
	// LeaderPose is propagating animation from driver to visible mesh.
	{
		static const FName RoleTags[] = {
			FName(TEXT("AssemblyRole=DriverBody")),
			FName(TEXT("AssemblyRole=BodyCustomization")),
			FName(TEXT("AssemblyRole=HeadCustomization")),
			FName(TEXT("AssemblyRole=LocalBodyCustomization"))
		};
		static const TCHAR* RoleNames[] = {
			TEXT("driverBodyBones"), TEXT("bodyCustomizationBones"),
			TEXT("headCustomizationBones"), TEXT("localBodyCustomizationBones")
		};

		for (int32 i = 0; i < UE_ARRAY_COUNT(RoleTags); ++i)
		{
			for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
			{
				if (Mesh->ComponentTags.Contains(RoleTags[i]) && Mesh->GetSkeletalMeshAsset())
				{
					TSharedPtr<FJsonObject> RoleBones = MakeShared<FJsonObject>();
					for (const FName& BoneName : DiagnosticBones)
					{
						int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
						if (BoneIndex != INDEX_NONE)
						{
							RoleBones->SetStringField(BoneName.ToString(),
								TransformToCompactString(Mesh->GetBoneTransform(BoneIndex)));
						}
					}
					Root->SetObjectField(RoleNames[i], RoleBones);
					break;
				}
			}
		}
	}

	return Root;
}

void UCharacterDebugCaptureComponent::CollectMeshInfo(TSharedPtr<FJsonObject>& OutObj, const FString& Prefix, USkeletalMeshComponent* Mesh) const
{
	if (!Mesh)
	{
		return;
	}

	OutObj->SetStringField(TEXT("name"), Mesh->GetName());
	OutObj->SetStringField(TEXT("asset"), Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetName() : TEXT("null"));
	OutObj->SetStringField(TEXT("parent"), Mesh->GetAttachParent() ? Mesh->GetAttachParent()->GetName() : TEXT("root"));
	OutObj->SetStringField(TEXT("socket"), Mesh->GetAttachSocketName().ToString());
	OutObj->SetBoolField(TEXT("visible"), Mesh->IsVisible());
	OutObj->SetBoolField(TEXT("onlyOwnerSee"), Mesh->bOnlyOwnerSee);
	OutObj->SetBoolField(TEXT("ownerNoSee"), Mesh->bOwnerNoSee);

	// Animation
	if (Mesh->GetAnimInstance())
	{
		OutObj->SetStringField(TEXT("animClass"), Mesh->GetAnimInstance()->GetClass()->GetName());
	}
	else
	{
		OutObj->SetStringField(TEXT("animClass"), TEXT("none"));
	}

	// PostProcess AnimInstance
	if (UAnimInstance* PostProc = Mesh->GetPostProcessInstance())
	{
		OutObj->SetStringField(TEXT("postProcessAnimClass"), PostProc->GetClass()->GetName());
	}

	// LeaderPose
	if (Mesh->LeaderPoseComponent.IsValid())
	{
		OutObj->SetStringField(TEXT("leaderPose"), Mesh->LeaderPoseComponent->GetName());
	}

	// Tags (for assembly roles)
	FString TagsStr;
	for (const FName& Tag : Mesh->ComponentTags)
	{
		if (!TagsStr.IsEmpty()) TagsStr += TEXT(", ");
		TagsStr += Tag.ToString();
	}
	if (!TagsStr.IsEmpty())
	{
		OutObj->SetStringField(TEXT("tags"), TagsStr);
	}
}

void UCharacterDebugCaptureComponent::CollectBoneTransforms(TSharedPtr<FJsonObject>& OutObj, USkeletalMeshComponent* Mesh) const
{
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		return;
	}

	TSharedPtr<FJsonObject> BonesObj = MakeShared<FJsonObject>();
	for (const FName& BoneName : DiagnosticBones)
	{
		int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			FTransform BoneWorldTransform = Mesh->GetBoneTransform(BoneIndex);
			BonesObj->SetStringField(BoneName.ToString(), TransformToCompactString(BoneWorldTransform));
		}
	}

	OutObj->SetObjectField(TEXT("boneTransforms"), BonesObj);
}

FString UCharacterDebugCaptureComponent::FormatOverlayText(const TSharedPtr<FJsonObject>& State) const
{
	if (!State.IsValid())
	{
		return TEXT("No diagnostic state");
	}

	FString Text;

	// Header
	Text += FString::Printf(TEXT("=== Character Debug [%s] ===\n"),
		*State->GetStringField(TEXT("system")));
	Text += FString::Printf(TEXT("Pawn: %s | Assembly: %s\n"),
		*State->GetStringField(TEXT("pawnClass")),
		*State->GetStringField(TEXT("assemblyState")));

	// Capabilities
	const TArray<TSharedPtr<FJsonValue>>& Caps = State->GetArrayField(TEXT("capabilities"));
	Text += FString::Printf(TEXT("Capabilities (%d): "), Caps.Num());
	for (const auto& Cap : Caps)
	{
		const TSharedPtr<FJsonObject>& CapObj = Cap->AsObject();
		Text += FString::Printf(TEXT("%s(%s) "),
			*CapObj->GetStringField(TEXT("id")),
			CapObj->GetBoolField(TEXT("active")) ? TEXT("on") : TEXT("off"));
	}
	Text += TEXT("\n");

	// Camera
	if (State->HasField(TEXT("camera")))
	{
		const TSharedPtr<FJsonObject>& Cam = State->GetObjectField(TEXT("camera"));
		Text += FString::Printf(TEXT("Camera: parent=%s usePawnRot=%s fov=%.0f\n"),
			*Cam->GetStringField(TEXT("parent")),
			Cam->GetBoolField(TEXT("usePawnControlRotation")) ? TEXT("Y") : TEXT("N"),
			Cam->GetNumberField(TEXT("fov")));
		Text += FString::Printf(TEXT("  world: %s\n"), *Cam->GetStringField(TEXT("worldTransform")));
	}

	// Movement
	if (State->HasField(TEXT("movement")))
	{
		const TSharedPtr<FJsonObject>& Move = State->GetObjectField(TEXT("movement"));
		Text += FString::Printf(TEXT("Movement: walk=%.0f crouch=%.0f jump=%.0f orient=%s ctrlRot=%s\n"),
			Move->GetNumberField(TEXT("maxWalkSpeed")),
			Move->GetNumberField(TEXT("maxWalkSpeedCrouched")),
			Move->GetNumberField(TEXT("jumpZVelocity")),
			Move->GetBoolField(TEXT("orientRotationToMovement")) ? TEXT("Y") : TEXT("N"),
			Move->GetBoolField(TEXT("useControllerDesiredRotation")) ? TEXT("Y") : TEXT("N"));
	}

	// Meshes
	const TArray<TSharedPtr<FJsonValue>>& Meshes = State->GetArrayField(TEXT("meshes"));
	Text += FString::Printf(TEXT("Meshes (%d):\n"), Meshes.Num());
	for (const auto& MeshVal : Meshes)
	{
		const TSharedPtr<FJsonObject>& M = MeshVal->AsObject();
		Text += FString::Printf(TEXT("  %s: asset=%s parent=%s vis=%s ownerOnly=%s ownerNoSee=%s\n"),
			*M->GetStringField(TEXT("name")),
			*M->GetStringField(TEXT("asset")),
			*M->GetStringField(TEXT("parent")),
			M->GetBoolField(TEXT("visible")) ? TEXT("Y") : TEXT("N"),
			M->GetBoolField(TEXT("onlyOwnerSee")) ? TEXT("Y") : TEXT("N"),
			M->GetBoolField(TEXT("ownerNoSee")) ? TEXT("Y") : TEXT("N"));
		if (M->HasField(TEXT("leaderPose")))
		{
			Text += FString::Printf(TEXT("    leaderPose=%s\n"), *M->GetStringField(TEXT("leaderPose")));
		}
	}

	// Bones
	if (State->HasField(TEXT("boneTransforms")))
	{
		const TSharedPtr<FJsonObject>& Bones = State->GetObjectField(TEXT("boneTransforms"));
		Text += TEXT("Bones:\n");
		for (const auto& Pair : Bones->Values)
		{
			Text += FString::Printf(TEXT("  %s: %s\n"), *Pair.Key, *Pair.Value->AsString());
		}
	}

	return Text;
}
