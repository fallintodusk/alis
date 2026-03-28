// Copyright ALIS. All Rights Reserved.

#include "Template/Interactable/InteractableActor.h"
#include "Data/ObjectDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"
#include "CapabilityRegistry.h"
#include "CapabilityHierarchyHelpers.h"
#include "Interfaces/IInteractableTarget.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteractableActor, Log, All);

namespace
{
	// Helper to set property by name (string value version)
	// Supports: FVector "(X,Y,Z)", FRotator "(P,Y,R)", FGameplayTag "Tag.Name", generic ImportText
	bool ImportPropertyValue(UObject* Object, FProperty* Property, void* PropertyAddr, const FString& Value)
	{
		if (!Object || !Property || !PropertyAddr)
		{
			return false;
		}

		// Special case: FGameplayTag (ImportText may not accept "Tag.Name" format)
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct == FGameplayTag::StaticStruct())
			{
				FGameplayTag* TagPtr = static_cast<FGameplayTag*>(PropertyAddr);
				*TagPtr = FGameplayTag::RequestGameplayTag(FName(*Value), false);
				if (!TagPtr->IsValid() && !Value.IsEmpty())
				{
					UE_LOG(LogInteractableActor, Error,
						TEXT("Invalid gameplay tag '%s' for %s.%s"),
						*Value,
						*GetNameSafe(Object),
						*Property->GetName());
				}
				return TagPtr->IsValid() || Value.IsEmpty();
			}

			// Special case: FVector (allow "(0,0,0)" and UE ImportText formats)
			if (StructProp->Struct == TBaseStructure<FVector>::Get())
			{
				FVector* VecPtr = static_cast<FVector*>(PropertyAddr);
				FString CleanValue = Value;
				CleanValue.RemoveFromStart(TEXT("("));
				CleanValue.RemoveFromEnd(TEXT(")"));
				TArray<FString> Components;
				CleanValue.ParseIntoArray(Components, TEXT(","));
				if (Components.Num() >= 3)
				{
					VecPtr->X = FCString::Atof(*Components[0]);
					VecPtr->Y = FCString::Atof(*Components[1]);
					VecPtr->Z = FCString::Atof(*Components[2]);
					return true;
				}
			}

			if (StructProp->Struct == TBaseStructure<FRotator>::Get())
			{
				FRotator* RotPtr = static_cast<FRotator*>(PropertyAddr);
				FString CleanValue = Value;
				CleanValue.RemoveFromStart(TEXT("("));
				CleanValue.RemoveFromEnd(TEXT(")"));
				TArray<FString> Components;
				CleanValue.ParseIntoArray(Components, TEXT(","));
				if (Components.Num() >= 3)
				{
					RotPtr->Pitch = FCString::Atof(*Components[0]);
					RotPtr->Yaw = FCString::Atof(*Components[1]);
					RotPtr->Roll = FCString::Atof(*Components[2]);
					return true;
				}
			}
		}

		// Generic case: use ImportText
		const TCHAR* ImportResult = Property->ImportText_Direct(*Value, PropertyAddr, Object, PPF_None);
		return ImportResult != nullptr;
	}

	// Helper to set property by name (string value version)
	// Supports: FVector "(X,Y,Z)", FRotator "(P,Y,R)", FGameplayTag "Tag.Name", generic ImportText
	// Also supports nested struct fields by name (one level deep).
	bool SetPropertyByName(UObject* Object, FName PropertyName, const FString& Value)
	{
		if (!Object)
		{
			return false;
		}

		FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		if (Property)
		{
			void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
			return ImportPropertyValue(Object, Property, PropertyAddr, Value);
		}

		// Fallback: allow setting nested struct fields by name (for config grouping).
		for (TFieldIterator<FStructProperty> It(Object->GetClass()); It; ++It)
		{
			FStructProperty* StructProp = *It;
			FProperty* InnerProp = StructProp->Struct->FindPropertyByName(PropertyName);
			if (!InnerProp)
			{
				continue;
			}

			void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(Object);
			void* InnerAddr = InnerProp->ContainerPtrToValuePtr<void>(StructAddr);
			return ImportPropertyValue(Object, InnerProp, InnerAddr, Value);
		}

		return false;
	}

	// Helper to set object property by name
	bool SetPropertyByName(UObject* Object, FName PropertyName, UObject* ObjectValue)
	{
		if (!Object)
		{
			return false;
		}

		FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		if (!Property)
		{
			return false;
		}

		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
		{
			void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
			ObjProp->SetObjectPropertyValue(PropertyAddr, ObjectValue);
			return true;
		}

		return false;
	}

	// Select exactly one capability for a given hit context.
	// Semantics match focus resolution: mesh-scoped match wins; otherwise actor-scoped fallback.
	UActorComponent* SelectBestInteractableComponentForHit(
		const TArray<UActorComponent*>& SortedCapabilities,
		UPrimitiveComponent* HitComponent)
	{
		if (SortedCapabilities.Num() == 0)
		{
			return nullptr;
		}

		// No mesh context: execute highest-priority capability only.
		if (!HitComponent)
		{
			return SortedCapabilities[0];
		}

		TSet<UPrimitiveComponent*> HierarchyMeshes;
		CapabilityHierarchyHelpers::CollectAttachmentHierarchy(HitComponent, HierarchyMeshes);

		UActorComponent* BestMeshScoped = nullptr;
		int32 BestMeshPriority = MIN_int32;
		UActorComponent* BestActorScoped = nullptr;
		int32 BestActorPriority = MIN_int32;

		for (UActorComponent* Comp : SortedCapabilities)
		{
			bool bHasMeshProperty = false;
			const bool bMatchesHierarchy = CapabilityHierarchyHelpers::CapabilityMatchesHierarchy(
				Comp, HierarchyMeshes, bHasMeshProperty);
			const int32 Priority = IInteractableComponentTargetInterface::Execute_GetInteractPriority(Comp);

			if (bMatchesHierarchy)
			{
				if (!BestMeshScoped || Priority > BestMeshPriority)
				{
					BestMeshScoped = Comp;
					BestMeshPriority = Priority;
				}
			}
			else if (!bHasMeshProperty)
			{
				if (!BestActorScoped || Priority > BestActorPriority)
				{
					BestActorScoped = Comp;
					BestActorPriority = Priority;
				}
			}
		}

		return BestMeshScoped ? BestMeshScoped : BestActorScoped;
	}
}

