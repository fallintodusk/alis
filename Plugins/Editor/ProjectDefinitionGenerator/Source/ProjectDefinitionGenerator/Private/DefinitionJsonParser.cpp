// Copyright ALIS. All Rights Reserved.

#include "DefinitionJsonParser.h"
#include "Data/ObjectDefinition.h"

#include "Dom/JsonObject.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"
#include "JsonObjectConverter.h"
#include "Misc/Paths.h"
#include "InstancedStruct.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionJsonParser, Log, All);

// Custom import callback for FJsonObjectConverter
// Handles: FVector/FRotator string parsing, TSoftObjectPtr/TSoftClassPtr path normalization
namespace
{
	bool TryNormalizeSpawnClassPath(const FString& RawPath, FString& OutNormalizedPath, FString& OutReason)
	{
		OutNormalizedPath.Reset();
		OutReason.Reset();

		if (RawPath.IsEmpty())
		{
			OutReason = TEXT("Path is empty");
			return false;
		}

		// Native class path contract: /Script/Module.ClassName
		if (RawPath.StartsWith(TEXT("/Script/")))
		{
			const int32 DotIndex = RawPath.Find(TEXT("."));
			if (DotIndex == INDEX_NONE || DotIndex <= 8 || DotIndex >= RawPath.Len() - 1)
			{
				OutReason = TEXT("Native class path must match /Script/Module.ClassName");
				return false;
			}

			OutNormalizedPath = RawPath;
			return true;
		}

		// Blueprint asset path contract: /Mount/Path/BP_Name
		// Also accept explicit BP class object path: /Mount/Path/BP_Name.BP_Name_C.
		if (!RawPath.StartsWith(TEXT("/")))
		{
			OutReason = TEXT("Path must start with '/'");
			return false;
		}

		int32 LastSlash = INDEX_NONE;
		int32 LastDot = INDEX_NONE;
		RawPath.FindLastChar(TEXT('/'), LastSlash);
		RawPath.FindLastChar(TEXT('.'), LastDot);

		if (LastSlash == INDEX_NONE || LastSlash >= RawPath.Len() - 1)
		{
			OutReason = TEXT("Invalid path format");
			return false;
		}

		// No object suffix -> treat as blueprint asset path and normalize to _C class object.
		if (LastDot == INDEX_NONE || LastDot < LastSlash)
		{
			OutNormalizedPath = FDefinitionJsonParser::NormalizeClassPath(RawPath);
			return true;
		}

		// Explicit object path must already be a class object (_C).
		const FString ObjectName = RawPath.Mid(LastDot + 1);
		if (!ObjectName.EndsWith(TEXT("_C")))
		{
			OutReason = TEXT("Blueprint object path must end with _C, or use asset path form");
			return false;
		}

		OutNormalizedPath = RawPath;
		return true;
	}

	bool CustomJsonImportCallback(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* OutValue)
	{
		if (!JsonValue.IsValid() || !Property)
		{
			return false;
		}

		// -------------------------------------------------------------------------
		// Universal: TMap<FGameplayTag, float/double> from JSON object
		// "magnitudes": { "SetByCaller.Hydration": -30.0, ... }
		// -------------------------------------------------------------------------
		if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			const FStructProperty* KeyStructProp = CastField<FStructProperty>(MapProp->KeyProp);
			const FFloatProperty* ValFloatProp = CastField<FFloatProperty>(MapProp->ValueProp);
			const FDoubleProperty* ValDoubleProp = CastField<FDoubleProperty>(MapProp->ValueProp);

			const bool bKeyIsGameplayTag = KeyStructProp && (KeyStructProp->Struct == FGameplayTag::StaticStruct());
			const bool bValIsNumber = (ValFloatProp != nullptr) || (ValDoubleProp != nullptr);

			if (bKeyIsGameplayTag && bValIsNumber && JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (!JsonValue->TryGetObject(Obj) || !Obj || !Obj->IsValid())
				{
					return false;
				}

				FScriptMapHelper MapHelper(MapProp, OutValue);
				MapHelper.EmptyValues();

				for (const auto& Pair : (*Obj)->Values)
				{
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Pair.Key), false);
					if (!Tag.IsValid())
					{
						UE_LOG(LogDefinitionJsonParser, Warning, TEXT("Invalid GameplayTag map key: %s"), *Pair.Key);
						continue;
					}

					double Num = 0.0;
					if (Pair.Value.IsValid())
					{
						if (Pair.Value->Type == EJson::Number)
						{
							Num = Pair.Value->AsNumber();
						}
						else if (Pair.Value->Type == EJson::String)
						{
							Num = FCString::Atod(*Pair.Value->AsString());
						}
						else
						{
							UE_LOG(LogDefinitionJsonParser, Warning, TEXT("Non-numeric value for tag '%s'"), *Pair.Key);
							continue;
						}
					}

					const int32 NewIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
					*reinterpret_cast<FGameplayTag*>(MapHelper.GetKeyPtr(NewIndex)) = Tag;

					if (ValFloatProp)
					{
						*reinterpret_cast<float*>(MapHelper.GetValuePtr(NewIndex)) = static_cast<float>(Num);
					}
					else
					{
						*reinterpret_cast<double*>(MapHelper.GetValuePtr(NewIndex)) = Num;
					}
				}

