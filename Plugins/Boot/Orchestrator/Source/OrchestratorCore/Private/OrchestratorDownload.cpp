// Copyright ALIS. All Rights Reserved.

#include "OrchestratorDownload.h"
#include "OrchestratorHash.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrchestratorDownload, Log, All);

FDownloadResult FOrchestratorDownload::DownloadFile(
	const FString& URL,
	const FString& DestinationPath,
	const FString& ExpectedHash)
{
	UE_LOG(LogOrchestratorDownload, Log, TEXT("Downloading: %s"), *URL);
	UE_LOG(LogOrchestratorDownload, Log, TEXT("  Destination: %s"), *DestinationPath);

	const double StartTime = FPlatformTime::Seconds();

	// Handle file:// URLs (dev mode - local files)
	if (URL.StartsWith(TEXT("file://")))
	{
		return HandleFileUrl(URL, DestinationPath, ExpectedHash);
	}

	// Create HTTP request
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(300.0f); // 5 minutes timeout

	// Process request synchronously (blocking)
	HttpRequest->ProcessRequest();

	// Wait for completion (blocking)
	while (HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		FPlatformProcess::Sleep(0.1f);
	}

	// Check if request succeeded
	if (HttpRequest->GetStatus() != EHttpRequestStatus::Succeeded)
	{
		const FString Error = FString::Printf(TEXT("HTTP request failed: %s"), *URL);
		UE_LOG(LogOrchestratorDownload, Error, TEXT("%s"), *Error);
		return FDownloadResult::MakeFailure(Error);
	}

	TSharedPtr<IHttpResponse> HttpResponse = HttpRequest->GetResponse();
	if (!HttpResponse.IsValid())
	{
		const FString Error = TEXT("Invalid HTTP response");
		UE_LOG(LogOrchestratorDownload, Error, TEXT("%s"), *Error);
		return FDownloadResult::MakeFailure(Error);
	}

	const int32 ResponseCode = HttpResponse->GetResponseCode();
	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		const FString Error = FString::Printf(TEXT("HTTP error %d: %s"), ResponseCode, *URL);
		UE_LOG(LogOrchestratorDownload, Error, TEXT("%s"), *Error);
		return FDownloadResult::MakeFailure(Error);
	}

	// Get downloaded data
	const TArray<uint8>& Content = HttpResponse->GetContent();
	UE_LOG(LogOrchestratorDownload, Log, TEXT("  Downloaded: %d bytes"), Content.Num());

	// Write to temp file first (atomic)
	if (!WriteDownloadedFile(Content, DestinationPath))
	{
		const FString Error = FString::Printf(TEXT("Failed to write file: %s"), *DestinationPath);
		UE_LOG(LogOrchestratorDownload, Error, TEXT("%s"), *Error);
		return FDownloadResult::MakeFailure(Error);
	}

	// Verify hash if provided
	if (!ExpectedHash.IsEmpty())
	{
		if (!VerifyDownloadedFile(DestinationPath, ExpectedHash))
		{
			// Delete invalid file
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			PlatformFile.DeleteFile(*DestinationPath);

			const FString Error = FString::Printf(TEXT("Hash verification failed: %s"), *DestinationPath);
			UE_LOG(LogOrchestratorDownload, Error, TEXT("%s"), *Error);
			return FDownloadResult::MakeFailure(Error);
		}
		UE_LOG(LogOrchestratorDownload, Log, TEXT("  Hash verified: %s"), *ExpectedHash);
	}

	const float DownloadTime = static_cast<float>(FPlatformTime::Seconds() - StartTime);
	UE_LOG(LogOrchestratorDownload, Log, TEXT("  Download complete: %.2f seconds"), DownloadTime);

	return FDownloadResult::MakeSuccess(DestinationPath, Content.Num(), DownloadTime);
}

