// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefinitionTypeInfo.h"

/**
 * Schema discovery for definition types.
 * Scans plugin directories for x-alis-generator schema files.
 *
 * SOLID: Single responsibility - only schema parsing and discovery.
 * Extracted from UDefinitionGeneratorSubsystem for maintainability.
 */
class PROJECTDEFINITIONGENERATOR_API FDefinitionSchemaDiscovery
{
public:
	/**
	 * Result of parsing a schema file.
	 */
	struct FSchemaParseResult
	{
		FString TypeName;
		FDefinitionTypeInfo TypeInfo;
		bool bSuccess = false;
		FString ErrorMessage;
	};

	/**
	 * Discover all definition type schemas in plugin directories.
	 * Scans Plugins/{Category}/{PluginName}/Data/Schemas/*.schema.json for x-alis-generator blocks.
	 * @return Array of successfully parsed schema results (TypeName -> TypeInfo)
	 */
	static TArray<FSchemaParseResult> DiscoverSchemas();

	/**
	 * Parse a single schema file.
	 * @param SchemaPath Full path to *.schema.json file
	 * @return Parse result with TypeInfo if successful
	 */
	static FSchemaParseResult ParseSchemaFile(const FString& SchemaPath);
};
