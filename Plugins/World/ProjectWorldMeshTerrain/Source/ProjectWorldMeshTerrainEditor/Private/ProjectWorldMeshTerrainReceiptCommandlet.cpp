// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldMeshTerrainReceiptCommandlet.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldMeshTerrainLayoutReceipt.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldMeshTerrainReceipt, Log, All);

UProjectWorldMeshTerrainReceiptCommandlet::UProjectWorldMeshTerrainReceiptCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectWorldMeshTerrainReceiptCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);
	const FString* CompileResult = Parameters.Find(TEXT("CompileResult"));
	const FString* Result = Parameters.Find(TEXT("Result"));
	if (CompileResult == nullptr || Result == nullptr)
	{
		UE_LOG(LogProjectWorldMeshTerrainReceipt, Error,
			TEXT("Usage requires -CompileResult=<canonical-compile-result> -Result=<saved-validation-json>."));
		return 2;
	}
	const FString ResultPath = FPaths::ConvertRelativePathToFull(*Result);
	const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization")));
	if (!FPaths::IsUnderDirectory(ResultPath, EvidenceRoot))
	{
		UE_LOG(LogProjectWorldMeshTerrainReceipt, Error, TEXT("Unsafe result path: %s"), *ResultPath);
		return 2;
	}
	IFileManager::Get().Delete(*ResultPath, false, true);

	FProjectWorldCanonicalBundle Bundle;
	FProjectWorldCanonicalValidation Validation;
	if (!FProjectWorldCanonicalLoader::Load(*CompileResult, Bundle, Validation))
	{
		UE_LOG(LogProjectWorldMeshTerrainReceipt, Error,
			TEXT("Canonical load rejected: %s (%s)"), *Validation.Message, *Validation.Detail);
		return 3;
	}
	FProjectWorldMeshTerrainLayoutReceipt Receipt;
	FString Error;
	if (!FProjectWorldMeshTerrainLayoutReceiptContract::Build(Bundle, Receipt, Error) ||
		!FProjectWorldMeshTerrainLayoutReceiptContract::Save(Receipt, ResultPath, Error))
	{
		UE_LOG(LogProjectWorldMeshTerrainReceipt, Error, TEXT("Receipt rejected: %s"), *Error);
		return 4;
	}
	UE_LOG(LogProjectWorldMeshTerrainReceipt, Display,
		TEXT("LAYOUT_RECEIPT_ACCEPTED receipt_sha256=%s layout_sha256=%s result=%s"),
		*Receipt.ReceiptSha256, *Receipt.LayoutSha256, *ResultPath);
	return 0;
}