bool FOrchestratorDownload::DownloadFileAsync(
	const FString& URL,
	const FString& DestinationPath,
	FDownloadProgressDelegate OnProgress,
	FDownloadCompleteDelegate OnComplete,
	const FString& ExpectedHash)
{
	UE_LOG(LogOrchestratorDownload, Log, TEXT("Starting async download: %s"), *URL);

	// Create HTTP request
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(300.0f);

	// Note: Progress callbacks not easily supported in synchronous mode
	// For progress tracking, use DownloadFileResumable and poll file size
	// or implement a fully async download manager

	// Bind completion callback
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[DestinationPath, ExpectedHash, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded)
		{
			if (!bSucceeded || !Response.IsValid())
			{
				UE_LOG(LogOrchestratorDownload, Error, TEXT("Async download failed"));
				OnComplete.Execute(FDownloadResult::MakeFailure(TEXT("HTTP request failed")));
				return;
			}

			const int32 ResponseCode = Response->GetResponseCode();
			if (ResponseCode < 200 || ResponseCode >= 300)
			{
				const FString Error = FString::Printf(TEXT("HTTP error %d"), ResponseCode);
				UE_LOG(LogOrchestratorDownload, Error, TEXT("%s"), *Error);
				OnComplete.Execute(FDownloadResult::MakeFailure(Error));
				return;
			}

			// Write file
			const TArray<uint8>& Content = Response->GetContent();
			if (!WriteDownloadedFile(Content, DestinationPath))
			{
				OnComplete.Execute(FDownloadResult::MakeFailure(TEXT("Failed to write file")));
				return;
			}

			// Verify hash
			if (!ExpectedHash.IsEmpty() && !VerifyDownloadedFile(DestinationPath, ExpectedHash))
			{
				// Delete invalid file
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				PlatformFile.DeleteFile(*DestinationPath);
				OnComplete.Execute(FDownloadResult::MakeFailure(TEXT("Hash verification failed")));
				return;
			}

			UE_LOG(LogOrchestratorDownload, Log, TEXT("Async download complete: %s"), *DestinationPath);
			OnComplete.Execute(FDownloadResult::MakeSuccess(DestinationPath, Content.Num(), 0.0f));
		});

	// Start request
	return HttpRequest->ProcessRequest();
}

