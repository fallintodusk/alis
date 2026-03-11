// Copyright ALIS. All Rights Reserved.

#include "JsonRefSync/JsonAssetRefFixer.h"
#include "JsonRefSync/SortedJsonSerializerPolicy.h"

#include "SyncHashUtils.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SourceControlOperations.h"
#include "Containers/StringConv.h"

DEFINE_LOG_CATEGORY_STATIC(LogJsonAssetRefFixer, Log, All);

int32 FJsonAssetRefFixer::FixReferences(
	const TMap<FString, FString>& PathMappings,
	const TArray<FString>& JsonRoots,
	TArray<FString>* OutChangedFiles,
	TMap<FString, FString>* OutChangedFileHashes)
{
	if (PathMappings.Num() == 0 || JsonRoots.Num() == 0)
	{
		return 0;
	}

	int32 TotalFixed = 0;

	if (OutChangedFiles)
	{
		OutChangedFiles->Reset();
	}
	if (OutChangedFileHashes)
	{
		OutChangedFileHashes->Reset();
	}

	// Build a set of normalized old paths for quick substring check
	TSet<FString> OldPathSubstrings;
	for (const auto& Pair : PathMappings)
	{
		// Add both normalized and original form for substring matching
		FString Normalized = NormalizePath(Pair.Key);
		OldPathSubstrings.Add(Normalized);
		OldPathSubstrings.Add(Pair.Key);

		UE_LOG(LogJsonAssetRefFixer, Log, TEXT("Searching for path: %s (normalized: %s) -> %s"),
			*Pair.Key, *Normalized, *Pair.Value);
	}

	// Scan all JSON files
	for (const FString& RootDir : JsonRoots)
	{
		TArray<FString> JsonFiles;
		IFileManager::Get().FindFilesRecursive(JsonFiles, *RootDir, TEXT("*.json"), true, false);

		UE_LOG(LogJsonAssetRefFixer, Log, TEXT("Scanning %d JSON files in: %s"), JsonFiles.Num(), *RootDir);

		for (const FString& JsonFile : JsonFiles)
		{
			// Quick check: read file as string and check if any old path exists
			FString FileContent;
			if (!FFileHelper::LoadFileToString(FileContent, *JsonFile))
			{
				continue;
			}

			bool bMayContainRef = false;
			FString MatchedPath;
			for (const FString& OldPath : OldPathSubstrings)
			{
				if (FileContent.Contains(OldPath))
				{
					bMayContainRef = true;
					MatchedPath = OldPath;
					break;
				}
			}

			if (!bMayContainRef)
			{
				continue;
			}

			UE_LOG(LogJsonAssetRefFixer, Log, TEXT("File contains path '%s': %s"), *MatchedPath, *JsonFile);

			// File may contain references - do proper JSON parsing
			FString WrittenHash;
			int32 FixedInFile = FixReferencesInFile(
				JsonFile,
				PathMappings,
				OutChangedFileHashes ? &WrittenHash : nullptr);
			TotalFixed += FixedInFile;

			if (FixedInFile > 0)
			{
				if (OutChangedFiles)
				{
					OutChangedFiles->Add(JsonFile);
				}
				if (OutChangedFileHashes && !WrittenHash.IsEmpty())
				{
					OutChangedFileHashes->Add(JsonFile, WrittenHash);
				}
				UE_LOG(LogJsonAssetRefFixer, Log, TEXT("Fixed %d references in: %s"), FixedInFile, *JsonFile);
			}
		}
	}

	return TotalFixed;
}

int32 FJsonAssetRefFixer::FixReferencesInFile(
	const FString& JsonFilePath,
	const TMap<FString, FString>& PathMappings,
	FString* OutWrittenHash)
{
	// Read JSON file
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		UE_LOG(LogJsonAssetRefFixer, Warning, TEXT("Failed to read JSON file: %s"), *JsonFilePath);
		return 0;
	}

	// Parse JSON
	TSharedPtr<FJsonValue> JsonRoot;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonRoot) || !JsonRoot.IsValid())
	{
		UE_LOG(LogJsonAssetRefFixer, Warning, TEXT("Failed to parse JSON file: %s"), *JsonFilePath);
		return 0;
	}

	// Fix references recursively
	int32 FixedCount = FixReferencesInJsonValue(JsonRoot, PathMappings);

	if (FixedCount == 0)
	{
		return 0;
	}

	// Checkout from source control if needed
	if (!CheckoutFileIfNeeded(JsonFilePath))
	{
		UE_LOG(LogJsonAssetRefFixer, Warning, TEXT("Failed to checkout file: %s"), *JsonFilePath);
		return 0;
	}

	// Write back to file - handle both object and array roots
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);

	// Use FSortedJsonSerializer for deterministic key ordering (alphabetical)
	// This ensures git diff shows only changed lines, not entire file reordering
	bool bSerializeSuccess = false;
	if (JsonRoot->Type == EJson::Object)
	{
		TSharedPtr<FJsonObject> RootObject = JsonRoot->AsObject();
		if (RootObject.IsValid())
		{
			bSerializeSuccess = FSortedJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
		}
	}
	else if (JsonRoot->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& RootArray = JsonRoot->AsArray();
		bSerializeSuccess = FSortedJsonSerializer::Serialize(RootArray, Writer);
	}
	else
	{
		UE_LOG(LogJsonAssetRefFixer, Warning, TEXT("JSON root is neither object nor array: %s"), *JsonFilePath);
		return 0;
	}

	if (!bSerializeSuccess)
	{
		UE_LOG(LogJsonAssetRefFixer, Warning, TEXT("Failed to serialize JSON: %s"), *JsonFilePath);
		return 0;
	}

	if (!FFileHelper::SaveStringToFile(OutputString, *JsonFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogJsonAssetRefFixer, Warning, TEXT("Failed to write JSON file: %s"), *JsonFilePath);
		return 0;
	}

	if (OutWrittenHash)
	{
		*OutWrittenHash = FSyncHashUtils::ComputeNormalizedHashFromString(OutputString);
	}

	return FixedCount;
}

