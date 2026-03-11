// Copyright ALIS. All Rights Reserved.

#include "Solvers/MotionConstraintHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "ProjectMotionSystemModule.h"

UPrimitiveComponent* FMotionConstraintHelpers::FindOrCreateAnchor(
	UStaticMeshComponent* TargetMesh,
	UBoxComponent*& OutCreatedAnchor)
{
	OutCreatedAnchor = nullptr;

	if (!TargetMesh)
	{
		return nullptr;
	}

	AActor* Owner = TargetMesh->GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// Try parent first (e.g., door frame mesh)
	if (UPrimitiveComponent* ParentPrim = Cast<UPrimitiveComponent>(TargetMesh->GetAttachParent()))
	{
		return ParentPrim;
	}

	// Try actor root
	if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		return RootPrim;
	}

	// Create invisible anchor with physics body (required for Chaos constraint)
	// Must use AddInstanceComponent for proper GC ownership
	OutCreatedAnchor = NewObject<UBoxComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(OutCreatedAnchor);
	OutCreatedAnchor->OnComponentCreated();

	// Configure as invisible kinematic anchor with physics body
	OutCreatedAnchor->SetMobility(EComponentMobility::Movable);
	OutCreatedAnchor->SetBoxExtent(FVector(1.f));  // Tiny box
	OutCreatedAnchor->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);  // Creates physics body for constraint binding
	OutCreatedAnchor->SetCollisionResponseToAllChannels(ECR_Ignore);  // Ignore all collision
	OutCreatedAnchor->SetGenerateOverlapEvents(false);  // No overlap events needed
	OutCreatedAnchor->SetSimulatePhysics(false);  // Kinematic anchor (immovable)
	OutCreatedAnchor->SetHiddenInGame(true);
	OutCreatedAnchor->SetVisibility(false);
	OutCreatedAnchor->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	OutCreatedAnchor->RegisterComponent();

	UE_LOG(LogProjectMotionSystem, Verbose,
		TEXT("FMotionConstraintHelpers: Created invisible anchor box for %s"),
		*Owner->GetName());

	return OutCreatedAnchor;
}

void FMotionConstraintHelpers::EnablePhysicsOnMesh(UStaticMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}

	// Ignore collision with parent/body meshes to prevent explosion when physics enables
	// Door overlapping body collision would push them apart violently
	if (UPrimitiveComponent* ParentPrim = Cast<UPrimitiveComponent>(Mesh->GetAttachParent()))
	{
		Mesh->IgnoreComponentWhenMoving(ParentPrim, true);
		ParentPrim->IgnoreComponentWhenMoving(Mesh, true);
	}

	// Also ignore root component if different from parent
	AActor* Owner = Mesh->GetOwner();
	if (Owner)
	{
		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			if (RootPrim != Mesh->GetAttachParent())
			{
				Mesh->IgnoreComponentWhenMoving(RootPrim, true);
				RootPrim->IgnoreComponentWhenMoving(Mesh, true);
			}
		}
	}

	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetEnableGravity(false);
}
