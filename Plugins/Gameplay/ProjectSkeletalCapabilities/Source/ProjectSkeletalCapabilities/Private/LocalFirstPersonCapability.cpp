// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "LocalFirstPersonCapability.h"
#include "ProjectSkeletalCapabilitiesModule.h"

#include "Interfaces/IAssemblyCapability.h"
#include "LocalBodyAnimInstance.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"

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
			World->GetTimerManager().ClearTimer(GroomRetryTimerHandle);
			World->GetTimerManager().ClearTimer(MutableBindRetryHandle);
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

	if (BoundMutableInstance.IsValid())
	{
		BoundMutableInstance->UpdatedNativeDelegate.RemoveAll(this);
	}

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

	// Try to bind to Mutable COI. If MutableCustomization hasn't created
	// CSKs yet (assembly delegate ordering), retry on next frame.
	if (!TryBindMutableCOI())
	{
		RetryMutableBinding();
	}

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[LocalFirstPerson] Discovered on '%s': LocalBody=%s WorldBody=%s Head=%s MutableBound=%s"),
		*GetNameSafe(Owner),
		LocalBodyMesh.IsValid() ? TEXT("yes") : TEXT("no"),
		WorldBodyMesh.IsValid() ? TEXT("yes") : TEXT("no"),
		HeadMesh.IsValid() ? TEXT("yes") : TEXT("no"),
		BoundMutableInstance.IsValid() ? TEXT("yes") : TEXT("deferred"));
}

// ---------------------------------------------------------------------------
// Mutable COI binding (with retry for delegate ordering)
// ---------------------------------------------------------------------------

bool ULocalFirstPersonCapability::TryBindMutableCOI()
{
	if (BoundMutableInstance.IsValid())
	{
		return true;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	TArray<UCustomizableSkeletalComponent*> CSKs;
	Owner->GetComponents<UCustomizableSkeletalComponent>(CSKs);

	for (UCustomizableSkeletalComponent* CSK : CSKs)
	{
		UCustomizableObjectInstance* COI = CSK->GetCustomizableObjectInstance();
		if (COI)
		{
			BoundMutableInstance = COI;
			COI->UpdatedNativeDelegate.AddUObject(
				this, &ULocalFirstPersonCapability::OnMutableInstanceUpdated);

			UE_LOG(LogProjectSkeletalCapabilities, Log,
				TEXT("[LocalFirstPerson] Bound to Mutable COI on '%s' (retry %d)"),
				*GetNameSafe(Owner), MutableBindRetryCount);
			return true;
		}
	}

	return false;
}

void ULocalFirstPersonCapability::RetryMutableBinding()
{
	if (MutableBindRetryCount >= MaxMutableBindRetries)
	{
		UE_LOG(LogProjectSkeletalCapabilities, Log,
			TEXT("[LocalFirstPerson] No Mutable CSKs found after %d retries on '%s'. Actor may not use Mutable."),
			MutableBindRetryCount, *GetNameSafe(GetOwner()));
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}

	++MutableBindRetryCount;

	// Retry next frame (0 delay = next tick)
	World->GetTimerManager().SetTimer(
		MutableBindRetryHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!TryBindMutableCOI())
			{
				RetryMutableBinding();
			}
		}),
		0.0f,
		false);
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

	// Attempt COI binding on every visibility pass -- CSKs may have been created
	// since our initial discovery (Mutable adapter creates them async).
	if (!BoundMutableInstance.IsValid())
	{
		TryBindMutableCOI();
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

	// LocalBody: hide specified bones (head removal for first-person view)
	// Apply to both parent LocalBody and its Mutable child (LocalBodyCustomization)
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

		// Also hide bones on child LocalBodyCustomization (Mutable output mesh)
		TArray<USceneComponent*> Children;
		LB->GetChildrenComponents(false, Children);
		for (USceneComponent* Child : Children)
		{
			if (USkeletalMeshComponent* ChildSKC = Cast<USkeletalMeshComponent>(Child))
			{
				if (ChildSKC->ComponentTags.Contains(FName(TEXT("AssemblyRole=LocalBodyCustomization"))))
				{
					HideBonesOnMesh(ChildSKC);

					// Clear LeaderPose FIRST -- must be done before installing anim
					// instance, otherwise init can tick under stale leader-pose state.
					if (ChildSKC->LeaderPoseComponent.IsValid())
					{
						ChildSKC->SetLeaderPoseComponent(nullptr);
					}

					// Install CopyPose + SpineLock anim instance for camera-locked
					// first-person body. Check for wrong class too -- Mutable rebuild
					// may reset the anim instance to something else.
					if (ChildSKC->GetSkeletalMeshAsset())
					{
						UAnimInstance* Anim = ChildSKC->GetAnimInstance();
						if (!Anim || !Anim->IsA(ULocalBodyAnimInstance::StaticClass()))
						{
							ChildSKC->SetAnimInstanceClass(ULocalBodyAnimInstance::StaticClass());
							ChildSKC->InitAnim(true);

							UE_LOG(LogProjectSkeletalCapabilities, Log,
								TEXT("[LocalFirstPerson] Installed LocalBodyAnimInstance on '%s'"),
								*GetNameSafe(ChildSKC));
						}
					}
				}
			}
		}
	}

	// Head mesh: prefer the world visual layer once it has a generated mesh.
	// This keeps the hidden first-person head/shadow aligned with the same body
	// source that drives LocalBody. Fall back to DriverBody while WorldBody is empty.
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

// ---------------------------------------------------------------------------
// Mutable rebuild callback
// ---------------------------------------------------------------------------

void ULocalFirstPersonCapability::OnMutableInstanceUpdated(
	UCustomizableObjectInstance* Instance)
{
	// Mutable replaces meshes and resets visibility flags -- re-apply
	ApplyVisibility();

	// Groom components may attach asynchronously after Mutable finishes
	RetryVisibility();
}

void ULocalFirstPersonCapability::RetryVisibility()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		GroomRetryTimerHandle,
		FTimerDelegate::CreateUObject(this, &ULocalFirstPersonCapability::ApplyVisibility),
		0.5f,
		false);
}
