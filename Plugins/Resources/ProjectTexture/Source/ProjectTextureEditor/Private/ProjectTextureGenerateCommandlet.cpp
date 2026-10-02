// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureGenerateCommandlet.h"

#include "ProjectTextureGenerationService.h"
#include "ProjectTexturePatternRecipe.h"
#include "Algo/AllOf.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectTextureCommandlet, Log, All);

namespace ProjectTextureCommandletPrivate
{
bool WriteReceipt(
	const FString& ReceiptPath,
	const FString& OperationId,
	const FString& Status,
	const FProjectTextureGenerationResult& Result)
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
	TArray<TSharedPtr<FJsonValue>> Retained;
	for (const FString& Orphan : Result.RetainedOrphanPackageNames)
	{
		Retained.Add(MakeShared<FJsonValueString>(Orphan));
	}
	Root->SetArrayField(TEXT("retained_orphans"), Retained);
	const FString Authentication = FProjectTexturePatternRecipeContract::ComputeStringSha256(
		FString::Printf(
			TEXT("operation=%s|status=%s|manifest=%s|generated=%d|skipped=%d"),
			*OperationId, *Status, *Result.ManifestSha256, Result.Generated, Result.Skipped));
	Root->SetStringField(TEXT("authentication_sha256"), Authentication);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	Json.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReceiptPath), true);
	return FFileHelper::SaveStringToFile(
		Json + TEXT("\n"), *ReceiptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
}

UProjectTextureGenerateCommandlet::UProjectTextureGenerateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectTextureGenerateCommandlet::Main(const FString& Params)
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
		UE_LOG(LogProjectTextureCommandlet, Error,
			TEXT("[Commandlet::Main] Pattern regeneration requires the matching host transaction owner."));
		return 2;
	}

	FProjectTextureGenerationRequest Request;
	bool bRegisteredTestMount = false;
	FString RegisteredTestContentPath;
	if (!TestRoot.IsEmpty())
	{
		const FString AbsoluteTestRoot = FPaths::ConvertRelativePathToFull(TestRoot);
		const FString MountRoot = FPaths::Combine(AbsoluteTestRoot, TEXT("content")) + TEXT("/");
		RegisteredTestContentPath = MountRoot;
		FPackageName::RegisterMountPoint(TEXT("/ProjectTextureTest/"), RegisteredTestContentPath);
		bRegisteredTestMount = true;
		Request.RecipeRoot = FPaths::Combine(AbsoluteTestRoot, TEXT("recipes"));
		Request.OutputPackageRoot = TEXT("/ProjectTextureTest/Patterns");
		Request.OutputContentRoot = FPaths::Combine(MountRoot, TEXT("Patterns"));
		Request.ManifestRoot = FPaths::Combine(AbsoluteTestRoot, TEXT("manifests"));
	}
	else
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProjectTexture"));
		if (!Plugin.IsValid())
		{
			UE_LOG(LogProjectTextureCommandlet, Error,
				TEXT("[Commandlet::Main] ProjectTexture plugin could not be resolved."));
			return 2;
		}
		Request.RecipeRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Data/Patterns"));
		Request.OutputPackageRoot = TEXT("/ProjectTexture/Patterns");
		Request.OutputContentRoot = FPaths::Combine(Plugin->GetContentDir(), TEXT("Patterns"));
		Request.ManifestRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Data/Manifests/Patterns"));
	}
	Request.bCleanupOrphans = Switches.Contains(TEXT("cleanup"));
	Request.bHostTransactionOwnsReplacement = Mode == TEXT("regenerate") && HostTransaction == OperationId;
	Request.FailureInjection = Parameters.FindRef(TEXT("injectfailure"));

	FProjectTextureGenerationResult Result;
	const bool bAccepted = Mode == TEXT("validate")
		? FProjectTextureGenerationService::Validate(Request, Result)
		: FProjectTextureGenerationService::RegenerateForCommandlet(Request, Result);
	const FString Status = bAccepted ? TEXT("accepted") : TEXT("rejected");
	if (!ProjectTextureCommandletPrivate::WriteReceipt(ReceiptPath, OperationId, Status, Result))
	{
		UE_LOG(LogProjectTextureCommandlet, Error,
			TEXT("[Commandlet::Main] Pattern receipt could not be written - path=%s"), *ReceiptPath);
		if (bRegisteredTestMount)
		{
			FPackageName::UnRegisterMountPoint(TEXT("/ProjectTextureTest/"), RegisteredTestContentPath);
		}
		return 3;
	}
	if (bRegisteredTestMount)
	{
		FPackageName::UnRegisterMountPoint(TEXT("/ProjectTextureTest/"), RegisteredTestContentPath);
	}
	if (!bAccepted)
	{
		UE_LOG(LogProjectTextureCommandlet, Error,
			TEXT("[Commandlet::Main] Pattern operation rejected - error=%s"), *Result.Error);
		return 1;
	}
	UE_LOG(LogProjectTextureCommandlet, Display,
		TEXT("[Commandlet::Main] Pattern operation accepted - operation=%s generated=%d skipped=%d"),
		*OperationId, Result.Generated, Result.Skipped);
	return 0;
}
