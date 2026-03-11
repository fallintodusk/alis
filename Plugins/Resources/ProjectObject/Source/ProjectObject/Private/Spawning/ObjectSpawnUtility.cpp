// Copyright ALIS. All Rights Reserved.

#include "Spawning/ObjectSpawnUtility.h"
#include "Data/ObjectDefinition.h"
#include "Template/Interactable/InteractableActor.h"
#include "ObjectDefinitionHostHelpers.h"
#include "ProjectObjectModule.h"
#include "CapabilityRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Interfaces/IInteractableTarget.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Character.h"

namespace
{
	static const FName NAME_DefaultObjectAttachRootTag(TEXT("ObjectAttachRoot"));

	// Null-safe error setter with logging
	void SetError(FText* OutError, const FText& Msg)
	{
		if (OutError)
		{
			*OutError = Msg;
		}
		UE_LOG(LogProjectObject, Error, TEXT("ObjectSpawn: %s"), *Msg.ToString());
	}

	// Ensure capability modules loaded before CDO scan (required for cooked builds)
	// Returns false and sets OutError if modules fail to load
	bool EnsureCapabilityModulesLoaded(FText* OutError)
	{
		static bool bDone = false;
		static bool bSuccess = true;
		static FText CachedError;

		if (bDone)
		{
			// Re-set error on subsequent calls if first call failed
			if (!bSuccess && OutError)
			{
				*OutError = CachedError;
			}
			return bSuccess;
		}
		bDone = true;

		// Use LoadModule with bIsRequired=false to avoid hard crash
		IModuleInterface* CapModule = FModuleManager::Get().LoadModule(TEXT("ProjectObjectCapabilities"));
		if (!CapModule)
		{
			CachedError = NSLOCTEXT("ObjectSpawn", "ModuleLoadFailed",
				"Failed to load ProjectObjectCapabilities module");
			SetError(OutError, CachedError);
			bSuccess = false;
			return false;
		}

		IModuleInterface* MotionModule = FModuleManager::Get().LoadModule(TEXT("ProjectMotionSystem"));
		if (!MotionModule)
		{
			CachedError = NSLOCTEXT("ObjectSpawn", "ModuleLoadFailed2",
				"Failed to load ProjectMotionSystem module");
			SetError(OutError, CachedError);
			bSuccess = false;
			return false;
		}

		return true;
	}

	bool ValidateSpawnClass(UClass* ActorClass, FString& OutReason)
	{
		if (!ActorClass)
		{
			OutReason = TEXT("Class is null");
			return false;
		}

		if (!ActorClass->IsChildOf(AActor::StaticClass()))
		{
			OutReason = TEXT("Class is not an AActor");
			return false;
		}

		if (ActorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			OutReason = TEXT("Class is abstract");
			return false;
		}

		if (ActorClass->HasAnyClassFlags(CLASS_NotPlaceable))
		{
			OutReason = TEXT("Class is not placeable");
			return false;
		}

		if (ActorClass->HasAnyClassFlags(CLASS_Deprecated))
		{
			OutReason = TEXT("Class is deprecated");
			return false;
		}

		if (ActorClass->IsChildOf(AController::StaticClass()))
		{
			OutReason = TEXT("Controller-derived classes are forbidden");
			return false;
		}

		if (ActorClass->IsChildOf(AWorldSettings::StaticClass()))
		{
			OutReason = TEXT("WorldSettings-derived classes are forbidden");
			return false;
		}

		if (ActorClass->IsChildOf(AGameModeBase::StaticClass()))
		{
			OutReason = TEXT("GameMode-derived classes are forbidden");
			return false;
		}

		return true;
	}

	USceneComponent* ResolveDefinitionAttachRoot(AActor* Actor, const UObjectDefinition* Def)
	{
		if (!Actor)
		{
			return nullptr;
		}

		// Definition-specific override tag has highest priority
		const FName AttachTag = (Def && !Def->AttachToComponentTag.IsNone())
			? Def->AttachToComponentTag
			: NAME_DefaultObjectAttachRootTag;

		if (!AttachTag.IsNone())
		{
			TInlineComponentArray<USceneComponent*> SceneComponents(Actor);
			for (USceneComponent* SceneComp : SceneComponents)
			{
				if (SceneComp && SceneComp->ComponentHasTag(AttachTag))
				{
					return SceneComp;
				}
			}

			// If definition requested explicit tag and it's not found, emit warning and continue to root.
			if (Def && !Def->AttachToComponentTag.IsNone())
			{
				UE_LOG(LogProjectObject, Warning,
					TEXT("ObjectSpawn: attachToComponentTag '%s' not found on actor class '%s' for '%s'. Falling back to root."),
					*Def->AttachToComponentTag.ToString(),
					*GetNameSafe(Actor->GetClass()),
					*Def->ObjectId.ToString());
			}
		}

		if (USceneComponent* Root = Actor->GetRootComponent())
		{
			return Root;
		}

		// Root is missing - create a simple scene root as fallback.
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Actor->AddInstanceComponent(Root);
		Root->RegisterComponent();
		return Root;
	}

