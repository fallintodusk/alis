// Copyright ALIS. All Rights Reserved.

#include "Components/SpringMotionComponentBase.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ProjectMotionSystemModule.h"

USpringMotionComponentBase::USpringMotionComponentBase()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USpringMotionComponentBase::BeginPlay()
{
	Super::BeginPlay();

	ResolveMeshReference();

	AActor* Owner = GetOwner();
	const TCHAR* MotionModeStr = (MotionMode == EMotionMode::Chaos) ? TEXT("Chaos") : TEXT("Kinematic");
	const TCHAR* UseModeStr = ShouldUseChaosMode() ? TEXT("Chaos") : TEXT("Kinematic");
	const int32 bForceChaos = bForceChaosMode ? 1 : 0;
	UE_LOG(LogProjectMotionSystem, Log,
		TEXT("[%s] %s BeginPlay: TargetMesh=%s ResolvedMesh=%s Reg=%d Phys=%d Coll=%d MotionMode=%s SelectedMode=%s ForceChaos=%d"),
		*GetNameSafe(Owner),
		*GetClass()->GetName(),
		TargetMesh ? *TargetMesh->GetName() : TEXT("null"),
		ResolvedMesh ? *ResolvedMesh->GetName() : TEXT("null"),
		ResolvedMesh ? ResolvedMesh->IsRegistered() : 0,
		ResolvedMesh ? ResolvedMesh->IsPhysicsStateCreated() : 0,
		ResolvedMesh ? (int32)ResolvedMesh->GetCollisionEnabled() : 0,
		MotionModeStr,
		UseModeStr,
		bForceChaos);
}

void USpringMotionComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* Owner = GetOwner();
	UE_LOG(LogProjectMotionSystem, Log,
		TEXT("[%s] EndPlay START: Reason=%d"),
		*GetNameSafe(Owner),
		(int)EndPlayReason);

	ShutdownMotionDriver();
	SetComponentTickEnabled(false);

	UE_LOG(LogProjectMotionSystem, Log,
		TEXT("[%s] EndPlay DONE"),
		*GetNameSafe(Owner));

	Super::EndPlay(EndPlayReason);
}

void USpringMotionComponentBase::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	AActor* Owner = GetOwner();
	UE_LOG(LogProjectMotionSystem, Log,
		TEXT("[%s] %s OnComponentDestroyed: DestroyingHierarchy=%d"),
		*GetNameSafe(Owner),
		*GetClass()->GetName(),
		bDestroyingHierarchy ? 1 : 0);

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void USpringMotionComponentBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!TickMotion(DeltaTime))
	{
		SetComponentTickEnabled(false);
	}
}

void USpringMotionComponentBase::Open()
{
	if (bDesiredOpen && !bIsAnimating && !bMotorActive)
	{
		return;  // Already open or heading there
	}

	bDesiredOpen = true;
	PlayOpenSound();

	DoOpen();
	SetComponentTickEnabled(true);
}

void USpringMotionComponentBase::Close()
{
	if (!bDesiredOpen && !bIsAnimating && !bMotorActive)
	{
		return;  // Already closed or heading there
	}

	bDesiredOpen = false;
	PlayCloseSound();

	DoClose();
	SetComponentTickEnabled(true);
}

void USpringMotionComponentBase::Toggle()
{
	// Use bDesiredOpen for correct reversal during animation/motor active
	if (bIsAnimating || bMotorActive)
	{
		// Reverse direction mid-motion
		if (bDesiredOpen)
		{
			Close();
		}
		else
		{
			Open();
		}
	}
	else if (bIsOpen)
	{
		Close();
	}
	else
	{
		Open();
	}
}

