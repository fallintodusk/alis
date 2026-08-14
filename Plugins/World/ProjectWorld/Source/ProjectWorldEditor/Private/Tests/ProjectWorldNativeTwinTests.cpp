// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLandscapeRealization.h"
#include "ProjectWorldPartitionPolicy.h"
#include "ProjectWorldRealizationService.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeStreamingProxy.h"
#include "Misc/AutomationTest.h"
#include "NaniteSceneProxy.h"

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
		for (int32 CellX = 0; CellX < 2; ++CellX)
		{
			FProjectWorldCanonicalCell Cell;
			Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
			Cell.CellX = CellX;
			Cell.CellY = 0;
			Cell.Bounds = FVector4d(CellX * 63.0, 0.0, (CellX + 1) * 63.0, 63.0);
			Cell.Terrain.Bounds = Cell.Bounds;
			Cell.Terrain.SampleSpacing = Bundle.SampleSpacingMeters;
			Cell.Terrain.SamplesX = 64;
			Cell.Terrain.SamplesY = 64;
			Cell.Terrain.ArtifactHash = FString::Printf(TEXT("terrain_%d"), CellX);
			for (int32 Row = 0; Row < 64; ++Row)
			{
				for (int32 Column = 0; Column < 64; ++Column)
				{
					const int32 GlobalX = CellX * 63 + Column;
					Cell.Terrain.HeightsMeters.Add(10.0 + GlobalX * 0.1 + Row * 0.05);
				}
			}
			Bundle.Cells.Add(MoveTemp(Cell));
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
	for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
	{
		++ProxyCount;
		ProxyComponentCount += It->LandscapeComponents.Num();
	}
	TestEqual(TEXT("The two-cell twin creates two streaming proxies."), ProxyCount, 2);
	TestEqual(TEXT("Each canonical cell remains one Landscape component."), ProxyComponentCount, 2);
	TestEqual(TEXT("The logical Landscape owns no streamed components."), Landscape->LandscapeComponents.Num(), 0);
	TestNotNull(
		TEXT("Generated Base survives native partitioning."),
		Landscape->GetEditLayer(TEXT("Generated Base")));
	TestNotNull(
		TEXT("Authored Corrections survives native partitioning."),
		Landscape->GetEditLayer(TEXT("Authored Corrections")));
	Bundle.Cells[0].Terrain.ArtifactHash = TEXT("terrain_0_changed");
	Bundle.Cells[0].Terrain.HeightsMeters[0] += 0.1;
	FProjectWorldRealizationResult IncrementalResult;
	const bool bUpdated = ProjectWorldLandscapeRealization::CreateOrUpdate(
		World, Bundle, Layout, TEXT("native_twin"), 1, IncrementalResult, Error);
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
	TestEqual(TEXT("Incremental realization preserves the two-proxy topology."), IncrementalProxyCount, 2);
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

#endif
