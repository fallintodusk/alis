// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldLandscapeRealization.h"
#include "ProjectWorldPartitionPolicy.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldSemanticEvidence.h"
#include "ProjectWorldTerrainVerification.h"
#include "Utilities/ProjectSha256.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeStreamingProxy.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "NaniteSceneProxy.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldNativeTwinTests
{
	FProjectWorldCanonicalBundle BuildLandscapeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = TEXT("native_twin_grid");
		Bundle.InputsHash = TEXT("native_twin_input");
		Bundle.CellQuads = FIntPoint(63, 63);
		Bundle.SampleSpacingMeters = FVector2D(1.0, 1.0);
		Bundle.HeightQuantizationMeters = 0.1;
		Bundle.CoordinateQuantizationMeters = 0.01;
		for (int32 CellY = 0; CellY < 3; ++CellY)
		{
			for (int32 CellX = 0; CellX < 2; ++CellX)
			{
				FProjectWorldCanonicalCell Cell;
				Cell.CellId = FString::Printf(TEXT("cell_%d_%d"), CellX, CellY);
				Cell.CellX = CellX;
				Cell.CellY = CellY;
				Cell.Bounds = FVector4d(
					CellX * 63.0,
					CellY * 63.0,
					(CellX + 1) * 63.0,
					(CellY + 1) * 63.0);
				Cell.Terrain.Bounds = Cell.Bounds;
				Cell.Terrain.SampleSpacing = Bundle.SampleSpacingMeters;
				Cell.Terrain.SamplesX = 64;
				Cell.Terrain.SamplesY = 64;
				Cell.Terrain.ArtifactHash = FString::Printf(TEXT("terrain_%d_%d"), CellX, CellY);
				for (int32 Row = 0; Row < 64; ++Row)
				{
					for (int32 Column = 0; Column < 64; ++Column)
					{
						const int32 GlobalX = CellX * 63 + Column;
						const int32 GlobalY = (2 - CellY) * 63 + Row;
						Cell.Terrain.HeightsMeters.Add(10.0 + GlobalX * 0.1 + GlobalY * 0.05);
					}
				}
				Bundle.Cells.Add(MoveTemp(Cell));
			}
		}
		return Bundle;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldNativeLandscapeTwinTest,
	"Project.World.Realization.NativeTwin.LandscapePartitionAndEditLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldNativeLandscapeTwinTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldNativeTwinTests;
	UWorld* World = GEditor->NewMap(true);
	FProjectWorldCanonicalBundle Bundle = BuildLandscapeBundle();
	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldRealizationResult Result;
	FString Error;
	TestTrue(
		TEXT("The twin starts with the production zero-HLOD partition policy."),
		ProjectWorldPartitionPolicy::DisableHLOD(World, Error));
	World->GetOutermost()->SetDirtyFlag(false);
	TestTrue(
		TEXT("Reapplying zero-HLOD policy accepts UE's empty structural partition row."),
		ProjectWorldPartitionPolicy::DisableHLOD(World, Error));
	TestFalse(
		TEXT("An empty structural HLOD partition row does not dirty the generated map."),
		World->GetOutermost()->IsDirty());
	const bool bCreated = ProjectWorldLandscapeRealization::CreateOrUpdate(
		World, Bundle, Layout, TEXT("native_twin"), 1, Result, Error);
	TestTrue(
		TEXT("The Landscape-compatible twin creates one logical Landscape."),
		bCreated);
	if (!bCreated)
	{
		AddError(Error);
	}

	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It))
		{
			Landscape = *It;
			break;
		}
	}
	TestNotNull(TEXT("The logical Landscape exists before partitioning."), Landscape);
	if (Landscape == nullptr)
	{
		return false;
	}
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	TestNotNull(TEXT("The logical Landscape exposes LandscapeInfo."), LandscapeInfo);
	if (LandscapeInfo == nullptr)
	{
		return false;
	}
	int32 ProxyCount = 0;
	int32 ProxyComponentCount = 0;
	TMap<FString, ALandscapeStreamingProxy*> ProxiesByCell;
	for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
	{
		++ProxyCount;
		ProxyComponentCount += It->LandscapeComponents.Num();
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			if (It->Tags.Contains(FName(*(FString(TEXT("ProjectWorld.TerrainCell=")) + Cell.CellId))))
			{
				ProxiesByCell.Add(Cell.CellId, *It);
			}
		}
	}
	TestEqual(TEXT("The 2x3 twin creates six streaming proxies."), ProxyCount, 6);
	TestEqual(TEXT("Each canonical cell remains one Landscape component."), ProxyComponentCount, 6);
	TestEqual(TEXT("Every 2x3 canonical cell resolves to one tagged proxy."), ProxiesByCell.Num(), 6);
	TestEqual(TEXT("The logical Landscape owns no streamed components."), Landscape->LandscapeComponents.Num(), 0);
	TestNotNull(
		TEXT("Generated Base survives native partitioning."),
		Landscape->GetEditLayer(TEXT("Generated Base")));
	TestNotNull(
		TEXT("Authored Corrections survives native partitioning."),
		Landscape->GetEditLayer(TEXT("Authored Corrections")));
	for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
	{
		ALandscapeStreamingProxy* const* Proxy = ProxiesByCell.Find(Cell.CellId);
		if (Proxy == nullptr || (*Proxy)->LandscapeComponents.Num() != 1)
		{
			AddError(FString::Printf(TEXT("Cell has no one-component proxy: %s"), *Cell.CellId));
			continue;
		}
		const FIntPoint SectionBase = (*Proxy)->LandscapeComponents[0]->GetSectionBase();
		const FVector NorthWest = FProjectWorldCanonicalLoader::UnrealToCanonical(
			Bundle,
			Landscape->GetActorTransform().TransformPosition(FVector(SectionBase.X, SectionBase.Y, 0.0)));
		const FVector SouthEast = FProjectWorldCanonicalLoader::UnrealToCanonical(
			Bundle,
			Landscape->GetActorTransform().TransformPosition(FVector(SectionBase.X + 63, SectionBase.Y + 63, 0.0)));
		TestTrue(TEXT("Proxy north-west corner matches canonical cell bounds."),
			FVector2D(NorthWest.X, NorthWest.Y).Equals(FVector2D(Cell.Bounds.X, Cell.Bounds.W), 0.0001));
		TestTrue(TEXT("Proxy south-east corner matches canonical cell bounds."),
			FVector2D(SouthEast.X, SouthEast.Y).Equals(FVector2D(Cell.Bounds.Z, Cell.Bounds.Y), 0.0001));
	}

	FProjectWorldRealizationResult CleanupResult;
	TestTrue(
		TEXT("Stale generated-actor cleanup preserves the native Landscape proxy family."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World, Bundle, FString(), true, CleanupResult));
	int32 PreservedProxyCount = 0;
	for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
	{
		TestTrue(
			TEXT("Each streaming proxy is part of the owned logical Landscape family."),
			ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It));
		++PreservedProxyCount;
	}
	TestEqual(
		TEXT("Stale cleanup preserves all six Landscape streaming proxies."),
		PreservedProxyCount,
		6);

	UPackage* LogicalPackage = Landscape->GetPackage();
	LogicalPackage->SetDirtyFlag(false);
	for (const TPair<FString, ALandscapeStreamingProxy*>& Entry : ProxiesByCell)
	{
		Entry.Value->GetPackage()->SetDirtyFlag(false);
	}
	UMaterialInterface* const UpdatedLandscapeMaterial = NewObject<UMaterial>();
	FProjectWorldRealizationResult MaterialResult;
	TestTrue(
		TEXT("A universal material migration updates the existing Landscape family."),
		ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("native_twin"), 1, MaterialResult, Error, UpdatedLandscapeMaterial));
	TestEqual(TEXT("A material-only migration updates zero terrain components."),
		MaterialResult.UpdatedLandscapeComponentCount, 0);
	for (const TPair<FString, ALandscapeStreamingProxy*>& Entry : ProxiesByCell)
	{
		TestTrue(
			TEXT("Every migrated Landscape proxy package becomes durable."),
			Entry.Value->GetPackage()->IsDirty());
		TestTrue(
			TEXT("Every existing Landscape proxy adopts the updated logical material."),
			Entry.Value->LandscapeMaterial == UpdatedLandscapeMaterial);
		Entry.Value->GetPackage()->SetDirtyFlag(false);
	}
	LogicalPackage->SetDirtyFlag(false);
	Bundle.Cells[0].Terrain.ArtifactHash = TEXT("terrain_0_0_changed");
	Bundle.Cells[0].Terrain.HeightsMeters[65] += 0.1;
	Bundle.InputsHash = TEXT("native_twin_input_terrain_changed");
	FProjectWorldRealizationResult IncrementalResult;
	const bool bUpdated = ProjectWorldLandscapeRealization::CreateOrUpdate(
		World, Bundle, Layout, TEXT("native_twin"), 1, IncrementalResult, Error, UpdatedLandscapeMaterial);
	TestTrue(
		TEXT("The real Landscape update path accepts an already-partitioned Landscape."),
		bUpdated);
	if (!bUpdated)
	{
		AddError(Error);
	}
	TestEqual(
		TEXT("One changed canonical cell updates one partitioned Landscape component."),
		IncrementalResult.UpdatedLandscapeComponentCount,
		1);
	TestFalse(TEXT("A cell-local terrain change does not dirty the logical map package."), LogicalPackage->IsDirty());
	for (const TPair<FString, ALandscapeStreamingProxy*>& Entry : ProxiesByCell)
	{
		TestEqual(
			TEXT("Only the changed terrain proxy package becomes dirty."),
			Entry.Value->GetPackage()->IsDirty(),
			Entry.Key == Bundle.Cells[0].CellId);
	}

	LogicalPackage->SetDirtyFlag(false);
	for (const TPair<FString, ALandscapeStreamingProxy*>& Entry : ProxiesByCell)
	{
		Entry.Value->GetPackage()->SetDirtyFlag(false);
	}
	Bundle.InputsHash = TEXT("native_twin_input_water_only_changed");
	FProjectWorldRealizationResult WaterOnlyResult;
	TestTrue(
		TEXT("A water-only canonical identity change leaves terrain realization valid."),
		ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("native_twin"), 1, WaterOnlyResult, Error, UpdatedLandscapeMaterial));
	TestEqual(TEXT("Water-only input changes update zero terrain components."), WaterOnlyResult.UpdatedLandscapeComponentCount, 0);
	TestFalse(TEXT("Water-only input changes do not dirty the logical map package."), LogicalPackage->IsDirty());
	for (const TPair<FString, ALandscapeStreamingProxy*>& Entry : ProxiesByCell)
	{
		TestFalse(TEXT("Water-only input changes do not dirty terrain proxy packages."), Entry.Value->GetPackage()->IsDirty());
	}
	int32 IncrementalProxyCount = 0;
	for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
	{
		++IncrementalProxyCount;
		TestEqual(TEXT("Each partitioned proxy remains one component."), It->LandscapeComponents.Num(), 1);
		TestTrue(
			TEXT("Each partitioned proxy carries exact canonical-cell identity."),
			It->Tags.ContainsByPredicate([](const FName& Tag)
			{
				return Tag.ToString().StartsWith(TEXT("ProjectWorld.TerrainCell="));
			}));
	}
	TestEqual(TEXT("Incremental realization preserves the six-proxy topology."), IncrementalProxyCount, 6);
	ProjectWorldPartitionPolicy::DisableGeneratedActorHLOD(World);
	TestEqual(
		TEXT("Native Landscape partitioning creates no HLOD authority."),
		ProjectWorldPartitionPolicy::CountHLODLayerReferences(World),
		0);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldNativeLandscapeTwinTest,
	"Project.World.Realization.NativeTwin.LandscapePartitionAndEditLayers",
	"[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldLandscapeProxySemanticIdentityTest,
	"Project.World.Realization.NativeTwin.LandscapeProxySemanticIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldLandscapeProxySemanticIdentityTest::RunTest(const FString& Parameters)
{
	auto CaptureProxy = [this](FProjectWorldRealizationResult& OutResult)
	{
		UWorld* World = GEditor->NewMap(false);
		ALandscapeStreamingProxy* Proxy = World->SpawnActor<ALandscapeStreamingProxy>();
		if (!TestNotNull(TEXT("The semantic fixture creates a Landscape proxy."), Proxy))
		{
			return false;
		}
		Proxy->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
		Proxy->Tags.Add(FName(TEXT("ProjectWorld.Landscape.v1")));
		Proxy->Tags.Add(FName(TEXT("ProjectWorld.TerrainCell=native_twin_grid:x0:y0")));
		FString Error;
		return TestTrue(
			TEXT("Landscape proxy semantics can be captured."),
			ProjectWorldSemanticEvidence::Capture(World, OutResult, Error));
	};

	FProjectWorldRealizationResult First;
	FProjectWorldRealizationResult Rebuilt;
	if (!CaptureProxy(First) || !CaptureProxy(Rebuilt))
	{
		return false;
	}
	TestEqual(
		TEXT("Epic-generated proxy GUID churn does not change canonical-cell semantics."),
		Rebuilt.SemanticFingerprint,
		First.SemanticFingerprint);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldLandscapeProxySemanticIdentityTest,
	"Project.World.Realization.NativeTwin.LandscapeProxySemanticIdentity",
	"[Fast][Architecture][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldSingleLayerWaterNaniteCompatibilityTest,
	"Project.World.Realization.NativeTwin.SingleLayerWaterNaniteCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldSingleLayerWaterNaniteCompatibilityTest::RunTest(const FString& Parameters)
{
	FMaterialShadingModelField DefaultLit;
	DefaultLit.AddShadingModel(MSM_DefaultLit);
	FMaterialShadingModelField SingleLayerWater;
	SingleLayerWater.AddShadingModel(MSM_SingleLayerWater);
	TestTrue(
		TEXT("UE 5.8 Nanite accepts the ordinary opaque control material."),
		Nanite::IsSupportedShadingModel(DefaultLit));
	TestFalse(
		TEXT("UE 5.8 Nanite explicitly rejects the Single Layer Water shading model."),
		Nanite::IsSupportedShadingModel(SingleLayerWater));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldSingleLayerWaterNaniteCompatibilityTest,
	"Project.World.Realization.NativeTwin.SingleLayerWaterNaniteCompatibility",
	"[Fast][Architecture][World]")

// Terrain-correctness proof at L0. Structural gates (counts, proxy identity, semantic hashes,
// georeference XY) all pass on a completely flat Landscape, so they answer only "did Unreal
// produce the same terrain again". This answers the different question: "did Unreal produce
// the terrain the canonical authority specified".
//
// SCOPE: this creates the Landscape and reads it back inside the same Editor process, so it
// proves only
//     created Generated Base layer authority == canonical terrain.
// It does NOT prove persistence. The separate -NullRHI L1 owns
//     save -> process exit -> reload -> persisted Generated Base == canonical terrain.
// Keep the two apart: this defect escaped precisely by conflating kinds of evidence.
//
// The fixture ramp is asymmetric in X and Y (0.1 vs 0.05 m per quad), so zeroing, a constant
// height, an inverted row order, and a transposed grid all fail rather than coincidentally pass.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldGeneratedBaseHeightAuthorityTest,
	"Project.World.Realization.NativeTwin.GeneratedBaseHeightAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGeneratedBaseHeightAuthorityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldNativeTwinTests;
	UWorld* World = GEditor->NewMap(true);
	FProjectWorldCanonicalBundle Bundle = BuildLandscapeBundle();
	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldRealizationResult Result;
	FString Error;
	if (!ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("height_authority"), 1, Result, Error))
	{
		AddError(FString::Printf(TEXT("Landscape creation failed: %s"), *Error));
		return false;
	}

	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It))
		{
			Landscape = *It;
			break;
		}
	}
	if (Landscape == nullptr)
	{
		AddError(TEXT("No generated Landscape was created."));
		return false;
	}
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	const ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(TEXT("Generated Base"));
	if (LandscapeInfo == nullptr || BaseLayer == nullptr)
	{
		AddError(TEXT("Generated Landscape is missing LandscapeInfo or the Generated Base layer."));
		return false;
	}

	FProjectWorldTerrainHeightComparison Comparison;
	FString CompareError;
	if (!FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
			Landscape, Bundle, Comparison, CompareError))
	{
		AddError(CompareError);
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("Generated Base: samples=%d/%d mismatches=%d max_error=%.4f m min=%.3f m max=%.3f m relief=%.3f m tolerance=%.4f m hash=%s"),
		Comparison.SampleCount,
		Comparison.ExpectedSampleCount,
		Comparison.MismatchCount,
		Comparison.MaximumErrorMeters,
		Comparison.MinimumHeightMeters,
		Comparison.MaximumHeightMeters,
		Comparison.ReliefMeters,
		Comparison.ToleranceMeters,
		*Comparison.RealizedHeightHash));

	// Pin the coverage claim itself. Shared seam vertices are intentionally compared once per
	// owning cell contract, so disagreeing canonical seam values must fail rather than average.
	TestEqual(
		TEXT("Every canonical terrain sample was compared."),
		Comparison.SampleCount,
		Comparison.ExpectedSampleCount);
	TestEqual(
		TEXT("Generated Base matches canonical elevation for every sample."),
		Comparison.MismatchCount,
		0);
	if (Comparison.MismatchCount > 0)
	{
		AddError(FString::Printf(TEXT("First mismatch: %s"), *Comparison.FirstMismatch));
	}
	TestTrue(
		TEXT("Generated Base retains canonical relief instead of realizing flat."),
		Comparison.ReliefMeters > Comparison.ToleranceMeters * 10.0);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldGeneratedBaseHeightAuthorityTest,
	"Project.World.Realization.NativeTwin.GeneratedBaseHeightAuthority",
	"[Fast][Integration][World]")

