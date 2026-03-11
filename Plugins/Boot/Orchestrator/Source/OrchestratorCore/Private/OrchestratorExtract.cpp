// Copyright ALIS. All Rights Reserved.

#include "OrchestratorExtract.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryReader.h"

// Include libzip header
#include "libzip/zip.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrchestratorExtract, Log, All);

FExtractionResult FOrchestratorExtract::ExtractZip(
	const FString& ZipFilePath,
	const FString& DestinationPath,
	bool bOverwriteExisting)
{
	UE_LOG(LogOrchestratorExtract, Log, TEXT("Extracting zip: %s"), *ZipFilePath);
	UE_LOG(LogOrchestratorExtract, Log, TEXT("  Destination: %s"), *DestinationPath);

	const double StartTime = FPlatformTime::Seconds();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Verify zip file exists
	if (!PlatformFile.FileExists(*ZipFilePath))
	{
		const FString Error = FString::Printf(TEXT("Zip file not found: %s"), *ZipFilePath);
		UE_LOG(LogOrchestratorExtract, Error, TEXT("%s"), *Error);
		return FExtractionResult::MakeFailure(Error);
	}

	// Create destination directory
	if (!EnsureDirectoryExists(DestinationPath))
	{
		const FString Error = FString::Printf(TEXT("Failed to create destination directory: %s"), *DestinationPath);
		UE_LOG(LogOrchestratorExtract, Error, TEXT("%s"), *Error);
		return FExtractionResult::MakeFailure(Error);
	}

	// Open zip archive
	int ZipError = 0;
	auto ZipPathUtf8 = FTCHARToUTF8(*ZipFilePath);
	zip_t* ZipArchive = zip_open(ZipPathUtf8.Get(), ZIP_RDONLY, &ZipError);

	if (!ZipArchive)
	{
		zip_error_t Error;
		zip_error_init_with_code(&Error, ZipError);
		const FString ErrorMsg = FString::Printf(TEXT("Failed to open zip: %s"), UTF8_TO_TCHAR(zip_error_strerror(&Error)));
		zip_error_fini(&Error);
		UE_LOG(LogOrchestratorExtract, Error, TEXT("%s"), *ErrorMsg);
		return FExtractionResult::MakeFailure(ErrorMsg);
	}

	// Get number of entries
	const zip_int64_t NumEntries = zip_get_num_entries(ZipArchive, 0);
	if (NumEntries < 0)
	{
		zip_close(ZipArchive);
		return FExtractionResult::MakeFailure(TEXT("Failed to get number of entries in zip"));
	}

	UE_LOG(LogOrchestratorExtract, Log, TEXT("  Zip contains %lld entries"), NumEntries);

	TArray<FString> ExtractedFiles;
	int32 SuccessCount = 0;
	int32 SkippedCount = 0;

	// Extract each file
	for (zip_int64_t i = 0; i < NumEntries; ++i)
	{
		// Get file info
		struct zip_stat FileStat;
		zip_stat_init(&FileStat);

		if (zip_stat_index(ZipArchive, i, 0, &FileStat) != 0)
		{
			UE_LOG(LogOrchestratorExtract, Warning, TEXT("  Failed to get info for entry %lld"), i);
			continue;
		}

		// Skip directories
		const FString FileName = UTF8_TO_TCHAR(FileStat.name);
		if (FileName.EndsWith(TEXT("/")))
		{
			UE_LOG(LogOrchestratorExtract, Verbose, TEXT("  Skipping directory: %s"), *FileName);
			continue;
		}

		// Open file in archive
		zip_file_t* ZipFile = zip_fopen_index(ZipArchive, i, 0);
		if (!ZipFile)
		{
			UE_LOG(LogOrchestratorExtract, Warning, TEXT("  Failed to open entry: %s"), *FileName);
			continue;
		}

		// Read file data
		TArray<uint8> FileData;
		FileData.SetNum(FileStat.size);

		const zip_int64_t BytesRead = zip_fread(ZipFile, FileData.GetData(), FileStat.size);
		zip_fclose(ZipFile);

		if (BytesRead != (zip_int64_t)FileStat.size)
		{
			UE_LOG(LogOrchestratorExtract, Warning, TEXT("  Failed to read complete file: %s (read %lld of %lld bytes)"),
				*FileName, BytesRead, FileStat.size);
			continue;
		}

		// Extract file to destination
		if (ExtractFile(FileName, FileData, DestinationPath, bOverwriteExisting))
		{
			ExtractedFiles.Add(FileName);
			SuccessCount++;
		}
		else
		{
			SkippedCount++;
		}
	}

	// Close archive
	zip_close(ZipArchive);

	const float ExtractionTime = static_cast<float>(FPlatformTime::Seconds() - StartTime);
	UE_LOG(LogOrchestratorExtract, Log, TEXT("  Extraction complete: %d files extracted, %d skipped, %.2fs"),
		SuccessCount, SkippedCount, ExtractionTime);

	return FExtractionResult::MakeSuccess(ExtractedFiles, ExtractionTime);
}

