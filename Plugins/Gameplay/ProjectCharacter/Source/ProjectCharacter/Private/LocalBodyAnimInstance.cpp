// AnimInstance for LocalBodyMesh - Camera-Centric Root Offset
//
// Pipeline: CopyPoseFromMesh -> LocalToCS -> ModifyBone(root) -> CSToLocal -> Output
//
// Root Offset: Calculates XY drift of neck_02 from camera position in Mesh CS,
// then applies the inverted drift as an additive translation to 'root'.
// Camera-based drift pulls the body toward the camera, placing the viewpoint
// at the neck/clavicle base — exactly where a first-person camera should sit.

#include "LocalBodyAnimInstance.h"
#include "ProjectCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"

void FLocalBodyAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	USkeletalMeshComponent* OwnerComp = InAnimInstance->GetSkelMeshComponent();
	if (!OwnerComp) return;

	AActor* Owner = OwnerComp->GetOwner();
	AProjectCharacter* Character = Cast<AProjectCharacter>(Owner);
	if (Character)
	{
		USkeletalMeshComponent* Source = Character->GetWorldBodyMesh();
		if (Source && Source->GetSkeletalMeshAsset())
		{
			CopyPoseNode.SourceMeshComponent = Source;
		}
	}

	// --- Chain Link: CopyPose -> CS -> RootOffset -> Local -> Output ---
	LocalToCSNode.LocalPose.SetLinkNode(&CopyPoseNode);
	RootFreezeNode.ComponentPose.SetLinkNode(&LocalToCSNode); 
	CSToLocalNode.ComponentPose.SetLinkNode(&RootFreezeNode);

	// --- Config: ModifyBone(root) strictly in Component Space ---
	RootFreezeNode.BoneToModify.BoneName = FName("root");
	RootFreezeNode.TranslationMode = BMM_Replace; // Entirely overrides XY
	RootFreezeNode.TranslationSpace = BCS_ComponentSpace;
	RootFreezeNode.RotationMode = BMM_Ignore;
	RootFreezeNode.ScaleMode = BMM_Ignore;
	RootFreezeNode.Alpha = 1.0f;

	// Note: We used to have separate SpineLockNode and NeckLockNode, 
	// but the Evening Correct (Root Offset) only needs one node on 'root'.
	// These are left for bypass or future per-bone tweaks if needed.

	FAnimInstanceProxy::Initialize(InAnimInstance);
}

void FLocalBodyAnimInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&CopyPoseNode);
	OutNodes.Add(&LocalToCSNode);
	OutNodes.Add(&RootFreezeNode);
	OutNodes.Add(&CSToLocalNode);
}

void FLocalBodyAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	ULocalBodyAnimInstance* AnimInst = Cast<ULocalBodyAnimInstance>(InAnimInstance);
	if (!AnimInst || !AnimInst->bEnableSpineLock || !SpineLockData.bValid)
	{
		RootFreezeNode.Alpha = 0.f;
		return;
	}

	APawn* PawnOwner = Cast<APawn>(AnimInst->GetOwningActor());
	UCameraComponent* Cam = PawnOwner ? PawnOwner->FindComponentByClass<UCameraComponent>() : nullptr;
	USkeletalMeshComponent* Mesh = AnimInst->GetOwningComponent();

	if (Cam && Mesh)
	{
		// 1. RAW POSITIONS from WorldBodyMesh (to break the feedback loop)
		if (!SpineLockData.bValid) return;

		// 2. Camera position in Mesh Component Space
		// Camera-based drift pulls body toward camera — places viewpoint at neck/clavicle base
		FTransform MeshTransform = Mesh->GetComponentTransform();
		FVector CamLocal = MeshTransform.InverseTransformPosition(Cam->GetComponentLocation());

		// 3. XYZ drift = how far neck_02 is from camera (pulls body to camera)
		// Bone follows camera — camera is the authority, not the animation
		float DriftX = SpineLockData.GoalLocation.X - CamLocal.X;
		float DriftY = SpineLockData.GoalLocation.Y - CamLocal.Y;
		float DriftZ = SpineLockData.GoalLocation.Z - CamLocal.Z;

		// Precision Dead-zone XY (prevents foot glide during idle sway)
		const float MaxSwayXY = 8.0f;
		float DriftMagXY = FMath::Sqrt(DriftX * DriftX + DriftY * DriftY);
		if (DriftMagXY > MaxSwayXY)
		{
			float Scale = (DriftMagXY - MaxSwayXY) / DriftMagXY;
			DriftX *= Scale;
			DriftY *= Scale;
		}
		else
		{
			DriftX = 0.f;
			DriftY = 0.f;
		}

		// Dead-zone Z — ignore small vertical differences (idle/breathing),
		// only correct large gaps (crouch ~30cm, jump)
		const float MaxSwayZ = 5.0f;
		if (FMath::Abs(DriftZ) > MaxSwayZ)
		{
			DriftZ = (DriftZ > 0.f) ? (DriftZ - MaxSwayZ) : (DriftZ + MaxSwayZ);
		}
		else
		{
			DriftZ = 0.f;
		}

		// Replace root XYZ to pull the skeleton under the camera
		RootFreezeNode.Alpha = 1.0f;
		RootFreezeNode.Translation = FVector(
			SpineLockData.WorldRootPos.X - DriftX,
			SpineLockData.WorldRootPos.Y - DriftY,
			SpineLockData.WorldRootPos.Z - DriftZ
		);

	}
	else
	{
		RootFreezeNode.Alpha = 0.f;
	}
}

void ULocalBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!bEnableSpineLock) return;

	USkeletalMeshComponent* LocalBodyComp = GetSkelMeshComponent();
	AProjectCharacter* Character = LocalBodyComp ? Cast<AProjectCharacter>(LocalBodyComp->GetOwner()) : nullptr;
	USkeletalMeshComponent* WorldBodyMesh = Character ? Character->GetWorldBodyMesh() : nullptr;

	if (WorldBodyMesh && WorldBodyMesh->GetSkeletalMeshAsset())
	{
		// Pass RAW World Socket locations to the Proxy (to break the feedback loop)
		LocalBodyProxy.SpineLockData.bValid = true;
		LocalBodyProxy.SpineLockData.WorldRootPos = WorldBodyMesh->GetSocketTransform(FName("root"), RTS_Component).GetLocation();
		LocalBodyProxy.SpineLockData.GoalLocation = WorldBodyMesh->GetSocketTransform(FName("neck_02"), RTS_Component).GetLocation();
	}
	else
	{
		LocalBodyProxy.SpineLockData.bValid = false;
	}
}
