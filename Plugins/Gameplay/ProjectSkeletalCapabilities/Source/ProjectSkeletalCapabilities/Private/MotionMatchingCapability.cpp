// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MotionMatchingCapability.h"
#include "MotionMatchingBridgeAnimInstance.h"
#include "ProjectSkeletalCapabilitiesModule.h"

#include "Interfaces/IAssemblyCapability.h"
#include "Components/SkeletalMeshComponent.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

UMotionMatchingCapability::UMotionMatchingCapability()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	}

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

	// Load the PostProcess bridge AnimBP. The BP asset extends
	// UMotionMatchingBridgeAnimInstance and has a pass-through AnimGraph
	// (LinkedAnimGraphInput -> OutputPose). C++ NativeUpdateAnimation injects
	// CharacterProperties after the primary ABP's BPI call zeros them.
	// Path comes from Hero.json capability properties; fallback to default.
	static const FSoftClassPath DefaultBridgePath(
		TEXT("/ProjectSkeletalCapabilities/MotionMatching/ABP_MotionMatchingBridge.ABP_MotionMatchingBridge_C"));
	const FSoftClassPath ResolvedPath = BridgeAnimBPPath.IsNull()
		? DefaultBridgePath
		: FSoftClassPath(BridgeAnimBPPath.ToSoftObjectPath().ToString());
	UClass* BridgeBPClass = ResolvedPath.TryLoadClass<UAnimInstance>();

	if (!BridgeBPClass || !BridgeBPClass->IsChildOf(UMotionMatchingBridgeAnimInstance::StaticClass()))
	{
		UE_LOG(LogProjectSkeletalCapabilities, Warning,
			TEXT("[MotionMatching] PostProcess bridge ABP invalid or not found: %s"),
			*ResolvedPath.ToString());
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
