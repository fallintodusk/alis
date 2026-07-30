// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Dom/JsonObject.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Interfaces/IItemDataProvider.h"
#include "Types/LootEntryTypes.h"
#include "ObjectDefinition.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimInstance;
class UTexture2D;
class AActor;

// -------------------------------------------------------------------------
// AUTO-PARSING REQUIREMENT
// -------------------------------------------------------------------------
// Field names in these structs MUST match JSON keys (case-insensitive) for
// FJsonObjectConverter::JsonObjectToUStruct auto-parsing to work.
//
// JSON "massKg" -> UPROPERTY MassKg (match)
// JSON "enableGravity" -> UPROPERTY EnableGravity (match)
// JSON "enableGravity" -> UPROPERTY bEnableGravity (NO MATCH - would fail)
//
// Custom callback in DefinitionJsonParser.cpp handles:
// - String-format vectors: "(X=val Y=val Z=val)" -> FVector via InitFromString
// - String-format rotators: "(P=val Y=val R=val)" -> FRotator via InitFromString
// - TSoftObjectPtr path normalization: "/Game/Foo" -> "/Game/Foo.Foo"
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Object Mesh Transform (relative to actor root)
// -------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FObjectMeshTransform
{
	GENERATED_BODY()

	/** Relative location (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FVector Location = FVector::ZeroVector;

	/** Relative rotation (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Scale (1.0 = no scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FVector Scale = FVector::OneVector;

	/** Convert to FTransform. */
	FTransform ToTransform() const
	{
		return FTransform(Rotation, Location, Scale);
	}

	/** Check if transform is identity (no modification needed). */
	bool IsIdentity() const
	{
		return Location.IsNearlyZero() && Rotation.IsNearlyZero() && Scale.Equals(FVector::OneVector);
	}
};

// -------------------------------------------------------------------------
// Object Mesh Physics Config
// -------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FObjectMeshPhysics
{
	GENERATED_BODY()

	/** Mass override in kg. If <= 0, uses mesh's auto-calculated mass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MassKg = 0.0f;

	/** Linear velocity damping (0.01 = default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float LinearDamping = 0.01f;

	/** Angular velocity damping (0.0 = default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float AngularDamping = 0.0f;

	/** Collision profile name. Empty = use default "PhysicsActor". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FName CollisionProfile;

	/** Enable gravity. False for constrained objects (doors, drawers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool EnableGravity = false;

	/** True if this config was explicitly set (vs default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool bIsSet = false;

	/** Check if physics should be enabled based on explicit config. */
	bool IsValid() const { return bIsSet; }
};

