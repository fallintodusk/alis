// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MotionMatchingCapability.h"
#include "MotionMatchingBridgeAnimInstance.h"
#include "ProjectSkeletalCapabilitiesModule.h"

#include "Interfaces/IAssemblyCapability.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "HAL/IConsoleManager.h"

#include "ReflectionWriteHelpers.h"

using namespace SkeletalCapabilities;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

UMotionMatchingCapability::UMotionMatchingCapability()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = false;
}

FPrimaryAssetId UMotionMatchingCapability::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(
		FPrimaryAssetType(TEXT("CapabilityComponent")),
		FName(TEXT("MotionMatching")));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UMotionMatchingCapability::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);

	IAssemblyCapability* AssemblyProvider = nullptr;
	for (UActorComponent* Comp : Components)
	{
		if (Comp == this) continue;
		AssemblyProvider = Cast<IAssemblyCapability>(Comp);
		if (AssemblyProvider)
		{
			CachedAssemblyComponent = Comp;
			break;
		}
	}

	if (AssemblyProvider)
	{
		const EAssemblyState CurrentState = AssemblyProvider->GetCurrentAssemblyState();
		if (CurrentState == EAssemblyState::Ready)
		{
			InitializeMotionMatching();
		}
		else
		{
			AssemblyStateHandle = AssemblyProvider->AddAssemblyStateChanged(
				FOnAssemblyStateChangedNative::FDelegate::CreateUObject(
					this, &UMotionMatchingCapability::OnAssemblyStateChanged));
		}
	}
	else
	{
		InitializeMotionMatching();
	}
}

void UMotionMatchingCapability::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BridgeRetryHandle);
		World->GetTimerManager().ClearTimer(MutableBindRetryHandle);
	}

	if (BoundMutableInstance)
	{
		BoundMutableInstance->UpdatedNativeDelegate.RemoveAll(this);
		BoundMutableInstance = nullptr;
	}
	bMutableBound = false;

	if (CachedAssemblyComponent.IsValid())
	{
		if (IAssemblyCapability* Asm = Cast<IAssemblyCapability>(CachedAssemblyComponent.Get()))
		{
			Asm->RemoveAssemblyStateChanged(AssemblyStateHandle);
		}
	}
	AssemblyStateHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UMotionMatchingCapability::OnAssemblyStateChanged(EAssemblyState NewState)
{
	if (NewState == EAssemblyState::Ready)
	{
		InitializeMotionMatching();
	}
}

void UMotionMatchingCapability::InitializeMotionMatching()
{
	if (!TryInstallPostProcessBridge())
	{
		RetryBridgeInstall();
	}

	if (!TryBindMutableDelegate())
	{
		RetryMutableBind();
	}
}

// ---------------------------------------------------------------------------
// Mesh discovery
// ---------------------------------------------------------------------------

USkeletalMeshComponent* UMotionMatchingCapability::FindMeshByRole(const TCHAR* RoleName) const
{
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	const FName RoleTag = FName(*FString::Printf(TEXT("AssemblyRole=%s"), RoleName));

	TArray<USkeletalMeshComponent*> SkelComps;
	Owner->GetComponents<USkeletalMeshComponent>(SkelComps);

	for (USkeletalMeshComponent* SKC : SkelComps)
	{
		if (SKC->ComponentTags.Contains(RoleTag))
		{
			return SKC;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// PostProcess bridge with pass-through AnimGraph
// ---------------------------------------------------------------------------

bool UMotionMatchingCapability::TryInstallPostProcessBridge()
{
	if (bBridgeInstalled) return true;

	USkeletalMeshComponent* DriverBody = FindMeshByRole(TEXT("DriverBody"));
	if (!DriverBody) return false;

	UAnimInstance* Primary = DriverBody->GetAnimInstance();
	if (!Primary) return false;

	CachedDriverBody = DriverBody;

	// Load the PostProcess bridge AnimBP. The BP asset extends
	// UMotionMatchingBridgeAnimInstance and has a pass-through AnimGraph
	// (LinkedAnimGraphInput -> OutputPose). C++ NativeUpdateAnimation injects
	// CharacterProperties after the primary ABP's BPI call zeros them.
	// Path comes from Hero.json capability properties; fallback to default.
	static const TCHAR* DefaultBridgePath =
		TEXT("/ProjectSkeletalCapabilities/MotionMatching/ABP_MotionMatchingBridge.ABP_MotionMatchingBridge_C");
	const TCHAR* ResolvedPath = BridgeAnimBPPath.IsEmpty() ? DefaultBridgePath : *BridgeAnimBPPath;
	UClass* BridgeBPClass = LoadObject<UClass>(nullptr, ResolvedPath);

	if (!BridgeBPClass || !BridgeBPClass->IsChildOf(UMotionMatchingBridgeAnimInstance::StaticClass()))
	{
		UE_LOG(LogProjectSkeletalCapabilities, Warning,
			TEXT("[MotionMatching] PostProcess bridge ABP invalid or not found: %s"),
			ResolvedPath);
		return false;
	}

	DriverBody->SetOverridePostProcessAnimBP(BridgeBPClass, true);

	bBridgeInstalled = true;

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[MotionMatching] Installed PostProcess bridge '%s' on DriverBody for '%s'"),
		*BridgeBPClass->GetName(), *GetNameSafe(GetOwner()));

	return true;
}

void UMotionMatchingCapability::RetryBridgeInstall()
{
	if (BridgeRetryCount >= MaxRetries)
	{
		UE_LOG(LogProjectSkeletalCapabilities, Warning,
			TEXT("[MotionMatching] DriverBody AnimInstance not ready after %d retries on '%s'"),
			BridgeRetryCount, *GetNameSafe(GetOwner()));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	++BridgeRetryCount;
	World->GetTimerManager().SetTimer(
		BridgeRetryHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!TryInstallPostProcessBridge()) RetryBridgeInstall();
		}),
		0.0f, false);
}

