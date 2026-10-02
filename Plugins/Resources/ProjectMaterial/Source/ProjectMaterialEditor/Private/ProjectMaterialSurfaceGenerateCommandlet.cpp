// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceGenerateCommandlet.h"

#include "ProjectMaterialCompilerIdentity.h"
#include "ProjectMaterialSurfaceGenerationService.h"
#include "Algo/AllOf.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMaterialSurfaceCommandlet, Log, All);

namespace ProjectMaterialSurfaceCommandletPrivate
{
bool WriteReceipt(
	const FString& ReceiptPath,
	const FString& OperationId,
	const FString& Status,
	const FProjectMaterialSurfaceGenerationResult& Result)
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
	TArray<TSharedPtr<FJsonValue>> Retained;
	for (const FString& Orphan : Result.RetainedOrphanPackageNames)
	{
		Retained.Add(MakeShared<FJsonValueString>(Orphan));
	}
	Root->SetArrayField(TEXT("retained_orphans"), Retained);
	const FString Authentication = FProjectMaterialCompilerIdentity::ComputeStringSha256(FString::Printf(
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

UProjectMaterialSurfaceGenerateCommandlet::UProjectMaterialSurfaceGenerateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectMaterialSurfaceGenerateCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);
	const FString OperationId = Parameters.FindRef(TEXT("operation"));
	const FString ReceiptPath = Parameters.FindRef(TEXT("receipt"));
	const FString TestRoot = Parameters.FindRef(TEXT("testroot"));
	const FString PatternTestRoot = Parameters.FindRef(TEXT("patterntestroot"));
	const FString Mode = Parameters.FindRef(TEXT("mode"));
	const FString HostTransaction = Parameters.FindRef(TEXT("hosttransaction"));
	if (OperationId.Len() != 32 || !Algo::AllOf(OperationId, [](TCHAR Character)
		{
			return FChar::IsHexDigit(Character) && !FChar::IsUpper(Character);
		}) || ReceiptPath.IsEmpty() ||
		(Mode != TEXT("validate") && Mode != TEXT("regenerate")) ||
		(Mode == TEXT("regenerate") && HostTransaction != OperationId))
	{
		UE_LOG(LogProjectMaterialSurfaceCommandlet, Error,
			TEXT("[Commandlet::Main] Surface regeneration requires the matching host transaction owner."));
		return 2;
	}
	if (!PatternTestRoot.IsEmpty() && TestRoot.IsEmpty())
	{
		UE_LOG(LogProjectMaterialSurfaceCommandlet, Error,
			TEXT("[Commandlet::Main] A pattern test root is admitted only for a surface test root."));
		return 2;
	}

	const TSharedPtr<IPlugin> MaterialPlugin = IPluginManager::Get().FindPlugin(TEXT("ProjectMaterial"));
	const TSharedPtr<IPlugin> TexturePlugin = IPluginManager::Get().FindPlugin(TEXT("ProjectTexture"));
	if (!MaterialPlugin.IsValid() || !TexturePlugin.IsValid())
	{
		UE_LOG(LogProjectMaterialSurfaceCommandlet, Error,
			TEXT("[Commandlet::Main] ProjectMaterial or ProjectTexture plugin could not be resolved."));
		return 2;
	}

	FProjectMaterialSurfaceGenerationRequest Request;
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
		Request.OutputPackageRoot = TEXT("/ProjectMaterialTest/Surfaces");
		Request.OutputContentRoot = FPaths::Combine(MountRoot, TEXT("Surfaces"));
		Request.ManifestRoot = FPaths::Combine(AbsoluteTestRoot, TEXT("manifests"));
	}
	else
	{
		Request.RecipeRoot = FPaths::Combine(MaterialPlugin->GetBaseDir(), TEXT("Data/SurfaceRecipes"));
		Request.OutputPackageRoot = TEXT("/ProjectMaterial/Surfaces");
		Request.OutputContentRoot = FPaths::Combine(MaterialPlugin->GetContentDir(), TEXT("Surfaces"));
		Request.ManifestRoot = FPaths::Combine(MaterialPlugin->GetBaseDir(), TEXT("Data/Manifests/Surfaces"));
	}
	FString RegisteredPatternContentPath;
	if (!PatternTestRoot.IsEmpty())
	{
		// A texture test mount lets the surface test consume a pattern the texture test regenerated.
		const FString AbsolutePatternRoot = FPaths::ConvertRelativePathToFull(PatternTestRoot);
		RegisteredPatternContentPath = FPaths::Combine(AbsolutePatternRoot, TEXT("content")) + TEXT("/");
		FPackageName::RegisterMountPoint(TEXT("/ProjectTextureTest/"), RegisteredPatternContentPath);
		Request.PatternManifestPath = FPaths::Combine(
			AbsolutePatternRoot, TEXT("manifests/accepted.pattern-manifest.json"));
		Request.PatternPackageRoot = TEXT("/ProjectTextureTest");
		Request.PatternContentRoot = RegisteredPatternContentPath;
	}
	else
	{
		Request.PatternManifestPath = FPaths::Combine(
			TexturePlugin->GetBaseDir(), TEXT("Data/Manifests/Patterns/accepted.pattern-manifest.json"));
		Request.PatternContentRoot = TexturePlugin->GetContentDir();
	}
	ON_SCOPE_EXIT
	{
		if (!RegisteredPatternContentPath.IsEmpty())
		{
			FPackageName::UnRegisterMountPoint(TEXT("/ProjectTextureTest/"), RegisteredPatternContentPath);
		}
	};
	Request.TerrainLayoutReceiptPath = Parameters.FindRef(TEXT("layoutreceipt"));
	Request.bCleanupOrphans = Switches.Contains(TEXT("cleanup"));
	Request.bHostTransactionOwnsReplacement = Mode == TEXT("regenerate") && HostTransaction == OperationId;
	Request.FailureInjection = Parameters.FindRef(TEXT("injectfailure"));

	FProjectMaterialSurfaceGenerationResult Result;
	const bool bAccepted = Mode == TEXT("validate")
		? FProjectMaterialSurfaceGenerationService::Validate(Request, Result)
		: FProjectMaterialSurfaceGenerationService::RegenerateForCommandlet(Request, Result);
	const FString Status = bAccepted ? TEXT("accepted") : TEXT("rejected");
	if (!ProjectMaterialSurfaceCommandletPrivate::WriteReceipt(
		ReceiptPath, OperationId, Status, Result))
	{
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
		UE_LOG(LogProjectMaterialSurfaceCommandlet, Error,
			TEXT("[Commandlet::Main] Surface operation rejected - error=%s"), *Result.Error);
		return 1;
	}
	UE_LOG(LogProjectMaterialSurfaceCommandlet, Display,
		TEXT("[Commandlet::Main] Surface operation accepted - operation=%s generated=%d skipped=%d"),
		*OperationId, Result.Generated, Result.Skipped);
	return 0;
}
