// Copyright ALIS. All Rights Reserved.

#include "DefinitionSchemaDiscovery.h"
#include "DefinitionJsonParser.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionSchemaDiscovery, Log, All);

TArray<FDefinitionSchemaDiscovery::FSchemaParseResult> FDefinitionSchemaDiscovery::DiscoverSchemas()
{
	TArray<FSchemaParseResult> Results;

	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("=== Schema Discovery Started ==="));

	const FString ProjectDir = FPaths::ProjectDir();
	const FString PluginsDir = ProjectDir / TEXT("Plugins");
	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("Scanning for schemas in: %s"), *PluginsDir);

	// Find all *.schema.json files in any plugin's Data/Schemas folder
	TArray<FString> SchemaFiles;
	IFileManager::Get().FindFilesRecursive(SchemaFiles, *PluginsDir, TEXT("*.schema.json"), true, false);
	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("Found %d *.schema.json files total"), SchemaFiles.Num());

	for (const FString& SchemaPath : SchemaFiles)
	{
		// Only process files in Schemas subdirectory
		if (SchemaPath.Contains(TEXT("/Data/Schemas/")) || SchemaPath.Contains(TEXT("\\Data\\Schemas\\")))
		{
			UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("Processing schema: %s"), *SchemaPath);
			FSchemaParseResult ParseResult = ParseSchemaFile(SchemaPath);
			if (ParseResult.bSuccess)
			{
				Results.Add(ParseResult);
			}
		}
		else
		{
			UE_LOG(LogDefinitionSchemaDiscovery, Verbose, TEXT("Skipping (not in Data/Schemas): %s"), *SchemaPath);
		}
	}

	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("=== Schema Discovery Complete: %d types discovered ==="), Results.Num());
	return Results;
}