// -------------------------------------------------------------------------
// AInteractableActor Implementation
// -------------------------------------------------------------------------

AInteractableActor::AInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AInteractableActor::BeginPlay()
{
	Super::BeginPlay();
	CacheInteractableComponents();
}

bool AInteractableActor::OnInteract_Implementation(AActor* InteractInstigator, UPrimitiveComponent* HitComponent)
{
	if (CachedInteractableComponents.Num() == 0)
	{
		UE_LOG(LogInteractableActor, Warning, TEXT("[%s] OnInteract: No interactable capabilities cached"),
			*GetNameSafe(this));
		return false;
	}

	UActorComponent* Selected = SelectBestInteractableComponentForHit(CachedInteractableComponents, HitComponent);
	if (!Selected)
	{
		UE_LOG(LogInteractableActor, Warning, TEXT("[%s] OnInteract: No capability selected for hit mesh '%s'"),
			*GetNameSafe(this), HitComponent ? *HitComponent->GetName() : TEXT("<none>"));
		return false;
	}

	UE_LOG(LogInteractableActor, Verbose, TEXT("[%s] OnInteract: Selected '%s' (priority %d) for hit mesh '%s'"),
		*GetNameSafe(this),
		*Selected->GetClass()->GetName(),
		IInteractableComponentTargetInterface::Execute_GetInteractPriority(Selected),
		HitComponent ? *HitComponent->GetName() : TEXT("<none>"));

	return IInteractableComponentTargetInterface::Execute_OnComponentInteract(Selected, InteractInstigator);
}

FInteractionFocusInfo AInteractableActor::GetFocusInfo_Implementation(UPrimitiveComponent* HitComponent) const
{
	FInteractionFocusInfo Result;

	if (!HitComponent)
	{
		return Result;
	}

	UActorComponent* TargetCapability = SelectBestInteractableComponentForHit(CachedInteractableComponents, HitComponent);
	if (TargetCapability)
	{
		// Get label from capability
		Result.Label = IInteractableComponentTargetInterface::Execute_GetInteractionLabel(TargetCapability);

		// Get target mesh from capability (for correct highlighting)
		UPrimitiveComponent* CapabilityMesh = CapabilityHierarchyHelpers::GetCapabilityTargetMesh(TargetCapability);
		Result.HighlightMesh = CapabilityMesh ? CapabilityMesh : HitComponent;
	}

	return Result;
}

