// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Data/ObjectCapabilityPropertyResolver.h"

#include "CapabilityRegistry.h"
#include "Components/ActorComponent.h"
#include "Data/ObjectDefinition.h"
#include "GameplayTagContainer.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	FProperty* ResolveProperty(UClass* Class, const FName PropertyName)
	{
		if (!Class)
		{
			return nullptr;
		}

		if (FProperty* Property = Class->FindPropertyByName(PropertyName))
		{
			return Property;
		}

		const FName BooleanName(*FString::Printf(
			TEXT("b%s"),
			*PropertyName.ToString()));
		FProperty* BooleanProperty = Class->FindPropertyByName(BooleanName);
		return CastField<FBoolProperty>(BooleanProperty) ? BooleanProperty : nullptr;
	}

	bool ImportValue(
		UObject* Object,
		FProperty* Property,
		void* PropertyAddress,
		const FString& Value)
	{
		if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			*TextProperty->GetPropertyValuePtr(PropertyAddress) = FText::FromString(Value);
			return true;
		}

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == FGameplayTag::StaticStruct())
			{
				FGameplayTag* Tag = static_cast<FGameplayTag*>(PropertyAddress);
				*Tag = FGameplayTag::RequestGameplayTag(FName(*Value), false);
				return Tag->IsValid() || Value.IsEmpty();
			}

			if (StructProperty->Struct == FGameplayTagContainer::StaticStruct())
			{
				FGameplayTagContainer* Container =
					static_cast<FGameplayTagContainer*>(PropertyAddress);
				Container->Reset();
				TArray<FString> TagStrings;
				Value.ParseIntoArray(TagStrings, TEXT(","));
				for (const FString& TagString : TagStrings)
				{
					const FString Trimmed = TagString.TrimStartAndEnd();
					if (Trimmed.IsEmpty())
					{
						continue;
					}
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(
						FName(*Trimmed), false);
					if (!Tag.IsValid())
					{
						return false;
					}
					Container->AddTag(Tag);
				}
				return true;
			}

			if (StructProperty->Struct == TBaseStructure<FVector>::Get())
			{
				return static_cast<FVector*>(PropertyAddress)->InitFromString(Value);
			}

			if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				return static_cast<FRotator*>(PropertyAddress)->InitFromString(Value);
			}

			if (StructProperty->Struct == TBaseStructure<FIntPoint>::Get())
			{
				FString CleanValue = Value;
				CleanValue.RemoveFromStart(TEXT("("));
				CleanValue.RemoveFromEnd(TEXT(")"));
				TArray<FString> Components;
				CleanValue.ParseIntoArray(Components, TEXT(","));
				if (Components.Num() < 2)
				{
					return false;
				}
				FIntPoint* Point = static_cast<FIntPoint*>(PropertyAddress);
				Point->X = FCString::Atoi(*Components[0].TrimStartAndEnd());
				Point->Y = FCString::Atoi(*Components[1].TrimStartAndEnd());
				return true;
			}
		}

		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			if (CastField<FSoftObjectProperty>(ArrayProperty->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyAddress);
				ArrayHelper.EmptyValues();
				TArray<FString> Paths;
				Value.ParseIntoArray(Paths, TEXT(";"));
				for (const FString& Path : Paths)
				{
					const FString Trimmed = Path.TrimStartAndEnd();
					if (Trimmed.IsEmpty())
					{
						continue;
					}
					const int32 Index = ArrayHelper.AddValue();
					FSoftObjectPtr* SoftPointer = reinterpret_cast<FSoftObjectPtr*>(
						ArrayHelper.GetRawPtr(Index));
					*SoftPointer = FSoftObjectPath(Trimmed);
				}
				return true;
			}
		}

		if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			FStructProperty* KeyProperty =
				CastField<FStructProperty>(MapProperty->KeyProp);
			FFloatProperty* ValueProperty =
				CastField<FFloatProperty>(MapProperty->ValueProp);
			if (KeyProperty
				&& KeyProperty->Struct == FGameplayTag::StaticStruct()
				&& ValueProperty)
			{
				FScriptMapHelper MapHelper(MapProperty, PropertyAddress);
				MapHelper.EmptyValues();
				TArray<FString> Pairs;
				Value.ParseIntoArray(Pairs, TEXT(";"));
				for (const FString& Pair : Pairs)
				{
					FString TagString;
					FString NumberString;
					if (!Pair.TrimStartAndEnd().Split(
						TEXT("="), &TagString, &NumberString))
					{
						return false;
					}
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(
						FName(*TagString.TrimStartAndEnd()), false);
					if (!Tag.IsValid())
					{
						return false;
					}
					const int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
					*reinterpret_cast<FGameplayTag*>(MapHelper.GetKeyPtr(Index)) = Tag;
					*reinterpret_cast<float*>(MapHelper.GetValuePtr(Index)) =
						FCString::Atof(*NumberString.TrimStartAndEnd());
				}
				MapHelper.Rehash();
				return true;
			}
		}

		return Property->ImportText_Direct(
			*Value, PropertyAddress, Object, PPF_None) != nullptr;
	}

	bool ResolveCapability(
		const FObjectCapabilityEntry& Entry,
		UClass*& OutClass,
		FString& OutError)
	{
		FString ReadinessError;
		if (!FCapabilityRegistry::IsReady(&ReadinessError))
		{
			OutError = FString::Printf(
				TEXT("Capability providers are not ready: %s"),
				*ReadinessError);
			return false;
		}

		OutClass = FCapabilityRegistry::GetCapabilityClass(Entry.Type);
		if (!OutClass)
		{
			OutError = FString::Printf(
				TEXT("Unknown capability '%s'"),
				*Entry.Type.ToString());
			return false;
		}
		return true;
	}

	bool TryAddSoftPath(
		const FString& Value,
		TSet<FTopLevelAssetPath>& References,
		FString& OutError)
	{
		const FString Trimmed = Value.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return true;
		}

		const FSoftObjectPath SoftPath(Trimmed);
		const FTopLevelAssetPath AssetPath = SoftPath.GetAssetPath();
		const FString PackageName = AssetPath.GetPackageName().ToString();
		if (!SoftPath.IsValid()
			|| !AssetPath.IsValid()
			|| PackageName.StartsWith(TEXT("/Script/"))
			|| !FPackageName::IsValidLongPackageName(PackageName, true))
		{
			OutError = FString::Printf(TEXT("Invalid content asset path '%s'"), *Trimmed);
			return false;
		}

		const FName MountName = FPackageName::GetPackageMountPoint(PackageName);
		const FString MountRoot = FString::Printf(TEXT("/%s/"), *MountName.ToString());
		if (MountName.IsNone() || !FPackageName::MountPointExists(MountRoot))
		{
			OutError = FString::Printf(TEXT("Unmounted content asset path '%s'"), *Trimmed);
			return false;
		}

		References.Add(AssetPath);
		return true;
	}
}

