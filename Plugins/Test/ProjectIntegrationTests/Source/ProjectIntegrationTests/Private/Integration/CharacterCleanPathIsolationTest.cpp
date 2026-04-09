// Copyright ALIS. All Rights Reserved.
// Clean-path isolation matrix for the modular first-person body behavior.
// Reuses the camera-yaw diagnostics, but runs the same spawned hero through:
// A Driver only -> B Driver + WorldBody -> C Driver + WorldBody + LocalBody -> D full chain.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

struct FCleanPathMode
{
	FString Name;
	FString VisibleRole;
	bool bExpectWorld = false;
	bool bExpectVisibleTracksWorld = false;
	bool bFullChain = false;
};

struct FCleanPathPhase
{
	FString Name;
	float DurationSec;
	FVector2D MoveInput;
	bool bCrouch;
	bool bJump;
	float StartYawOffsetDeg;
	float EndYawOffsetDeg;
	bool bExpectVisibleResponse;
	bool bExpectLargeBodyTurn;
	bool bExpectTurnState;
};

static const FCleanPathMode GCleanPathModes[] = {
	{ TEXT("ModeA_DriverOnly"), TEXT("DriverBody"), false, false, false },
	{ TEXT("ModeB_DriverWorld"), TEXT("WorldBody"), true, false, false },
	{ TEXT("ModeC_DriverWorldLocal"), TEXT("LocalBody"), true, true, false },
	{ TEXT("ModeD_FullChain"), TEXT("OwnerVisible"), true, true, true },
};
static constexpr int32 GNumCleanPathModes = UE_ARRAY_COUNT(GCleanPathModes);

static const FCleanPathPhase GCleanPathPhases[] = {
	{ TEXT("Idle"),            1.0f, { 0,  0}, false, false,   0.f,   0.f, false, false, false },
	{ TEXT("ForwardAccel"),    1.0f, { 1,  0}, false, false,   0.f,   0.f, true,  false, false },
	{ TEXT("ForwardSteady"),   1.5f, { 1,  0}, false, false,   0.f,   0.f, true,  false, false },
	{ TEXT("Stop"),            1.0f, { 0,  0}, false, false,   0.f,   0.f, false, false, false },
	{ TEXT("Backward"),        1.5f, {-1,  0}, false, false,   0.f,   0.f, true,  false, false },
	{ TEXT("StrafeLeft"),      1.5f, { 0, -1}, false, false,   0.f,   0.f, true,  false, false },
	{ TEXT("StrafeRight"),     1.5f, { 0,  1}, false, false,   0.f,   0.f, true,  false, false },
	{ TEXT("DiagonalLeft"),    1.5f, { 1, -1}, false, false,   0.f,   0.f, true,  false, false },
	{ TEXT("DiagonalRight"),   1.5f, { 1,  1}, false, false,   0.f,   0.f, true,  false, false },
	{ TEXT("CrouchIdle"),      1.0f, { 0,  0}, true,  false,   0.f,   0.f, false, false, false },
	{ TEXT("CrouchForward"),   1.5f, { 1,  0}, true,  false,   0.f,   0.f, true,  false, false },
	{ TEXT("IdleYaw30"),       1.0f, { 0,  0}, false, false,   0.f,  30.f, true,  false, false },
	{ TEXT("IdleYaw60"),       1.0f, { 0,  0}, false, false,  30.f,  60.f, true,  true,  false },
	{ TEXT("IdleYaw100"),      1.0f, { 0,  0}, false, false,  60.f, 100.f, true,  true,  true  },
	{ TEXT("YawBack"),         1.0f, { 0,  0}, false, false, 100.f,   0.f, true,  true,  true  },
	{ TEXT("MoveYaw90"),       1.5f, { 1,  0}, false, false,   0.f,  90.f, true,  true,  false },
	{ TEXT("JumpFallLand"),    2.0f, { 0,  0}, false, true,    0.f,   0.f, true,  false, false },
};
static constexpr int32 GNumCleanPathPhases = UE_ARRAY_COUNT(GCleanPathPhases);

namespace CleanPathHelpers
{

FProperty* FindProp(const UStruct* Owner, const TCHAR* Name)
{
	if (FProperty* P = Owner->FindPropertyByName(FName(Name)))
	{
		return P;
	}

	const FString Prefix = FString(Name) + TEXT("_");
	for (TFieldIterator<FProperty> It(Owner); It; ++It)
	{
		if (It->GetName().StartsWith(Prefix))
		{
			return *It;
		}
	}

	return nullptr;
}

FProperty* FindSubProp(const UScriptStruct* Owner, const TCHAR* Name)
{
	return Owner ? FindProp(Owner, Name) : nullptr;
}

float NormalizeYawDelta(float A, float B)
{
	return FRotator::NormalizeAxis(A - B);
}

bool IsTurnClipName(const FString& Name)
{
	return Name.Contains(TEXT("_Turn_"));
}

FString RotToString(const FRotator& R)
{
	return FString::Printf(TEXT("%.2f,%.2f,%.2f"), R.Pitch, R.Yaw, R.Roll);
}

FString TransformToCompactString(const FTransform& T)
{
	const FVector L = T.GetLocation();
	const FRotator R = T.GetRotation().Rotator();
	return FString::Printf(TEXT("L=(%.2f,%.2f,%.2f) R=(%.2f,%.2f,%.2f)"),
		L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll);
}

float PlanarHeadingFromTransform(const FTransform& T, const EAxis::Type Axis = EAxis::X)
{
	FVector Heading = T.GetUnitAxis(Axis);
	Heading.Z = 0.f;

	if (!Heading.Normalize())
	{
		return 0.f;
	}

	return FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, Heading.X));
}

bool ReadByteValue(FProperty* Prop, void* Container, uint8& OutValue)
{
	if (!Prop || !Container)
	{
		return false;
	}

	if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		OutValue = ByteProp->GetPropertyValue(Prop->ContainerPtrToValuePtr<void>(Container));
		return true;
	}

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		OutValue = static_cast<uint8>(EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(
			EnumProp->ContainerPtrToValuePtr<void>(Container)));
		return true;
	}

	return false;
}

bool ReadFloatValue(FProperty* Prop, void* Container, float& OutValue)
{
	if (!Prop || !Container)
	{
		return false;
	}

	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		OutValue = FloatProp->GetPropertyValue(Prop->ContainerPtrToValuePtr<void>(Container));
		return true;
	}

	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		OutValue = static_cast<float>(DoubleProp->GetPropertyValue(Prop->ContainerPtrToValuePtr<void>(Container)));
		return true;
	}

	return false;
}

bool ReadBoolValue(FProperty* Prop, void* Container, bool& OutValue)
{
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		OutValue = BoolProp->GetPropertyValue(Prop->ContainerPtrToValuePtr<void>(Container));
		return true;
	}

	return false;
}

bool ReadRotatorValue(FProperty* Prop, void* Container, FRotator& OutValue)
{
	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		if (StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			OutValue = *StructProp->ContainerPtrToValuePtr<FRotator>(Container);
			return true;
		}
	}

	return false;
}

bool ReadTopLevelByte(UAnimInstance* Instance, const TCHAR* PropName, uint8& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	return ReadByteValue(FindProp(Instance->GetClass(), PropName), Instance, OutValue);
}

bool ReadTopLevelFloat(UAnimInstance* Instance, const TCHAR* PropName, float& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	return ReadFloatValue(FindProp(Instance->GetClass(), PropName), Instance, OutValue);
}

bool ReadTopLevelBool(UAnimInstance* Instance, const TCHAR* PropName, bool& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	return ReadBoolValue(FindProp(Instance->GetClass(), PropName), Instance, OutValue);
}

bool ReadTopLevelRotator(UAnimInstance* Instance, const TCHAR* PropName, FRotator& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	return ReadRotatorValue(FindProp(Instance->GetClass(), PropName), Instance, OutValue);
}

bool ReadObjectNameValue(FProperty* Prop, void* Container, FString& OutValue)
{
	if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Prop))
	{
		if (UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(Container)))
		{
			OutValue = ObjectValue->GetName();
			return true;
		}

		OutValue = TEXT("none");
		return true;
	}

	return false;
}

bool ReadStringArrayValue(FProperty* Prop, void* Container, TArray<FString>& OutValues)
{
	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
	if (!ArrayProp || !Container)
	{
		return false;
	}

	if (!CastField<FStrProperty>(ArrayProp->Inner))
	{
		return false;
	}

	FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Container));
	OutValues.Reset();
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		const FString* ValuePtr = reinterpret_cast<const FString*>(Helper.GetRawPtr(Index));
		OutValues.Add(ValuePtr ? *ValuePtr : FString());
	}
	return true;
}

bool ReadTopLevelObjectName(UAnimInstance* Instance, const TCHAR* PropName, FString& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	return ReadObjectNameValue(FindProp(Instance->GetClass(), PropName), Instance, OutValue);
}

bool ReadTopLevelStringArray(UAnimInstance* Instance, const TCHAR* PropName, TArray<FString>& OutValues)
{
	if (!Instance)
	{
		return false;
	}

	return ReadStringArrayValue(FindProp(Instance->GetClass(), PropName), Instance, OutValues);
}

bool CallAnimBoolFunction(UAnimInstance* Instance, const TCHAR* FunctionName, bool& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	UFunction* Func = Instance->FindFunction(FName(FunctionName));
	if (!Func)
	{
		return false;
	}

	struct FBoolReturnParams
	{
		bool ReturnValue = false;
	};

	FBoolReturnParams Params;
	Instance->ProcessEvent(Func, &Params);
	OutValue = Params.ReturnValue;
	return true;
}

bool CallAnimVectorFunction(UAnimInstance* Instance, const TCHAR* FunctionName, FVector& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	UFunction* Func = Instance->FindFunction(FName(FunctionName));
	if (!Func)
	{
		return false;
	}

	struct FVectorReturnParams
	{
		FVector ReturnValue = FVector::ZeroVector;
	};

	FVectorReturnParams Params;
	Instance->ProcessEvent(Func, &Params);
	OutValue = Params.ReturnValue;
	return true;
}