// -------------------------------------------------------------------------
// Object Mesh Entry
// -------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FObjectMeshEntry
{
	GENERATED_BODY()

	/** Mesh identifier for capability scope references (e.g., "frame", "panel"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName Id;

	/**
	 * Soft reference to mesh asset (UStaticMesh or USkeletalMesh).
	 * Stored as TSoftObjectPtr<UObject> because UE's idiomatic pattern for
	 * "one field, multiple allowed types" is TSoftObjectPtr<UObject> with
	 * AllowedClasses meta (see SceneThumbnailInfoWithPrimitive.h).
	 * Spawn/Apply code casts the loaded UObject to determine component type.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh",
		meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
	TSoftObjectPtr<UObject> Asset;

	/** Optional parent mesh ID. If set, this mesh attaches to parent instead of actor root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName Parent;

	/** Relative transform (location, rotation, scale). Defaults to identity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FObjectMeshTransform Transform;

	/** Physics simulation settings. If set OR capability has hinge/slider, enables physics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FObjectMeshPhysics Physics;

	/** Material overrides (optional). Array index = material slot index. Empty = use mesh defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	/**
	 * Optional AnimBlueprint class for skeletal meshes.
	 * Ignored for static meshes. Sets USkeletalMeshComponent::AnimClass at spawn.
	 * Uses TSoftClassPtr so the AnimBP is soft-referenced (not loaded until needed).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftClassPtr<UAnimInstance> AnimClass;

	/**
	 * Groom binding asset path. Required when Kind=Groom.
	 * Links GroomAsset to parent SkeletalMesh skeleton.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftObjectPtr<UObject> BindingAsset;

	/**
	 * IK Retargeter lookup tag. Inserted as first ComponentTag on the mesh.
	 * ABP_WorldBodyRetarget reads ComponentTags[0] to resolve which
	 * IKRetargeter asset to use from its IKRetargeter_Map.
	 * Must match a key in that map (e.g. "RTG_GrandPa").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName RetargetTag;

	/**
	 * Optional component type override.
	 * When omitted, auto-detects from asset (StaticMesh or SkeletalMesh).
	 * Required for types that cannot be auto-detected from asset alone.
	 * Valid values: "SkeletalMesh", "StaticMesh", "CustomizableSkeletalMesh", "Groom".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName Kind;

	/**
	 * Optional semantic role for assembly orchestration.
	 * Only valid when SkeletalAssembly capability is present on the definition.
	 * The assembly component reads roles to build the component graph.
	 * Valid values: "DriverBody", "WorldBody", "LocalBody", "Head",
	 * "BodyCustomization", "HeadCustomization", "LocalBodyCustomization".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName Role;

	/**
	 * Optional visibility policy for assembly orchestration.
	 * Only valid when SkeletalAssembly capability is present on the definition.
	 * Valid values:
	 *   "Hidden"    - invisible to all, no shadow (driver mesh)
	 *   "OwnerOnly" - only owner sees, casts shadow (first-person local body)
	 *   "SkipOwner" - hidden from owner, others see, casts hidden shadow (world body)
	 * When omitted, default visibility (visible to all).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName Visibility;
};

// -------------------------------------------------------------------------
// Object Trigger Entry
// -------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EObjectTriggerKind : uint8
{
	Box,
	Sphere,
	Capsule
};

/**
 * Optional post-process config for a trigger volume.
 * UPostProcessComponent is attached to the parent UShapeComponent
 * and automatically inherits its spatial bounds.
 *
 * Component-level fields: Priority, BlendRadius, BlendWeight.
 * Settings: raw JSON object auto-parsed into FPostProcessSettings at spawn time.
 * Material: optional post-process material blendable.
 */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FObjectTriggerPostProcess
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
	float Priority = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
	float BlendRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
	float BlendWeight = 1.0f;

	/** Post process material asset path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Blend weight for the post process material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
	float MaterialWeight = 1.0f;

	/** True if this config was explicitly set in JSON. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess")
	bool bIsSet = false;

	/**
	 * Raw JSON settings object. Parsed into FPostProcessSettings at spawn time
	 * via FJsonObjectConverter. Any FPostProcessSettings UPROPERTY name is valid.
	 * Not serialized to UAsset -- only used during generation pipeline.
	 */
	TSharedPtr<FJsonObject> SettingsJson;

	bool IsValid() const { return bIsSet; }
};

