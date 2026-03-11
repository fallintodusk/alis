// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectUIRegistrySubsystem.h"
#include "Subsystems/ProjectUILayoutValidatorSubsystem.h"
#include "ProjectGameplayTags.h"
#include "ProjectPaths.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectUIRegistry, Log, All);

void UProjectUIRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UProjectUILayoutValidatorSubsystem>();
	Super::Initialize(Collection);

	LoadDefinitions();
}

void UProjectUIRegistrySubsystem::Deinitialize()
{
	DefinitionsById.Empty();
	DefinitionIdsBySlot.Empty();
	DefinitionIdsByLayer.Empty();

	Super::Deinitialize();
}

const FProjectUIDefinition* UProjectUIRegistrySubsystem::FindDefinition(FName Id) const
{
	return DefinitionsById.Find(Id);
}

void UProjectUIRegistrySubsystem::GetDefinitionsForSlot(FGameplayTag SlotTag, TArray<const FProjectUIDefinition*>& OutDefinitions) const
{
	OutDefinitions.Reset();

	TArray<FName> Ids;
	DefinitionIdsBySlot.MultiFind(SlotTag, Ids);
	for (const FName& Id : Ids)
	{
		if (const FProjectUIDefinition* Def = DefinitionsById.Find(Id))
		{
			OutDefinitions.Add(Def);
		}
	}

	SortDefinitionsByPriority(OutDefinitions);
}

void UProjectUIRegistrySubsystem::GetDefinitionsForLayer(FGameplayTag LayerTag, TArray<const FProjectUIDefinition*>& OutDefinitions) const
{
	OutDefinitions.Reset();

	TArray<FName> Ids;
	DefinitionIdsByLayer.MultiFind(LayerTag, Ids);
	for (const FName& Id : Ids)
	{
		if (const FProjectUIDefinition* Def = DefinitionsById.Find(Id))
		{
			OutDefinitions.Add(Def);
		}
	}

	SortDefinitionsByPriority(OutDefinitions);
}

void UProjectUIRegistrySubsystem::LoadDefinitions()
{
	DefinitionsById.Empty();
	DefinitionIdsBySlot.Empty();
	DefinitionIdsByLayer.Empty();

	// Scan all enabled plugins for Data/ui_definitions.json
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		FString DataDir = FProjectPaths::GetPluginDataDir(Plugin->GetName());
		if (DataDir.IsEmpty())
		{
			continue;
		}

		FString DefPath = DataDir / TEXT("ui_definitions.json");
		if (FPaths::FileExists(DefPath))
		{
			LoadDefinitionsFromFile(DefPath, Plugin->GetName());
		}
	}

	UE_LOG(LogProjectUIRegistry, Log, TEXT("Loaded %d UI definitions"), DefinitionsById.Num());
}

bool UProjectUIRegistrySubsystem::LoadDefinitionsFromFile(const FString& FilePath, const FString& PluginName)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogProjectUIRegistry, Warning, TEXT("Failed to read UI definitions: %s"), *FilePath);
		return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	TArray<TSharedPtr<FJsonValue>> RootArray;
	if (!FJsonSerializer::Deserialize(Reader, RootArray))
	{
		UE_LOG(LogProjectUIRegistry, Warning, TEXT("Failed to parse UI definitions JSON: %s"), *FilePath);
		return false;
	}

	UProjectUILayoutValidatorSubsystem* Validator = GetGameInstance()->GetSubsystem<UProjectUILayoutValidatorSubsystem>();
	for (const TSharedPtr<FJsonValue>& Value : RootArray)
	{
		const TSharedPtr<FJsonObject> Object = Value ? Value->AsObject() : nullptr;
		if (!Object.IsValid())
		{
			UE_LOG(LogProjectUIRegistry, Warning, TEXT("Invalid JSON object in %s"), *FilePath);
			continue;
		}

		FProjectUIDefinition Definition;
		if (!ParseDefinitionObject(Object, PluginName, FilePath, Definition))
		{
			continue;
		}

		if (DefinitionsById.Contains(Definition.Id))
		{
			UE_LOG(LogProjectUIRegistry, Error, TEXT("Duplicate UI definition Id: %s (file: %s)"),
				*Definition.Id.ToString(), *FilePath);
			continue;
		}

		if (Validator && !Validator->ValidateAndLog(Definition))
		{
			continue;
		}

		DefinitionsById.Add(Definition.Id, Definition);

		if (Definition.SlotTag.IsValid())
		{
			DefinitionIdsBySlot.Add(Definition.SlotTag, Definition.Id);
		}
		if (Definition.LayerTag.IsValid())
		{
			DefinitionIdsByLayer.Add(Definition.LayerTag, Definition.Id);
		}
	}

	return true;
}