				MapHelper.Rehash();
				return true;
			}
		}

		// Handle FGameplayTag from string "Tag.Name"
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct == FGameplayTag::StaticStruct() && JsonValue->Type == EJson::String)
			{
				const FString TagStr = JsonValue->AsString();
				if (!TagStr.IsEmpty())
				{
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
					if (Tag.IsValid())
					{
						*static_cast<FGameplayTag*>(OutValue) = Tag;
						return true;
					}
					UE_LOG(LogDefinitionJsonParser, Warning, TEXT("Invalid GameplayTag: %s"), *TagStr);
				}
				return false;
			}

			// Handle FVector from string "(X=val Y=val Z=val)"
			if (StructProp->Struct == TBaseStructure<FVector>::Get() && JsonValue->Type == EJson::String)
			{
				FString StrValue = JsonValue->AsString();
				if (!StrValue.IsEmpty())
				{
					FVector* VecPtr = static_cast<FVector*>(OutValue);
					if (VecPtr->InitFromString(StrValue))
					{
						return true;
					}
					UE_LOG(LogDefinitionJsonParser, Warning, TEXT("Failed to parse FVector from: %s"), *StrValue);
				}
				return false;
			}

			// Handle FRotator from string "(P=val Y=val R=val)"
			if (StructProp->Struct == TBaseStructure<FRotator>::Get() && JsonValue->Type == EJson::String)
			{
				FString StrValue = JsonValue->AsString();
				if (!StrValue.IsEmpty())
				{
					FRotator* RotPtr = static_cast<FRotator*>(OutValue);
					if (RotPtr->InitFromString(StrValue))
					{
						return true;
					}
					UE_LOG(LogDefinitionJsonParser, Warning, TEXT("Failed to parse FRotator from: %s"), *StrValue);
				}
				return false;
			}

			// Handle FIntPoint from string "X,Y" (e.g., "1,2" for grid size)
			if (StructProp->Struct == TBaseStructure<FIntPoint>::Get() && JsonValue->Type == EJson::String)
			{
				FString StrValue = JsonValue->AsString();
				if (!StrValue.IsEmpty())
				{
					TArray<FString> Parts;
					StrValue.ParseIntoArray(Parts, TEXT(","));
					if (Parts.Num() == 2)
					{
						FIntPoint* PointPtr = static_cast<FIntPoint*>(OutValue);
						PointPtr->X = FCString::Atoi(*Parts[0].TrimStartAndEnd());
						PointPtr->Y = FCString::Atoi(*Parts[1].TrimStartAndEnd());
						return true;
					}
					UE_LOG(LogDefinitionJsonParser, Warning, TEXT("Failed to parse FIntPoint from: %s (expected X,Y format)"), *StrValue);
				}
				return false;
			}
		}

		// Handle TSoftObjectPtr - normalize asset path (add .AssetName suffix)
		if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
		{
			FString Path = JsonValue->AsString();
			if (!Path.IsEmpty())
			{
				Path = FDefinitionJsonParser::NormalizeAssetPath(Path);
				FSoftObjectPtr SoftPtr{FSoftObjectPath{Path}};
				SoftObjProp->SetPropertyValue(OutValue, SoftPtr);
				return true;
			}
			return false;
		}

		// Handle TSoftClassPtr - normalize class path (add .AssetName_C suffix)
		if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
		{
			FString Path = JsonValue->AsString();
			if (!Path.IsEmpty())
			{
				Path = FDefinitionJsonParser::NormalizeClassPath(Path);
				FSoftObjectPtr SoftPtr{FSoftObjectPath{Path}};
				SoftClassProp->SetPropertyValue(OutValue, SoftPtr);
				return true;
			}
			return false;
		}

		return false; // Fall back to default converter
	}
}