/**
 * Trigger shape component definition.
 * Spawned as UBoxComponent/USphereComponent/UCapsuleComponent.
 * Capabilities reference triggers by Id via TriggerComponentName.
 * Optional PostProcess settings create a UPostProcessComponent
 * attached to the shape (inherits bounds automatically).
 */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FObjectTriggerEntry
{
	GENERATED_BODY()

	/** Trigger identifier. Capabilities reference this via TriggerComponentName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FName Id;

	/** Shape type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	EObjectTriggerKind Kind = EObjectTriggerKind::Box;

	/** Box half-extent (only used when Kind=Box). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FVector Extent = FVector(100.0, 100.0, 100.0);

	/** Radius in cm (used for Sphere and Capsule). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	float Radius = 100.0f;

	/** Half-height in cm (only used for Capsule). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	float HalfHeight = 100.0f;

	/** Relative transform from actor root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FObjectMeshTransform Transform;

	/** Collision profile. Default: OverlapAllDynamic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FName CollisionProfile = FName(TEXT("OverlapAllDynamic"));

	/** Optional post-process volume using this trigger's bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FObjectTriggerPostProcess PostProcess;
};

// -------------------------------------------------------------------------
// Object Capability Entry
// -------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FObjectCapabilityEntry
{
	GENERATED_BODY()

	/** Capability type ID (resolves via CapabilityRegistry CDO scan). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capability")
	FName Type;

	/**
	 * Scope targets for this capability.
	 * - ["actor"] = per-actor, attached to root
	 * - ["panel"] = per-mesh, assigned via interaction target interface
	 * - ["left", "right"] = spawns separate component per mesh (same config)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capability")
	TArray<FName> Scope;

	/**
	 * Property overrides (string values, parsed via FProperty::ImportText).
	 * Key = UPROPERTY name, Value = string representation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capability")
	TMap<FName, FString> Properties;

	/**
	 * True if MotionMode = "Chaos" was found in JSON (explicit opt-in).
	 * Set by parser for legacy compatibility.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capability")
	bool bUsePhysicsMode = false;
};

// -------------------------------------------------------------------------
// Sections (Extensible Data Blocks)
// -------------------------------------------------------------------------
// ObjectDefinition has three lanes:
// 1. Visuals (meshes)
// 2. Interactions (capabilities) - world-visible player interactions
// 3. Sections (data) - optional blocks like Item, Loot, Vehicle, etc.
//
// Sections are stored in TMap<FName, FInstancedStruct> for extensibility.
// New section types can be added without modifying ObjectDefinition.
//
// See: docs/layer_contract.md for architecture
// -------------------------------------------------------------------------

/**
 * Item section - identity + rules + behavior refs for inventory/UI/GAS.
 * Stored in ObjectDefinition.Sections["Item"].
 *
 * Contains:
 * - Identity: DisplayName, Description, Icon, Tags
 * - Rules: Weight, Volume, MaxStack, GridSize, drop/trade flags
 * - Behavior: OnUseEffects, Magnitudes, EquipAbilitySet (soft refs - no GAS compile dep)
 *
 * See: docs/layer_contract.md
 */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FItemSection
{
	GENERATED_BODY()

	// -------------------------------------------------------------------------
	// Identity
	// -------------------------------------------------------------------------

	/** UI display name (inventory, tooltips). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Identity")
	FText DisplayName;

	/** Tooltip/description text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Identity")
	FText Description;

	/** Icon font codepoint for UI (Game Icons font, e.g. "\uF88D"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Identity")
	FString IconCode;

	/** Item categorization tags (Item.Type.*, Item.Survival.*, Item.Key.*). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Identity")
	FGameplayTagContainer Tags;

	// -------------------------------------------------------------------------
	// Rules
	// -------------------------------------------------------------------------

	/** Weight in kg (for encumbrance systems). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Rules")
	float Weight = 0.0f;

	/** Volume in liters (for container capacity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Rules")
	float Volume = 0.0f;

	/** Maximum stack size (1 = non-stackable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Rules")
	int32 MaxStack = 1;

	/** Quantity represented by one depth unit for 1x1 stacks (0 = compatibility fallback to MaxStack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Rules")
	int32 UnitsPerDepthUnit = 0;

	/** Grid inventory size (X=width, Y=height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Rules")
	FIntPoint GridSize = FIntPoint(1, 1);

	/** Whether this item can be dropped from inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Rules")
	bool bCanBeDropped = true;

	/** Whether this item can be traded/sold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Rules")
	bool bCanBeTraded = true;

	/** Quest item flag (cannot be dropped/sold, shows quest marker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Rules")
	bool bIsQuestItem = false;

	// -------------------------------------------------------------------------
	// Behavior - Consumable (soft refs - no GAS compile dependency)
	// -------------------------------------------------------------------------

	/** SetByCaller tag to magnitude mapping for consumable effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Consumable")
	TMap<FGameplayTag, float> Magnitudes;

	/** Whether item is consumed (removed) after use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Consumable")
	bool bConsumeOnUse = true;

	// -------------------------------------------------------------------------
	// Behavior - Equipment (inline specs -> generator emits AbilitySet)
	// -------------------------------------------------------------------------

	/** Ability classes to grant when equipped. JSON SOT, generator creates AbilitySet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment")
	TArray<FSoftClassPath> GrantedAbilities;

	/** Effect assets to apply when equipped. JSON SOT, generator creates AbilitySet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment")
	TArray<FSoftObjectPath> GrantedEffects;

	/** Equipment slot tag (Item.EquipmentSlot.*). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment")
	FGameplayTag EquipSlotTag;

	/** Generated AbilitySet asset. Filled by generator from GrantedAbilities/Effects. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Equipment")
	FSoftObjectPath EquipAbilitySet;

	/** Containers granted while equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment")
	TArray<FInventoryContainerGrantView> ContainerGrants;

	// -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

	/** Returns true if item has consumable behavior (Magnitudes set). */
	bool IsConsumable() const { return Magnitudes.Num() > 0; }

	/** Returns true if item has equipment behavior (slot, grants, or ability/effect). */
	bool IsEquipment() const
	{
		return EquipSlotTag.IsValid()
			|| ContainerGrants.Num() > 0
			|| GrantedAbilities.Num() > 0
			|| GrantedEffects.Num() > 0;
	}
};

/**
 * Storage seed entry - canonical authored contents for world storage.
 * Stored inside ObjectDefinition.Sections["Storage"].SeedEntries.
 */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FStorageSeedEntry
{
	GENERATED_BODY()

	/** Object to place into the container when it initializes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FPrimaryAssetId ObjectId;

	/** Quantity of the object to seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage", meta = (ClampMin = 1))
	int32 Quantity = 1;

	bool IsValid() const { return ObjectId.IsValid() && Quantity > 0; }

	FLootEntryView ToLootEntryView() const
	{
		FLootEntryView Entry;
		Entry.ObjectId = ObjectId;
		Entry.Quantity = FMath::Max(Quantity, 1);
		return Entry;
	}
};

/**
 * Storage section - canonical storage data for world item holders.
 * Stored in ObjectDefinition.Sections["Storage"].
 *
 * Contains:
 * - Container spec: GridSize, MaxWeight, MaxVolume, MaxCells, AllowedTags
 * - Runtime policy: AllowRotation, Persistent, ContainerSlotId
 * - Authored contents: exact SeedEntries + optional shared LootProfileId
 */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FStorageSection
{
	GENERATED_BODY()

	/** Grid size in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FIntPoint GridSize = FIntPoint(0, 0);

	/** Optional max weight in kg (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	float MaxWeight = 0.0f;

	/** Optional max volume in liters (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	float MaxVolume = 0.0f;

	/** Optional hard cap on total occupied cells (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	int32 MaxCells = 0;

	/** Max stack height per cell for 1x1 depth stacking (default 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	int32 CellDepthUnits = 1;

	/** Optional filter for items allowed in this container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FGameplayTagContainer AllowedTags;

	/** Whether item rotation is allowed inside this container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	bool AllowRotation = true;

	/** Whether this world storage should persist across sessions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	bool Persistent = false;

	/** Stable logical slot name for multi-container actors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FName ContainerSlotId = FName(TEXT("Primary"));

	/** Canonical authored contents for initial fill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	TArray<FStorageSeedEntry> SeedEntries;

	/** Optional shared loot profile for randomized initial fill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FPrimaryAssetId LootProfileId;

	bool HasSeedEntries() const { return SeedEntries.Num() > 0; }
	bool HasLootProfile() const { return LootProfileId.IsValid(); }
	bool HasInitialContents() const { return HasSeedEntries() || HasLootProfile(); }

	void BuildSeedLootEntries(TArray<FLootEntryView>& OutEntries) const;
};

// -------------------------------------------------------------------------
// Skeletal Assembly Sections
// -------------------------------------------------------------------------

/** Animation config for skeletal assembly definitions. */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FAnimationSection
{
	GENERATED_BODY()

	/** Locomotion profile reference (e.g. "Human.Default"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FName LocomotionProfile;

	/** Optional traversal profile (e.g. "Human.Parkour"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FName TraversalProfile;
};

/** Customization config for skeletal assembly definitions. */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FCustomizationSection
{
	GENERATED_BODY()

	/** Mutable CustomizableObject asset path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	TSoftObjectPtr<UObject> MutableSource;
};

/** View/camera config for skeletal assembly definitions. */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FViewSection
{
	GENERATED_BODY()

	/** Default camera mode: "FirstPerson" or "ThirdPerson". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
	FName DefaultMode;

	/** Camera parent component or attach point (e.g. "Root", "Head"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
	FName CameraParent;

	/** Camera attachment policy (e.g. "CapsuleFixed", "BoneFollow"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
	FName AttachmentPolicy;

	/** Camera relative offset from attach point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
	FVector RelativeOffset = FVector::ZeroVector;
};

/** Section ID constants for type-safe access. */
namespace ObjectSectionIds
{
	inline const FName Item = TEXT("Item");
	inline const FName Storage = TEXT("Storage");
	inline const FName Animation = TEXT("Animation");
	inline const FName Customization = TEXT("Customization");
	inline const FName View = TEXT("View");
}

// -------------------------------------------------------------------------
// Object Definition (UPrimaryDataAsset)
// -------------------------------------------------------------------------

/**
 * Static object definition - JSON-sourced blueprint for world objects.
 * Uses explicit ObjectId for FPrimaryAssetId (decoupled from asset path).
 *
 * Consumed by UProjectObjectActorFactory to spawn fully-configured actors.
 *
 * Architecture: [flexible_path.md] - Pattern A (DataAssets)
 */
UCLASS(BlueprintType)
class PROJECTOBJECT_API UObjectDefinition : public UPrimaryDataAsset, public IItemDataProvider
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------------
	// Identity (explicit, decoupled from asset path)
	// -------------------------------------------------------------------------

	/**
	 * Explicit object identifier used by GetPrimaryAssetId().
	 * Set from JSON "id" field during generation.
	 * Decoupled from asset path - renames/moves don't break saves.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Object")
	FName ObjectId;

	// -------------------------------------------------------------------------
	// Spawn
	// -------------------------------------------------------------------------

	/**
	 * Optional actor class to spawn for this definition.
	 * If empty, spawn utility falls back to legacy default (AInteractableActor).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object|Spawn")
	TSoftClassPtr<AActor> SpawnClass;

	/**
	 * Optional component tag used as attachment root for generated meshes.
	 * If empty, spawn uses component tag "ObjectAttachRoot" when present, otherwise actor root.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object|Spawn")
	FName AttachToComponentTag;

	/**
	 * Optional actor tags applied to spawned actor instance.
	 * Used for scenario wiring (for example ActorWatcher tag lookup).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object|Spawn")
	TArray<FName> ActorTags;

	/**
	 * Uniform actor scale applied to root component.
	 * Scales entire actor (capsule, meshes, grooms). Default 1.0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object|Spawn")
	float ActorScale = 1.0f;

	// -------------------------------------------------------------------------
	// Visual
	// -------------------------------------------------------------------------

	/** Meshes that compose this object. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object|Visual")
	TArray<FObjectMeshEntry> Meshes;

	// -------------------------------------------------------------------------
	// Triggers (Shape Components)
	// -------------------------------------------------------------------------

	/** Trigger shape components (Box, Sphere, Capsule). Capabilities reference by Id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object|Triggers")
	TArray<FObjectTriggerEntry> Triggers;

	// -------------------------------------------------------------------------
	// Capabilities
	// -------------------------------------------------------------------------

	/** Capabilities (components) attached to this object. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object|Capabilities")
	TArray<FObjectCapabilityEntry> Capabilities;

	// -------------------------------------------------------------------------
	// Sections (Extensible Data Blocks)
	// -------------------------------------------------------------------------

	/**
	 * Optional data sections (Item, Storage, Vehicle, etc.).
	 * Key = section ID (e.g., "Item"), Value = section struct via FInstancedStruct.
	 *
	 * Use GetSection<T>() for type-safe access.
	 * See: docs/layer_contract.md
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Object|Sections")
	TMap<FName, FInstancedStruct> Sections;

	/** Check if a section exists. */
	bool HasSection(FName SectionId) const { return Sections.Contains(SectionId); }

	/** Get section by type (returns nullptr if not found or wrong type). */
	template<typename T>
	const T* GetSection(FName SectionId) const
	{
		if (const FInstancedStruct* Section = Sections.Find(SectionId))
		{
			return Section->GetPtr<T>();
		}
		return nullptr;
	}

	/** Get mutable section by type (for generator to modify). */
	template<typename T>
	T* GetMutableSection(FName SectionId)
	{
		if (FInstancedStruct* Section = Sections.Find(SectionId))
		{
			return Section->GetMutablePtr<T>();
		}
		return nullptr;
	}

	/** Convenience: check if has Item section. */
	bool HasItemSection() const { return HasSection(ObjectSectionIds::Item); }

	/** Convenience: get Item section. Returns nullptr if not present. */
	const FItemSection* GetItemSection() const { return GetSection<FItemSection>(ObjectSectionIds::Item); }

	/** Convenience: get mutable Item section (for generator to set EquipAbilitySet). */
	FItemSection* GetMutableItemSection() { return GetMutableSection<FItemSection>(ObjectSectionIds::Item); }

	/** Convenience: check if has Storage section. */
	bool HasStorageSection() const { return HasSection(ObjectSectionIds::Storage); }

	/** Convenience: get Storage section. Returns nullptr if not present. */
	const FStorageSection* GetStorageSection() const { return GetSection<FStorageSection>(ObjectSectionIds::Storage); }

	/** Convenience: get mutable Storage section (for generator/parser). */
	FStorageSection* GetMutableStorageSection() { return GetMutableSection<FStorageSection>(ObjectSectionIds::Storage); }

	// -------------------------------------------------------------------------
	// IItemDataProvider
	// -------------------------------------------------------------------------

	virtual FItemDataView GetItemDataView_Implementation() const override;

	// -------------------------------------------------------------------------
	// Generation Tracking (set by generator, read-only in editor)
	// AssetRegistrySearchable allows reading via FAssetData without loading
	// -------------------------------------------------------------------------

	/** True if this asset was generated from JSON (not hand-created). */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	bool bGenerated = false;

	/** Generator version that created this asset. */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	int32 GeneratorVersion = 0;

	/** Source JSON file path (relative to plugin). */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString SourceJsonPath;

	/** Hash of source JSON for incremental regeneration. */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString SourceJsonHash;

	/**
	 * Hash of structural elements (mesh IDs + capability types, sorted for determinism).
	 * Changes only when components are added/removed (reordering is ignored).
	 * Used to determine if actor Replace is needed.
	 * Computed by ProjectDefinitionGenerator from source JSON.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString DefinitionStructureHash;

	/**
	 * Hash of full definition content (source JSON).
	 * Changes when ANY property changes.
	 * Used to detect if actor Reapply is needed.
	 * Computed by ProjectDefinitionGenerator from source JSON.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString DefinitionContentHash;

	// -------------------------------------------------------------------------
	// UPrimaryDataAsset Interface
	// -------------------------------------------------------------------------

	/**
	 * Returns FPrimaryAssetId using explicit ObjectId (not asset name).
	 * Format: "ObjectDefinition:WoodenDoor"
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/**
	 * Export capability tags for AssetRegistry filtering (Phase 0).
	 * Allows UI to filter objects by capability without loading assets.
	 */
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;

#if WITH_EDITORONLY_DATA
	/**
	 * Register soft references (meshes, materials, anim classes, etc.) with
	 * the Asset Registry so they appear in Reference Viewer and are included
	 * in cook dependency graphs.
	 * Called automatically by UPrimaryDataAsset::PreSave().
	 */
	virtual void UpdateAssetBundleData() override;
#endif
};