FExtractionResult FOrchestratorExtract::ExtractZipSelective(
	const FString& ZipFilePath,
	const FString& DestinationPath,
	const TArray<FString>& FilesToExtract,
	bool bOverwriteExisting)
{
	UE_LOG(LogOrchestratorExtract, Log, TEXT("Extracting zip (selective): %s"), *ZipFilePath);
	UE_LOG(LogOrchestratorExtract, Log, TEXT("  Destination: %s"), *DestinationPath);
	UE_LOG(LogOrchestratorExtract, Log, TEXT("  Files to extract: %d"), FilesToExtract.Num());

	// If no specific files requested, extract all
	if (FilesToExtract.Num() == 0)
	{
		UE_LOG(LogOrchestratorExtract, Log, TEXT("  No specific files requested, extracting all"));
		return ExtractZip(ZipFilePath, DestinationPath, bOverwriteExisting);
	}

	const double StartTime = FPlatformTime::Seconds();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Verify zip file exists
	if (!PlatformFile.FileExists(*ZipFilePath))
	{
		return FExtractionResult::MakeFailure(FString::Printf(TEXT("Zip file not found: %s"), *ZipFilePath));
	}

	// Create destination directory
	if (!EnsureDirectoryExists(DestinationPath))
	{
		return FExtractionResult::MakeFailure(FString::Printf(TEXT("Failed to create destination directory: %s"), *DestinationPath));
	}

	// Open zip archive
	int ZipError = 0;
	auto ZipPathUtf8 = FTCHARToUTF8(*ZipFilePath);
	zip_t* ZipArchive = zip_open(ZipPathUtf8.Get(), ZIP_RDONLY, &ZipError);

	if (!ZipArchive)
	{
		return FExtractionResult::MakeFailure(TEXT("Failed to open zip archive"));
	}

	TArray<FString> ExtractedFiles;
	int32 SuccessCount = 0;

	// Extract specific files
	for (const FString& FileToExtract : FilesToExtract)
	{
		auto FileNameUtf8 = FTCHARToUTF8(*FileToExtract);
		const zip_int64_t FileIndex = zip_name_locate(ZipArchive, FileNameUtf8.Get(), 0);

		if (FileIndex < 0)
		{
			UE_LOG(LogOrchestratorExtract, Warning, TEXT("  File not found in archive: %s"), *FileToExtract);
			continue;
		}

		// Get file info
		struct zip_stat FileStat;
		zip_stat_init(&FileStat);

		if (zip_stat_index(ZipArchive, FileIndex, 0, &FileStat) != 0)
		{
			UE_LOG(LogOrchestratorExtract, Warning, TEXT("  Failed to get info for: %s"), *FileToExtract);
			continue;
		}

		// Open file
		zip_file_t* ZipFile = zip_fopen_index(ZipArchive, FileIndex, 0);
		if (!ZipFile)
		{
			UE_LOG(LogOrchestratorExtract, Warning, TEXT("  Failed to open: %s"), *FileToExtract);
			continue;
		}

		// Read file data
		TArray<uint8> FileData;
		FileData.SetNum(FileStat.size);

		const zip_int64_t BytesRead = zip_fread(ZipFile, FileData.GetData(), FileStat.size);
		zip_fclose(ZipFile);

		if (BytesRead == (zip_int64_t)FileStat.size)
		{
			if (ExtractFile(FileToExtract, FileData, DestinationPath, bOverwriteExisting))
			{
				ExtractedFiles.Add(FileToExtract);
				SuccessCount++;
			}
		}
	}

	zip_close(ZipArchive);

	const float ExtractionTime = static_cast<float>(FPlatformTime::Seconds() - StartTime);
	UE_LOG(LogOrchestratorExtract, Log, TEXT("  Selective extraction complete: %d files extracted, %.2fs"),
		SuccessCount, ExtractionTime);

	return FExtractionResult::MakeSuccess(ExtractedFiles, ExtractionTime);
}

