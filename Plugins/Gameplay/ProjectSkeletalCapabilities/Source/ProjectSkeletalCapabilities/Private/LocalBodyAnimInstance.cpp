// AnimInstance for LocalBodyMesh - Spine Yaw Tracking + Neck Anti-Clip
//
// Pipeline: CopyPoseFromMesh -> CS -> Spine01 -> Spine02 -> Spine03 -> NeckLock -> Local -> Output

#include "LocalBodyAnimInstance.h"
#include "ProjectSkeletalCapabilitiesModule.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

// Find the CopyPose source mesh on the owning actor.
// Modular path: prefer WorldBody once it is fully initialized as the visual source,
// otherwise fall back to DriverBody.
// Legacy path: WorldBodyMesh (component name) -- has retarget output from DriverBody.
// No cast to AProjectCharacter needed -- avoids coupling to legacy module.
static USkeletalMeshComponent* FindCopyPoseSource(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}

	static const FName WorldBodyRoleTag(TEXT("AssemblyRole=WorldBody"));
	static const FName DriverBodyRoleTag(TEXT("AssemblyRole=DriverBody"));
	static const FName LegacyWorldBodyName(TEXT("WorldBodyMesh"));

	TArray<USkeletalMeshComponent*> SkeletalComps;
	Owner->GetComponents<USkeletalMeshComponent>(SkeletalComps);

	// Modular path: prefer the world visual layer once it has a generated mesh and
	// its own animation evaluation. Until then, DriverBody is the only live source.
	for (USkeletalMeshComponent* SKC : SkeletalComps)
	{
		if (SKC->ComponentTags.Contains(WorldBodyRoleTag) &&
			SKC->GetSkeletalMeshAsset() &&
			(SKC->GetAnimInstance() || SKC->LeaderPoseComponent.IsValid()))
		{
			return SKC;
		}
	}

	// Fallback: use DriverBody while the world visual layer is still empty.
	for (USkeletalMeshComponent* SKC : SkeletalComps)
	{
		if (SKC->ComponentTags.Contains(DriverBodyRoleTag) && SKC->GetSkeletalMeshAsset())
		{
			return SKC;
		}
	}

	// Legacy path: WorldBodyMesh (has retarget output)
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
// Helper: configure a ModifyBone node for additive rotation in component space
// ---------------------------------------------------------------------------

static void ConfigureSpineNode(
	FAnimNode_ModifyBone& Node,
	const FName& BoneName)
{
	Node.BoneToModify.BoneName = BoneName;
	Node.TranslationMode = BMM_Ignore;
	Node.RotationMode = BMM_Additive;
	Node.RotationSpace = BCS_ComponentSpace;
	Node.ScaleMode = BMM_Ignore;
	Node.Alpha = 1.0f;
}

// ---------------------------------------------------------------------------
// Proxy: Initialize
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

	// Configure spine nodes for additive yaw rotation
	ConfigureSpineNode(Spine01Node, FName("spine_01"));
	ConfigureSpineNode(Spine02Node, FName("spine_02"));
	ConfigureSpineNode(Spine03Node, FName("spine_03"));

	// Configure neck node for additive pitch anti-clip
	ConfigureSpineNode(NeckLockNode, FName("neck_01"));

	// Chain: CopyPose -> CS -> Spine01 -> Spine02 -> Spine03 -> NeckLock -> Local -> Output
	LocalToCSNode.LocalPose.SetLinkNode(&CopyPoseNode);
	Spine01Node.ComponentPose.SetLinkNode(&LocalToCSNode);
	Spine02Node.ComponentPose.SetLinkNode(&Spine01Node);
	Spine03Node.ComponentPose.SetLinkNode(&Spine02Node);
	NeckLockNode.ComponentPose.SetLinkNode(&Spine03Node);
	CSToLocalNode.ComponentPose.SetLinkNode(&NeckLockNode);

	FAnimInstanceProxy::Initialize(InAnimInstance);
}

// ---------------------------------------------------------------------------
// Proxy: GetCustomNodes
// ---------------------------------------------------------------------------

void FLocalBodyAnimInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&CopyPoseNode);
	OutNodes.Add(&LocalToCSNode);
	OutNodes.Add(&Spine01Node);
	OutNodes.Add(&Spine02Node);
	OutNodes.Add(&Spine03Node);
	OutNodes.Add(&NeckLockNode);
	OutNodes.Add(&CSToLocalNode);
}

// ---------------------------------------------------------------------------
// Proxy: PreUpdate -- apply spine yaw + neck pitch from game-thread data
// ---------------------------------------------------------------------------

void FLocalBodyAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	ULocalBodyAnimInstance* AnimInst = Cast<ULocalBodyAnimInstance>(InAnimInstance);
	if (!AnimInst || !AnimInst->bEnableSpineLock || !SpineLockData.bValid)
	{
		Spine01Node.Alpha = 0.f;
		Spine02Node.Alpha = 0.f;
		Spine03Node.Alpha = 0.f;
		NeckLockNode.Alpha = 0.f;
		return;
	}

	// Distribute yaw delta across 3 spine bones (40/30/30 split)
	const float YawDelta = SpineLockData.YawDeltaDeg;
	Spine01Node.Rotation = FRotator(0.f, YawDelta * 0.4f, 0.f);
	Spine02Node.Rotation = FRotator(0.f, YawDelta * 0.3f, 0.f);
	Spine03Node.Rotation = FRotator(0.f, YawDelta * 0.3f, 0.f);
	Spine01Node.Alpha = 1.0f;
	Spine02Node.Alpha = 1.0f;
	Spine03Node.Alpha = 1.0f;

	// Neck pitch anti-clip
	NeckLockNode.Rotation = FRotator(SpineLockData.NeckPitchDeg, 0.f, 0.f);
	NeckLockNode.Alpha = 1.0f;
}

// ---------------------------------------------------------------------------
// ULocalBodyAnimInstance: game-thread update
// ---------------------------------------------------------------------------

void ULocalBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	USkeletalMeshComponent* LocalBodyComp = GetSkelMeshComponent();
	AActor* Owner = LocalBodyComp ? LocalBodyComp->GetOwner() : nullptr;
	USkeletalMeshComponent* SourceMesh = FindCopyPoseSource(Owner);

	// Hot-swap source mesh if WorldBody becomes available after init
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

	// Compute yaw delta: how far camera yaw is ahead of actor facing
	const FRotator ControlRot = PawnOwner->GetControlRotation();
	const FRotator ActorRot = PawnOwner->GetActorRotation();
	const float YawDelta = FMath::FindDeltaAngleDegrees(ActorRot.Yaw, ControlRot.Yaw);
	const float ClampedYaw = FMath::Clamp(YawDelta, -90.f, 90.f);

	// Compute neck pitch anti-clip: tuck neck backward when looking steeply down
	const float ControlPitch = ControlRot.Pitch;
	float NeckPitch = 0.f;
	if (ControlPitch < -15.f)
	{
		// Map [-15, -45] -> [0, -8] degrees of backward neck tilt
		NeckPitch = FMath::GetMappedRangeValueClamped(
			FVector2D(-15.f, -45.f),
			FVector2D(0.f, -8.f),
			ControlPitch);
	}

	LocalBodyProxy.SpineLockData.bValid = true;
	LocalBodyProxy.SpineLockData.YawDeltaDeg = ClampedYaw;
	LocalBodyProxy.SpineLockData.NeckPitchDeg = NeckPitch;

	// One-time diagnostic log
	if (!bLoggedOnce && SourceMesh)
	{
		bLoggedOnce = true;
		UE_LOG(LogProjectSkeletalCapabilities, Log,
			TEXT("[LocalBody] Spine tracking active: SourceMesh=%s YawDelta=%.1f NeckPitch=%.1f"),
			*GetNameSafe(SourceMesh),
			LocalBodyProxy.SpineLockData.YawDeltaDeg,
			LocalBodyProxy.SpineLockData.NeckPitchDeg);
	}
}

FAnimInstanceProxy* ULocalBodyAnimInstance::CreateAnimInstanceProxy()
{
	return &LocalBodyProxy;
}

void ULocalBodyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) {}
