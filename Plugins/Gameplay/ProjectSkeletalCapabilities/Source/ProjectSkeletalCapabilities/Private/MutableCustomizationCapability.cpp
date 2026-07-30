// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MutableCustomizationCapability.h"
#include "ProjectSkeletalCapabilitiesModule.h"

#include "Interfaces/IAssemblyCapability.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

UMutableCustomizationCapability::UMutableCustomizationCapability()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = false;
}

FPrimaryAssetId UMutableCustomizationCapability::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(
		FPrimaryAssetType(TEXT("CapabilityComponent")),
		FName(TEXT("MutableCustomization")));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UMutableCustomizationCapability::BeginPlay()
{
	Super::BeginPlay();

	ParseComponentNameMapping();
	ParseDefaultParameters();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

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
			InitializeMutable();
		}
		else
		{
			AssemblyStateHandle = AssemblyProvider->AddAssemblyStateChanged(
				FOnAssemblyStateChangedNative::FDelegate::CreateUObject(
					this, &UMutableCustomizationCapability::OnAssemblyStateChanged));
		}
	}
	else
	{
		InitializeMutable();
	}
}

void UMutableCustomizationCapability::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (CachedAssemblyComponent.IsValid())
	{
		if (IAssemblyCapability* Asm = Cast<IAssemblyCapability>(CachedAssemblyComponent.Get()))
		{
			Asm->RemoveAssemblyStateChanged(AssemblyStateHandle);
		}
	}
	AssemblyStateHandle.Reset();

	TeardownMutable();
	Super::EndPlay(EndPlayReason);
}

void UMutableCustomizationCapability::OnAssemblyStateChanged(EAssemblyState NewState)
{
	if (NewState == EAssemblyState::Ready && !bMutableInitialized)
	{
		InitializeMutable();
	}
	else if (NewState == EAssemblyState::TearingDown && bMutableInitialized)
	{
		TeardownMutable();
	}
}

// ---------------------------------------------------------------------------
// ComponentName mapping
// ---------------------------------------------------------------------------

void UMutableCustomizationCapability::ParseComponentNameMapping()
{
	RoleToComponentName.Reset();

	if (ComponentNameMapping.IsEmpty())
	{
		return;
	}

	// Format: "BodyCustomization=Body,HeadCustomization=Head,LocalBodyCustomization=Body"
	TArray<FString> Pairs;
	ComponentNameMapping.ParseIntoArray(Pairs, TEXT(","));

	for (const FString& Pair : Pairs)
	{
		FString RoleStr, CompNameStr;
		if (Pair.Split(TEXT("="), &RoleStr, &CompNameStr))
		{
			RoleToComponentName.Add(
				FName(*RoleStr.TrimStartAndEnd()),
				FName(*CompNameStr.TrimStartAndEnd()));
		}
	}

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[MutableCustomization] Parsed %d explicit ComponentName mappings"),
		RoleToComponentName.Num());
}

void UMutableCustomizationCapability::ParseDefaultParameters()
{
	ParsedDefaultParameters.Reset();

	if (DefaultParameters.IsEmpty())
	{
		return;
	}

	TArray<FString> Pairs;
	DefaultParameters.ParseIntoArray(Pairs, TEXT(","));

	for (const FString& Pair : Pairs)
	{
		FString ParamName, OptionName;
		if (Pair.Split(TEXT("="), &ParamName, &OptionName))
		{
			ParsedDefaultParameters.Emplace(
				ParamName.TrimStartAndEnd(), OptionName.TrimStartAndEnd());
		}
	}

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[MutableCustomization] Parsed %d default parameters"),
		ParsedDefaultParameters.Num());
}