	FName MakeDefinitionMeshTag(const FName MeshId)
	{
		return FName(*FString::Printf(TEXT("DefMeshId=%s"), *MeshId.ToString()));
	}

	bool HasDefinitionMeshTag(const UMeshComponent* MeshComponent, const FName MeshId)
	{
		return MeshComponent && MeshComponent->ComponentTags.Contains(MakeDefinitionMeshTag(MeshId));
	}

	bool IsCompatibleMeshComponent(const UObject* LoadedAsset, const UMeshComponent* MeshComponent)
	{
		if (!LoadedAsset || !MeshComponent)
		{
			return false;
		}

		if (LoadedAsset->IsA<UStaticMesh>())
		{
			return MeshComponent->IsA<UStaticMeshComponent>();
		}

		if (LoadedAsset->IsA<USkeletalMesh>())
		{
			return MeshComponent->IsA<USkeletalMeshComponent>();
		}

		return false;
	}

	UMeshComponent* FindReusableMeshComponent(
		AActor* SpawnedActor,
		const FObjectMeshEntry& Entry,
		const UObject* LoadedAsset,
		USceneComponent* DefaultAttachRoot,
		const TArray<UMeshComponent*>& ExistingMeshComponents,
		const TSet<UMeshComponent*>& ClaimedExistingMeshComponents)
	{
		if (!SpawnedActor || !LoadedAsset)
		{
			return nullptr;
		}

		// Highest priority: explicit mesh id binding.
		for (UMeshComponent* ExistingMesh : ExistingMeshComponents)
		{
			if (!ExistingMesh || ClaimedExistingMeshComponents.Contains(ExistingMesh))
			{
				continue;
			}

			if (!IsCompatibleMeshComponent(LoadedAsset, ExistingMesh))
			{
				continue;
			}

			if (HasDefinitionMeshTag(ExistingMesh, Entry.Id))
			{
				return ExistingMesh;
			}
		}

		// Character path: body mesh should normally reuse CharacterMesh0.
		if (LoadedAsset->IsA<USkeletalMesh>() && Entry.Parent.IsNone())
		{
			if (ACharacter* Character = Cast<ACharacter>(SpawnedActor))
			{
				if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
				{
					if (!ClaimedExistingMeshComponents.Contains(CharacterMesh)
						&& IsCompatibleMeshComponent(LoadedAsset, CharacterMesh))
					{
						return CharacterMesh;
					}
				}
			}
		}

		// Attach-root hint path.
		if (Entry.Parent.IsNone())
		{
			if (UMeshComponent* RootMesh = Cast<UMeshComponent>(DefaultAttachRoot))
			{
				if (!ClaimedExistingMeshComponents.Contains(RootMesh)
					&& IsCompatibleMeshComponent(LoadedAsset, RootMesh))
				{
					return RootMesh;
				}
			}
		}

		// Fallback: only reuse when there is exactly one compatible mesh component.
		UMeshComponent* SingleCompatible = nullptr;
		for (UMeshComponent* ExistingMesh : ExistingMeshComponents)
		{
			if (!ExistingMesh || ClaimedExistingMeshComponents.Contains(ExistingMesh))
			{
				continue;
			}

			if (!IsCompatibleMeshComponent(LoadedAsset, ExistingMesh))
			{
				continue;
			}

			if (SingleCompatible)
			{
				return nullptr;
			}

			SingleCompatible = ExistingMesh;
		}

		return SingleCompatible;
	}
}

namespace ProjectObjectSpawn
{

uint32 ComputeStructureHash(const UObjectDefinition* Def)
{
	if (!Def)
	{
		return 0;
	}

	uint32 Hash = 0;

	// Collect and sort mesh IDs
	TArray<FName> MeshIds;
	for (const FObjectMeshEntry& Mesh : Def->Meshes)
	{
		MeshIds.Add(Mesh.Id);
	}
	MeshIds.Sort(FNameLexicalLess());
	for (const FName& Id : MeshIds)
	{
		Hash = HashCombine(Hash, GetTypeHash(Id));
	}

	// Collect and sort capability signatures
	TArray<FString> CapSignatures;
	for (const FObjectCapabilityEntry& Cap : Def->Capabilities)
	{
		TArray<FName> SortedScopes = Cap.Scope;
		SortedScopes.Sort(FNameLexicalLess());
		FString Sig = Cap.Type.ToString();
		for (const FName& S : SortedScopes)
		{
			Sig += TEXT(":") + S.ToString();
		}
		CapSignatures.Add(Sig);
	}
	CapSignatures.Sort();
	for (const FString& Sig : CapSignatures)
	{
		Hash = HashCombine(Hash, GetTypeHash(Sig));
	}

	return Hash;
}

bool SetPropertyByName(UObject* Object, FName PropertyName, const FString& Value)
{
	if (!Object)
	{
		return false;
	}

	// Find property - try exact name first
	FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);

