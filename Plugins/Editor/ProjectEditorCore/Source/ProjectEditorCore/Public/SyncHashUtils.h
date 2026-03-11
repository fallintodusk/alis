// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Shared hash utilities for sync batch operations.
 *
 * Provides consistent hashing for file content to support sync batch ignore mechanisms.
 * Both publishers (e.g., JsonRefSync) and subscribers (e.g., DefinitionGenerator)
 * must use these same functions to ensure hash matching works correctly.
 */
class PROJECTEDITORCORE_API FSyncHashUtils
{
public:
	/**
	 * Compute normalized MD5 hash from string content.
	 *
	 * Normalization:
	 * - Line endings normalized to \n (handles Windows \r\n, Mac \r, Unix \n)
	 * - Content encoded as UTF-8 before hashing
	 * - MD5 digest returned as lowercase hex string
	 *
	 * @param Content The string content to hash
	 * @return 32-character lowercase hex MD5 hash, or empty string if content is empty
	 */
	static FString ComputeNormalizedHashFromString(const FString& Content);

	/**
	 * Compute normalized MD5 hash from file content.
	 *
	 * Loads file and applies same normalization as ComputeNormalizedHashFromString().
	 *
	 * @param FilePath Absolute path to the file
	 * @return 32-character lowercase hex MD5 hash, or empty string if file cannot be read
	 */
	static FString ComputeNormalizedHashFromFile(const FString& FilePath);

	/**
	 * Normalize file path for cross-platform comparison.
	 *
	 * Normalization:
	 * - Converts to absolute path
	 * - Uses forward slashes (/)
	 *
	 * @param Path Relative or absolute path
	 * @return Normalized absolute path with forward slashes
	 */
	static FString NormalizeFilePath(const FString& Path);
};
