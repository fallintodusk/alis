// Copyright ALIS. All Rights Reserved.

#include "DefinitionGeneratorSubsystem.h"
#include "DefinitionJsonParser.h"
#include "DefinitionAssetManager.h"
#include "DefinitionSchemaDiscovery.h"
#include "DefinitionValidator.h"
#include "DefinitionEvents.h"
#include "ProjectPaths.h"
#include "SyncHashUtils.h"
#include "Data/ObjectDefinition.h"
#include "Abilities/ProjectAbilitySet.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Containers/StringConv.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "ObjectTools.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionGenerator, Log, All);

UDefinitionGeneratorSubsystem* UDefinitionGeneratorSubsystem::Get()
{
	return GEngine->GetEngineSubsystem<UDefinitionGeneratorSubsystem>();
}

void UDefinitionGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogDefinitionGenerator, Log, TEXT("DefinitionGeneratorSubsystem initialized"));
}

void UDefinitionGeneratorSubsystem::Deinitialize()
{
	RegisteredTypes.Empty();
	Super::Deinitialize();
}

void UDefinitionGeneratorSubsystem::RegisterDefinitionType(const FString& TypeName, const FDefinitionTypeInfo& Info)
{
	if (!Info.IsValid())
	{
		UE_LOG(LogDefinitionGenerator, Error, TEXT("Invalid DefinitionTypeInfo for type '%s'"), *TypeName);
		return;
	}

	RegisteredTypes.Add(TypeName, Info);
	UE_LOG(LogDefinitionGenerator, Log, TEXT("Registered definition type '%s' (class: %s, plugin: %s)"),
		*TypeName, *Info.DefinitionClass->GetName(), *Info.PluginName);
}

void UDefinitionGeneratorSubsystem::UnregisterDefinitionType(const FString& TypeName)
{
	if (RegisteredTypes.Remove(TypeName) > 0)
	{
		UE_LOG(LogDefinitionGenerator, Log, TEXT("Unregistered definition type '%s'"), *TypeName);
	}
}

bool UDefinitionGeneratorSubsystem::GetDefinitionTypeInfo(const FString& TypeName, FDefinitionTypeInfo& OutInfo) const
{
	if (const FDefinitionTypeInfo* Found = RegisteredTypes.Find(TypeName))
	{
		OutInfo = *Found;
		return true;
	}
	return false;
}

TArray<FString> UDefinitionGeneratorSubsystem::GetRegisteredTypeNames() const
{
	TArray<FString> Names;
	RegisteredTypes.GetKeys(Names);
	return Names;
}

TArray<FString> UDefinitionGeneratorSubsystem::GetExclusionGlobsForType(const FString& TypeName) const
{
	TArray<FString> ExclusionGlobs;

	const FDefinitionTypeInfo* ThisType = RegisteredTypes.Find(TypeName);
	if (!ThisType)
	{
		return ExclusionGlobs;
	}

	const FString ThisSourceDir = GetSourceDirectory(*ThisType);

	for (const auto& Pair : RegisteredTypes)
	{
		if (Pair.Key == TypeName || Pair.Value.SourceFileGlob.IsEmpty())
		{
			continue;
		}

		// Only exclude if the other type shares the same source directory
		const FString OtherSourceDir = GetSourceDirectory(Pair.Value);
		if (FPaths::IsSamePath(ThisSourceDir, OtherSourceDir))
		{
			ExclusionGlobs.Add(Pair.Value.SourceFileGlob);
		}
	}

	return ExclusionGlobs;
}

bool UDefinitionGeneratorSubsystem::MatchesAnyGlob(const FString& FileName, const TArray<FString>& Globs)
{
	for (const FString& Glob : Globs)
	{
		if (FileName.MatchesWildcard(Glob))
		{
			return true;
		}
	}
	return false;
}

FString UDefinitionGeneratorSubsystem::GetSourceDirectory(const FDefinitionTypeInfo& TypeInfo) const
{
	FString BaseDir = FProjectPaths::GetPluginDataDir(TypeInfo.PluginName);
	if (!TypeInfo.SourceSubDir.IsEmpty())
	{
		BaseDir = BaseDir / TypeInfo.SourceSubDir;
	}
	// Normalize path to collapse ../ sequences (e.g., "Data/../../Content" -> "Content")
	FPaths::CollapseRelativeDirectories(BaseDir);
	return BaseDir;
}

