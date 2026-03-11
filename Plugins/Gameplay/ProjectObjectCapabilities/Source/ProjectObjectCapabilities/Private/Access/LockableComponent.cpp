// Copyright ALIS. All Rights Reserved.

#include "Access/LockableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectObjectCapabilitiesModule.h"
#include "Interfaces/IProjectActionReceiver.h"

namespace
{
void ForwardUnlockedInteractionToScopedMotion(ULockableComponent* Self)
{
	if (!Self)
	{
		return;
	}

	AActor* Owner = Self->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Keep mutation behavior authoritative and consistent with motion receiver contract.
	if (!Owner->HasAuthority())
	{
		return;
	}

	UPrimitiveComponent* LockTargetMesh =
		IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(Self);

	TInlineComponentArray<UActorComponent*> Components;
	Owner->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (!Comp || Comp == Self)
		{
			continue;
		}

		if (!Comp->GetClass()->ImplementsInterface(UProjectActionReceiver::StaticClass()))
		{
			continue;
		}

		if (!Comp->GetClass()->ImplementsInterface(UInteractableComponentTargetInterface::StaticClass()))
		{
			continue;
		}

		UPrimitiveComponent* CandidateTargetMesh =
			IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(Comp);
		const bool bSameScope =
			(LockTargetMesh && CandidateTargetMesh == LockTargetMesh) ||
			(!LockTargetMesh && !CandidateTargetMesh);
		if (!bSameScope)
		{
			continue;
		}

		if (IProjectActionReceiver* Receiver = Cast<IProjectActionReceiver>(Comp))
		{
			Receiver->HandleAction(TEXT("lock.pass"), TEXT("motion.toggle"));
			break;
		}
	}
}
}

// -------------------------------------------------------------------------
// Validation Registration - NOT NEEDED for Lockable
// -------------------------------------------------------------------------
// Capabilities with REQUIRED properties must register a validator via:
//   FCapabilityValidationRegistry::RegisterValidation(Id, &ValidateFunc)
//
// Lockable has NO required properties:
//   - LockTag: empty = unlocked (valid default)
//   - bConsumeKeyOnUnlock: has default (false)
//   - Sounds: optional
//
// All capabilities use validation for warnings only (defaults exist).
// See: PickupCapabilityComponent.cpp for validation pattern.
//
// Pattern: Use static initializer in .cpp with #if WITH_EDITOR guard.
// See: SpringRotatorComponent.cpp, PickupCapabilityComponent.cpp
// -------------------------------------------------------------------------

ULockableComponent::ULockableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

FPrimaryAssetId ULockableComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("Lockable")));
}

bool ULockableComponent::IsLocked() const
{
	return LockTag.IsValid() && !bIsUnlocked;
}

bool ULockableComponent::OnComponentInteract_Implementation(AActor* Instigator)
{
	UE_LOG(LogProjectObjectCapabilities, Log,
		TEXT("[%s::Lockable] Interact (Locked=%s, LockTag=%s, Instigator=%s)"),
		*GetNameSafe(GetOwner()),
		IsLocked() ? TEXT("true") : TEXT("false"),
		*LockTag.ToString(),
		*GetNameSafe(Instigator));

	// If not locked, allow interaction to continue
	if (!IsLocked())
	{
		// Single-selection interaction path chooses Lockable first.
		// When unlocked, forward to motion capability on the same scope so doors
		// keep normal open/close behavior without broad multi-capability fan-out.
		ForwardUnlockedInteractionToScopedMotion(this);
		return true;
	}

	// Play access denied sound
	if (AccessDeniedSound && GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, AccessDeniedSound, GetOwner()->GetActorLocation());
	}

	// Locked - broadcast access denied for UI feedback
	UE_LOG(LogProjectObjectCapabilities, Log,
		TEXT("[%s::Lockable] Access denied (RequiredTag=%s, Instigator=%s)"),
		*GetNameSafe(GetOwner()),
		*LockTag.ToString(),
		*GetNameSafe(Instigator));
	OnAccessDenied.Broadcast(LockTag, Instigator);

	// Block further interaction
	return false;
}

FText ULockableComponent::GetInteractionLabel_Implementation() const
{
	return IsLocked()
		? NSLOCTEXT("LockableComponent", "Unlock", "Unlock")
		: NSLOCTEXT("LockableComponent", "Interact", "Interact");
}

UPrimitiveComponent* ULockableComponent::GetInteractTargetMesh_Implementation() const
{
	return InteractTargetMesh;
}

void ULockableComponent::SetInteractTargetMesh_Implementation(UPrimitiveComponent* TargetMesh)
{
	InteractTargetMesh = TargetMesh;
}

void ULockableComponent::Unlock()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bIsUnlocked)
	{
		return;
	}

	bIsUnlocked = true;

	// Play unlock sound
	if (UnlockSound && GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, UnlockSound, GetOwner()->GetActorLocation());
	}

	OnUnlocked.Broadcast();
}

void ULockableComponent::Lock()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!bIsUnlocked)
	{
		return;
	}

	bIsUnlocked = false;
	OnLocked.Broadcast();
}

FInteractionPrompt ULockableComponent::GetInteractionPrompt_Implementation(AActor* Instigator) const
{
	FInteractionPrompt Prompt;

	// If not locked, skip prompt
	if (!IsLocked())
	{
		return Prompt; // bSkipPrompt = true by default
	}

	// Generate prompt from LockTag
	// Tag format: "Item.Type.Key.Apartment.Luxury" -> extract last segment for display
	FString TagString = LockTag.ToString();
	FString KeyName;
	TagString.Split(TEXT("."), nullptr, &KeyName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (KeyName.IsEmpty())
	{
		KeyName = TagString;
	}

	Prompt.bSkipPrompt = false;
	Prompt.Title = FText::Format(NSLOCTEXT("LockableComponent", "UnlockPrompt", "Unlock with {0}?"), FText::FromString(KeyName));
	Prompt.Options.Add(NSLOCTEXT("LockableComponent", "Yes", "Yes"));
	Prompt.Options.Add(NSLOCTEXT("LockableComponent", "No", "No"));
	Prompt.ConfirmIndex = 0;

	return Prompt;
}

void ULockableComponent::HandleAction(const FString& Context, const FString& Action)
{
	// Authority guard: lock state is replicated, mutations must happen on server
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogProjectObjectCapabilities, Warning,
			TEXT("[%s::HandleAction] Ignoring '%s' - not authority (actions must be dispatched on server)"),
			*GetNameSafe(GetOwner()), *Action);
		return;
	}

	if (Action == TEXT("lock.unlock"))
	{
		UE_LOG(LogProjectObjectCapabilities, Log,
			TEXT("[%s::HandleAction] lock.unlock (Context='%s')"),
			*GetNameSafe(GetOwner()), *Context);
		Unlock();
	}
	else if (Action == TEXT("lock.lock"))
	{
		UE_LOG(LogProjectObjectCapabilities, Log,
			TEXT("[%s::HandleAction] lock.lock (Context='%s')"),
			*GetNameSafe(GetOwner()), *Context);
		Lock();
	}
}

void ULockableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULockableComponent, bIsUnlocked);
}
