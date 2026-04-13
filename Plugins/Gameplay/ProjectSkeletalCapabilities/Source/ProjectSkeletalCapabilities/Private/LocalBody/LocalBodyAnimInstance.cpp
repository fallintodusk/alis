// AnimInstance for LocalBodyMesh - thin orchestrator.
// Owns CopyPose, space conversion, spine yaw. Delegates correction to strategy.

#include "LocalBodyAnimInstance.h"
#include "LocalBodyDebug.h"
#include "ProjectSkeletalCapabilitiesModule.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

static FVector LoadNeckOffsetFromHeroJson()
{
	FVector Result(-8.f, 0.f, -10.f);
	const FString Path = FPaths::ProjectPluginsDir() / TEXT("Resources/ProjectObject/Content/Human/Hero/Hero.json");
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *Path)) return Result;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return Result;

	const TSharedPtr<FJsonObject>* Sections = nullptr;
	if (!Root->TryGetObjectField(TEXT("sections"), Sections)) return Result;

	const TSharedPtr<FJsonObject>* View = nullptr;
	if (!(*Sections)->TryGetObjectField(TEXT("view"), View)) return Result;

	FString OffsetStr;
	if ((*View)->TryGetStringField(TEXT("neckOffset"), OffsetStr))
	{
		FVector Parsed;
		if (Parsed.InitFromString(OffsetStr))
		{
			Result = Parsed;
		}
	}
	return Result;
}

static USkeletalMeshComponent* FindCopyPoseSource(AActor* Owner)
{
	if (!Owner) return nullptr;

	static const FName WorldBodyRoleTag(TEXT("AssemblyRole=WorldBody"));
	static const FName DriverBodyRoleTag(TEXT("AssemblyRole=DriverBody"));
	static const FName LegacyWorldBodyName(TEXT("WorldBodyMesh"));

	TArray<USkeletalMeshComponent*> SkeletalComps;
	Owner->GetComponents<USkeletalMeshComponent>(SkeletalComps);

	for (USkeletalMeshComponent* SKC : SkeletalComps)
	{
		if (SKC->ComponentTags.Contains(WorldBodyRoleTag) &&
			SKC->GetSkeletalMeshAsset() &&
			(SKC->GetAnimInstance() || SKC->LeaderPoseComponent.IsValid()))
		{
			return SKC;
		}
	}

	for (USkeletalMeshComponent* SKC : SkeletalComps)
	{
		if (SKC->ComponentTags.Contains(DriverBodyRoleTag) && SKC->GetSkeletalMeshAsset())
		{
			return SKC;
		}
	}

	for (USkeletalMeshComponent* SKC : SkeletalComps)
	{
		if (SKC->GetFName() == LegacyWorldBodyName)
		{
			return SKC;
		}
	}

	return nullptr;
}

// ---------------------------------------------------------------------------
// Proxy
// ---------------------------------------------------------------------------

void FLocalBodyAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	USkeletalMeshComponent* OwnerComp = InAnimInstance->GetSkelMeshComponent();
	if (!OwnerComp) return;

	AActor* Owner = OwnerComp->GetOwner();
	USkeletalMeshComponent* Source = FindCopyPoseSource(Owner);
	if (Source && Source->GetSkeletalMeshAsset())
	{
		CopyPoseNode.SourceMeshComponent = Source;
	}

	// Spine yaw nodes (always active)
	auto ConfigureYawNode = [](FAnimNode_ModifyBone& Node, const FName& BoneName)
	{
		Node.BoneToModify.BoneName = BoneName;
		Node.TranslationMode = BMM_Ignore;
		Node.RotationMode = BMM_Additive;
		Node.RotationSpace = BCS_ComponentSpace;
		Node.ScaleMode = BMM_Ignore;
		Node.Alpha = 1.0f;
	};
	ConfigureYawNode(Spine01Node, FName("spine_01"));
	ConfigureYawNode(Spine02Node, FName("spine_02"));

	// Base chain: CopyPose -> CS -> Spine01 -> Spine02
	LocalToCSNode.LocalPose.SetLinkNode(&CopyPoseNode);
	Spine01Node.ComponentPose.SetLinkNode(&LocalToCSNode);
	Spine02Node.ComponentPose.SetLinkNode(&Spine01Node);

	// Resolve and initialize the active correction strategy
	ULocalBodyAnimInstance* AnimInst = Cast<ULocalBodyAnimInstance>(InAnimInstance);
	if (AnimInst)
	{
		ActiveCorrection = AnimInst->ResolveCorrection();
	}

	if (ActiveCorrection)
	{
		ActiveCorrection->InitializeNodes(InAnimInstance, &Spine02Node);
		CSToLocalNode.ComponentPose.SetLinkNode(ActiveCorrection->GetOutputNode());
	}
	else
	{
		CSToLocalNode.ComponentPose.SetLinkNode(&Spine02Node);
	}

	FAnimInstanceProxy::Initialize(InAnimInstance);
}

void FLocalBodyAnimInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&CopyPoseNode);
	OutNodes.Add(&LocalToCSNode);
	OutNodes.Add(&Spine01Node);
	OutNodes.Add(&Spine02Node);

	if (ActiveCorrection)
	{
		ActiveCorrection->GetNodes(OutNodes);
	}

	OutNodes.Add(&CSToLocalNode);
}

void FLocalBodyAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	ULocalBodyAnimInstance* AnimInst = Cast<ULocalBodyAnimInstance>(InAnimInstance);
	if (!AnimInst || !AnimInst->bEnableSpineLock || !SpineLockData.bValid)
	{
		Spine01Node.Alpha = 0.f;
		Spine02Node.Alpha = 0.f;
		if (ActiveCorrection)
		{
			// Zero the correction by passing invalid data
			FSpineLockData ZeroData;
			ActiveCorrection->Update(ZeroData);
		}
		return;
	}

	// Spine yaw tracking
	const float YawDelta = SpineLockData.YawDeltaDeg;
	Spine01Node.Rotation = FRotator(0.f, YawDelta * 0.4f, 0.f);
	Spine02Node.Rotation = FRotator(0.f, YawDelta * 0.3f, 0.f);
	Spine01Node.Alpha = 1.0f;
	Spine02Node.Alpha = 1.0f;

	// Delegate correction
	if (ActiveCorrection)
	{
		ActiveCorrection->Update(SpineLockData);
	}
}

// ---------------------------------------------------------------------------
// ULocalBodyAnimInstance
// ---------------------------------------------------------------------------

ILocalBodyCorrection* ULocalBodyAnimInstance::ResolveCorrection()
{
	// Copy settings to the active correction before returning
	CorrectionTransitionGuard.Settings = TransitionGuardSettings;
	CorrectionAngleClamp.Settings = AngleClampSettings;

	InitializedMode = UpperChainMode;

	switch (UpperChainMode)
	{
	case ELocalBodyUpperChainMode::TransitionGuard:
		return &CorrectionTransitionGuard;
	case ELocalBodyUpperChainMode::AngleClamp:
		return &CorrectionAngleClamp;
	case ELocalBodyUpperChainMode::ChainIK:
		return &CorrectionChainIK;
	case ELocalBodyUpperChainMode::Disabled:
	default:
		return &CorrectionDisabled;
	}
}

void ULocalBodyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	NeckOffsetFromCamera = LoadNeckOffsetFromHeroJson();
}

FName ULocalBodyAnimInstance::GetCurrentSourceName() const
{
	if (const USkeletalMeshComponent* SourceMesh = LocalBodyProxy.CopyPoseNode.SourceMeshComponent.Get())
	{
		return SourceMesh->GetFName();
	}

	const USkeletalMeshComponent* LocalBodyComp = GetSkelMeshComponent();
	const AActor* Owner = LocalBodyComp ? LocalBodyComp->GetOwner() : nullptr;
	if (USkeletalMeshComponent* SourceMesh = FindCopyPoseSource(const_cast<AActor*>(Owner)))
	{
		return SourceMesh->GetFName();
	}

	return NAME_None;
}

void ULocalBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Detect runtime mode change (e.g. test forces UpperChainMode via reflection).
	// Reinitialize the anim instance to rewire the node chain.
	if (UpperChainMode != InitializedMode)
	{
		USkeletalMeshComponent* MeshComp = GetSkelMeshComponent();
		if (MeshComp)
		{
			MeshComp->InitAnim(true);
		}
		return;
	}

	USkeletalMeshComponent* LocalBodyComp = GetSkelMeshComponent();
	AActor* Owner = LocalBodyComp ? LocalBodyComp->GetOwner() : nullptr;
	USkeletalMeshComponent* SourceMesh = FindCopyPoseSource(Owner);

	if (LocalBodyProxy.CopyPoseNode.SourceMeshComponent != SourceMesh)
	{
		LocalBodyProxy.CopyPoseNode.SourceMeshComponent = SourceMesh;
	}

	if (!bEnableSpineLock) return;

	APawn* PawnOwner = Cast<APawn>(Owner);
	if (!PawnOwner)
	{
		LocalBodyProxy.SpineLockData.bValid = false;
		return;
	}

	const FRotator ControlRot = PawnOwner->GetControlRotation();
	const FRotator ActorRot = PawnOwner->GetActorRotation();
	const float YawDelta = FMath::FindDeltaAngleDegrees(ActorRot.Yaw, ControlRot.Yaw);
	const float ClampedYaw = FMath::Clamp(YawDelta, -90.f, 90.f);

	const float ControlPitch = FRotator::NormalizeAxis(ControlRot.Pitch);
	float NeckPitch = 0.f;
	if (ControlPitch < -15.f)
	{
		NeckPitch = FMath::GetMappedRangeValueClamped(
			FVector2D(-15.f, -45.f),
			FVector2D(0.f, -8.f),
			ControlPitch);
	}

	FVector NeckTargetCS = FVector::ZeroVector;
	bool bNeckTargetValid = false;
	UCameraComponent* Camera = PawnOwner->FindComponentByClass<UCameraComponent>();
	const FVector CameraWorld = Camera ? Camera->GetComponentLocation() : PawnOwner->GetPawnViewLocation();
	const FVector NeckTargetWorld = CameraWorld + ActorRot.RotateVector(NeckOffsetFromCamera);
	if (LocalBodyComp)
	{
		NeckTargetCS = LocalBodyComp->GetComponentTransform().InverseTransformPosition(NeckTargetWorld);
		bNeckTargetValid = true;
	}

	// Save source hand positions before correction (for TwoBoneIK arm restore)
	bool bHandTargetsValid = false;
	FVector HandLTargetCS = FVector::ZeroVector;
	FVector HandRTargetCS = FVector::ZeroVector;
	FVector JointTargetLCS = FVector::ZeroVector;
	FVector JointTargetRCS = FVector::ZeroVector;
	if (SourceMesh && LocalBodyComp)
	{
		const FTransform CompTransform = LocalBodyComp->GetComponentTransform();
		const int32 HandLIdx = SourceMesh->GetBoneIndex(FName("hand_l"));
		const int32 HandRIdx = SourceMesh->GetBoneIndex(FName("hand_r"));
		const int32 LowerArmLIdx = SourceMesh->GetBoneIndex(FName("lowerarm_l"));
		const int32 LowerArmRIdx = SourceMesh->GetBoneIndex(FName("lowerarm_r"));
		if (HandLIdx != INDEX_NONE && HandRIdx != INDEX_NONE &&
			LowerArmLIdx != INDEX_NONE && LowerArmRIdx != INDEX_NONE)
		{
			HandLTargetCS = CompTransform.InverseTransformPosition(
				SourceMesh->GetBoneTransform(HandLIdx).GetLocation());
			HandRTargetCS = CompTransform.InverseTransformPosition(
				SourceMesh->GetBoneTransform(HandRIdx).GetLocation());
			// Elbow targets: use the actual elbow position from source pose
			JointTargetLCS = CompTransform.InverseTransformPosition(
				SourceMesh->GetBoneTransform(LowerArmLIdx).GetLocation());
			JointTargetRCS = CompTransform.InverseTransformPosition(
				SourceMesh->GetBoneTransform(LowerArmRIdx).GetLocation());
			bHandTargetsValid = true;
		}
	}

	// Write base data to proxy
	LocalBodyProxy.SpineLockData.bValid = true;
	LocalBodyProxy.SpineLockData.YawDeltaDeg = ClampedYaw;
	LocalBodyProxy.SpineLockData.NeckPitchDeg = NeckPitch;
	LocalBodyProxy.SpineLockData.NeckTargetCS = NeckTargetCS;
	LocalBodyProxy.SpineLockData.bNeckTargetValid = bNeckTargetValid;
	LocalBodyProxy.SpineLockData.HandLTargetCS = HandLTargetCS;
	LocalBodyProxy.SpineLockData.HandRTargetCS = HandRTargetCS;
	LocalBodyProxy.SpineLockData.JointTargetLCS = JointTargetLCS;
	LocalBodyProxy.SpineLockData.JointTargetRCS = JointTargetRCS;
	LocalBodyProxy.SpineLockData.bHandTargetsValid = bHandTargetsValid;

	// Let the active correction evaluate and write its results
	ILocalBodyCorrection* Correction = LocalBodyProxy.ActiveCorrection;
	if (Correction && UpperChainMode != ELocalBodyUpperChainMode::Disabled &&
		LocalBodyComp && SourceMesh && Camera)
	{
		FLocalBodyFrameContext Ctx;
		Ctx.ComponentTransform = LocalBodyComp->GetComponentTransform();
		Ctx.CameraTransform = Camera->GetComponentTransform();
		Ctx.CameraWorldPos = CameraWorld;
		Ctx.NeckTargetWorld = NeckTargetWorld;
		Ctx.ControlPitch = ControlPitch;
		Ctx.DeltaSeconds = DeltaSeconds;

		const int32 HeadIdx = SourceMesh->GetBoneIndex(FName("head"));
		if (HeadIdx != INDEX_NONE)
		{
			Ctx.SourceHeadWorld = SourceMesh->GetBoneTransform(HeadIdx).GetLocation();
			Ctx.bHasSourceHead = true;
		}
		const int32 NeckIdx = SourceMesh->GetBoneIndex(FName("neck_01"));
		if (NeckIdx != INDEX_NONE)
		{
			Ctx.SourceNeckWorld = SourceMesh->GetBoneTransform(NeckIdx).GetLocation();
			Ctx.bHasSourceNeck = true;
		}
		const int32 Spine05Idx = SourceMesh->GetBoneIndex(FName("spine_05"));
		if (Spine05Idx != INDEX_NONE)
		{
			Ctx.SourceSpine05World = SourceMesh->GetBoneTransform(Spine05Idx).GetLocation();
			Ctx.bHasSourceSpine05 = true;
		}

		const ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
		const UCharacterMovementComponent* MoveComp = CharacterOwner
			? CharacterOwner->GetCharacterMovement()
			: nullptr;
		Ctx.HorizontalSpeed = CharacterOwner
			? CharacterOwner->GetVelocity().Size2D()
			: PawnOwner->GetVelocity().Size2D();
		Ctx.bIsFalling = MoveComp && MoveComp->IsFalling();
		Ctx.bIsCrouching = MoveComp && MoveComp->IsCrouching();

		Correction->EvaluateGameThread(Ctx, LocalBodyProxy.SpineLockData);
	}

	// Debug draw
	if (LocalBodyDebug::IsDebugDrawEnabled() && LocalBodyComp)
	{
		FLocalBodyFilterState DebugState;
		if (UpperChainMode == ELocalBodyUpperChainMode::TransitionGuard)
		{
			DebugState.StopGuardAlpha = CorrectionTransitionGuard.State.StopGuardAlpha;
			DebugState.LandingGuardAlpha = CorrectionTransitionGuard.State.LandingGuardAlpha;
			DebugState.StopGuardTimeRemaining = CorrectionTransitionGuard.State.StopGuardTimeRemaining;
			DebugState.LandingGuardTimeRemaining = CorrectionTransitionGuard.State.LandingGuardTimeRemaining;
		}
		LocalBodyDebug::DrawSpineTrackingDebug(
			PawnOwner->GetWorld(),
			PawnOwner,
			SourceMesh,
			LocalBodyComp,
			Camera,
			CameraWorld,
			NeckTargetWorld,
			LocalBodyProxy.SpineLockData.UpperSpineFollowCS,
			LocalBodyProxy.SpineLockData.UpperSpinePitchDeg,
			LocalBodyProxy.SpineLockData.UpperSpineGuardAlpha,
			DebugState);
	}

	if (!bLoggedOnce && SourceMesh && Camera)
	{
		bLoggedOnce = true;
		LocalBodyDebug::LogIdleNeckOffset(SourceMesh, Camera, ActorRot, NeckOffsetFromCamera);
	}
}

FAnimInstanceProxy* ULocalBodyAnimInstance::CreateAnimInstanceProxy()
{
	return &LocalBodyProxy;
}

void ULocalBodyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) {}
