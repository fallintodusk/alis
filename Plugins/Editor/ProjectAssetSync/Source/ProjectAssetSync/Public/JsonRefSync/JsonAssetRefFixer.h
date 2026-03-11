// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Static helper for fixing JSON asset references.
 *
 * Scans JSON files and replaces old asset paths with new ones.
 * No persistent state - just pure logic.
 */
class PROJECTASSETSYNC_API FJsonAssetRefFixer
{
public:
	/**
	 * Scan all JSON files in given directories and fix asset references.
	 *
	 * @param PathMappings Map of OldPath -> NewPath to replace
	 * @param JsonRoots Root directories to scan for JSON files
	 * @param OutChangedFiles Optional list of files that were modified
	 * @param OutChangedFileHashes Optional map of file path -> content hash for modified files
	 * @return Number of references fixed
	 */
	static int32 FixReferences(
		const TMap<FString, FString>& PathMappings,
		const TArray<FString>& JsonRoots,
		TArray<FString>* OutChangedFiles = nullptr,
		TMap<FString, FString>* OutChangedFileHashes = nullptr);

	/**
	 * Normalize asset path to canonical form (without .AssetName suffix).
	 * Handles both "/Game/Foo/Bar" and "/Game/Foo/Bar.Bar" forms.
	 *
	 * @param Path Asset path to normalize
	 * @return Normalized path (package path without object name)
	 */
	static FString NormalizePath(const FString& Path);

	/**
	 * Check if a string value looks like an asset path.
	 *
	 * @param Value String value to check
	 * @return True if it looks like an asset reference
	 */
	static bool LooksLikeAssetPath(const FString& Value);

private:
	/**
	 * Fix references in a single JSON file.
	 *
	 * @param JsonFilePath Path to JSON file
	 * @param PathMappings Map of OldPath -> NewPath to replace
	 * @return Number of references fixed in this file
	 */
	static int32 FixReferencesInFile(
		const FString& JsonFilePath,
		const TMap<FString, FString>& PathMappings,
		FString* OutWrittenHash = nullptr);

	/**
	 * Recursively fix references in a JSON value.
	 *
	 * @param JsonValue JSON value to process (modified in place)
	 * @param PathMappings Map of OldPath -> NewPath to replace
	 * @return Number of references fixed
	 */
	static int32 FixReferencesInJsonValue(
		TSharedPtr<FJsonValue>& JsonValue,
		const TMap<FString, FString>& PathMappings);

	/**
	 * Checkout file from source control if needed.
	 *
	 * @param FilePath Path to file
	 * @return True if file can be modified
	 */
	static bool CheckoutFileIfNeeded(const FString& FilePath);
};
