// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialGenerateCommandlet.h"

#include "ProjectMaterialGenerationService.h"
#include "ProjectMaterialRecipe.h"
#include "Algo/AllOf.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMaterialCommandlet, Log, All);

namespace ProjectMaterialCommandletPrivate
{
bool WriteReceipt(
	const FString& ReceiptPath,
	const FString& OperationId,
	const FString& Status,
	const FProjectMaterialGenerationResult& Result)
{
	if (ReceiptPath.IsEmpty())
	{
		return false;
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema_version"), TEXT("1"));
	Root->SetStringField(TEXT("operation_id"), OperationId);
	Root->SetStringField(TEXT("status"), Status);
	Root->SetNumberField(TEXT("validated"), Result.Validated);
	Root->SetNumberField(TEXT("generated"), Result.Generated);
	Root->SetNumberField(TEXT("skipped"), Result.Skipped);
	Root->SetNumberField(TEXT("shader_compiles"), Result.ShaderCompiles);
	Root->SetStringField(TEXT("manifest_sha256"), Result.ManifestSha256);
	Root->SetStringField(TEXT("error"), Result.Error);
	TArray<TSharedPtr<FJsonValue>> Outputs;
	for (const FString& Output : Result.OutputObjectPaths)
	{
		Outputs.Add(MakeShared<FJsonValueString>(Output));
	}
	Root->SetArrayField(TEXT("outputs"), Outputs);
	TArray<TSharedPtr<FJsonValue>> Orphans;
	for (const FString& Orphan : Result.OrphanPackageNames)
	{
		Orphans.Add(MakeShared<FJsonValueString>(Orphan));
	}
	Root->SetArrayField(TEXT("orphans"), Orphans);
	const FString Authentication = FProjectMaterialRecipeContract::ComputeStringSha256(FString::Printf(
		TEXT("operation=%s|status=%s|manifest=%s|generated=%d|skipped=%d"),
		*OperationId, *Status, *Result.ManifestSha256, Result.Generated, Result.Skipped));
	Root->SetStringField(TEXT("authentication_sha256"), Authentication);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReceiptPath), true);
	return FFileHelper::SaveStringToFile(
		Json + LINE_TERMINATOR,
		*ReceiptPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
}

UProjectMaterialGenerateCommandlet::UProjectMaterialGenerateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectMaterialGenerateCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);
	const FString OperationId = Parameters.FindRef(TEXT("operation"));
	const FString ReceiptPath = Parameters.FindRef(TEXT("receipt"));
	const FString TestRoot = Parameters.FindRef(TEXT("testroot"));
	const FString Mode = Parameters.FindRef(TEXT("mode"));
	const FString HostTransaction = Parameters.FindRef(TEXT("hosttransaction"));
	if (OperationId.Len() != 32 || !Algo::AllOf(OperationId, [](TCHAR Character)
		{
			return FChar::IsHexDigit(Character) && !FChar::IsUpper(Character);
		}) || ReceiptPath.IsEmpty() ||
		(Mode != TEXT("validate") && Mode != TEXT("regenerate")) ||
		(Mode == TEXT("regenerate") && HostTransaction != OperationId))
	{
		UE_LOG(LogProjectMaterialCommandlet, Error,
			TEXT("[Commandlet::Main] Regeneration requires the matching host transaction owner."));
		return 2;
	}
	FProjectMaterialGenerationRequest Request;
	bool bRegisteredTestMount = false;
	FString RegisteredTestContentPath;
	if (!TestRoot.IsEmpty())
	{
		const FString AbsoluteTestRoot = FPaths::ConvertRelativePathToFull(TestRoot);
		const FString MountRoot = FPaths::Combine(AbsoluteTestRoot, TEXT("content")) + TEXT("/");
		RegisteredTestContentPath = MountRoot;
		FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"), RegisteredTestContentPath);
		bRegisteredTestMount = true;
		Request.RecipeRoot = FPaths::Combine(AbsoluteTestRoot, TEXT("recipes"));
		Request.OutputPackageRoot = TEXT("/ProjectMaterialTest/Generated");
		Request.OutputContentRoot = FPaths::Combine(MountRoot, TEXT("Generated"));
		Request.ManifestRoot = FPaths::Combine(AbsoluteTestRoot, TEXT("manifests"));
	}
	else
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProjectMaterial"));
		if (!Plugin.IsValid())
		{
			UE_LOG(LogProjectMaterialCommandlet, Error,
				TEXT("[Commandlet::Main] ProjectMaterial plugin could not be resolved."));
			return 2;
		}
		Request.RecipeRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Data/Materials"));
		Request.OutputPackageRoot = TEXT("/ProjectMaterial/Generated");
		Request.OutputContentRoot = FPaths::Combine(Plugin->GetContentDir(), TEXT("Generated"));
		Request.ManifestRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Data/Manifests/Materials"));
	}
	Request.bCleanupOrphans = Switches.Contains(TEXT("cleanup"));
	Request.bHostTransactionOwnsReplacement = Mode == TEXT("regenerate") && HostTransaction == OperationId;
	Request.FailureInjection = Parameters.FindRef(TEXT("injectfailure"));
	FProjectMaterialGenerationResult Result;
	const bool bAccepted = Mode == TEXT("validate")
		? FProjectMaterialGenerationService::Validate(Request, Result)
		: FProjectMaterialGenerationService::RegenerateForCommandlet(Request, Result);
	const FString Status = bAccepted ? TEXT("accepted") : TEXT("rejected");
	if (!ProjectMaterialCommandletPrivate::WriteReceipt(
		ReceiptPath, OperationId, Status, Result))
	{
		UE_LOG(LogProjectMaterialCommandlet, Error,
			TEXT("[Commandlet::Main] Receipt could not be written - path=%s"), *ReceiptPath);
		if (bRegisteredTestMount)
		{
			FPackageName::UnRegisterMountPoint(TEXT("/ProjectMaterialTest/"), RegisteredTestContentPath);
		}
		return 3;
	}
	if (bRegisteredTestMount)
	{
		FPackageName::UnRegisterMountPoint(TEXT("/ProjectMaterialTest/"), RegisteredTestContentPath);
	}
	if (!bAccepted)
	{
		UE_LOG(LogProjectMaterialCommandlet, Error,
			TEXT("[Commandlet::Main] Material operation rejected - error=%s"), *Result.Error);
		return 1;
	}
	UE_LOG(LogProjectMaterialCommandlet, Display,
		TEXT("[Commandlet::Main] Material operation accepted - operation=%s generated=%d skipped=%d"),
		*OperationId, Result.Generated, Result.Skipped);
	return 0;
}