FDownloadResult FOrchestratorDownload::DownloadFileResumable(
	const FString& URL,
	const FString& DestinationPath,
	int64 ExpectedSize,
	const FString& ExpectedHash)
{
	UE_LOG(LogOrchestratorDownload, Log, TEXT("Resumable download: %s"), *URL);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check if partial file exists
	int64 ExistingSize = 0;
	if (PlatformFile.FileExists(*DestinationPath))
	{
		ExistingSize = PlatformFile.FileSize(*DestinationPath);
		UE_LOG(LogOrchestratorDownload, Log, TEXT("  Existing partial file: %lld bytes"), ExistingSize);

		if (ExistingSize >= ExpectedSize)
		{
			// File already complete, verify hash
			if (!ExpectedHash.IsEmpty() && VerifyDownloadedFile(DestinationPath, ExpectedHash))
			{
				UE_LOG(LogOrchestratorDownload, Log, TEXT("  File already complete and verified"));
				return FDownloadResult::MakeSuccess(DestinationPath, ExistingSize, 0.0f);
			}
			// Hash failed, delete and re-download
			UE_LOG(LogOrchestratorDownload, Warning, TEXT("  Existing file failed verification, re-downloading"));
			PlatformFile.DeleteFile(*DestinationPath);
			ExistingSize = 0;
		}
	}

	// Create HTTP request with Range header for resume
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(300.0f);

	if (ExistingSize > 0)
	{
		// Request remaining bytes
		const FString RangeHeader = FString::Printf(TEXT("bytes=%lld-"), ExistingSize);
		HttpRequest->SetHeader(TEXT("Range"), RangeHeader);
		UE_LOG(LogOrchestratorDownload, Log, TEXT("  Resume from byte: %lld"), ExistingSize);
	}

	// Process request (synchronous)
	const double StartTime = FPlatformTime::Seconds();
	HttpRequest->ProcessRequest();

	while (HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		FPlatformProcess::Sleep(0.1f);
	}

	if (HttpRequest->GetStatus() != EHttpRequestStatus::Succeeded)
	{
		return FDownloadResult::MakeFailure(TEXT("HTTP request failed"));
	}

	TSharedPtr<IHttpResponse> HttpResponse = HttpRequest->GetResponse();
	if (!HttpResponse.IsValid())
	{
		return FDownloadResult::MakeFailure(TEXT("Invalid HTTP response"));
	}

	const int32 ResponseCode = HttpResponse->GetResponseCode();
	// Accept 200 (full) or 206 (partial) responses
	if (ResponseCode != 200 && ResponseCode != 206)
	{
		const FString Error = FString::Printf(TEXT("HTTP error %d"), ResponseCode);
		return FDownloadResult::MakeFailure(Error);
	}

	// Append or write downloaded data
	const TArray<uint8>& Content = HttpResponse->GetContent();
	if (ResponseCode == 206 && ExistingSize > 0)
	{
		// Append to existing file
		UE_LOG(LogOrchestratorDownload, Log, TEXT("  Appending %d bytes"), Content.Num());
		if (!FFileHelper::SaveArrayToFile(Content, *DestinationPath, &IFileManager::Get(), FILEWRITE_Append))
		{
			return FDownloadResult::MakeFailure(TEXT("Failed to append to file"));
		}
	}
	else
	{
		// Write full file
		UE_LOG(LogOrchestratorDownload, Log, TEXT("  Writing %d bytes"), Content.Num());
		if (!WriteDownloadedFile(Content, DestinationPath))
		{
			return FDownloadResult::MakeFailure(TEXT("Failed to write file"));
		}
	}

	// Verify hash
	if (!ExpectedHash.IsEmpty() && !VerifyDownloadedFile(DestinationPath, ExpectedHash))
	{
		PlatformFile.DeleteFile(*DestinationPath);
		return FDownloadResult::MakeFailure(TEXT("Hash verification failed"));
	}

	const float DownloadTime = static_cast<float>(FPlatformTime::Seconds() - StartTime);
	const int64 FinalSize = PlatformFile.FileSize(*DestinationPath);
	UE_LOG(LogOrchestratorDownload, Log, TEXT("  Resumable download complete: %.2f seconds, %lld bytes"),
		DownloadTime, FinalSize);

	return FDownloadResult::MakeSuccess(DestinationPath, FinalSize, DownloadTime);
}

bool FOrchestratorDownload::WriteDownloadedFile(const TArray<uint8>& Data, const FString& DestinationPath)
{
	// Write to temp file first (atomic operation)
	const FString TempPath = DestinationPath + TEXT(".tmp");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Ensure directory exists
	const FString Directory = FPaths::GetPath(DestinationPath);
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		if (!PlatformFile.CreateDirectoryTree(*Directory))
		{
			UE_LOG(LogOrchestratorDownload, Error, TEXT("Failed to create directory: %s"), *Directory);
			return false;
		}
	}

	// Write to temp file
	if (!FFileHelper::SaveArrayToFile(Data, *TempPath))
	{
		UE_LOG(LogOrchestratorDownload, Error, TEXT("Failed to write temp file: %s"), *TempPath);
		return false;
	}

	// Atomic rename
	if (PlatformFile.FileExists(*DestinationPath))
	{
		PlatformFile.DeleteFile(*DestinationPath);
	}

	if (!PlatformFile.MoveFile(*DestinationPath, *TempPath))
	{
		UE_LOG(LogOrchestratorDownload, Error, TEXT("Failed to rename temp file: %s -> %s"),
			*TempPath, *DestinationPath);
		PlatformFile.DeleteFile(*TempPath);
		return false;
	}

	return true;
}

