// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeometryParsing.h"
#include "ProjectWorldWaterContractParsing.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ProjectWorldCanonical
{
	struct FOutputDescriptor
	{
		FString Path;
		FString Sha256;
		int64 ByteSize = 0;
	};

	void Reject(
		FProjectWorldCanonicalValidation& Validation,
		const TCHAR* Code,
		const FString& Message,
		const FString& Detail = FString())
	{
		if (!Validation.ErrorCode.IsEmpty())
		{
			return;
		}

		Validation.ErrorCode = Code;
		Validation.Message = Message;
		Validation.Detail = Detail;
	}

	bool LoadJsonObject(
		const FString& Path,
		TSharedPtr<FJsonObject>& OutObject,
		FProjectWorldCanonicalValidation& Validation)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			Reject(Validation, TEXT("input-read"), TEXT("Cannot read canonical JSON."), Path);
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
		{
			Reject(Validation, TEXT("invalid-json"), TEXT("Canonical JSON is malformed."), Path);
			return false;
		}

		return true;
	}

	bool RequireString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FString& OutValue,
		FProjectWorldCanonicalValidation& Validation)
	{
		if (!Object->TryGetStringField(Field, OutValue) || OutValue.IsEmpty())
		{
			Reject(Validation, TEXT("contract-field"), TEXT("Required string is missing."), Field);
			return false;
		}
		return true;
	}

	bool RequireInt(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		int32& OutValue,
		FProjectWorldCanonicalValidation& Validation)
	{
		if (!Object->TryGetNumberField(Field, OutValue))
		{
			Reject(Validation, TEXT("contract-field"), TEXT("Required integer is missing."), Field);
			return false;
		}
		return true;
	}

	bool HasContractIdentity(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* ExpectedSchema,
		FProjectWorldCanonicalValidation& Validation)
	{
		FString Schema;
		int32 Version = 0;
		return RequireString(Object, TEXT("$schema"), Schema, Validation) &&
			RequireInt(Object, TEXT("schema_version"), Version, Validation) &&
			Schema == ExpectedSchema && Version == 1;
	}

	bool RequireDouble(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		double& OutValue,
		FProjectWorldCanonicalValidation& Validation)
	{
		if (!Object->TryGetNumberField(Field, OutValue) || !FMath::IsFinite(OutValue))
		{
			Reject(Validation, TEXT("contract-field"), TEXT("Required finite number is missing."), Field);
			return false;
		}
		return true;
	}

	bool RequireArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		const TArray<TSharedPtr<FJsonValue>>*& OutArray,
		FProjectWorldCanonicalValidation& Validation)
	{
		if (!Object->TryGetArrayField(Field, OutArray) || OutArray == nullptr)
		{
			Reject(Validation, TEXT("contract-field"), TEXT("Required array is missing."), Field);
			return false;
		}
		return true;
	}

	bool RequireObject(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TSharedPtr<FJsonObject>& OutObject,
		FProjectWorldCanonicalValidation& Validation)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!Object->TryGetObjectField(Field, ObjectPtr) || ObjectPtr == nullptr || !ObjectPtr->IsValid())
		{
			Reject(Validation, TEXT("contract-field"), TEXT("Required object is missing."), Field);
			return false;
		}
		OutObject = *ObjectPtr;
		return true;
	}

	bool ReadVector2(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FVector2D& OutValue,
		FProjectWorldCanonicalValidation& Validation)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!RequireArray(Object, Field, Values, Validation) || Values->Num() != 2)
		{
			Reject(Validation, TEXT("contract-shape"), TEXT("Expected a two-number vector."), Field);
			return false;
		}

		double X = 0.0;
		double Y = 0.0;
		if (!(*Values)[0]->TryGetNumber(X) || !(*Values)[1]->TryGetNumber(Y) ||
			!FMath::IsFinite(X) || !FMath::IsFinite(Y))
		{
			Reject(Validation, TEXT("contract-shape"), TEXT("Vector contains a non-number."), Field);
			return false;
		}
		OutValue = FVector2D(X, Y);
		return true;
	}

	bool ReadBounds(
		const TSharedPtr<FJsonObject>& Object,
		FVector4d& OutBounds,
		FProjectWorldCanonicalValidation& Validation)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!RequireArray(Object, TEXT("bounds"), Values, Validation) || Values->Num() != 4)
		{
			Reject(Validation, TEXT("contract-shape"), TEXT("Expected four cell bounds."));
			return false;
		}

		double Parsed[4] = {};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (!(*Values)[Index]->TryGetNumber(Parsed[Index]) || !FMath::IsFinite(Parsed[Index]))
			{
				Reject(Validation, TEXT("contract-shape"), TEXT("Cell bounds contain a non-number."));
				return false;
			}
		}
		OutBounds = FVector4d(Parsed[0], Parsed[1], Parsed[2], Parsed[3]);
		return Parsed[0] < Parsed[2] && Parsed[1] < Parsed[3];
	}

	bool ReadStringArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<FString>& OutValues,
		FProjectWorldCanonicalValidation& Validation)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!RequireArray(Object, Field, Values, Validation))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Item;
			if (!Value->TryGetString(Item) || Item.IsEmpty())
			{
				Reject(Validation, TEXT("contract-shape"), TEXT("String array contains an invalid item."), Field);
				return false;
			}
			OutValues.Add(Item);
		}
		return true;
	}

	bool BoundsEqual(const FVector4d& Left, const FVector4d& Right)
	{
		return FMath::IsNearlyEqual(Left.X, Right.X) &&
			FMath::IsNearlyEqual(Left.Y, Right.Y) &&
			FMath::IsNearlyEqual(Left.Z, Right.Z) &&
			FMath::IsNearlyEqual(Left.W, Right.W);
	}

	bool SameUniqueStrings(const TArray<FString>& Left, const TArray<FString>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		TSet<FString> LeftSet;
		for (const FString& Value : Left)
		{
			LeftSet.Add(Value);
		}
		if (LeftSet.Num() != Left.Num())
		{
			return false;
		}

		for (const FString& Value : Right)
		{
			if (!LeftSet.Contains(Value))
			{
				return false;
			}
		}
		return true;
	}

	bool ParseTerrain(
		const TSharedPtr<FJsonObject>& Object,
		const FString& ExpectedGridId,
		const FString& ExpectedCellId,
		const FIntPoint& CellQuads,
		FProjectWorldCanonicalTerrain& OutTerrain,
		FProjectWorldCanonicalValidation& Validation);

	bool ParseFeatures(
		const TSharedPtr<FJsonObject>& Object,
		const FString& ExpectedGridId,
		const FString& ExpectedCellId,
		TMap<FString, FProjectWorldCanonicalFeature>& InOutFeatures,
		TArray<FString>& OutFeatureIds,
		FProjectWorldCanonicalValidation& Validation);
}