bool FDefinitionJsonParser::ParseJsonToAsset(
	const FDefinitionTypeInfo& TypeInfo,
	const TSharedPtr<FJsonObject>& JsonObject,
	UObject* Asset,
	FString& OutError)
{
	if (!Asset)
	{
		OutError = TEXT("Null asset");
		return false;
	}

	// -------------------------------------------------------------------------
	// AUTO-PARSE MODE (for non-ObjectDefinition types)
	// -------------------------------------------------------------------------
	// ObjectDefinition: handled entirely in post-parse block below
	// Other types: auto-parse using FJsonObjectConverter
	// -------------------------------------------------------------------------

	const bool bIsObjectDefinition = TypeInfo.DefinitionClass->IsChildOf(UObjectDefinition::StaticClass());

	if (!bIsObjectDefinition)
	{
		FJsonObjectConverter::CustomImportCallback Callback =
			FJsonObjectConverter::CustomImportCallback::CreateStatic(&CustomJsonImportCallback);

		if (!FJsonObjectConverter::JsonObjectToUStruct(
			JsonObject.ToSharedRef(),
			TypeInfo.DefinitionClass,
			Asset,
			0, 0, false, nullptr, &Callback))
		{
			OutError = TEXT("Auto-parse failed: JsonObjectToUStruct returned false");
			return false;
		}

		UE_LOG(LogDefinitionJsonParser, Verbose, TEXT("Auto-parsed %s using JsonObjectToUStruct"), *Asset->GetName());
	}

	// -------------------------------------------------------------------------
	// OBJECTDEFINITION POST-PARSE
	// -------------------------------------------------------------------------
	// ObjectDefinition requires explicit parsing for:
	// - ObjectId: JSON "id" -> ObjectId property (name mismatch)
	// - Meshes: complex array with transform strings, materials
	// - Capabilities: TArray<FName>, TMap<FName,FString>, derived bUsePhysicsMode
	// - Sections: TMap<FName, FInstancedStruct> requires explicit struct init
	// -------------------------------------------------------------------------
	if (UObjectDefinition* ObjDef = Cast<UObjectDefinition>(Asset))
	{
		// Reusable callback for auto-parsing nested structs
		FJsonObjectConverter::CustomImportCallback Callback =
			FJsonObjectConverter::CustomImportCallback::CreateStatic(&CustomJsonImportCallback);

		// Reset optional spawn settings to avoid stale values after regeneration.
		ObjDef->SpawnClass.Reset();
		ObjDef->AttachToComponentTag = NAME_None;
		ObjDef->ActorTags.Reset();

		// ---------------------------------------------------------------------
		// 1. OBJECTID (required)
		// ---------------------------------------------------------------------
		if (JsonObject->HasField(TEXT("id")))
		{
			ObjDef->ObjectId = FName(*JsonObject->GetStringField(TEXT("id")));
		}
		else
		{
			UE_LOG(LogDefinitionJsonParser, Error, TEXT("[%s] Missing required field 'id'"), *Asset->GetName());
			OutError = TEXT("Missing required field: id");
			return false;
		}

		// ---------------------------------------------------------------------
		// 1.1 OPTIONAL SPAWN SETTINGS
		// ---------------------------------------------------------------------
		// spawnClass: optional class path override for spawned actor type
		if (JsonObject->HasField(TEXT("spawnClass")))
		{
			const FString RawSpawnClass = JsonObject->GetStringField(TEXT("spawnClass"));
			if (!RawSpawnClass.IsEmpty())
			{
				FString NormalizedClassPath;
				FString NormalizeError;
				if (TryNormalizeSpawnClassPath(RawSpawnClass, NormalizedClassPath, NormalizeError))
				{
					ObjDef->SpawnClass = TSoftClassPtr<AActor>(FSoftObjectPath(NormalizedClassPath));
				}
				else
				{
					UE_LOG(LogDefinitionJsonParser, Error,
						TEXT("[%s] Invalid spawnClass '%s': %s. Field ignored, fallback spawn class will be used."),
						*Asset->GetName(),
						*RawSpawnClass,
						*NormalizeError);
				}
			}
		}

		// attachToComponentTag: optional mesh attachment override tag
		if (JsonObject->HasField(TEXT("attachToComponentTag")))
		{
			const FString AttachTag = JsonObject->GetStringField(TEXT("attachToComponentTag"));
			ObjDef->AttachToComponentTag = FName(*AttachTag);
		}

		// actorTags: optional actor tags applied to spawned actor
		const TArray<TSharedPtr<FJsonValue>>* ActorTagsArray = nullptr;
		if (JsonObject->TryGetArrayField(TEXT("actorTags"), ActorTagsArray) && ActorTagsArray)
		{
			for (const TSharedPtr<FJsonValue>& TagValue : *ActorTagsArray)
			{
				if (!TagValue.IsValid() || TagValue->Type != EJson::String)
				{
					continue;
				}

				const FString TagStr = TagValue->AsString().TrimStartAndEnd();
				if (!TagStr.IsEmpty())
				{
					const FName TagName(*TagStr);
					if (!ObjDef->ActorTags.Contains(TagName))
					{
						ObjDef->ActorTags.Add(TagName);
					}
				}
			}
		}

		// ---------------------------------------------------------------------
		// 2. MESHES (required)
		// ---------------------------------------------------------------------
		const TArray<TSharedPtr<FJsonValue>>* MeshesArray = nullptr;
		if (JsonObject->TryGetArrayField(TEXT("meshes"), MeshesArray) && MeshesArray && MeshesArray->Num() > 0)
		{
			ObjDef->Meshes.Empty();
			int32 MeshIndex = 0;
			for (const TSharedPtr<FJsonValue>& Element : *MeshesArray)
			{
				const TSharedPtr<FJsonObject>* MeshObj;
				if (Element->TryGetObject(MeshObj))
				{
					if (!(*MeshObj)->HasField(TEXT("id")) || !(*MeshObj)->HasField(TEXT("asset")))
					{
						UE_LOG(LogDefinitionJsonParser, Error, TEXT("[%s] Mesh[%d]: missing required 'id' or 'asset'"),
							*Asset->GetName(), MeshIndex);
						MeshIndex++;
						continue;
					}

					FObjectMeshEntry Entry;
					FJsonObjectConverter::JsonObjectToUStruct(
						MeshObj->ToSharedRef(), FObjectMeshEntry::StaticStruct(), &Entry, 0, 0, false, nullptr, &Callback);

					// Internal flag: physics was specified in JSON
					if ((*MeshObj)->HasField(TEXT("physics")))
					{
						Entry.Physics.bIsSet = true;
					}

					// Normalize material paths
					for (TSoftObjectPtr<UMaterialInterface>& MatPtr : Entry.Materials)
					{
						if (!MatPtr.IsNull())
						{
							FString Path = MatPtr.ToString();
							MatPtr = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(NormalizeAssetPath(Path)));
						}
					}

					ObjDef->Meshes.Add(Entry);
				}
				MeshIndex++;
			}
		}
		else
		{
			UE_LOG(LogDefinitionJsonParser, Error, TEXT("[%s] Missing required field 'meshes' or array is empty"), *Asset->GetName());
			OutError = TEXT("Missing required field: meshes");
			return false;
		}

		// ---------------------------------------------------------------------
		// 3. CAPABILITIES (optional)
		// ---------------------------------------------------------------------
		const TArray<TSharedPtr<FJsonValue>>* CapsArray = nullptr;
		if (JsonObject->TryGetArrayField(TEXT("capabilities"), CapsArray) && CapsArray)
		{
			ObjDef->Capabilities.Empty();
			int32 CapIndex = 0;
			for (const TSharedPtr<FJsonValue>& Element : *CapsArray)
			{
				const TSharedPtr<FJsonObject>* CapObj;
				if (Element->TryGetObject(CapObj))
				{
					if (!(*CapObj)->HasField(TEXT("type")) || !(*CapObj)->HasField(TEXT("scope")))
					{
						UE_LOG(LogDefinitionJsonParser, Error, TEXT("[%s] Capability[%d]: missing required 'type' or 'scope'"),
							*Asset->GetName(), CapIndex);
						CapIndex++;
						continue;
					}

					FObjectCapabilityEntry Entry;
					Entry.Type = FName(*(*CapObj)->GetStringField(TEXT("type")));

					// Scope: TArray<FName> from JSON array
					const TArray<TSharedPtr<FJsonValue>>* ScopeArray;
					if ((*CapObj)->TryGetArrayField(TEXT("scope"), ScopeArray))
					{
						for (const TSharedPtr<FJsonValue>& ScopeVal : *ScopeArray)
						{
							Entry.Scope.Add(FName(*ScopeVal->AsString()));
						}
					}

					// Properties: TMap<FName, FString> from JSON object
					const TSharedPtr<FJsonObject>* PropsObj;
					if ((*CapObj)->TryGetObjectField(TEXT("properties"), PropsObj))
					{
						for (const auto& Prop : (*PropsObj)->Values)
						{
							Entry.Properties.Add(FName(*Prop.Key), Prop.Value->AsString());

							// Derived: MotionMode = "Chaos" -> bUsePhysicsMode
							if (Prop.Key.Equals(TEXT("MotionMode"), ESearchCase::IgnoreCase)
								&& Prop.Value->AsString().Equals(TEXT("Chaos"), ESearchCase::IgnoreCase))
							{
								Entry.bUsePhysicsMode = true;
							}
						}
					}

					ObjDef->Capabilities.Add(Entry);
				}
				CapIndex++;
			}
		}

		// ---------------------------------------------------------------------
		// 4. SECTIONS (optional) - currently only "item" section
		// ---------------------------------------------------------------------
		const TSharedPtr<FJsonObject>* SectionsObj = nullptr;
		const TSharedPtr<FJsonObject>* ItemObj = nullptr;

		// Try canonical form: sections.item
		if (JsonObject->TryGetObjectField(TEXT("sections"), SectionsObj) && SectionsObj && SectionsObj->IsValid())
		{
			(*SectionsObj)->TryGetObjectField(TEXT("item"), ItemObj);
		}

		// Fallback: top-level "item" (sugar form)
		if (!ItemObj || !ItemObj->IsValid())
		{
			JsonObject->TryGetObjectField(TEXT("item"), ItemObj);
		}

		if (ItemObj && ItemObj->IsValid())
		{
			FItemSection ItemData;

			if (FJsonObjectConverter::JsonObjectToUStruct(
				ItemObj->ToSharedRef(), FItemSection::StaticStruct(), &ItemData, 0, 0, false, nullptr, &Callback))
			{
				// Tags: support string "A,B" or array ["A","B"] format
				FString TagsString;
				const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;

				if ((*ItemObj)->TryGetStringField(TEXT("tags"), TagsString) && !TagsString.IsEmpty())
				{
					TArray<FString> TagStrings;
					TagsString.ParseIntoArray(TagStrings, TEXT(","));
					for (const FString& TagStr : TagStrings)
					{
						FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr.TrimStartAndEnd()), false);
						if (Tag.IsValid())
						{
							ItemData.Tags.AddTag(Tag);
						}
						else
						{
							UE_LOG(LogDefinitionJsonParser, Warning, TEXT("[%s] Invalid tag: %s"), *Asset->GetName(), *TagStr);
						}
					}
				}
				else if ((*ItemObj)->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray)
				{
					for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
					{
						FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagValue->AsString()), false);
						if (Tag.IsValid())
						{
							ItemData.Tags.AddTag(Tag);
						}
						else
						{
							UE_LOG(LogDefinitionJsonParser, Warning, TEXT("[%s] Invalid tag: %s"), *Asset->GetName(), *TagValue->AsString());
						}
					}
				}

				// Normalize GrantedAbilities (class paths - /Script/ passes through)
				for (FSoftClassPath& ClassPath : ItemData.GrantedAbilities)
				{
					if (ClassPath.IsValid())
					{
						ClassPath = FSoftClassPath(NormalizeAssetPath(ClassPath.ToString()));
					}
				}

				// Normalize GrantedEffects (asset paths)
				for (FSoftObjectPath& EffectPath : ItemData.GrantedEffects)
				{
					if (EffectPath.IsValid())
					{
						EffectPath = FSoftObjectPath(NormalizeAssetPath(EffectPath.ToString()));
					}
				}

				// Add to Sections map
				FInstancedStruct ItemSection;
				ItemSection.InitializeAs(FItemSection::StaticStruct());
				*ItemSection.GetMutablePtr<FItemSection>() = MoveTemp(ItemData);
				ObjDef->Sections.Add(ObjectSectionIds::Item, MoveTemp(ItemSection));

				UE_LOG(LogDefinitionJsonParser, Log, TEXT("[%s] Parsed item section: '%s'"),
					*Asset->GetName(), *ObjDef->GetItemSection()->DisplayName.ToString());
			}
			else
			{
				UE_LOG(LogDefinitionJsonParser, Error, TEXT("[%s] Failed to parse item section"), *Asset->GetName());
			}
		}

		// ---------------------------------------------------------------------
		// 5. LOOT SECTION (optional) - for loot containers
		// ---------------------------------------------------------------------
		const TSharedPtr<FJsonObject>* LootObj = nullptr;

		// Try canonical form: sections.loot
		if (SectionsObj && SectionsObj->IsValid())
		{
			(*SectionsObj)->TryGetObjectField(TEXT("loot"), LootObj);
		}

		// Fallback: top-level "loot" (sugar form)
		if (!LootObj || !LootObj->IsValid())
		{
			JsonObject->TryGetObjectField(TEXT("loot"), LootObj);
		}

		if (LootObj && LootObj->IsValid())
		{
			FLootSection LootData;

			// Parse entries array
			const TArray<TSharedPtr<FJsonValue>>* EntriesArray = nullptr;
			if ((*LootObj)->TryGetArrayField(TEXT("entries"), EntriesArray))
			{
				for (const TSharedPtr<FJsonValue>& EntryValue : *EntriesArray)
				{
					const TSharedPtr<FJsonObject>* EntryObj = nullptr;
					if (EntryValue->TryGetObject(EntryObj))
					{
						FLootEntryView Entry;

						// Parse objectId as FPrimaryAssetId
						FString ObjectIdStr;
						if ((*EntryObj)->TryGetStringField(TEXT("objectId"), ObjectIdStr))
						{
							Entry.ObjectId = FPrimaryAssetId::FromString(ObjectIdStr);
						}

						// Parse quantity (default 1)
						Entry.Quantity = (*EntryObj)->GetIntegerField(TEXT("quantity"));
						if (Entry.Quantity < 1)
						{
							Entry.Quantity = 1;
						}

						if (Entry.IsValid())
						{
							LootData.Entries.Add(Entry);
						}
						else
						{
							UE_LOG(LogDefinitionJsonParser, Warning,
								TEXT("[%s] Invalid loot entry: objectId='%s'"),
								*Asset->GetName(), *ObjectIdStr);
						}
					}
				}
			}

			if (LootData.Entries.Num() > 0)
			{
				// Add to Sections map
				FInstancedStruct LootSection;
				LootSection.InitializeAs(FLootSection::StaticStruct());
				*LootSection.GetMutablePtr<FLootSection>() = MoveTemp(LootData);
				ObjDef->Sections.Add(ObjectSectionIds::Loot, MoveTemp(LootSection));

				UE_LOG(LogDefinitionJsonParser, Log, TEXT("[%s] Parsed loot section: %d entries"),
					*Asset->GetName(), ObjDef->GetLootSection()->Entries.Num());
			}
		}

		UE_LOG(LogDefinitionJsonParser, Log, TEXT("[%s] ObjectDefinition: %d meshes, %d capabilities, %d sections"),
			*Asset->GetName(), ObjDef->Meshes.Num(), ObjDef->Capabilities.Num(), ObjDef->Sections.Num());
	}

	// Call custom parser if provided (works for both modes)
	if (TypeInfo.CustomParser)
	{
		if (!TypeInfo.CustomParser(JsonObject, Asset, OutError))
		{
			return false;
		}
	}

	return true;
}