int32 FJsonAssetRefFixer::FixReferencesInJsonValue(
	TSharedPtr<FJsonValue>& JsonValue,
	const TMap<FString, FString>& PathMappings)
{
	if (!JsonValue.IsValid())
	{
		return 0;
	}

	int32 FixedCount = 0;

	switch (JsonValue->Type)
	{
	case EJson::String:
		{
			FString StringValue = JsonValue->AsString();

			// Check if this looks like an asset path
			if (LooksLikeAssetPath(StringValue))
			{
				FString NormalizedValue = NormalizePath(StringValue);

				UE_LOG(LogJsonAssetRefFixer, Verbose, TEXT("Checking asset path value: '%s' (normalized: '%s')"),
					*StringValue, *NormalizedValue);

				// Check all mappings
				for (const auto& Pair : PathMappings)
				{
					FString NormalizedOld = NormalizePath(Pair.Key);
					FString NormalizedNew = NormalizePath(Pair.Value);

					// Check for exact match (normalized)
					if (NormalizedValue == NormalizedOld)
					{
						// Preserve original format (with or without .AssetName)
						FString NewValue;
						if (StringValue.Contains(TEXT(".")))
						{
							// Original had .AssetName suffix - add it to new path
							FString AssetName = FPaths::GetBaseFilename(NormalizedNew);
							NewValue = NormalizedNew + TEXT(".") + AssetName;
						}
						else
						{
							NewValue = NormalizedNew;
						}

						JsonValue = MakeShared<FJsonValueString>(NewValue);
						FixedCount++;

						UE_LOG(LogJsonAssetRefFixer, Verbose, TEXT("  Replaced: %s -> %s"), *StringValue, *NewValue);
						break;
					}
				}
			}
		}
		break;

	case EJson::Array:
		{
			TArray<TSharedPtr<FJsonValue>>& Array = const_cast<TArray<TSharedPtr<FJsonValue>>&>(JsonValue->AsArray());
			for (TSharedPtr<FJsonValue>& Element : Array)
			{
				FixedCount += FixReferencesInJsonValue(Element, PathMappings);
			}
		}
		break;

	case EJson::Object:
		{
			TSharedPtr<FJsonObject> Object = JsonValue->AsObject();
			if (Object.IsValid())
			{
				for (auto& Pair : Object->Values)
				{
					FixedCount += FixReferencesInJsonValue(Pair.Value, PathMappings);
				}
			}
		}
		break;

	default:
		// Number, Boolean, Null - nothing to do
		break;
	}

	return FixedCount;
}

FString FJsonAssetRefFixer::NormalizePath(const FString& Path)
{
	// Remove .AssetName suffix if present
	// "/Game/Foo/Bar.Bar" -> "/Game/Foo/Bar"
	int32 DotIndex;
	if (Path.FindLastChar(TEXT('.'), DotIndex))
	{
		// Check if there's a slash after the dot (meaning it's part of extension, not object name)
		int32 LastSlash;
		if (Path.FindLastChar(TEXT('/'), LastSlash) && DotIndex > LastSlash)
		{
			// Dot is after last slash - it's the object name separator
			return Path.Left(DotIndex);
		}
	}

	return Path;
}

bool FJsonAssetRefFixer::LooksLikeAssetPath(const FString& Value)
{
	// Treat any string starting with / as potential asset path
	// Exact matching in FixReferencesInJsonValue prevents false positives
	return !Value.IsEmpty() && Value[0] == TEXT('/');
}

bool FJsonAssetRefFixer::CheckoutFileIfNeeded(const FString& FilePath)
{
	ISourceControlModule& SCCModule = ISourceControlModule::Get();
	ISourceControlProvider& SCCProvider = SCCModule.GetProvider();

	if (!SCCProvider.IsEnabled())
	{
		// Source control not enabled - file can be modified
		return true;
	}

	// Check if file is already writable
	if (!IFileManager::Get().IsReadOnly(*FilePath))
	{
		return true;
	}

	// Try to checkout
	TArray<FString> Files;
	Files.Add(FilePath);

	FSourceControlStatePtr FileState = SCCProvider.GetState(FilePath, EStateCacheUsage::Use);
	if (FileState.IsValid() && FileState->CanCheckout())
	{
		ECommandResult::Type Result = SCCProvider.Execute(
			ISourceControlOperation::Create<FCheckOut>(),
			Files
		);

		if (Result != ECommandResult::Succeeded)
		{
			UE_LOG(LogJsonAssetRefFixer, Warning, TEXT("Source control checkout failed for: %s"), *FilePath);
			return false;
		}
	}

	return true;
}