// Negative proof for the verifier itself. A correctness check nobody has watched fail is not
// evidence, so this realizes a good Landscape, deliberately flattens its Generated Base, and
// requires the verifier to reject. This is the durable regression; the historical flat territory
// artifact is only supporting evidence.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldTerrainVerifierRejectsFlatTest,
	"Project.World.Realization.NativeTwin.TerrainVerifierRejectsFlat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTerrainVerifierRejectsFlatTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldNativeTwinTests;
	UWorld* World = GEditor->NewMap(true);
	FProjectWorldCanonicalBundle Bundle = BuildLandscapeBundle();
	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldRealizationResult Result;
	FString Error;
	if (!ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("verifier_negative"), 1, Result, Error))
	{
		AddError(FString::Printf(TEXT("Landscape creation failed: %s"), *Error));
		return false;
	}

	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It))
		{
			Landscape = *It;
			break;
		}
	}
	ULandscapeInfo* LandscapeInfo = Landscape != nullptr ? Landscape->GetLandscapeInfo() : nullptr;
	const ULandscapeEditLayerBase* BaseLayer =
		Landscape != nullptr ? Landscape->GetEditLayer(TEXT("Generated Base")) : nullptr;
	if (LandscapeInfo == nullptr || BaseLayer == nullptr)
	{
		AddError(TEXT("Generated Landscape is missing LandscapeInfo or the Generated Base layer."));
		return false;
	}

	FProjectWorldTerrainHeightComparison Healthy;
	FString CompareError;
	if (!FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
			Landscape, Bundle, Healthy, CompareError))
	{
		AddError(CompareError);
		return false;
	}
	TestEqual(
		TEXT("Baseline realization matches canonical elevation before sabotage."),
		Healthy.MismatchCount,
		0);

	int32 MinimumX = 0;
	int32 MinimumY = 0;
	int32 MaximumX = 0;
	int32 MaximumY = 0;
	if (!LandscapeInfo->GetLandscapeExtent(MinimumX, MinimumY, MaximumX, MaximumY))
	{
		AddError(TEXT("Cannot read the Landscape extent."));
		return false;
	}
	const int32 Width = MaximumX - MinimumX + 1;
	const int32 Height = MaximumY - MinimumY + 1;
	TArray<uint16> FlatHeights;
	FlatHeights.Init(LandscapeDataAccess::GetTexHeight(0.0f), Width * Height);
	{
		FScopedSetLandscapeEditingLayer LayerScope(
			Landscape,
			BaseLayer->GetGuid(),
			[Landscape]()
			{
				Landscape->RequestLayersContentUpdateForceAll();
			});
		FLandscapeEditDataInterface EditData(LandscapeInfo, BaseLayer->GetGuid(), false);
		EditData.SetHeightData(
			MinimumX, MinimumY, MaximumX, MaximumY, FlatHeights.GetData(), Width, true);
	}

	FProjectWorldTerrainHeightComparison Flattened;
	if (!FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
			Landscape, Bundle, Flattened, CompareError))
	{
		AddError(CompareError);
		return false;
	}
	AddInfo(FString::Printf(
		TEXT("Sabotaged Generated Base: mismatches=%d relief=%.3f m (healthy relief=%.3f m)"),
		Flattened.MismatchCount,
		Flattened.ReliefMeters,
		Healthy.ReliefMeters));

	TestTrue(
		TEXT("Flattening Generated Base produces canonical mismatches."),
		Flattened.MismatchCount > 0);
	TestEqual(
		TEXT("Sabotage still compares the full canonical sample set."),
		Flattened.SampleCount,
		Flattened.ExpectedSampleCount);
	TestTrue(
		TEXT("Flattened relief collapses below the healthy realization."),
		Flattened.ReliefMeters < Healthy.ReliefMeters);
	TestNotEqual(
		TEXT("Sabotage changes the realized-height identity."),
		Flattened.RealizedHeightHash,
		Healthy.RealizedHeightHash);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldTerrainVerifierRejectsFlatTest,
	"Project.World.Realization.NativeTwin.TerrainVerifierRejectsFlat",
	"[Fast][Integration][World]")

