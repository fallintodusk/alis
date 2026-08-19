// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

// Bounded 2x2 terrain discriminator. Known facts: the 6-component synthetic twin realizes a
// correct FINAL heightmap while the 210-component Kazan territory realizes flat, through the
// same CreateOrUpdate path. This matrix separates the failure class mechanically:
//
//                      synthetic ramp     real Kazan heights
//   small 2x3 subset        A                   B
//   full 210 grid           C                   D (known failing)
//
//   A fails            -> territory metadata/layout/profile class
//   A passes, B fails  -> Kazan height data / origin / encoding class
//   A+B pass, C fails  -> component-count / topology scale class
//   A+B+C pass, D fail -> interaction of real data with full extent
//
// Every case shares the territory configuration (cell quads, 30 m spacing, height origin,
// georeference origin, layout selection, logical Landscape ID "kazan_main",
// components-per-proxy 1) inside a transient NewMap world. Nothing is saved.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLandscapeRealization.h"
#include "ProjectWorldPartitionPolicy.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldTerrainVerification.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldTerrainMatrixTests
{
	// The materialized territory compile result used by every recent territory run. This is a
	// diagnostic (machine-local canonical cache); the test fails loudly when it is absent.
	const TCHAR* TerritoryCompileResultRelativePath =
		TEXT("tmp/world/canonical_compilation/materialized/kazan_territory_v1/")
		TEXT("c64bfab68a4ce9c2016dbe3f8cbb6239ff7fdf514affde7ac29e563c819f361f/compile_result.json");

	struct FCaseResult
	{
		FString Name;
		bool bRan = false;
		FString Error;
		int32 Components = 0;
		FProjectWorldTerrainHeightComparison Source;
		FProjectWorldTerrainHeightComparison Final;
	};

	FProjectWorldCanonicalBundle MakeSubset(
		const FProjectWorldCanonicalBundle& Bundle,
		int32 MinCellX,
		int32 MaxCellX,
		int32 MinCellY,
		int32 MaxCellY)
	{
		FProjectWorldCanonicalBundle Subset = Bundle;
		Subset.Cells.Reset();
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			if (Cell.CellX >= MinCellX && Cell.CellX <= MaxCellX &&
				Cell.CellY >= MinCellY && Cell.CellY <= MaxCellY)
			{
				Subset.Cells.Add(Cell);
			}
		}
		return Subset;
	}

	// Asymmetric ramp in canonical metres, seam-consistent because shared vertices share the
	// same global sample index. Replaces heights only; every other territory property stays.
	void ApplySyntheticRamp(FProjectWorldCanonicalBundle& Bundle)
	{
		int32 MinCellX = MAX_int32;
		int32 MaximumCellY = MIN_int32;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			MinCellX = FMath::Min(MinCellX, Cell.CellX);
			MaximumCellY = FMath::Max(MaximumCellY, Cell.CellY);
		}
		for (FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const int32 OffsetX = (Cell.CellX - MinCellX) * Bundle.CellQuads.X;
			const int32 OffsetY = (MaximumCellY - Cell.CellY) * Bundle.CellQuads.Y;
			for (int32 Row = 0; Row < Cell.Terrain.SamplesY; ++Row)
			{
				for (int32 Column = 0; Column < Cell.Terrain.SamplesX; ++Column)
				{
					Cell.Terrain.HeightsMeters[Row * Cell.Terrain.SamplesX + Column] =
						10.0 + (OffsetX + Column) * 0.1 + (OffsetY + Row) * 0.05;
				}
			}
		}
	}

	FCaseResult RunCase(const FString& Name, const FProjectWorldCanonicalBundle& Bundle)
	{
		FCaseResult Out;
		Out.Name = Name;
		UWorld* World = GEditor->NewMap(true);
		FString Error;
		if (!ProjectWorldPartitionPolicy::DisableHLOD(World, Error))
		{
			Out.Error = FString::Printf(TEXT("DisableHLOD: %s"), *Error);
			return Out;
		}
		const FProjectWorldLandscapeLayout Layout =
			FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
		if (!Layout.bCompatible)
		{
			Out.Error = FString::Printf(TEXT("Layout: %s"), *Layout.Reason);
			return Out;
		}
		FProjectWorldRealizationResult Result;
		if (!ProjectWorldLandscapeRealization::CreateOrUpdate(
				World, Bundle, Layout, TEXT("kazan_main"), 1, Result, Error))
		{
			Out.Error = FString::Printf(TEXT("CreateOrUpdate: %s"), *Error);
			return Out;
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
			Out.Error = TEXT("No generated Landscape.");
			return Out;
		}
		for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
		{
			Out.Components += It->LandscapeComponents.Num();
		}
		if (!FProjectWorldTerrainVerification::CompareGeneratedBaseToCanonical(
				Landscape, Bundle, Out.Source, Error))
		{
			Out.Error = FString::Printf(TEXT("Source compare: %s"), *Error);
			return Out;
		}
		if (!FProjectWorldTerrainVerification::CompareFinalHeightmapToCanonical(
				Landscape, Bundle, Out.Final, Error))
		{
			Out.Error = FString::Printf(TEXT("Final compare: %s"), *Error);
			return Out;
		}
		Out.bRan = true;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldTerrainConfigMatrixTest,
	"Project.World.Realization.NativeTwin.TerrainConfigMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTerrainConfigMatrixTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldTerrainMatrixTests;

	FProjectWorldCanonicalBundle Territory;
	FProjectWorldCanonicalValidation Validation;
	const FString CompileResultPath =
		FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir() / TerritoryCompileResultRelativePath);
	if (!FProjectWorldCanonicalLoader::Load(CompileResultPath, Territory, Validation))
	{
		AddError(FString::Printf(
			TEXT("Cannot load the territory compile result (%s): %s"),
			*CompileResultPath,
			*Validation.Message));
		return false;
	}
	AddInfo(FString::Printf(
		TEXT("Territory bundle: cells=%d cell_quads=%dx%d spacing=%.1f m height_origin=%.1f"),
		Territory.Cells.Num(),
		Territory.CellQuads.X,
		Territory.CellQuads.Y,
		Territory.SampleSpacingMeters.X,
		Territory.HeightOriginMeters));

	FProjectWorldCanonicalBundle CaseA = MakeSubset(Territory, 0, 1, 0, 2);
	ApplySyntheticRamp(CaseA);
	FProjectWorldCanonicalBundle CaseB = MakeSubset(Territory, 0, 1, 0, 2);
	FProjectWorldCanonicalBundle CaseC = Territory;
	ApplySyntheticRamp(CaseC);
	const FProjectWorldCanonicalBundle& CaseD = Territory;

	TArray<FCaseResult> Results;
	Results.Add(RunCase(TEXT("A small+synthetic"), CaseA));
	Results.Add(RunCase(TEXT("B small+kazan    "), CaseB));
	Results.Add(RunCase(TEXT("C full+synthetic "), CaseC));
	Results.Add(RunCase(TEXT("D full+kazan     "), CaseD));

	for (const FCaseResult& Case : Results)
	{
		if (!Case.bRan)
		{
			AddInfo(FString::Printf(TEXT("MATRIX %s: DID NOT RUN - %s"), *Case.Name, *Case.Error));
			continue;
		}
		AddInfo(FString::Printf(
			TEXT("MATRIX %s: components=%d SOURCE mis=%d relief=%.3f | FINAL mis=%d relief=%.3f min=%.3f max=%.3f"),
			*Case.Name,
			Case.Components,
			Case.Source.MismatchCount,
			Case.Source.ReliefMeters,
			Case.Final.MismatchCount,
			Case.Final.ReliefMeters,
			Case.Final.MinimumHeightMeters,
			Case.Final.MaximumHeightMeters));
	}
	for (const FCaseResult& Case : Results)
	{
		TestTrue(FString::Printf(TEXT("%s ran to comparison."), *Case.Name), Case.bRan);
		if (Case.bRan)
		{
			TestEqual(
				FString::Printf(TEXT("%s FINAL matches canonical."), *Case.Name),
				Case.Final.MismatchCount,
				0);
		}
	}
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldTerrainConfigMatrixTest,
	"Project.World.Realization.NativeTwin.TerrainConfigMatrix",
	"[Slow][Integration][World]")

#endif
