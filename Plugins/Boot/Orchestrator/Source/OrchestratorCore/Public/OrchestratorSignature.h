// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Signature verification result.
 */
struct ORCHESTRATORCORE_API FSignatureVerificationResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString SignerThumbprint;
	FString SignerName;
	bool bTrusted = false;

	FSignatureVerificationResult() = default;

	static FSignatureVerificationResult MakeSuccess(
		const FString& Thumbprint,
		const FString& SignerName,
		bool bIsTrusted)
	{
		FSignatureVerificationResult Result;
		Result.bSuccess = true;
		Result.SignerThumbprint = Thumbprint;
		Result.SignerName = SignerName;
		Result.bTrusted = bIsTrusted;
		return Result;
	}

	static FSignatureVerificationResult MakeFailure(const FString& Error)
	{
		FSignatureVerificationResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = Error;
		return Result;
	}
};

/**
 * Authenticode signature verification for Windows DLLs.
 * CRITICAL SECURITY COMPONENT - implements code signing verification.
 *
 * Per immutable_bootloader.md requirements:
 * - Verify Authenticode signatures on all DLLs before loading
 * - Check against signature_thumbprint allow-list from manifest
 * - Support cert rotation (allow N previous thumbprints)
 *
 * TODO: This is a placeholder implementation that needs Windows Crypto API integration.
 * Production implementation requires:
 * - WinVerifyTrust API for signature verification
 * - Certificate chain validation
 * - Thumbprint extraction and comparison
 * - Allow-list checking against manifest.signature_thumbprint
 */
class ORCHESTRATORCORE_API FOrchestratorSignature
{
public:
	/**
	 * Verify Authenticode signature on a DLL file.
	 *
	 * @param DllPath Absolute path to DLL file
	 * @param AllowedThumbprints List of allowed certificate thumbprints from manifest
	 * @return Verification result with thumbprint and trust info
	 *
	 * TODO: Implement using Windows Crypto API:
	 * - WinVerifyTrust with WINTRUST_ACTION_GENERIC_VERIFY_V2
	 * - Extract certificate thumbprint (SHA-1 hash of cert)
	 * - Verify thumbprint is in AllowedThumbprints list
	 * - Check certificate chain validity
	 * - Verify signature timestamp is valid
	 *
	 * Research notes:
	 * - Use WinTrust.h and Wincrypt.h APIs
	 * - CERT_CONTEXT to get certificate info
	 * - CryptBinaryToString to format thumbprint as hex
	 * - Handle cert revocation checking (optional but recommended)
	 *
	 * Example:
	 *   WINTRUST_FILE_INFO FileData = {};
	 *   FileData.pcwszFilePath = DllPathW.c_str();
	 *   WINTRUST_DATA WinTrustData = {};
	 *   WinTrustData.pFile = &FileData;
	 *   LONG Status = WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);
	 */
	static FSignatureVerificationResult VerifyDllSignature(
		const FString& DllPath,
		const TArray<FString>& AllowedThumbprints);

	/**
	 * Verify all DLL files in a plugin directory.
	 *
	 * @param PluginPath Path to plugin directory
	 * @param AllowedThumbprints List of allowed certificate thumbprints
	 * @param OutFailedDlls Receives list of DLLs that failed verification
	 * @return true if all DLLs verified successfully
	 *
	 * TODO: Implement recursive scan of Binaries/Win64/*.dll
	 * Verify each DLL and collect results
	 */
	static bool VerifyPluginDlls(
		const FString& PluginPath,
		const TArray<FString>& AllowedThumbprints,
		TArray<FString>& OutFailedDlls);

	/**
	 * Check if a DLL file is signed (without verifying trust chain).
	 *
	 * @param DllPath Absolute path to DLL
	 * @return true if DLL has any signature (trusted or not)
	 *
	 * TODO: Implement basic signature presence check
	 * Useful for diagnostic purposes
	 */
	static bool IsDllSigned(const FString& DllPath);

	/**
	 * Extract certificate thumbprint from a signed DLL.
	 *
	 * @param DllPath Absolute path to DLL
	 * @param OutThumbprint Receives thumbprint as hex string (e.g., "AB12CD34...")
	 * @return true if thumbprint extracted successfully
	 *
	 * TODO: Implement thumbprint extraction
	 * - Open signed DLL
	 * - Get certificate from signature
	 * - Compute SHA-1 hash of certificate
	 * - Format as uppercase hex string
	 */
	static bool GetDllThumbprint(
		const FString& DllPath,
		FString& OutThumbprint);

	/**
	 * Validate thumbprint format.
	 *
	 * @param Thumbprint Thumbprint string to validate
	 * @return true if thumbprint is valid hex string of correct length
	 *
	 * Thumbprint format: 40 hex characters (SHA-1 hash)
	 * Example: "1234567890ABCDEF1234567890ABCDEF12345678"
	 */
	static bool IsValidThumbprint(const FString& Thumbprint);
};
