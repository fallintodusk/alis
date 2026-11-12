// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "AssetCompilingManager.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldGangwayAssemblyPhysicsTest,
	"Project.World.Realization.Road.StaticGangwayAssemblyPhysics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGangwayAssemblyPhysicsTest::RunTest(const FString&)
{
	TArray<TArray<FVector>> Faces;
	FString InputHash;
	const FString Input = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_GANGWAY_ASSEMBLY_INPUT"));
	if (Input.IsEmpty())
	{
		Faces = {{FVector(-4,-3,1.2), FVector(0,-3,1.2), FVector(0,3,1.2)},
			{FVector(-4,-3,1.2), FVector(0,3,1.2), FVector(-4,3,1.2)},
			{FVector(0,-1.5,1.2), FVector(20,-1.5,3.55), FVector(0,1.5,1.2)},
			{FVector(20,-1.5,3.55), FVector(20,1.5,3.55), FVector(0,1.5,1.2)},
			{FVector(20,-3,3.55), FVector(24,-3,3.55), FVector(20,3,3.55)},
			{FVector(24,-3,3.55), FVector(24,3,3.55), FVector(20,3,3.55)}};
	}
	else
	{
		FString Text; TSharedPtr<FJsonObject> Surface;
		const FString Expected = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_GANGWAY_ASSEMBLY_SHA256"));
		if (!TestTrue(TEXT("The exact supplied finite assembly is authenticated."),
			Expected.Len() == 64 && FProjectWorldCanonicalLoader::ComputeFileSha256(Input, InputHash) &&
			InputHash == Expected && FFileHelper::LoadFileToString(Text, *Input) &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Surface))) return false;
		if (!TestEqual(TEXT("The isolated model is explicitly identified."),
			Surface->GetStringField(TEXT("basis")), FString(TEXT("identified_model")))) return false;
		for (const auto& Value : Surface->GetArrayField(TEXT("triangles")))
		{
			const auto& Triangle = Value->AsArray(); TArray<FVector> Face;
			if (!TestEqual(TEXT("Each assembly face is a triangle."), Triangle.Num(), 3)) return false;
			for (const auto& Corner : Triangle)
			{
				const auto& P = Corner->AsArray();
				if (!TestEqual(TEXT("Each assembly point is XYZ."), P.Num(), 3)) return false;
				Face.Add(FVector(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber()));
				if (!TestFalse(TEXT("Assembly coordinates are finite."), Face.Last().ContainsNaN())) return false;
			}
			Faces.Add(MoveTemp(Face));
		}
	}
	if (!TestTrue(TEXT("The isolated assembly has a bounded nonempty face census."), Faces.Num() > 0 && Faces.Num() <= 4096)) return false;
	const FVector Origin(Faces[0][0].X, Faces[0][0].Y, 0);
	FMeshDescription Description; FStaticMeshAttributes Attributes(Description); Attributes.Register();
	Attributes.GetVertexInstanceUVs().SetNumChannels(1);
	const auto Group = Description.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Deck");
	TArray<FVector> Centers;
	for (const auto& Face : Faces)
	{
		FVector P[3];
		for (int32 I=0; I<3; ++I) P[I] = FVector((Face[I].X-Origin.X)*100, -(Face[I].Y-Origin.Y)*100, Face[I].Z*100);
		FVector Cross = FVector::CrossProduct(P[1]-P[0], P[2]-P[0]);
		if (!TestTrue(TEXT("The body has finite noncollapsed projected faces."), FMath::IsFinite(Cross.Z) && FMath::Abs(Cross.Z) > 1e-6)) return false;
		if (Cross.Z > 0) { Swap(P[1], P[2]); Cross = -Cross; }
		const FVector Normal = -Cross.GetSafeNormal();
		const FVector Tangent = (FVector::XAxisVector-Normal*Normal.X).GetSafeNormal();
		TArray<FVertexInstanceID> Instances;
		for (const auto& Point : P)
		{
			const auto Vertex = Description.CreateVertex(); Attributes.GetVertexPositions()[Vertex] = FVector3f(Point);
			const auto Instance = Description.CreateVertexInstance(Vertex);
			Attributes.GetVertexInstanceNormals()[Instance] = FVector3f(Normal);
			Attributes.GetVertexInstanceTangents()[Instance] = FVector3f(Tangent);
			Attributes.GetVertexInstanceBinormalSigns()[Instance] = 1;
			Attributes.GetVertexInstanceColors()[Instance] = FVector4f(1,1,1,1);
			Attributes.GetVertexInstanceUVs().Set(Instance, 0, FVector2f(Point.X,Point.Y)*.001f);
			Instances.Add(Instance);
		}
		Description.CreatePolygon(Group, Instances); Centers.Add((P[0]+P[1]+P[2])/3);
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
	Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), TEXT("Deck"), TEXT("Deck")));
	FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings(); Nanite.bEnabled = true;
	Nanite.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
	Nanite.FallbackPercentTriangles = Nanite.KeepPercentTriangles = 1;
	Nanite.FallbackRelativeError = Nanite.TrimRelativeError = 0;
	Mesh->SetNaniteSettings(Nanite); Mesh->bGenerateMeshDistanceField = false; Mesh->bHasNavigationData = false;
	Mesh->SetNumSourceModels(1);
	UStaticMesh::FBuildMeshDescriptionsParams Build; Build.bUseHashAsGuid = true;
	if (!TestTrue(TEXT("The complete body compiles through native StaticMesh build."), Mesh->BuildFromMeshDescriptions({&Description}, Build))) return false;
	FAssetCompilingManager::Get().FinishAllCompilation();
	Mesh->CreateBodySetup(); Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
	Mesh->GetBodySetup()->CreatePhysicsMeshes();
	const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
	if (!TestNotNull(TEXT("The physics scene is isolated from accepted maps."), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transient;
	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("The transient body actor exists."), Actor)) return false;
	auto* Component = Actor->GetStaticMeshComponent(); Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh); Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Component->SetCollisionResponseToAllChannels(ECR_Block); Component->RecreatePhysicsState();
	int32 Hits = 0; double MaximumContactErrorMeters = 0;
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(34, 88);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldGangwayAssembly), true);
	for (const auto& Center : Centers)
	{
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Center+FVector(0,0,250), Center-FVector(0,0,100),
			FQuat::Identity, ECC_Pawn, Capsule, Query) && Hit.bBlockingHit && Hit.GetComponent() == Component)
		{
			++Hits; MaximumContactErrorMeters = FMath::Max(MaximumContactErrorMeters, FMath::Abs(Hit.Location.Z-88-Center.Z)/100.);
		}
	}
	TestEqual(TEXT("Capsule sweeps at every supplied face center find blocking support."), Hits, Centers.Num());
	TestTrue(TEXT("Compiled capsule support agrees with the supplied surface within 3 cm."), MaximumContactErrorMeters <= .03);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FHitResult Missing;
	TestFalse(TEXT("The missing-collision control refuses real capsule support."), World->SweepSingleByChannel(Missing,
		Centers[0]+FVector(0,0,250), Centers[0]-FVector(0,0,100), FQuat::Identity, ECC_Pawn, Capsule, Query));
	AddInfo(FString::Printf(TEXT("GangwayAssemblyPhysics faces=%d capsule_hits=%d max_contact_error_m=%.9g input=%s; transient component proof only"),
		Faces.Num(), Hits, MaximumContactErrorMeters, *InputHash));
	return !HasAnyErrors();
}
#endif