void USpringMotionComponentBase::HandleAction(const FString& Context, const FString& Action)
{
	// Authority guard: motion state should be driven by server to avoid desync
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogProjectMotionSystem, Warning,
			TEXT("[%s::HandleAction] Ignoring '%s' - not authority (actions must be dispatched on server)"),
			*GetNameSafe(GetOwner()), *Action);
		return;
	}

	if (Action == TEXT("motion.toggle"))
	{
		UE_LOG(LogProjectMotionSystem, Log,
			TEXT("[%s::HandleAction] motion.toggle (Context='%s')"),
			*GetNameSafe(GetOwner()), *Context);
		Toggle();
	}
	else if (Action == TEXT("motion.open"))
	{
		UE_LOG(LogProjectMotionSystem, Log,
			TEXT("[%s::HandleAction] motion.open (Context='%s')"),
			*GetNameSafe(GetOwner()), *Context);
		Open();
	}
	else if (Action == TEXT("motion.close"))
	{
		UE_LOG(LogProjectMotionSystem, Log,
			TEXT("[%s::HandleAction] motion.close (Context='%s')"),
			*GetNameSafe(GetOwner()), *Context);
		Close();
	}
}

bool USpringMotionComponentBase::OnComponentInteract_Implementation(AActor* Instigator)
{
	Toggle();
	return true; // Allow further components to process
}

FText USpringMotionComponentBase::GetInteractionLabel_Implementation() const
{
	return bIsOpen
		? NSLOCTEXT("Interaction", "Close", "Close")
		: NSLOCTEXT("Interaction", "Open", "Open");
}

UPrimitiveComponent* USpringMotionComponentBase::GetInteractTargetMesh_Implementation() const
{
	// Prefer resolved runtime mesh; fallback to explicit reference for pre-BeginPlay/editor paths.
	return ResolvedMesh ? ResolvedMesh : TargetMesh;
}

void USpringMotionComponentBase::SetInteractTargetMesh_Implementation(UPrimitiveComponent* InTargetMesh)
{
	UStaticMeshComponent* StaticMeshTarget = Cast<UStaticMeshComponent>(InTargetMesh);
	if (InTargetMesh && !StaticMeshTarget)
	{
		UE_LOG(LogProjectMotionSystem, Warning,
			TEXT("[%s] %s requires UStaticMeshComponent target, got '%s'"),
			*GetNameSafe(GetOwner()),
			*GetClass()->GetName(),
			*GetNameSafe(InTargetMesh));
		return;
	}

	TargetMesh = StaticMeshTarget;
	ResolvedMesh = StaticMeshTarget;
}

void USpringMotionComponentBase::ResolveMeshReference()
{
	// Use explicit reference if set
	if (TargetMesh)
	{
		ResolvedMesh = TargetMesh;
		return;
	}

	// Auto-find StaticMeshComponent on owner
	AActor* Owner = GetOwner();
	if (Owner)
	{
		ResolvedMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
		if (!ResolvedMesh)
		{
			UE_LOG(LogProjectMotionSystem, Warning,
				TEXT("%s on %s: No StaticMeshComponent found. Set TargetMesh explicitly."),
				*GetClass()->GetName(),
				*Owner->GetName());
		}
	}
}

void USpringMotionComponentBase::PlayOpenSound()
{
	if (OpenSound && GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetOwner()->GetActorLocation());
	}
}

void USpringMotionComponentBase::PlayCloseSound()
{
	if (CloseSound && GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, CloseSound, GetOwner()->GetActorLocation());
	}
}

bool USpringMotionComponentBase::ParseVector(const FString& Value, FVector& OutVec)
{
	// Try UE format first (X=1 Y=2 Z=3)
	FVector Tmp;
	if (Tmp.InitFromString(Value))
	{
		OutVec = Tmp;
		return true;
	}

	// Try tuple format (1,2,3) or (1, 2, 3)
	FString Clean = Value;
	Clean.TrimStartAndEndInline();
	Clean.RemoveFromStart(TEXT("("));
	Clean.RemoveFromEnd(TEXT(")"));

	TArray<FString> Parts;
	Clean.ParseIntoArray(Parts, TEXT(","), true);

	if (Parts.Num() == 3)
	{
		OutVec.X = FCString::Atof(*Parts[0]);
		OutVec.Y = FCString::Atof(*Parts[1]);
		OutVec.Z = FCString::Atof(*Parts[2]);
		return true;
	}

	return false;
}