bool UProjectUIRegistrySubsystem::ParseDefinitionObject(const TSharedPtr<FJsonObject>& Object, const FString& PluginName, const FString& SourceFilePath, FProjectUIDefinition& OutDefinition) const
{
	FString IdString;
	if (!Object->TryGetStringField(TEXT("id"), IdString) || IdString.IsEmpty())
	{
		UE_LOG(LogProjectUIRegistry, Warning, TEXT("UI definition missing id in %s"), *SourceFilePath);
		return false;
	}

	OutDefinition.Id = FName(*IdString);
	OutDefinition.SourcePluginName = PluginName;
	OutDefinition.SourceFilePath = SourceFilePath;

	FString LayerString;
	if (Object->TryGetStringField(TEXT("layer"), LayerString))
	{
		OutDefinition.LayerTag = ResolveLayerTag(LayerString);
	}

	FString SlotString;
	if (Object->TryGetStringField(TEXT("slot"), SlotString))
	{
		OutDefinition.SlotTag = ResolveTag(SlotString);
	}

	FString WidgetClassString;
	if (Object->TryGetStringField(TEXT("widget_class"), WidgetClassString))
	{
		OutDefinition.WidgetClassPath = FSoftClassPath(WidgetClassString);
	}

	FString ViewModelClassString;
	if (Object->TryGetStringField(TEXT("viewmodel_class"), ViewModelClassString))
	{
		OutDefinition.ViewModelClassPath = FSoftClassPath(ViewModelClassString);
	}

	Object->TryGetStringField(TEXT("layout_json"), OutDefinition.LayoutJson);
	if (!OutDefinition.LayoutJson.IsEmpty())
	{
		// Use plugin data path: Data/<layout_json>
		FString DataDir = FProjectPaths::GetPluginDataDir(PluginName);
		if (!DataDir.IsEmpty())
		{
			OutDefinition.LayoutJsonPath = DataDir / OutDefinition.LayoutJson;
		}
	}

	FString LoadString;
	if (Object->TryGetStringField(TEXT("load"), LoadString))
	{
		OutDefinition.LoadPolicy = ParseLoadPolicy(LoadString);
	}

	FString SpawnString;
	if (Object->TryGetStringField(TEXT("spawn"), SpawnString))
	{
		OutDefinition.SpawnPolicy = ParseSpawnPolicy(SpawnString);
	}

	FString ScopeString;
	if (Object->TryGetStringField(TEXT("scope"), ScopeString))
	{
		OutDefinition.Scope = ParseScope(ScopeString);
	}

	FString SizeString;
	if (Object->TryGetStringField(TEXT("size_policy"), SizeString))
	{
		OutDefinition.SizePolicy = ParseSizePolicy(SizeString);
	}

	FString InputString;
	if (Object->TryGetStringField(TEXT("input"), InputString))
	{
		OutDefinition.RequestedInput = ParseInputMode(InputString);
	}

	FString ViewModelCreationString;
	if (Object->TryGetStringField(TEXT("vm_creation"), ViewModelCreationString))
	{
		OutDefinition.ViewModelCreationPolicy = ParseViewModelCreationPolicy(ViewModelCreationString);
	}

	Object->TryGetStringField(TEXT("vm_property_path"), OutDefinition.ViewModelPropertyPath);

	double PriorityValue = 0.0;
	if (Object->TryGetNumberField(TEXT("priority"), PriorityValue))
	{
		OutDefinition.Priority = static_cast<int32>(PriorityValue);
	}

	FString AutoVisString;
	if (Object->TryGetStringField(TEXT("auto_visibility"), AutoVisString) && !AutoVisString.IsEmpty())
	{
		OutDefinition.AutoVisibilityProperty = FName(*AutoVisString);
	}

	return true;
}