void UMutableCustomizationCapability::ApplyDefaultParameters()
{
	if (!CachedInstance || ParsedDefaultParameters.IsEmpty())
	{
		return;
	}

	for (const auto& Param : ParsedDefaultParameters)
	{
		CachedInstance->SetEnumParameterSelectedOption(Param.Key, Param.Value);

		UE_LOG(LogProjectSkeletalCapabilities, Log,
			TEXT("[MutableCustomization] Applied default parameter: %s=%s"),
			*Param.Key, *Param.Value);
	}
}

FName UMutableCustomizationCapability::ResolveComponentName(
	FName Role, const TArray<FName>& COComponentNames, int32 TargetIndex) const
{
	// Explicit mapping takes priority
	if (const FName* Mapped = RoleToComponentName.Find(Role))
	{
		return *Mapped;
	}

	// Fallback: ordinal mapping
	if (COComponentNames.IsValidIndex(TargetIndex))
	{
		UE_LOG(LogProjectSkeletalCapabilities, Warning,
			TEXT("[MutableCustomization] No explicit mapping for role '%s', using ordinal [%d]='%s'"),
			*Role.ToString(), TargetIndex, *COComponentNames[TargetIndex].ToString());
		return COComponentNames[TargetIndex];
	}

	UE_LOG(LogProjectSkeletalCapabilities, Error,
		TEXT("[MutableCustomization] No mapping for role '%s' and ordinal %d out of range (%d components)"),
		*Role.ToString(), TargetIndex, COComponentNames.Num());
	return NAME_None;
}

// ---------------------------------------------------------------------------
// Target discovery
// ---------------------------------------------------------------------------

namespace
{
	const FString MutableRoleTagPrefix(TEXT("AssemblyRole="));

	USkeletalMeshComponent* FindMeshByRole(AActor* Owner, const TCHAR* RoleName)
	{
		if (!Owner)
		{
			return nullptr;
		}

		const FName RoleTag(*FString::Printf(TEXT("AssemblyRole=%s"), RoleName));
		TArray<USkeletalMeshComponent*> SkeletalComps;
		Owner->GetComponents<USkeletalMeshComponent>(SkeletalComps);

		for (USkeletalMeshComponent* SKC : SkeletalComps)
		{
			if (SKC->ComponentTags.Contains(RoleTag))
			{
				return SKC;
			}
		}

		return nullptr;
	}

	bool EnsureBlueprintAnimReady(USkeletalMeshComponent* Mesh, const bool bMeshChanged)
	{
		if (!Mesh)
		{
			return false;
		}

		const bool bNeedsBlueprintMode = Mesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint;
		if (bNeedsBlueprintMode)
		{
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		}

		const bool bNeedsInit = bMeshChanged || bNeedsBlueprintMode || !Mesh->GetAnimInstance();
		if (bNeedsInit && Mesh->GetAnimClass())
		{
			Mesh->InitAnim(true);
		}

		return bNeedsInit;
	}
}

void UMutableCustomizationCapability::DiscoverCustomizationTargets()
{
	CustomizationTargets.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<USkeletalMeshComponent*> SkeletalComps;
	Owner->GetComponents<USkeletalMeshComponent>(SkeletalComps);

	for (USkeletalMeshComponent* SKC : SkeletalComps)
	{
		for (const FName& Tag : SKC->ComponentTags)
		{
			const FString TagStr = Tag.ToString();
			if (!TagStr.StartsWith(MutableRoleTagPrefix))
			{
				continue;
			}

			const FString RoleName = TagStr.Mid(MutableRoleTagPrefix.Len());
			if (RoleName.Contains(TEXT("Customization")))
			{
				CustomizationTargets.Emplace(SKC, FName(*RoleName));
			}
		}
	}

	// Sort by role name for deterministic ordering (used as fallback when no explicit mapping)
	CustomizationTargets.Sort([](const auto& A, const auto& B)
	{
		return A.Value.LexicalLess(B.Value);
	});

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[MutableCustomization] Discovered %d customization targets on '%s'"),
		CustomizationTargets.Num(),
		*GetNameSafe(Owner));
}