bool ReadCharacterPropertiesByte(UAnimInstance* Instance, const TCHAR* FieldName, uint8& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	FStructProperty* StructProp = CastField<FStructProperty>(FindProp(Instance->GetClass(), TEXT("CharacterProperties")));
	if (!StructProp)
	{
		return false;
	}

	void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(Instance);
	return ReadByteValue(FindSubProp(StructProp->Struct, FieldName), StructPtr, OutValue);
}

bool ReadCharacterPropertiesRotator(UAnimInstance* Instance, const TCHAR* FieldName, FRotator& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	FStructProperty* StructProp = CastField<FStructProperty>(FindProp(Instance->GetClass(), TEXT("CharacterProperties")));
	if (!StructProp)
	{
		return false;
	}

	void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(Instance);
	return ReadRotatorValue(FindSubProp(StructProp->Struct, FieldName), StructPtr, OutValue);
}

bool ReadCharacterPropertiesInputStateBool(UAnimInstance* Instance, const TCHAR* FieldName, bool& OutValue)
{
	if (!Instance)
	{
		return false;
	}

	FStructProperty* StructProp = CastField<FStructProperty>(FindProp(Instance->GetClass(), TEXT("CharacterProperties")));
	if (!StructProp)
	{
		return false;
	}

	void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(Instance);
	FStructProperty* InputStateProp = CastField<FStructProperty>(FindSubProp(StructProp->Struct, TEXT("InputState")));
	if (!InputStateProp)
	{
		return false;
	}

	void* InputStatePtr = InputStateProp->ContainerPtrToValuePtr<void>(StructPtr);
	return ReadBoolValue(FindSubProp(InputStateProp->Struct, FieldName), InputStatePtr, OutValue);
}

USkeletalMeshComponent* FindMeshByRole(AActor* Owner, const TCHAR* RoleName)
{
	if (!Owner)
	{
		return nullptr;
	}

	const FName RoleTag(*FString::Printf(TEXT("AssemblyRole=%s"), RoleName));

	TArray<USkeletalMeshComponent*> Components;
	Owner->GetComponents<USkeletalMeshComponent>(Components);

	for (USkeletalMeshComponent* Component : Components)
	{
		if (Component->ComponentTags.Contains(RoleTag))
		{
			return Component;
		}
	}

	return nullptr;
}

USkeletalMeshComponent* FindMeshByNameSubstring(AActor* Owner, const TCHAR* Substring)
{
	if (!Owner)
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> Components;
	Owner->GetComponents<USkeletalMeshComponent>(Components);

	for (USkeletalMeshComponent* Component : Components)
	{
		if (Component->GetName().Contains(Substring))
		{
			return Component;
		}
	}

	return nullptr;
}

USkeletalMeshComponent* FindDriverMesh(ACharacter* Character)
{
	if (!Character)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* Driver = FindMeshByRole(Character, TEXT("DriverBody")))
	{
		return Driver;
	}

	return Character->GetMesh();
}

USkeletalMeshComponent* FindWorldVisibleMesh(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* Mesh = FindMeshByRole(Owner, TEXT("WorldBody")))
	{
		if (!Mesh->bHiddenInGame && !Mesh->bOnlyOwnerSee && Mesh->GetSkeletalMeshAsset())
		{
			return Mesh;
		}
	}

	if (USkeletalMeshComponent* Mesh = FindMeshByRole(Owner, TEXT("BodyCustomization")))
	{
		if (!Mesh->bHiddenInGame && !Mesh->bOnlyOwnerSee && Mesh->GetSkeletalMeshAsset())
		{
			return Mesh;
		}
	}

	return FindMeshByNameSubstring(Owner, TEXT("WorldBody"));
}

USkeletalMeshComponent* FindOwnerVisibleMesh(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* Mesh = FindMeshByRole(Owner, TEXT("LocalBodyCustomization")))
	{
		if (!Mesh->bHiddenInGame && Mesh->GetSkeletalMeshAsset())
		{
			return Mesh;
		}
	}

	if (USkeletalMeshComponent* Mesh = FindMeshByRole(Owner, TEXT("LocalBody")))
	{
		if (!Mesh->bHiddenInGame && Mesh->GetSkeletalMeshAsset())
		{
			return Mesh;
		}
	}

	TArray<USkeletalMeshComponent*> Components;
	Owner->GetComponents<USkeletalMeshComponent>(Components);
	for (USkeletalMeshComponent* Component : Components)
	{
		if (Component->bOnlyOwnerSee)
		{
			return Component;
		}
	}

	return FindMeshByNameSubstring(Owner, TEXT("LocalBody"));
}

struct FMeshSample
{
	bool bFound = false;
	FString Name;
	FString Asset;
	FString LeaderPose;
	FString AnimClass;
	FString Tags;
	bool bHiddenInGame = false;
	bool bOnlyOwnerSee = false;
	bool bOwnerNoSee = false;
	float MeshYawWorld = 0.f;
	float RootFacingYawWorld = 0.f;
	float PelvisFacingYawComponent = 0.f;
	float SpineFacingYawComponent = 0.f;
	float HeadFacingYawComponent = 0.f;
	FString RootCS;
	FString PelvisCS;
	FString SpineCS;
	FString HeadCS;
	FString FootLCS;
	FString FootRCS;
};

bool CaptureBone(USkeletalMeshComponent* Mesh, const FName BoneName, FTransform& OutCS, FTransform& OutWS)
{
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		return false;
	}

	if (Mesh->GetBoneIndex(BoneName) == INDEX_NONE)
	{
		return false;
	}

	OutCS = Mesh->GetSocketTransform(BoneName, RTS_Component);
	OutWS = Mesh->GetSocketTransform(BoneName, RTS_World);
	return true;
}

FMeshSample SampleMesh(USkeletalMeshComponent* Mesh)
{
	FMeshSample Sample;
	if (!Mesh)
	{
		return Sample;
	}

	Sample.bFound = true;
	Sample.Name = Mesh->GetName();
	Sample.Asset = Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetName() : TEXT("null");
	Sample.LeaderPose = Mesh->LeaderPoseComponent.IsValid() ? Mesh->LeaderPoseComponent->GetName() : TEXT("none");
	Sample.AnimClass = Mesh->GetAnimInstance() ? Mesh->GetAnimInstance()->GetClass()->GetName() : TEXT("none");
	Sample.bHiddenInGame = Mesh->bHiddenInGame;
	Sample.bOnlyOwnerSee = Mesh->bOnlyOwnerSee;
	Sample.bOwnerNoSee = Mesh->bOwnerNoSee;
	Sample.MeshYawWorld = Mesh->GetComponentRotation().Yaw;
	for (int32 TagIdx = 0; TagIdx < Mesh->ComponentTags.Num(); ++TagIdx)
	{
		if (TagIdx > 0)
		{
			Sample.Tags += TEXT(",");
		}
		Sample.Tags += Mesh->ComponentTags[TagIdx].ToString();
	}

	FTransform CS;
	FTransform WS;
	if (CaptureBone(Mesh, TEXT("root"), CS, WS))
	{
		Sample.RootCS = TransformToCompactString(CS);
		Sample.RootFacingYawWorld = PlanarHeadingFromTransform(WS);
	}
	if (CaptureBone(Mesh, TEXT("pelvis"), CS, WS))
	{
		Sample.PelvisCS = TransformToCompactString(CS);
		Sample.PelvisFacingYawComponent = PlanarHeadingFromTransform(CS);
	}
	if (CaptureBone(Mesh, TEXT("spine_03"), CS, WS))
	{
		Sample.SpineCS = TransformToCompactString(CS);
		Sample.SpineFacingYawComponent = PlanarHeadingFromTransform(CS);
	}
	if (CaptureBone(Mesh, TEXT("head"), CS, WS))
	{
		Sample.HeadCS = TransformToCompactString(CS);
		Sample.HeadFacingYawComponent = PlanarHeadingFromTransform(CS);
	}
	if (CaptureBone(Mesh, TEXT("foot_l"), CS, WS))
	{
		Sample.FootLCS = TransformToCompactString(CS);
	}
	if (CaptureBone(Mesh, TEXT("foot_r"), CS, WS))
	{
		Sample.FootRCS = TransformToCompactString(CS);
	}

	return Sample;
}

TSharedPtr<FJsonObject> MeshSampleToJson(const FMeshSample& Mesh)
{
	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("Found"), Mesh.bFound);
	Object->SetStringField(TEXT("Name"), Mesh.Name);
	Object->SetStringField(TEXT("Asset"), Mesh.Asset);
	Object->SetStringField(TEXT("LeaderPose"), Mesh.LeaderPose);
	Object->SetStringField(TEXT("AnimClass"), Mesh.AnimClass);
	Object->SetStringField(TEXT("Tags"), Mesh.Tags);
	Object->SetBoolField(TEXT("HiddenInGame"), Mesh.bHiddenInGame);
	Object->SetBoolField(TEXT("OnlyOwnerSee"), Mesh.bOnlyOwnerSee);
	Object->SetBoolField(TEXT("OwnerNoSee"), Mesh.bOwnerNoSee);
	Object->SetNumberField(TEXT("MeshYawWorld"), Mesh.MeshYawWorld);
	Object->SetNumberField(TEXT("RootFacingYawWorld"), Mesh.RootFacingYawWorld);
	Object->SetNumberField(TEXT("PelvisFacingYawComponent"), Mesh.PelvisFacingYawComponent);
	Object->SetNumberField(TEXT("SpineFacingYawComponent"), Mesh.SpineFacingYawComponent);
	Object->SetNumberField(TEXT("HeadFacingYawComponent"), Mesh.HeadFacingYawComponent);
	Object->SetStringField(TEXT("RootCS"), Mesh.RootCS);
	Object->SetStringField(TEXT("PelvisCS"), Mesh.PelvisCS);
	Object->SetStringField(TEXT("SpineCS"), Mesh.SpineCS);
	Object->SetStringField(TEXT("HeadCS"), Mesh.HeadCS);
	Object->SetStringField(TEXT("FootLCS"), Mesh.FootLCS);
	Object->SetStringField(TEXT("FootRCS"), Mesh.FootRCS);
	return Object;
}

struct FMeshRestoreState
{
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;
	TObjectPtr<UClass> AnimClass = nullptr;
	EAnimationMode::Type AnimationMode = EAnimationMode::AnimationBlueprint;
	bool bHiddenInGame = false;
	bool bOnlyOwnerSee = false;
	bool bOwnerNoSee = false;
	bool bCastHiddenShadow = false;
	TWeakObjectPtr<USkinnedMeshComponent> LeaderPose;
};