FGameplayTag UProjectUIRegistrySubsystem::ResolveLayerTag(const FString& LayerString) const
{
	// Accept common aliases to reduce friction in JSON.
	const FString Normalized = LayerString.ToLower();
	if (Normalized == TEXT("ui.layer.menu"))
	{
		return ProjectTags::UI_Layer_GameMenu;
	}
	if (Normalized == TEXT("ui.layer.overlay"))
	{
		return ProjectTags::UI_Layer_Notification;
	}

	return ResolveTag(LayerString);
}

FGameplayTag UProjectUIRegistrySubsystem::ResolveTag(const FString& TagString) const
{
	if (TagString.IsEmpty())
	{
		return FGameplayTag();
	}

	return UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagString), false);
}

EProjectWidgetInputMode UProjectUIRegistrySubsystem::ParseInputMode(const FString& InputString) const
{
	const FString Normalized = InputString.ToLower();
	if (Normalized == TEXT("game"))
	{
		return EProjectWidgetInputMode::Game;
	}
	if (Normalized == TEXT("menu") || Normalized == TEXT("uionly"))
	{
		return EProjectWidgetInputMode::Menu;
	}
	if (Normalized == TEXT("gameandmenu") || Normalized == TEXT("gameandui"))
	{
		return EProjectWidgetInputMode::GameAndMenu;
	}
	if (Normalized == TEXT("passthrough"))
	{
		return EProjectWidgetInputMode::Game;
	}

	return EProjectWidgetInputMode::Default;
}

EProjectUILoadPolicy UProjectUIRegistrySubsystem::ParseLoadPolicy(const FString& LoadString) const
{
	const FString Normalized = LoadString.ToLower();
	if (Normalized == TEXT("preload"))
	{
		return EProjectUILoadPolicy::Preload;
	}

	return EProjectUILoadPolicy::OnDemand;
}

EProjectUISpawnPolicy UProjectUIRegistrySubsystem::ParseSpawnPolicy(const FString& SpawnString) const
{
	const FString Normalized = SpawnString.ToLower();
	if (Normalized == TEXT("transient"))
	{
		return EProjectUISpawnPolicy::Transient;
	}

	return EProjectUISpawnPolicy::Persistent;
}

EProjectUIScope UProjectUIRegistrySubsystem::ParseScope(const FString& ScopeString) const
{
	const FString Normalized = ScopeString.ToLower();
	if (Normalized == TEXT("global"))
	{
		return EProjectUIScope::Global;
	}

	return EProjectUIScope::PerPlayer;
}

EProjectUISizePolicy UProjectUIRegistrySubsystem::ParseSizePolicy(const FString& SizeString) const
{
	const FString Normalized = SizeString.ToLower();
	if (Normalized == TEXT("autosize"))
	{
		return EProjectUISizePolicy::AutoSize;
	}
	if (Normalized == TEXT("desired"))
	{
		return EProjectUISizePolicy::Desired;
	}

	return EProjectUISizePolicy::Fill;
}

EProjectUIViewModelCreationPolicy UProjectUIRegistrySubsystem::ParseViewModelCreationPolicy(const FString& PolicyString) const
{
	const FString Normalized = PolicyString.ToLower();
	if (Normalized == TEXT("global"))
	{
		return EProjectUIViewModelCreationPolicy::Global;
	}
	if (Normalized == TEXT("propertypath"))
	{
		return EProjectUIViewModelCreationPolicy::PropertyPath;
	}

	return EProjectUIViewModelCreationPolicy::CreateInstance;
}

void UProjectUIRegistrySubsystem::SortDefinitionsByPriority(TArray<const FProjectUIDefinition*>& Definitions) const
{
	Definitions.Sort([](const FProjectUIDefinition& A, const FProjectUIDefinition& B)
	{
		if (A.Priority == B.Priority)
		{
			return A.Id.ToString() < B.Id.ToString();
		}
		return A.Priority > B.Priority;
	});
}