namespace ProjectObjectCapabilityProperties
{

bool ApplyProperty(
	UObject* Object,
	const FName PropertyName,
	const FString& Value,
	FString* OutError)
{
	if (!Object)
	{
		SetError(OutError, TEXT("Capability target object is null"));
		return false;
	}

	FProperty* Property = ResolveProperty(Object->GetClass(), PropertyName);
	if (!Property)
	{
		SetError(OutError, FString::Printf(
			TEXT("Unknown property '%s' on '%s'"),
			*PropertyName.ToString(),
			*Object->GetClass()->GetName()));
		return false;
	}

	void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(Object);
	if (!ImportValue(Object, Property, PropertyAddress, Value))
	{
		SetError(OutError, FString::Printf(
			TEXT("Invalid value '%s' for property '%s' on '%s'"),
			*Value,
			*PropertyName.ToString(),
			*Object->GetClass()->GetName()));
		return false;
	}

	SetError(OutError, FString());
	return true;
}

bool ApplyEntryProperties(
	UObject* Object,
	const FObjectCapabilityEntry& Entry,
	FString& OutError)
{
	for (const TPair<FName, FString>& Property : Entry.Properties)
	{
		if (!ApplyProperty(Object, Property.Key, Property.Value, &OutError))
		{
			return false;
		}
	}
	OutError.Reset();
	return true;
}

bool ValidateEntry(const FObjectCapabilityEntry& Entry, FString& OutError)
{
	UClass* CapabilityClass = nullptr;
	if (!ResolveCapability(Entry, CapabilityClass, OutError))
	{
		return false;
	}

	UActorComponent* Probe = NewObject<UActorComponent>(
		GetTransientPackage(), CapabilityClass, NAME_None, RF_Transient);
	if (!Probe)
	{
		OutError = FString::Printf(
			TEXT("Failed to create validation instance for capability '%s'"),
			*Entry.Type.ToString());
		return false;
	}
	if (!ApplyEntryProperties(Probe, Entry, OutError))
	{
		return false;
	}

	TArray<FTopLevelAssetPath> SoftReferences;
	return CollectSoftReferences(Entry, SoftReferences, OutError);
}

bool ValidateDefinition(const UObjectDefinition* Definition, FString& OutError)
{
	if (!Definition)
	{
		OutError = TEXT("ObjectDefinition is null");
		return false;
	}
	for (const FObjectCapabilityEntry& Entry : Definition->Capabilities)
	{
		if (!ValidateEntry(Entry, OutError))
		{
			return false;
		}
	}
	OutError.Reset();
	return true;
}

bool CollectSoftReferences(
	const FObjectCapabilityEntry& Entry,
	TArray<FTopLevelAssetPath>& OutReferences,
	FString& OutError)
{
	UClass* CapabilityClass = nullptr;
	if (!ResolveCapability(Entry, CapabilityClass, OutError))
	{
		return false;
	}

	TSet<FTopLevelAssetPath> References;
	for (const FTopLevelAssetPath& ExistingReference : OutReferences)
	{
		References.Add(ExistingReference);
	}
	for (const TPair<FName, FString>& Pair : Entry.Properties)
	{
		FProperty* Property = ResolveProperty(CapabilityClass, Pair.Key);
		if (!Property)
		{
			OutError = FString::Printf(
				TEXT("Unknown property '%s' on capability '%s'"),
				*Pair.Key.ToString(),
				*Entry.Type.ToString());
			return false;
		}

		if (CastField<FSoftObjectProperty>(Property))
		{
			if (!TryAddSoftPath(Pair.Value, References, OutError))
			{
				return false;
			}
			continue;
		}

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			if (!CastField<FSoftObjectProperty>(ArrayProperty->Inner))
			{
				continue;
			}
			TArray<FString> Values;
			Pair.Value.ParseIntoArray(Values, TEXT(";"));
			for (const FString& Value : Values)
			{
				if (!TryAddSoftPath(Value, References, OutError))
				{
					return false;
				}
			}
		}
	}

	OutReferences = References.Array();
	OutReferences.Sort([](const FTopLevelAssetPath& Left, const FTopLevelAssetPath& Right)
	{
		return Left.ToString() < Right.ToString();
	});
	OutError.Reset();
	return true;
}

}