FMeshRestoreState CaptureRestoreState(USkeletalMeshComponent* Mesh)
{
	FMeshRestoreState State;
	State.Mesh = Mesh;
	if (!Mesh)
	{
		return State;
	}

	State.SkeletalMesh = Mesh->GetSkeletalMeshAsset();
	State.AnimClass = Mesh->GetAnimClass();
	State.AnimationMode = Mesh->GetAnimationMode();
	State.bHiddenInGame = Mesh->bHiddenInGame;
	State.bOnlyOwnerSee = Mesh->bOnlyOwnerSee;
	State.bOwnerNoSee = Mesh->bOwnerNoSee;
	State.bCastHiddenShadow = Mesh->bCastHiddenShadow;
	State.LeaderPose = Mesh->LeaderPoseComponent.Get();
	return State;
}

void RestoreMeshState(const FMeshRestoreState& State)
{
	USkeletalMeshComponent* Mesh = State.Mesh.Get();
	if (!Mesh)
	{
		return;
	}

	Mesh->SetSkeletalMeshAsset(State.SkeletalMesh);
	Mesh->SetAnimationMode(State.AnimationMode);
	Mesh->SetAnimInstanceClass(State.AnimClass.Get());
	Mesh->SetHiddenInGame(State.bHiddenInGame);
	Mesh->SetOnlyOwnerSee(State.bOnlyOwnerSee);
	Mesh->SetOwnerNoSee(State.bOwnerNoSee);
	Mesh->SetCastHiddenShadow(State.bCastHiddenShadow);
	Mesh->SetLeaderPoseComponent(State.LeaderPose.Get());
	if (State.AnimationMode == EAnimationMode::AnimationBlueprint && State.AnimClass)
	{
		Mesh->InitAnim(true);
	}
}

void EnsureBlueprintAnimReady(USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}

	if (Mesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	}

	if (Mesh->GetAnimClass())
	{
		Mesh->InitAnim(true);
	}
}

struct FCleanPathPhaseSummary
{
	FString PhaseName;
	int32 SampleCount = 0;
	bool bExpectYawResponse = false;
	bool bExpectLargeBodyTurn = false;
	bool bExpectTurnState = false;
	bool bHasDriver = false;
	bool bHasVisible = false;
	bool bHasWorld = false;
	float MaxAbsActorControlDelta = 0.f;
	float MaxAbsDriverVisibleHeadDelta = 0.f;
	float MaxAbsDriverVisibleRootDelta = 0.f;
	float MaxAbsControlOrientationIntentDelta = 0.f;
	float MaxAbsControlAimingDelta = 0.f;
	float MaxAbsTargetRotationDelta = 0.f;
	float MaxAbsActorYawFromStart = 0.f;
	float MaxAbsDriverRootYawFromStart = 0.f;
	float MaxAbsDriverHeadYawFromStart = 0.f;
	float MaxAbsVisibleRootYawFromStart = 0.f;
	float MaxAbsVisiblePelvisYawFromStart = 0.f;
	float MaxAbsVisibleSpineYawFromStart = 0.f;
	float MaxAbsVisibleHeadYawFromStart = 0.f;
	float MaxAbsWorldHeadYawFromStart = 0.f;
	float MaxAbsWorldVisiblePelvisDelta = 0.f;
	float MaxAbsWorldVisibleSpineDelta = 0.f;
	float MaxAbsWorldVisibleHeadDelta = 0.f;
	float MaxAbsControlYawFromStart = 0.f;
	float MaxSpeed2D = 0.f;
	float MaxAbsAOYaw = 0.f;
	bool bStartCaptured = false;
	float StartActorYaw = 0.f;
	float StartControlYaw = 0.f;
	float StartDriverRootYaw = 0.f;
	float StartDriverHeadYaw = 0.f;
	float StartVisibleRootYaw = 0.f;
	float StartVisiblePelvisYaw = 0.f;
	float StartVisibleSpineYaw = 0.f;
	float StartVisibleHeadYaw = 0.f;
	float StartWorldHeadYaw = 0.f;
	int32 StartTransitionHistoryCount = 0;
	int32 MaxTransitionHistoryCount = 0;
	FString StartSelectedAnim;
	FString LastSelectedAnim;
	bool bSawTopLevelRotationMode = false;
	uint8 MinTopLevelRotationMode = 255;
	uint8 MaxTopLevelRotationMode = 0;
	bool bAnyAimOffsetEnabled = false;
	bool bAnyTurnInPlaceRequested = false;
	bool bTurnTransitionObserved = false;
	bool bRotationBreakObserved = false;
	bool bSelectedAnimChanged = false;
	bool bPersistentTurnClipObserved = false;
	bool bTurnStateObserved = false;
	bool bActorCatchUpObserved = false;
	bool bRootTurnObserved = false;
	bool bAnyWantsToStrafe = false;
	bool bAnyWantsToAim = false;
	bool bAnyWantsToCrouch = false;
	bool bBridgeTracksControlYaw = false;
	bool bDriverRespondedToYaw = false;
	bool bVisibleRespondedToYaw = false;
	bool bBodyRespondedToYaw = false;
	bool bVisibleTracksDriver = false;
	bool bVisibleTracksWorld = false;
	bool bLargeBodyTurnObserved = false;
	bool bSawRetargetWorldVisual = false;
};

void UpdateYawSummary(
	FCleanPathPhaseSummary& Summary,
	float ActorYaw,
	float ControlYaw,
	float Speed2D,
	float TargetRotationDelta,
	const TOptional<uint8>& TopLevelRotationMode,
	const TOptional<FRotator>& OrientationIntent,
	const TOptional<FRotator>& AimingRotation,
	const TOptional<bool>& bAimOffsetEnabled,
	const TOptional<float>& AOYaw,
	const TOptional<bool>& bShouldTurnInPlace,
	const TArray<FString>* TransitionHistory,
	const FString* CurrentSelectedAnim,
	const FMeshSample& Driver,
	const FMeshSample& Visible,
	const FMeshSample& World)
{
	++Summary.SampleCount;
	Summary.MaxSpeed2D = FMath::Max(Summary.MaxSpeed2D, Speed2D);
	Summary.MaxAbsActorControlDelta = FMath::Max(
		Summary.MaxAbsActorControlDelta,
		FMath::Abs(NormalizeYawDelta(ControlYaw, ActorYaw)));
	Summary.MaxAbsTargetRotationDelta = FMath::Max(
		Summary.MaxAbsTargetRotationDelta,
		FMath::Abs(TargetRotationDelta));

	if (TopLevelRotationMode.IsSet())
	{
		Summary.bSawTopLevelRotationMode = true;
		Summary.MinTopLevelRotationMode = FMath::Min(Summary.MinTopLevelRotationMode, TopLevelRotationMode.GetValue());
		Summary.MaxTopLevelRotationMode = FMath::Max(Summary.MaxTopLevelRotationMode, TopLevelRotationMode.GetValue());
	}

	if (OrientationIntent.IsSet())
	{
		Summary.MaxAbsControlOrientationIntentDelta = FMath::Max(
			Summary.MaxAbsControlOrientationIntentDelta,
			FMath::Abs(NormalizeYawDelta(ControlYaw, OrientationIntent.GetValue().Yaw)));
	}

	if (AimingRotation.IsSet())
	{
		Summary.MaxAbsControlAimingDelta = FMath::Max(
			Summary.MaxAbsControlAimingDelta,
			FMath::Abs(NormalizeYawDelta(ControlYaw, AimingRotation.GetValue().Yaw)));
	}

	if (bAimOffsetEnabled.IsSet())
	{
		Summary.bAnyAimOffsetEnabled = Summary.bAnyAimOffsetEnabled || bAimOffsetEnabled.GetValue();
	}

	if (AOYaw.IsSet())
	{
		Summary.MaxAbsAOYaw = FMath::Max(Summary.MaxAbsAOYaw, FMath::Abs(AOYaw.GetValue()));
	}

	if (bShouldTurnInPlace.IsSet())
	{
		Summary.bAnyTurnInPlaceRequested = Summary.bAnyTurnInPlaceRequested || bShouldTurnInPlace.GetValue();
	}

	if (!Summary.bStartCaptured)
	{
		Summary.bStartCaptured = true;
		Summary.StartActorYaw = ActorYaw;
		Summary.StartControlYaw = ControlYaw;
		Summary.StartTransitionHistoryCount = TransitionHistory ? TransitionHistory->Num() : 0;
		Summary.StartSelectedAnim = CurrentSelectedAnim ? *CurrentSelectedAnim : TEXT("none");
		Summary.LastSelectedAnim = Summary.StartSelectedAnim;
		if (Driver.bFound)
		{
			Summary.StartDriverRootYaw = Driver.RootFacingYawWorld;
			Summary.StartDriverHeadYaw = Driver.HeadFacingYawComponent;
			Summary.bHasDriver = true;
		}
		if (Visible.bFound)
		{
			Summary.StartVisibleRootYaw = Visible.RootFacingYawWorld;
			Summary.StartVisiblePelvisYaw = Visible.PelvisFacingYawComponent;
			Summary.StartVisibleSpineYaw = Visible.SpineFacingYawComponent;
			Summary.StartVisibleHeadYaw = Visible.HeadFacingYawComponent;
			Summary.bHasVisible = true;
		}
		if (World.bFound)
		{
			Summary.StartWorldHeadYaw = World.HeadFacingYawComponent;
			Summary.bHasWorld = true;
		}
	}

	Summary.MaxAbsActorYawFromStart = FMath::Max(
		Summary.MaxAbsActorYawFromStart,
		FMath::Abs(NormalizeYawDelta(ActorYaw, Summary.StartActorYaw)));
	Summary.MaxAbsControlYawFromStart = FMath::Max(
		Summary.MaxAbsControlYawFromStart,
		FMath::Abs(NormalizeYawDelta(ControlYaw, Summary.StartControlYaw)));

	if (Driver.bFound)
	{
		Summary.bHasDriver = true;
		Summary.MaxAbsDriverRootYawFromStart = FMath::Max(
			Summary.MaxAbsDriverRootYawFromStart,
			FMath::Abs(NormalizeYawDelta(Driver.RootFacingYawWorld, Summary.StartDriverRootYaw)));
		Summary.MaxAbsDriverHeadYawFromStart = FMath::Max(
			Summary.MaxAbsDriverHeadYawFromStart,
			FMath::Abs(NormalizeYawDelta(Driver.HeadFacingYawComponent, Summary.StartDriverHeadYaw)));
	}

	if (Visible.bFound)
	{
		Summary.bHasVisible = true;
		Summary.MaxAbsVisibleRootYawFromStart = FMath::Max(
			Summary.MaxAbsVisibleRootYawFromStart,
			FMath::Abs(NormalizeYawDelta(Visible.RootFacingYawWorld, Summary.StartVisibleRootYaw)));
		Summary.MaxAbsVisiblePelvisYawFromStart = FMath::Max(
			Summary.MaxAbsVisiblePelvisYawFromStart,
			FMath::Abs(NormalizeYawDelta(Visible.PelvisFacingYawComponent, Summary.StartVisiblePelvisYaw)));
		Summary.MaxAbsVisibleSpineYawFromStart = FMath::Max(
			Summary.MaxAbsVisibleSpineYawFromStart,
			FMath::Abs(NormalizeYawDelta(Visible.SpineFacingYawComponent, Summary.StartVisibleSpineYaw)));
		Summary.MaxAbsVisibleHeadYawFromStart = FMath::Max(
			Summary.MaxAbsVisibleHeadYawFromStart,
			FMath::Abs(NormalizeYawDelta(Visible.HeadFacingYawComponent, Summary.StartVisibleHeadYaw)));
	}

	if (World.bFound)
	{
		Summary.bHasWorld = true;
		Summary.MaxAbsWorldHeadYawFromStart = FMath::Max(
			Summary.MaxAbsWorldHeadYawFromStart,
			FMath::Abs(NormalizeYawDelta(World.HeadFacingYawComponent, Summary.StartWorldHeadYaw)));
		Summary.bSawRetargetWorldVisual =
			Summary.bSawRetargetWorldVisual ||
			World.AnimClass.Contains(TEXT("ABP_WorldBodyRetarget"));
	}

	if (Driver.bFound && Visible.bFound)
	{
		Summary.MaxAbsDriverVisibleHeadDelta = FMath::Max(
			Summary.MaxAbsDriverVisibleHeadDelta,
			FMath::Abs(NormalizeYawDelta(Driver.HeadFacingYawComponent, Visible.HeadFacingYawComponent)));
		Summary.MaxAbsDriverVisibleRootDelta = FMath::Max(
			Summary.MaxAbsDriverVisibleRootDelta,
			FMath::Abs(NormalizeYawDelta(Driver.RootFacingYawWorld, Visible.RootFacingYawWorld)));
	}

	if (World.bFound && Visible.bFound)
	{
		Summary.MaxAbsWorldVisiblePelvisDelta = FMath::Max(
			Summary.MaxAbsWorldVisiblePelvisDelta,
			FMath::Abs(NormalizeYawDelta(World.PelvisFacingYawComponent, Visible.PelvisFacingYawComponent)));
		Summary.MaxAbsWorldVisibleSpineDelta = FMath::Max(
			Summary.MaxAbsWorldVisibleSpineDelta,
			FMath::Abs(NormalizeYawDelta(World.SpineFacingYawComponent, Visible.SpineFacingYawComponent)));
		Summary.MaxAbsWorldVisibleHeadDelta = FMath::Max(
			Summary.MaxAbsWorldVisibleHeadDelta,
			FMath::Abs(NormalizeYawDelta(World.HeadFacingYawComponent, Visible.HeadFacingYawComponent)));
	}

	if (TransitionHistory)
	{
		Summary.MaxTransitionHistoryCount = FMath::Max(Summary.MaxTransitionHistoryCount, TransitionHistory->Num());
		for (int32 Index = Summary.StartTransitionHistoryCount; Index < TransitionHistory->Num(); ++Index)
		{
			const FString& Entry = (*TransitionHistory)[Index];
			Summary.bTurnTransitionObserved = Summary.bTurnTransitionObserved || Entry.Contains(TEXT("Turn In Place"));
			Summary.bRotationBreakObserved = Summary.bRotationBreakObserved || Entry.Contains(TEXT("Broke"));
		}
	}

	if (CurrentSelectedAnim)
	{
		Summary.LastSelectedAnim = *CurrentSelectedAnim;
		Summary.bSelectedAnimChanged =
			Summary.bSelectedAnimChanged ||
			(!Summary.StartSelectedAnim.IsEmpty() && Summary.StartSelectedAnim != *CurrentSelectedAnim);
		Summary.bPersistentTurnClipObserved =
			Summary.bPersistentTurnClipObserved || IsTurnClipName(*CurrentSelectedAnim);
	}
}

