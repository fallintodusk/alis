// Copyright ALIS. All Rights Reserved.

#include "OrchestratorHash.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "GenericPlatform/GenericPlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrchestratorHash, Log, All);

bool FOrchestratorHash::HashFile(const FString& FilePath, FString& OutHash)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check file exists
	if (!PlatformFile.FileExists(*FilePath))
	{
		UE_LOG(LogOrchestratorHash, Warning, TEXT("File not found: %s"), *FilePath);
		return false;
	}

	// Read file contents
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		UE_LOG(LogOrchestratorHash, Error, TEXT("Failed to read file: %s"), *FilePath);
		return false;
	}

	// Compute SHA-256
	return ComputeSHA256(FileData, OutHash);
}

bool FOrchestratorHash::HashDirectory(const FString& DirectoryPath, FString& OutHash)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check directory exists
	if (!PlatformFile.DirectoryExists(*DirectoryPath))
	{
		UE_LOG(LogOrchestratorHash, Warning, TEXT("Directory not found: %s"), *DirectoryPath);
		return false;
	}

	// Get all files recursively, sorted for deterministic hashing
	TArray<FString> Files;
	GetFilesRecursive(DirectoryPath, Files);

	if (Files.Num() == 0)
	{
		UE_LOG(LogOrchestratorHash, Warning, TEXT("No files found in directory: %s"), *DirectoryPath);
		// Empty directory -> hash of empty string
		OutHash = FMD5::HashAnsiString(TEXT(""));
		return true;
	}

	// Concatenate all file hashes
	FString CombinedHash;
	for (const FString& FilePath : Files)
	{
		FString FileHash;
		if (!HashFile(FilePath, FileHash))
		{
			UE_LOG(LogOrchestratorHash, Error, TEXT("Failed to hash file: %s"), *FilePath);
			return false;
		}
		CombinedHash += FileHash;
	}

	// Hash the combined hashes
	TArray<uint8> CombinedData;
	CombinedData.Append((uint8*)TCHAR_TO_UTF8(*CombinedHash), CombinedHash.Len());
	return ComputeSHA256(CombinedData, OutHash);
}

bool FOrchestratorHash::ComputeCodeHash(const FString& PluginRootPath, FString& OutHash)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check plugin root exists
	if (!PlatformFile.DirectoryExists(*PluginRootPath))
	{
		UE_LOG(LogOrchestratorHash, Warning, TEXT("Plugin root not found: %s"), *PluginRootPath);
		return false;
	}

	// Find .uplugin file
	TArray<FString> UpluginFiles;
	GetFilesRecursive(PluginRootPath, UpluginFiles, TEXT(".uplugin"));

	if (UpluginFiles.Num() == 0)
	{
		UE_LOG(LogOrchestratorHash, Error, TEXT("No .uplugin file found in: %s"), *PluginRootPath);
		return false;
	}

	// Hash .uplugin file
	FString UpluginHash;
	if (!HashFile(UpluginFiles[0], UpluginHash))
	{
		return false;
	}

	// Hash Binaries/ directory if it exists
	const FString BinariesPath = FPaths::Combine(PluginRootPath, TEXT("Binaries"));
	FString BinariesHash;

	if (PlatformFile.DirectoryExists(*BinariesPath))
	{
		if (!HashDirectory(BinariesPath, BinariesHash))
		{
			return false;
		}
	}
	else
	{
		// No binaries (content-only plugin)
		BinariesHash = FMD5::HashAnsiString(TEXT(""));
	}

	// Combine .uplugin hash + binaries hash
	FString CombinedHash = UpluginHash + BinariesHash;
	TArray<uint8> CombinedData;
	CombinedData.Append((uint8*)TCHAR_TO_UTF8(*CombinedHash), CombinedHash.Len());
	return ComputeSHA256(CombinedData, OutHash);
}

bool FOrchestratorHash::ComputeContentHash(const FString& PluginRootPath, FString& OutHash)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check plugin root exists
	if (!PlatformFile.DirectoryExists(*PluginRootPath))
	{
		UE_LOG(LogOrchestratorHash, Warning, TEXT("Plugin root not found: %s"), *PluginRootPath);
		return false;
	}

	// Get all .utoc and .ucas files
	TArray<FString> ContentFiles;
	GetFilesRecursive(PluginRootPath, ContentFiles, TEXT(".utoc"));
	GetFilesRecursive(PluginRootPath, ContentFiles, TEXT(".ucas"));

	if (ContentFiles.Num() == 0)
	{
		// No content files (code-only plugin or no IoStore packs)
		OutHash = FMD5::HashAnsiString(TEXT(""));
		return true;
	}

	// Sort for deterministic hashing
	ContentFiles.Sort();

	// Hash all content files
	FString CombinedHash;
	for (const FString& FilePath : ContentFiles)
	{
		FString FileHash;
		if (!HashFile(FilePath, FileHash))
		{
			UE_LOG(LogOrchestratorHash, Error, TEXT("Failed to hash content file: %s"), *FilePath);
			return false;
		}
		CombinedHash += FileHash;
	}

	// Hash the combined hashes
	TArray<uint8> CombinedData;
	CombinedData.Append((uint8*)TCHAR_TO_UTF8(*CombinedHash), CombinedHash.Len());
	return ComputeSHA256(CombinedData, OutHash);
}

bool FOrchestratorHash::VerifyHash(const FString& ComputedHash, const FString& ExpectedHash)
{
	// Case-insensitive comparison
	return ComputedHash.Equals(ExpectedHash, ESearchCase::IgnoreCase);
}

void FOrchestratorHash::GetFilesRecursive(const FString& DirectoryPath, TArray<FString>& OutFiles, const FString& Extension)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Visitor to collect files
	class FFileVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		TArray<FString>& Files;
		FString TargetExtension;

		FFileVisitor(TArray<FString>& InFiles, const FString& InExtension)
			: Files(InFiles), TargetExtension(InExtension) {}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (!bIsDirectory)
			{
				FString Filename(FilenameOrDirectory);
				if (TargetExtension.IsEmpty() || Filename.EndsWith(TargetExtension))
				{
					Files.Add(Filename);
				}
			}
			return true;
		}
	};

	FFileVisitor Visitor(OutFiles, Extension);
	PlatformFile.IterateDirectoryRecursively(*DirectoryPath, Visitor);

	// Sort for deterministic hashing
	OutFiles.Sort();
}

bool FOrchestratorHash::ComputeSHA256(const TArray<uint8>& Data, FString& OutHash)
{
	if (Data.Num() == 0)
	{
		// Empty data -> compute hash of empty string
		FSHA256Signature EmptySignature;
		if (!FGenericPlatformMisc::GetSHA256Signature(nullptr, 0, EmptySignature))
		{
			UE_LOG(LogOrchestratorHash, Error, TEXT("Failed to compute SHA-256 of empty data"));
			return false;
		}
		OutHash = EmptySignature.ToString();
		return true;
	}

	// Compute SHA-256 using UE's FGenericPlatformMisc
	FSHA256Signature Signature;
	if (!FGenericPlatformMisc::GetSHA256Signature(Data.GetData(), Data.Num(), Signature))
	{
		UE_LOG(LogOrchestratorHash, Error, TEXT("Failed to compute SHA-256"));
		return false;
	}

	// Convert to hex string
	OutHash = Signature.ToString();
	return true;
}
