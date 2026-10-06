// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldTwinVerifyCommandlet.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldPipelineVerifyProjection.h"
#include "ProjectWorldVerify.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldTwinVerify, Log, All);

namespace ProjectWorldTwinVerifyCommandlet
{
	bool AddProfileFixture(FProjectWorldVerifyIdentity& Identity, const FString& Path, FString& OutError)
	{
		FString Relative = FPaths::ConvertRelativePathToFull(Path);
		const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		if (!FPaths::MakePathRelativeTo(Relative, *ProjectRoot) || Relative.StartsWith(TEXT("..")))
		{
			OutError = TEXT("Twin verify profile is outside the project.");
			return false;
		}
		Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Identity.AddFixtureFile(Relative, OutError);
	}

	bool AddTwinInputs(FProjectWorldVerifyIdentity& Identity,
		const FProjectWorldCanonicalBundle& Bundle, const TArray<FString>& Profiles,
		FString& OutError)
	{
		for (const FString& Profile : Profiles)
		{
			if (!AddProfileFixture(Identity, Profile, OutError))
			{
				return false;
			}
		}
		for (const TCHAR* Path : {
			TEXT("Plugins/World/ProjectWorldTestData/Data/Profiles/SourceIngestion/synthetic_territory_twin.source.json"),
			TEXT("Plugins/World/ProjectWorldTestData/Data/Profiles/CanonicalCompilation/synthetic_territory_twin.compile.json"),
			TEXT("Plugins/World/ProjectWorldTestData/Data/Fixtures/Provider/synthetic_territory_twin/synthetic_provider.json"),
			TEXT("Plugins/World/ProjectWorldTestData/Data/Fixtures/CanonicalCompilation/synthetic_territory_twin/authored_overlay.json"),
			TEXT("Plugins/World/ProjectWorldTestData/Data/GameplayPlacement/synthetic_territory_twin.json"),
			TEXT("Plugins/World/ProjectWorldTestData/Content/Authored/Fixtures/L_ProjectWorldMarker.umap")})
		{
			if (!Identity.AddFixtureFile(Path, OutError))
			{
				return false;
			}
		}
		Identity.AddFixtureText(TEXT("canonical:grid"), Bundle.GridId);
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			Identity.AddFixtureText(TEXT("canonical:terrain:") + Cell.CellId, Cell.Terrain.ArtifactHash);
			Identity.AddFixtureText(TEXT("canonical:features:") + Cell.CellId, Cell.FeatureArtifactHash);
		}
		return true;
	}

	bool WriteResult(const FString& Path, bool bAccepted, const FString& MapMessage,
		const FString& TerrainMessage, const FString& PipelineMessage)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("status"), bAccepted ? TEXT("accepted") : TEXT("rejected"));
		Root->SetStringField(TEXT("map"), MapMessage);
		Root->SetStringField(TEXT("terrain"), TerrainMessage);
		Root->SetStringField(TEXT("pipeline"), PipelineMessage);
		FString Json;
		if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)))
		{
			return false;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Json, *Path,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool RunProjection(const FString& Verify, const FString& GeneratorId,
		const FProjectWorldVerifyScope& Scope, const FProjectWorldCanonicalBundle& Bundle,
		const TArray<FString>& Profiles, ProjectWorldVerify::EMode Mode,
		const FString& ProbeDir, const TMap<FString, FString>& PackageDigests,
		FString& OutMessage, bool& bOutRecorded)
	{
		bOutRecorded = false;
		int32 ComparisonVersion = 0;
		FProjectWorldProjectFn Project;
		FString Error;
		if (!ProjectWorldVerify::FindProjection(GeneratorId, 1, ComparisonVersion, Project))
		{
			OutMessage = TEXT("No registered projection for ") + GeneratorId;
			return false;
		}
		FProjectWorldVerifyIdentity Identity;
		if (!FProjectWorldVerifyIdentity::ForProducer(
			Verify, GeneratorId, 1, ComparisonVersion, Identity, Error))
		{
			OutMessage = Error;
			return false;
		}
		if (!AddTwinInputs(Identity, Bundle, Profiles, Error))
		{
			OutMessage = Error;
			return false;
		}
		FProjectWorldProjection Projection;
		if (!Project(Scope, Projection, Error) || !Projection.IsValid(Error) || Projection.IsEmpty())
		{
			OutMessage = Error.IsEmpty() ? TEXT("Twin projection is empty.") : Error;
			return false;
		}
		const ProjectWorldVerify::FResult Result = Mode == ProjectWorldVerify::EMode::Probe
			? ProjectWorldVerify::WriteProbe(Identity, Projection, PackageDigests, ProbeDir)
			: ProjectWorldVerify::CompareOrRecord(Identity, Projection, Mode);
		bOutRecorded = Result.bPassed && Result.Message.StartsWith(TEXT("Recorded "));
		if (Mode == ProjectWorldVerify::EMode::Record && !Result.bPassed &&
			Result.Message.Contains(TEXT("record refused: identity unchanged; nothing to record")))
		{
			const ProjectWorldVerify::FResult Compared = ProjectWorldVerify::CompareOrRecord(
				Identity, Projection, ProjectWorldVerify::EMode::Compare);
			OutMessage = Compared.Message;
			return Compared.bPassed;
		}
		OutMessage = Result.Message;
		return Result.bPassed;
	}
}