bool FProjectWorldCanonicalLoader::Load(
	const FString& CompileResultPath,
	FProjectWorldCanonicalBundle& OutBundle,
	FProjectWorldCanonicalValidation& OutValidation)
{
	using namespace ProjectWorldCanonical;
	OutBundle = FProjectWorldCanonicalBundle();
	OutValidation = FProjectWorldCanonicalValidation();
	OutBundle.CompileResultPath = FPaths::ConvertRelativePathToFull(CompileResultPath);
	const FString OutputRoot = FPaths::GetPath(OutBundle.CompileResultPath);

	TSharedPtr<FJsonObject> ResultObject;
	FString Schema;
	FString Status;
	FString PathBase;
	int32 SchemaVersion = 0;
	if (!ComputeFileSha256(OutBundle.CompileResultPath, OutBundle.CompileResultHash) ||
		!LoadJsonObject(OutBundle.CompileResultPath, ResultObject, OutValidation) ||
		!RequireString(ResultObject, TEXT("$schema"), Schema, OutValidation) ||
		!RequireInt(ResultObject, TEXT("schema_version"), SchemaVersion, OutValidation) ||
		!RequireString(ResultObject, TEXT("status"), Status, OutValidation) ||
		!RequireString(ResultObject, TEXT("profile_id"), OutBundle.ProfileId, OutValidation) ||
		!RequireString(ResultObject, TEXT("inputs_hash"), OutBundle.InputsHash, OutValidation) ||
		!RequireString(ResultObject, TEXT("path_base"), PathBase, OutValidation))
	{
		Reject(OutValidation, TEXT("receipt-read"), TEXT("Cannot load the compiler receipt."), OutBundle.CompileResultPath);
		return false;
	}

	if (Schema != TEXT("https://alis.world/schemas/world-compiler/compile-result-v1.json") ||
		SchemaVersion != 1 || Status != TEXT("accepted") || PathBase != TEXT("output_root"))
	{
		Reject(OutValidation, TEXT("receipt-contract"), TEXT("Compiler receipt is not an accepted v1 output-root receipt."));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
	TMap<FString, FOutputDescriptor> Descriptors;
	if (!RequireArray(ResultObject, TEXT("outputs"), Outputs, OutValidation) ||
		!RequireArray(ResultObject, TEXT("errors"), Errors, OutValidation) || Errors->Num() != 0)
	{
		Reject(OutValidation, TEXT("receipt-errors"), TEXT("Accepted compiler receipt contains errors."));
		return false;
	}

	for (const TSharedPtr<FJsonValue>& OutputValue : *Outputs)
	{
		const TSharedPtr<FJsonObject>* OutputPtr = nullptr;
		FOutputDescriptor Descriptor;
		FString Kind;
		if (!OutputValue->TryGetObject(OutputPtr) || OutputPtr == nullptr ||
			!RequireString(*OutputPtr, TEXT("kind"), Kind, OutValidation) ||
			!RequireString(*OutputPtr, TEXT("path"), Descriptor.Path, OutValidation) ||
			!RequireString(*OutputPtr, TEXT("sha256"), Descriptor.Sha256, OutValidation) ||
			!(*OutputPtr)->TryGetNumberField(TEXT("byte_size"), Descriptor.ByteSize) ||
			Kind != TEXT("compiler_document") || Descriptors.Contains(Descriptor.Path))
		{
			Reject(OutValidation, TEXT("output-descriptor"), TEXT("Compiler output descriptor is invalid."));
			return false;
		}

		FString FullPath;
		FString ActualHash;
		if (!ResolveOwnedOutputPath(OutputRoot, Descriptor.Path, FullPath) ||
			IFileManager::Get().FileSize(*FullPath) != Descriptor.ByteSize ||
			!ComputeFileSha256(FullPath, ActualHash) ||
			!ActualHash.Equals(Descriptor.Sha256, ESearchCase::IgnoreCase))
		{
			Reject(OutValidation, TEXT("output-integrity"), TEXT("Compiler output does not match its receipt."), Descriptor.Path);
			return false;
		}
		Descriptors.Add(Descriptor.Path, MoveTemp(Descriptor));
	}
	OutBundle.VerifiedOutputCount = Descriptors.Num();

	const FOutputDescriptor* CoverageDescriptor = Descriptors.Find(TEXT("canonical/coverage.json"));
	const FOutputDescriptor* ProvenanceDescriptor = Descriptors.Find(TEXT("reports/provenance.json"));
	if (CoverageDescriptor == nullptr || ProvenanceDescriptor == nullptr)
	{
		Reject(OutValidation, TEXT("required-output"), TEXT("Coverage or provenance output is missing."));
		return false;
	}

	FString CoveragePath;
	FString ProvenancePath;
	TSharedPtr<FJsonObject> Coverage;
	TSharedPtr<FJsonObject> Provenance;
	ResolveOwnedOutputPath(OutputRoot, CoverageDescriptor->Path, CoveragePath);
	ResolveOwnedOutputPath(OutputRoot, ProvenanceDescriptor->Path, ProvenancePath);
	FString CoverageProfileId;
	FString ProvenanceProfileId;
	FString PolicyResult;
	if (!LoadJsonObject(CoveragePath, Coverage, OutValidation) ||
		!LoadJsonObject(ProvenancePath, Provenance, OutValidation) ||
		!HasContractIdentity(
			Coverage,
			TEXT("https://alis.world/schemas/world-compiler/coverage-manifest-v1.json"),
			OutValidation) ||
		!HasContractIdentity(
			Provenance,
			TEXT("https://alis.world/schemas/world-compiler/provenance-report-v1.json"),
			OutValidation) ||
		!RequireString(Coverage, TEXT("profile_id"), CoverageProfileId, OutValidation) ||
		!RequireString(Provenance, TEXT("profile_id"), ProvenanceProfileId, OutValidation) ||
		!RequireString(Provenance, TEXT("policy_result"), PolicyResult, OutValidation) ||
		CoverageProfileId != OutBundle.ProfileId || ProvenanceProfileId != OutBundle.ProfileId ||
		PolicyResult != TEXT("accepted"))
	{
		Reject(OutValidation, TEXT("document-contract"), TEXT("Coverage or provenance contract is incompatible."));
		return false;
	}

	TSharedPtr<FJsonObject> Grid;
	FVector2D CellQuadsValue;
	if (!RequireString(Coverage, TEXT("world_data_plugin"), OutBundle.WorldDataPluginName, OutValidation) ||
		!ReadVector2(Coverage, TEXT("engine_georeference_origin"), OutBundle.EngineGeoreferenceOriginMeters, OutValidation) ||
		!RequireString(Coverage, TEXT("grid_id"), OutBundle.GridId, OutValidation) ||
		!RequireObject(Coverage, TEXT("grid"), Grid, OutValidation) ||
		!RequireString(Grid, TEXT("canonical_crs"), OutBundle.CanonicalCrs, OutValidation) ||
		!RequireString(Grid, TEXT("vertical_datum"), OutBundle.VerticalDatum, OutValidation) ||
		!RequireString(Grid, TEXT("coordinate_transform"), OutBundle.CoordinateTransform, OutValidation) ||
		!ReadVector2(Grid, TEXT("origin"), OutBundle.LatticeOriginMeters, OutValidation) ||
		!ReadVector2(Grid, TEXT("sample_spacing"), OutBundle.SampleSpacingMeters, OutValidation) ||
		!ReadVector2(Grid, TEXT("cell_quads"), CellQuadsValue, OutValidation) ||
		!RequireDouble(Grid, TEXT("coordinate_quantization"), OutBundle.CoordinateQuantizationMeters, OutValidation) ||
		!RequireDouble(Grid, TEXT("height_quantization"), OutBundle.HeightQuantizationMeters, OutValidation) ||
		!RequireDouble(Grid, TEXT("vertical_origin_m"), OutBundle.HeightOriginMeters, OutValidation))
	{
		return false;
	}
	if (!FMath::IsNearlyEqual(
			OutBundle.EngineGeoreferenceOriginMeters.X,
			FMath::RoundToDouble(OutBundle.EngineGeoreferenceOriginMeters.X)) ||
		!FMath::IsNearlyEqual(
			OutBundle.EngineGeoreferenceOriginMeters.Y,
			FMath::RoundToDouble(OutBundle.EngineGeoreferenceOriginMeters.Y)))
	{
		Reject(
			OutValidation,
			TEXT("georeference-origin"),
			TEXT("Engine projected georeference origin must use integer metres."));
		return false;
	}
	OutBundle.CellQuads = FIntPoint(FMath::RoundToInt(CellQuadsValue.X), FMath::RoundToInt(CellQuadsValue.Y));
	if (OutBundle.CellQuads.X <= 0 || OutBundle.CellQuads.Y <= 0 ||
		!FMath::IsNearlyEqual(CellQuadsValue.X, OutBundle.CellQuads.X) ||
		!FMath::IsNearlyEqual(CellQuadsValue.Y, OutBundle.CellQuads.Y))
	{
		Reject(OutValidation, TEXT("grid-shape"), TEXT("Canonical cell quads must be positive integers."));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
	if (!RequireArray(Coverage, TEXT("cells"), Cells, OutValidation) || Cells->Num() == 0)
	{
		Reject(OutValidation, TEXT("coverage-empty"), TEXT("Coverage contains no canonical cells."));
		return false;
	}

	TSet<FString> CoverageCellIds;
	for (const TSharedPtr<FJsonValue>& CellValue : *Cells)
	{
		const TSharedPtr<FJsonObject>* CoverageCellPtr = nullptr;
		FString CoverageCellId;
		FString CellPath;
		FString CellHash;
		if (!CellValue->TryGetObject(CoverageCellPtr) || CoverageCellPtr == nullptr ||
			!RequireString(*CoverageCellPtr, TEXT("cell_id"), CoverageCellId, OutValidation) ||
			!RequireString(*CoverageCellPtr, TEXT("path"), CellPath, OutValidation) ||
			!RequireString(*CoverageCellPtr, TEXT("sha256"), CellHash, OutValidation) ||
			CoverageCellIds.Contains(CoverageCellId))
		{
			Reject(OutValidation, TEXT("coverage-cell"), TEXT("Coverage cell identity is missing or duplicated."), CoverageCellId);
			return false;
		}
		CoverageCellIds.Add(CoverageCellId);

		const FOutputDescriptor* CellDescriptor = Descriptors.Find(CellPath);
		FString FullCellPath;
		TSharedPtr<FJsonObject> CellObject;
		if (CellDescriptor == nullptr || !CellHash.Equals(CellDescriptor->Sha256, ESearchCase::IgnoreCase) ||
			!ResolveOwnedOutputPath(OutputRoot, CellPath, FullCellPath) ||
			!LoadJsonObject(FullCellPath, CellObject, OutValidation))
		{
			Reject(OutValidation, TEXT("cell-integrity"), TEXT("Coverage cell does not match the compiler receipt."), CellPath);
			return false;
		}

		FProjectWorldCanonicalCell Cell;
		FString CellSchema;
		FString CellGridId;
		FString CellProvenance;
		int32 CellSchemaVersion = 0;
		TSharedPtr<FJsonObject> TerrainArtifact;
		TSharedPtr<FJsonObject> FeatureArtifact;
		FString TerrainPath;
		FString FeaturePath;
		if (!RequireString(CellObject, TEXT("$schema"), CellSchema, OutValidation) ||
			!RequireInt(CellObject, TEXT("schema_version"), CellSchemaVersion, OutValidation) ||
			!RequireString(CellObject, TEXT("grid_id"), CellGridId, OutValidation) ||
			!RequireString(CellObject, TEXT("cell_id"), Cell.CellId, OutValidation) ||
			!RequireInt(CellObject, TEXT("cell_x"), Cell.CellX, OutValidation) ||
			!RequireInt(CellObject, TEXT("cell_y"), Cell.CellY, OutValidation) ||
			!ReadBounds(CellObject, Cell.Bounds, OutValidation) ||
			!ReadStringArray(CellObject, TEXT("owned_feature_ids"), Cell.OwnedFeatureIds, OutValidation) ||
			!ReadStringArray(CellObject, TEXT("referenced_feature_ids"), Cell.ReferencedFeatureIds, OutValidation) ||
			!RequireString(CellObject, TEXT("provenance_result"), CellProvenance, OutValidation) ||
			!RequireObject(CellObject, TEXT("terrain_artifact"), TerrainArtifact, OutValidation) ||
			!RequireObject(CellObject, TEXT("feature_artifact"), FeatureArtifact, OutValidation) ||
			!RequireString(TerrainArtifact, TEXT("path"), TerrainPath, OutValidation) ||
			!RequireString(TerrainArtifact, TEXT("content_hash"), Cell.Terrain.ArtifactHash, OutValidation) ||
			!RequireString(FeatureArtifact, TEXT("path"), FeaturePath, OutValidation) ||
			!RequireString(FeatureArtifact, TEXT("content_hash"), Cell.FeatureArtifactHash, OutValidation))
		{
			return false;
		}

		if (CellSchema != TEXT("https://alis.world/schemas/world-compiler/cell-manifest-v1.json") ||
			CellSchemaVersion != 1 || CellGridId != OutBundle.GridId || Cell.CellId != CoverageCellId ||
			CellProvenance != TEXT("accepted"))
		{
			Reject(OutValidation, TEXT("cell-contract"), TEXT("Cell identity, schema, or provenance is invalid."), Cell.CellId);
			return false;
		}

		const FOutputDescriptor* TerrainDescriptor = Descriptors.Find(TerrainPath);
		const FOutputDescriptor* FeatureDescriptor = Descriptors.Find(FeaturePath);
		FString TerrainFullPath;
		FString FeatureFullPath;
		TSharedPtr<FJsonObject> TerrainObject;
		TSharedPtr<FJsonObject> FeatureObject;
		TArray<FString> DocumentFeatureIds;
		if (TerrainDescriptor == nullptr || FeatureDescriptor == nullptr ||
			!Cell.Terrain.ArtifactHash.Equals(TerrainDescriptor->Sha256, ESearchCase::IgnoreCase) ||
			!Cell.FeatureArtifactHash.Equals(FeatureDescriptor->Sha256, ESearchCase::IgnoreCase) ||
			!ResolveOwnedOutputPath(OutputRoot, TerrainPath, TerrainFullPath) ||
			!ResolveOwnedOutputPath(OutputRoot, FeaturePath, FeatureFullPath) ||
			!LoadJsonObject(TerrainFullPath, TerrainObject, OutValidation) ||
			!LoadJsonObject(FeatureFullPath, FeatureObject, OutValidation) ||
			!ParseTerrain(TerrainObject, OutBundle.GridId, Cell.CellId, OutBundle.CellQuads, Cell.Terrain, OutValidation) ||
			!ParseFeatures(
				FeatureObject,
				OutBundle.GridId,
				Cell.CellId,
				OutBundle.Features,
				DocumentFeatureIds,
				OutValidation))
		{
			return false;
		}

		if (!BoundsEqual(Cell.Terrain.Bounds, Cell.Bounds) ||
			!Cell.Terrain.SampleSpacing.Equals(OutBundle.SampleSpacingMeters) ||
			Cell.Terrain.VerticalDatum != OutBundle.VerticalDatum ||
			!FMath::IsNearlyEqual(
				Cell.Terrain.SamplingQuantizationResidualMeters,
				OutBundle.HeightQuantizationMeters * 0.5))
		{
			Reject(OutValidation, TEXT("terrain-contract"), TEXT("Terrain bounds, spacing, datum, or quantization residual does not match its grid."), Cell.CellId);
			return false;
		}

		if (!SameUniqueStrings(Cell.OwnedFeatureIds, DocumentFeatureIds))
		{
			Reject(OutValidation, TEXT("feature-manifest"), TEXT("Owned feature identities do not match the canonical feature document."), Cell.CellId);
			return false;
		}
		OutBundle.Cells.Add(MoveTemp(Cell));
	}

	for (const FProjectWorldCanonicalCell& Cell : OutBundle.Cells)
	{
		for (const FString& FeatureId : Cell.OwnedFeatureIds)
		{
			const FProjectWorldCanonicalFeature* Feature = OutBundle.Features.Find(FeatureId);
			if (Feature == nullptr || Feature->OwnerCellId != Cell.CellId)
			{
				Reject(OutValidation, TEXT("owned-feature"), TEXT("Owned feature is missing from its canonical document."), FeatureId);
				return false;
			}
		}
	}

	for (const FProjectWorldCanonicalCell& Cell : OutBundle.Cells)
	{
		for (const FString& FeatureId : Cell.ReferencedFeatureIds)
		{
			if (!OutBundle.Features.Contains(FeatureId))
			{
				Reject(OutValidation, TEXT("referenced-feature"), TEXT("Referenced feature has no canonical owner."), FeatureId);
				return false;
			}
		}
	}

	return true;
}

namespace ProjectWorldCanonical
{
	bool ParseTerrain(
		const TSharedPtr<FJsonObject>& Object,
		const FString& ExpectedGridId,
		const FString& ExpectedCellId,
		const FIntPoint& CellQuads,
		FProjectWorldCanonicalTerrain& OutTerrain,
		FProjectWorldCanonicalValidation& Validation)
	{
		FString Schema;
		FString GridId;
		FString CellId;
		int32 SchemaVersion = 0;
		TSharedPtr<FJsonObject> VerticalProvenance;
		if (!RequireString(Object, TEXT("$schema"), Schema, Validation) ||
			!RequireInt(Object, TEXT("schema_version"), SchemaVersion, Validation) ||
			!RequireString(Object, TEXT("grid_id"), GridId, Validation) ||
			!RequireString(Object, TEXT("cell_id"), CellId, Validation) ||
			!ReadBounds(Object, OutTerrain.Bounds, Validation) ||
			!ReadVector2(Object, TEXT("sample_spacing"), OutTerrain.SampleSpacing, Validation) ||
			!RequireObject(Object, TEXT("vertical_provenance"), VerticalProvenance, Validation) ||
			!RequireString(
				VerticalProvenance,
				TEXT("source_ref"),
				OutTerrain.VerticalProvenanceId,
				Validation) ||
			!RequireString(
				VerticalProvenance,
				TEXT("vertical_datum"),
				OutTerrain.VerticalDatum,
				Validation) ||
			!RequireString(
				VerticalProvenance,
				TEXT("confidence"),
				OutTerrain.VerticalConfidence,
				Validation) ||
			!RequireDouble(
				VerticalProvenance,
				TEXT("source_accuracy_m"),
				OutTerrain.VerticalSourceAccuracyMeters,
				Validation) ||
			!RequireDouble(
				VerticalProvenance,
				TEXT("sampling_quantization_residual_m"),
				OutTerrain.SamplingQuantizationResidualMeters,
				Validation))
		{
			return false;
		}

		if (Schema != TEXT("https://alis.world/schemas/world-compiler/terrain-cell-v1.json") ||
			SchemaVersion != 1 || GridId != ExpectedGridId || CellId != ExpectedCellId ||
			OutTerrain.VerticalProvenanceId.IsEmpty() ||
			OutTerrain.VerticalDatum.IsEmpty() ||
			OutTerrain.VerticalConfidence.IsEmpty() ||
			OutTerrain.VerticalSourceAccuracyMeters < 0.0 ||
			OutTerrain.SamplingQuantizationResidualMeters < 0.0)
		{
			Reject(Validation, TEXT("terrain-identity"), TEXT("Terrain identity or schema does not match its cell."), CellId);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!RequireArray(Object, TEXT("core_samples"), Rows, Validation) || Rows->Num() != CellQuads.Y + 1)
		{
			Reject(Validation, TEXT("terrain-shape"), TEXT("Terrain row count does not match the canonical grid."), CellId);
			return false;
		}

		OutTerrain.SamplesX = CellQuads.X + 1;
		OutTerrain.SamplesY = CellQuads.Y + 1;
		for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
		{
			const TArray<TSharedPtr<FJsonValue>>* Row = nullptr;
			if (!RowValue->TryGetArray(Row) || Row == nullptr || Row->Num() != OutTerrain.SamplesX)
			{
				Reject(Validation, TEXT("terrain-shape"), TEXT("Terrain column count does not match the canonical grid."), CellId);
				return false;
			}

			for (const TSharedPtr<FJsonValue>& Sample : *Row)
			{
				double Height = 0.0;
				if (!Sample->TryGetNumber(Height) || !FMath::IsFinite(Height))
				{
					Reject(Validation, TEXT("terrain-value"), TEXT("Terrain contains a non-finite height."), CellId);
					return false;
				}
				OutTerrain.HeightsMeters.Add(Height);
			}
		}
		return true;
	}

	bool ParseFeatures(
		const TSharedPtr<FJsonObject>& Object,
		const FString& ExpectedGridId,
		const FString& ExpectedCellId,
		TMap<FString, FProjectWorldCanonicalFeature>& InOutFeatures,
		TArray<FString>& OutFeatureIds,
		FProjectWorldCanonicalValidation& Validation)
	{
		FString Schema;
		FString GridId;
		FString CellId;
		int32 SchemaVersion = 0;
		const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
		if (!RequireString(Object, TEXT("$schema"), Schema, Validation) ||
			!RequireInt(Object, TEXT("schema_version"), SchemaVersion, Validation) ||
			!RequireString(Object, TEXT("grid_id"), GridId, Validation) ||
			!RequireString(Object, TEXT("cell_id"), CellId, Validation) ||
			!RequireArray(Object, TEXT("features"), Features, Validation))
		{
			return false;
		}

		if (Schema != TEXT("https://alis.world/schemas/world-compiler/canonical-feature-v1.json") ||
			SchemaVersion != 1 || GridId != ExpectedGridId || CellId != ExpectedCellId)
		{
			Reject(Validation, TEXT("feature-identity"), TEXT("Feature document identity does not match its cell."), CellId);
			return false;
		}

		for (const TSharedPtr<FJsonValue>& FeatureValue : *Features)
		{
			const TSharedPtr<FJsonObject>* FeaturePtr = nullptr;
			if (!FeatureValue->TryGetObject(FeaturePtr) || FeaturePtr == nullptr || !FeaturePtr->IsValid())
			{
				Reject(Validation, TEXT("feature-shape"), TEXT("Feature entry is not an object."), CellId);
				return false;
			}

			FProjectWorldCanonicalFeature Feature;
			FString Provenance;
			TSharedPtr<FJsonObject> Geometry;
			TSharedPtr<FJsonObject> Attributes;
			if (!RequireString(*FeaturePtr, TEXT("feature_id"), Feature.FeatureId, Validation) ||
				!RequireString(*FeaturePtr, TEXT("feature_class"), Feature.FeatureClass, Validation) ||
				!RequireString(*FeaturePtr, TEXT("owner_cell_id"), Feature.OwnerCellId, Validation) ||
				!RequireString(*FeaturePtr, TEXT("provenance_result"), Provenance, Validation) ||
				!RequireObject(*FeaturePtr, TEXT("geometry"), Geometry, Validation) ||
				!RequireObject(*FeaturePtr, TEXT("attributes"), Attributes, Validation) ||
				!ProjectWorldGeometryParsing::ReadGeometry(
					Geometry,
					Feature.GeometryType,
					Feature.GeometryPoints,
					Validation,
					&Feature.GeometryParts,
					&Feature.GeometryPolygons))
			{
				return false;
			}

			if (Feature.OwnerCellId != ExpectedCellId || Provenance != TEXT("accepted") ||
				InOutFeatures.Contains(Feature.FeatureId))
			{
				Reject(Validation, TEXT("feature-ownership"), TEXT("Feature ownership, provenance, or identity is invalid."), Feature.FeatureId);
				return false;
			}

			Attributes->TryGetNumberField(TEXT("width_m"), Feature.WidthMeters);
			Attributes->TryGetNumberField(TEXT("height_m"), Feature.HeightMeters);
			Attributes->TryGetStringField(TEXT("road_class"), Feature.RoadClass);
			Attributes->TryGetStringField(TEXT("vegetation_class"), Feature.VegetationClass);
			Attributes->TryGetStringField(TEXT("foliage_class"), Feature.FoliageClass);
			Attributes->TryGetStringField(TEXT("leaf_type"), Feature.LeafType);
			Attributes->TryGetStringField(TEXT("leaf_cycle"), Feature.LeafCycle);
			Attributes->TryGetStringField(TEXT("species"), Feature.Species);
			if (!ProjectWorldWaterContractParsing::Read(Attributes, Feature, Validation))
			{
				return false;
			}
			const TArray<TSharedPtr<FJsonValue>>* Representations = nullptr;
			if (!RequireArray(*FeaturePtr, TEXT("representations"), Representations, Validation))
			{
				return false;
			}

			for (const TSharedPtr<FJsonValue>& RepresentationValue : *Representations)
			{
				const TSharedPtr<FJsonObject>* RepresentationPtr = nullptr;
				if (!RepresentationValue->TryGetObject(RepresentationPtr) || RepresentationPtr == nullptr)
				{
					Reject(Validation, TEXT("feature-representation"), TEXT("Feature representation is invalid."), Feature.FeatureId);
					return false;
				}

				FProjectWorldCanonicalRepresentation Representation;
				if (!RequireString(*RepresentationPtr, TEXT("cell_id"), Representation.CellId, Validation) ||
					!RequireString(*RepresentationPtr, TEXT("representation"), Representation.Kind, Validation))
				{
					return false;
				}

				const TSharedPtr<FJsonObject>* RepresentationGeometry = nullptr;
				if ((*RepresentationPtr)->TryGetObjectField(TEXT("geometry"), RepresentationGeometry) &&
					RepresentationGeometry != nullptr)
				{
					FString Type;
					if (!ProjectWorldGeometryParsing::ReadGeometry(
						*RepresentationGeometry,
						Type,
						Representation.Points,
						Validation,
						&Representation.Parts,
						&Representation.Polygons))
					{
						return false;
					}
				}
				Feature.Representations.Add(MoveTemp(Representation));
			}

			OutFeatureIds.Add(Feature.FeatureId);
			InOutFeatures.Add(Feature.FeatureId, MoveTemp(Feature));
		}
		return true;
	}
}