FInteractionExecutionSpec AInteractableActor::GetInteractionExecutionSpec_Implementation(AActor* InteractInstigator, UPrimitiveComponent* HitComponent) const
{
	UActorComponent* TargetCapability = SelectBestInteractableComponentForHit(CachedInteractableComponents, HitComponent);
	if (!TargetCapability)
	{
		return FInteractionExecutionSpec();
	}

	return IInteractableComponentTargetInterface::Execute_GetInteractionExecutionSpec(TargetCapability, InteractInstigator);
}

void AInteractableActor::RefreshInteractableComponents()
{
	CacheInteractableComponents();
}

void AInteractableActor::CacheInteractableComponents()
{
	CachedInteractableComponents.Reset();

	for (UActorComponent* Comp : GetComponents())
	{
		if (Comp && Comp->Implements<UInteractableComponentTargetInterface>())
		{
			CachedInteractableComponents.Add(Comp);
		}
	}

	// Sort by priority descending (higher priority first)
	CachedInteractableComponents.Sort([](const UActorComponent& A, const UActorComponent& B)
	{
		int32 PriorityA = IInteractableComponentTargetInterface::Execute_GetInteractPriority(&A);
		int32 PriorityB = IInteractableComponentTargetInterface::Execute_GetInteractPriority(&B);
		return PriorityA > PriorityB;
	});
}