// ---------------------------------------------------------------------------
// Per-frame tick: write CharacterProperties to primary ABP
// ---------------------------------------------------------------------------

void UMotionMatchingCapability::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// CharacterProperties injection is handled entirely by the PostProcess
	// bridge's NativeUpdateAnimation (correct timing, correct evaluation path).
	// This tick exists for LeaderPose re-wiring and future movement tuning.
}

// ---------------------------------------------------------------------------
// Mutable delegate binding + LeaderPose wiring
// ---------------------------------------------------------------------------

bool UMotionMatchingCapability::TryBindMutableDelegate()
{
	if (bMutableBound && BoundMutableInstance)
	{
		return true;
	}
	bMutableBound = false;

	AActor* Owner = GetOwner();
	if (!Owner) return false;

	TArray<UCustomizableSkeletalComponent*> CSKs;
	Owner->GetComponents<UCustomizableSkeletalComponent>(CSKs);

	for (UCustomizableSkeletalComponent* CSK : CSKs)
	{
		UCustomizableObjectInstance* COI = CSK->GetCustomizableObjectInstance();
		if (COI)
		{
			if (BoundMutableInstance && BoundMutableInstance != COI)
			{
				BoundMutableInstance->UpdatedNativeDelegate.RemoveAll(this);
			}

			COI->UpdatedNativeDelegate.AddUObject(
				this, &UMotionMatchingCapability::OnMutableInstanceUpdated);
			BoundMutableInstance = COI;
			bMutableBound = true;

			UE_LOG(LogProjectSkeletalCapabilities, Log,
				TEXT("[MotionMatching] Bound Mutable rebuild delegate on '%s'"),
				*GetNameSafe(Owner));

			WireLeaderPoseChain();
			return true;
		}
	}

	return false;
}

void UMotionMatchingCapability::RetryMutableBind()
{
	if (MutableBindRetryCount >= MaxRetries)
	{
		UE_LOG(LogProjectSkeletalCapabilities, Log,
			TEXT("[MotionMatching] No Mutable CSKs found after %d retries on '%s'. Actor may not use Mutable."),
			MutableBindRetryCount, *GetNameSafe(GetOwner()));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	++MutableBindRetryCount;
	World->GetTimerManager().SetTimer(
		MutableBindRetryHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!TryBindMutableDelegate()) RetryMutableBind();
		}),
		0.0f, false);
}

void UMotionMatchingCapability::OnMutableInstanceUpdated(UCustomizableObjectInstance* Instance)
{
	WireLeaderPoseChain();
}

void UMotionMatchingCapability::WireLeaderPoseChain()
{
	USkeletalMeshComponent* DriverBody = CachedDriverBody.Get();
	if (!DriverBody)
	{
		DriverBody = FindMeshByRole(TEXT("DriverBody"));
		CachedDriverBody = DriverBody;
	}

	if (!DriverBody) return;

	USkeletalMeshComponent* WorldBody = FindMeshByRole(TEXT("WorldBody"));

	if (USkeletalMeshComponent* BodyCust = FindMeshByRole(TEXT("BodyCustomization")))
	{
		if (BodyCust->GetSkeletalMeshAsset())
		{
			BodyCust->SetLeaderPoseComponent(DriverBody);
			UE_LOG(LogProjectSkeletalCapabilities, Log,
				TEXT("[MotionMatching] LeaderPose: BodyCustomization -> DriverBody on '%s'"),
				*GetNameSafe(GetOwner()));
		}
	}

	if (USkeletalMeshComponent* HeadCust = FindMeshByRole(TEXT("HeadCustomization")))
	{
		if (HeadCust->GetSkeletalMeshAsset())
		{
			USkeletalMeshComponent* LeaderSource =
				(WorldBody && WorldBody->GetSkeletalMeshAsset()) ? WorldBody : DriverBody;
			HeadCust->SetLeaderPoseComponent(LeaderSource);
			UE_LOG(LogProjectSkeletalCapabilities, Log,
				TEXT("[MotionMatching] LeaderPose: HeadCustomization -> %s on '%s'"),
				*GetNameSafe(LeaderSource),
				*GetNameSafe(GetOwner()));
		}
	}
}