FString FDefinitionJsonParser::NormalizeAssetPath(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return Path;
	}

	// Pass through C++ class paths unchanged (e.g., /Script/ProjectGAS.MyClass)
	if (Path.StartsWith(TEXT("/Script/")))
	{
		return Path;
	}

	int32 LastSlash = INDEX_NONE;
	int32 LastDot = INDEX_NONE;
	Path.FindLastChar(TEXT('/'), LastSlash);
	Path.FindLastChar(TEXT('.'), LastDot);

	if (LastDot > LastSlash)
	{
		return Path;
	}

	const FString AssetName = FPaths::GetBaseFilename(Path);
	return Path + TEXT(".") + AssetName;
}

FString FDefinitionJsonParser::NormalizeClassPath(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return Path;
	}

	// Pass through C++ class paths unchanged (e.g., /Script/ProjectGAS.MyClass)
	if (Path.StartsWith(TEXT("/Script/")))
	{
		return Path;
	}

	int32 LastSlash = INDEX_NONE;
	int32 LastDot = INDEX_NONE;
	Path.FindLastChar(TEXT('/'), LastSlash);
	Path.FindLastChar(TEXT('.'), LastDot);

	if (LastDot > LastSlash)
	{
		return Path;
	}

	const FString AssetName = FPaths::GetBaseFilename(Path);
	return Path + TEXT(".") + AssetName + TEXT("_C");
}

