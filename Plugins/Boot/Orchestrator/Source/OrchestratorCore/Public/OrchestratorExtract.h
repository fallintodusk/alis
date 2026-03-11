// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Extraction result information.
 */
struct ORCHESTRATORCORE_API FExtractionResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	TArray<FString> ExtractedFiles;
	int32 FilesExtracted = 0;
	float ExtractionTimeSeconds = 0.0f;

	FExtractionResult() = default;

	static FExtractionResult MakeSuccess(const TArray<FString>& Files, float Time)
	{
		FExtractionResult Result;
		Result.bSuccess = true;
		Result.ExtractedFiles = Files;
		Result.FilesExtracted = Files.Num();
		Result.ExtractionTimeSeconds = Time;
		return Result;
	}

	static FExtractionResult MakeFailure(const FString& Error)
	{
		FExtractionResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = Error;
		return Result;
	}
};

/**
 * File extraction utility for Orchestrator.
 * Handles zip file extraction for downloaded plugin packages.
 */
class ORCHESTRATORCORE_API FOrchestratorExtract
{
public:
	/**
	 * Extract a zip file to destination directory.
	 * @param ZipFilePath Path to zip file
	 * @param DestinationPath Directory to extract to (will be created if needed)
	 * @param bOverwriteExisting If true, overwrites existing files
	 * @return Extraction result
	 */
	static FExtractionResult ExtractZip(
		const FString& ZipFilePath,
		const FString& DestinationPath,
		bool bOverwriteExisting = true);

	/**
	 * Extract specific files from a zip archive.
	 * @param ZipFilePath Path to zip file
	 * @param DestinationPath Base directory for extraction
	 * @param FilesToExtract Array of file paths within zip to extract (empty = all)
	 * @param bOverwriteExisting If true, overwrites existing files
	 * @return Extraction result
	 */
	static FExtractionResult ExtractZipSelective(
		const FString& ZipFilePath,
		const FString& DestinationPath,
		const TArray<FString>& FilesToExtract,
		bool bOverwriteExisting = true);

	/**
	 * List contents of a zip file without extracting.
	 * @param ZipFilePath Path to zip file
	 * @param OutFileList List of files in the archive
	 * @return true if successful
	 */
	static bool ListZipContents(
		const FString& ZipFilePath,
		TArray<FString>& OutFileList);

	/**
	 * Verify zip file integrity.
	 * @param ZipFilePath Path to zip file
	 * @return true if zip is valid and readable
	 */
	static bool VerifyZipIntegrity(const FString& ZipFilePath);

private:
	/** Helper: Create directory tree for file path */
	static bool EnsureDirectoryExists(const FString& DirectoryPath);

	/** Helper: Extract single file from zip data */
	static bool ExtractFile(
		const FString& FileName,
		const TArray<uint8>& FileData,
		const FString& DestinationPath,
		bool bOverwriteExisting);
};