void FinalizeYawSummary(FCleanPathPhaseSummary& Summary)
{
	Summary.bBridgeTracksControlYaw =
		Summary.MaxAbsControlOrientationIntentDelta <= 5.f &&
		Summary.MaxAbsControlAimingDelta <= 5.f;

	Summary.bDriverRespondedToYaw =
		Summary.MaxAbsDriverHeadYawFromStart >= 15.f ||
		Summary.MaxAbsDriverRootYawFromStart >= 15.f ||
		Summary.MaxAbsTargetRotationDelta >= 15.f;

	Summary.bVisibleRespondedToYaw =
		Summary.MaxAbsVisibleHeadYawFromStart >= 10.f ||
		Summary.MaxAbsVisibleSpineYawFromStart >= 10.f ||
		Summary.MaxAbsVisibleRootYawFromStart >= 10.f;

	Summary.bBodyRespondedToYaw =
		Summary.MaxAbsVisiblePelvisYawFromStart >= 10.f ||
		Summary.MaxAbsVisibleSpineYawFromStart >= 15.f ||
		Summary.MaxAbsVisibleRootYawFromStart >= 10.f;

	Summary.bVisibleTracksDriver =
		!Summary.bHasDriver || !Summary.bHasVisible ||
		(Summary.MaxAbsDriverVisibleHeadDelta <= 15.f &&
		 Summary.MaxAbsDriverVisibleRootDelta <= 15.f);

	Summary.bVisibleTracksWorld =
		!Summary.bHasWorld || !Summary.bHasVisible ||
		(Summary.MaxAbsWorldVisiblePelvisDelta <= 5.f &&
		 Summary.MaxAbsWorldVisibleSpineDelta <= 5.f &&
		 Summary.MaxAbsWorldVisibleHeadDelta <= 5.f);

	Summary.bActorCatchUpObserved =
		Summary.MaxAbsActorYawFromStart >= 10.f ||
		Summary.MaxAbsVisibleRootYawFromStart >= 10.f ||
		Summary.MaxAbsTargetRotationDelta >= 15.f;
	Summary.bRootTurnObserved = Summary.bActorCatchUpObserved;

	const bool bBodyCatchUpObserved =
		Summary.bActorCatchUpObserved ||
		Summary.MaxAbsVisiblePelvisYawFromStart >= 20.f ||
		Summary.MaxAbsVisibleSpineYawFromStart >= 25.f ||
		Summary.MaxAbsVisibleRootYawFromStart >= 15.f;

	Summary.bTurnStateObserved =
		Summary.bAnyTurnInPlaceRequested ||
		Summary.bTurnTransitionObserved ||
		Summary.bRotationBreakObserved ||
		Summary.MaxAbsTargetRotationDelta >= 15.f ||
		(Summary.bPersistentTurnClipObserved && bBodyCatchUpObserved);

	Summary.bLargeBodyTurnObserved =
		Summary.bTurnStateObserved ||
		Summary.MaxAbsVisiblePelvisYawFromStart >= 20.f ||
		Summary.MaxAbsVisibleSpineYawFromStart >= 25.f ||
		Summary.MaxAbsVisibleRootYawFromStart >= 15.f;
}