bool AInteractableActor::ApplyDefinition_Implementation(UPrimaryDataAsset* Definition)
{
	// Cast to UObjectDefinition (this actor type handles ObjectDefinition)
	UObjectDefinition* ObjDef = Cast<UObjectDefinition>(Definition);
	if (!ObjDef)
	{
		UE_LOG(LogInteractableActor, Error, TEXT("[%s] ApplyDefinition: Expected UObjectDefinition, got %s"),
			*GetNameSafe(this),
			Definition ? *Definition->GetClass()->GetName() : TEXT("null"));
		return false;
	}

	// Check content hash - if already up to date, skip update (idempotent)
	if (!AppliedContentHash.IsEmpty() && AppliedContentHash == ObjDef->DefinitionContentHash)
	{
		// Already up to date - no work needed
		return true;
	}

	// Structure mismatch: definition has structural changes (meshes/capabilities changed).
	// Cannot apply in-place - ActorSync should auto-replace when definition is regenerated.
	if (!AppliedStructureHash.IsEmpty() && AppliedStructureHash != ObjDef->DefinitionStructureHash)
	{
		UE_LOG(LogInteractableActor, Log, TEXT("[%s] Structural changes detected, cannot update in-place. ActorSync should auto-replace when definition regenerated. (Hash: %s -> %s)"),
			*GetNameSafe(this), *AppliedStructureHash, *ObjDef->DefinitionStructureHash);
		return false;
	}

	const double StartTime = FPlatformTime::Seconds();

	Modify(); // Enable undo

	UE_LOG(LogInteractableActor, Log, TEXT("[%s] Applying definition: %s"),
		*GetNameSafe(this), *ObjDef->ObjectId.ToString());

	// Build mesh map by finding components via their DefMeshId tag (NOT asset path!)
	// Tag format: "DefMeshId=<id>" - set by ProjectObjectActorFactory during spawn.
	// UMeshComponent* is the common base of UStaticMeshComponent and USkeletalMeshComponent,
	// so the map works for both mesh types created by ObjectSpawnUtility.
	TMap<FName, UMeshComponent*> MeshMap;
	for (UActorComponent* Comp : GetComponents())
	{
		UMeshComponent* Mesh = Cast<UMeshComponent>(Comp);
		if (!Mesh)
		{
			continue;
		}

		for (const FName& Tag : Mesh->ComponentTags)
		{
			FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(TEXT("DefMeshId=")))
			{
				FName MeshId = FName(*TagStr.Mid(10));
				MeshMap.Add(MeshId, Mesh);
				break;
			}
		}
	}

	// Update mesh assets AND transforms
	for (const FObjectMeshEntry& Entry : ObjDef->Meshes)
	{
		if (UMeshComponent** Found = MeshMap.Find(Entry.Id))
		{
			UMeshComponent* Mesh = *Found;
			Mesh->Modify();

			// Load asset as UObject - runtime cast determines mesh type
			UObject* LoadedAsset = Entry.Asset.LoadSynchronous();

			// Update mesh asset if changed, branching on component type
			if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Mesh))
			{
				UStaticMesh* NewMesh = Cast<UStaticMesh>(LoadedAsset);
				if (NewMesh && SMC->GetStaticMesh() != NewMesh)
				{
					SMC->SetStaticMesh(NewMesh);
					UE_LOG(LogInteractableActor, Verbose, TEXT("  Updated static mesh for '%s'"), *Entry.Id.ToString());
				}

				// Restore defaults when no material overrides specified
				if (Entry.Materials.Num() == 0 && NewMesh)
				{
					bool bRestoredDefaults = false;
					for (int32 SlotIndex = 0; SlotIndex < NewMesh->GetStaticMaterials().Num(); ++SlotIndex)
					{
						UMaterialInterface* DefaultMat = NewMesh->GetStaticMaterials()[SlotIndex].MaterialInterface;
						if (SMC->GetMaterial(SlotIndex) != DefaultMat)
						{
							SMC->SetMaterial(SlotIndex, DefaultMat);
							bRestoredDefaults = true;
						}
					}
					if (bRestoredDefaults)
					{
						UE_LOG(LogInteractableActor, Verbose, TEXT("  Restored default materials for '%s'"), *Entry.Id.ToString());
					}
				}
			}
			else if (USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(Mesh))
			{
				USkeletalMesh* NewSkelMesh = Cast<USkeletalMesh>(LoadedAsset);
				if (NewSkelMesh && SKC->GetSkeletalMeshAsset() != NewSkelMesh)
				{
					SKC->SetSkeletalMesh(NewSkelMesh);
					UE_LOG(LogInteractableActor, Verbose, TEXT("  Updated skeletal mesh for '%s'"), *Entry.Id.ToString());
				}

				// Update AnimClass if specified in definition
				if (!Entry.AnimClass.IsNull())
				{
					if (UClass* AnimBPClass = Entry.AnimClass.LoadSynchronous())
					{
						SKC->SetAnimationMode(EAnimationMode::AnimationBlueprint);
						SKC->SetAnimInstanceClass(AnimBPClass);
					}
				}
			}

			// Material overrides - SetMaterial is on UMeshComponent base, works for both types
			if (Entry.Materials.Num() > 0)
			{
				bool bMaterialsChanged = false;
				for (int32 SlotIndex = 0; SlotIndex < Entry.Materials.Num(); ++SlotIndex)
				{
					const TSoftObjectPtr<UMaterialInterface>& MaterialPtr = Entry.Materials[SlotIndex];
					if (MaterialPtr.IsNull())
					{
						continue;
					}

					UMaterialInterface* TargetMaterial = MaterialPtr.LoadSynchronous();
					if (!TargetMaterial)
					{
						UE_LOG(LogInteractableActor, Warning,
							TEXT("  [%s] Failed to load material slot %d: %s"),
							*Entry.Id.ToString(), SlotIndex, *MaterialPtr.ToString());
						continue;
					}

					if (TargetMaterial != Mesh->GetMaterial(SlotIndex))
					{
						Mesh->SetMaterial(SlotIndex, TargetMaterial);
						bMaterialsChanged = true;
					}
				}
				if (bMaterialsChanged)
				{
					UE_LOG(LogInteractableActor, Verbose, TEXT("  Updated materials for '%s'"), *Entry.Id.ToString());
				}
			}

			// Update transform
			const FTransform OldTransform = Mesh->GetRelativeTransform();
			const FTransform NewTransform = Entry.Transform.ToTransform();
			Mesh->SetRelativeTransform(NewTransform);
			UE_LOG(LogInteractableActor, Log, TEXT("  [%s] Transform: %s -> %s"),
				*Entry.Id.ToString(),
				*OldTransform.GetLocation().ToString(),
				*NewTransform.GetLocation().ToString());
		}
		else
		{
			UE_LOG(LogInteractableActor, Warning,
				TEXT("  Mesh '%s' not found on actor (missing DefMeshId tag?)"), *Entry.Id.ToString());
		}
	}

	// Create and configure capability components
	// Each capability entry gets its own component (supports multiple hinges/sliders)
	// Components are tagged with DefCapId for identification during updates
	for (const FObjectCapabilityEntry& CapEntry : ObjDef->Capabilities)
	{
		UClass* CapClass = FCapabilityRegistry::GetCapabilityClass(CapEntry.Type);
		if (!CapClass)
		{
			UE_LOG(LogInteractableActor, Warning,
				TEXT("  Unknown capability type: %s"), *CapEntry.Type.ToString());
			continue;
		}

		// Build unique tag for this capability (Type:Scope or Type:actor)
		// Used to find existing component during re-apply
		FString ScopeStr = CapEntry.Scope.Num() > 0 ? CapEntry.Scope[0].ToString() : TEXT("actor");
		FName CapTag = FName(*FString::Printf(TEXT("DefCapId=%s:%s"), *CapEntry.Type.ToString(), *ScopeStr));

		// Find existing component with matching tag (for re-apply/update)
		UActorComponent* Comp = nullptr;
		for (UActorComponent* Existing : GetComponents())
		{
			if (Existing && Existing->IsA(CapClass) && Existing->ComponentTags.Contains(CapTag))
			{
				Comp = Existing;
				break;
			}
		}

		// Create component if doesn't exist (with proper GC ownership)
		if (!Comp)
		{
			Comp = NewObject<UActorComponent>(this, CapClass, NAME_None, RF_Transient);
			AddInstanceComponent(Comp);
			Comp->ComponentTags.Add(CapTag);
			Comp->RegisterComponent();
			UE_LOG(LogInteractableActor, Log, TEXT("  Created capability component: %s (%s)"),
				*CapClass->GetName(), *CapTag.ToString());
		}

		Comp->Modify(); // Enable undo for this component

		// Update mesh scope reference via strict interaction interface contract.
		// RULE: Scoped capabilities must have 0 or 1 non-actor scope ID
		FName NonActorScopeId = NAME_None;
		for (const FName& ScopeId : CapEntry.Scope)
		{
			// "actor" scope means no specific mesh target (capability attached to root)
			if (ScopeId != FName(TEXT("actor")))
			{
				if (NonActorScopeId != NAME_None)
				{
					UE_LOG(LogInteractableActor, Warning,
						TEXT("  Capability %s has multiple non-actor scopes - only first mesh scope is used"),
						*CapEntry.Type.ToString());
					break;
				}
				NonActorScopeId = ScopeId;
			}
		}

		if (NonActorScopeId != NAME_None)
		{
			if (UMeshComponent** Found = MeshMap.Find(NonActorScopeId))
			{
				if (Comp->Implements<UInteractableComponentTargetInterface>())
				{
					IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(Comp, *Found);
					if (!IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(Comp))
					{
						UE_LOG(LogInteractableActor, Warning,
							TEXT("  Capability %s did not accept mesh scope '%s'"),
							*CapEntry.Type.ToString(),
							*GetNameSafe(*Found));
					}
				}
				else
				{
					UE_LOG(LogInteractableActor, Warning,
						TEXT("  Capability %s does not implement interaction target interface for mesh scope '%s'"),
						*CapEntry.Type.ToString(),
						*NonActorScopeId.ToString());
				}
			}
		}

		// Apply property values from definition
		for (const auto& Prop : CapEntry.Properties)
		{
			if (SetPropertyByName(Comp, Prop.Key, Prop.Value))
			{
				UE_LOG(LogInteractableActor, Verbose, TEXT("  Set %s.%s = %s"),
					*CapClass->GetName(), *Prop.Key.ToString(), *Prop.Value);
			}
			else
			{
				UE_LOG(LogInteractableActor, Warning, TEXT("  Failed to set %s.%s = %s"),
					*CapClass->GetName(), *Prop.Key.ToString(), *Prop.Value);
			}
		}

		// Motion mode opt-in (explicit): MotionMode = "Chaos"
		const FString* MotionModeValue = CapEntry.Properties.Find(FName(TEXT("MotionMode")));
		if (MotionModeValue)
		{
			UE_LOG(LogInteractableActor, Verbose, TEXT("  MotionMode=%s for %s"),
				**MotionModeValue, *CapClass->GetName());
		}
		else if (CapEntry.bUsePhysicsMode)
		{
			UE_LOG(LogInteractableActor, Warning,
				TEXT("  %s has legacy bUsePhysicsMode set, but MotionMode is missing. Defaulting to Kinematic. Set MotionMode=\"Chaos\" to opt in."),
				*CapClass->GetName());
		}
	}

	// Refresh interaction cache
	RefreshInteractableComponents();

	// Ensure definition actor tags exist on the instance (non-destructive merge).
	for (const FName& ActorTag : ObjDef->ActorTags)
	{
		if (!ActorTag.IsNone() && !Tags.Contains(ActorTag))
		{
			Tags.Add(ActorTag);
		}
	}

	// Update hash values to match applied definition
	ObjectDefinitionId = ObjDef->GetPrimaryAssetId();
	AppliedStructureHash = ObjDef->DefinitionStructureHash;
	AppliedContentHash = ObjDef->DefinitionContentHash;

	// Mark package dirty
	MarkPackageDirty();

	const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	UE_LOG(LogInteractableActor, Log, TEXT("[%s] ApplyDefinition complete (%.2fms)"), *GetNameSafe(this), ElapsedMs);

	return true;
}
