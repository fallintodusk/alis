// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Hash verification utilities for Orchestrator.
 * Provides SHA-256 hashing for code and content verification.
 */
class ORCHESTRATORCORE_API FOrchestratorHash
{
public:
	/**
	 * Compute SHA-256 hash of a file.
	 * @param FilePath Absolute path to file
	 * @param OutHash Hex-encoded SHA-256 hash (64 characters)
	 * @return true if hash computed successfully
	 */
	static bool HashFile(const FString& FilePath, FString& OutHash);

	/**
	 * Compute SHA-256 hash of a directory's contents.
	 * Recursively hashes all files in the directory.
	 * @param DirectoryPath Absolute path to directory
	 * @param OutHash Hex-encoded SHA-256 hash
	 * @return true if hash computed successfully
	 */
	static bool HashDirectory(const FString& DirectoryPath, FString& OutHash);

	/**
	 * Compute code_hash for a plugin.
	 * Hashes .uplugin file + all files in Binaries/ directory.
	 * @param PluginRootPath Path to plugin root (contains .uplugin)
	 * @param OutHash Hex-encoded SHA-256 hash
	 * @return true if hash computed successfully
	 */
	static bool ComputeCodeHash(const FString& PluginRootPath, FString& OutHash);

	/**
	 * Compute content_hash for a plugin.
	 * Hashes all .utoc and .ucas files in plugin directory.
	 * @param PluginRootPath Path to plugin root
	 * @param OutHash Hex-encoded SHA-256 hash
	 * @return true if hash computed successfully
	 */
	static bool ComputeContentHash(const FString& PluginRootPath, FString& OutHash);

	/**
	 * Verify hash matches expected value (case-insensitive comparison).
	 * @param ComputedHash Hash computed from file/directory
	 * @param ExpectedHash Expected hash from manifest
	 * @return true if hashes match
	 */
	static bool VerifyHash(const FString& ComputedHash, const FString& ExpectedHash);

private:
	/** Helper: Get all files in directory recursively, sorted for deterministic hashing */
	static void GetFilesRecursive(const FString& DirectoryPath, TArray<FString>& OutFiles, const FString& Extension = TEXT(""));

	/** Helper: Compute SHA-256 of buffer */
	static bool ComputeSHA256(const TArray<uint8>& Data, FString& OutHash);
};