FDownloadResult FOrchestratorDownload::HandleFileUrl(const FString& FileUrl, const FString& DestinationPath, const FString& ExpectedHash)
{
	// Parse file:// URL to get local path
	// Formats: file://./relative/path or file:///C:/absolute/path or file://C:/absolute/path
	FString LocalPath = FileUrl;

	// Remove file:// prefix
	LocalPath.RemoveFromStart(TEXT("file://"));

	// Handle relative paths (./path) - resolve against project root
	if (LocalPath.StartsWith(TEXT("./")))
	{
		LocalPath.RemoveFromStart(TEXT("./"));
		LocalPath = FPaths::Combine(FPaths::ProjectDir(), LocalPath);
	}
	// Handle Windows absolute paths with leading slash (/C:/path)
	else if (LocalPath.Len() > 2 && LocalPath[0] == TEXT('/') && LocalPath[2] == TEXT(':'))
	{
		LocalPath.RemoveFromStart(TEXT("/"));
	}

	// Normalize path separators
	FPaths::NormalizeDirectoryName(LocalPath);
	FPaths::CollapseRelativeDirectories(LocalPath);

	UE_LOG(LogOrchestratorDownload, Log, TEXT("  Dev mode: file:// URL resolved to: %s"), *LocalPath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check if path exists (can be directory or file)
	const bool bIsDirectory = PlatformFile.DirectoryExists(*LocalPath);
	const bool bIsFile = PlatformFile.FileExists(*LocalPath);

	if (!bIsDirectory && !bIsFile)
	{
		const FString Error = FString::Printf(TEXT("Dev mode: Local path not found: %s"), *LocalPath);
		UE_LOG(LogOrchestratorDownload, Error, TEXT("%s"), *Error);
		return FDownloadResult::MakeFailure(Error);
	}

	if (bIsDirectory)
	{
		UE_LOG(LogOrchestratorDownload, Log, TEXT("  Dev mode: Plugin directory found at: %s"), *LocalPath);
		UE_LOG(LogOrchestratorDownload, Log, TEXT("  Dev mode: Skipping download/extract - plugin already in place"));
		return FDownloadResult::MakeLocalDevMode(LocalPath);
	}

	// It's a file (zip) - copy it to destination
	UE_LOG(LogOrchestratorDownload, Log, TEXT("  Dev mode: Copying local file to: %s"), *DestinationPath);

	// Ensure destination directory exists
	const FString DestDir = FPaths::GetPath(DestinationPath);
	if (!PlatformFile.DirectoryExists(*DestDir))
	{
		PlatformFile.CreateDirectoryTree(*DestDir);
	}

	if (!PlatformFile.CopyFile(*DestinationPath, *LocalPath))
	{
		const FString Error = FString::Printf(TEXT("Dev mode: Failed to copy file from %s to %s"), *LocalPath, *DestinationPath);
		UE_LOG(LogOrchestratorDownload, Error, TEXT("%s"), *Error);
		return FDownloadResult::MakeFailure(Error);
	}

	// Verify hash if provided
	if (!ExpectedHash.IsEmpty() && !VerifyDownloadedFile(DestinationPath, ExpectedHash))
	{
		PlatformFile.DeleteFile(*DestinationPath);
		return FDownloadResult::MakeFailure(TEXT("Dev mode: Hash verification failed"));
	}

	const int64 FileSize = PlatformFile.FileSize(*DestinationPath);
	UE_LOG(LogOrchestratorDownload, Log, TEXT("  Dev mode: Copied %lld bytes"), FileSize);

	return FDownloadResult::MakeSuccess(DestinationPath, FileSize, 0.0f);
}

bool FOrchestratorDownload::VerifyDownloadedFile(const FString& FilePath, const FString& ExpectedHash)
{
	FString ComputedHash;
	if (!FOrchestratorHash::HashFile(FilePath, ComputedHash))
	{
		UE_LOG(LogOrchestratorDownload, Error, TEXT("Failed to compute hash: %s"), *FilePath);
		return false;
	}

	if (!FOrchestratorHash::VerifyHash(ComputedHash, ExpectedHash))
	{
		UE_LOG(LogOrchestratorDownload, Error, TEXT("Hash mismatch: expected %s, got %s"),
			*ExpectedHash, *ComputedHash);
		return false;
	}

	return true;
}
