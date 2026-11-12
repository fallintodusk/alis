// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRuntimeMesh.h"
#include "ProjectWorldConstructedRuntimeInput.h"
#include "ProjectWorldConstructedRuntimeRoutes.h"
#include "Tests/ProjectWorldConstructedRuntimePawn.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldStoredSurface.h"
#include "ProjectWorldRuntimeNavigation.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldRealizationService.h"
#include "AssetCompilingManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/BrushComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "NavigationSystem.h"
#include "NavigationInvokerComponent.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedRuntimeTest
{
	UStaticMesh* Compile(const FProjectWorldStoredSurface& Unit,FAutomationTestBase& Test,UPackage* Package)
	{
		FMeshDescription D; FStaticMeshAttributes A(D); A.Register(); A.GetVertexInstanceUVs().SetNumChannels(1);
		const auto Group=D.CreatePolygonGroup(); A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Structure");
		for (const auto& T : Unit.Triangles)
		{
			const auto Cross=FVector::CrossProduct(T.Points[1]-T.Points[0],T.Points[2]-T.Points[0]);
			if (!Test.TestTrue(TEXT("Candidate source retains native top winding and nonzero area."),FMath::IsFinite(Cross.Z) && Cross.Z<0)) return nullptr;
			const auto N=-Cross.GetSafeNormal(0.0),Tangent=(FVector::XAxisVector-N*N.X).GetSafeNormal(0.0);
			TArray<FVertexInstanceID> Instances;
			for (const auto& P : T.Points)
			{
				const auto V=D.CreateVertex(); A.GetVertexPositions()[V]=FVector3f(P);
				const auto I=D.CreateVertexInstance(V); Instances.Add(I);
				A.GetVertexInstanceNormals()[I]=FVector3f(N); A.GetVertexInstanceTangents()[I]=FVector3f(Tangent);
				A.GetVertexInstanceBinormalSigns()[I]=1; A.GetVertexInstanceColors()[I]=FVector4f(1,1,1,1);
				A.GetVertexInstanceUVs().Set(I,0,FVector2f(P.X*.001,P.Y*.001));
			}
			D.CreateTriangle(Group,Instances);
		}
		auto* Mesh=Package ? NewObject<UStaticMesh>(Package,TEXT("Surface"),RF_Public|RF_Standalone) : NewObject<UStaticMesh>(GetTransientPackage());
		Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface)));
		Mesh->bAllowCPUAccess=true;
		auto Nanite=Mesh->GetNaniteSettings(); Nanite.bEnabled=true; Nanite.FallbackTarget=ENaniteFallbackTarget::PercentTriangles;
		Nanite.FallbackPercentTriangles=Nanite.KeepPercentTriangles=1; Nanite.FallbackRelativeError=Nanite.TrimRelativeError=0;
		Mesh->SetNaniteSettings(Nanite); Mesh->SetNumSourceModels(1); Mesh->bGenerateMeshDistanceField=false; Mesh->bHasNavigationData=true;
		Mesh->GetSourceModel(0).BuildSettings.bRemoveDegenerates=false;
		UStaticMesh::FBuildMeshDescriptionsParams Params; Params.bUseHashAsGuid=true;
		if (!Test.TestTrue(TEXT("The exact candidate compiles through the native StaticMesh builder."),Mesh->BuildFromMeshDescriptions({&D},Params))) return nullptr;
		FAssetCompilingManager::Get().FinishAllCompilation();
		Mesh->CreateBodySetup(); auto* Body=Mesh->GetBodySetup(); Body->CollisionTraceFlag=CTF_UseComplexAsSimple;
		Body->bDoubleSidedGeometry=true; Body->CreatePhysicsMeshes(); Mesh->CreateNavCollision(true); return Mesh;
	}
	bool SameGeometry(const FProjectWorldStoredSurface& Unit,UStaticMesh& Mesh,FAutomationTestBase& Test)
	{
		auto Key = [](const FVector P[3])
		{
			TArray<FString> Points;
			for (int32 I=0; I<3; ++I) Points.Add(FString::Printf(TEXT("%.17g,%.17g,%.17g"),P[I].X,P[I].Y,P[I].Z));
			Points.Sort(); return FString::Join(Points,TEXT(";"));
		};
		TArray<FString> Expected,Source,Fallback;
		for (const auto& T : Unit.Triangles) Expected.Add(Key(T.Points));
		Expected.Sort();
		const auto* Description=Mesh.GetMeshDescription(0);
		const auto* Render=Mesh.GetRenderData();
		if (!Test.TestNotNull(TEXT("Reloaded mesh retains its actual source description."),Description) ||
			!Test.TestNotNull(TEXT("Reloaded mesh retains its built render data."),Render) || Render->LODResources.IsEmpty()) return false;
		const auto Positions=FStaticMeshConstAttributes(*Description).GetVertexPositions();
		for (const auto Face : Description->Triangles().GetElementIDs())
		{
			FVector P[3];
			for (int32 I=0; I<3; ++I) P[I]=FVector(Positions[Description->GetVertexInstanceVertex(Description->GetTriangleVertexInstance(Face,I))]);
			if (!Test.TestTrue(TEXT("Saved source preserves nonzero native top-face winding."),FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).Z<0)) return false;
			Source.Add(Key(P));
		}
		const auto& LOD=Render->LODResources[0];
		if (!Test.TestTrue(TEXT("The diagnostic mesh retains CPU access to reloaded fallback buffers."),Mesh.bAllowCPUAccess &&
			LOD.IndexBuffer.GetAllowCPUAccess() && LOD.VertexBuffers.PositionVertexBuffer.GetAllowCPUAccess())) return false;
		const auto Indices=LOD.IndexBuffer.GetArrayView();
		if (!Test.TestEqual(TEXT("Every fallback index belongs to a complete triangle."),Indices.Num()%3,0)) return false;
		for (uint32 I=0; I+2<static_cast<uint32>(Indices.Num()); I+=3)
		{
			FVector P[3];
			for (int32 J=0; J<3; ++J) P[J]=FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+J]));
			if (!Test.TestTrue(TEXT("Reloaded fallback preserves nonzero native top-face winding."),FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).Z<0)) return false;
			Fallback.Add(Key(P));
		}
		Source.Sort(); Fallback.Sort();
		return Test.TestTrue(TEXT("Saved source triangles exactly preserve native XYZ and census."),Source==Expected) &&
			Test.TestTrue(TEXT("Saved 100-percent fallback triangles exactly preserve native XYZ and census."),Fallback==Expected) &&
			Test.TestTrue(TEXT("Reloaded mesh contains valid built Nanite data."),Mesh.HasValidNaniteData()) &&
			Test.TestTrue(TEXT("Reloaded complex collision remains cooked."),Mesh.GetBodySetup() &&
				Mesh.GetBodySetup()->CollisionTraceFlag==CTF_UseComplexAsSimple && Mesh.GetBodySetup()->bCreatedPhysicsMeshes);
	}
	bool FSavedCandidate::Unload(const TArray<UPackage*>& Packages)
	{
		UPackageTools::FUnloadPackageParams Params(Packages); Params.bResetTransBuffer=false;
		return UPackageTools::UnloadPackages(Params);
	}
	FSavedCandidate::FSavedCandidate(bool MapDomain)
	{
		const auto Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Mount=(MapDomain ? TEXT("/Game/ProjectWorldConstructedTest") : TEXT("/ProjectWorldConstructedTest"))+Id+TEXT("/");
		Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world/constructed_runtime")/Id)+TEXT("/");
		MountedDirectory=MapDomain ? Directory+TEXT("meshes/") : Directory;
		IFileManager::Get().MakeDirectory(*MountedDirectory,true); FPackageName::RegisterMountPoint(Mount,MountedDirectory);
	}
	FSavedCandidate::~FSavedCandidate()
	{
		TArray<UPackage*> Packages;
		for (const auto& Name : PackageNames) if (auto* Package=FindPackage(nullptr,*Name)) Packages.Add(Package);
		if (!Packages.IsEmpty() && Test) Test->TestTrue(TEXT("Isolated saved mesh packages unload after runtime teardown."),Unload(Packages));
		FPackageName::UnRegisterMountPoint(Mount,MountedDirectory);
	}
	UStaticMesh* FSavedCandidate::RoundTrip(const FProjectWorldStoredSurface& Unit,FAutomationTestBase& Check)
	{
		Test=&Check;
		const auto Name=Mount+FString::Printf(TEXT("SM_Structure_%d"),PackageNames.Num());
		PackageNames.Add(Name);
		auto* Package=CreatePackage(*Name); auto* Mesh=Compile(Unit,Check,Package); if (!Mesh) return nullptr;
		const auto Filename=FPackageName::LongPackageNameToFilename(Name,FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
		if (!Check.TestTrue(TEXT("The isolated actual candidate package saves."),UPackage::SavePackage(Package,Mesh,*Filename,Args))) return nullptr;
		TWeakObjectPtr<UStaticMesh> Before(Mesh); Mesh=nullptr;
		if (!Check.TestTrue(TEXT("The saved candidate package unloads before reload."),Unload({Package})) ||
			!Check.TestFalse(TEXT("The reload cannot reuse the original in-memory mesh."),Before.IsValid())) return nullptr;
		Mesh=LoadObject<UStaticMesh>(nullptr,*(Name+TEXT(".Surface")));
		if (!Check.TestNotNull(TEXT("The exact saved candidate reloads from disk."),Mesh)) return nullptr;
		return SameGeometry(Unit,*Mesh,Check) ? Mesh : nullptr;
	}
}
#endif
