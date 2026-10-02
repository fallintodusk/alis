// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldMeshTerrainLayoutReceipt.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldMeshTerrainProducer.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInterface.h"
#include "MeshPartitionDefinition.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Utilities/ProjectSha256.h"

namespace
{
	bool HashText(const FString& Value, FString& OutHash)
	{
		FTCHARToUTF8 Utf8(*Value);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return FProjectSha256::HashBuffer(Bytes, OutHash);
	}

	bool IsSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character) || FChar::IsUpper(Character))
			{
				return false;
			}
		}
		return true;
	}
}

FString FProjectWorldMeshTerrainLayoutReceiptContract::GetLayoutPayload()
{
	return TEXT("project_mesh_terrain_layout_v1|channels=ground:0,hydro_transition:1|")
		TEXT("source=float32_vertex_weight|compiled=unorm8_texture2d_array|")
		TEXT("texel_cm=3000|max_dimension=4096|uv_set=0");
}

FString FProjectWorldMeshTerrainLayoutReceiptContract::GetBuildPolicyPayload()
{
	const UE::MeshPartition::UMeshPartitionDefinition* Definition =
		LoadObject<UE::MeshPartition::UMeshPartitionDefinition>(
			nullptr, ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath);
	const FString MaterialPath = Definition != nullptr && Definition->GetMaterial() != nullptr
		? Definition->GetMaterial()->GetPathName() : FString();
	if (MaterialPath.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(TEXT("project_mesh_terrain_build_policy_v1|section_max_complexity=2048|")
		TEXT("modifier_priorities=Base|uv_layout=ReferenceBoxProject|")
		TEXT("material=%s|")
		TEXT("physical_material_channels=none|default_physical_material=none|")
		TEXT("collision=complex_as_simple:navigation=true:fast_cook=false:disable_active_edge_precompute=false|")
		TEXT("nanite=static_mesh:use_nanite=true:navigation=false:fallback_percent_triangles=1.0|")
		TEXT("fallback=static_mesh:use_nanite=false:navigation=false|variants=collision,nanite,fallback|")
		TEXT("windows=collision,nanite|windows_client=collision,nanite|")
		TEXT("windows_server=collision|linux_server=collision|")
		TEXT("default=collision,fallback|split_runtime_grid=false"), *MaterialPath);
}

FString FProjectWorldMeshTerrainLayoutReceiptContract::GetAdapterCompilerFingerprint()
{
#ifndef PROJECT_WORLD_MESH_TERRAIN_COMPILER_SOURCE_SHA256
#error ProjectWorldMeshTerrainEditor must bind its compiler fingerprint to the admitted source set.
#endif
	return FString(UTF8_TO_TCHAR(PROJECT_WORLD_MESH_TERRAIN_COMPILER_SOURCE_SHA256));
}

bool FProjectWorldMeshTerrainLayoutReceiptContract::Build(
	const FProjectWorldCanonicalBundle& Bundle,
	FProjectWorldMeshTerrainLayoutReceipt& OutReceipt,
	FString& OutError)
{
	OutReceipt = {};
	if (Bundle.Cells.IsEmpty())
	{
		OutError = TEXT("Mesh Terrain layout receipt requires canonical terrain cells.");
		return false;
	}
	const FProjectWorldCanonicalTerrain& FirstTerrain = Bundle.Cells[0].Terrain;
	if (FirstTerrain.SurfaceContractId.IsEmpty() || FirstTerrain.SurfaceContractVersion <= 0 ||
		!IsSha256(FirstTerrain.SurfaceContractHash))
	{
		OutError = TEXT("Canonical terrain surface contract is incomplete.");
		return false;
	}
	for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
	{
		if (Cell.Terrain.SurfaceContractId != FirstTerrain.SurfaceContractId ||
			Cell.Terrain.SurfaceContractVersion != FirstTerrain.SurfaceContractVersion ||
			Cell.Terrain.SurfaceContractHash != FirstTerrain.SurfaceContractHash)
		{
			OutError = TEXT("Canonical terrain cells disagree on the surface contract.");
			return false;
		}
	}

	const FString DefinitionPackage = FPackageName::ObjectPathToPackageName(
		FString(ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath));
	const FString DefinitionFilename = FPackageName::LongPackageNameToFilename(
		DefinitionPackage, FPackageName::GetAssetPackageExtension());
	if (!FProjectSha256::HashFile(DefinitionFilename, OutReceipt.SharedDefinitionPackageSha256) ||
		!IsSha256(OutReceipt.SharedDefinitionPackageSha256))
	{
		OutError = FString::Printf(TEXT("Cannot hash shared Mesh Terrain definition: %s"), *DefinitionFilename);
		return false;
	}

	OutReceipt.CanonicalSurfaceContractId = FirstTerrain.SurfaceContractId;
	OutReceipt.CanonicalSurfaceContractVersion = FirstTerrain.SurfaceContractVersion;
	OutReceipt.CanonicalSurfaceContractSha256 = FirstTerrain.SurfaceContractHash;
	OutReceipt.AdapterCompilerSha256 = GetAdapterCompilerFingerprint();
	const FEngineVersion Engine = FEngineVersion::Current();
	OutReceipt.EngineIdentity = FString::Printf(
		TEXT("%s|changelist=%u"), *Engine.ToString(), Engine.GetChangelist());
	const FString BuildPolicyPayload = GetBuildPolicyPayload();
	if (BuildPolicyPayload.IsEmpty() || !HashText(GetLayoutPayload(), OutReceipt.LayoutSha256) ||
		!HashText(BuildPolicyPayload, OutReceipt.BuildPolicySha256))
	{
		OutError = TEXT("Cannot hash the Mesh Terrain layout or build policy.");
		return false;
	}
	OutReceipt.ReceiptPayload = FString::Printf(
		TEXT("project_mesh_terrain_receipt_v1|surface=%s:%d:%s|adapter=project_mesh_terrain:1:%s|")
		TEXT("engine=%s|layout=project_mesh_terrain_channels:1:%s|definition=%s:%s|build=%s"),
		*OutReceipt.CanonicalSurfaceContractId,
		OutReceipt.CanonicalSurfaceContractVersion,
		*OutReceipt.CanonicalSurfaceContractSha256,
		*OutReceipt.AdapterCompilerSha256,
		*OutReceipt.EngineIdentity,
		*OutReceipt.LayoutSha256,
		ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath,
		*OutReceipt.SharedDefinitionPackageSha256,
		*OutReceipt.BuildPolicySha256);
	if (!HashText(OutReceipt.ReceiptPayload, OutReceipt.ReceiptSha256))
	{
		OutError = TEXT("Cannot hash the Mesh Terrain layout receipt.");
		return false;
	}
	return true;
}

bool FProjectWorldMeshTerrainLayoutReceiptContract::Save(
	const FProjectWorldMeshTerrainLayoutReceipt& Receipt,
	const FString& OutputPath,
	FString& OutError)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"), TEXT("../../Data/Schemas/mesh-terrain-layout-receipt.schema.json"));
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("receipt_id"), TEXT("project_mesh_terrain_layout"));
	Root->SetStringField(TEXT("canonical_surface_contract_id"), Receipt.CanonicalSurfaceContractId);
	Root->SetNumberField(TEXT("canonical_surface_contract_version"), Receipt.CanonicalSurfaceContractVersion);
	Root->SetStringField(TEXT("canonical_surface_contract_sha256"), Receipt.CanonicalSurfaceContractSha256);
	Root->SetStringField(TEXT("adapter_id"), TEXT("project_mesh_terrain"));
	Root->SetNumberField(TEXT("adapter_version"), 1);
	Root->SetStringField(TEXT("adapter_compiler_sha256"), Receipt.AdapterCompilerSha256);
	Root->SetStringField(TEXT("engine_identity"), Receipt.EngineIdentity);
	Root->SetStringField(TEXT("layout_id"), TEXT("project_mesh_terrain_channels"));
	Root->SetNumberField(TEXT("layout_version"), 1);
	Root->SetStringField(TEXT("layout_sha256"), Receipt.LayoutSha256);
	TArray<TSharedPtr<FJsonValue>> Channels;
	for (const TPair<const TCHAR*, int32> Channel : {
		TPair<const TCHAR*, int32>(TEXT("ground"), 0),
		TPair<const TCHAR*, int32>(TEXT("hydro_transition"), 1)})
	{
		TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("semantic_name"), Channel.Key);
		Value->SetNumberField(TEXT("private_channel_index"), Channel.Value);
		Channels.Add(MakeShared<FJsonValueObject>(Value));
	}
	Root->SetArrayField(TEXT("channels"), Channels);
	Root->SetStringField(TEXT("source_encoding"), TEXT("float32_vertex_weight"));
	Root->SetStringField(TEXT("compiled_encoding"), TEXT("unorm8_texture2d_array"));
	Root->SetNumberField(TEXT("channel_texel_size_cm"), 3000);
	Root->SetNumberField(TEXT("channel_texture_max_dimension"), 4096);
	Root->SetNumberField(TEXT("uv_set"), 0);
	Root->SetStringField(TEXT("shared_definition_object_path"),
		ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath);
	Root->SetStringField(TEXT("shared_definition_package_sha256"), Receipt.SharedDefinitionPackageSha256);
	Root->SetStringField(TEXT("build_policy_sha256"), Receipt.BuildPolicySha256);
	Root->SetStringField(TEXT("receipt_payload"), Receipt.ReceiptPayload);
	Root->SetStringField(TEXT("receipt_sha256"), Receipt.ReceiptSha256);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutError = TEXT("Cannot serialize the Mesh Terrain layout receipt.");
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
	const FString StagingPath = OutputPath + TEXT(".staging");
	if (!FFileHelper::SaveStringToFile(
		Json + LINE_TERMINATOR, *StagingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
		!IFileManager::Get().Move(*OutputPath, *StagingPath, true, true, false, false))
	{
		IFileManager::Get().Delete(*StagingPath, false, true);
		OutError = FString::Printf(TEXT("Cannot atomically save layout receipt: %s"), *OutputPath);
		return false;
	}
	return true;
}