EDefinitionFieldType FDefinitionJsonParser::StringToFieldType(const FString& TypeString)
{
	if (TypeString.Equals(TEXT("String"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::String;
	if (TypeString.Equals(TEXT("Name"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::Name;
	if (TypeString.Equals(TEXT("Text"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::Text;
	if (TypeString.Equals(TEXT("Int"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::Int;
	if (TypeString.Equals(TEXT("Float"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::Float;
	if (TypeString.Equals(TEXT("Bool"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::Bool;
	if (TypeString.Equals(TEXT("Vector"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::Vector;
	if (TypeString.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::Rotator;
	if (TypeString.Equals(TEXT("IntPoint"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::IntPoint;
	if (TypeString.Equals(TEXT("SoftObject"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::SoftObject;
	if (TypeString.Equals(TEXT("SoftClass"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::SoftClass;
	if (TypeString.Equals(TEXT("GameplayTag"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::GameplayTag;
	if (TypeString.Equals(TEXT("GameplayTagContainer"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::GameplayTagContainer;
	if (TypeString.Equals(TEXT("SoftObjectArray"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::SoftObjectArray;
	if (TypeString.Equals(TEXT("SoftClassArray"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::SoftClassArray;
	if (TypeString.Equals(TEXT("GameplayTagFloatMap"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::GameplayTagFloatMap;
	if (TypeString.Equals(TEXT("MeshEntryArray"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::MeshEntryArray;
	if (TypeString.Equals(TEXT("CapabilityEntryArray"), ESearchCase::IgnoreCase)) return EDefinitionFieldType::CapabilityEntryArray;

	return EDefinitionFieldType::Auto;
}