	// Fallback: try with 'b' prefix for booleans (JSON "CanBeDropped" -> UPROPERTY "bCanBeDropped")
	if (!Property)
	{
		FString PrefixedName = FString::Printf(TEXT("b%s"), *PropertyName.ToString());
		Property = Object->GetClass()->FindPropertyByName(FName(*PrefixedName));
	}

	if (!Property)
	{
		return false;
	}

	void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);

	// -------------------------------------------------------------------------
	// FText special case (FText is not a USTRUCT, uses FTextProperty)
	// -------------------------------------------------------------------------
	if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		FText* TextPtr = TextProp->GetPropertyValuePtr(PropertyAddr);
		*TextPtr = FText::FromString(Value);
		return true;
	}

	// -------------------------------------------------------------------------
	// Struct property special cases
	// -------------------------------------------------------------------------
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		// FGameplayTag: "Tag.Name" format
		if (StructProp->Struct == FGameplayTag::StaticStruct())
		{
			FGameplayTag* TagPtr = static_cast<FGameplayTag*>(PropertyAddr);
			*TagPtr = FGameplayTag::RequestGameplayTag(FName(*Value), false);
			if (!TagPtr->IsValid() && !Value.IsEmpty())
			{
				UE_LOG(LogProjectObject, Error,
					TEXT("Invalid gameplay tag '%s' for %s.%s"),
					*Value,
					*GetNameSafe(Object),
					*PropertyName.ToString());
			}
			return TagPtr->IsValid() || Value.IsEmpty();
		}

		// FGameplayTagContainer: comma-separated "Tag.A,Tag.B,Tag.C"
		// Fails if any tag is invalid (strict validation - bad JSON should not spawn)
		if (StructProp->Struct == FGameplayTagContainer::StaticStruct())
		{
			FGameplayTagContainer* Container = static_cast<FGameplayTagContainer*>(PropertyAddr);
			Container->Reset();

			if (!Value.IsEmpty())
			{
				TArray<FString> TagStrings;
				Value.ParseIntoArray(TagStrings, TEXT(","));
				for (const FString& TagStr : TagStrings)
				{
					FString Trimmed = TagStr.TrimStartAndEnd();
					if (!Trimmed.IsEmpty())
					{
						FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Trimmed), false);
						if (Tag.IsValid())
						{
							Container->AddTag(Tag);
						}
						else
						{
							UE_LOG(LogProjectObject, Error, TEXT("Invalid gameplay tag: '%s'"), *Trimmed);
							return false;
						}
					}
				}
			}
			return true;
		}

		// FVector: "(X=0 Y=0 Z=0)" or "(0,0,0)" format
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			FVector* VecPtr = static_cast<FVector*>(PropertyAddr);
			return VecPtr->InitFromString(Value);
		}

		// FRotator: "(P=0 Y=0 R=0)" or "(0,0,0)" format
		if (StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			FRotator* RotPtr = static_cast<FRotator*>(PropertyAddr);
			return RotPtr->InitFromString(Value);
		}

		// FIntPoint: "X,Y" format (for GridSize)
		if (StructProp->Struct == TBaseStructure<FIntPoint>::Get())
		{
			FIntPoint* PointPtr = static_cast<FIntPoint*>(PropertyAddr);
			FString CleanValue = Value;
			CleanValue.RemoveFromStart(TEXT("("));
			CleanValue.RemoveFromEnd(TEXT(")"));
			TArray<FString> Components;
			CleanValue.ParseIntoArray(Components, TEXT(","));
			if (Components.Num() >= 2)
			{
				PointPtr->X = FCString::Atoi(*Components[0].TrimStartAndEnd());
				PointPtr->Y = FCString::Atoi(*Components[1].TrimStartAndEnd());
				return true;
			}
			return false;
		}
	}

	// -------------------------------------------------------------------------
	// Array property special cases
	// -------------------------------------------------------------------------
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		// TArray<TSoftObjectPtr<...>>: semicolon-separated "/Game/Path1;/Game/Path2"
		if (FSoftObjectProperty* InnerSoftProp = CastField<FSoftObjectProperty>(ArrayProp->Inner))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, PropertyAddr);
			ArrayHelper.EmptyValues();

			if (!Value.IsEmpty())
			{
				TArray<FString> Paths;
				Value.ParseIntoArray(Paths, TEXT(";"));

				for (const FString& Path : Paths)
				{
					FString Trimmed = Path.TrimStartAndEnd();
					if (!Trimmed.IsEmpty())
					{
						int32 Index = ArrayHelper.AddValue();
						FSoftObjectPtr* SoftPtr = reinterpret_cast<FSoftObjectPtr*>(ArrayHelper.GetRawPtr(Index));
						*SoftPtr = FSoftObjectPath(Trimmed);
					}
				}
			}
			return true;
		}
	}

	// -------------------------------------------------------------------------
	// Map property special cases
	// -------------------------------------------------------------------------
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		// TMap<FGameplayTag, float>: "Tag1=1.0;Tag2=2.5" format
		// Fails if any tag key is invalid (strict validation - bad JSON should not spawn)
		FStructProperty* KeyStructProp = CastField<FStructProperty>(MapProp->KeyProp);
		FFloatProperty* ValueFloatProp = CastField<FFloatProperty>(MapProp->ValueProp);

		if (KeyStructProp && KeyStructProp->Struct == FGameplayTag::StaticStruct() && ValueFloatProp)
		{
			FScriptMapHelper MapHelper(MapProp, PropertyAddr);
			MapHelper.EmptyValues();

			if (!Value.IsEmpty())
			{
				TArray<FString> Pairs;
				Value.ParseIntoArray(Pairs, TEXT(";"));

				for (const FString& Pair : Pairs)
				{
					FString Trimmed = Pair.TrimStartAndEnd();
					FString TagStr, ValueStr;
					if (Trimmed.Split(TEXT("="), &TagStr, &ValueStr))
					{
						FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr.TrimStartAndEnd()), false);
						float FloatValue = FCString::Atof(*ValueStr.TrimStartAndEnd());

						if (Tag.IsValid())
						{
							int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
							FGameplayTag* KeyPtr = reinterpret_cast<FGameplayTag*>(MapHelper.GetKeyPtr(Index));
							float* ValPtr = reinterpret_cast<float*>(MapHelper.GetValuePtr(Index));
							*KeyPtr = Tag;
							*ValPtr = FloatValue;
						}
						else
						{
							UE_LOG(LogProjectObject, Error, TEXT("Invalid gameplay tag in map: '%s'"), *TagStr);
							return false;
						}
					}
				}
				MapHelper.Rehash();
			}
			return true;
		}
	}

	// -------------------------------------------------------------------------
	// Generic case: use ImportText for universal parsing
	// -------------------------------------------------------------------------
	const TCHAR* ImportResult = Property->ImportText_Direct(*Value, PropertyAddr, Object, PPF_None);
	return ImportResult != nullptr;
}

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

	// Handle TObjectPtr and raw pointer properties
	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
		ObjProp->SetObjectPropertyValue(PropertyAddr, ObjectValue);
		return true;
	}

	return false;
}