TSharedPtr<FJsonObject> YawSummaryToJson(const FCleanPathPhaseSummary& Summary)
{
	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Phase"), Summary.PhaseName);
	Object->SetNumberField(TEXT("Samples"), Summary.SampleCount);
	Object->SetBoolField(TEXT("ExpectYawResponse"), Summary.bExpectYawResponse);
	Object->SetBoolField(TEXT("ExpectLargeBodyTurn"), Summary.bExpectLargeBodyTurn);
	Object->SetBoolField(TEXT("ExpectTurnState"), Summary.bExpectTurnState);
	Object->SetBoolField(TEXT("HasDriver"), Summary.bHasDriver);
	Object->SetBoolField(TEXT("HasVisible"), Summary.bHasVisible);
	Object->SetBoolField(TEXT("HasWorld"), Summary.bHasWorld);
	Object->SetNumberField(TEXT("MaxAbsActorControlDelta"), Summary.MaxAbsActorControlDelta);
	Object->SetNumberField(TEXT("MaxAbsDriverVisibleHeadDelta"), Summary.MaxAbsDriverVisibleHeadDelta);
	Object->SetNumberField(TEXT("MaxAbsDriverVisibleRootDelta"), Summary.MaxAbsDriverVisibleRootDelta);
	Object->SetNumberField(TEXT("MaxAbsControlOrientationIntentDelta"), Summary.MaxAbsControlOrientationIntentDelta);
	Object->SetNumberField(TEXT("MaxAbsControlAimingDelta"), Summary.MaxAbsControlAimingDelta);
	Object->SetNumberField(TEXT("MaxAbsTargetRotationDelta"), Summary.MaxAbsTargetRotationDelta);
	Object->SetNumberField(TEXT("MaxAbsActorYawFromStart"), Summary.MaxAbsActorYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsControlYawFromStart"), Summary.MaxAbsControlYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsDriverRootYawFromStart"), Summary.MaxAbsDriverRootYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsDriverHeadYawFromStart"), Summary.MaxAbsDriverHeadYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsVisibleRootYawFromStart"), Summary.MaxAbsVisibleRootYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsVisiblePelvisYawFromStart"), Summary.MaxAbsVisiblePelvisYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsVisibleSpineYawFromStart"), Summary.MaxAbsVisibleSpineYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsVisibleHeadYawFromStart"), Summary.MaxAbsVisibleHeadYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsWorldHeadYawFromStart"), Summary.MaxAbsWorldHeadYawFromStart);
	Object->SetNumberField(TEXT("MaxAbsWorldVisiblePelvisDelta"), Summary.MaxAbsWorldVisiblePelvisDelta);
	Object->SetNumberField(TEXT("MaxAbsWorldVisibleSpineDelta"), Summary.MaxAbsWorldVisibleSpineDelta);
	Object->SetNumberField(TEXT("MaxAbsWorldVisibleHeadDelta"), Summary.MaxAbsWorldVisibleHeadDelta);
	Object->SetNumberField(TEXT("MaxSpeed2D"), Summary.MaxSpeed2D);
	Object->SetNumberField(TEXT("MaxAbsAOYaw"), Summary.MaxAbsAOYaw);
	Object->SetNumberField(TEXT("StartTransitionHistoryCount"), Summary.StartTransitionHistoryCount);
	Object->SetNumberField(TEXT("MaxTransitionHistoryCount"), Summary.MaxTransitionHistoryCount);
	Object->SetStringField(TEXT("StartSelectedAnim"), Summary.StartSelectedAnim);
	Object->SetStringField(TEXT("LastSelectedAnim"), Summary.LastSelectedAnim);
	Object->SetBoolField(TEXT("SawTopLevelRotationMode"), Summary.bSawTopLevelRotationMode);
	Object->SetNumberField(TEXT("MinTopLevelRotationMode"),
		Summary.bSawTopLevelRotationMode ? Summary.MinTopLevelRotationMode : 255);
	Object->SetNumberField(TEXT("MaxTopLevelRotationMode"),
		Summary.bSawTopLevelRotationMode ? Summary.MaxTopLevelRotationMode : 255);
	Object->SetBoolField(TEXT("AnyAimOffsetEnabled"), Summary.bAnyAimOffsetEnabled);
	Object->SetBoolField(TEXT("AnyTurnInPlaceRequested"), Summary.bAnyTurnInPlaceRequested);
	Object->SetBoolField(TEXT("TurnTransitionObserved"), Summary.bTurnTransitionObserved);
	Object->SetBoolField(TEXT("RotationBreakObserved"), Summary.bRotationBreakObserved);
	Object->SetBoolField(TEXT("SelectedAnimChanged"), Summary.bSelectedAnimChanged);
	Object->SetBoolField(TEXT("PersistentTurnClipObserved"), Summary.bPersistentTurnClipObserved);
	Object->SetBoolField(TEXT("TurnStateObserved"), Summary.bTurnStateObserved);
	Object->SetBoolField(TEXT("ActorCatchUpObserved"), Summary.bActorCatchUpObserved);
	Object->SetBoolField(TEXT("RootTurnObserved"), Summary.bRootTurnObserved);
	Object->SetBoolField(TEXT("AnyWantsToStrafe"), Summary.bAnyWantsToStrafe);
	Object->SetBoolField(TEXT("AnyWantsToAim"), Summary.bAnyWantsToAim);
	Object->SetBoolField(TEXT("AnyWantsToCrouch"), Summary.bAnyWantsToCrouch);
	Object->SetBoolField(TEXT("BridgeTracksControlYaw"), Summary.bBridgeTracksControlYaw);
	Object->SetBoolField(TEXT("DriverRespondedToYaw"), Summary.bDriverRespondedToYaw);
	Object->SetBoolField(TEXT("VisibleRespondedToYaw"), Summary.bVisibleRespondedToYaw);
	Object->SetBoolField(TEXT("BodyRespondedToYaw"), Summary.bBodyRespondedToYaw);
	Object->SetBoolField(TEXT("VisibleTracksDriver"), Summary.bVisibleTracksDriver);
	Object->SetBoolField(TEXT("VisibleTracksWorld"), Summary.bVisibleTracksWorld);
	Object->SetBoolField(TEXT("LargeBodyTurnObserved"), Summary.bLargeBodyTurnObserved);
	Object->SetBoolField(TEXT("SawRetargetWorldVisual"), Summary.bSawRetargetWorldVisual);
	return Object;
}

} // namespace CleanPathHelpers

namespace CleanPathHelpers
{

class FCleanPathIsolationMatrixCommand : public IAutomationLatentCommand
{
public:
	explicit FCleanPathIsolationMatrixCommand(FAutomationTestBase* InTest)
		: Test(InTest)
		, RunId(FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")))
		, OutputDir(FPaths::ProjectSavedDir() / TEXT("Validation/CharacterDebug"))
	{}

	virtual bool Update() override
	{
		if (!Test)
		{
			return true;
		}

		const uint64 Frame = GFrameCounter;
		if (Frame == LastFrame)
		{
			return false;
		}
		LastFrame = Frame;
		++Tick;

		switch (Stage)
		{
		case 0: return WaitForPawn();
		case 1: return EnsureModularPawn();
		case 2: return WaitForFullChainReady();
		case 3: return RunModeMatrix();
		case 4: return WriteModeSummary();
		case 5:
			Test->AddInfo(TEXT("Clean path isolation matrix complete"));
			return true;
		default:
			return true;
		}
	}

private:
	bool WaitForPawn()
	{
		if (APlayerController* PC = FindPC())
		{
			World = PC->GetWorld();
			Test->AddInfo(FString::Printf(TEXT("CameraYaw pawn: %s RunId: %s"),
				*PC->GetPawn()->GetClass()->GetName(), *RunId));
			NextStage();
			return false;
		}

		if (Tick > 1800)
		{
			Test->AddError(TEXT("Timed out waiting for pawn"));
			return true;
		}

		return false;
	}

	bool EnsureModularPawn()
	{
		if (Tick < 120)
		{
			return false;
		}

		if (APlayerController* PC = FindPC())
		{
			if (PC->GetPawn() && PC->GetPawn()->GetClass()->GetName().Contains(TEXT("DefinitionCharacter")))
			{
				Test->AddInfo(TEXT("CameraYaw using modular default pawn"));
				NextStage();
				return false;
			}
		}

		GEngine->Exec(World, TEXT("project.character.switch modular"));
		Test->AddInfo(TEXT("CameraYaw switched to modular"));
		NextStage();
		return false;
	}

	bool WaitForFullChainReady()
	{
		if (APlayerController* PC = FindPC())
		{
			if (ACharacter* Character = Cast<ACharacter>(PC->GetPawn()))
			{
				World = PC->GetWorld();
				DriverMesh = FindDriverMesh(Character);
				WorldMesh = FindMeshByRole(Character, TEXT("WorldBody"));
				LocalMesh = FindMeshByRole(Character, TEXT("LocalBody"));
				HeadMesh = FindMeshByRole(Character, TEXT("Head"));
				BodyCustomizationMesh = FindMeshByRole(Character, TEXT("BodyCustomization"));
				HeadCustomizationMesh = FindMeshByRole(Character, TEXT("HeadCustomization"));
				LocalBodyCustomizationMesh = FindMeshByRole(Character, TEXT("LocalBodyCustomization"));

				const bool bReady =
					DriverMesh && DriverMesh->GetSkeletalMeshAsset() &&
					WorldMesh && WorldMesh->GetSkeletalMeshAsset() &&
					LocalMesh &&
					BodyCustomizationMesh && BodyCustomizationMesh->GetSkeletalMeshAsset() &&
					LocalBodyCustomizationMesh && LocalBodyCustomizationMesh->GetSkeletalMeshAsset();

				if (bReady)
				{
					CaptureOriginalStates();
					NextStage();
					return false;
				}
			}
		}

		if (Tick > 2400)
		{
			Test->AddError(TEXT("Timed out waiting for full modular chain"));
			return true;
		}

		return false;
	}

	bool RunModeMatrix()
	{
		if (ModeIdx >= GNumCleanPathModes)
		{
			NextStage();
			return false;
		}

		APlayerController* PC = FindPC();
		ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!PC || !Character)
		{
			Test->AddError(TEXT("CleanPath lost pawn during mode matrix"));
			return true;
		}

		const FCleanPathMode& Mode = GCleanPathModes[ModeIdx];
		if (!bModeConfigured)
		{
			RestoreOriginalStates();
			ApplyIsolationMode(Mode);
			ResetPhaseRunner();

			FRotator ControlRot = PC->GetControlRotation();
			ControlRot.Yaw = Character->GetActorRotation().Yaw;
			PC->SetControlRotation(ControlRot);
			BaseControlYaw = ControlRot.Yaw;

			if (Character->bIsCrouched)
			{
				Character->UnCrouch();
			}

			bModeConfigured = true;
			ModeSettleTicks = 0;
			CurrentModeLabel = Mode.Name;
			Test->AddInfo(FString::Printf(TEXT("[CleanPath] Applied %s"), *Mode.Name));
			return false;
		}

		if (ModeSettleTicks < 60)
		{
			++ModeSettleTicks;
			return false;
		}

		if (RunPhases(Mode))
		{
			return true;
		}

		if (PhaseIdx >= GNumCleanPathPhases)
		{
			ModeIdx++;
			bModeConfigured = false;
		}

		return false;
	}