TArray<FDefinitionGenerationResult> UDefinitionGeneratorSubsystem::GenerateAllForType(const FString& TypeName, bool bForceRegenerate)
{
	TArray<FDefinitionGenerationResult> Results;

	const FDefinitionTypeInfo* TypeInfo = RegisteredTypes.Find(TypeName);
	if (!TypeInfo)
	{
		UE_LOG(LogDefinitionGenerator, Error, TEXT("Unknown definition type: %s"), *TypeName);
		return Results;
	}

	const FString SourceDir = GetSourceDirectory(*TypeInfo);
	UE_LOG(LogDefinitionGenerator, Log, TEXT("[%s] Source directory: %s"), *TypeName, *SourceDir);

	// Find JSON files recursively (use SourceFileGlob if set, otherwise all *.json)
	const FString FileGlob = TypeInfo->SourceFileGlob.IsEmpty() ? TEXT("*.json") : TypeInfo->SourceFileGlob;
	TArray<FString> AllJsonFiles;
	IFileManager::Get().FindFilesRecursive(AllJsonFiles, *SourceDir, *FileGlob, true, false);

	// Collect globs claimed by other types sharing the same source directory
	const TArray<FString> ExclusionGlobs = GetExclusionGlobsForType(TypeName);

	// Filter out schema files and files belonging to other types
	TArray<FString> JsonFiles;
	for (const FString& FilePath : AllJsonFiles)
	{
		const FString FileName = FPaths::GetCleanFilename(FilePath);
		if (FileName.EndsWith(TEXT(".schema.json")) || FileName.EndsWith(TEXT(".md")))
		{
			continue;
		}
		if (MatchesAnyGlob(FileName, ExclusionGlobs))
		{
			continue;
		}
		JsonFiles.Add(FilePath);
	}

	UE_LOG(LogDefinitionGenerator, Log, TEXT("[%s] Found %d source JSON files"), *TypeName, JsonFiles.Num());

	// Build asset lookup map once (O(N) instead of O(N²))
	CachedAssetMap = FDefinitionAssetManager::BuildAssetLookupMap(*TypeInfo);
	bUsingCachedAssetMap = true;

	for (const FString& FullPath : JsonFiles)
	{
		FDefinitionGenerationResult Result = GenerateFromJson(TypeName, FullPath, bForceRegenerate);
		Results.Add(Result);
	}

	// Clear cache after batch operation
	CachedAssetMap.Empty();
	bUsingCachedAssetMap = false;

	// Log summary
	int32 SuccessCount = 0, SkippedCount = 0, FailedCount = 0;
	for (const FDefinitionGenerationResult& Result : Results)
	{
		if (Result.bSkipped) SkippedCount++;
		else if (Result.bSuccess) SuccessCount++;
		else FailedCount++;
	}

	UE_LOG(LogDefinitionGenerator, Log, TEXT("[%s] Generation complete: %d succeeded, %d skipped, %d failed"),
		*TypeName, SuccessCount, SkippedCount, FailedCount);

	// Clean up empty directories left by relocations
	CleanupEmptyDirectories(TypeInfo->GeneratedContentPath);

	return Results;
}

TArray<FDefinitionGenerationResult> UDefinitionGeneratorSubsystem::GenerateAll(bool bForceRegenerate)
{
	TArray<FDefinitionGenerationResult> AllResults;
	for (const auto& Pair : RegisteredTypes)
	{
		AllResults.Append(GenerateAllForType(Pair.Key, bForceRegenerate));
	}
	return AllResults;
}

