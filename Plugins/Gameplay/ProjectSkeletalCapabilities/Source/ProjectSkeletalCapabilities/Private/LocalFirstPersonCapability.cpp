// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "LocalFirstPersonCapability.h"
#include "ProjectSkeletalCapabilitiesModule.h"

#include "Interfaces/IAssemblyCapability.h"
#include "LocalBodyAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

ULocalFirstPersonCapability::ULocalFirstPersonCapability()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = false;
}

FPrimaryAssetId ULocalFirstPersonCapability::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(
		FPrimaryAssetType(TEXT("CapabilityComponent")),
		FName(TEXT("LocalFirstPerson")));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ULocalFirstPersonCapability::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Find assembly provider and bind to state delegate
	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);

	IAssemblyCapability* AssemblyProvider = nullptr;
	for (UActorComponent* Comp : Components)
	{
		if (Comp == this)
		{
			continue;
		}

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
			DiscoverMeshes();
			ApplyVisibility();
		}
		else
		{
			AssemblyStateHandle = AssemblyProvider->AddAssemblyStateChanged(
				FOnAssemblyStateChangedNative::FDelegate::CreateUObject(
					this, &ULocalFirstPersonCapability::OnAssemblyStateChanged));
		}
	}
	else
	{
		// No assembly -- initialize immediately (simple actor with first-person)
		DiscoverMeshes();
		ApplyVisibility();
	}
}

void ULocalFirstPersonCapability::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetTimerManager().ClearTimer(LocalControlRetryHandle);
		}
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

// ---------------------------------------------------------------------------
// Assembly state callback
// ---------------------------------------------------------------------------

void ULocalFirstPersonCapability::OnAssemblyStateChanged(EAssemblyState NewState)
{
	if (NewState == EAssemblyState::Ready && !bInitialized)
	{
		DiscoverMeshes();
		ApplyVisibility();
	}
}

// ---------------------------------------------------------------------------
// Mesh discovery
// ---------------------------------------------------------------------------

static const FString RoleTagPrefix(TEXT("AssemblyRole="));

static USkeletalMeshComponent* FindMeshByRole(AActor* Actor, const FString& RoleName)
{
	TArray<USkeletalMeshComponent*> SkeletalComps;
	Actor->GetComponents<USkeletalMeshComponent>(SkeletalComps);

	const FString WantedTag = RoleTagPrefix + RoleName;
	for (USkeletalMeshComponent* SKC : SkeletalComps)
	{
		for (const FName& Tag : SKC->ComponentTags)
		{
			if (Tag.ToString() == WantedTag)
			{
				return SKC;
			}
		}
	}
	return nullptr;
}

void ULocalFirstPersonCapability::DiscoverMeshes()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	LocalBodyMesh = FindMeshByRole(Owner, TEXT("LocalBody"));
	WorldBodyMesh = FindMeshByRole(Owner, TEXT("WorldBody"));
	HeadMesh = FindMeshByRole(Owner, TEXT("Head"));
	DriverBodyMesh = FindMeshByRole(Owner, TEXT("DriverBody"));

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[LocalFirstPerson] Discovered on '%s': LocalBody=%s WorldBody=%s Head=%s"),
		*GetNameSafe(Owner),
		LocalBodyMesh.IsValid() ? TEXT("yes") : TEXT("no"),
		WorldBodyMesh.IsValid() ? TEXT("yes") : TEXT("no"),
		HeadMesh.IsValid() ? TEXT("yes") : TEXT("no"));
}

// ---------------------------------------------------------------------------
// Visibility application
// ---------------------------------------------------------------------------

void ULocalFirstPersonCapability::ApplyVisibility()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Only apply on locally controlled pawns.
	// Possession may not have happened yet when assembly reaches Ready,
	// so retry with a bounded count until local control is established.
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (!Pawn->IsLocallyControlled())
		{
			if (LocalControlRetryCount < MaxLocalControlRetries)
			{
				++LocalControlRetryCount;

				if (UWorld* World = Owner->GetWorld())
				{
					World->GetTimerManager().SetTimer(
						LocalControlRetryHandle,
						FTimerDelegate::CreateUObject(this, &ULocalFirstPersonCapability::ApplyVisibility),
						0.1f,
						false);
				}

				UE_LOG(LogProjectSkeletalCapabilities, Verbose,
					TEXT("[LocalFirstPerson] Not locally controlled yet on '%s', retry %d/%d"),
					*GetNameSafe(Owner), LocalControlRetryCount, MaxLocalControlRetries);
			}
			return;
		}
	}

	// LocalBody owns the player-visible fixed mesh and its CopyPose animation.
	auto HideBonesOnMesh = [this](USkeletalMeshComponent* Mesh)
	{
		if (!Mesh || HiddenBones.IsEmpty())
		{
			return;
		}

		TArray<FString> BoneNames;
		HiddenBones.ParseIntoArray(BoneNames, TEXT(","));
		for (const FString& BoneName : BoneNames)
		{
			const FName Bone(*BoneName.TrimStartAndEnd());
			Mesh->HideBoneByName(Bone, PBO_None);
		}
	};

	if (USkeletalMeshComponent* LB = LocalBodyMesh.Get())
	{
		HideBonesOnMesh(LB);
		if (LB->LeaderPoseComponent.IsValid())
		{
			LB->SetLeaderPoseComponent(nullptr);
		}

		if (LB->GetSkeletalMeshAsset())
		{
			UAnimInstance* Anim = LB->GetAnimInstance();
			if (!Anim || !Anim->IsA(ULocalBodyAnimInstance::StaticClass()))
			{
				LB->SetAnimInstanceClass(ULocalBodyAnimInstance::StaticClass());
				LB->InitAnim(true);

				UE_LOG(LogProjectSkeletalCapabilities, Log,
					TEXT("[LocalFirstPerson] Installed LocalBodyAnimInstance on '%s'"),
					*GetNameSafe(LB));
			}
		}
	}

	// Head mesh follows the world visual layer.
	// This keeps the hidden first-person head/shadow aligned with the same body
	// source that drives LocalBody. Fall back to DriverBody during early init.
	if (USkeletalMeshComponent* Head = HeadMesh.Get())
	{
		USkeletalMeshComponent* LeaderSource = WorldBodyMesh.Get();
		if (!LeaderSource || !LeaderSource->GetSkeletalMeshAsset())
		{
			LeaderSource = DriverBodyMesh.Get();
		}
		if (LeaderSource)
		{
			Head->SetLeaderPoseComponent(LeaderSource);
		}
		Head->SetOwnerNoSee(true);
		Head->SetCastHiddenShadow(true);
	}

	// Head children (Groom bindings: hair, beard, eyebrows, etc.)
	if (USkeletalMeshComponent* Head = HeadMesh.Get())
	{
		TArray<USceneComponent*> HeadChildren;
		Head->GetChildrenComponents(true, HeadChildren);

		for (USceneComponent* Child : HeadChildren)
		{
			if (UPrimitiveComponent* PrimChild = Cast<UPrimitiveComponent>(Child))
			{
				PrimChild->SetOwnerNoSee(true);
				PrimChild->SetCastHiddenShadow(true);
			}
		}
	}

	bInitialized = true;
	bVisibilityApplied = true;
	LocalControlRetryCount = 0;

	UE_LOG(LogProjectSkeletalCapabilities, Verbose,
		TEXT("[LocalFirstPerson] Applied visibility on '%s'"),
		*GetNameSafe(Owner));
}
