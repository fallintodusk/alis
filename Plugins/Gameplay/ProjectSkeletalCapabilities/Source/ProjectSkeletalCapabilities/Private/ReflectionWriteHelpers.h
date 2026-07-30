// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
// Shared reflection helpers for writing to UE property fields.
// Used by MotionMatchingBridgeAnimInstance and MotionMatchingCapability.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

namespace SkeletalCapabilities
{

inline FProperty* FindPropWithFallback(const UStruct* Owner, const TCHAR* CleanName)
{
	if (FProperty* Direct = Owner->FindPropertyByName(FName(CleanName)))
	{
		return Direct;
	}
	const FString Prefix = FString(CleanName) + TEXT("_");
	for (TFieldIterator<FProperty> It(Owner); It; ++It)
	{
		if (It->GetName().StartsWith(Prefix))
		{
			return *It;
		}
	}
	return nullptr;
}

inline void WriteByteField(FProperty* Prop, uint8* StructBase, uint8 Value)
{
	if (!Prop) return;
	uint8* Dest = Prop->ContainerPtrToValuePtr<uint8>(StructBase);
	if (CastField<FByteProperty>(Prop))
	{
		*Dest = Value;
	}
	else if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
	{
		EP->GetUnderlyingProperty()->SetIntPropertyValue(Dest, static_cast<int64>(Value));
	}
}

inline void WriteBoolField(FProperty* Prop, uint8* StructBase, bool Value)
{
	if (!Prop) return;
	if (FBoolProperty* BP = CastField<FBoolProperty>(Prop))
	{
		BP->SetPropertyValue(Prop->ContainerPtrToValuePtr<void>(StructBase), Value);
	}
}

inline void WriteFloatField(FProperty* Prop, uint8* StructBase, double Value)
{
	if (!Prop) return;
	void* Dest = Prop->ContainerPtrToValuePtr<void>(StructBase);
	if (CastField<FDoubleProperty>(Prop)) *static_cast<double*>(Dest) = Value;
	else if (CastField<FFloatProperty>(Prop)) *static_cast<float*>(Dest) = static_cast<float>(Value);
}

inline void WriteVectorField(FProperty* Prop, uint8* StructBase, const FVector& Value)
{
	if (!Prop) return;
	if (FStructProperty* SP = CastField<FStructProperty>(Prop))
	{
		void* Dest = Prop->ContainerPtrToValuePtr<void>(StructBase);
		if (SP->Struct == TBaseStructure<FVector>::Get())
			*static_cast<FVector*>(Dest) = Value;
	}
}

inline void WriteRotatorField(FProperty* Prop, uint8* StructBase, const FRotator& Value)
{
	if (!Prop) return;
	if (FStructProperty* SP = CastField<FStructProperty>(Prop))
	{
		void* Dest = Prop->ContainerPtrToValuePtr<void>(StructBase);
		if (SP->Struct == TBaseStructure<FRotator>::Get())
			*static_cast<FRotator*>(Dest) = Value;
	}
}

inline void WriteTransformField(FProperty* Prop, uint8* StructBase, const FTransform& Value)
{
	if (!Prop) return;
	if (FStructProperty* SP = CastField<FStructProperty>(Prop))
	{
		void* Dest = Prop->ContainerPtrToValuePtr<void>(StructBase);
		if (SP->Struct == TBaseStructure<FTransform>::Get())
			*static_cast<FTransform*>(Dest) = Value;
	}
}

} // namespace SkeletalCapabilities