// ---------------------------------------------------------------------------
// Mutable initialization
// ---------------------------------------------------------------------------

void UMutableCustomizationCapability::InitializeMutable()
{
	if (bMutableInitialized)
	{
		return;
	}

	DiscoverCustomizationTargets();

	if (CustomizationTargets.IsEmpty())
	{
		UE_LOG(LogProjectSkeletalCapabilities, Warning,
			TEXT("[MutableCustomization] No customization targets found on '%s'"),
			*GetNameSafe(GetOwner()));
		return;
	}

	// Prefer pre-saved COI (preset): clone carries saved parameter selections.
	// Fall back to CO->CreateInstance() for fresh runtime customization.
	bool bUsedPreset = false;
	if (!MutableInstance.IsNull())
	{
		UCustomizableObjectInstance* SourceCOI = MutableInstance.LoadSynchronous();
		if (SourceCOI)
		{
			CachedInstance = SourceCOI->Clone();
			if (CachedInstance)
			{
				bUsedPreset = true;
				UE_LOG(LogProjectSkeletalCapabilities, Log,
					TEXT("[MutableCustomization] Cloned preset COI '%s' on '%s'"),
					*GetNameSafe(SourceCOI),
					*GetNameSafe(GetOwner()));
			}
			else
			{
				UE_LOG(LogProjectSkeletalCapabilities, Warning,
					TEXT("[MutableCustomization] Failed to clone COI '%s', falling back to MutableSource"),
					*GetNameSafe(SourceCOI));
			}
		}
		else
		{
			UE_LOG(LogProjectSkeletalCapabilities, Warning,
				TEXT("[MutableCustomization] Failed to load MutableInstance '%s', falling back to MutableSource"),
				*MutableInstance.ToString());
		}
	}

	if (!CachedInstance)
	{
		if (MutableSource.IsNull())
		{
			UE_LOG(LogProjectSkeletalCapabilities, Warning,
				TEXT("[MutableCustomization] Neither MutableInstance nor MutableSource set on '%s'"),
				*GetNameSafe(GetOwner()));
			return;
		}

		UCustomizableObject* CO = MutableSource.LoadSynchronous();
		if (!CO)
		{
			UE_LOG(LogProjectSkeletalCapabilities, Error,
				TEXT("[MutableCustomization] Failed to load CO '%s' for '%s'"),
				*MutableSource.ToString(),
				*GetNameSafe(GetOwner()));
			return;
		}

		CachedInstance = CO->CreateInstance();
		if (!CachedInstance)
		{
			UE_LOG(LogProjectSkeletalCapabilities, Error,
				TEXT("[MutableCustomization] Failed to create COI from '%s'"),
				*GetNameSafe(CO));
			return;
		}

		UE_LOG(LogProjectSkeletalCapabilities, Log,
			TEXT("[MutableCustomization] Created fresh COI from '%s' on '%s' (compiled=%s)"),
			*GetNameSafe(CO),
			*GetNameSafe(GetOwner()),
			CO->IsCompiled() ? TEXT("true") : TEXT("false"));
	}

	const TArray<FName> COComponentNames = CachedInstance->GetComponentNames();

	// Create one CSK per target, using explicit ComponentName mapping
	for (int32 i = 0; i < CustomizationTargets.Num(); ++i)
	{
		USkeletalMeshComponent* TargetMesh = CustomizationTargets[i].Key.Get();
		const FName Role = CustomizationTargets[i].Value;

		if (!TargetMesh)
		{
			UE_LOG(LogProjectSkeletalCapabilities, Warning,
				TEXT("[MutableCustomization] Target mesh for role '%s' is stale"),
				*Role.ToString());
			continue;
		}

		const FName CompName = ResolveComponentName(Role, COComponentNames, i);
		if (CompName.IsNone())
		{
			continue;
		}

		UCustomizableSkeletalComponent* CSK = NewObject<UCustomizableSkeletalComponent>(GetOwner());
		CSK->SetComponentName(CompName);
		CSK->SetCustomizableObjectInstance(CachedInstance);
		CSK->AttachToComponent(TargetMesh, FAttachmentTransformRules::KeepRelativeTransform);
		CSK->RegisterComponent();

		CreatedCSKs.Add(CSK);

		UE_LOG(LogProjectSkeletalCapabilities, Log,
			TEXT("[MutableCustomization] CSK '%s' -> mesh '%s' (role: %s) parent=%s parentMesh=%s"),
			*CompName.ToString(),
			*GetNameSafe(TargetMesh),
			*Role.ToString(),
			*GetNameSafe(CSK->GetAttachParent()),
			TargetMesh->GetSkeletalMeshAsset() ? *TargetMesh->GetSkeletalMeshAsset()->GetName() : TEXT("null"));
	}

	// Apply DefaultParameters as overrides on top of whatever the instance has.
	// For preset COI clones: overrides tweak the saved baseline.
	// For fresh CO instances: CO may not be compiled yet at this point.
	//   Mutable auto-compiles asynchronously. If params fail now, we retry in
	//   OnMutableInstanceUpdated after the first successful async update.
	if (!ParsedDefaultParameters.IsEmpty())
	{
		ApplyDefaultParameters();
		bNeedsDeferredParamApply = !bUsedPreset && (CachedInstance->GetCustomizableObject() && !CachedInstance->GetCustomizableObject()->IsCompiled());
	}

	CachedInstance->UpdatedNativeDelegate.AddUObject(
		this, &UMutableCustomizationCapability::OnMutableInstanceUpdated);

	CachedInstance->UpdateSkeletalMeshAsync(false, false);

	bMutableInitialized = true;

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[MutableCustomization] Initialized %d CSKs on '%s' (source: %s, deferredParams: %s)"),
		CreatedCSKs.Num(),
		*GetNameSafe(GetOwner()),
		bUsedPreset ? TEXT("preset COI") : TEXT("fresh CO"),
		bNeedsDeferredParamApply ? TEXT("yes") : TEXT("no"));
}