bool FOrchestratorExtract::ListZipContents(
	const FString& ZipFilePath,
	TArray<FString>& OutFileList)
{
	UE_LOG(LogOrchestratorExtract, Log, TEXT("Listing zip contents: %s"), *ZipFilePath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.FileExists(*ZipFilePath))
	{
		UE_LOG(LogOrchestratorExtract, Error, TEXT("Zip file not found: %s"), *ZipFilePath);
		return false;
	}

	// Open zip archive
	int ZipError = 0;
	auto ZipPathUtf8 = FTCHARToUTF8(*ZipFilePath);
	zip_t* ZipArchive = zip_open(ZipPathUtf8.Get(), ZIP_RDONLY, &ZipError);

	if (!ZipArchive)
	{
		UE_LOG(LogOrchestratorExtract, Error, TEXT("Failed to open zip"));
		return false;
	}

	// Get number of entries
	const zip_int64_t NumEntries = zip_get_num_entries(ZipArchive, 0);
	if (NumEntries < 0)
	{
		zip_close(ZipArchive);
		return false;
	}

	OutFileList.Empty();

	// List all entries
	for (zip_int64_t i = 0; i < NumEntries; ++i)
	{
		struct zip_stat FileStat;
		zip_stat_init(&FileStat);

		if (zip_stat_index(ZipArchive, i, 0, &FileStat) == 0)
		{
			OutFileList.Add(UTF8_TO_TCHAR(FileStat.name));
		}
	}

	zip_close(ZipArchive);

	UE_LOG(LogOrchestratorExtract, Log, TEXT("  Found %d entries"), OutFileList.Num());
	return true;
}

bool FOrchestratorExtract::VerifyZipIntegrity(const FString& ZipFilePath)
{
	UE_LOG(LogOrchestratorExtract, Log, TEXT("Verifying zip integrity: %s"), *ZipFilePath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Basic check: file exists and has reasonable size
	if (!PlatformFile.FileExists(*ZipFilePath))
	{
		UE_LOG(LogOrchestratorExtract, Error, TEXT("Zip file not found"));
		return false;
	}

	const int64 FileSize = PlatformFile.FileSize(*ZipFilePath);
	if (FileSize <= 0)
	{
		UE_LOG(LogOrchestratorExtract, Error, TEXT("Zip file is empty or invalid"));
		return false;
	}

	// Try to open the zip to verify format
	int ZipError = 0;
	auto ZipPathUtf8 = FTCHARToUTF8(*ZipFilePath);
	zip_t* ZipArchive = zip_open(ZipPathUtf8.Get(), ZIP_RDONLY | ZIP_CHECKCONS, &ZipError);

	if (!ZipArchive)
	{
		zip_error_t Error;
		zip_error_init_with_code(&Error, ZipError);
		UE_LOG(LogOrchestratorExtract, Error, TEXT("Zip integrity check failed: %s"),
			UTF8_TO_TCHAR(zip_error_strerror(&Error)));
		zip_error_fini(&Error);
		return false;
	}

	// Get entry count as additional verification
	const zip_int64_t NumEntries = zip_get_num_entries(ZipArchive, 0);
	zip_close(ZipArchive);

	if (NumEntries < 0)
	{
		UE_LOG(LogOrchestratorExtract, Error, TEXT("Failed to read zip entries"));
		return false;
	}

	UE_LOG(LogOrchestratorExtract, Log, TEXT("Integrity check passed: %lld bytes, %lld entries"),
		FileSize, NumEntries);

	return true;
}

bool FOrchestratorExtract::EnsureDirectoryExists(const FString& DirectoryPath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.DirectoryExists(*DirectoryPath))
	{
		return true;
	}

	if (!PlatformFile.CreateDirectoryTree(*DirectoryPath))
	{
		UE_LOG(LogOrchestratorExtract, Error, TEXT("Failed to create directory: %s"), *DirectoryPath);
		return false;
	}

	UE_LOG(LogOrchestratorExtract, Log, TEXT("Created directory: %s"), *DirectoryPath);
	return true;
}

bool FOrchestratorExtract::ExtractFile(
	const FString& FileName,
	const TArray<uint8>& FileData,
	const FString& DestinationPath,
	bool bOverwriteExisting)
{
	const FString FullPath = FPaths::Combine(DestinationPath, FileName);
	UE_LOG(LogOrchestratorExtract, Verbose, TEXT("Extracting file: %s"), *FullPath);

	// Ensure directory exists
	const FString Directory = FPaths::GetPath(FullPath);
	if (!EnsureDirectoryExists(Directory))
	{
		return false;
	}

	// Check if file exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*FullPath) && !bOverwriteExisting)
	{
		UE_LOG(LogOrchestratorExtract, Verbose, TEXT("File exists and overwrite disabled: %s"), *FullPath);
		return false;
	}

	// Write file
	if (!FFileHelper::SaveArrayToFile(FileData, *FullPath))
	{
		UE_LOG(LogOrchestratorExtract, Error, TEXT("Failed to write file: %s"), *FullPath);
		return false;
	}

	UE_LOG(LogOrchestratorExtract, Verbose, TEXT("Extracted: %s (%d bytes)"), *FullPath, FileData.Num());
	return true;
}
