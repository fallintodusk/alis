// Copyright ALIS. All Rights Reserved.

#include "OrchestratorSignature.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformFile.h"

// Windows Crypto API for Authenticode verification (Windows only)
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#endif

DEFINE_LOG_CATEGORY_STATIC(LogOrchestratorSignature, Log, All);

FSignatureVerificationResult FOrchestratorSignature::VerifyDllSignature(
	const FString& DllPath,
	const TArray<FString>& AllowedThumbprints)
{
	UE_LOG(LogOrchestratorSignature, Log, TEXT("Verifying DLL signature: %s"), *DllPath);
	UE_LOG(LogOrchestratorSignature, Log, TEXT("  Allowed thumbprints: %d"), AllowedThumbprints.Num());

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Verify DLL exists
	if (!PlatformFile.FileExists(*DllPath))
	{
		const FString Error = FString::Printf(TEXT("DLL not found: %s"), *DllPath);
		UE_LOG(LogOrchestratorSignature, Error, TEXT("%s"), *Error);
		return FSignatureVerificationResult::MakeFailure(Error);
	}

#if PLATFORM_WINDOWS
	// Windows Authenticode verification implementation

	// Convert UE FString to wide string for Windows API
	const WCHAR* DllPathW = *DllPath;

	// Step 1: Verify Authenticode signature using WinVerifyTrust
	WINTRUST_FILE_INFO FileData = {};
	FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
	FileData.pcwszFilePath = DllPathW;
	FileData.hFile = NULL;
	FileData.pgKnownSubject = NULL;

	GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	WINTRUST_DATA WinTrustData = {};
	WinTrustData.cbStruct = sizeof(WinTrustData);
	WinTrustData.dwUIChoice = WTD_UI_NONE;
	WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE; // No revocation check (offline-friendly)
	WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;
	WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
	WinTrustData.hWVTStateData = NULL;
	WinTrustData.pwszURLReference = NULL;
	WinTrustData.dwProvFlags = WTD_SAFER_FLAG;
	WinTrustData.pFile = &FileData;

	LONG Status = WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

	if (Status != ERROR_SUCCESS)
	{
		FString ErrorMsg;
		switch (Status)
		{
			case TRUST_E_NOSIGNATURE:
				ErrorMsg = TEXT("DLL is not signed");
				break;
			case TRUST_E_EXPLICIT_DISTRUST:
				ErrorMsg = TEXT("Signature is explicitly distrusted");
				break;
			case TRUST_E_SUBJECT_NOT_TRUSTED:
				ErrorMsg = TEXT("Signature is not trusted");
				break;
			case CRYPT_E_SECURITY_SETTINGS:
				ErrorMsg = TEXT("Security settings prevent verification");
				break;
			default:
				ErrorMsg = FString::Printf(TEXT("WinVerifyTrust failed: 0x%08X"), Status);
				break;
		}

		UE_LOG(LogOrchestratorSignature, Error, TEXT("  %s"), *ErrorMsg);

		// Close verification
		WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
		WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

		return FSignatureVerificationResult::MakeFailure(ErrorMsg);
	}

	UE_LOG(LogOrchestratorSignature, Log, TEXT("  ✓ Signature valid"));

	// Step 2: Extract certificate thumbprint
	FString Thumbprint;
	if (!GetDllThumbprint(DllPath, Thumbprint))
	{
		// Close verification
		WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
		WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

		return FSignatureVerificationResult::MakeFailure(TEXT("Failed to extract certificate thumbprint"));
	}

	// Close verification
	WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

	UE_LOG(LogOrchestratorSignature, Log, TEXT("  Thumbprint: %s"), *Thumbprint);

	// Step 3: Compare against allowed thumbprints
	if (AllowedThumbprints.Num() > 0)
	{
		bool bMatched = false;
		for (const FString& AllowedThumbprint : AllowedThumbprints)
		{
			if (Thumbprint.Equals(AllowedThumbprint, ESearchCase::IgnoreCase))
			{
				bMatched = true;
				break;
			}
		}

		if (!bMatched)
		{
			UE_LOG(LogOrchestratorSignature, Warning, TEXT("  Thumbprint not in allow-list"));
			return FSignatureVerificationResult::MakeSuccess(
				Thumbprint,
				TEXT("Signature valid but thumbprint not in allow-list"),
				false); // bTrusted = false
		}
	}

	UE_LOG(LogOrchestratorSignature, Log, TEXT("  ✓ Thumbprint verified"));
	return FSignatureVerificationResult::MakeSuccess(
		Thumbprint,
		TEXT("Signature verified and trusted"),
		true); // bTrusted = true

#else
	// Non-Windows platforms: Authenticode not applicable
	UE_LOG(LogOrchestratorSignature, Warning, TEXT("Authenticode verification only available on Windows"));
	return FSignatureVerificationResult::MakeSuccess(
		TEXT("PLATFORM_NOT_WINDOWS"),
		TEXT("Authenticode verification skipped (non-Windows platform)"),
		false); // bTrusted = false
#endif
}

