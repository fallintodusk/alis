// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldCanonicalContractTests
{
	enum class EConflict : uint8
	{
		None,
		CoverageVersion,
		ProvenanceSchema,
		CoverageCell,
		TerrainBounds,
		TerrainSpacing,
		FeatureManifest
	};

	struct FDocument
	{
		FString Path;
		FString Hash;
		int64 ByteSize = 0;
	};

	bool WriteDocument(
		const FString& Root,
		const FString& RelativePath,
		const FString& Contents,
		TArray<FDocument>& OutDocuments)
	{
		const FString FullPath = FPaths::Combine(Root, RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);
		if (!FFileHelper::SaveStringToFile(Contents, *FullPath))
		{
			return false;
		}

		FDocument Document;
		Document.Path = RelativePath;
		Document.ByteSize = IFileManager::Get().FileSize(*FullPath);
		if (!FProjectWorldCanonicalLoader::ComputeFileSha256(FullPath, Document.Hash))
		{
			return false;
		}
		OutDocuments.Add(MoveTemp(Document));
		return true;
	}

	FString TerrainDocument(const FString& CellId, int32 CellX, EConflict Conflict)
	{
		const FString Bounds = Conflict == EConflict::TerrainBounds && CellX == 0
			? TEXT("[0,0,2,1]")
			: FString::Printf(TEXT("[%d,0,%d,1]"), CellX, CellX + 1);
		const FString Spacing = Conflict == EConflict::TerrainSpacing && CellX == 0
			? TEXT("[2,1]")
			: TEXT("[1,1]");
		return FString::Printf(
			LR"({"$schema":"https://alis.world/schemas/world-compiler/terrain-cell-v1.json","schema_version":1,"grid_id":"grid_0123456789abcdef","cell_id":"%s","bounds":%s,"sample_spacing":%s,"core_samples":[[0,0],[0,0]]})",
			*CellId,
			*Bounds,
			*Spacing);
	}

	FString FeatureDocument(const FString& CellId)
	{
		return FString::Printf(
			LR"({"$schema":"https://alis.world/schemas/world-compiler/canonical-feature-v1.json","schema_version":1,"grid_id":"grid_0123456789abcdef","cell_id":"%s","features":[]})",
			*CellId);
	}

	FString CellDocument(
		const FString& CellId,
		int32 CellX,
		const FDocument& Terrain,
		const FDocument& Features,
		EConflict Conflict)
	{
		const FString Owned = Conflict == EConflict::FeatureManifest && CellX == 0
			? TEXT("[\"alis:test:road:1\"]")
			: TEXT("[]");
		return FString::Printf(
			LR"({"$schema":"https://alis.world/schemas/world-compiler/cell-manifest-v1.json","schema_version":1,"grid_id":"grid_0123456789abcdef","cell_id":"%s","cell_x":%d,"cell_y":0,"bounds":[%d,0,%d,1],"owned_feature_ids":%s,"referenced_feature_ids":[],"provenance_result":"accepted","terrain_artifact":{"path":"%s","content_hash":"%s"},"feature_artifact":{"path":"%s","content_hash":"%s"}})",
			*CellId,
			CellX,
			CellX,
			CellX + 1,
			*Owned,
			*Terrain.Path,
			*Terrain.Hash,
			*Features.Path,
			*Features.Hash);
	}

	bool WriteFixture(EConflict Conflict, FString& OutReceiptPath)
	{
		const FString Root = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/ProjectWorldContract"),
			FString::FromInt(static_cast<int32>(Conflict)));
		IFileManager::Get().DeleteDirectory(*Root, false, true);
		IFileManager::Get().MakeDirectory(*Root, true);

		TArray<FDocument> Documents;
		TArray<FDocument> Terrains;
		TArray<FDocument> Features;
		TArray<FDocument> Cells;
		for (int32 CellX = 0; CellX < 2; ++CellX)
		{
			const FString CellId = FString::Printf(TEXT("grid_0123456789abcdef:x%d:y0"), CellX);
			const FString TerrainPath = FString::Printf(TEXT("canonical/terrain/cell_x%d_y0.json"), CellX);
			const FString FeaturePath = FString::Printf(TEXT("canonical/features/cell_x%d_y0.json"), CellX);
			if (!WriteDocument(Root, TerrainPath, TerrainDocument(CellId, CellX, Conflict), Terrains) ||
				!WriteDocument(Root, FeaturePath, FeatureDocument(CellId), Features))
			{
				return false;
			}
		}
		Documents.Append(Terrains);
		Documents.Append(Features);

		for (int32 CellX = 0; CellX < 2; ++CellX)
		{
			const FString CellId = FString::Printf(TEXT("grid_0123456789abcdef:x%d:y0"), CellX);
			const FString CellPath = FString::Printf(TEXT("canonical/cells/cell_x%d_y0.json"), CellX);
			if (!WriteDocument(
				Root,
				CellPath,
				CellDocument(CellId, CellX, Terrains[CellX], Features[CellX], Conflict),
				Cells))
			{
				return false;
			}
		}
		Documents.Append(Cells);

		const int32 CoverageVersion = Conflict == EConflict::CoverageVersion ? 2 : 1;
		const FString FirstCoverageCellId = Conflict == EConflict::CoverageCell
			? TEXT("grid_0123456789abcdef:x99:y0")
			: TEXT("grid_0123456789abcdef:x0:y0");
		const FString Coverage = FString::Printf(
			LR"({"$schema":"https://alis.world/schemas/world-compiler/coverage-manifest-v1.json","schema_version":%d,"profile_id":"fixture","grid_id":"grid_0123456789abcdef","grid":{"canonical_crs":"EPSG:32639","vertical_datum":"EPSG:3855","vertical_origin_m":0,"coordinate_transform":"always_xy","origin":[0,0],"sample_spacing":[1,1],"cell_quads":[1,1],"coordinate_quantization":0.01,"height_quantization":0.1},"cells":[{"cell_id":"%s","path":"%s","sha256":"%s"},{"cell_id":"grid_0123456789abcdef:x1:y0","path":"%s","sha256":"%s"}]})",
			CoverageVersion,
			*FirstCoverageCellId,
			*Cells[0].Path,
			*Cells[0].Hash,
			*Cells[1].Path,
			*Cells[1].Hash);
		const FString ProvenanceSchema = Conflict == EConflict::ProvenanceSchema
			? TEXT("https://alis.world/schemas/world-compiler/provenance-report-v2.json")
			: TEXT("https://alis.world/schemas/world-compiler/provenance-report-v1.json");
		const FString Provenance = FString::Printf(
			LR"({"$schema":"%s","schema_version":1,"profile_id":"fixture","policy_result":"accepted"})",
			*ProvenanceSchema);
		TArray<FDocument> Reports;
		if (!WriteDocument(Root, TEXT("canonical/coverage.json"), Coverage, Reports) ||
			!WriteDocument(Root, TEXT("reports/provenance.json"), Provenance, Reports))
		{
			return false;
		}
		Documents.Append(Reports);

		FString Outputs;
		for (const FDocument& Document : Documents)
		{
			if (!Outputs.IsEmpty())
			{
				Outputs += TEXT(",");
			}
			Outputs += FString::Printf(
				LR"({"kind":"compiler_document","path":"%s","sha256":"%s","byte_size":%lld})",
				*Document.Path,
				*Document.Hash,
				Document.ByteSize);
		}
		const FString Receipt = FString::Printf(
			LR"({"$schema":"https://alis.world/schemas/world-compiler/compile-result-v1.json","schema_version":1,"status":"accepted","profile_id":"fixture","inputs_hash":"0000000000000000000000000000000000000000000000000000000000000000","path_base":"output_root","outputs":[%s],"errors":[]})",
			*Outputs);
		OutReceiptPath = FPaths::Combine(Root, TEXT("compile_result.json"));
		return FFileHelper::SaveStringToFile(Receipt, *OutReceiptPath);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldCanonicalContractsFailClosedTest,
	"Project.World.Realization.CanonicalContractsFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldCanonicalContractsFailClosedTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldCanonicalContractTests;
	FString ReceiptPath;
	TestTrue(TEXT("Receipt-consistent baseline fixture is written."), WriteFixture(EConflict::None, ReceiptPath));
	FProjectWorldCanonicalBundle Bundle;
	FProjectWorldCanonicalValidation Validation;
	TestTrue(TEXT("Compatible v1 fixture loads."), FProjectWorldCanonicalLoader::Load(ReceiptPath, Bundle, Validation));
	TestEqual(TEXT("Grid-owned vertical origin is loaded."), Bundle.HeightOriginMeters, 0.0);

	const TArray<TPair<EConflict, FString>> Cases = {
		{EConflict::CoverageVersion, TEXT("document-contract")},
		{EConflict::ProvenanceSchema, TEXT("document-contract")},
		{EConflict::CoverageCell, TEXT("cell-contract")},
		{EConflict::TerrainBounds, TEXT("terrain-contract")},
		{EConflict::TerrainSpacing, TEXT("terrain-contract")},
		{EConflict::FeatureManifest, TEXT("feature-manifest")}
	};
	for (const TPair<EConflict, FString>& Case : Cases)
	{
		TestTrue(TEXT("Conflicting fixture remains receipt-consistent."), WriteFixture(Case.Key, ReceiptPath));
		Validation = FProjectWorldCanonicalValidation();
		TestFalse(TEXT("Semantic contract conflict fails before mutation."), FProjectWorldCanonicalLoader::Load(ReceiptPath, Bundle, Validation));
		TestEqual(TEXT("Conflict reports the stable boundary error."), Validation.ErrorCode, Case.Value);
	}
	return true;
}

#endif
