// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldWaterRealization.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPersistentWaterLayerTest,
	"Project.World.Realization.Layers.PersistentWater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPersistentWaterLayerTest::RunTest(const FString& Parameters)
{
	const FString ProfilePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_landscape_water_twin.realization.json"));
	FProjectWorldRealizationProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRealizationProfile::Load(ProfilePath, Profile, ErrorCode, Error))
	{
		AddError(Error);
		return false;
	}
	const FProjectWorldRealizationLayer* WaterLayer = Profile.Layers.FindByPredicate([](const auto& Layer)
	{
		return Layer.GeneratorId == TEXT("project_water_mesh");
	});
	TestNotNull(TEXT("The fixture has an executable water layer."), WaterLayer);
	if (WaterLayer == nullptr)
	{
		return false;
	}

	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("persistent_water_test");
	Bundle.InputsHash = TEXT("persistent_water_input");
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	Bundle.HeightOriginMeters = 10.0;
	FProjectWorldCanonicalCell Cell;
	Cell.CellId = TEXT("persistent_water:x0:y0");
	Cell.Bounds = FVector4d(0.0, 0.0, 63.0, 63.0);
	Cell.OwnedFeatureIds.Add(TEXT("water/test/lake"));
	Bundle.Cells.Add(Cell);
	FProjectWorldCanonicalFeature Lake;
	Lake.FeatureId = TEXT("water/test/lake");
	Lake.FeatureClass = TEXT("water");
	Lake.OwnerCellId = Cell.CellId;
	Lake.WaterSurface.bValid = true;
	Lake.WaterSurface.SurfaceGroupId = TEXT("water/test/lake");
	Lake.WaterSurface.Geometry = TEXT("polygon");
	Lake.WaterSurface.Behavior = TEXT("standing");
	Lake.WaterSurface.FunctionId = TEXT("standing_polygon_quantile");
	Lake.WaterSurface.FunctionVersion = 1;
	Lake.WaterSurface.LevelMeters = 12.0;
	FProjectWorldCanonicalPolygon Polygon;
	Polygon.Outer = {
		FVector2D(5.0, 5.0), FVector2D(58.0, 5.0), FVector2D(58.0, 58.0),
		FVector2D(5.0, 58.0), FVector2D(5.0, 5.0)};
	Polygon.Holes.Add({
		FVector2D(20.0, 20.0), FVector2D(20.0, 30.0), FVector2D(30.0, 30.0),
		FVector2D(30.0, 20.0), FVector2D(20.0, 20.0)});
	Lake.GeometryPolygons.Add(MoveTemp(Polygon));
	Bundle.Features.Add(Lake.FeatureId, MoveTemp(Lake));

	FProjectWorldRealizationResult Result;
	FProjectWorldLayerInventory Inventory;
	Inventory.LayerId = WaterLayer->LayerId;
	Inventory.GeneratorId = WaterLayer->GeneratorId;
	Inventory.GeneratorVersion = WaterLayer->GeneratorVersion;
	Inventory.ArtifactRoot = WaterLayer->ArtifactRoot;
	Inventory.NormalizedLayerContractHash = WaterLayer->ContractHash;
	Inventory.FinalDirtyUnits.Add(TEXT("*"));
	Result.LayerInventories.Add(MoveTemp(Inventory));
	UWorld* World = GEditor->NewMap(true);
	TestTrue(
		TEXT("The production water layer realizes the canonical cell."),
		ProjectWorldWaterRealization::Apply(World, Bundle, Profile, Result, Error));

	AStaticMeshActor* WaterActor = nullptr;
	FString InitialSemantic;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		FString CellId;
		FString Semantic;
		if (ProjectWorldWaterRealization::ReadActorIdentity(*It, CellId, Semantic))
		{
			WaterActor = *It;
			InitialSemantic = Semantic;
			TestEqual(TEXT("Water actor identity is cell-local."), CellId, Cell.CellId);
			TestEqual(TEXT("Water actor semantic identity is SHA-256."), Semantic.Len(), 64);
		}
	}
	TestNotNull(TEXT("One persistent water actor is created."), WaterActor);
	if (WaterActor != nullptr)
	{
		UStaticMeshComponent* Component = WaterActor->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
		TestNotNull(TEXT("The water actor consumes a persistent StaticMesh."), Mesh);
		TestEqual(TEXT("Water has no collision."), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestTrue(TEXT("Water is spatially loaded."), WaterActor->GetIsSpatiallyLoaded());
		if (Mesh != nullptr)
		{
			TestFalse(TEXT("Prototype Water remains non-Nanite."), Mesh->GetNaniteSettings().bEnabled);
			TestTrue(TEXT("The persistent water mesh has one source model."), Mesh->GetNumSourceModels() == 1);
			if (Mesh->GetNumSourceModels() == 1)
			{
				TestEqual(
					TEXT("Water opts out of unused asynchronous mesh distance-field generation."),
					Mesh->GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale,
					0.0f);
			}
			TestTrue(TEXT("The water mesh package was saved."), FPaths::FileExists(
				FPackageName::LongPackageNameToFilename(
					Mesh->GetOutermost()->GetName(),
					FPackageName::GetAssetPackageExtension())));
			UMaterial* WaterMaterial = !Mesh->GetStaticMaterials().IsEmpty()
				? Cast<UMaterial>(Mesh->GetStaticMaterials()[0].MaterialInterface)
				: nullptr;
			TestTrue(
				TEXT("The generated prototype Water material is opaque Default Lit."),
				WaterMaterial != nullptr &&
				WaterMaterial->GetShadingModels().HasShadingModel(MSM_DefaultLit) &&
				WaterMaterial->BlendMode == BLEND_Opaque);
			TestTrue(
				TEXT("The profile-owned clearance separates Water 0.25 m above its canonical level."),
				FMath::IsNearlyEqual(Mesh->GetBounds().Origin.Z, 225.0, 0.1));
			const UMaterialExpressionConstant3Vector* BaseColor = WaterMaterial != nullptr
				? FindObject<UMaterialExpressionConstant3Vector>(WaterMaterial, TEXT("BaseColor"))
				: nullptr;
			const UMaterialExpressionConstant* Roughness = WaterMaterial != nullptr
				? FindObject<UMaterialExpressionConstant>(WaterMaterial, TEXT("Roughness"))
				: nullptr;
			const UMaterialExpressionConstant* Specular = WaterMaterial != nullptr
				? FindObject<UMaterialExpressionConstant>(WaterMaterial, TEXT("Specular"))
				: nullptr;
			TestTrue(
				TEXT("The prototype water surface owns a clearly blue legibility color."),
				BaseColor != nullptr && BaseColor->Constant.B >= 0.60f &&
				BaseColor->Constant.B > BaseColor->Constant.G * 3.0f &&
				BaseColor->Constant.G > BaseColor->Constant.R * 3.0f);
			TestTrue(
				TEXT("The solid-blue placeholder has no reflective material response."),
				Roughness != nullptr && Roughness->R == 1.0f &&
				Specular != nullptr && Specular->R == 0.0f);
			TestEqual(
				TEXT("The Water graph contains only three time-invariant constants."),
				WaterMaterial != nullptr
					? WaterMaterial->GetExpressionCollection().Expressions.Num()
					: 0,
				3);
		}
	}

	FProjectWorldRealizationResult LifecycleResult;
	TestTrue(
		TEXT("The shared Apply lifecycle preserves a current water-layer actor."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World, Bundle, FString(), false, LifecycleResult));
	TestEqual(
		TEXT("Current water-layer actors are not retired before the layer update."),
		LifecycleResult.RemovedActorCount,
		0);

	Bundle.Features.FindChecked(TEXT("water/test/lake")).WaterSurface.LevelMeters = 13.0;
	Result.LayerInventories[0].FinalDirtyUnits = {Cell.CellId};
	const int32 UpdatedBeforeChange = Result.UpdatedActorCount;
	TestTrue(
		TEXT("A changed canonical water cell updates through the production realization path."),
		ProjectWorldWaterRealization::Apply(World, Bundle, Profile, Result, Error));
	TestEqual(
		TEXT("Exactly one persistent water actor is replaced for one changed cell."),
		Result.UpdatedActorCount,
		UpdatedBeforeChange + 1);
	FString UpdatedCellId;
	FString UpdatedSemantic;
	TestTrue(
		TEXT("The updated actor retains readable cell-local identity."),
		ProjectWorldWaterRealization::ReadActorIdentity(WaterActor, UpdatedCellId, UpdatedSemantic));
	TestEqual(TEXT("The updated water actor keeps its canonical cell."), UpdatedCellId, Cell.CellId);
	TestNotEqual(TEXT("Changed canonical water changes semantic identity."), UpdatedSemantic, InitialSemantic);

	Result.LayerInventories[0].FinalDirtyUnits.Reset();
	const int32 UpdatedBeforeNoOp = Result.UpdatedActorCount;
	TestTrue(
		TEXT("An unchanged water layer executes as a semantic no-op."),
		ProjectWorldWaterRealization::Apply(World, Bundle, Profile, Result, Error));
	TestEqual(
		TEXT("A semantic no-op does not rewrite the water actor."),
		Result.UpdatedActorCount,
		UpdatedBeforeNoOp);

	TArray<UPackage*> Packages;
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		if (It->GetName().StartsWith(WaterLayer->ArtifactRoot))
		{
			Packages.Add(*It);
		}
	}
	if (!Packages.IsEmpty())
	{
		UPackageTools::UnloadPackages(Packages);
	}
	const FString RootFilename = FPackageName::LongPackageNameToFilename(WaterLayer->ArtifactRoot);
	IFileManager::Get().DeleteDirectory(*RootFilename, false, true);
	TestFalse(TEXT("Disposable water packages are removed after the proof."), FPaths::DirectoryExists(RootFilename));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldPersistentWaterLayerTest,
	"Project.World.Realization.Layers.PersistentWater",
	"[Slow][Integration][World]")

#endif
