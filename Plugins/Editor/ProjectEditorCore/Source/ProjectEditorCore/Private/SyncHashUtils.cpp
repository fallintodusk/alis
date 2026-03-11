// Copyright ALIS. All Rights Reserved.

#include "SyncHashUtils.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Containers/StringConv.h"

FString FSyncHashUtils::ComputeNormalizedHashFromString(const FString& Content)
{
	if (Content.IsEmpty())
	{
		return FString();
	}

	// Normalize line endings to Unix style (\n)
	FString Normalized = Content;
	Normalized.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Normalized.ReplaceInline(TEXT("\r"), TEXT("\n"));

	// Convert to UTF-8 for consistent byte representation
	FTCHARToUTF8 Utf8String(*Normalized);

	// Compute MD5
	FMD5 Md5;
	Md5.Update(reinterpret_cast<const uint8*>(Utf8String.Get()), Utf8String.Length());

	uint8 Digest[16];
	Md5.Final(Digest);

	// Return lowercase hex string
	return FString::Printf(
		TEXT("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
		Digest[0], Digest[1], Digest[2], Digest[3],
		Digest[4], Digest[5], Digest[6], Digest[7],
		Digest[8], Digest[9], Digest[10], Digest[11],
		Digest[12], Digest[13], Digest[14], Digest[15]);
}

FString FSyncHashUtils::ComputeNormalizedHashFromFile(const FString& FilePath)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		return FString();
	}

	return ComputeNormalizedHashFromString(FileContent);
}

FString FSyncHashUtils::NormalizeFilePath(const FString& Path)
{
	FString Normalized = FPaths::ConvertRelativePathToFull(Path);
	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
	return Normalized;
}