bool FOrchestratorSignature::VerifyPluginDlls(
	const FString& PluginPath,
	const TArray<FString>& AllowedThumbprints,
	TArray<FString>& OutFailedDlls)
{
	UE_LOG(LogOrchestratorSignature, Log, TEXT("Verifying plugin DLLs: %s"), *PluginPath);

	OutFailedDlls.Empty();

	// Find all DLL files in Binaries/Win64
	const FString BinariesPath = PluginPath / TEXT("Binaries/Win64");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.DirectoryExists(*BinariesPath))
	{
		UE_LOG(LogOrchestratorSignature, Log, TEXT("  No Binaries directory found - content-only plugin"));
		return true; // Content-only plugin, no DLLs to verify
	}

	TArray<FString> DllFiles;
	IFileManager::Get().FindFilesRecursive(DllFiles, *BinariesPath, TEXT("*.dll"), true, false);

	if (DllFiles.Num() == 0)
	{
		UE_LOG(LogOrchestratorSignature, Log, TEXT("  No DLL files found"));
		return true;
	}

	UE_LOG(LogOrchestratorSignature, Log, TEXT("  Found %d DLL file(s)"), DllFiles.Num());

	int32 VerifiedCount = 0;
	int32 FailedCount = 0;

	for (const FString& DllPath : DllFiles)
	{
		UE_LOG(LogOrchestratorSignature, Log, TEXT("  Verifying: %s"), *FPaths::GetCleanFilename(DllPath));

		FSignatureVerificationResult Result = VerifyDllSignature(DllPath, AllowedThumbprints);

		if (!Result.bSuccess || !Result.bTrusted)
		{
			UE_LOG(LogOrchestratorSignature, Warning, TEXT("    Failed verification"));
			OutFailedDlls.Add(DllPath);
			FailedCount++;
		}
		else
		{
			UE_LOG(LogOrchestratorSignature, Log, TEXT("    ✓ Verified"));
			VerifiedCount++;
		}
	}

	UE_LOG(LogOrchestratorSignature, Log, TEXT("Verification complete: %d verified, %d failed"),
		VerifiedCount, FailedCount);

	// In Shipping builds, enforce strict verification (all DLLs must be signed and trusted)
	// In Development/Debug builds, allow untrusted DLLs (for internal testing)
#if UE_BUILD_SHIPPING
	if (OutFailedDlls.Num() > 0)
	{
		UE_LOG(LogOrchestratorSignature, Error, TEXT("CRITICAL: DLL verification failed in Shipping build"));
		UE_LOG(LogOrchestratorSignature, Error, TEXT("  %d DLL(s) failed verification"), OutFailedDlls.Num());
		return false;
	}
	return true;
#else
	// Development/Debug: Log warnings but allow loading
	if (OutFailedDlls.Num() > 0)
	{
		UE_LOG(LogOrchestratorSignature, Warning, TEXT("WARNING: %d DLL(s) failed verification (allowed in non-Shipping)"), OutFailedDlls.Num());
	}
	return true;
#endif
}

bool FOrchestratorSignature::IsDllSigned(const FString& DllPath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.FileExists(*DllPath))
	{
		return false;
	}

	// TODO: Implement signature presence check
	// Call WinVerifyTrust and check if signature exists
	// Return true even if signature is invalid (just checks presence)

	UE_LOG(LogOrchestratorSignature, Warning, TEXT("IsDllSigned not implemented - assuming unsigned"));
	return false;
}

bool FOrchestratorSignature::GetDllThumbprint(
	const FString& DllPath,
	FString& OutThumbprint)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.FileExists(*DllPath))
	{
		UE_LOG(LogOrchestratorSignature, Error, TEXT("DLL not found: %s"), *DllPath);
		return false;
	}

