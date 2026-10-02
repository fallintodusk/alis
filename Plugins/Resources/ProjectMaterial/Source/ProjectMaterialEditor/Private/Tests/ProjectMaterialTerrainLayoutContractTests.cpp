// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "ProjectMaterialCompilerIdentity.h"
#include "ProjectMaterialTerrainLayoutContract.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString MakeReceipt(const bool bCorruptLayout)
	{
		const FString SurfaceHash = FString::ChrN(64, TEXT('a'));
		const FString AdapterHash = FString::ChrN(64, TEXT('b'));
		const FString DefinitionHash = FString::ChrN(64, TEXT('c'));
		const FString LayoutPayload = TEXT(
			"project_mesh_terrain_layout_v1|channels=ground:0,hydro_transition:1|"
			"source=float32_vertex_weight|compiled=unorm8_texture2d_array|"
			"texel_cm=3000|max_dimension=4096|uv_set=0");
		const FString BuildPayload = TEXT(
			"project_mesh_terrain_build_policy_v1|section_max_complexity=2048|"
			"modifier_priorities=Base|uv_layout=ReferenceBoxProject|"
			"material=/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default|"
			"physical_material_channels=none|default_physical_material=none|"
			"collision=complex_as_simple:navigation=true:fast_cook=false:disable_active_edge_precompute=false|"
			"nanite=static_mesh:use_nanite=true:navigation=false:fallback_percent_triangles=1.0|"
			"fallback=static_mesh:use_nanite=false:navigation=false|variants=collision,nanite,fallback|"
			"windows=collision,nanite|windows_client=collision,nanite|"
			"windows_server=collision|linux_server=collision|"
			"default=collision,fallback|split_runtime_grid=false");
		FString LayoutHash = FProjectMaterialCompilerIdentity::ComputeStringSha256(LayoutPayload);
		const FString BuildHash = FProjectMaterialCompilerIdentity::ComputeStringSha256(BuildPayload);
		if (bCorruptLayout)
		{
			LayoutHash[0] = LayoutHash[0] == TEXT('0') ? TEXT('1') : TEXT('0');
		}
		const FString EngineIdentity = TEXT("5.8.3-test|changelist=1");
		const FString Payload = FString::Printf(
			TEXT("project_mesh_terrain_receipt_v1|surface=terrain_surface_semantics:1:%s|")
			TEXT("adapter=project_mesh_terrain:1:%s|engine=%s|")
			TEXT("layout=project_mesh_terrain_channels:1:%s|definition=%s:%s|build=%s"),
			*SurfaceHash,
			*AdapterHash,
			*EngineIdentity,
			*LayoutHash,
			TEXT("/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1.MPD_ProjectTerrain_Shared_v1"),
			*DefinitionHash,
			*BuildHash);

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("$schema"), TEXT("../../Data/Schemas/mesh-terrain-layout-receipt.schema.json"));
		Root->SetNumberField(TEXT("schema_version"), 1);
		Root->SetStringField(TEXT("receipt_id"), TEXT("project_mesh_terrain_layout"));
		Root->SetStringField(TEXT("canonical_surface_contract_id"), TEXT("terrain_surface_semantics"));
		Root->SetNumberField(TEXT("canonical_surface_contract_version"), 1);
		Root->SetStringField(TEXT("canonical_surface_contract_sha256"), SurfaceHash);
		Root->SetStringField(TEXT("adapter_id"), TEXT("project_mesh_terrain"));
		Root->SetNumberField(TEXT("adapter_version"), 1);
		Root->SetStringField(TEXT("adapter_compiler_sha256"), AdapterHash);
		Root->SetStringField(TEXT("engine_identity"), EngineIdentity);
		Root->SetStringField(TEXT("layout_id"), TEXT("project_mesh_terrain_channels"));
		Root->SetNumberField(TEXT("layout_version"), 1);
		Root->SetStringField(TEXT("layout_sha256"), LayoutHash);
		TArray<TSharedPtr<FJsonValue>> Channels;
		for (const TPair<FString, int32>& Value : TArray<TPair<FString, int32>>({
			{TEXT("ground"), 0}, {TEXT("hydro_transition"), 1}}))
		{
			TSharedRef<FJsonObject> Channel = MakeShared<FJsonObject>();
			Channel->SetStringField(TEXT("semantic_name"), Value.Key);
			Channel->SetNumberField(TEXT("private_channel_index"), Value.Value);
			Channels.Add(MakeShared<FJsonValueObject>(Channel));
		}
		Root->SetArrayField(TEXT("channels"), Channels);
		Root->SetStringField(TEXT("source_encoding"), TEXT("float32_vertex_weight"));
		Root->SetStringField(TEXT("compiled_encoding"), TEXT("unorm8_texture2d_array"));
		Root->SetNumberField(TEXT("channel_texel_size_cm"), 3000);
		Root->SetNumberField(TEXT("channel_texture_max_dimension"), 4096);
		Root->SetNumberField(TEXT("uv_set"), 0);
		Root->SetStringField(TEXT("shared_definition_object_path"),
			TEXT("/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1.MPD_ProjectTerrain_Shared_v1"));
		Root->SetStringField(TEXT("shared_definition_package_sha256"), DefinitionHash);
		Root->SetStringField(TEXT("build_policy_sha256"), BuildHash);
		Root->SetStringField(TEXT("receipt_payload"), Payload);
		Root->SetStringField(TEXT("receipt_sha256"),
			FProjectMaterialCompilerIdentity::ComputeStringSha256(Payload));
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		return Json;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectMaterialTerrainLayoutContractTest,
	"Project.Material.MeshTerrain.LayoutReceipt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectMaterialTerrainLayoutContractTest::RunTest(const FString& Parameters)
{
	FProjectMaterialTerrainLayoutContract Contract;
	FString Error;
	TestTrue(TEXT("Independent material host accepts the adapter receipt"),
		FProjectMaterialTerrainLayoutCompiler::CompileJson(MakeReceipt(false), FString(), Contract, Error));
	TestEqual(TEXT("Ground semantic maps to private channel zero"), Contract.SemanticChannels[TEXT("ground")], 0);
	TestEqual(TEXT("Hydro semantic maps to private channel one"),
		Contract.SemanticChannels[TEXT("hydro_transition")], 1);
	TestFalse(TEXT("Injected layout mismatch fails before generation"),
		FProjectMaterialTerrainLayoutCompiler::CompileJson(MakeReceipt(true), FString(), Contract, Error));
	return true;
}

#endif
