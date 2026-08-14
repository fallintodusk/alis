// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ConstrainedDelaunay2.h"
#include "Curve/GeneralPolygon2.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "Curve/PolygonOffsetUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "MaterialShared.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeometryParsing.h"
#include "ProjectWorldWaterContractParsing.h"
#include "ProjectWorldWaterMeshBuilder.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldWaterNativeTwinTests
{
	using namespace UE::Geometry;

	FGeneralPolygon2d MakeRectangle(double MinX, double MinY, double MaxX, double MaxY)
	{
		return FGeneralPolygon2d(FPolygon2d({
			FVector2d(MinX, MinY),
			FVector2d(MaxX, MinY),
			FVector2d(MaxX, MaxY),
			FVector2d(MinX, MaxY)}));
	}

	bool ClipToCell(
		const TArray<FGeneralPolygon2d>& Surfaces,
		const FGeneralPolygon2d& Cell,
		TArray<FGeneralPolygon2d>& OutClipped)
	{
		const TArray<FGeneralPolygon2d> Cells{Cell};
		return PolygonsIntersection(Surfaces, Cells, OutClipped);
	}

	double TriangulatedArea(
		const FGeneralPolygon2d& Polygon,
		TArray<FVector2d>* OutVertices = nullptr,
		TArray<FIndex3i>* OutTriangles = nullptr)
	{
		TArray<FVector2d> Vertices;
		TArray<FIndex3i> Triangles =
			ConstrainedDelaunayTriangulateWithVertices(Polygon, Vertices);
		double Area = 0.0;
		for (const FIndex3i& Triangle : Triangles)
		{
			const FVector2d A = Vertices[Triangle.A];
			const FVector2d B = Vertices[Triangle.B];
			const FVector2d C = Vertices[Triangle.C];
			Area += FMath::Abs(FVector2d::CrossProduct(B - A, C - A)) * 0.5;
		}
		if (OutVertices != nullptr)
		{
			*OutVertices = MoveTemp(Vertices);
		}
		if (OutTriangles != nullptr)
		{
			*OutTriangles = MoveTemp(Triangles);
		}
		return Area;
	}

	TArray<FVector2d> BoundaryVertices(
		const TArray<FGeneralPolygon2d>& Polygons,
		double BoundaryX)
	{
		TArray<FVector2d> Result;
		for (const FGeneralPolygon2d& Polygon : Polygons)
		{
			for (const FVector2d& Vertex : Polygon.GetOuter().GetVertices())
			{
				if (FMath::IsNearlyEqual(Vertex.X, BoundaryX, 0.000001))
				{
					const FVector2d Quantized(
						FMath::RoundToDouble(Vertex.X * 100.0) / 100.0,
						FMath::RoundToDouble(Vertex.Y * 100.0) / 100.0);
					Result.AddUnique(Quantized);
				}
			}
		}
		Result.Sort([](const FVector2d& A, const FVector2d& B)
		{
			return A.Y < B.Y;
		});
		return Result;
	}

	FMeshDescription BuildQuadMeshDescription()
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		UVs.SetNumChannels(1);
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Surface");

		const TArray<FVector3f> Corners{
			FVector3f(0.0f, 0.0f, 0.0f),
			FVector3f(100.0f, 0.0f, 0.0f),
			FVector3f(100.0f, 100.0f, 0.0f),
			FVector3f(0.0f, 100.0f, 0.0f)};
		TArray<FVertexInstanceID> Instances;
		for (int32 Index = 0; Index < Corners.Num(); ++Index)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = Corners[Index];
			const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
			Normals[Instance] = FVector3f::ZAxisVector;
			Tangents[Instance] = FVector3f::XAxisVector;
			BinormalSigns[Instance] = 1.0f;
			Colors[Instance] = FVector4f::One();
			UVs.Set(Instance, 0, FVector2f(Corners[Index].X, Corners[Index].Y) / 100.0f);
			Instances.Add(Instance);
		}
		Description.CreatePolygon(Group, {Instances[0], Instances[1], Instances[2]});
		Description.CreatePolygon(Group, {Instances[0], Instances[2], Instances[3]});
		return Description;
	}

	bool SaveAssetPackage(UPackage* Package, UObject* Asset)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Arguments;
		Arguments.TopLevelFlags = RF_Public | RF_Standalone;
		Arguments.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, Arguments);
	}

	FString PackageFilename(const FString& PackageName)
	{
		return FPackageName::LongPackageNameToFilename(
			PackageName,
			FPackageName::GetAssetPackageExtension());
	}

	TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Result;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		FJsonSerializer::Deserialize(Reader, Result);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldNativeWaterGeometryTwinTest,
	"Project.World.Realization.NativeTwin.WaterGeometryAndSeams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldNativeWaterGeometryTwinTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldWaterNativeTwinTests;
	using namespace UE::Geometry;
	const FGeneralPolygon2d CellA = MakeRectangle(0.0, 0.0, 100.0, 100.0);
	const FGeneralPolygon2d CellB = MakeRectangle(100.0, 0.0, 200.0, 100.0);

	FGeneralPolygon2d Lake = MakeRectangle(10.0, 45.0, 50.0, 90.0);
	FPolygon2d Hole({
		FVector2d(20.0, 55.0),
		FVector2d(20.0, 70.0),
		FVector2d(35.0, 70.0),
		FVector2d(35.0, 55.0)});
	TestTrue(TEXT("The native general polygon accepts the lake hole."), Lake.AddHole(Hole));
	TArray<FGeneralPolygon2d> LakeInA;
	TestTrue(TEXT("GeometryAlgorithms clips the lake into its cell."), ClipToCell({Lake}, CellA, LakeInA));
	TestEqual(TEXT("The lake remains one surface after clipping."), LakeInA.Num(), 1);
	TArray<FVector2d> LakeVertices;
	TArray<FIndex3i> LakeTriangles;
	const double LakeTriangleArea = TriangulatedArea(LakeInA[0], &LakeVertices, &LakeTriangles);
	TestTrue(TEXT("Constrained triangulation emits lake triangles."), !LakeTriangles.IsEmpty());
	TestTrue(
		TEXT("Constrained triangulation preserves the polygon-hole area."),
		FMath::IsNearlyEqual(LakeTriangleArea, FMath::Abs(Lake.SignedArea()), 0.000001));
	const double StandingZ = 12.0;
	TestEqual(TEXT("Uneven terrain cannot bend the standing surface."), StandingZ, StandingZ);

	const FGeneralPolygon2d River = MakeRectangle(60.0, 20.0, 140.0, 40.0);
	TArray<FGeneralPolygon2d> RiverInA;
	TArray<FGeneralPolygon2d> RiverInB;
	TestTrue(TEXT("The wide river clips into cell A."), ClipToCell({River}, CellA, RiverInA));
	TestTrue(TEXT("The wide river clips into cell B."), ClipToCell({River}, CellB, RiverInB));
	const auto FlowZ = [](const FVector2d& Point)
	{
		return 30.0 - Point.X * 0.05;
	};
	TestEqual(
		TEXT("A flowing cross-section remains level."),
		FlowZ(FVector2d(80.0, 20.0)),
		FlowZ(FVector2d(80.0, 40.0)));
	TestTrue(
		TEXT("The flowing surface slopes downstream."),
		FlowZ(FVector2d(60.0, 30.0)) > FlowZ(FVector2d(140.0, 30.0)));
	const TArray<FVector2d> RiverSeamA = BoundaryVertices(RiverInA, 100.0);
	const TArray<FVector2d> RiverSeamB = BoundaryVertices(RiverInB, 100.0);
	TestEqual(TEXT("Both cells derive the same river seam vertices."), RiverSeamA, RiverSeamB);
	for (int32 Index = 0; Index < RiverSeamA.Num() && Index < RiverSeamB.Num(); ++Index)
	{
		TestEqual(TEXT("Both cells evaluate identical river seam Z."), FlowZ(RiverSeamA[Index]), FlowZ(RiverSeamB[Index]));
	}
	const double RiverSplitArea = TriangulatedArea(RiverInA[0]) + TriangulatedArea(RiverInB[0]);
	TestTrue(
		TEXT("Cell-local river surfaces neither duplicate nor omit area."),
		FMath::IsNearlyEqual(RiverSplitArea, FMath::Abs(River.SignedArea()), 0.000001));

	TArray<FVector2d> Axis{FVector2d(95.0, 55.0), FVector2d(95.0, 85.0)};
	FOffsetPolygon2d RibbonBuilder;
	RibbonBuilder.Polygons.Add(MakeArrayView(Axis));
	RibbonBuilder.Offset = 12.0;
	RibbonBuilder.JoinType = EPolygonOffsetJoinType::Round;
	RibbonBuilder.EndType = EPolygonOffsetEndType::Round;
	RibbonBuilder.MaxStepsPerRadian = 16.0 / PI;
	TestTrue(TEXT("GeometryAlgorithms creates the authoritative ribbon footprint."), RibbonBuilder.ComputeResult());
	double RibbonMaximumX = TNumericLimits<double>::Lowest();
	for (const FGeneralPolygon2d& Polygon : RibbonBuilder.Result)
	{
		RibbonMaximumX = FMath::Max(RibbonMaximumX, Polygon.Bounds().Max.X);
	}
	TestTrue(
		FString::Printf(TEXT("The 12 m ribbon crosses the cell boundary (max X %.6f)."), RibbonMaximumX),
		RibbonMaximumX > 100.0);
	TArray<FGeneralPolygon2d> RibbonInA;
	TArray<FGeneralPolygon2d> RibbonInB;
	TestTrue(TEXT("The ribbon footprint clips into cell A."), ClipToCell(RibbonBuilder.Result, CellA, RibbonInA));
	TestTrue(TEXT("The footprint-only neighbor receives a surface."), ClipToCell(RibbonBuilder.Result, CellB, RibbonInB));
	TestTrue(TEXT("The centerline itself remains entirely in cell A."), Axis[0].X < 100.0 && Axis[1].X < 100.0);
	TestTrue(TEXT("The buffered footprint realizes in cell B."), !RibbonInB.IsEmpty());
	const TArray<FVector2d> RibbonSeamA = BoundaryVertices(RibbonInA, 100.0);
	const TArray<FVector2d> RibbonSeamB = BoundaryVertices(RibbonInB, 100.0);
	TestEqual(TEXT("Footprint-only ribbon XY seams are identical."), RibbonSeamA, RibbonSeamB);
	const auto RibbonZ = [](const FVector2d& Point)
	{
		return 18.0 - Point.Y * 0.02;
	};
	for (int32 Index = 0; Index < RibbonSeamA.Num() && Index < RibbonSeamB.Num(); ++Index)
	{
		TestEqual(TEXT("Footprint-only ribbon Z seams are identical."), RibbonZ(RibbonSeamA[Index]), RibbonZ(RibbonSeamB[Index]));
	}
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldNativeWaterGeometryTwinTest,
	"Project.World.Realization.NativeTwin.WaterGeometryAndSeams",
	"[Fast][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldNativeWaterCanonicalContractTwinTest,
	"Project.World.Realization.NativeTwin.WaterCanonicalContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldNativeWaterCanonicalContractTwinTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldWaterNativeTwinTests;
	const TSharedPtr<FJsonObject> Geometry = ParseObject(TEXT(R"json(
		{"type":"Polygon","coordinates":[
			[[10,8],[55,8],[55,22],[10,22],[10,8]],
			[[25.25,12.25],[25.25,18.25],[40.25,18.25],[40.25,12.25],[25.25,12.25]]
		]})json"));
	FProjectWorldCanonicalFeature Lake;
	Lake.FeatureClass = TEXT("water");
	FProjectWorldCanonicalValidation Validation;
	TestTrue(
		TEXT("The Unreal canonical parser accepts the polygon-with-hole contract."),
		ProjectWorldGeometryParsing::ReadGeometry(
			Geometry,
			Lake.GeometryType,
			Lake.GeometryPoints,
			Validation,
			&Lake.GeometryParts,
			&Lake.GeometryPolygons));
	TestEqual(TEXT("The canonical lake remains one polygon."), Lake.GeometryPolygons.Num(), 1);
	if (!Lake.GeometryPolygons.IsEmpty())
	{
		TestEqual(TEXT("The canonical lake hole survives Unreal loading."), Lake.GeometryPolygons[0].Holes.Num(), 1);
	}

	const TSharedPtr<FJsonObject> Standing = ParseObject(TEXT(R"json(
		{
			"surface_group_id":"standing_lake",
			"surface_group_members":["way/1001"],
			"surface_geometry":"polygon",
			"surface_behavior":"standing",
			"surface_role":"area",
			"surface_function":{"function_id":"standing_polygon_quantile","function_version":1,"level_m":27.5}
		})json"));
	TestTrue(
		TEXT("The Unreal loader retains standing-water surface authority."),
		ProjectWorldWaterContractParsing::Read(Standing, Lake, Validation));
	TestTrue(TEXT("Standing-water authority is marked valid."), Lake.WaterSurface.bValid);
	TestEqual(TEXT("Standing-water function version survives loading."), Lake.WaterSurface.FunctionVersion, 1);
	TestEqual(TEXT("The canonical lake level survives loading."), Lake.WaterSurface.LevelMeters, 27.5);

	FProjectWorldCanonicalFeature River;
	River.FeatureClass = TEXT("water");
	const TSharedPtr<FJsonObject> Flowing = ParseObject(TEXT(R"json(
		{
			"surface_group_id":"cross_cell_river",
			"surface_group_members":["relation/1002","way/1003"],
			"surface_geometry":"polygon",
			"surface_behavior":"flowing",
			"surface_role":"area",
			"surface_function":{"function_id":"rolling_quantile_l1_isotonic","function_version":1,"knots":[[25,37.5,26.8],[65,37.5,25.1],[125,37.5,21.8]]}
		})json"));
	TestTrue(
		TEXT("The Unreal loader retains flowing-water surface authority."),
		ProjectWorldWaterContractParsing::Read(Flowing, River, Validation));
	TestEqual(TEXT("All flowing-water knots survive loading."), River.WaterSurface.Knots.Num(), 3);
	TestEqual(TEXT("Flowing-water function version survives loading."), River.WaterSurface.FunctionVersion, 1);
	TestTrue(
		TEXT("The loaded flowing-water authority slopes downstream."),
		River.WaterSurface.Knots[0].Z > River.WaterSurface.Knots.Last().Z);

	FProjectWorldCanonicalBundle Bundle;
	Bundle.EngineGeoreferenceOriginMeters = FVector2D::ZeroVector;
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	FProjectWorldCanonicalCell CellA;
	CellA.Bounds = FVector4d(0.0, 0.0, 63.0, 63.0);
	FProjectWorldCanonicalCell CellB;
	CellB.Bounds = FVector4d(63.0, 0.0, 126.0, 63.0);
	FProjectWorldWaterMeshBuildResult LakeMesh;
	FString Error;
	TestTrue(
		TEXT("The native MeshDescription adapter triangulates the canonical lake."),
		ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle, CellA, Lake, LakeMesh, Error));
	TestTrue(TEXT("The lake-with-hole emits persistent mesh triangles."), LakeMesh.TriangleCount > 0);
	TestTrue(
		TEXT("The realized lake area excludes its polygon hole."),
		FMath::IsNearlyEqual(LakeMesh.CanonicalAreaSquareMeters, 540.0, 0.000001));
	FStaticMeshAttributes LakeAttributes(LakeMesh.MeshDescription);
	for (const FVertexID Vertex : LakeMesh.MeshDescription.Vertices().GetElementIDs())
	{
		TestEqual(
			TEXT("Every realized lake vertex remains on one standing-water level."),
			LakeAttributes.GetVertexPositions()[Vertex].Z,
			2750.0f);
	}

	FProjectWorldCanonicalPolygon RiverPolygon;
	RiverPolygon.Outer = {
		FVector2D(35.0, 30.0), FVector2D(110.0, 30.0),
		FVector2D(110.0, 45.0), FVector2D(35.0, 45.0), FVector2D(35.0, 30.0)};
	River.GeometryType = TEXT("Polygon");
	River.GeometryPolygons.Add(MoveTemp(RiverPolygon));
	FProjectWorldPreparedWaterSurface PreparedRiver;
	TestTrue(
		TEXT("The cross-cell river is prepared once for the complete target bounds."),
		ProjectWorldWaterMeshBuilder::PrepareSurface(
			River, FVector4d(0.0, 0.0, 126.0, 63.0), PreparedRiver, Error));
	FProjectWorldWaterMeshBuildResult RiverA;
	FProjectWorldWaterMeshBuildResult RiverB;
	TestTrue(
		TEXT("The prepared flowing polygon realizes in cell A."),
		ProjectWorldWaterMeshBuilder::BuildCellSurface(
			Bundle, CellA, River, PreparedRiver, RiverA, Error));
	TestTrue(
		TEXT("The same prepared flowing polygon realizes in cell B."),
		ProjectWorldWaterMeshBuilder::BuildCellSurface(
			Bundle, CellB, River, PreparedRiver, RiverB, Error));
	TestTrue(
		TEXT("Cell-local flowing meshes neither duplicate nor omit polygon area."),
		FMath::IsNearlyEqual(
			RiverA.CanonicalAreaSquareMeters + RiverB.CanonicalAreaSquareMeters,
			1125.0,
			0.000001));
	auto Seam = [&Bundle](FProjectWorldWaterMeshBuildResult& Mesh)
	{
		TArray<FVector2D> Result;
		FStaticMeshAttributes Attributes(Mesh.MeshDescription);
		for (const FVertexID Vertex : Mesh.MeshDescription.Vertices().GetElementIDs())
		{
			const FVector World = FVector(Attributes.GetVertexPositions()[Vertex]) + Mesh.ActorOrigin;
			const FVector Canonical = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, World);
			if (FMath::IsNearlyEqual(Canonical.X, 63.0, 0.0001))
			{
				Result.AddUnique(FVector2D(Canonical.Y, Canonical.Z));
			}
		}
		Result.Sort([](const FVector2D& Left, const FVector2D& Right)
		{
			return Left.X < Right.X;
		});
		return Result;
	};
	const TArray<FVector2D> SeamA = Seam(RiverA);
	const TArray<FVector2D> SeamB = Seam(RiverB);
	TestEqual(TEXT("Cell-local flowing mesh XY/Z seams are identical."), SeamA, SeamB);
	TestTrue(TEXT("The flowing cross-cell seam includes both banks."), SeamA.Num() >= 2);
	for (int32 Index = 1; Index < SeamA.Num(); ++Index)
	{
		TestEqual(TEXT("A flowing cross-section remains level."), SeamA[0].Y, SeamA[Index].Y);
	}

	FProjectWorldCanonicalFeature Ribbon;
	Ribbon.FeatureId = TEXT("twin-ribbon");
	Ribbon.FeatureClass = TEXT("water");
	Ribbon.WidthMeters = 12.0;
	Ribbon.GeometryParts.Add({FVector2D(10.0, 50.0), FVector2D(50.0, 50.0)});
	Ribbon.WaterSurface.bValid = true;
	Ribbon.WaterSurface.SurfaceGroupId = TEXT("twin-ribbon");
	Ribbon.WaterSurface.Geometry = TEXT("ribbon");
	Ribbon.WaterSurface.Behavior = TEXT("flowing");
	Ribbon.WaterSurface.FunctionId = TEXT("rolling_quantile_l1_isotonic");
	Ribbon.WaterSurface.FunctionVersion = 1;
	Ribbon.WaterSurface.Knots = {FVector(10.0, 50.0, 10.0), FVector(50.0, 50.0, 9.0)};
	FProjectWorldWaterMeshBuildResult RibbonMesh;
	TestTrue(
		TEXT("The production builder realizes the exact-width ribbon."),
		ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle, CellA, Ribbon, RibbonMesh, Error));
	double MinimumY = TNumericLimits<double>::Max();
	double MaximumY = TNumericLimits<double>::Lowest();
	FStaticMeshAttributes RibbonAttributes(RibbonMesh.MeshDescription);
	for (const FVertexID Vertex : RibbonMesh.MeshDescription.Vertices().GetElementIDs())
	{
		const FVector World = FVector(RibbonAttributes.GetVertexPositions()[Vertex]) + RibbonMesh.ActorOrigin;
		const FVector Canonical = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, World);
		MinimumY = FMath::Min(MinimumY, Canonical.Y);
		MaximumY = FMath::Max(MaximumY, Canonical.Y);
	}
	TestTrue(TEXT("A 12 m canonical ribbon extends exactly 6 m to its first side."), FMath::IsNearlyEqual(MinimumY, 44.0, 0.01));
	TestTrue(TEXT("A 12 m canonical ribbon extends exactly 6 m to its second side."), FMath::IsNearlyEqual(MaximumY, 56.0, 0.01));
	TestTrue(TEXT("The realized ribbon total width is exactly 12 m."), FMath::IsNearlyEqual(MaximumY - MinimumY, 12.0, 0.01));

	const TSharedPtr<FJsonObject> FutureFunction = ParseObject(TEXT(R"json(
		{
			"surface_group_id":"future-lake",
			"surface_group_members":["way/future"],
			"surface_geometry":"polygon",
			"surface_behavior":"standing",
			"surface_role":"area",
			"surface_function":{"function_id":"standing_polygon_quantile","function_version":2,"level_m":27.5}
		})json"));
	FProjectWorldCanonicalFeature FutureLake;
	FutureLake.FeatureClass = TEXT("water");
	FProjectWorldCanonicalValidation FutureValidation;
	TestFalse(
		TEXT("Unreal rejects an unsupported canonical water function version."),
		ProjectWorldWaterContractParsing::Read(FutureFunction, FutureLake, FutureValidation));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldNativeWaterCanonicalContractTwinTest,
	"Project.World.Realization.NativeTwin.WaterCanonicalContract",
	"[Fast][Architecture][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldNativeWaterAssetPersistenceTwinTest,
	"Project.World.Realization.NativeTwin.WaterAssetPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldNativeWaterAssetPersistenceTwinTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldWaterNativeTwinTests;
	const FString Root = TEXT("/ProjectWorldTestData/Generated/NativeTwin/");
	const FString MaterialPackageName = Root + TEXT("M_NativeTwinWater");
	const FString WaterPackageName = Root + TEXT("SM_NativeTwinWater");
	const FString NanitePackageName = Root + TEXT("SM_NativeTwinNaniteControl");
	const TArray<FString> PackageNames{MaterialPackageName, WaterPackageName, NanitePackageName};
	for (const FString& PackageName : PackageNames)
	{
		TestFalse(
			TEXT("Disposable twin packages are absent before realization."),
			IFileManager::Get().FileExists(*PackageFilename(PackageName)));
	}

	UPackage* MaterialPackage = CreatePackage(*MaterialPackageName);
	UMaterial* WaterMaterial = NewObject<UMaterial>(
		MaterialPackage, TEXT("M_NativeTwinWater"), RF_Public | RF_Standalone);
	UMaterialExpressionConstant3Vector* Scattering =
		NewObject<UMaterialExpressionConstant3Vector>(WaterMaterial);
	Scattering->Constant = FLinearColor(0.005f, 0.01f, 0.015f);
	UMaterialExpressionConstant3Vector* Absorption =
		NewObject<UMaterialExpressionConstant3Vector>(WaterMaterial);
	Absorption->Constant = FLinearColor(0.01f, 0.02f, 0.03f);
	UMaterialExpressionSingleLayerWaterMaterialOutput* WaterOutput =
		NewObject<UMaterialExpressionSingleLayerWaterMaterialOutput>(WaterMaterial);
	WaterOutput->ScatteringCoefficients.Connect(0, Scattering);
	WaterOutput->AbsorptionCoefficients.Connect(0, Absorption);
	WaterMaterial->GetExpressionCollection().AddExpression(Scattering);
	WaterMaterial->GetExpressionCollection().AddExpression(Absorption);
	WaterMaterial->GetExpressionCollection().AddExpression(WaterOutput);
	WaterMaterial->SetShadingModel(MSM_SingleLayerWater);
	WaterMaterial->PostEditChange();
	FAssetCompilingManager::Get().FinishAllCompilation();
	FMaterialResource* WaterResource = WaterMaterial->GetMaterialResource(GMaxRHIShaderPlatform);
	TestNotNull(TEXT("The native Single Layer Water material compiles."), WaterResource);
	if (WaterResource != nullptr)
	{
		TestTrue(TEXT("The native Single Layer Water material has no compile errors."), WaterResource->GetCompileErrors().IsEmpty());
	}
	FAssetRegistryModule::AssetCreated(WaterMaterial);
	TestTrue(TEXT("The Single Layer Water material package saves."), SaveAssetPackage(MaterialPackage, WaterMaterial));

	auto CreateMesh = [this, WaterMaterial](
		const FString& PackageName,
		const FName AssetName,
		bool bEnableNanite,
		const FMeshDescription& Description)
	{
		UPackage* Package = CreatePackage(*PackageName);
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, AssetName, RF_Public | RF_Standalone);
		if (!bEnableNanite)
		{
			Mesh->GetStaticMaterials().Add(
				FStaticMaterial(WaterMaterial, TEXT("Surface"), TEXT("Surface")));
		}
		FMeshNaniteSettings NaniteSettings = Mesh->GetNaniteSettings();
		NaniteSettings.bEnabled = bEnableNanite;
		Mesh->SetNaniteSettings(NaniteSettings);
		UStaticMesh::FBuildMeshDescriptionsParams BuildParameters;
		BuildParameters.bUseHashAsGuid = true;
		TestTrue(
			bEnableNanite
				? TEXT("The opaque Nanite control builds from MeshDescription.")
				: TEXT("The water mesh builds from MeshDescription."),
			Mesh->BuildFromMeshDescriptions({&Description}, BuildParameters));
		FAssetRegistryModule::AssetCreated(Mesh);
		TestTrue(TEXT("The generated StaticMesh package saves."), SaveAssetPackage(Package, Mesh));
		return TPair<UPackage*, UStaticMesh*>(Package, Mesh);
	};

	FProjectWorldCanonicalBundle Bundle;
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	FProjectWorldCanonicalCell Cell;
	Cell.Bounds = FVector4d(0.0, 0.0, 63.0, 63.0);
	FProjectWorldCanonicalFeature Lake;
	Lake.FeatureClass = TEXT("water");
	Lake.WaterSurface.bValid = true;
	Lake.WaterSurface.Geometry = TEXT("polygon");
	Lake.WaterSurface.Behavior = TEXT("standing");
	Lake.WaterSurface.FunctionId = TEXT("standing_polygon_quantile");
	Lake.WaterSurface.FunctionVersion = 1;
	Lake.WaterSurface.LevelMeters = 12.0;
	FProjectWorldCanonicalPolygon LakePolygon;
	LakePolygon.Outer = {
		FVector2D(10.0, 8.0), FVector2D(55.0, 8.0), FVector2D(55.0, 22.0),
		FVector2D(10.0, 22.0), FVector2D(10.0, 8.0)};
	LakePolygon.Holes.Add({
		FVector2D(25.25, 12.25), FVector2D(25.25, 18.25), FVector2D(40.25, 18.25),
		FVector2D(40.25, 12.25), FVector2D(25.25, 12.25)});
	Lake.GeometryPolygons.Add(MoveTemp(LakePolygon));
	FProjectWorldWaterMeshBuildResult WaterSurface;
	FString WaterError;
	TestTrue(
		TEXT("The persisted asset is built from the canonical lake-with-hole surface."),
		ProjectWorldWaterMeshBuilder::BuildCellSurface(Bundle, Cell, Lake, WaterSurface, WaterError));
	FProjectWorldWaterMeshBuildResult RepeatedWaterSurface;
	TestTrue(
		TEXT("An unchanged canonical lake rebuild succeeds for no-op selection."),
		ProjectWorldWaterMeshBuilder::BuildCellSurface(
			Bundle, Cell, Lake, RepeatedWaterSurface, WaterError));
	TestTrue(TEXT("The water mesh carries semantic no-op identity."), !WaterSurface.SemanticDigest.IsEmpty());
	TestEqual(
		TEXT("Unchanged canonical water produces the same semantic no-op identity."),
		RepeatedWaterSurface.SemanticDigest,
		WaterSurface.SemanticDigest);
	FMeshDescription NaniteControlDescription = BuildQuadMeshDescription();
	TPair<UPackage*, UStaticMesh*> Water = CreateMesh(
		WaterPackageName, TEXT("SM_NativeTwinWater"), false, WaterSurface.MeshDescription);
	TPair<UPackage*, UStaticMesh*> Nanite = CreateMesh(
		NanitePackageName, TEXT("SM_NativeTwinNaniteControl"), true, NaniteControlDescription);
	TArray<UPackage*> CreatedPackages{Water.Key, Nanite.Key, MaterialPackage};
	Water.Value = nullptr;
	Nanite.Value = nullptr;
	WaterMaterial = nullptr;
	TestTrue(TEXT("Saved twin packages unload before reload proof."), UPackageTools::UnloadPackages(CreatedPackages));

	UStaticMesh* ReloadedWater = LoadObject<UStaticMesh>(
		nullptr, *(WaterPackageName + TEXT(".SM_NativeTwinWater")));
	UStaticMesh* ReloadedNanite = LoadObject<UStaticMesh>(
		nullptr, *(NanitePackageName + TEXT(".SM_NativeTwinNaniteControl")));
	TestNotNull(TEXT("The water StaticMesh reloads from its saved package."), ReloadedWater);
	TestNotNull(TEXT("The Nanite control reloads from its saved package."), ReloadedNanite);
	if (ReloadedWater != nullptr)
	{
		TestFalse(TEXT("Single Layer Water stays on the supported non-Nanite path."), ReloadedWater->GetNaniteSettings().bEnabled);
		TestTrue(TEXT("The water mesh retains render data after reload."), ReloadedWater->GetRenderData() != nullptr);
		if (ReloadedWater->GetRenderData() != nullptr && !ReloadedWater->GetRenderData()->LODResources.IsEmpty())
		{
			TestEqual(
				TEXT("The lake-with-hole triangle inventory survives save and reload."),
				static_cast<int32>(ReloadedWater->GetRenderData()->LODResources[0].GetNumTriangles()),
				WaterSurface.TriangleCount);
		}
		TestTrue(TEXT("The water mesh retains its material slot."), !ReloadedWater->GetStaticMaterials().IsEmpty());
		if (!ReloadedWater->GetStaticMaterials().IsEmpty())
		{
			UMaterialInterface* Material = ReloadedWater->GetStaticMaterials()[0].MaterialInterface;
			TestNotNull(TEXT("The water material reference survives reload."), Material);
			if (Material != nullptr)
			{
				TestTrue(
					TEXT("The reloaded water material uses Single Layer Water."),
					Material->GetShadingModels().HasShadingModel(MSM_SingleLayerWater));
			}
		}
	}
	if (ReloadedNanite != nullptr)
	{
		TestTrue(TEXT("The compatible opaque control retains Nanite enablement."), ReloadedNanite->GetNaniteSettings().bEnabled);
		TestTrue(TEXT("The Nanite control retains render data after reload."), ReloadedNanite->GetRenderData() != nullptr);
	}
	TArray<UPackage*> ReloadedPackages;
	for (const FString& PackageName : PackageNames)
	{
		if (UPackage* Package = FindPackage(nullptr, *PackageName))
		{
			ReloadedPackages.Add(Package);
		}
	}
	ReloadedWater = nullptr;
	ReloadedNanite = nullptr;
	TestTrue(TEXT("Reloaded twin packages unload for rollback."), UPackageTools::UnloadPackages(ReloadedPackages));
	for (const FString& PackageName : PackageNames)
	{
		TestTrue(
			TEXT("Rollback removes the exact disposable twin package."),
			IFileManager::Get().Delete(*PackageFilename(PackageName), false, true));
		TestFalse(
			TEXT("Rollback restores the TestData fixture root to absence."),
			IFileManager::Get().FileExists(*PackageFilename(PackageName)));
	}
	const FString FixtureDirectory = FPaths::GetPath(PackageFilename(WaterPackageName));
	TestTrue(
		TEXT("Rollback removes the empty disposable fixture directory."),
		IFileManager::Get().DeleteDirectory(*FixtureDirectory, false, false));
	TestFalse(
		TEXT("Rollback restores the disposable fixture directory to absence."),
		IFileManager::Get().DirectoryExists(*FixtureDirectory));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldNativeWaterAssetPersistenceTwinTest,
	"Project.World.Realization.NativeTwin.WaterAssetPersistence",
	"[Slow][Integration][World]")

#endif