// Acceptance surface. Source (Generated Base edit layer) is one input to UE's edit-layer
// blend; the final/base heightmap is what renders, collides, and sets cached bounds. The
// Kazan defect had a correct source and a wrong runtime surface, so this compares BOTH on
// the same canonical samples and requires the FINAL surface to match.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldFinalHeightmapAuthorityTest,
	"Project.World.Realization.NativeTwin.FinalHeightmapAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldFinalHeightmapAuthorityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldNativeTwinTests;
	UWorld* World = GEditor->NewMap(true);
	FProjectWorldCanonicalBundle Bundle = BuildLandscapeBundle();
	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldRealizationResult Result;
	FString Error;
	if (!ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("final_surface"), 1, Result, Error))
	{
		AddError(FString::Printf(TEXT("Landscape creation failed: %s"), *Error));
		return false;
	}

	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It))
		{
			Landscape = *It;
			break;
		}
	}
	if (Landscape == nullptr)
	{
		AddError(TEXT("No generated Landscape was created."));
		return false;
	}

	FProjectWorldTerrainHeightComparison Source;
	FProjectWorldTerrainHeightComparison Final;
	FString CompareError;
	if (!FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
			Landscape, Bundle, Source, CompareError))
	{
		AddError(FString::Printf(TEXT("Source comparison failed: %s"), *CompareError));
		return false;
	}
	if (!FProjectWorldTerrainVerification::CompareFinalHeightmapToCanonical(
			Landscape, Bundle, Final, CompareError))
	{
		AddError(FString::Printf(TEXT("Final comparison failed: %s"), *CompareError));
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("SOURCE (Generated Base): samples=%d/%d mismatches=%d max_error=%.4f m min=%.3f max=%.3f relief=%.3f hash=%s"),
		Source.SampleCount, Source.ExpectedSampleCount, Source.MismatchCount,
		Source.MaximumErrorMeters, Source.MinimumHeightMeters, Source.MaximumHeightMeters,
		Source.ReliefMeters, *Source.RealizedHeightHash));
	AddInfo(FString::Printf(
		TEXT("FINAL  (base heightmap): samples=%d/%d mismatches=%d max_error=%.4f m min=%.3f max=%.3f relief=%.3f hash=%s"),
		Final.SampleCount, Final.ExpectedSampleCount, Final.MismatchCount,
		Final.MaximumErrorMeters, Final.MinimumHeightMeters, Final.MaximumHeightMeters,
		Final.ReliefMeters, *Final.RealizedHeightHash));
	if (!Final.FirstMismatch.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("First final mismatch: %s"), *Final.FirstMismatch));
	}

	TestEqual(
		TEXT("Final heightmap compared every canonical sample."),
		Final.SampleCount,
		Final.ExpectedSampleCount);
	TestEqual(
		TEXT("Final composed heightmap matches canonical elevation for every sample."),
		Final.MismatchCount,
		0);
	TestTrue(
		TEXT("Final composed heightmap retains canonical relief instead of realizing flat."),
		Final.ReliefMeters > Final.ToleranceMeters * 10.0);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldFinalHeightmapAuthorityTest,
	"Project.World.Realization.NativeTwin.FinalHeightmapAuthority",
	"[Fast][Integration][World]")