	bool RunPhases(const FCleanPathMode& Mode)
	{
		if (PhaseIdx >= GNumCleanPathPhases)
		{
			return false;
		}

		if (!World)
		{
			return true;
		}

		const float DT = World->GetDeltaSeconds();
		if (DT <= 0.f)
		{
			return false;
		}

		APlayerController* PC = FindPC();
		if (!PC || !PC->GetPawn())
		{
			Test->AddError(TEXT("Pawn lost during camera-yaw timeline"));
			return true;
		}

		ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
		if (!Character)
		{
			Test->AddError(TEXT("Pawn is not ACharacter during camera-yaw timeline"));
			return true;
		}

		const FCleanPathPhase& Phase = GCleanPathPhases[PhaseIdx];

		if (!bPhaseStarted)
		{
			bPhaseStarted = true;
			PhaseElapsed = 0.f;
			SampleAccum = 0.f;
			PhaseSampleIdx = 0;

			if (Phase.bCrouch && !Character->bIsCrouched)
			{
				Character->Crouch();
			}
			if (!Phase.bCrouch && Character->bIsCrouched)
			{
				Character->UnCrouch();
			}
			if (Phase.bJump)
			{
				Character->Jump();
			}

			FCleanPathPhaseSummary Summary;
			Summary.PhaseName = Mode.Name + TEXT("/") + Phase.Name;
			Summary.bExpectYawResponse = Phase.bExpectVisibleResponse;
			Summary.bExpectLargeBodyTurn = Phase.bExpectLargeBodyTurn;
			Summary.bExpectTurnState = Phase.bExpectTurnState;
			CurrentSummaries.Add(Summary);
		}

		const float Alpha = FMath::Clamp(PhaseElapsed / FMath::Max(Phase.DurationSec, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const float DesiredYaw = BaseControlYaw + FMath::Lerp(Phase.StartYawOffsetDeg, Phase.EndYawOffsetDeg, Alpha);
		FRotator ControlRot = PC->GetControlRotation();
		ControlRot.Yaw = DesiredYaw;
		PC->SetControlRotation(ControlRot);

		if (!Phase.MoveInput.IsNearlyZero())
		{
			const FRotator YawRot(0.f, PC->GetControlRotation().Yaw, 0.f);
			const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			const FVector Direction = (Forward * Phase.MoveInput.X + Right * Phase.MoveInput.Y).GetSafeNormal();
			Character->AddMovementInput(Direction, 1.0f);
		}

		SampleAccum += DT;
		if (SampleAccum >= 0.25f)
		{
			SampleAccum -= 0.25f;
			CaptureSample(Character, PC, Mode, Phase, PhaseSampleIdx);
			++PhaseSampleIdx;
		}

		PhaseElapsed += DT;
		if (PhaseElapsed >= Phase.DurationSec)
		{
			FRotator EndControlRot = PC->GetControlRotation();
			EndControlRot.Yaw = BaseControlYaw + Phase.EndYawOffsetDeg;
			PC->SetControlRotation(EndControlRot);

			CaptureSample(Character, PC, Mode, Phase, PhaseSampleIdx);
			FinalizeYawSummary(CurrentSummaries.Last());

			const FCleanPathPhaseSummary& Summary = CurrentSummaries.Last();
			Test->AddInfo(FString::Printf(
				TEXT("[%s] %s: ActorDelta=%.1f VisibleRoot=%.1f VisiblePelvis=%.1f VisibleSpine=%.1f VisibleHead=%.1f AO=%.1f/%s TIPReq=%s TIPState=%s TIPEvent=%s RotBreak=%s TurnClip=%s AnimChange=%s Mode=%d-%d BridgeOK=%s WorldTrack=%s Retarget=%s"),
				*Mode.Name,
				*Phase.Name,
				Summary.MaxAbsActorControlDelta,
				Summary.MaxAbsVisibleRootYawFromStart,
				Summary.MaxAbsVisiblePelvisYawFromStart,
				Summary.MaxAbsVisibleSpineYawFromStart,
				Summary.MaxAbsVisibleHeadYawFromStart,
				Summary.MaxAbsAOYaw,
				Summary.bAnyAimOffsetEnabled ? TEXT("Y") : TEXT("N"),
				Summary.bAnyTurnInPlaceRequested ? TEXT("Y") : TEXT("N"),
				Summary.bTurnStateObserved ? TEXT("Y") : TEXT("N"),
				Summary.bTurnTransitionObserved ? TEXT("Y") : TEXT("N"),
				Summary.bRotationBreakObserved ? TEXT("Y") : TEXT("N"),
				Summary.bPersistentTurnClipObserved ? TEXT("Y") : TEXT("N"),
				Summary.bSelectedAnimChanged ? TEXT("Y") : TEXT("N"),
				Summary.bSawTopLevelRotationMode ? Summary.MinTopLevelRotationMode : 255,
				Summary.bSawTopLevelRotationMode ? Summary.MaxTopLevelRotationMode : 255,
				Summary.bBridgeTracksControlYaw ? TEXT("Y") : TEXT("N"),
				Summary.bVisibleTracksWorld ? TEXT("Y") : TEXT("N"),
				Summary.bSawRetargetWorldVisual ? TEXT("Y") : TEXT("N")));

			++PhaseIdx;
			bPhaseStarted = false;
		}

		return false;
	}

	void CaptureSample(
		ACharacter* Character,
		APlayerController* PC,
		const FCleanPathMode& Mode,
		const FCleanPathPhase& Phase,
		int32 SampleIdx)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
		UAnimInstance* ABP = FindPrimaryABP(Character);

		const float ActorYaw = Character->GetActorRotation().Yaw;
		const float ControlYaw = PC->GetControlRotation().Yaw;
		const float ActorControlDelta = NormalizeYawDelta(ControlYaw, ActorYaw);

		float Speed2D = CMC ? CMC->Velocity.Size2D() : 0.f;
		float ABPSpeed2D = 0.f;
		float TargetRotationDelta = 0.f;
		bool bNoValidAnim = false;
		uint8 MovementState = 255;
		uint8 MovementDirection = 255;
		uint8 RotationMode = 255;
		uint8 TopLevelRotationMode = 255;
		uint8 StateMachineState = 255;
		uint8 MovementMode = 255;
		uint8 Gait = 255;
		uint8 Stance = 255;
		FRotator AimingRotation = FRotator::ZeroRotator;
		FRotator OrientationIntent = FRotator::ZeroRotator;
		FRotator TargetRotation = FRotator::ZeroRotator;
		bool bEnableAO = false;
		bool bShouldTurnInPlace = false;
		bool bWantsToStrafe = false;
		bool bWantsToAim = false;
		bool bWantsToCrouch = false;
		FVector AOValue = FVector::ZeroVector;
		FString CurrentSelectedAnimName;
		TArray<FString> TransitionHistory;
		const bool bHasABPSpeed = ReadTopLevelFloat(ABP, TEXT("Speed2D"), ABPSpeed2D);
		const bool bHasTargetDelta = ReadTopLevelFloat(ABP, TEXT("TargetRotationDelta"), TargetRotationDelta);
		const bool bHasNoValidAnim = ReadTopLevelBool(ABP, TEXT("NoValidAnim"), bNoValidAnim);
		const bool bHasMovementState = ReadTopLevelByte(ABP, TEXT("MovementState"), MovementState);
		const bool bHasMovementDirection = ReadTopLevelByte(ABP, TEXT("MovementDirection"), MovementDirection);
		const bool bHasTopLevelRotationMode = ReadTopLevelByte(ABP, TEXT("RotationMode"), TopLevelRotationMode);
		const bool bHasStateMachineState = ReadTopLevelByte(ABP, TEXT("StateMachineState"), StateMachineState);
		const bool bHasTargetRotation = ReadTopLevelRotator(ABP, TEXT("TargetRotation"), TargetRotation);
		const bool bHasRotationMode = ReadCharacterPropertiesByte(ABP, TEXT("RotationMode"), RotationMode);
		const bool bHasMovementMode = ReadCharacterPropertiesByte(ABP, TEXT("MovementMode"), MovementMode);
		const bool bHasGait = ReadCharacterPropertiesByte(ABP, TEXT("Gait"), Gait);
		const bool bHasStance = ReadCharacterPropertiesByte(ABP, TEXT("Stance"), Stance);
		const bool bHasOrientationIntent = ReadCharacterPropertiesRotator(ABP, TEXT("OrientationIntent"), OrientationIntent);
		const bool bHasAimingRotation = ReadCharacterPropertiesRotator(ABP, TEXT("AimingRotation"), AimingRotation);
		const bool bHasWantsToStrafe = ReadCharacterPropertiesInputStateBool(ABP, TEXT("WantsToStrafe"), bWantsToStrafe);
		const bool bHasWantsToAim = ReadCharacterPropertiesInputStateBool(ABP, TEXT("WantsToAim"), bWantsToAim);
		const bool bHasWantsToCrouch = ReadCharacterPropertiesInputStateBool(ABP, TEXT("WantsToCrouch"), bWantsToCrouch);
		const bool bHasEnableAO = CallAnimBoolFunction(ABP, TEXT("Enable_AO"), bEnableAO);
		const bool bHasShouldTurnInPlace = CallAnimBoolFunction(ABP, TEXT("ShouldTurnInPlace"), bShouldTurnInPlace);
		const bool bHasAOValue = CallAnimVectorFunction(ABP, TEXT("Get_AOValue"), AOValue);
		const bool bHasCurrentSelectedAnim = ReadTopLevelObjectName(ABP, TEXT("CurrentSelectedAnim"), CurrentSelectedAnimName);
		const bool bHasTransitionHistory = ReadTopLevelStringArray(ABP, TEXT("TransitionHistory"), TransitionHistory);

		const FMeshSample Driver = SampleMesh(FindDriverMesh(Character));
		const FMeshSample WorldVisible = SampleMesh(FindWorldVisibleMesh(Character));
		const FMeshSample OwnerVisible = SampleMesh(FindOwnerVisibleMesh(Character));
		FMeshSample VisibleSample = OwnerVisible;
		if (Mode.VisibleRole == TEXT("DriverBody"))
		{
			VisibleSample = Driver;
		}
		else if (Mode.VisibleRole == TEXT("WorldBody"))
		{
			VisibleSample = WorldVisible;
		}
		else if (Mode.VisibleRole == TEXT("LocalBody"))
		{
			VisibleSample = SampleMesh(LocalMesh);
		}

		Row->SetStringField(TEXT("RunId"), RunId);
		Row->SetStringField(TEXT("System"), TEXT("Modular"));
		Row->SetStringField(TEXT("Mode"), Mode.Name);
		Row->SetStringField(TEXT("Phase"), Phase.Name);
		Row->SetNumberField(TEXT("Sample"), SampleIdx);
		Row->SetNumberField(TEXT("Frame"), static_cast<double>(GFrameCounter));
		Row->SetNumberField(TEXT("WorldTime"), World->GetTimeSeconds());
		Row->SetNumberField(TEXT("PhaseTime"), PhaseElapsed);
		Row->SetNumberField(TEXT("DeltaTime"), World->GetDeltaSeconds());

		TSharedPtr<FJsonObject> Movement = MakeShared<FJsonObject>();
		Movement->SetNumberField(TEXT("ActorYaw"), ActorYaw);
		Movement->SetNumberField(TEXT("ControlYaw"), ControlYaw);
		Movement->SetNumberField(TEXT("ActorControlYawDelta"), ActorControlDelta);
		Movement->SetStringField(TEXT("ActorRotation"), RotToString(Character->GetActorRotation()));
		Movement->SetStringField(TEXT("ControlRotation"), RotToString(PC->GetControlRotation()));
		Movement->SetNumberField(TEXT("Speed2D"), Speed2D);
		Movement->SetStringField(TEXT("Acceleration"), CMC ? CMC->GetCurrentAcceleration().ToCompactString() : TEXT("none"));
		Movement->SetStringField(TEXT("Velocity"), CMC ? CMC->Velocity.ToCompactString() : TEXT("none"));
		Movement->SetBoolField(TEXT("Crouched"), Character->bIsCrouched);
		Movement->SetBoolField(TEXT("Falling"), CMC ? CMC->IsFalling() : false);
		Row->SetObjectField(TEXT("Movement"), Movement);

		TSharedPtr<FJsonObject> ABPState = MakeShared<FJsonObject>();
		ABPState->SetBoolField(TEXT("HasABP"), ABP != nullptr);
		ABPState->SetBoolField(TEXT("HasSpeed2D"), bHasABPSpeed);
		ABPState->SetBoolField(TEXT("HasTargetRotationDelta"), bHasTargetDelta);
		ABPState->SetBoolField(TEXT("HasTargetRotation"), bHasTargetRotation);
		ABPState->SetBoolField(TEXT("HasNoValidAnim"), bHasNoValidAnim);
		ABPState->SetBoolField(TEXT("HasMovementState"), bHasMovementState);
		ABPState->SetBoolField(TEXT("HasMovementDirection"), bHasMovementDirection);
		ABPState->SetBoolField(TEXT("HasTopLevelRotationMode"), bHasTopLevelRotationMode);
		ABPState->SetBoolField(TEXT("HasStateMachineState"), bHasStateMachineState);
		ABPState->SetBoolField(TEXT("HasEnableAO"), bHasEnableAO);
		ABPState->SetBoolField(TEXT("HasShouldTurnInPlace"), bHasShouldTurnInPlace);
		ABPState->SetBoolField(TEXT("HasAOValue"), bHasAOValue);
		ABPState->SetBoolField(TEXT("HasCurrentSelectedAnim"), bHasCurrentSelectedAnim);
		ABPState->SetBoolField(TEXT("HasTransitionHistory"), bHasTransitionHistory);
		ABPState->SetNumberField(TEXT("Speed2D"), ABPSpeed2D);
		ABPState->SetNumberField(TEXT("TargetRotationDelta"), TargetRotationDelta);
		ABPState->SetBoolField(TEXT("NoValidAnim"), bNoValidAnim);
		ABPState->SetNumberField(TEXT("MovementState"), MovementState);
		ABPState->SetNumberField(TEXT("MovementDirection"), MovementDirection);
		ABPState->SetNumberField(TEXT("RotationMode"), TopLevelRotationMode);
		ABPState->SetNumberField(TEXT("StateMachineState"), StateMachineState);
		ABPState->SetBoolField(TEXT("EnableAO"), bEnableAO);
		ABPState->SetBoolField(TEXT("ShouldTurnInPlace"), bShouldTurnInPlace);
		ABPState->SetStringField(TEXT("AOValue"), AOValue.ToCompactString());
		ABPState->SetNumberField(TEXT("AOYaw"), AOValue.X);
		ABPState->SetNumberField(TEXT("AOPitch"), AOValue.Y);
		ABPState->SetStringField(TEXT("TargetRotation"), RotToString(TargetRotation));
		ABPState->SetStringField(TEXT("CurrentSelectedAnim"), CurrentSelectedAnimName);
		ABPState->SetNumberField(TEXT("TransitionHistoryCount"), TransitionHistory.Num());
		if (TransitionHistory.Num() > 0)
		{
			const int32 TailStart = FMath::Max(0, TransitionHistory.Num() - 4);
			TArray<FString> TransitionTail;
			for (int32 Index = TailStart; Index < TransitionHistory.Num(); ++Index)
			{
				TransitionTail.Add(TransitionHistory[Index]);
			}
			ABPState->SetStringField(TEXT("TransitionHistoryTail"), FString::Join(TransitionTail, TEXT(" | ")));
		}
		else
		{
			ABPState->SetStringField(TEXT("TransitionHistoryTail"), TEXT(""));
		}
		Row->SetObjectField(TEXT("ABP"), ABPState);

		TSharedPtr<FJsonObject> CharacterProps = MakeShared<FJsonObject>();
		CharacterProps->SetBoolField(TEXT("HasRotationMode"), bHasRotationMode);
		CharacterProps->SetBoolField(TEXT("HasMovementMode"), bHasMovementMode);
		CharacterProps->SetBoolField(TEXT("HasGait"), bHasGait);
		CharacterProps->SetBoolField(TEXT("HasStance"), bHasStance);
		CharacterProps->SetBoolField(TEXT("HasOrientationIntent"), bHasOrientationIntent);
		CharacterProps->SetBoolField(TEXT("HasAimingRotation"), bHasAimingRotation);
		CharacterProps->SetNumberField(TEXT("RotationMode"), RotationMode);
		CharacterProps->SetNumberField(TEXT("MovementMode"), MovementMode);
		CharacterProps->SetNumberField(TEXT("Gait"), Gait);
		CharacterProps->SetNumberField(TEXT("Stance"), Stance);
		CharacterProps->SetStringField(TEXT("OrientationIntent"), RotToString(OrientationIntent));
		CharacterProps->SetStringField(TEXT("AimingRotation"), RotToString(AimingRotation));
		CharacterProps->SetBoolField(TEXT("HasWantsToStrafe"), bHasWantsToStrafe);
		CharacterProps->SetBoolField(TEXT("HasWantsToAim"), bHasWantsToAim);
		CharacterProps->SetBoolField(TEXT("HasWantsToCrouch"), bHasWantsToCrouch);
		CharacterProps->SetBoolField(TEXT("WantsToStrafe"), bWantsToStrafe);
		CharacterProps->SetBoolField(TEXT("WantsToAim"), bWantsToAim);
		CharacterProps->SetBoolField(TEXT("WantsToCrouch"), bWantsToCrouch);
		CharacterProps->SetNumberField(TEXT("ControlToOrientationIntentYawDelta"),
			bHasOrientationIntent ? FMath::Abs(NormalizeYawDelta(ControlYaw, OrientationIntent.Yaw)) : -1.f);
		CharacterProps->SetNumberField(TEXT("ControlToAimingYawDelta"),
			bHasAimingRotation ? FMath::Abs(NormalizeYawDelta(ControlYaw, AimingRotation.Yaw)) : -1.f);
		Row->SetObjectField(TEXT("CharacterProperties"), CharacterProps);

		TSharedPtr<FJsonObject> Camera = MakeShared<FJsonObject>();
		if (UCameraComponent* CameraComp = Character->FindComponentByClass<UCameraComponent>())
		{
			Camera->SetStringField(TEXT("WorldTransform"), TransformToCompactString(CameraComp->GetComponentTransform()));
			Camera->SetStringField(TEXT("RelativeTransform"), TransformToCompactString(CameraComp->GetRelativeTransform()));
			Camera->SetBoolField(TEXT("UsePawnControlRotation"), CameraComp->bUsePawnControlRotation);
			Camera->SetStringField(TEXT("Parent"),
				CameraComp->GetAttachParent() ? CameraComp->GetAttachParent()->GetName() : TEXT("none"));
		}
		Row->SetObjectField(TEXT("Camera"), Camera);

		Row->SetStringField(TEXT("VisibleMeshName"), VisibleSample.Name);
		Row->SetStringField(TEXT("VisibleMeshLeaderPose"), VisibleSample.LeaderPose);
		Row->SetStringField(TEXT("VisibleMeshIdentity"), Mode.VisibleRole);
		Row->SetObjectField(TEXT("DriverMesh"), MeshSampleToJson(Driver));
		Row->SetObjectField(TEXT("WorldVisibleMesh"), MeshSampleToJson(WorldVisible));
		Row->SetObjectField(TEXT("OwnerVisibleMesh"), MeshSampleToJson(VisibleSample));

		FString Line;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
		FJsonSerializer::Serialize(Row.ToSharedRef(), Writer);
		TimelineLines.Add(Line);

		if (CurrentSummaries.Num() > 0)
		{
			UpdateYawSummary(
				CurrentSummaries.Last(),
				ActorYaw,
				ControlYaw,
				bHasABPSpeed ? ABPSpeed2D : Speed2D,
				bHasTargetDelta ? TargetRotationDelta : 0.f,
				bHasTopLevelRotationMode ? TOptional<uint8>(TopLevelRotationMode) : TOptional<uint8>(),
				bHasOrientationIntent ? TOptional<FRotator>(OrientationIntent) : TOptional<FRotator>(),
				bHasAimingRotation ? TOptional<FRotator>(AimingRotation) : TOptional<FRotator>(),
				bHasEnableAO ? TOptional<bool>(bEnableAO) : TOptional<bool>(),
				bHasAOValue ? TOptional<float>(AOValue.X) : TOptional<float>(),
				bHasShouldTurnInPlace ? TOptional<bool>(bShouldTurnInPlace) : TOptional<bool>(),
				bHasTransitionHistory ? &TransitionHistory : nullptr,
				bHasCurrentSelectedAnim ? &CurrentSelectedAnimName : nullptr,
				Driver,
				VisibleSample,
				WorldVisible);

			CurrentSummaries.Last().bAnyWantsToStrafe =
				CurrentSummaries.Last().bAnyWantsToStrafe || (bHasWantsToStrafe && bWantsToStrafe);
			CurrentSummaries.Last().bAnyWantsToAim =
				CurrentSummaries.Last().bAnyWantsToAim || (bHasWantsToAim && bWantsToAim);
			CurrentSummaries.Last().bAnyWantsToCrouch =
				CurrentSummaries.Last().bAnyWantsToCrouch || (bHasWantsToCrouch && bWantsToCrouch);
		}
	}

	bool WriteModeSummary()
	{
		const FString TimelinePath = OutputDir / FString::Printf(TEXT("modular_clean_path_timeline_%s.jsonl"),
			*RunId);
		FFileHelper::SaveStringToFile(
			FString::Join(TimelineLines, TEXT("\n")),
			*TimelinePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test->AddInfo(FString::Printf(TEXT("[CleanPath] Timeline -> %s"), *TimelinePath));

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("RunId"), RunId);
		Root->SetStringField(TEXT("System"), TEXT("Modular"));

		TArray<TSharedPtr<FJsonValue>> PhaseArray;
		for (const FCleanPathPhaseSummary& Summary : CurrentSummaries)
		{
			PhaseArray.Add(MakeShared<FJsonValueObject>(YawSummaryToJson(Summary)));
		}
		Root->SetArrayField(TEXT("Phases"), PhaseArray);

		FString SummaryText;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SummaryText);
		FJsonSerializer::Serialize(Root, Writer);

		const FString SummaryPath = OutputDir / FString::Printf(TEXT("modular_clean_path_summary_%s.json"),
			*RunId);
		FFileHelper::SaveStringToFile(
			SummaryText,
			*SummaryPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test->AddInfo(FString::Printf(TEXT("[CleanPath] Summary -> %s"), *SummaryPath));

		FString PreviousMode;
		for (const FCleanPathPhaseSummary& Summary : CurrentSummaries)
		{
			FString ModeName = Summary.PhaseName;
			FString PhaseName = Summary.PhaseName;
			Summary.PhaseName.Split(TEXT("/"), &ModeName, &PhaseName);
			if (!PreviousMode.IsEmpty() && ModeName != PreviousMode)
			{
				Test->AddInfo(FString::Printf(TEXT("[CleanPath] --- %s complete ---"), *PreviousMode));
			}
			PreviousMode = ModeName;

			if (Summary.bExpectYawResponse && (!Summary.bBridgeTracksControlYaw || !Summary.bAnyWantsToStrafe || !Summary.bAnyWantsToAim))
			{
				Test->AddError(FString::Printf(
					TEXT("[CleanPath] %s failed at Layer1_MMContract: yaw contract broke (Bridge=%s Strafe=%s Aim=%s)"),
					*Summary.PhaseName,
					Summary.bBridgeTracksControlYaw ? TEXT("Y") : TEXT("N"),
					Summary.bAnyWantsToStrafe ? TEXT("Y") : TEXT("N"),
					Summary.bAnyWantsToAim ? TEXT("Y") : TEXT("N")));
				break;
			}

			if (ModeName == TEXT("ModeA_DriverOnly"))
			{
				if (Summary.bExpectYawResponse && !Summary.bVisibleRespondedToYaw && !Summary.bBodyRespondedToYaw)
				{
					Test->AddError(FString::Printf(
						TEXT("[CleanPath] %s failed at Layer2_RawPose: driver-visible mode never changed bones"),
						*Summary.PhaseName));
					break;
				}
			}
			else if (ModeName == TEXT("ModeB_DriverWorld"))
			{
				if (Summary.bExpectYawResponse && !Summary.bVisibleRespondedToYaw && !Summary.bBodyRespondedToYaw)
				{
					Test->AddError(FString::Printf(
						TEXT("[CleanPath] %s failed at Layer3_RetargetPropagation: driver changed but WorldBody did not"),
						*Summary.PhaseName));
					break;
				}
			}
			else
			{
				if (Summary.bExpectYawResponse && ((!Summary.bVisibleRespondedToYaw && !Summary.bBodyRespondedToYaw) || !Summary.bVisibleTracksWorld))
				{
					Test->AddError(FString::Printf(
						TEXT("[CleanPath] %s failed at Layer4_LocalCustomizationPropagation: world changed but owner-visible layer diverged"),
						*Summary.PhaseName));
					break;
				}
			}

			if (Summary.bExpectTurnState && !Summary.bTurnStateObserved)
			{
				Test->AddError(FString::Printf(
					TEXT("[CleanPath] %s failed at Layer1_MMContract: large idle yaw never entered a turn state"),
					*Summary.PhaseName));
				break;
			}
		}

		NextStage();
		return false;
	}

	void SetMeshVisibility(USkeletalMeshComponent* Mesh, const bool bHidden, const bool bOnlyOwnerSee, const bool bOwnerNoSee) const
	{
		if (!Mesh)
		{
			return;
		}

		Mesh->SetHiddenInGame(bHidden);
		Mesh->SetOnlyOwnerSee(bOnlyOwnerSee);
		Mesh->SetOwnerNoSee(bOwnerNoSee);
	}

	void CaptureOriginalStates()
	{
		OriginalDriverState = CaptureRestoreState(DriverMesh);
		OriginalWorldState = CaptureRestoreState(WorldMesh);
		OriginalLocalState = CaptureRestoreState(LocalMesh);
		OriginalHeadState = CaptureRestoreState(HeadMesh);
		OriginalBodyCustomizationState = CaptureRestoreState(BodyCustomizationMesh);
		OriginalHeadCustomizationState = CaptureRestoreState(HeadCustomizationMesh);
		OriginalLocalBodyCustomizationState = CaptureRestoreState(LocalBodyCustomizationMesh);
	}

	void RestoreOriginalStates()
	{
		RestoreMeshState(OriginalDriverState);
		RestoreMeshState(OriginalWorldState);
		RestoreMeshState(OriginalLocalState);
		RestoreMeshState(OriginalHeadState);
		RestoreMeshState(OriginalBodyCustomizationState);
		RestoreMeshState(OriginalHeadCustomizationState);
		RestoreMeshState(OriginalLocalBodyCustomizationState);
	}

	void ApplyIsolationMode(const FCleanPathMode& Mode)
	{
		if (!DriverMesh || !DriverMesh->GetSkeletalMeshAsset())
		{
			return;
		}

		USkeletalMesh* DriverAsset = DriverMesh->GetSkeletalMeshAsset();

		SetMeshVisibility(DriverMesh, true, false, true);
		SetMeshVisibility(WorldMesh, true, false, true);
		SetMeshVisibility(LocalMesh, true, true, false);
		SetMeshVisibility(HeadMesh, true, false, true);
		SetMeshVisibility(BodyCustomizationMesh, true, false, true);
		SetMeshVisibility(HeadCustomizationMesh, true, false, true);
		SetMeshVisibility(LocalBodyCustomizationMesh, true, true, false);

		if (Mode.Name == TEXT("ModeA_DriverOnly"))
		{
			SetMeshVisibility(DriverMesh, false, false, false);
			return;
		}

		if (WorldMesh)
		{
			WorldMesh->SetSkeletalMeshAsset(DriverAsset);
			WorldMesh->SetAnimInstanceClass(OriginalWorldState.AnimClass.Get());
			WorldMesh->SetLeaderPoseComponent(nullptr);
			WorldMesh->AddTickPrerequisiteComponent(DriverMesh);
			EnsureBlueprintAnimReady(WorldMesh);
			SetMeshVisibility(WorldMesh, false, false, Mode.Name != TEXT("ModeB_DriverWorld"));
		}

		if (Mode.Name == TEXT("ModeB_DriverWorld"))
		{
			return;
		}

		if (LocalMesh)
		{
			LocalMesh->SetSkeletalMeshAsset(DriverAsset);
			LocalMesh->SetLeaderPoseComponent(nullptr);
			UClass* LocalAnimClass = OriginalLocalBodyCustomizationState.AnimClass.Get();
			if (!LocalAnimClass)
			{
				LocalAnimClass = OriginalLocalState.AnimClass.Get();
			}
			LocalMesh->SetAnimInstanceClass(LocalAnimClass);
			if (WorldMesh)
			{
				LocalMesh->AddTickPrerequisiteComponent(WorldMesh);
			}
			EnsureBlueprintAnimReady(LocalMesh);
			SetMeshVisibility(LocalMesh, false, true, false);
		}

		if (Mode.Name == TEXT("ModeC_DriverWorldLocal"))
		{
			return;
		}

		RestoreOriginalStates();
	}

	APlayerController* FindPC() const
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* CandidateWorld = Context.World())
			{
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(CandidateWorld, 0))
				{
					if (PC->GetPawn())
					{
						return PC;
					}
				}
			}
		}

		return nullptr;
	}

	UAnimInstance* FindPrimaryABP(ACharacter* Character) const
	{
		if (!Character)
		{
			return nullptr;
		}

		if (USkeletalMeshComponent* Driver = FindDriverMesh(Character))
		{
			if (UAnimInstance* Anim = Driver->GetAnimInstance())
			{
				return Anim;
			}
		}

		return Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	}

	void NextStage()
	{
		++Stage;
		Tick = 0;
	}

	void ResetPhaseRunner()
	{
		PhaseIdx = 0;
		bPhaseStarted = false;
		PhaseElapsed = 0.f;
		SampleAccum = 0.f;
		PhaseSampleIdx = 0;
		BaseControlYaw = 0.f;
	}