// ---------------------------------------------------------------------------
// Teardown
// ---------------------------------------------------------------------------

void UMutableCustomizationCapability::TeardownMutable()
{
	if (!bMutableInitialized)
	{
		return;
	}

	if (CachedInstance)
	{
		CachedInstance->UpdatedNativeDelegate.RemoveAll(this);
	}

	for (UCustomizableSkeletalComponent* CSK : CreatedCSKs)
	{
		if (CSK)
		{
			CSK->DestroyComponent();
		}
	}
	CreatedCSKs.Reset();

	CachedInstance = nullptr;
	CustomizationTargets.Reset();
	bMutableInitialized = false;
}

// ---------------------------------------------------------------------------
// Rebuild callback
// ---------------------------------------------------------------------------

void UMutableCustomizationCapability::OnMutableInstanceUpdated(
	UCustomizableObjectInstance* Instance)
{
	// Deferred parameter application: CO is now compiled after the first async update.
	// Re-apply overrides and trigger a second update to generate the correct mesh.
	if (bNeedsDeferredParamApply)
	{
		bNeedsDeferredParamApply = false;
		UE_LOG(LogProjectSkeletalCapabilities, Log,
			TEXT("[MutableCustomization] CO now compiled on '%s', applying deferred parameters"),
			*GetNameSafe(GetOwner()));
		ApplyDefaultParameters();
		CachedInstance->UpdateSkeletalMeshAsync(false, false);
		return;
	}

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[MutableCustomization] COI updated on '%s' - %d CSKs active"),
		*GetNameSafe(GetOwner()),
		CreatedCSKs.Num());

	// Log parent mesh state after Mutable rebuild
	for (int32 i = 0; i < CreatedCSKs.Num() && i < CustomizationTargets.Num(); ++i)
	{
		USkeletalMeshComponent* Parent = CustomizationTargets[i].Key.Get();
		if (Parent)
		{
			UE_LOG(LogProjectSkeletalCapabilities, Log,
				TEXT("[MutableCustomization] Post-rebuild: '%s' parentMesh=%s"),
				*CustomizationTargets[i].Value.ToString(),
				Parent->GetSkeletalMeshAsset() ? *Parent->GetSkeletalMeshAsset()->GetName() : TEXT("null"));
		}
	}

	AActor* Owner = GetOwner();
	USkeletalMeshComponent* WorldBody = FindMeshByRole(Owner, TEXT("WorldBody"));
	USkeletalMeshComponent* BodyCustomization = FindMeshByRole(Owner, TEXT("BodyCustomization"));
	USkeletalMeshComponent* DriverBody = FindMeshByRole(Owner, TEXT("DriverBody"));
	USkeletalMeshComponent* HeadCustomization = FindMeshByRole(Owner, TEXT("HeadCustomization"));
	USkeletalMeshComponent* LocalBodyCustomization = FindMeshByRole(Owner, TEXT("LocalBodyCustomization"));
	if (!WorldBody || !BodyCustomization || !BodyCustomization->GetSkeletalMeshAsset())
	{
		return;
	}

	bool bWorldBodyMeshChanged = false;
	if (WorldBody->GetSkeletalMeshAsset() != BodyCustomization->GetSkeletalMeshAsset())
	{
		WorldBody->SetSkeletalMeshAsset(BodyCustomization->GetSkeletalMeshAsset());
		bWorldBodyMeshChanged = true;
	}

	const bool bWorldBodyAnimReinitialized = EnsureBlueprintAnimReady(WorldBody, bWorldBodyMeshChanged);
	if (!WorldBody->GetAnimClass())
	{
		UE_LOG(LogProjectSkeletalCapabilities, Warning,
			TEXT("[MutableCustomization] WorldBody has no AnimClass after Mutable rebuild on '%s'. ")
			TEXT("WorldBody animClass must come from object data or legacy Blueprint defaults."),
			*GetNameSafe(Owner));
	}

	if (DriverBody)
	{
		WorldBody->AddTickPrerequisiteComponent(DriverBody);
	}

	WorldBody->SetHiddenInGame(false);
	WorldBody->SetOwnerNoSee(true);
	WorldBody->SetCastHiddenShadow(true);

	BodyCustomization->SetHiddenInGame(true);
	BodyCustomization->SetCastShadow(false);
	if (DriverBody)
	{
		BodyCustomization->SetLeaderPoseComponent(DriverBody);
	}

	if (HeadCustomization && WorldBody->GetSkeletalMeshAsset())
	{
		HeadCustomization->SetLeaderPoseComponent(WorldBody);
	}

	if (LocalBodyCustomization && WorldBody->GetSkeletalMeshAsset())
	{
		LocalBodyCustomization->AddTickPrerequisiteComponent(WorldBody);
	}

	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[MutableCustomization] Promoted WorldBody visual source on '%s': WorldBodyMesh=%s AnimClass=%s AnimInstance=%s Reinit=%s BodyCustomizationHidden=%s"),
		*GetNameSafe(Owner),
		WorldBody->GetSkeletalMeshAsset() ? *WorldBody->GetSkeletalMeshAsset()->GetName() : TEXT("null"),
		WorldBody->GetAnimClass() ? *WorldBody->GetAnimClass()->GetName() : TEXT("null"),
		WorldBody->GetAnimInstance() ? *WorldBody->GetAnimInstance()->GetClass()->GetName() : TEXT("null"),
		bWorldBodyAnimReinitialized ? TEXT("true") : TEXT("false"),
		BodyCustomization->bHiddenInGame ? TEXT("true") : TEXT("false"));
}