// Invariant 18. The dirty planner owns which units are rebuilt; the per-component
// ProjectWorld.TerrainInput tag is a cache identity, not a second authority. A tag can be
// truthful about its INPUT while the realized output is wrong - which is exactly what the flat
// territory looked like - so a forced rebuild must not be vetoed by it. This sabotages the
// realized Generated Base of one cell, leaves its tag and its canonical artifact hash untouched,
// and requires the planner-selected rebuild to restore the surface.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPlannerDirtyOverridesCacheTagTest,
	"Project.World.Realization.NativeTwin.PlannerDirtyOverridesCacheTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace ProjectWorldNativeTwinTests
{
	ALandscape* FindGeneratedLandscape(UWorld* World)
	{
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	// Flatten ONE canonical cell's region of the realized Generated Base without touching any
	// identity tag: that component keeps claiming the canonical artifact it no longer contains.
	// Cell-local rather than whole-extent, so an unselected cell can act as a control.
	bool SabotageCellRegion(
		ALandscape* Landscape,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		FString& OutError)
	{
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		const ULandscapeEditLayerBase* BaseLayer = Landscape->GetEditLayer(TEXT("Generated Base"));
		if (LandscapeInfo == nullptr || BaseLayer == nullptr)
		{
			OutError = TEXT("Generated Landscape is missing LandscapeInfo or the Generated Base layer.");
			return false;
		}
		// Same cell-to-region mapping the generator uses, so the sabotage lands exactly on the
		// component the planner will later name.
		int32 MinimumCellX = MAX_int32;
		int32 MaximumCellY = MIN_int32;
		for (const FProjectWorldCanonicalCell& Candidate : Bundle.Cells)
		{
			MinimumCellX = FMath::Min(MinimumCellX, Candidate.CellX);
			MaximumCellY = FMath::Max(MaximumCellY, Candidate.CellY);
		}
		const int32 RegionX1 = (Cell.CellX - MinimumCellX) * Bundle.CellQuads.X;
		const int32 RegionY1 = (MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y;
		const int32 RegionX2 = RegionX1 + Bundle.CellQuads.X;
		const int32 RegionY2 = RegionY1 + Bundle.CellQuads.Y;
		const int32 Width = RegionX2 - RegionX1 + 1;
		const int32 Height = RegionY2 - RegionY1 + 1;
		TArray<uint16> FlatHeights;
		FlatHeights.Init(LandscapeDataAccess::GetTexHeight(0.0f), Width * Height);
		FScopedSetLandscapeEditingLayer LayerScope(
			Landscape,
			BaseLayer->GetGuid(),
			[Landscape]()
			{
				Landscape->RequestLayersContentUpdateForceAll();
			});
		FLandscapeEditDataInterface EditData(LandscapeInfo, BaseLayer->GetGuid(), false);
		EditData.SetHeightData(
			RegionX1, RegionY1, RegionX2, RegionY2, FlatHeights.GetData(), Width, true);
		return true;
	}

	FProjectWorldLayerInventory TerrainInventory(const TArray<FString>& DirtyUnits)
	{
		FProjectWorldLayerInventory Inventory;
		Inventory.LayerId = TEXT("terrain");
		Inventory.GeneratorId = TEXT("project_landscape");
		Inventory.GeneratorVersion = 1;
		Inventory.FinalDirtyUnits = DirtyUnits;
		return Inventory;
	}
}

bool FProjectWorldPlannerDirtyOverridesCacheTagTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldNativeTwinTests;
	UWorld* World = GEditor->NewMap(true);
	FProjectWorldCanonicalBundle Bundle = BuildLandscapeBundle();
	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldRealizationResult Result;
	FString Error;
	if (!ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("planner_authority"), 1, Result, Error))
	{
		AddError(FString::Printf(TEXT("Landscape creation failed: %s"), *Error));
		return false;
	}
	ALandscape* Landscape = FindGeneratedLandscape(World);
	if (Landscape == nullptr)
	{
		AddError(TEXT("No generated Landscape was created."));
		return false;
	}
	// Two cells are sabotaged and only ONE is named by the planner. The unselected cell is the
	// control: production dirtiness is canonical-cell granular, so "any non-empty dirty set
	// rebuilds the whole Landscape" must fail this test as loudly as "the tag vetoes the
	// planner" does. cell_0_0 and cell_1_2 share no edge, so neither sabotage can repair the
	// other through a shared seam.
	const FProjectWorldCanonicalCell* Target = Bundle.Cells.FindByPredicate(
		[](const FProjectWorldCanonicalCell& Cell)
		{
			return Cell.CellId == TEXT("cell_0_0");
		});
	const FProjectWorldCanonicalCell* Control = Bundle.Cells.FindByPredicate(
		[](const FProjectWorldCanonicalCell& Cell)
		{
			return Cell.CellId == TEXT("cell_1_2");
		});
	if (Target == nullptr || Control == nullptr)
	{
		AddError(TEXT("The twin bundle no longer exposes the expected target and control cells."));
		return false;
	}
	FProjectWorldTerrainHeightComparison Healthy;
	FString CompareError;
	if (!FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
			Landscape, Bundle, Healthy, CompareError))
	{
		AddError(CompareError);
		return false;
	}
	if (Healthy.MismatchCount != 0)
	{
		AddError(TEXT("Baseline realization does not match canonical, so nothing below is meaningful."));
		return false;
	}
	if (!SabotageCellRegion(Landscape, Bundle, *Target, Error) ||
		!SabotageCellRegion(Landscape, Bundle, *Control, Error))
	{
		AddError(Error);
		return false;
	}
	FProjectWorldTerrainHeightComparison Sabotaged;
	if (!FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
			Landscape, Bundle, Sabotaged, CompareError))
	{
		AddError(CompareError);
		return false;
	}
	if (Sabotaged.MismatchCount == 0)
	{
		AddError(TEXT("Sabotage did not change the realized surface, so the test proves nothing."));
		return false;
	}

	// Canonical is byte-identical, so every component's ProjectWorld.TerrainInput tag still
	// matches. Only the planner can authorize this work, and only for the cell it named.
	FProjectWorldRealizationResult Rebuild;
	Rebuild.LayerInventories.Add(TerrainInventory({ Target->CellId }));
	if (!ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("planner_authority"), 1, Rebuild, Error))
	{
		AddError(FString::Printf(TEXT("Planner-selected rebuild failed: %s"), *Error));
		return false;
	}
	FProjectWorldTerrainHeightComparison Restored;
	if (!FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
			Landscape, Bundle, Restored, CompareError))
	{
		AddError(CompareError);
		return false;
	}
	AddInfo(FString::Printf(
		TEXT("two cells sabotaged mismatches=%d -> planner rebuilt %s only: mismatches=%d components=%d relief=%.3f m"),
		Sabotaged.MismatchCount,
		*Target->CellId,
		Restored.MismatchCount,
		Rebuild.UpdatedLandscapeComponentCount,
		Restored.ReliefMeters));
	TestTrue(
		TEXT("The planner-selected cell is rebuilt even though its terrain input tag is unchanged."),
		Restored.MismatchCount < Sabotaged.MismatchCount);
	TestTrue(
		TEXT("The unselected sabotaged cell is left alone; the rebuild is not territory-wide."),
		Restored.MismatchCount > 0);
	TestEqual(
		TEXT("Exactly the planner-selected cell's component was written."),
		Rebuild.UpdatedLandscapeComponentCount,
		1);

	// The cache tag must still suppress work the planner did NOT select.
	FProjectWorldRealizationResult NoOp;
	NoOp.LayerInventories.Add(TerrainInventory(TArray<FString>()));
	if (!ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("planner_authority"), 1, NoOp, Error))
	{
		AddError(FString::Printf(TEXT("Unselected no-op run failed: %s"), *Error));
		return false;
	}
	TestEqual(
		TEXT("An unselected cell with a matching terrain input tag stays a no-op."),
		NoOp.UpdatedLandscapeComponentCount,
		0);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldPlannerDirtyOverridesCacheTagTest,
	"Project.World.Realization.NativeTwin.PlannerDirtyOverridesCacheTag",
	"[Fast][Integration][World]")

#endif