AActor* SpawnFromDefinition(
	UWorld* World,
	const UObjectDefinition* Def,
	const FTransform& Transform,
	const FActorSpawnParameters& InSpawnParams,
	FText* OutError,
	TSubclassOf<AActor> ActorClass)
{
	if (!World)
	{
		SetError(OutError, NSLOCTEXT("ObjectSpawn", "NoWorld", "World is null"));
		return nullptr;
	}

	if (!Def)
	{
		SetError(OutError, NSLOCTEXT("ObjectSpawn", "NoDef", "ObjectDefinition is null"));
		return nullptr;
	}

	const FPrimaryAssetId DefinitionId = Def->GetPrimaryAssetId();
	if (!DefinitionId.IsValid())
	{
		SetError(OutError, FText::Format(
			NSLOCTEXT("ObjectSpawn", "InvalidDefinitionId",
				"ObjectDefinition '{0}' has empty id. Regenerate definition before placement."),
			FText::FromString(GetNameSafe(Def))));
		return nullptr;
	}

	// Ensure capability modules loaded (required for cooked builds)
	if (!EnsureCapabilityModulesLoaded(OutError))
	{
		return nullptr;
	}

	// Resolve spawn class: explicit override -> definition SpawnClass -> legacy default.
	UClass* ResolvedSpawnClass = ActorClass.Get();
	if (!ResolvedSpawnClass && !Def->SpawnClass.IsNull())
	{
		ResolvedSpawnClass = Def->SpawnClass.LoadSynchronous();
		if (!ResolvedSpawnClass)
		{
			UE_LOG(LogProjectObject, Error,
				TEXT("ObjectSpawn: Failed to load spawnClass '%s' for '%s'. Falling back to legacy default."),
				*Def->SpawnClass.ToString(),
				*Def->ObjectId.ToString());
		}
	}

	if (!ResolvedSpawnClass)
	{
		// LEGACY_OBJECT_PARENT_GENERALIZATION(L001): Default spawn class remains AInteractableActor for definitions
		// without spawnClass. Remove when all targeted definitions use explicit parent/host path and fallback-free
		// smoke tests pass.
		ResolvedSpawnClass = AInteractableActor::StaticClass();
	}

	// Validate class safety. If invalid, fallback to legacy class when possible.
	FString ClassValidationReason;
	if (!ValidateSpawnClass(ResolvedSpawnClass, ClassValidationReason))
	{
		UE_LOG(LogProjectObject, Error,
			TEXT("ObjectSpawn: Rejected spawn class '%s' for '%s' (%s). Falling back to AInteractableActor."),
			*GetNameSafe(ResolvedSpawnClass),
			*Def->ObjectId.ToString(),
			*ClassValidationReason);

		ResolvedSpawnClass = AInteractableActor::StaticClass();
		if (!ValidateSpawnClass(ResolvedSpawnClass, ClassValidationReason))
		{
			SetError(OutError, FText::Format(
				NSLOCTEXT("ObjectSpawn", "InvalidSpawnClass", "Spawn class is invalid for '{0}'"),
				FText::FromName(Def->ObjectId)));
			return nullptr;
		}
	}

	// 1. Spawn base actor (use provided params, set collision handling if not specified)
	FActorSpawnParameters SpawnParams = InSpawnParams;
	if (SpawnParams.SpawnCollisionHandlingOverride == ESpawnActorCollisionHandlingMethod::Undefined)
	{
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	}

	AActor* SpawnedActor = World->SpawnActor(ResolvedSpawnClass, &Transform, SpawnParams);
	if (!SpawnedActor)
	{
		SetError(OutError, FText::Format(
			NSLOCTEXT("ObjectSpawn", "SpawnFailed", "Failed to spawn actor for ObjectDefinition '{0}'"),
			FText::FromName(Def->ObjectId)));
		return nullptr;
	}

	// Apply optional actor tags from definition for scenario-level wiring.
	for (const FName& ActorTag : Def->ActorTags)
	{
		if (ActorTag.IsNone())
		{
			continue;
		}

		if (!SpawnedActor->Tags.Contains(ActorTag))
		{
			SpawnedActor->Tags.Add(ActorTag);
		}
	}

	// Write definition tracking metadata through host abstraction.
	if (!ProjectWorldDefinitionHost::WriteHostState(
		SpawnedActor,
		DefinitionId,
		Def->DefinitionStructureHash,
		Def->DefinitionContentHash,
		true))
	{
		SetError(OutError, FText::Format(
			NSLOCTEXT("ObjectSpawn", "MissingDefinitionHost",
				"Spawned actor '{0}' has no definition host (interface/component unavailable)"),
			FText::FromString(GetNameSafe(SpawnedActor->GetClass()))));
		SpawnedActor->Destroy();
		return nullptr;
	}

	UE_LOG(LogProjectObject, Log,
		TEXT("ObjectSpawn: Host state set for '%s' (DefinitionId=%s)"),
		*GetNameSafe(SpawnedActor),
		*DefinitionId.ToString());

	// Guardrail: validate host state write by immediate readback.
	// If this fails, placement must fail loudly with:
	// "Failed to persist definition host state ..."
	// This prevents silently placed actors that look valid in viewport but are detached from definition sync.
	{
		FPrimaryAssetId ReadbackDefinitionId;
		FString ReadbackStructureHash;
		FString ReadbackContentHash;
		const bool bReadbackOk = ProjectWorldDefinitionHost::ReadHostState(
			SpawnedActor,
			ReadbackDefinitionId,
			ReadbackStructureHash,
			ReadbackContentHash);

		if (!bReadbackOk || ReadbackDefinitionId != DefinitionId)
		{
			SetError(OutError, FText::Format(
				NSLOCTEXT("ObjectSpawn", "HostReadbackFailed",
					"Failed to persist definition host state for '{0}' (expected '{1}', got '{2}')"),
				FText::FromString(GetNameSafe(SpawnedActor)),
				FText::FromString(DefinitionId.ToString()),
				FText::FromString(ReadbackDefinitionId.ToString())));
			SpawnedActor->Destroy();
			return nullptr;
		}
	}

	USceneComponent* DefaultAttachRoot = ResolveDefinitionAttachRoot(SpawnedActor, Def);

	// 2. Collect mesh IDs that have motion capabilities (only these need Movable)
	TSet<FName> MotionMeshIds;
	for (const FObjectCapabilityEntry& CapEntry : Def->Capabilities)
	{
		if (FCapabilityRegistry::IsMotionCapability(CapEntry.Type))
		{
			for (const FName& ScopeId : CapEntry.Scope)
			{
				if (ScopeId != NAME_CapabilityScope_Actor)
				{
					MotionMeshIds.Add(ScopeId);
				}
			}
		}
	}

	// 3. Create meshes (attach to parent mesh if specified, otherwise to root)
	// UMeshComponent* is the common base of UStaticMeshComponent and USkeletalMeshComponent.
	// Asset field is TSoftObjectPtr<UObject> - runtime cast determines which component to create.
	TMap<FName, UMeshComponent*> MeshMap;
	TArray<UMeshComponent*> ExistingMeshComponents;
	SpawnedActor->GetComponents(ExistingMeshComponents);
	TSet<UMeshComponent*> ClaimedExistingMeshComponents;

	for (const FObjectMeshEntry& Entry : Def->Meshes)
	{
		// Load asset as UObject - could be UStaticMesh or USkeletalMesh
		UObject* LoadedAsset = Entry.Asset.LoadSynchronous();
		if (!LoadedAsset)
		{
			UE_LOG(LogProjectObject, Warning,
				TEXT("Failed to load mesh '%s' for ObjectDefinition '%s'"),
				*Entry.Asset.ToString(),
				*Def->ObjectId.ToString());
			continue;
		}

		// Branch on loaded asset type to create the correct component
		UMeshComponent* MeshComp = FindReusableMeshComponent(
			SpawnedActor,
			Entry,
			LoadedAsset,
			DefaultAttachRoot,
			ExistingMeshComponents,
			ClaimedExistingMeshComponents);
		bool bReusedExistingComponent = (MeshComp != nullptr);

		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(LoadedAsset))
		{
			UStaticMeshComponent* SMC = bReusedExistingComponent
				? Cast<UStaticMeshComponent>(MeshComp)
				: NewObject<UStaticMeshComponent>(SpawnedActor);
			if (!SMC)
			{
				SMC = NewObject<UStaticMeshComponent>(SpawnedActor);
				bReusedExistingComponent = false;
			}
			SMC->SetStaticMesh(StaticMesh);
			MeshComp = SMC;
		}
		else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadedAsset))
		{
			USkeletalMeshComponent* SKC = bReusedExistingComponent
				? Cast<USkeletalMeshComponent>(MeshComp)
				: NewObject<USkeletalMeshComponent>(SpawnedActor);
			if (!SKC)
			{
				SKC = NewObject<USkeletalMeshComponent>(SpawnedActor);
				bReusedExistingComponent = false;
			}
			SKC->SetSkeletalMesh(SkeletalMesh);

			// Optional AnimBlueprint - sets animation mode + anim class
			if (!Entry.AnimClass.IsNull())
			{
				if (UClass* AnimBPClass = Entry.AnimClass.LoadSynchronous())
				{
					SKC->SetAnimationMode(EAnimationMode::AnimationBlueprint);
					SKC->SetAnimInstanceClass(AnimBPClass);
				}
				else
				{
					UE_LOG(LogProjectObject, Warning,
						TEXT("Failed to load AnimClass '%s' for mesh '%s' in '%s'"),
						*Entry.AnimClass.ToString(), *Entry.Id.ToString(), *Def->ObjectId.ToString());
				}
			}

			MeshComp = SKC;
		}
		else
		{
			UE_LOG(LogProjectObject, Error,
				TEXT("Mesh '%s' loaded as unsupported type '%s' (expected StaticMesh or SkeletalMesh) in '%s'"),
				*Entry.Id.ToString(),
				*LoadedAsset->GetClass()->GetName(),
				*Def->ObjectId.ToString());
			continue;
		}

		if (bReusedExistingComponent)
		{
			ClaimedExistingMeshComponents.Add(MeshComp);
		}

		// Material overrides - SetMaterial is on UMeshComponent base, works for both types
		if (Entry.Materials.Num() > 0)
		{
			for (int32 SlotIndex = 0; SlotIndex < Entry.Materials.Num(); ++SlotIndex)
			{
				const TSoftObjectPtr<UMaterialInterface>& MaterialPtr = Entry.Materials[SlotIndex];

				if (MaterialPtr.IsNull())
				{
					continue;
				}

				if (UMaterialInterface* Material = MaterialPtr.LoadSynchronous())
				{
					MeshComp->SetMaterial(SlotIndex, Material);
				}
				else
				{
					UE_LOG(LogProjectObject, Warning,
						TEXT("Failed to load material for mesh '%s' slot %d: %s (ObjectDefinition '%s')"),
						*Entry.Id.ToString(),
						SlotIndex,
						*MaterialPtr.ToString(),
						*Def->ObjectId.ToString());
				}
			}
		}

		// Attach to parent mesh if specified, otherwise attach to resolved default root.
		USceneComponent* AttachParent = DefaultAttachRoot;
		if (!Entry.Parent.IsNone())
		{
			if (UMeshComponent** ParentMesh = MeshMap.Find(Entry.Parent))
			{
				AttachParent = *ParentMesh;
			}
			else
			{
				UE_LOG(LogProjectObject, Warning,
					TEXT("Mesh '%s' specifies unknown parent '%s' in ObjectDefinition '%s' - attaching to root"),
					*Entry.Id.ToString(),
					*Entry.Parent.ToString(),
					*Def->ObjectId.ToString());
			}
		}
		MeshComp->AttachToComponent(AttachParent, FAttachmentTransformRules::KeepRelativeTransform);

		if (!Entry.Transform.IsIdentity())
		{
			MeshComp->SetRelativeTransform(Entry.Transform.ToTransform());
		}

		if (MotionMeshIds.Contains(Entry.Id))
		{
			MeshComp->SetMobility(EComponentMobility::Movable);
		}

		// Physics settings - SetSimulatePhysics/damping/gravity are on UPrimitiveComponent base
		if (Entry.Physics.IsValid())
		{
			MeshComp->SetMobility(EComponentMobility::Movable);
			MeshComp->SetSimulatePhysics(true);

			if (!Entry.Physics.CollisionProfile.IsNone())
			{
				MeshComp->SetCollisionProfileName(Entry.Physics.CollisionProfile);
			}
			else
			{
				MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
			}

			if (Entry.Physics.MassKg > 0.0f)
			{
				MeshComp->BodyInstance.bOverrideMass = true;
				MeshComp->BodyInstance.SetMassOverride(Entry.Physics.MassKg, true);
			}

			MeshComp->SetLinearDamping(Entry.Physics.LinearDamping);
			MeshComp->SetAngularDamping(Entry.Physics.AngularDamping);
			MeshComp->SetEnableGravity(Entry.Physics.EnableGravity);
		}

		const FName DefMeshTag = MakeDefinitionMeshTag(Entry.Id);
		if (!MeshComp->ComponentTags.Contains(DefMeshTag))
		{
			MeshComp->ComponentTags.Add(DefMeshTag);
		}

		if (!bReusedExistingComponent)
		{
			SpawnedActor->AddInstanceComponent(MeshComp);
			MeshComp->RegisterComponent();
		}

		MeshMap.Add(Entry.Id, MeshComp);
	}

	// 4. Create capabilities
	bool bCapabilityPropertyFailure = false;
	FString FirstCapabilityPropertyFailure;

	for (const FObjectCapabilityEntry& CapEntry : Def->Capabilities)
	{
		// Resolve capability type -> component class via registry (CDO scan)
		UClass* CapClass = FCapabilityRegistry::GetCapabilityClass(CapEntry.Type);
		if (!CapClass)
		{
			UE_LOG(LogProjectObject, Error,
				TEXT("Unknown capability type '%s' in ObjectDefinition '%s'"),
				*CapEntry.Type.ToString(),
				*Def->ObjectId.ToString());
			continue; // Skip unknown capability, don't crash
		}

		// Expand scope (always array: ["actor"], ["panel"], ["left", "right"])
		// UMeshComponent* because TargetMesh can be static or skeletal
		TArray<UMeshComponent*> TargetMeshes;
		for (const FName& ScopeId : CapEntry.Scope)
		{
			if (ScopeId == NAME_CapabilityScope_Actor)
			{
				TargetMeshes.Add(nullptr); // nullptr = per-actor, no TargetMesh
			}
			else if (UMeshComponent** Found = MeshMap.Find(ScopeId))
			{
				TargetMeshes.Add(*Found);
			}
			else
			{
				UE_LOG(LogProjectObject, Warning,
					TEXT("Capability scope '%s' not found in meshes for ObjectDefinition '%s'"),
					*ScopeId.ToString(),
					*Def->ObjectId.ToString());
			}
		}

		// Spawn component per target
		for (UMeshComponent* TargetMesh : TargetMeshes)
		{
			UActorComponent* Comp = NewObject<UActorComponent>(SpawnedActor, CapClass);

			// Strict path: mesh-scoped capabilities must use interaction interface target contract.
			if (TargetMesh)
			{
				if (Comp->Implements<UInteractableComponentTargetInterface>())
				{
					IInteractableComponentTargetInterface::Execute_SetInteractTargetMesh(Comp, TargetMesh);
					if (!IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(Comp))
					{
						UE_LOG(LogProjectObject, Warning,
							TEXT("Capability '%s' did not accept mesh scope '%s' for ObjectDefinition '%s'"),
							*CapEntry.Type.ToString(),
							*GetNameSafe(TargetMesh),
							*Def->ObjectId.ToString());
					}
				}
				else
				{
					UE_LOG(LogProjectObject, Warning,
						TEXT("Capability '%s' uses mesh scope '%s' but does not implement interaction target interface (ObjectDefinition '%s')"),
						*CapEntry.Type.ToString(),
						*GetNameSafe(TargetMesh),
						*Def->ObjectId.ToString());
				}
			}

			// Apply properties from JSON (override CDO defaults)
			for (const auto& Prop : CapEntry.Properties)
			{
				if (!SetPropertyByName(Comp, Prop.Key, Prop.Value))
				{
					bCapabilityPropertyFailure = true;
					if (FirstCapabilityPropertyFailure.IsEmpty())
					{
						FirstCapabilityPropertyFailure = FString::Printf(
							TEXT("%s.%s='%s'"),
							*CapEntry.Type.ToString(),
							*Prop.Key.ToString(),
							*Prop.Value);
					}

					UE_LOG(LogProjectObject, Warning,
						TEXT("Failed to set property '%s' = '%s' on capability '%s' (ObjectDefinition '%s')"),
						*Prop.Key.ToString(),
						*Prop.Value,
						*CapEntry.Type.ToString(),
						*Def->ObjectId.ToString());
				}
			}

			// Special handling: LootContainer gets InstanceLoot from sections.loot
			if (CapEntry.Type == FName(TEXT("LootContainer")))
			{
				if (const FLootSection* LootSection = Def->GetLootSection())
				{
					// Set InstanceLoot array via reflection
					FArrayProperty* ArrayProp = CastField<FArrayProperty>(
						Comp->GetClass()->FindPropertyByName(TEXT("InstanceLoot")));
					if (ArrayProp)
					{
						void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Comp);
						FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);
						ArrayHelper.Resize(LootSection->Entries.Num());

						for (int32 i = 0; i < LootSection->Entries.Num(); ++i)
						{
							const FLootEntryView& SrcEntry = LootSection->Entries[i];
							FLootEntryView* DstEntry = reinterpret_cast<FLootEntryView*>(ArrayHelper.GetRawPtr(i));
							*DstEntry = SrcEntry;
						}

						UE_LOG(LogProjectObject, Log,
							TEXT("LootContainer: Set %d entries from sections.loot for '%s'"),
							LootSection->Entries.Num(),
							*Def->ObjectId.ToString());
					}
				}
			}

			SpawnedActor->AddInstanceComponent(Comp);
			Comp->RegisterComponent();
		}
	}

	// Guardrail: do not allow partially-applied capabilities.
	// If any property fails to apply (for example LockTag typo or unsupported property name),
	// fail spawn with:
	// "Capability property apply failed ..."
	// so content issues are fixed at source instead of producing ambiguous runtime behavior.
	if (bCapabilityPropertyFailure)
	{
		SetError(OutError, FText::Format(
			NSLOCTEXT("ObjectSpawn", "CapabilityPropertyApplyFailed",
				"Capability property apply failed for '{0}' (first failure: {1}). Fix definition JSON and regenerate."),
			FText::FromName(Def->ObjectId),
			FText::FromString(FirstCapabilityPropertyFailure)));
		SpawnedActor->Destroy();
		return nullptr;
	}

	// 5. Rebuild interaction component cache for compatibility actor path.
	if (AInteractableActor* InteractableActor = Cast<AInteractableActor>(SpawnedActor))
	{
		InteractableActor->RefreshInteractableComponents();
	}

	return SpawnedActor;
}

