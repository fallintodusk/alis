// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialTerrainLayoutCommandlet.h"

#include "ProjectMaterialTerrainLayoutContract.h"
#include "Utilities/ProjectSha256.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMaterialTerrainLayout, Log, All);

UProjectMaterialTerrainLayoutCommandlet::UProjectMaterialTerrainLayoutCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectMaterialTerrainLayoutCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);
	const FString ReceiptPath = Parameters.FindRef(TEXT("LayoutReceipt"));
	const FString ExpectedLayoutSha256 = Parameters.FindRef(TEXT("ExpectedLayoutSha256"));
	const FString ProbeMaterialPath = Parameters.FindRef(TEXT("ProbeMaterial"));
	const FString ProbeTwoSided = Parameters.FindRef(TEXT("ProbeTwoSided"));
	const FString ResultPath = FPaths::ConvertRelativePathToFull(Parameters.FindRef(TEXT("Result")));
	const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/Material")));
	if (ReceiptPath.IsEmpty() || ExpectedLayoutSha256.Len() != 64 ||
		ResultPath.IsEmpty() || !FPaths::IsUnderDirectory(ResultPath, EvidenceRoot))
	{
		UE_LOG(LogProjectMaterialTerrainLayout, Error,
			TEXT("Usage requires -LayoutReceipt=<json> -ExpectedLayoutSha256=<sha256> ")
			TEXT("-Result=<Saved/Validation/Material/json>."));
		return 2;
	}

	FProjectMaterialTerrainLayoutContract Contract;
	FString Error;
	bool bAccepted = FProjectMaterialTerrainLayoutCompiler::CompileFile(
		ReceiptPath, ExpectedLayoutSha256, Contract, Error);
	FString MaterialBeforeSha256;
	FString MaterialAfterSha256;
	if (bAccepted && !ProbeMaterialPath.IsEmpty())
	{
		const bool bProbeValueValid = ProbeTwoSided == TEXT("true") || ProbeTwoSided == TEXT("false");
		const FString PackageName = FPackageName::ObjectPathToPackageName(ProbeMaterialPath);
		const FString Filename = FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetAssetPackageExtension());
		UMaterial* Material = ProbeMaterialPath.StartsWith(TEXT("/ProjectWorldTestData/Generated/"))
			? LoadObject<UMaterial>(nullptr, *ProbeMaterialPath)
			: nullptr;
		if (!bProbeValueValid || Material == nullptr ||
			!FProjectSha256::HashFile(Filename, MaterialBeforeSha256))
		{
			bAccepted = false;
			Error = TEXT("ProjectMaterial same-path probe requires an existing test-owned material and boolean value.");
		}
		else
		{
			Material->TwoSided = ProbeTwoSided == TEXT("true");
			Material->MarkPackageDirty();
			FSavePackageArgs SaveArguments;
			SaveArguments.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArguments.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Material->GetPackage(), Material, *Filename, SaveArguments) ||
				!FProjectSha256::HashFile(Filename, MaterialAfterSha256) ||
				MaterialAfterSha256 == MaterialBeforeSha256)
			{
				bAccepted = false;
				Error = TEXT("ProjectMaterial same-path probe did not produce a distinct saved material package.");
			}
		}
	}
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("schema_version"), TEXT("1"));
	Result->SetStringField(TEXT("status"), bAccepted ? TEXT("accepted") : TEXT("rejected"));
	Result->SetStringField(TEXT("layout_sha256"), Contract.LayoutSha256);
	Result->SetStringField(TEXT("receipt_sha256"), Contract.ReceiptSha256);
	Result->SetStringField(TEXT("probe_material"), ProbeMaterialPath);
	Result->SetStringField(TEXT("material_before_sha256"), MaterialBeforeSha256);
	Result->SetStringField(TEXT("material_after_sha256"), MaterialAfterSha256);
	Result->SetStringField(TEXT("error"), Error);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Result, Writer);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true);
	if (!FFileHelper::SaveStringToFile(
		Json + LINE_TERMINATOR, *ResultPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogProjectMaterialTerrainLayout, Error, TEXT("Cannot write result: %s"), *ResultPath);
		return 3;
	}
	if (!bAccepted)
	{
		UE_LOG(LogProjectMaterialTerrainLayout, Error, TEXT("Layout receipt rejected: %s"), *Error);
		return 1;
	}
	UE_LOG(LogProjectMaterialTerrainLayout, Display,
		TEXT("MATERIAL_LAYOUT_ACCEPTED receipt_sha256=%s layout_sha256=%s"),
		*Contract.ReceiptSha256, *Contract.LayoutSha256);
	return 0;
}
