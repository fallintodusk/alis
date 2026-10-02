#include "Misc/AutomationTest.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "HAL/FileManager.h"
#include "MeshPartition.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionModifierComponent.h"
#include "MeshPartitionChannelCollection.h"
#include "MeshPartitionTransformerPipeline.h"
#include "Materials/MaterialInterface.h"
#include "Modifiers/MeshPartitionMeshProvider.h"
#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldMeshTerrainLayoutReceipt.h"
#include "ProjectWorldMeshTerrainAuditCommandlet.h"
#include "ProjectWorldMeshTerrainTransformer.h"
#include "ProjectWorldTerrainRuntimeRole.h"
#include "ProjectWorldTerrainProducerRegistry.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Utilities/ProjectSha256.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainRendererFacingWindingTest,
	"Project.World.Realization.MeshTerrain.RendererFacingWinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainRendererFacingWindingTest::RunTest(const FString& Parameters)
{
	UE::Geometry::FDynamicMesh3 Mesh;
	Mesh.AppendVertex(FVector3d(0.0, 0.0, 0.0));
	Mesh.AppendVertex(FVector3d(1.0, 0.0, 0.0));
	Mesh.AppendVertex(FVector3d(0.0, 1.0, 0.0));
	Mesh.AppendVertex(FVector3d(1.0, 1.0, 0.0));
	ProjectWorldMeshTerrainProducer::AppendRendererFacingGridTriangles(Mesh, 2, 2);

	TestEqual(TEXT("A grid quad produces exactly two triangles"), Mesh.TriangleCount(), 2);
	TestEqual(TEXT("First triangle uses renderer-facing winding"),
		Mesh.GetTriangle(0), UE::Geometry::FIndex3i(0, 2, 1));
	TestEqual(TEXT("Second triangle uses renderer-facing winding"),
		Mesh.GetTriangle(1), UE::Geometry::FIndex3i(1, 2, 3));

	const auto SignedAreaXY = [&Mesh](const UE::Geometry::FIndex3i& Triangle)
	{
		const FVector3d A = Mesh.GetVertex(Triangle.A);
		const FVector3d B = Mesh.GetVertex(Triangle.B);
		const FVector3d C = Mesh.GetVertex(Triangle.C);
		return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
	};
	TestTrue(TEXT("Both triangles are clockwise in projected Unreal XY"),
		SignedAreaXY(Mesh.GetTriangle(0)) < 0.0 && SignedAreaXY(Mesh.GetTriangle(1)) < 0.0);
	TestTrue(TEXT("The rejected legacy winding is counter-clockwise"),
		SignedAreaXY(UE::Geometry::FIndex3i(0, 1, 2)) > 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainBaseIdentityTest,
	"Project.World.Realization.MeshTerrain.BaseIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainBaseIdentityTest::RunTest(const FString& Parameters)
{
	const FString Input(TEXT("terrain-hash"));
	const FString Engine(TEXT("5.8"));
	const FString Material(TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default"));
	const FString AdapterCompiler =
		FProjectWorldMeshTerrainLayoutReceiptContract::GetAdapterCompilerFingerprint();
	const TArray<FName> CurrentTags = {
		FName(*(TEXT("ProjectWorld.MeshTerrain.Input=") + Input)),
		FName(*(TEXT("ProjectWorld.MeshTerrain.Engine=") + Engine)),
		FName(*(TEXT("ProjectWorld.MeshTerrain.Material=") + Material)),
		FName(*(TEXT("ProjectWorld.MeshTerrain.AdapterCompiler=") + AdapterCompiler))};
	TestTrue(TEXT("An exact base identity is reusable"),
		ProjectWorldMeshTerrainProducer::MatchesBaseIdentity(
			CurrentTags, Input, Engine, Material, AdapterCompiler));
	TestFalse(TEXT("A stale material path is not reusable"),
		ProjectWorldMeshTerrainProducer::MatchesBaseIdentity(
			CurrentTags, Input, Engine,
			TEXT("/ProjectMaterial/Surfaces/Terrain/MI_StaleTerrain.MI_StaleTerrain"), AdapterCompiler));
	TestFalse(TEXT("A stale adapter compiler is not reusable"),
		ProjectWorldMeshTerrainProducer::MatchesBaseIdentity(
			CurrentTags, Input, Engine, Material,
			TEXT("0000000000000000000000000000000000000000000000000000000000000000")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainAuditMapBoundaryTest,
	"Project.World.Realization.MeshTerrain.AuditMapBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainAuditMapBoundaryTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Synthetic generated maps remain auditable"),
		UProjectWorldMeshTerrainAuditCommandlet::IsSupportedMapPackagePath(
			TEXT("/ProjectWorldTestData/Generated/MeshTerrain/L_MeshTerrain")));
	TestTrue(TEXT("Production generated maps are auditable"),
		UProjectWorldMeshTerrainAuditCommandlet::IsSupportedMapPackagePath(
			TEXT("/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory")));
	TestFalse(TEXT("Unowned game maps remain rejected"),
		UProjectWorldMeshTerrainAuditCommandlet::IsSupportedMapPackagePath(
			TEXT("/Game/Maps/L_Unowned")));
	TestFalse(TEXT("Generated-root prefix lookalikes remain rejected"),
		UProjectWorldMeshTerrainAuditCommandlet::IsSupportedMapPackagePath(
			TEXT("/ProjectWorldData/GeneratedOutside/L_Unowned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainArchitectureBoundaryTest,
	"Project.World.Realization.MeshTerrain.ArchitectureBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainArchitectureBoundaryTest::RunTest(const FString& Parameters)
{
	const TArray<FString> ForbiddenRoots = {
		TEXT("Plugins/World/ProjectWorld/Source"),
		TEXT("tools/World/CanonicalCompilation"),
		TEXT("tools/World/SourceIngestion")};
	const TArray<FString> Extensions = {
		TEXT("*.h"), TEXT("*.cpp"), TEXT("*.cs"), TEXT("*.py"), TEXT("*.json"), TEXT("*.ps1")};
	TArray<FString> Violations;
	for (const FString& RelativeRoot : ForbiddenRoots)
	{
		const FString Root = FPaths::Combine(FPaths::ProjectDir(), RelativeRoot);
		for (const FString& Extension : Extensions)
		{
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *Root, *Extension, true, false);
			for (const FString& File : Files)
			{
				FString Source;
				if (!FFileHelper::LoadFileToString(Source, *File))
				{
					Violations.Add(FString::Printf(TEXT("unreadable:%s"), *File));
					continue;
				}
				const FString Lower = Source.ToLower();
				if (Lower.Contains(TEXT("meshpartition")) ||
					Lower.Contains(TEXT("projectworldmeshterrain")))
				{
					Violations.Add(FString::Printf(TEXT("mesh-terrain-boundary:%s"), *File));
				}
				if (RelativeRoot == TEXT("Plugins/World/ProjectWorld/Source") &&
					Lower.Contains(TEXT("projectmaterialeditor")))
				{
					Violations.Add(FString::Printf(TEXT("material-editor-boundary:%s"), *File));
				}
			}
		}
	}
	TestTrue(*FString::Printf(TEXT("Forbidden dependency violations: %s"),
		*FString::Join(Violations, TEXT(", "))), Violations.IsEmpty());
	return Violations.IsEmpty();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainExportSeamTest,
	"Project.World.MeshTerrain.ExportSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainExportSeamTest::RunTest(const FString& Parameters)
{
	using namespace UE::MeshPartition;

	const auto SetDefinition = &AMeshPartition::SetMeshPartitionDefinition;
	const auto SetAffectedPartition = &UModifierComponent::SetAffectedMeshPartition;
	const auto SetMesh = &UMeshProviderModifier::SetMesh;
	const auto MakeUnit = &MakeTransformerUnit;
	const auto Wait = &WaitOnGameThread;
	TestTrue(TEXT("SetMeshPartitionDefinition export resolves"), SetDefinition != nullptr);
	TestTrue(TEXT("SetAffectedMeshPartition export resolves"), SetAffectedPartition != nullptr);
	TestTrue(TEXT("Mesh provider SetMesh export resolves"), SetMesh != nullptr);
	TestTrue(TEXT("MakeTransformerUnit export resolves"), MakeUnit != nullptr);
	TestTrue(TEXT("WaitOnGameThread export resolves"), Wait != nullptr);

	TestNotNull(TEXT("AMeshPartition reflection resolves"), AMeshPartition::StaticClass());
	TestNotNull(TEXT("UMeshPartitionDefinition reflection resolves"),
		UMeshPartitionDefinition::StaticClass());
	TestNotNull(TEXT("UTransformerPipeline reflection resolves"),
		UTransformerPipeline::StaticClass());

	TestNotNull(TEXT("Project transformer reflection resolves"),
		FProjectWorldMeshTerrainTransformer::StaticStruct());
	TestEqual(TEXT("Terrain runtime tag remains representation-neutral"),
		ProjectWorldTerrainRuntimeRole::RoleTag(),
		FName(TEXT("ProjectWorld.Terrain.v1")));
	TestEqual(TEXT("MeshPartition channel packing capacity remains supported"),
		static_cast<int32>(FChannelPacking::MaxNumberPackedChannels), 24);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainSharedDefinitionCreateTest,
	"Project.World.Realization.MeshTerrain.SharedDefinitionCreate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainSharedDefinitionCreateTest::RunTest(const FString& Parameters)
{
	FString Error;
	UMaterialInterface* TerrainMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default"));
	TestNotNull(TEXT("The authenticated terrain.default material loads"), TerrainMaterial);
	TestTrue(TEXT("Shared definition is created and saved through supported reflection"),
		ProjectWorldMeshTerrainProducer::EnsureSharedDefinition(TerrainMaterial, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainSharedDefinitionTest,
	"Project.World.Realization.MeshTerrain.SharedDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainSharedDefinitionTest::RunTest(const FString& Parameters)
{
	using namespace UE::MeshPartition;
	FString Error;
	const FString TerrainMaterialObjectPath =
		TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default");
	const FString DefinitionFilename = FPackageName::LongPackageNameToFilename(
		FPackageName::ObjectPathToPackageName(
			FString(ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath)),
		FPackageName::GetAssetPackageExtension());
	FString BeforeSha256;
	TestTrue(TEXT("Shared definition package is hashable before validation"),
		FProjectSha256::HashFile(DefinitionFilename, BeforeSha256));
	TestTrue(TEXT("Shared definition is current before readback"),
		ProjectWorldMeshTerrainProducer::ValidateSharedDefinition(TerrainMaterialObjectPath, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
		return false;
	}
	UMeshPartitionDefinition* Definition = LoadObject<UMeshPartitionDefinition>(
		nullptr, ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath);
	TestNotNull(TEXT("Saved shared definition reloads"), Definition);
	if (Definition == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Prototype channel texel size"), Definition->GetChannelTexelSize(), 3000.0f);
	TestEqual(TEXT("Engine channel texture maximum"),
		FChannelTextureRenderer::DefaultMaxImageResolution, 4096);
	TestEqual(TEXT("Shared channel count"), Definition->GetChannelMap().GetNumChannels(), 2);
	TestEqual(TEXT("Ground keeps private index zero"),
		Definition->GetChannelMap().FindChannel(ProjectWorldMeshTerrainProducer::GroundChannel), 0);
	TestEqual(TEXT("Hydro transition keeps private index one"),
		Definition->GetChannelMap().FindChannel(ProjectWorldMeshTerrainProducer::HydroTransitionChannel), 1);
	TestEqual(TEXT("Modifier priority is the single Base category"),
		Definition->GetModifierTypePriorities(), TArray<FName>({TEXT("Base")}));
	TestEqual(TEXT("Channel UV layout uses reference-box projection"),
		Definition->GetChannelUVLayoutMethod(), EChannelCollectionUVLayoutMethod::ReferenceBoxProject);
	TestTrue(TEXT("Definition has no physical-material channels"),
		Definition->GetPhysicalMaterialChannels().IsEmpty());
	TestNull(TEXT("Definition has no default physical material"),
		Definition->GetDefaultPhysicalMaterial());
	TestEqual(TEXT("Definition has three explicit build variants"),
		Definition->GetCompiledSectionBuildVariants().Num(), 3);
	TestEqual(TEXT("Windows cooks collision and Nanite with its SM5 fallback"),
		Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("Windows"), TEXT("Windows")),
		TArray<FName>({TEXT("collision"), TEXT("nanite")}));
	TestEqual(TEXT("WindowsClient uses the Windows render contract"),
		Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("WindowsClient"), TEXT("Windows")),
		TArray<FName>({TEXT("collision"), TEXT("nanite")}));
	TestEqual(TEXT("WindowsServer cooks collision only"),
		Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("WindowsServer"), TEXT("Windows")),
		TArray<FName>({TEXT("collision")}));
	TestEqual(TEXT("LinuxServer cooks collision only"),
		Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("LinuxServer"), TEXT("Linux")),
		TArray<FName>({TEXT("collision")}));
	TestEqual(TEXT("Unknown platforms use collision and explicit non-Nanite fallback"),
		Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("Unknown"), TEXT("Unknown")),
		TArray<FName>({TEXT("collision"), TEXT("fallback")}));
	const TConstArrayView<FCompiledSectionBuildVariant> Variants =
		Definition->GetCompiledSectionBuildVariants();
	const TArray<FString> ExpectedFirstTransformers = {
		TEXT("/Script/MeshPartitionEditor.CollisionTransformer"),
		TEXT("/Script/MeshPartitionEditor.StaticMeshTransformer"),
		TEXT("/Script/MeshPartitionEditor.StaticMeshTransformer")};
	for (int32 Index = 0; Index < Variants.Num(); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Variant %d section complexity bound"), Index),
			Variants[Index].MaxSectionComplexity, 2048.0);
		TestFalse(*FString::Printf(TEXT("Variant %d is runtime-grid independent"), Index),
			Variants[Index].bSplitSectionsToMatchWorldPartitionRuntimeGrid);
		const UTransformerPipeline* Pipeline = Variants[Index].TransformerPipeline;
		TestNotNull(*FString::Printf(TEXT("Variant %d has a pipeline"), Index), Pipeline);
		TestTrue(*FString::Printf(TEXT("Variant %d pipeline is editor-only"), Index),
			Pipeline != nullptr && Pipeline->IsEditorOnly());
		if (Pipeline == nullptr)
		{
			continue;
		}
		const TArray<TInstancedStruct<FTransformer>>& Transformers = Pipeline->GetTransformers();
		TestEqual(*FString::Printf(TEXT("Variant %d transformer count"), Index), Transformers.Num(), 2);
		if (Transformers.Num() == 2)
		{
			TestEqual(*FString::Printf(TEXT("Variant %d first transformer"), Index),
				Transformers[0].GetScriptStruct()->GetPathName(), ExpectedFirstTransformers[Index]);
			TestEqual(*FString::Printf(TEXT("Variant %d runtime tag transformer"), Index),
				Transformers[1].GetScriptStruct()->GetPathName(),
				FString(TEXT("/Script/ProjectWorldMeshTerrain.ProjectWorldMeshTerrainTransformer")));
			if (Index > 0)
			{
				const FBoolProperty* UseNanite = FindFProperty<FBoolProperty>(
					Transformers[0].GetScriptStruct(), TEXT("bUseNanite"));
				const FBoolProperty* AffectsNavigation = FindFProperty<FBoolProperty>(
					Transformers[0].GetScriptStruct(), TEXT("bCanEverAffectNavigation"));
				TestNotNull(*FString::Printf(TEXT("Variant %d exposes Nanite policy"), Index), UseNanite);
				TestNotNull(*FString::Printf(TEXT("Variant %d exposes navigation policy"), Index), AffectsNavigation);
				if (UseNanite != nullptr)
				{
					TestEqual(*FString::Printf(TEXT("Variant %d has the expected Nanite mapping"), Index),
						UseNanite->GetPropertyValue_InContainer(Transformers[0].GetMemory()), Index == 1);
				}
				if (AffectsNavigation != nullptr)
				{
					TestFalse(*FString::Printf(TEXT("Variant %d never exports navigation"), Index),
						AffectsNavigation->GetPropertyValue_InContainer(Transformers[0].GetMemory()));
				}
			}
			else
			{
				const FBoolProperty* AffectsNavigation = FindFProperty<FBoolProperty>(
					Transformers[0].GetScriptStruct(), TEXT("bCanEverAffectNavigation"));
				const FBoolProperty* FastCook = FindFProperty<FBoolProperty>(
					Transformers[0].GetScriptStruct(), TEXT("bFastCook"));
				const FBoolProperty* DisableActiveEdgePrecompute = FindFProperty<FBoolProperty>(
					Transformers[0].GetScriptStruct(), TEXT("bDisableActiveEdgePrecompute"));
				TestTrue(TEXT("Collision exports navigation"),
					AffectsNavigation != nullptr &&
					AffectsNavigation->GetPropertyValue_InContainer(Transformers[0].GetMemory()));
				TestFalse(TEXT("Collision uses deterministic full cook"),
					FastCook == nullptr || FastCook->GetPropertyValue_InContainer(Transformers[0].GetMemory()));
				TestFalse(TEXT("Collision retains active-edge precomputation"),
					DisableActiveEdgePrecompute == nullptr ||
					DisableActiveEdgePrecompute->GetPropertyValue_InContainer(Transformers[0].GetMemory()));
			}
			if (Index == 1)
			{
				const FFloatProperty* FallbackPercent = FindFProperty<FFloatProperty>(
					Transformers[0].GetScriptStruct(), TEXT("NaniteFallbackPercentTriangles"));
				TestNotNull(TEXT("Nanite fallback percentage is reflected"), FallbackPercent);
				if (FallbackPercent != nullptr)
				{
					TestEqual(TEXT("SM5 fallback retains the complete terrain surface"),
						FallbackPercent->GetPropertyValue_InContainer(Transformers[0].GetMemory()), 1.0f);
				}
			}
		}
	}
	TestEqual(TEXT("Definition material uses ProjectWorld's authenticated terrain.default identity"),
		Definition->GetMaterial()->GetPathName(),
		TerrainMaterialObjectPath);
	FString AfterSha256;
	TestTrue(TEXT("Shared definition package is hashable after validation"),
		FProjectSha256::HashFile(DefinitionFilename, AfterSha256));
	TestEqual(TEXT("Realization validation never rewrites the versioned shared definition"),
		AfterSha256, BeforeSha256);

	FProperty* TexelSize = Definition->GetClass()->FindPropertyByName(TEXT("ChannelTexelSize"));
	TestNotNull(TEXT("Known-bad control can reach channel texel size"), TexelSize);
	if (TexelSize != nullptr)
	{
		FString OriginalValue;
		TexelSize->ExportTextItem_Direct(
			OriginalValue,
			TexelSize->ContainerPtrToValuePtr<void>(Definition),
			nullptr,
			Definition,
			PPF_None);
		TestNotNull(TEXT("Known-bad channel mutation is accepted in memory"),
			TexelSize->ImportText_Direct(
				TEXT("2999.0"),
				TexelSize->ContainerPtrToValuePtr<void>(Definition),
				Definition,
				PPF_None));
		FString Rejection;
		TestFalse(TEXT("Stale shared-definition ABI fails closed"),
			ProjectWorldMeshTerrainProducer::ValidateSharedDefinition(
				TerrainMaterialObjectPath, Rejection));
		TestFalse(TEXT("Known-bad rejection is diagnostic"), Rejection.IsEmpty());
		TexelSize->ImportText_Direct(
			*OriginalValue,
			TexelSize->ContainerPtrToValuePtr<void>(Definition),
			Definition,
			PPF_None);
	}
	if (Variants.Num() > 1 && Variants[1].TransformerPipeline != nullptr)
	{
		FArrayProperty* TransformersProperty = FindFProperty<FArrayProperty>(
			Variants[1].TransformerPipeline->GetClass(), TEXT("Transformers"));
		auto* MutableTransformers = TransformersProperty != nullptr
			? TransformersProperty->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(
				Variants[1].TransformerPipeline)
			: nullptr;
		TestNotNull(TEXT("Known-bad control can reach Nanite transformer pipeline"), MutableTransformers);
		if (MutableTransformers != nullptr && MutableTransformers->Num() > 0)
		{
			FInstancedStruct& NaniteTransformer = (*MutableTransformers)[0];
			FFloatProperty* FallbackPercent = FindFProperty<FFloatProperty>(
				NaniteTransformer.GetScriptStruct(), TEXT("NaniteFallbackPercentTriangles"));
			TestNotNull(TEXT("Known-bad control can reach Nanite fallback percentage"), FallbackPercent);
			if (FallbackPercent != nullptr)
			{
				const float OriginalFallback = FallbackPercent->GetPropertyValue_InContainer(
					NaniteTransformer.GetMemory());
				FallbackPercent->SetPropertyValue_InContainer(
					NaniteTransformer.GetMutableMemory(), 0.5f);
				FString Rejection;
				TestFalse(TEXT("Stale shared-definition fallback policy fails closed"),
					ProjectWorldMeshTerrainProducer::ValidateSharedDefinition(
						TerrainMaterialObjectPath, Rejection));
				TestFalse(TEXT("Known-bad fallback rejection is diagnostic"), Rejection.IsEmpty());
				FallbackPercent->SetPropertyValue_InContainer(
					NaniteTransformer.GetMutableMemory(), OriginalFallback);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainProducerSelectionTest,
	"Project.World.Realization.MeshTerrain.ProducerSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainProducerSelectionTest::RunTest(const FString& Parameters)
{
	const FString Settings = TEXT(
		"{\"channel_texel_size_cm\":3000,\"channel_texture_max_dimension\":4096,"
		"\"collision\":\"complex_as_simple\",\"render_variants\":[\"nanite\",\"fallback\"],"
		"\"section_max_complexity\":2048,\"shared_definition\":"
		"\"/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1."
		"MPD_ProjectTerrain_Shared_v1\",\"surface_contract_id\":\"terrain_surface_semantics\","
		"\"surface_contract_version\":1}");
	FString Error;
	TestTrue(TEXT("Mesh Terrain tuple is registered"),
		ProjectWorldTerrainProducerRegistry::IsRegistered(TEXT("project_mesh_terrain"), 1));
	TestTrue(TEXT("Generator ID selects and validates the Mesh Terrain producer"),
		ProjectWorldTerrainProducerRegistry::ValidateLayer(
			TEXT("project_mesh_terrain"), 1, {TEXT("terrain")},
			TEXT("compiled_sections_from_canonical_cells"), TEXT("world_partition_spatial"),
			0, Settings, Error));
	TestFalse(TEXT("Unknown generator version is rejected"),
		ProjectWorldTerrainProducerRegistry::ValidateLayer(
			TEXT("project_mesh_terrain"), 2, {TEXT("terrain")},
			TEXT("compiled_sections_from_canonical_cells"), TEXT("world_partition_spatial"),
			0, Settings, Error));
	return true;
}

#endif