#if PLATFORM_WINDOWS
	// Windows implementation using Crypto API

	const WCHAR* DllPathW = *DllPath;

	HCERTSTORE hStore = NULL;
	HCRYPTMSG hMsg = NULL;
	PCCERT_CONTEXT pCertContext = NULL;

	// Query the file for certificate information
	DWORD dwEncoding, dwContentType, dwFormatType;
	if (!CryptQueryObject(
		CERT_QUERY_OBJECT_FILE,
		DllPathW,
		CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
		CERT_QUERY_FORMAT_FLAG_BINARY,
		0,
		&dwEncoding,
		&dwContentType,
		&dwFormatType,
		&hStore,
		&hMsg,
		NULL))
	{
		UE_LOG(LogOrchestratorSignature, Error, TEXT("CryptQueryObject failed: 0x%08X"), GetLastError());
		return false;
	}

	// Get the signer certificate from the message
	DWORD dwSignerInfo = 0;
	if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &dwSignerInfo))
	{
		UE_LOG(LogOrchestratorSignature, Error, TEXT("CryptMsgGetParam (size) failed: 0x%08X"), GetLastError());
		if (hStore) CertCloseStore(hStore, 0);
		if (hMsg) CryptMsgClose(hMsg);
		return false;
	}

	PCMSG_SIGNER_INFO pSignerInfo = (PCMSG_SIGNER_INFO)FMemory::Malloc(dwSignerInfo);
	if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, pSignerInfo, &dwSignerInfo))
	{
		UE_LOG(LogOrchestratorSignature, Error, TEXT("CryptMsgGetParam failed: 0x%08X"), GetLastError());
		FMemory::Free(pSignerInfo);
		if (hStore) CertCloseStore(hStore, 0);
		if (hMsg) CryptMsgClose(hMsg);
		return false;
	}

	// Find the signer's certificate in the store
	CERT_INFO CertInfo = {};
	CertInfo.Issuer = pSignerInfo->Issuer;
	CertInfo.SerialNumber = pSignerInfo->SerialNumber;

	pCertContext = CertFindCertificateInStore(
		hStore,
		dwEncoding,
		0,
		CERT_FIND_SUBJECT_CERT,
		&CertInfo,
		NULL);

	FMemory::Free(pSignerInfo);

	if (!pCertContext)
	{
		UE_LOG(LogOrchestratorSignature, Error, TEXT("CertFindCertificateInStore failed: 0x%08X"), GetLastError());
		if (hStore) CertCloseStore(hStore, 0);
		if (hMsg) CryptMsgClose(hMsg);
		return false;
	}

	// Get the SHA-1 thumbprint from the certificate
	BYTE ThumbprintBytes[20];
	DWORD ThumbprintSize = sizeof(ThumbprintBytes);

	if (!CertGetCertificateContextProperty(
		pCertContext,
		CERT_SHA1_HASH_PROP_ID,
		ThumbprintBytes,
		&ThumbprintSize))
	{
		UE_LOG(LogOrchestratorSignature, Error, TEXT("CertGetCertificateContextProperty failed: 0x%08X"), GetLastError());
		CertFreeCertificateContext(pCertContext);
		if (hStore) CertCloseStore(hStore, 0);
		if (hMsg) CryptMsgClose(hMsg);
		return false;
	}

	// Convert to hex string (uppercase)
	OutThumbprint.Empty();
	for (DWORD i = 0; i < ThumbprintSize; i++)
	{
		OutThumbprint += FString::Printf(TEXT("%02X"), ThumbprintBytes[i]);
	}

	// Cleanup
	CertFreeCertificateContext(pCertContext);
	if (hStore) CertCloseStore(hStore, 0);
	if (hMsg) CryptMsgClose(hMsg);

	return true;

#else
	// Non-Windows platforms
	UE_LOG(LogOrchestratorSignature, Warning, TEXT("GetDllThumbprint only available on Windows"));
	OutThumbprint = TEXT("PLATFORM_NOT_WINDOWS");
	return false;
#endif
}

bool FOrchestratorSignature::IsValidThumbprint(const FString& Thumbprint)
{
	// SHA-1 thumbprint is 40 hex characters
	if (Thumbprint.Len() != 40)
	{
		return false;
	}

	// Check all characters are hex
	for (TCHAR Char : Thumbprint)
	{
		if (!FChar::IsHexDigit(Char))
		{
			return false;
		}
	}

	return true;
}