UProjectWorldTwinVerifyCommandlet::UProjectWorldTwinVerifyCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectWorldTwinVerifyCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);
	const FString* Map = Parameters.Find(TEXT("Map"));
	const FString* CompileResult = Parameters.Find(TEXT("CompileResult"));
	const FString* RealizationProfile = Parameters.Find(TEXT("RealizationProfile"));
	const FString* PresentationProfile = Parameters.Find(TEXT("PresentationProfile"));
	const FString* RuntimeProfile = Parameters.Find(TEXT("RuntimeProfile"));
	const FString* AuthoredOverlayProfile = Parameters.Find(TEXT("AuthoredOverlayProfile"));
	const FString* ManifestRoot = Parameters.Find(TEXT("ManifestRoot"));
	const FString* ResultPath = Parameters.Find(TEXT("Result"));
	if (Map == nullptr || CompileResult == nullptr || RealizationProfile == nullptr ||
		PresentationProfile == nullptr || RuntimeProfile == nullptr || ManifestRoot == nullptr ||
		AuthoredOverlayProfile == nullptr || ResultPath == nullptr ||
		!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(*ResultPath),
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization"))) ||
		!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(*ManifestRoot),
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization"))))
	{
		UE_LOG(LogProjectWorldTwinVerify, Error, TEXT("Twin verify requires safe map, compile result, profiles and result paths."));
		return 2;
	}
	FProjectWorldCanonicalBundle Bundle;
	FProjectWorldCanonicalValidation Validation;
	if (!FProjectWorldCanonicalLoader::Load(*CompileResult, Bundle, Validation))
	{
		UE_LOG(LogProjectWorldTwinVerify, Error, TEXT("Canonical twin could not be loaded: %s"), *Validation.Message);
		return 3;
	}
	FProjectWorldRealizationProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRealizationProfile::Load(*RealizationProfile, Profile, ErrorCode, Error) ||
		Profile.MapPackagePath != *Map)
	{
		UE_LOG(LogProjectWorldTwinVerify, Error, TEXT("Twin realization profile does not match the map: %s"), *Error);
		return 3;
	}
	const FString MapFilename = FPackageName::LongPackageNameToFilename(
		*Map, FPackageName::GetMapPackageExtension());
	TArray<FString> ExternalActors;
	IFileManager::Get().FindFilesRecursive(ExternalActors,
		*FPackageName::LongPackageNameToFilename(ULevel::GetExternalActorsPath(*Map)),
		TEXT("*.uasset"), true, false);
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
		.Get().ScanFilesSynchronous(ExternalActors, true);
	if (!FEditorFileUtils::LoadMap(MapFilename, false, false))
	{
		UE_LOG(LogProjectWorldTwinVerify, Error, TEXT("Twin map cannot be loaded: %s"), **Map);
		return 4;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World == nullptr || World->GetWorldPartition() == nullptr)
	{
		return 4;
	}
	TArray<FWorldPartitionReference> References;
	World->GetWorldPartition()->LoadAllActors(References);
	FAssetCompilingManager::Get().FinishAllCompilation();
	const FProjectWorldRealizationLayer* Terrain = Profile.Layers.FindByPredicate([](const auto& Layer)
	{
		return Layer.LayerId == TEXT("terrain");
	});
	if (Terrain == nullptr)
	{
		return 4;
	}
	FString ProbeDir;
	const ProjectWorldVerify::EMode Mode = ProjectWorldVerify::ModeFromCommandLine(ProbeDir);
	if (Mode == ProjectWorldVerify::EMode::Probe &&
		!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(ProbeDir),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world/realization_verify"))))
	{
		UE_LOG(LogProjectWorldTwinVerify, Error, TEXT("Twin probe path must be under tmp/world/realization_verify."));
		return 2;
	}
	const TArray<FString> Profiles = {
		*RealizationProfile, *PresentationProfile, *RuntimeProfile, *AuthoredOverlayProfile};
	FString MapMessage;
	FString TerrainMessage;
	FString PipelineMessage;
	bool bMapRecorded = false;
	bool bTerrainRecorded = false;
	bool bPipelineRecorded = false;
	FProjectWorldVerifyIdentity PipelineIdentity;
	FProjectWorldProjection PipelineProjection;
	TMap<FString, FString> PackageDigests;
	bool bPipeline = FProjectWorldVerifyIdentity::ForPipeline(PipelineIdentity, PipelineMessage);
	if (bPipeline)
	{
		bPipeline = ProjectWorldTwinVerifyCommandlet::AddTwinInputs(
			PipelineIdentity, Bundle, Profiles, PipelineMessage);
	}
	if (bPipeline)
	{
		bPipeline = ProjectWorldPipelineVerifyProjection::Project(*World, Bundle, *ManifestRoot,
			Profile, PipelineIdentity, PipelineProjection, PackageDigests, PipelineMessage) &&
			PipelineProjection.IsValid(PipelineMessage) && !PipelineProjection.IsEmpty();
	}
	const bool bMap = ProjectWorldTwinVerifyCommandlet::RunProjection(
		TEXT("Project.World.Realization.Verify.Map"), TEXT("map"),
		{World, *Map}, Bundle, Profiles, Mode, ProbeDir, PackageDigests, MapMessage, bMapRecorded);
	const bool bTerrain = ProjectWorldTwinVerifyCommandlet::RunProjection(
		TEXT("Project.World.Realization.Verify.MeshTerrain"), TEXT("project_mesh_terrain"),
		{World, Terrain->ArtifactRoot, &Bundle}, Bundle, Profiles, Mode,
		ProbeDir, PackageDigests, TerrainMessage, bTerrainRecorded);
	if (bPipeline)
	{
		ProjectWorldVerify::FResult Result = Mode == ProjectWorldVerify::EMode::Probe
			? ProjectWorldVerify::WriteProbe(PipelineIdentity, PipelineProjection, PackageDigests, ProbeDir)
			: ProjectWorldVerify::CompareOrRecord(PipelineIdentity, PipelineProjection, Mode);
		bPipelineRecorded = Result.bPassed && Result.Message.StartsWith(TEXT("Recorded "));
		if (Mode == ProjectWorldVerify::EMode::Record && !Result.bPassed &&
			Result.Message.Contains(TEXT("record refused: identity unchanged; nothing to record")))
		{
			Result = ProjectWorldVerify::CompareOrRecord(PipelineIdentity,
				PipelineProjection, ProjectWorldVerify::EMode::Compare);
		}
		PipelineMessage = Result.Message;
		bPipeline = Result.bPassed;
	}
	const bool bAccepted = bMap && bTerrain && bPipeline &&
		(Mode != ProjectWorldVerify::EMode::Record ||
			bMapRecorded || bTerrainRecorded || bPipelineRecorded);
	if (!ProjectWorldTwinVerifyCommandlet::WriteResult(
		*ResultPath, bAccepted, MapMessage, TerrainMessage, PipelineMessage))
	{
		return 5;
	}
	UE_LOG(LogProjectWorldTwinVerify, Display, TEXT("Twin verify %s: map=%s terrain=%s pipeline=%s"),
		bAccepted ? TEXT("accepted") : TEXT("rejected"), *MapMessage, *TerrainMessage, *PipelineMessage);
	return bAccepted ? 0 : 5;
}