FDefinitionGenerationResult UDefinitionGeneratorSubsystem::GenerateFromJson(const FString& TypeName, const FString& JsonFilePath, bool bForceRegenerate)
{
	FDefinitionGenerationResult Result;
	Result.TypeName = TypeName;
	Result.SourcePath = JsonFilePath;

	const FDefinitionTypeInfo* TypeInfo = RegisteredTypes.Find(TypeName);
	if (!TypeInfo)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Unknown definition type: %s"), *TypeName);
		return Result;
	}

	// Read JSON file
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to read file: %s"), *JsonFilePath);
		UE_LOG(LogDefinitionGenerator, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Compute hash for incremental generation
	const FString FileHash = ComputeFileHash(JsonFilePath);

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to parse JSON: %s"), *JsonFilePath);
		UE_LOG(LogDefinitionGenerator, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	if (!ResolveAssetIdFromJson(*TypeInfo, JsonObject, Result.AssetId, Result.ErrorMessage))
	{
		Result.ErrorMessage = FString::Printf(TEXT("%s in: %s"), *Result.ErrorMessage, *JsonFilePath);
		UE_LOG(LogDefinitionGenerator, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Extract relative directory from source path (preserves full hierarchy)
	const FString SourceDir = FPaths::ConvertRelativePathToFull(GetSourceDirectory(*TypeInfo));
	FString NormalizedJsonPath = FPaths::ConvertRelativePathToFull(JsonFilePath);
	FString RelativePath = NormalizedJsonPath.Replace(TEXT("\\"), TEXT("/"));
	FString NormalizedSourceDir = SourceDir.Replace(TEXT("\\"), TEXT("/"));
	if (!NormalizedSourceDir.EndsWith(TEXT("/")))
	{
		NormalizedSourceDir += TEXT("/");
	}
	RelativePath.RemoveFromStart(NormalizedSourceDir);

	// Get relative directory (full path, not just first component)
	// e.g., "Consumable/Food/apple.json" -> "Consumable/Food"
	// If bFlattenGeneratedPath, skip hierarchy - all assets go flat into GeneratedContentPath
	const FString RelativeDir = TypeInfo->bFlattenGeneratedPath ? FString() : FPaths::GetPath(RelativePath);

	// Check for existing asset
	UObject* ExistingAsset = FDefinitionAssetManager::FindExistingAsset(
		*TypeInfo, Result.AssetId, bUsingCachedAssetMap ? &CachedAssetMap : nullptr);

	// Check if asset needs relocation (path changed due to hierarchy or flatten)
	bool bNeedsRelocation = false;
	if (ExistingAsset)
	{
		const FString ExpectedPath = RelativeDir.IsEmpty()
			? TypeInfo->GeneratedContentPath / Result.AssetId
			: TypeInfo->GeneratedContentPath / RelativeDir / Result.AssetId;
		const FString ActualPath = ExistingAsset->GetOutermost()->GetName();
		if (ActualPath != ExpectedPath)
		{
			UE_LOG(LogDefinitionGenerator, Log, TEXT("Relocating %s from %s to %s"),
				*Result.AssetId, *ActualPath, *ExpectedPath);

			// Delete old asset at wrong path, will recreate at correct location
			TArray<UObject*> ObjectsToDelete = { ExistingAsset };
			ObjectTools::DeleteObjects(ObjectsToDelete, false);
			ExistingAsset = nullptr;
			bNeedsRelocation = true;
		}
	}

	// Check if regeneration needed via reflection
	if (ExistingAsset && !bForceRegenerate && !bNeedsRelocation)
	{
		FString ExistingHash;
		int32 ExistingVersion = 0;

		// Get hash via reflection
		if (FStrProperty* HashProp = CastField<FStrProperty>(TypeInfo->DefinitionClass->FindPropertyByName(*TypeInfo->HashPropertyName)))
		{
			ExistingHash = HashProp->GetPropertyValue_InContainer(ExistingAsset);
		}
		// Get version via reflection
		if (FIntProperty* VersionProp = CastField<FIntProperty>(TypeInfo->DefinitionClass->FindPropertyByName(*TypeInfo->VersionPropertyName)))
		{
			ExistingVersion = VersionProp->GetPropertyValue_InContainer(ExistingAsset);
		}

		if (ExistingHash == FileHash && ExistingVersion == TypeInfo->GeneratorVersion)
		{
			Result.bSuccess = true;
			Result.bSkipped = true;
			return Result;
		}
	}

	// Create or update asset
	UObject* Asset = ExistingAsset ? ExistingAsset : FDefinitionAssetManager::CreateNewAsset(*TypeInfo, Result.AssetId, RelativeDir);
	if (!Asset)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create asset for: %s"), *Result.AssetId);
		UE_LOG(LogDefinitionGenerator, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	// Parse JSON into asset (delegated to FDefinitionJsonParser)
	FString ParseError;
	if (!FDefinitionJsonParser::ParseJsonToAsset(*TypeInfo, JsonObject, Asset, ParseError))
	{
		Result.ErrorMessage = ParseError;
		UE_LOG(LogDefinitionGenerator, Error, TEXT("Parse error for %s: %s"), *Result.AssetId, *ParseError);
		return Result;
	}

	// Compute definition version hashes (for actor update detection)
	if (UObjectDefinition* ObjDef = Cast<UObjectDefinition>(Asset))
	{
		// Normalize line endings for cross-platform consistency
		FString NormalizedJson = JsonString;
		NormalizedJson.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		NormalizedJson.ReplaceInline(TEXT("\r"), TEXT("\n"));

		// Content hash: raw JSON bytes (KISS - formatting changes trigger reapply, which is cheap)
		ObjDef->DefinitionContentHash = ComputeContentHash(NormalizedJson);

		// Structure hash: detailed structural signature (mesh IDs + capability types + targets)
		ObjDef->DefinitionStructureHash = ComputeStructureHash(ObjDef);

		// Generate AbilitySet from inline GrantedAbilities/GrantedEffects (if present)
		GenerateAbilitySetIfNeeded(ObjDef);

		UE_LOG(LogDefinitionGenerator, Verbose, TEXT("[%s] Computed hashes - Structure: %s, Content: %s"),
			*Result.AssetId, *ObjDef->DefinitionStructureHash, *ObjDef->DefinitionContentHash);
	}

	// Set generation metadata via reflection
	if (FBoolProperty* GeneratedProp = CastField<FBoolProperty>(TypeInfo->DefinitionClass->FindPropertyByName(*TypeInfo->GeneratedFlagPropertyName)))
	{
		GeneratedProp->SetPropertyValue_InContainer(Asset, true);
	}
	if (FIntProperty* VersionProp = CastField<FIntProperty>(TypeInfo->DefinitionClass->FindPropertyByName(*TypeInfo->VersionPropertyName)))
	{
		VersionProp->SetPropertyValue_InContainer(Asset, TypeInfo->GeneratorVersion);
	}
	if (FStrProperty* SourcePathProp = CastField<FStrProperty>(TypeInfo->DefinitionClass->FindPropertyByName(*TypeInfo->SourcePathPropertyName)))
	{
		// Store full relative path (e.g., "Consumable/Food/apple.json") for orphan cleanup
		SourcePathProp->SetPropertyValue_InContainer(Asset, RelativePath);
	}
	if (FStrProperty* HashProp = CastField<FStrProperty>(TypeInfo->DefinitionClass->FindPropertyByName(*TypeInfo->HashPropertyName)))
	{
		HashProp->SetPropertyValue_InContainer(Asset, FileHash);
	}

	// Validate asset (shows 30-sec notification popup for errors)
	FDefinitionValidator::ValidateAndNotify(*TypeInfo, Asset, Result.AssetId, TypeName);

	// Save asset
	if (!FDefinitionAssetManager::SaveAsset(Asset))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to save asset for: %s"), *Result.AssetId);
		UE_LOG(LogDefinitionGenerator, Error, TEXT("%s"), *Result.ErrorMessage);
		return Result;
	}

	Result.bSuccess = true;
	UE_LOG(LogDefinitionGenerator, Log, TEXT("[%s] Generated: %s"), *TypeName, *Result.AssetId);

	// Broadcast to subscribers via ProjectEditorCore (decoupled)
	// DefinitionActorSyncSubsystem (ProjectPlacementEditor) listens for actor updates
	FDefinitionEvents::OnDefinitionRegenerated().Broadcast(TypeName, Asset);

	return Result;
}

bool UDefinitionGeneratorSubsystem::ResolveAssetIdFromJson(
	const FDefinitionTypeInfo& TypeInfo,
	const TSharedPtr<FJsonObject>& JsonObject,
	FString& OutAssetId,
	FString& OutError)
{
	OutAssetId.Reset();
	OutError.Reset();

	if (!JsonObject.IsValid())
	{
		OutError = TEXT("Invalid JSON object");
		return false;
	}

	TArray<FString> CandidateIdFields;
	if (!TypeInfo.IdPropertyName.IsEmpty())
	{
		CandidateIdFields.Add(TypeInfo.IdPropertyName);

		FString LowerCamelIdField = TypeInfo.IdPropertyName;
		if (!LowerCamelIdField.IsEmpty())
		{
			LowerCamelIdField[0] = FChar::ToLower(LowerCamelIdField[0]);
			CandidateIdFields.AddUnique(LowerCamelIdField);
		}
	}
	CandidateIdFields.AddUnique(TEXT("id"));

	for (const FString& CandidateField : CandidateIdFields)
	{
		if (JsonObject->HasTypedField<EJson::String>(CandidateField))
		{
			OutAssetId = JsonObject->GetStringField(CandidateField);
			return !OutAssetId.IsEmpty();
		}
	}

	OutError = FString::Printf(
		TEXT("Missing id field (%s)"),
		*FString::Join(CandidateIdFields, TEXT(", ")));
	return false;
}

int32 UDefinitionGeneratorSubsystem::CleanupOrphanedAssets(const FString& TypeName)
{
	const FDefinitionTypeInfo* TypeInfo = RegisteredTypes.Find(TypeName);
	if (!TypeInfo)
	{
		return 0;
	}

	const FString SourceDir = GetSourceDirectory(*TypeInfo);

	// Build set of existing JSON relative paths (e.g., "Consumable/Food/apple.json")
	const FString OrphanFileGlob = TypeInfo->SourceFileGlob.IsEmpty() ? TEXT("*.json") : TypeInfo->SourceFileGlob;
	const TArray<FString> OrphanExclusionGlobs = GetExclusionGlobsForType(TypeName);
	TSet<FString> ExistingJsonPaths;
	{
		TArray<FString> AllJsonFiles;
		IFileManager::Get().FindFilesRecursive(AllJsonFiles, *SourceDir, *OrphanFileGlob, true, false);

		const FString NormalizedSourceDir = SourceDir.Replace(TEXT("\\"), TEXT("/")) + TEXT("/");

		for (const FString& FullPath : AllJsonFiles)
		{
			const FString FileName = FPaths::GetCleanFilename(FullPath);
			if (FileName.EndsWith(TEXT(".schema.json")) || MatchesAnyGlob(FileName, OrphanExclusionGlobs))
			{
				continue;
			}
			// Compute relative path from source directory
			FString RelPath = FullPath.Replace(TEXT("\\"), TEXT("/"));
			RelPath.RemoveFromStart(NormalizedSourceDir);
			ExistingJsonPaths.Add(RelPath);
		}
	}

	// Scan generated assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(TypeInfo->DefinitionClass->GetClassPathName());
	Filter.PackagePaths.Add(FName(*TypeInfo->GeneratedContentPath));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<FAssetData> OrphanedAssets;

	for (const FAssetData& AssetData : AssetDataList)
	{
		// Check bGenerated tag
		FAssetTagValueRef GeneratedTag = AssetData.TagsAndValues.FindTag(*TypeInfo->GeneratedFlagPropertyName);
		if (!GeneratedTag.IsSet() || GeneratedTag.GetValue() != TEXT("True"))
		{
			continue;
		}

		// Get source path tag
		FAssetTagValueRef SourcePathTag = AssetData.TagsAndValues.FindTag(*TypeInfo->SourcePathPropertyName);
		if (!SourcePathTag.IsSet())
		{
			continue;
		}

		const FString SourceJsonPath = SourcePathTag.GetValue();
		if (!ExistingJsonPaths.Contains(SourceJsonPath))
		{
			OrphanedAssets.Add(AssetData);
		}
	}

	// Delete orphaned assets
	int32 DeletedCount = 0;
	if (OrphanedAssets.Num() > 0)
	{
		TArray<UObject*> ObjectsToDelete;
		for (const FAssetData& AssetData : OrphanedAssets)
		{
			if (UObject* Asset = AssetData.GetAsset())
			{
				ObjectsToDelete.Add(Asset);
			}
		}

		if (ObjectsToDelete.Num() > 0)
		{
			DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, false);
			UE_LOG(LogDefinitionGenerator, Log, TEXT("[%s] Deleted %d orphaned assets"), *TypeName, DeletedCount);
		}
	}

	// Clean up empty directories left behind after asset deletion
	CleanupEmptyDirectories(TypeInfo->GeneratedContentPath);

	return DeletedCount;
}

void UDefinitionGeneratorSubsystem::CleanupEmptyDirectories(const FString& ContentPath)
{
	// Convert content path (e.g., "/ProjectDialogue/Trees") to disk path
	FString DiskPath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(ContentPath + TEXT("/"), DiskPath))
	{
		return;
	}

	// Recursively find and delete empty directories (bottom-up)
	TArray<FString> AllDirs;
	IFileManager::Get().FindFilesRecursive(AllDirs, *DiskPath, TEXT("*"), false, true);

	// Sort by depth descending (deepest first) so children are deleted before parents
	AllDirs.Sort([](const FString& A, const FString& B)
	{
		return A.Len() > B.Len();
	});

	for (const FString& Dir : AllDirs)
	{
		TArray<FString> Contents;
		IFileManager::Get().FindFiles(Contents, *(Dir / TEXT("*")), true, true);
		if (Contents.Num() == 0)
		{
			IFileManager::Get().DeleteDirectory(*Dir, false, false);
			UE_LOG(LogDefinitionGenerator, Log, TEXT("Removed empty directory: %s"), *Dir);
		}
	}
}

UObject* UDefinitionGeneratorSubsystem::FindExistingAssetForType(const FString& TypeName, const FString& AssetId)
{
	const FDefinitionTypeInfo* TypeInfo = RegisteredTypes.Find(TypeName);
	if (!TypeInfo)
	{
		UE_LOG(LogDefinitionGenerator, Warning, TEXT("FindExistingAssetForType: Unknown type '%s'"), *TypeName);
		return nullptr;
	}

	return FDefinitionAssetManager::FindExistingAsset(*TypeInfo, AssetId);
}

FString UDefinitionGeneratorSubsystem::ComputeFileHash(const FString& FilePath)
{
	return FSyncHashUtils::ComputeNormalizedHashFromFile(FilePath);
}

void UDefinitionGeneratorSubsystem::DiscoverAndRegisterManifests()
{
	// Delegate to FDefinitionSchemaDiscovery for schema parsing
	TArray<FDefinitionSchemaDiscovery::FSchemaParseResult> DiscoveredSchemas = FDefinitionSchemaDiscovery::DiscoverSchemas();

	for (const FDefinitionSchemaDiscovery::FSchemaParseResult& ParseResult : DiscoveredSchemas)
	{
		// Log resolved source directory for debugging
		FString ResolvedSourceDir = GetSourceDirectory(ParseResult.TypeInfo);
		UE_LOG(LogDefinitionGenerator, Log, TEXT("  [%s] Resolved SourceDir: %s"), *ParseResult.TypeName, *ResolvedSourceDir);
		UE_LOG(LogDefinitionGenerator, Log, TEXT("  [%s] SourceDir exists: %s"), *ParseResult.TypeName, IFileManager::Get().DirectoryExists(*ResolvedSourceDir) ? TEXT("YES") : TEXT("NO"));

		// Register the discovered type
		RegisterDefinitionType(ParseResult.TypeName, ParseResult.TypeInfo);
		UE_LOG(LogDefinitionGenerator, Log, TEXT("  [%s] Registered with %d field mappings"), *ParseResult.TypeName, ParseResult.TypeInfo.FieldMappings.Num());
	}

	UE_LOG(LogDefinitionGenerator, Log, TEXT("=== Schema Registration Complete: %d types registered ==="), DiscoveredSchemas.Num());
}

FString UDefinitionGeneratorSubsystem::GetSourceDirectoryForType(const FString& TypeName) const
{
	const FDefinitionTypeInfo* TypeInfo = RegisteredTypes.Find(TypeName);
	if (!TypeInfo)
	{
		return FString();
	}
	return GetSourceDirectory(*TypeInfo);
}

FString UDefinitionGeneratorSubsystem::ComputeContentHash(const FString& NormalizedJsonText) const
{
	// KISS: Hash raw JSON file bytes (already normalized line endings in caller)
	// Formatting changes (whitespace, key order) will trigger reapply - that's OK!
	// Reapply is cheap (in-place property updates), so prefer simplicity over canonicalization.

	// CRITICAL: Use UTF-8 byte hashing, not HashAnsiString (which mangles non-ASCII)
	FTCHARToUTF8 Utf8(*NormalizedJsonText);
	const uint8* Data = reinterpret_cast<const uint8*>(Utf8.Get());
	const int32 Len = Utf8.Length();

	FMD5 Md5;
	Md5.Update(Data, Len);
	uint8 Digest[16];
	Md5.Final(Digest);

	return BytesToHex(Digest, 16);
}

FString UDefinitionGeneratorSubsystem::ComputeStructureHash(const UObjectDefinition* Def) const
{
	// Include: mesh count, mesh IDs (sorted), capability types + target meshes (sorted)
	// Sorted for determinism (order doesn't matter since we target meshes by ID)
	// This catches: mesh added/removed, mesh ID changed, capability target changed

	TArray<FString> MeshIds;
	for (const FObjectMeshEntry& Mesh : Def->Meshes)
	{
		// Use mesh ID (stored in Id field)
		FString MeshId = Mesh.Id.IsNone() ? TEXT("") : Mesh.Id.ToString();
		if (MeshId.IsEmpty())
		{
			// Fallback: use asset path if no ID
			MeshId = Mesh.Asset.ToString();
		}
		MeshIds.Add(MeshId);
	}
	MeshIds.Sort();  // Sorted for deterministic hash

	TArray<FString> CapSignatures;
	for (const FObjectCapabilityEntry& Cap : Def->Capabilities)
	{
		// CRITICAL: Use stable capability Type, NOT class name
		// Using class name means renaming a C++ class becomes "structural mismatch"
		FString CapId = Cap.Type.IsNone() ? TEXT("") : Cap.Type.ToString();

		// Include capability ID + target scopes (if applicable)
		FString CapSig = CapId;
		if (Cap.Scope.Num() > 0)
		{
			TArray<FString> ScopeNames;
			for (const FName& ScopeName : Cap.Scope)
			{
				ScopeNames.Add(ScopeName.ToString());
			}
			ScopeNames.Sort();
			CapSig += TEXT(":") + FString::Join(ScopeNames, TEXT(","));
		}
		CapSignatures.Add(CapSig);
	}
	CapSignatures.Sort();  // Sorted for deterministic hash

	// Build signature: count|ids|capabilities
	FString StructSig = FString::Printf(TEXT("Meshes:%d|MeshIds:%s|Caps:%s"),
		Def->Meshes.Num(),
		*FString::Join(MeshIds, TEXT(",")),
		*FString::Join(CapSignatures, TEXT(",")));

	return FMD5::HashAnsiString(*StructSig);
}

void UDefinitionGeneratorSubsystem::GenerateAbilitySetIfNeeded(UObjectDefinition* ObjDef)
{
	// Get item section (if exists)
	FItemSection* ItemSection = ObjDef->GetMutableItemSection();
	if (!ItemSection)
	{
		return;
	}

	// Check if there are abilities or effects to generate AbilitySet for
	if (ItemSection->GrantedAbilities.Num() == 0 && ItemSection->GrantedEffects.Num() == 0)
	{
		return;
	}

	// Compute deterministic hash for deduplication
	const FString AbilitySetHash = ComputeAbilitySetHash(ItemSection->GrantedAbilities, ItemSection->GrantedEffects);
	const FString AbilitySetName = FString::Printf(TEXT("AS_%s"), *AbilitySetHash.Left(16));
	const FString AbilitySetPath = FString::Printf(TEXT("/ProjectGAS/AbilitySets/%s"), *AbilitySetName);

	// Check if AbilitySet already exists
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData ExistingAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(AbilitySetPath + TEXT(".") + AbilitySetName));

	UProjectAbilitySet* AbilitySet = nullptr;

	if (ExistingAsset.IsValid())
	{
		// Reuse existing AbilitySet (same hash = same abilities)
		AbilitySet = Cast<UProjectAbilitySet>(ExistingAsset.GetAsset());
		UE_LOG(LogDefinitionGenerator, Log, TEXT("[%s] Reusing existing AbilitySet: %s"),
			*ObjDef->ObjectId.ToString(), *AbilitySetPath);
	}
	else
	{
		// Create new AbilitySet
		const FString PackagePath = TEXT("/ProjectGAS/AbilitySets");
		const FString PackageName = PackagePath / AbilitySetName;

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			UE_LOG(LogDefinitionGenerator, Error, TEXT("[%s] Failed to create package for AbilitySet: %s"),
				*ObjDef->ObjectId.ToString(), *PackageName);
			return;
		}

		AbilitySet = NewObject<UProjectAbilitySet>(Package, *AbilitySetName, RF_Public | RF_Standalone);
		if (!AbilitySet)
		{
			UE_LOG(LogDefinitionGenerator, Error, TEXT("[%s] Failed to create AbilitySet: %s"),
				*ObjDef->ObjectId.ToString(), *AbilitySetName);
			return;
		}

		// Populate AbilitySet from item section data
		for (const FSoftClassPath& AbilityPath : ItemSection->GrantedAbilities)
		{
			if (UClass* AbilityClass = AbilityPath.TryLoadClass<UGameplayAbility>())
			{
				AbilitySet->GrantedAbilities.Add(AbilityClass);
			}
			else
			{
				UE_LOG(LogDefinitionGenerator, Warning, TEXT("[%s] Failed to load ability class: %s"),
					*ObjDef->ObjectId.ToString(), *AbilityPath.ToString());
			}
		}

		for (const FSoftObjectPath& EffectPath : ItemSection->GrantedEffects)
		{
			if (UClass* EffectClass = LoadClass<UGameplayEffect>(nullptr, *EffectPath.ToString()))
			{
				AbilitySet->GrantedEffects.Add(EffectClass);
			}
			else
			{
				UE_LOG(LogDefinitionGenerator, Warning, TEXT("[%s] Failed to load effect class: %s"),
					*ObjDef->ObjectId.ToString(), *EffectPath.ToString());
			}
		}

		// Save AbilitySet
		Package->MarkPackageDirty();
		const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

		if (UPackage::Save(Package, AbilitySet, *PackageFileName, SaveArgs).Result == ESavePackageResult::Success)
		{
			UE_LOG(LogDefinitionGenerator, Log, TEXT("[%s] Generated AbilitySet: %s"),
				*ObjDef->ObjectId.ToString(), *AbilitySetPath);
		}
		else
		{
			UE_LOG(LogDefinitionGenerator, Error, TEXT("[%s] Failed to save AbilitySet: %s"),
				*ObjDef->ObjectId.ToString(), *AbilitySetPath);
			return;
		}
	}

	// Set EquipAbilitySet path on item section
	ItemSection->EquipAbilitySet = FSoftObjectPath(AbilitySetPath + TEXT(".") + AbilitySetName);
}

FString UDefinitionGeneratorSubsystem::ComputeAbilitySetHash(const TArray<FSoftClassPath>& Abilities, const TArray<FSoftObjectPath>& Effects) const
{
	// Build sorted signature for deterministic hash
	TArray<FString> AbilityPaths;
	for (const FSoftClassPath& Path : Abilities)
	{
		AbilityPaths.Add(Path.ToString());
	}
	AbilityPaths.Sort();

	TArray<FString> EffectPaths;
	for (const FSoftObjectPath& Path : Effects)
	{
		EffectPaths.Add(Path.ToString());
	}
	EffectPaths.Sort();

	FString Signature = FString::Printf(TEXT("Abilities:%s|Effects:%s"),
		*FString::Join(AbilityPaths, TEXT(",")),
		*FString::Join(EffectPaths, TEXT(",")));

	return FMD5::HashAnsiString(*Signature);
}
