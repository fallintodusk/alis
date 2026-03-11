// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

/**
 * Download result information.
 */
struct ORCHESTRATORCORE_API FDownloadResult
{
	bool bSuccess = false;
	bool bIsLocalDevMode = false;  // True if file:// URL - skip extraction, plugin already in place
	FString ErrorMessage;
	FString DownloadedFilePath;
	FString LocalPluginPath;       // For dev mode: actual plugin directory path
	int64 BytesDownloaded = 0;
	float DownloadTimeSeconds = 0.0f;

	FDownloadResult() = default;

	static FDownloadResult MakeSuccess(const FString& FilePath, int64 Bytes, float Time)
	{
		FDownloadResult Result;
		Result.bSuccess = true;
		Result.DownloadedFilePath = FilePath;
		Result.BytesDownloaded = Bytes;
		Result.DownloadTimeSeconds = Time;
		return Result;
	}

	static FDownloadResult MakeLocalDevMode(const FString& LocalPath)
	{
		FDownloadResult Result;
		Result.bSuccess = true;
		Result.bIsLocalDevMode = true;
		Result.LocalPluginPath = LocalPath;
		return Result;
	}

	static FDownloadResult MakeFailure(const FString& Error)
	{
		FDownloadResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = Error;
		return Result;
	}
};

/**
 * Download progress callback.
 * @param BytesReceived Bytes downloaded so far
 * @param TotalBytes Total bytes to download (may be 0 if unknown)
 */
DECLARE_DELEGATE_TwoParams(FDownloadProgressDelegate, int64 /*BytesReceived*/, int64 /*TotalBytes*/);

/**
 * Download completion callback.
 * @param Result Download result information
 */
DECLARE_DELEGATE_OneParam(FDownloadCompleteDelegate, const FDownloadResult& /*Result*/);

/**
 * HTTP download utility for Orchestrator.
 * Provides resumable, verified downloads with progress tracking.
 */
class ORCHESTRATORCORE_API FOrchestratorDownload
{
public:
	/**
	 * Download a file from URL to local path (synchronous, blocking).
	 * @param URL Source URL
	 * @param DestinationPath Absolute path to save file
	 * @param ExpectedHash Optional SHA-256 hash to verify after download
	 * @return Download result
	 */
	static FDownloadResult DownloadFile(
		const FString& URL,
		const FString& DestinationPath,
		const FString& ExpectedHash = TEXT(""));

	/**
	 * Download a file asynchronously (non-blocking).
	 * @param URL Source URL
	 * @param DestinationPath Absolute path to save file
	 * @param OnProgress Progress callback (optional)
	 * @param OnComplete Completion callback (required)
	 * @param ExpectedHash Optional SHA-256 hash to verify after download
	 * @return true if download started successfully
	 */
	static bool DownloadFileAsync(
		const FString& URL,
		const FString& DestinationPath,
		FDownloadProgressDelegate OnProgress,
		FDownloadCompleteDelegate OnComplete,
		const FString& ExpectedHash = TEXT(""));

	/**
	 * Download with resume support (partial download).
	 * Checks if file partially exists and requests remaining bytes via Range header.
	 * @param URL Source URL
	 * @param DestinationPath Absolute path to save file
	 * @param ExpectedSize Expected final file size (for resume validation)
	 * @param ExpectedHash Expected SHA-256 hash
	 * @return Download result
	 */
	static FDownloadResult DownloadFileResumable(
		const FString& URL,
		const FString& DestinationPath,
		int64 ExpectedSize,
		const FString& ExpectedHash);

private:
	/** Helper: Write downloaded data to file (atomic temp + rename) */
	static bool WriteDownloadedFile(const TArray<uint8>& Data, const FString& DestinationPath);

	/** Helper: Verify downloaded file hash */
	static bool VerifyDownloadedFile(const FString& FilePath, const FString& ExpectedHash);

	/** Helper: Handle file:// URLs (dev mode - local plugins) */
	static FDownloadResult HandleFileUrl(const FString& FileUrl, const FString& DestinationPath, const FString& ExpectedHash);
};