private:
	FAutomationTestBase* Test = nullptr;
	UWorld* World = nullptr;
	const FString RunId;
	const FString OutputDir;

	int32 Stage = 0;
	int32 Tick = 0;
	uint64 LastFrame = 0;
	int32 ModeIdx = 0;
	bool bModeConfigured = false;
	int32 ModeSettleTicks = 0;
	FString CurrentModeLabel;

	int32 PhaseIdx = 0;
	bool bPhaseStarted = false;
	float PhaseElapsed = 0.f;
	float SampleAccum = 0.f;
	int32 PhaseSampleIdx = 0;
	float BaseControlYaw = 0.f;
	USkeletalMeshComponent* DriverMesh = nullptr;
	USkeletalMeshComponent* WorldMesh = nullptr;
	USkeletalMeshComponent* LocalMesh = nullptr;
	USkeletalMeshComponent* HeadMesh = nullptr;
	USkeletalMeshComponent* BodyCustomizationMesh = nullptr;
	USkeletalMeshComponent* HeadCustomizationMesh = nullptr;
	USkeletalMeshComponent* LocalBodyCustomizationMesh = nullptr;
	FMeshRestoreState OriginalDriverState;
	FMeshRestoreState OriginalWorldState;
	FMeshRestoreState OriginalLocalState;
	FMeshRestoreState OriginalHeadState;
	FMeshRestoreState OriginalBodyCustomizationState;
	FMeshRestoreState OriginalHeadCustomizationState;
	FMeshRestoreState OriginalLocalBodyCustomizationState;

	TArray<FString> TimelineLines;
	TArray<FCleanPathPhaseSummary> CurrentSummaries;
};

} // namespace CleanPathHelpers

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCharacterCleanPathIsolationTest,
	"ProjectIntegrationTests.Character.Parity.CleanPathIsolationMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCharacterCleanPathIsolationTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(CleanPathHelpers::FCleanPathIsolationMatrixCommand(this));
	return true;
}
