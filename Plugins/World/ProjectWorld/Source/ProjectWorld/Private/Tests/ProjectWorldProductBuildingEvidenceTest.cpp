// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductBuildingEvidence.h"
#include "Utilities/ProjectSha256.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/Package.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldProductBuildingReferenceAdmissionTest,
	"Project.World.Presentation.ProductBuildingReferenceAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldProductBuildingReferenceAdmissionTest::RunTest(const FString& Parameters)
{
	const FString Map(TEXT("/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory"));
	const FString RuntimeHash = FString::ChrN(64, TEXT('a'));
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
		TEXT("tmp/world/product_building_evidence_tests"), FGuid::NewGuid().ToString() + TEXT(".json")));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("project-world-building-surface-reference:v1"));
	Root->SetStringField(TEXT("status"), TEXT("accepted"));
	Root->SetStringField(TEXT("map_package"), Map);
	Root->SetStringField(TEXT("runtime_profile_sha256"), RuntimeHash);
	TArray<TSharedPtr<FJsonValue>> Samples;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		for (const FString& Kind : {FString(TEXT("wall")), FString(TEXT("roof"))})
		{
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("kind"), Kind);
			Row->SetStringField(TEXT("mesh"), FString::Printf(
				TEXT("/ProjectWorldData/Generated/Territory/Buildings/SM_%d.SM_%d"), Index, Index));
			Row->SetStringField(TEXT("body_setup_guid"), FGuid::NewGuid().ToString());
			Row->SetStringField(TEXT("component_transform"), FTransform::Identity.ToString());
			Row->SetNumberField(TEXT("source_triangle"), Index);
			const auto Zero = TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(0),
				MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0)};
			const auto Normal = TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Kind == TEXT("wall") ? 1 : 0),
				MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(Kind == TEXT("roof") ? 1 : 0)};
			Row->SetArrayField(TEXT("source_local_target_cm"), Zero);
			Row->SetArrayField(TEXT("source_target_cm"), Zero);
			Row->SetArrayField(TEXT("reference_hit_cm"), Zero);
			Row->SetArrayField(TEXT("source_local_normal"), Normal);
			Row->SetArrayField(TEXT("normal"), Normal);
			Row->SetNumberField(TEXT("source_error_cm"), 0);
			Samples.Add(MakeShared<FJsonValueObject>(Row));
		}
	}
	Root->SetArrayField(TEXT("samples"), Samples);
	auto Write = [&]()
	{
		FString Payload, Hash;
		FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Payload));
		FFileHelper::SaveStringToFile(Payload, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FProjectSha256::HashFile(Path, Hash);
		return Hash;
	};
	FProjectWorldProductBuildingEvidence Evidence;
	FString Error;
	FString Hash = Write();
	TestTrue(TEXT("Six named source wall/roof references admit."), Evidence.Load(Path, Hash, Map, RuntimeHash, Error));
	TestFalse(TEXT("Changed source bytes refuse the old hash."),
		Evidence.Load(Path, FString::ChrN(64, TEXT('0')), Map, RuntimeHash, Error));
	TestFalse(TEXT("A reference for another runtime refuses."),
		Evidence.Load(Path, Hash, Map, FString::ChrN(64, TEXT('b')), Error));
	Root->SetArrayField(TEXT("samples"), {Samples[0], Samples[0], Samples[2], Samples[3], Samples[4], Samples[5]});
	Hash = Write();
	TestFalse(TEXT("A duplicate wall cannot replace the roof reference."), Evidence.Load(Path, Hash, Map, RuntimeHash, Error));
	Root->SetArrayField(TEXT("samples"), Samples);
	Samples[0]->AsObject()->SetArrayField(TEXT("source_target_cm"), {
		MakeShared<FJsonValueNumber>(1), MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0)});
	Hash = Write();
	TestFalse(TEXT("A target that differs from its current source transform refuses."),
		Evidence.Load(Path, Hash, Map, RuntimeHash, Error));
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldProductBuildingComponentPhysicsTest,
	"Project.World.Presentation.ProductBuildingComponentPhysics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldProductBuildingComponentPhysicsTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Initialization);
	if (!TestNotNull(TEXT("An isolated runtime physics world exists."), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	ACharacter* Character = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(2000, 2000, 2000), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("A native player controller exists."), Controller) ||
		!TestNotNull(TEXT("A native character capsule exists."), Character)) return false;
	Controller->Possess(Character);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The native static-mesh control exists."), Cube)) return false;
	const FString Root = TEXT("/ProjectWorldTestData/Generated/BuildingPhysics_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Map = Root + TEXT("/L_Physics");
	const FString RuntimeHash = FString::ChrN(64, TEXT('a'));
	TArray<UStaticMeshComponent*> Components;
	TArray<TSharedPtr<FJsonValue>> Samples;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FString Name = FString::Printf(TEXT("SM_Control_%d"), Index);
		UPackage* Package = CreatePackage(*(Root + TEXT("/Buildings/") + Name));
		Package->SetFlags(RF_Transient);
		UStaticMesh* Mesh = DuplicateObject<UStaticMesh>(Cube, Package, *Name);
		if (!TestNotNull(TEXT("A disposable source mesh exists."), Mesh) ||
			!TestNotNull(TEXT("The disposable mesh has a body setup."), Mesh->GetBodySetup())) return false;
		Mesh->SetFlags(RF_Transient);
		Mesh->GetBodySetup()->CreatePhysicsMeshes();
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
			FVector(Index * 400.0, 0, 0), FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("A transient runtime actor exists."), Actor)) return false;
		Actor->Tags.Add(TEXT("ProjectWorld.BuildingMassing.v2"));
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Components.Add(Component);
		for (int32 Kind = 0; Kind < 2; ++Kind)
		{
			const FBox Bounds = Mesh->GetBoundingBox();
			const FVector Normal = Kind == 0 ? FVector::ForwardVector : FVector::UpVector;
			const FVector LocalTarget = Bounds.GetCenter() + Normal * (Kind == 0 ? Bounds.GetExtent().X : Bounds.GetExtent().Z);
			const FVector Target = Component->GetComponentTransform().TransformPosition(LocalTarget);
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldBuildingComponentControl), true);
			if (!TestTrue(TEXT("The actual named component geometry ray intersects."), Component->LineTraceComponent(
				Hit, Target + Normal * 100, Target - Normal * 100, Query))) return false;
			TestFalse(TEXT("Geometry intersection is independent of the overlap-filter blocking flag."), Hit.bBlockingHit);
			auto Vector = [](const FVector& Value)
			{
				return TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Value.X),
					MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)};
			};
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("kind"), Kind == 0 ? TEXT("wall") : TEXT("roof"));
			Row->SetStringField(TEXT("mesh"), Mesh->GetPathName());
			Row->SetStringField(TEXT("body_setup_guid"), Mesh->GetBodySetup()->BodySetupGuid.ToString());
			Row->SetStringField(TEXT("component_transform"), Component->GetComponentTransform().ToString());
			Row->SetNumberField(TEXT("source_triangle"), Kind);
			Row->SetArrayField(TEXT("source_local_target_cm"), Vector(LocalTarget));
			// An oblique query direction distinguishes measured impact normals from copied metadata.
			const FVector QueryNormal = Index == 0 && Kind == 0 ? FVector(1, 0.2, 0).GetSafeNormal() : Normal;
			Row->SetArrayField(TEXT("source_local_normal"), Vector(QueryNormal));
			Row->SetArrayField(TEXT("source_target_cm"), Vector(Target));
			Row->SetArrayField(TEXT("normal"), Vector(QueryNormal));
			Row->SetArrayField(TEXT("reference_hit_cm"), Vector(Hit.ImpactPoint));
			Row->SetNumberField(TEXT("source_error_cm"), FVector::Distance(Hit.ImpactPoint, Target));
			Samples.Add(MakeShared<FJsonValueObject>(Row));
		}
	}
	TSharedRef<FJsonObject> Reference = MakeShared<FJsonObject>();
	Reference->SetStringField(TEXT("schema"), TEXT("project-world-building-surface-reference:v1"));
	Reference->SetStringField(TEXT("status"), TEXT("accepted"));
	Reference->SetStringField(TEXT("map_package"), Map);
	Reference->SetStringField(TEXT("runtime_profile_sha256"), RuntimeHash);
	Reference->SetArrayField(TEXT("samples"), Samples);
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
		TEXT("tmp/world/product_building_evidence_tests"), FGuid::NewGuid().ToString() + TEXT(".json")));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*Path); };
	FString Payload, Hash, Error;
	FJsonSerializer::Serialize(Reference, TJsonWriterFactory<>::Create(&Payload));
	FFileHelper::SaveStringToFile(Payload, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FProjectSha256::HashFile(Path, Hash);
	FProjectWorldProductBuildingEvidence Evidence;
	if (!TestTrue(TEXT("The actual component references authenticate."), Evidence.Load(Path, Hash, Map, RuntimeHash, Error)))
	{
		AddError(Error); return false;
	}
	TestTrue(TEXT("The runtime proof accepts native geometry intersections with Pawn blocking configured."), Evidence.Probe(*World, *Character, Error));
	TSharedRef<FJsonObject> Receipt = MakeShared<FJsonObject>();
	Evidence.AppendReceipt(*Receipt);
	const TSharedPtr<FJsonObject> Proof = Receipt->GetObjectField(TEXT("building_surface_evidence"));
	TestTrue(TEXT("The live player's capsule dimensions are measured."), Proof->HasTypedField<EJson::Number>(TEXT("capsule_radius_cm")));
	for (const TSharedPtr<FJsonValue>& Value : Proof->GetArrayField(TEXT("samples")))
	{
		const TSharedPtr<FJsonObject> Row = Value->AsObject();
		TestTrue(TEXT("Each named surface records its measured ray normal."), Row->HasTypedField<EJson::Array>(TEXT("cooked_hit_normal")));
		TestTrue(TEXT("Each named surface records the actual capsule contact."), Row->HasTypedField<EJson::Array>(TEXT("capsule_hit_cm")));
		TestTrue(TEXT("Each capsule contact records its measured normal."), Row->HasTypedField<EJson::Array>(TEXT("capsule_hit_normal")));
		const FVector ExpectedNormal = Row->GetStringField(TEXT("kind")) == TEXT("wall") ? FVector::ForwardVector : FVector::UpVector;
		for (const TCHAR* Field : {TEXT("cooked_hit_normal"), TEXT("capsule_hit_normal")})
		{
			const auto& Values = Row->GetArrayField(Field);
			const FVector Measured(Values[0]->AsNumber(), Values[1]->AsNumber(), Values[2]->AsNumber());
			TestTrue(TEXT("Measured contact normals follow native geometry, including an oblique query."), Measured.Equals(ExpectedNormal, 0.0001));
		}
	}
	Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	FHitResult RetainedRay;
	FCollisionQueryParams RetainedQuery(SCENE_QUERY_STAT(ProjectWorldBuildingRetainedRay), true);
	TestTrue(TEXT("Ignoring WorldStatic on the pawn leaves the target geometry ray intact."),
		Components[0]->LineTraceComponent(RetainedRay, FVector(150, 0, 0), FVector(-50, 0, 0), RetainedQuery));
	TestFalse(TEXT("Target rays cannot substitute for the pawn's actual collision responses."), Evidence.Probe(*World, *Character, Error));
	Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Components[0]->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	TestFalse(TEXT("Geometry intersections cannot waive configured Pawn blocking."), Evidence.Probe(*World, *Character, Error));
	Receipt = MakeShared<FJsonObject>();
	Evidence.AppendReceipt(*Receipt);
	TestFalse(TEXT("Refusal cannot retain earlier accepted geometry evidence."), Receipt->HasField(TEXT("building_surface_evidence")));
	Components[0]->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Components[0]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestFalse(TEXT("A disabled component cannot reuse its previous accepted physical rays."), Evidence.Probe(*World, *Character, Error));
	Components[0]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	const FVector EmptyTarget(10000, 10000, 10000);
	const auto EmptyVector = TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(EmptyTarget.X),
		MakeShared<FJsonValueNumber>(EmptyTarget.Y), MakeShared<FJsonValueNumber>(EmptyTarget.Z)};
	const TSharedPtr<FJsonObject> MissingRow = Samples[0]->AsObject();
	const auto& OldHit = MissingRow->GetArrayField(TEXT("reference_hit_cm"));
	MissingRow->SetArrayField(TEXT("source_target_cm"), EmptyVector);
	MissingRow->SetArrayField(TEXT("source_local_target_cm"), EmptyVector);
	MissingRow->SetNumberField(TEXT("source_error_cm"), FVector::Distance(EmptyTarget,
		FVector(OldHit[0]->AsNumber(), OldHit[1]->AsNumber(), OldHit[2]->AsNumber())));
	FJsonSerializer::Serialize(Reference, TJsonWriterFactory<>::Create(&Payload));
	FFileHelper::SaveStringToFile(Payload, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FProjectSha256::HashFile(Path, Hash);
	TestTrue(TEXT("The authenticated missing-target control has valid identity and transform."), Evidence.Load(Path, Hash, Map, RuntimeHash, Error));
	TestFalse(TEXT("A valid body's named ray miss refuses."), Evidence.Probe(*World, *Character, Error));
	Receipt = MakeShared<FJsonObject>();
	Evidence.AppendReceipt(*Receipt);
	const TSharedPtr<FJsonObject>* Rejected = nullptr;
	if (TestTrue(TEXT("A valid-body ray miss retains structured rejected evidence."), Receipt->TryGetObjectField(TEXT("building_surface_evidence"), Rejected)))
	{
		TestEqual(TEXT("Ray-miss evidence is rejected."), (*Rejected)->GetStringField(TEXT("status")), FString(TEXT("rejected")));
		const auto Failure = (*Rejected)->GetArrayField(TEXT("samples"))[0]->AsObject();
		TestEqual(TEXT("A ray miss is distinct from unloaded physics."), Failure->GetStringField(TEXT("ray_query_status")), FString(TEXT("miss")));
		TestFalse(TEXT("A miss cannot fabricate an impact point."), Failure->HasField(TEXT("cooked_hit_cm")));
	}
	return true;
}

#endif