FDefinitionSchemaDiscovery::FSchemaParseResult FDefinitionSchemaDiscovery::ParseSchemaFile(const FString& SchemaPath)
{
	FSchemaParseResult Result;

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *SchemaPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to read schema: %s"), *SchemaPath);
		UE_LOG(LogDefinitionSchemaDiscovery, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to parse schema JSON: %s"), *SchemaPath);
		UE_LOG(LogDefinitionSchemaDiscovery, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Check for x-alis-generator extension block
	const TSharedPtr<FJsonObject>* GeneratorBlock;
	if (!JsonObject->TryGetObjectField(TEXT("x-alis-generator"), GeneratorBlock))
	{
		// No generator block - this schema doesn't define a generated type (not an error)
		return Result;
	}

	// Check for disabled flag (skip generation entirely)
	if ((*GeneratorBlock)->HasField(TEXT("Disabled")) && (*GeneratorBlock)->GetBoolField(TEXT("Disabled")))
	{
		UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("Skipping disabled schema: %s"), *SchemaPath);
		return Result;
	}

	// Required fields from x-alis-generator block
	FString TypeName = (*GeneratorBlock)->GetStringField(TEXT("TypeName"));
	FString DefinitionClassPath = (*GeneratorBlock)->GetStringField(TEXT("DefinitionClass"));
	FString GeneratedContentPath = (*GeneratorBlock)->GetStringField(TEXT("GeneratedContentPath"));

	if (TypeName.IsEmpty() || DefinitionClassPath.IsEmpty() || GeneratedContentPath.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Schema x-alis-generator missing required fields: %s"), *SchemaPath);
		UE_LOG(LogDefinitionSchemaDiscovery, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Infer PluginName from schema path if not specified
	// Path format: .../Plugins/<Category>/<PluginName>/Data/Schemas/<type>.schema.json
	FString PluginName;
	if ((*GeneratorBlock)->HasField(TEXT("PluginName")))
	{
		PluginName = (*GeneratorBlock)->GetStringField(TEXT("PluginName"));
	}
	else
	{
		// Extract from path
		FString NormalizedPath = SchemaPath.Replace(TEXT("\\"), TEXT("/"));
		TArray<FString> PathParts;
		NormalizedPath.ParseIntoArray(PathParts, TEXT("/"));

		// Find "Data" in path and take the previous component
		for (int32 i = PathParts.Num() - 1; i >= 1; --i)
		{
			if (PathParts[i].Equals(TEXT("Data"), ESearchCase::IgnoreCase))
			{
				PluginName = PathParts[i - 1];
				break;
			}
		}
	}

	if (PluginName.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Could not determine PluginName for schema: %s"), *SchemaPath);
		UE_LOG(LogDefinitionSchemaDiscovery, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Load the definition class
	UClass* DefinitionClass = LoadClass<UObject>(nullptr, *DefinitionClassPath);
	if (!DefinitionClass)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to load definition class '%s' from schema: %s"), *DefinitionClassPath, *SchemaPath);
		UE_LOG(LogDefinitionSchemaDiscovery, Warning, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Build FDefinitionTypeInfo
	FDefinitionTypeInfo TypeInfo;
	TypeInfo.DefinitionClass = DefinitionClass;
	TypeInfo.PluginName = PluginName;
	TypeInfo.GeneratedContentPath = GeneratedContentPath;

	// Optional fields with defaults
	TypeInfo.SourceSubDir = (*GeneratorBlock)->HasField(TEXT("SourceSubDir")) ? (*GeneratorBlock)->GetStringField(TEXT("SourceSubDir")) : TEXT("");
	TypeInfo.SourceFileGlob = (*GeneratorBlock)->HasField(TEXT("SourceFileGlob")) ? (*GeneratorBlock)->GetStringField(TEXT("SourceFileGlob")) : TEXT("");
	TypeInfo.IdPropertyName = (*GeneratorBlock)->HasField(TEXT("IdPropertyName")) ? (*GeneratorBlock)->GetStringField(TEXT("IdPropertyName")) : TEXT("Id");
	TypeInfo.GeneratorVersion = (*GeneratorBlock)->HasField(TEXT("GeneratorVersion")) ? (*GeneratorBlock)->GetIntegerField(TEXT("GeneratorVersion")) : 1;
	TypeInfo.bFlattenGeneratedPath = (*GeneratorBlock)->HasField(TEXT("FlattenGeneratedPath")) ? (*GeneratorBlock)->GetBoolField(TEXT("FlattenGeneratedPath")) : false;

	// Optional metadata property names (with defaults)
	TypeInfo.HashPropertyName = (*GeneratorBlock)->HasField(TEXT("HashPropertyName")) ? (*GeneratorBlock)->GetStringField(TEXT("HashPropertyName")) : TEXT("SourceJsonHash");
	TypeInfo.VersionPropertyName = (*GeneratorBlock)->HasField(TEXT("VersionPropertyName")) ? (*GeneratorBlock)->GetStringField(TEXT("VersionPropertyName")) : TEXT("GeneratorVersion");
	TypeInfo.GeneratedFlagPropertyName = (*GeneratorBlock)->HasField(TEXT("GeneratedFlagPropertyName")) ? (*GeneratorBlock)->GetStringField(TEXT("GeneratedFlagPropertyName")) : TEXT("bGenerated");
	TypeInfo.SourcePathPropertyName = (*GeneratorBlock)->HasField(TEXT("SourcePathPropertyName")) ? (*GeneratorBlock)->GetStringField(TEXT("SourcePathPropertyName")) : TEXT("SourceJsonPath");

	// Parse field mappings
	const TArray<TSharedPtr<FJsonValue>>* FieldMappingsArray;
	if ((*GeneratorBlock)->TryGetArrayField(TEXT("FieldMappings"), FieldMappingsArray))
	{
		for (const TSharedPtr<FJsonValue>& MappingValue : *FieldMappingsArray)
		{
			const TSharedPtr<FJsonObject>* MappingObj;
			if (MappingValue->TryGetObject(MappingObj))
			{
				FDefinitionFieldMapping Mapping;
				Mapping.JsonPath = (*MappingObj)->GetStringField(TEXT("JsonPath"));
				Mapping.PropertyName = (*MappingObj)->GetStringField(TEXT("PropertyName"));

				FString TypeString = (*MappingObj)->HasField(TEXT("FieldType")) ? (*MappingObj)->GetStringField(TEXT("FieldType")) : TEXT("Auto");
				Mapping.FieldType = FDefinitionJsonParser::StringToFieldType(TypeString);

				Mapping.bRequired = (*MappingObj)->HasField(TEXT("bRequired")) && (*MappingObj)->GetBoolField(TEXT("bRequired"));

				TypeInfo.FieldMappings.Add(Mapping);
			}
		}
	}

	// Log extracted config
	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("  [%s] PluginName: %s"), *TypeName, *PluginName);
	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("  [%s] DefinitionClass: %s"), *TypeName, *DefinitionClassPath);
	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("  [%s] SourceSubDir: '%s'"), *TypeName, *TypeInfo.SourceSubDir);
	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("  [%s] GeneratedContentPath: %s"), *TypeName, *GeneratedContentPath);
	UE_LOG(LogDefinitionSchemaDiscovery, Log, TEXT("  [%s] FieldMappings: %d"), *TypeName, TypeInfo.FieldMappings.Num());

	Result.TypeName = TypeName;
	Result.TypeInfo = TypeInfo;
	Result.bSuccess = true;
	return Result;
}