AActor* SpawnFromDefinition(
	UWorld* World,
	FPrimaryAssetId ObjectId,
	const FTransform& Transform,
	const FActorSpawnParameters& SpawnParams,
	FText* OutError,
	TSubclassOf<AActor> ActorClass)
{
	if (!ObjectId.IsValid())
	{
		SetError(OutError, NSLOCTEXT("ObjectSpawn", "InvalidId", "ObjectId is invalid"));
		return nullptr;
	}

	UAssetManager& AM = UAssetManager::Get();

	// Fast path: already loaded
	UObjectDefinition* Def = AM.GetPrimaryAssetObject<UObjectDefinition>(ObjectId);

	// Slow path: sync load if not in memory
	if (!Def)
	{
		FSoftObjectPath AssetPath = AM.GetPrimaryAssetPath(ObjectId);
		if (!AssetPath.IsValid())
		{
			SetError(OutError, FText::Format(
				NSLOCTEXT("ObjectSpawn", "DefNotFound", "ObjectDefinition '{0}' not found in AssetManager"),
				FText::FromString(ObjectId.ToString())));
			return nullptr;
		}

		UObject* Loaded = AM.GetStreamableManager().LoadSynchronous(AssetPath, false);
		Def = Cast<UObjectDefinition>(Loaded);
	}

	if (!Def)
	{
		SetError(OutError, FText::Format(
			NSLOCTEXT("ObjectSpawn", "DefLoadFailed", "Failed to load ObjectDefinition '{0}'"),
			FText::FromString(ObjectId.ToString())));
		return nullptr;
	}

	return SpawnFromDefinition(World, Def, Transform, SpawnParams, OutError, ActorClass);
}

} // namespace ProjectObjectSpawn
