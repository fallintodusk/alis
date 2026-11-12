// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedRuntimeInput.h"
#include "ProjectWorldConstructedRuntimeRoutes.h"
#include "ProjectWorldConstructedRuntimeMesh.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldModeledApproachInputTest,
	"Project.World.Realization.Constructed.ModeledApproachInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldModeledApproachInputTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedRuntimeTest;
	auto Root=MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("physical_owner"),TEXT("modeled_approach_diagnostic"));
	Root->SetStringField(TEXT("canonical_crs"),TEXT("EPSG:32618")); Root->SetStringField(TEXT("vertical_datum"),TEXT("EPSG:3855"));
	Root->SetNumberField(TEXT("diagnostic_height_origin_m"),0); Root->SetNumberField(TEXT("presentation_lift_m"),0);
	Root->SetNumberField(TEXT("maximum_grade"),.05); Root->SetNumberField(TEXT("maximum_cross_slope"),.02);
	Root->SetNumberField(TEXT("maximum_model_error_m"),.001);
	auto Array = [](std::initializer_list<double> Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const double V : Values) Result.Add(MakeShared<FJsonValueNumber>(V));
		return Result;
	};
	Root->SetArrayField(TEXT("engine_georeference_origin_m"),Array({0,0}));
	auto Cell=MakeShared<FJsonObject>(); Cell->SetStringField(TEXT("cell_id"),TEXT("fixture_cell"));
	Cell->SetArrayField(TEXT("bounds"),Array({-2,-1,12,1})); Root->SetObjectField(TEXT("canonical_cell"),Cell);
	Root->SetArrayField(TEXT("source_route"),{MakeShared<FJsonValueArray>(Array({-1,0})),MakeShared<FJsonValueArray>(Array({11,0}))});
	Root->SetArrayField(TEXT("mapped_source_route"),{MakeShared<FJsonValueArray>(Array({0,0})),MakeShared<FJsonValueArray>(Array({10,0}))});
	const TArray<FProjectWorldSurfaceTriangle> Field={
		{{FVector(-2,-1,3),FVector(0,-1,3),FVector(-2,1,3)}},{{FVector(0,-1,3),FVector(0,1,3),FVector(-2,1,3)}},
		{{FVector(0,-1,3),FVector(10,-1,3.4),FVector(0,1,3)}},{{FVector(10,-1,3.4),FVector(10,1,3.4),FVector(0,1,3)}},
		{{FVector(10,-1,3.4),FVector(12,-1,3.4),FVector(10,1,3.4)}},{{FVector(12,-1,3.4),FVector(12,1,3.4),FVector(10,1,3.4)}}};
	TArray<TSharedPtr<FJsonValue>> Faces;
	for (const auto& T : Field)
	{
		TArray<TSharedPtr<FJsonValue>> Points;
		for (const auto& P : T.Points) Points.Add(MakeShared<FJsonValueArray>(Array({P.X,P.Y,P.Z})));
		Faces.Add(MakeShared<FJsonValueArray>(Points));
	}
	Root->SetArrayField(TEXT("source_model_triangles"),Faces);
	const FString Directory=FPaths::ProjectDir()/TEXT("tmp/world/constructed_runtime/fixtures")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!TestTrue(TEXT("The self-contained fixture uses isolated project scratch."),IFileManager::Get().MakeDirectory(*Directory,true))) return false;
	const FString Path=Directory/TEXT("approach.json"); FInput Input; FString Error;
	auto Read = [&]()
	{
		FString Text,Hash;
		if (!FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Path) ||
			!FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Hash)) return false;
		return Load(Input,Error,Path,Hash);
	};
	if (!TestTrue(TEXT("A small canonical approach builds actual native geometry without retained scratch inputs."),Read())) { AddError(Error); return false; }
	TestTrue(TEXT("The canonical-field mode is explicit and nonempty."),Input.Approach && Input.Units.Num()==1 && !Input.Units[0].Triangles.IsEmpty());
	TestTrue(TEXT("The final native gradient retains the explicit quality gate."),Input.Slopes.MaximumGrade<=.05 && Input.Slopes.MaximumCrossSlope<=.02);
	Root->SetArrayField(TEXT("native_physical_units"),{});
	TestFalse(TEXT("Canonical and native geometry modes cannot be mixed."),Read());
	TestTrue(TEXT("Mode refusal clears previously successful geometry."),Input.Units.IsEmpty()); Root->RemoveField(TEXT("native_physical_units"));
	Root->SetNumberField(TEXT("maximum_grade"),.06);
	TestFalse(TEXT("A fixture cannot weaken the accepted ordinary grade gate."),Read()); Root->SetNumberField(TEXT("maximum_grade"),.05);
	Root->SetNumberField(TEXT("presentation_lift_m"),.15);
	TestFalse(TEXT("A canonical physical approach cannot acquire a presentation lift."),Read()); Root->SetNumberField(TEXT("presentation_lift_m"),0);
	Root->SetStringField(TEXT("vertical_datum"),TEXT("unknown"));
	TestFalse(TEXT("Unknown vertical meaning cannot reach native runtime proof."),Read());
	Root->SetStringField(TEXT("vertical_datum"),TEXT("EPSG:3855"));
	FProjectWorldCanonicalBundle Frame; FProjectWorldCanonicalCell NativeCell; NativeCell.Bounds=FVector4(-2,-1,12,1);
	FMeshDescription Native; FVector NativeOrigin; TArray<int32> Sources;
	if (!TestTrue(TEXT("Connector fixture captures real native output and its original source ordinals."),
		ProjectWorldConstructedMesh::Build(Frame,NativeCell,Field,.001,Native,NativeOrigin,Error,&Sources))) { AddError(Error); return false; }
	auto Unit=MakeShared<FJsonObject>(); Unit->SetArrayField(TEXT("origin_cm"),Array({NativeOrigin.X,NativeOrigin.Y,NativeOrigin.Z}));
	TArray<TSharedPtr<FJsonValue>> Points,Indices,Trace; const auto Positions=FStaticMeshConstAttributes(Native).GetVertexPositions(); int32 Count=0;
	for (const auto Triangle : Native.Triangles().GetElementIDs())
	{
		for (int32 I=0; I<3; ++I)
		{
			const FVector P(Positions[Native.GetVertexInstanceVertex(Native.GetTriangleVertexInstance(Triangle,I))]);
			Points.Add(MakeShared<FJsonValueArray>(Array({P.X,P.Y,P.Z})));
		}
		Indices.Add(MakeShared<FJsonValueArray>(Array({double(Count),double(Count+1),double(Count+2)}))); Count+=3;
	}
	for (const int32 Source : Sources) Trace.Add(MakeShared<FJsonValueNumber>(Source));
	Unit->SetArrayField(TEXT("vertices_cm"),Points); Unit->SetArrayField(TEXT("triangles"),Indices); Unit->SetArrayField(TEXT("source_triangle_ordinals"),Trace);
	Root->SetArrayField(TEXT("native_physical_units"),{MakeShared<FJsonValueObject>(Unit)});
	Root->SetStringField(TEXT("physical_owner"),TEXT("complete_structural_surface_diagnostic")); Root->RemoveField(TEXT("canonical_cell"));
	Root->SetStringField(TEXT("body_role_quality_sha256"),FString::ChrN(64,'a')); Root->SetStringField(TEXT("assembly_sha256"),FString::ChrN(64,'b'));
	TestFalse(TEXT("Partial provenance cannot silently disable connector selection."),Read());
	Root->SetArrayField(TEXT("connector_source_triangle_ordinals"),Array({2,3}));
	if (!TestTrue(TEXT("Pinned native source controls select exactly the two connector faces."),Read())) { AddError(Error); return false; }
	TestEqual(TEXT("The source control retains an exact unique connector census."),Input.ConnectorSources.Num(),2);
	Unit->SetArrayField(TEXT("source_triangle_ordinals"),{});
	TestFalse(TEXT("A trace with the wrong actual face cardinality refuses."),Read());
	Unit->SetArrayField(TEXT("source_triangle_ordinals"),Trace); Root->SetArrayField(TEXT("connector_source_triangle_ordinals"),Array({6}));
	TestFalse(TEXT("Connector ordinals outside the authenticated field refuse."),Read());
	Root->SetArrayField(TEXT("connector_source_triangle_ordinals"),Array({2,2}));
	TestFalse(TEXT("Repeated connector source ordinals refuse."),Read());
	for (auto& Value : Trace) if (Value->AsNumber()==2) Value=MakeShared<FJsonValueNumber>(0);
	Unit->SetArrayField(TEXT("source_triangle_ordinals"),Trace); Root->SetArrayField(TEXT("connector_source_triangle_ordinals"),Array({2}));
	TestFalse(TEXT("A selected source that never emitted cannot calibrate disconnection."),Read());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedRuntimeTest,
	"Project.World.Realization.Constructed.CandidatePhysics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedRuntimeTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedRuntimeTest;
	FInput Input; FString Error;
	if (!TestTrue(TEXT("The structural runtime input is bounded and authenticated."),Load(Input,Error,
		FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_INPUT")),
		FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_SHA256"))))) { AddError(Error); return false; }
	FSavedCandidate Saved;
	FTestWorldWrapper Fixture;
	if (!TestTrue(TEXT("The native game-world fixture initializes."),Fixture.CreateTestWorld(EWorldType::Game))) return false;
	auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
	auto* NavigationConfig=NewObject<UNavigationSystemModuleConfig>(World->GetWorldSettings());
	NavigationConfig->NavigationSystemClass=FSoftClassPath(UNavigationSystemV1::StaticClass());
	World->GetWorldSettings()->SetNavigationSystemConfigOverride(NavigationConfig);
	FNavigationSystem::AddNavigationSystemToWorld(*World,FNavigationSystemRunMode::GameMode,NavigationConfig);
	if (!TestNotNull(TEXT("The isolated game fixture includes the native navigation system."),World->GetNavigationSystem())) return false;
	TArray<UStaticMeshComponent*> Components; TArray<FVector> Centers;
	for (const auto& Unit : Input.Units)
	{
		auto* Mesh=Saved.RoundTrip(Unit,*this); if (!Mesh) return false;
		auto* Actor=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),Unit.Origin,FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("The candidate has a real isolated static-mesh actor."),Actor)) return false;
		auto* C=Actor->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); C->SetCollisionResponseToAllChannels(ECR_Block);
		C->SetCanEverAffectNavigation(true); C->RecreatePhysicsState(); Components.Add(C);
		for (const auto& T : Unit.Triangles) Centers.Add((T.Points[0]+T.Points[1]+T.Points[2])/3+Unit.Origin);
	}
	int32 Hits=0,UnknownContactHeights=0; double MaxContact=0;
	const auto Capsule=FCollisionShape::MakeCapsule(34,88); FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldStructuralRuntime),true);
	for (const auto& Center : Centers)
	{
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit,Center+FVector(0,0,250),Center-FVector(0,0,100),FQuat::Identity,ECC_Pawn,Capsule,Query) &&
			Hit.bBlockingHit && Components.Contains(Cast<UStaticMeshComponent>(Hit.GetComponent())))
		{
			++Hits; double Expected;
			if (!ExpectedHeight(Input,Center,Expected)) ++UnknownContactHeights;
			else MaxContact=FMath::Max(MaxContact,FMath::Abs(Hit.Location.Z-88-Expected));
		}
	}
	TestEqual(TEXT("Actual cooked collision supports every candidate face center."),Hits,Centers.Num());
	TestEqual(TEXT("Every capsule contact has qualified source support."),UnknownContactHeights,0);
	TestTrue(TEXT("Cooked capsule contacts stay within 3 cm of the source field."),MaxContact<=3);
	if (!TestTrue(TEXT("The isolated fixture begins real gameplay."),Fixture.BeginPlayInTestWorld())) return false;
	auto* Controller=World->SpawnActor<APlayerController>();
	auto* Character=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),Input.Start+FVector(0,0,150),FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("A real player controller exists."),Controller) || !TestNotNull(TEXT("A real walking character exists."),Character)) return false;
	Controller->SetAsLocalPlayerController(); Controller->bAutoManageActiveCameraTarget=false;
	Character->GetCapsuleComponent()->SetCapsuleSize(34,88); Controller->Possess(Character);
	auto* Invoker=NewObject<UNavigationInvokerComponent>(Character);
	Character->AddInstanceComponent(Invoker);
	Invoker->SetGenerationRadii(FVector::Distance(Input.Start,Input.End)+1000,FVector::Distance(Input.Start,Input.End)+2000);
	Invoker->RegisterComponent();
	TestTrue(TEXT("The isolated controller supplies actual local possessed movement."),Character->IsLocallyControlled());
	auto* Movement=Character->GetCharacterMovement(); Movement->MaxWalkSpeed=220;
	for (int32 I=0; I<60; ++I) Fixture.TickTestWorld(1.f/60);
	TestTrue(TEXT("The possessed character settles on the physical surface."),Controller->GetPawn()==Character && Movement->IsMovingOnGround());
	int32 WalkingFrames=0,FallingFrames=0,UnknownHeightFrames=0; double MaximumWalkingHeightError=0;
	FRouteMetrics RouteMetrics;
	if (Input.Grouped)
	{
		if (!WalkRoutes(Input,Character,[&](float Delta){ Fixture.TickTestWorld(Delta); },*this,RouteMetrics)) return false;
		WalkingFrames=RouteMetrics.WalkingFrames; FallingFrames=RouteMetrics.FallingFrames; UnknownHeightFrames=RouteMetrics.UnknownFrames; MaximumWalkingHeightError=RouteMetrics.MaximumFootError;
	}
	else for (const auto& Target : {Input.End,Input.Start})
	{
		bool Reached=false;
		for (int32 I=0; I<1800; ++I)
		{
			FVector Delta=Target-Character->GetActorLocation(); Delta.Z=0;
			if (Delta.Length()<15) { Reached=true; break; }
			Character->AddMovementInput(Delta.GetSafeNormal(),1); Fixture.TickTestWorld(1.f/60);
			WalkingFrames+=Movement->IsMovingOnGround(); FallingFrames+=Movement->IsFalling();
			double Expected;
			if (!ExpectedHeight(Input,Character->GetActorLocation(),Expected)) ++UnknownHeightFrames;
			else MaximumWalkingHeightError=FMath::Max(MaximumWalkingHeightError,FMath::Abs(Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Expected));
		}
		TestTrue(TEXT("The possessed character walks the complete mapped route in each direction."),Reached);
	}
	TestEqual(TEXT("The actual walking route has no falling frames."),FallingFrames,0);
	TestEqual(TEXT("All actual walking positions retain qualified source support."),UnknownHeightFrames,0);
	TestTrue(TEXT("Actual player feet follow the modeled surface, not an unrelated lower support."),MaximumWalkingHeightError<=3);
	auto* Ground=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
		FVector((Input.Start.X+Input.End.X)*.5,(Input.Start.Y+Input.End.Y)*.5,-Input.Frame.HeightOriginMeters*100-5),FRotator::ZeroRotator);
	auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("The lower-support control has a native actor and mesh."),Ground) || !Cube) return false;
	auto* GroundComponent=Ground->GetStaticMeshComponent(); GroundComponent->SetMobility(EComponentMobility::Movable);
	GroundComponent->SetStaticMesh(Cube); Ground->SetActorScale3D(FVector(100,100,.1));
	GroundComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); GroundComponent->SetCollisionResponseToAllChannels(ECR_Block);
	GroundComponent->SetCanEverAffectNavigation(true); GroundComponent->RecreatePhysicsState();
	auto* NavBounds=World->SpawnActor<ANavMeshBoundsVolume>();
	if (!TestNotNull(TEXT("The isolated route has navigation bounds."),NavBounds)) return false;
	NavBounds->GetBrushComponent()->SetMobility(EComponentMobility::Movable);
	FProjectWorldRuntimeProfile Profile; Profile.NavigationPaddingMeters=10; Profile.NavigationHeightMeters=10;
	FProjectWorldRealizationResult NavReceipt; ANavigationData* NavData=nullptr;
	if (!TestTrue(TEXT("The existing runtime owner builds the actual route navigation domain."),
		ProjectWorldRuntimeNavigation::ConfigureRouteDomain(World,Character,NavBounds,Input.Start,Input.End,Profile,NavReceipt,NavData,Error)))
	{
		auto* System=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		auto* Recast=System ? Cast<ARecastNavMesh>(System->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
		TArray<FBox> Bounds; if (System && Recast) System->GetNavigationBoundsForNavData(*Recast,Bounds);
		AddError(FString::Printf(TEXT("%s bounds=%s registered=%d invokers=%d active_tiles=%d time=%.6f"),*Error,
			*NavBounds->GetComponentsBoundingBox(true).ToString(),Bounds.Num(),System ? System->GetInvokerLocations().Num() : -1,
			Recast ? Recast->GetNumActiveTiles() : -1,World->GetTimeSeconds())); return false;
	}
	auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	int32 PathPoints=0; double MaximumNavHeightError=0;
	auto ProjectDeckEndpoints = [&](FNavLocation& Start,FNavLocation& End)
	{
		if (!Navigation || !NavData || !Navigation->ProjectPointToNavigation(Input.Start,Start,FVector(50,50,30),NavData) ||
			!Navigation->ProjectPointToNavigation(Input.End,End,FVector(50,50,30),NavData)) return false;
		double StartZ,EndZ;
		return ExpectedHeight(Input,Start.Location,StartZ) && ExpectedHeight(Input,End.Location,EndZ) &&
			FMath::Abs(Start.Location.Z-StartZ)<=30 && FMath::Abs(End.Location.Z-EndZ)<=30;
	};
	auto HasDeckRoute = [&]()
	{
		PathPoints=0; MaximumNavHeightError=0; FNavLocation Start,End;
		if (!ProjectDeckEndpoints(Start,End)) return false;
		FPathFindingQuery QueryPath(Character,*NavData,Start.Location,End.Location); QueryPath.SetAllowPartialPaths(false);
		const auto Path=Navigation->FindPathSync(QueryPath);
		if (!Path.IsSuccessful() || !Path.Path.IsValid() || Path.Path->IsPartial()) return false;
		for (const auto& P : Path.Path->GetPathPoints())
		{
			double Z; if (!ExpectedHeight(Input,P.Location,Z)) return false;
			MaximumNavHeightError=FMath::Max(MaximumNavHeightError,FMath::Abs(P.Location.Z-Z)); ++PathPoints;
		}
		return PathPoints>=2 && MaximumNavHeightError<=30;
	};
	if (!TestTrue(TEXT("Actual navigation connects the two decks at their modeled heights despite lower Ground."),HasDeckRoute())) return false;
	const int32 CompletePathPoints=PathPoints; const double CompleteNavHeightError=MaximumNavHeightError;
	if (Input.Grouped && !NavRoutes(Input,World,Character,NavBounds,NavData,*this,RouteMetrics)) return false;
	int32 RemovedRampFaces=0; TOptional<FVector> RetainedContact;
	for (int32 I=0; I<Input.Units.Num(); ++I)
	{
		auto Broken=Input.Units[I]; Broken.Triangles.Reset();
		int32 TriangleOrdinal=0;
		for (const auto& T : Input.Units[I].Triangles)
		{
			const double MinZ=FMath::Min3(T.Points[0].Z,T.Points[1].Z,T.Points[2].Z),MaxZ=FMath::Max3(T.Points[0].Z,T.Points[1].Z,T.Points[2].Z);
			bool Remove=Input.ConnectorSources.IsEmpty() ? MaxZ-MinZ>1 :
				(!Input.Grouped || Input.UnitGroups[I]==Input.ConnectorGroup) && Input.ConnectorSources.Contains(Input.UnitSources[I][TriangleOrdinal]);
			if (Input.Approach)
			{
				const FVector2D Axis(Input.CoreEnd-Input.CoreStart),Center((T.Points[0]+T.Points[1]+T.Points[2])/3+Input.Units[I].Origin-Input.CoreStart);
				const double Station=FVector2D::DotProduct(Center,Axis.GetSafeNormal());
				Remove=Station>0 && Station<Axis.Length();
			}
			if (Remove) ++RemovedRampFaces;
			else { Broken.Triangles.Add(T); if (!RetainedContact.IsSet()) RetainedContact=(T.Points[0]+T.Points[1]+T.Points[2])/3+Broken.Origin; }
			++TriangleOrdinal;
		}
		if (Broken.Triangles.IsEmpty()) Components[I]->SetStaticMesh(nullptr);
		else { auto* BrokenMesh=Compile(Broken,*this); if (!BrokenMesh) return false; Components[I]->SetStaticMesh(BrokenMesh); }
		Components[I]->RecreatePhysicsState();
	}
	TestTrue(TEXT("The disconnection control actually removes connector geometry."),RemovedRampFaces>0);
	Fixture.TickTestWorld(1.f/60);
	if (!TestTrue(TEXT("The same native navigation owner rebuilds after physical gangway removal."),
		ProjectWorldRuntimeNavigation::ConfigureRouteDomain(World,Character,NavBounds,Input.Start,Input.End,Profile,NavReceipt,NavData,Error)))
	{ AddError(Error); return false; }
	FNavLocation BrokenStart,BrokenEnd;
	if (!TestTrue(TEXT("Both disconnected decks retain navigation at their modeled heights."),ProjectDeckEndpoints(BrokenStart,BrokenEnd))) return false;
	FPathFindingQuery BrokenQuery(Character,*NavData,BrokenStart.Location,BrokenEnd.Location); BrokenQuery.SetAllowPartialPaths(false);
	const auto BrokenPath=Navigation->FindPathSync(BrokenQuery);
	TestFalse(TEXT("Lower Ground cannot manufacture a complete route when the gangway is missing."),
		BrokenPath.IsSuccessful() && BrokenPath.Path.IsValid() && !BrokenPath.Path->IsPartial());
	Query.AddIgnoredActor(Character);
	Query.AddIgnoredActor(Ground);
	if (!TestTrue(TEXT("The collision-off calibration keeps a real deck contact after connector removal."),RetainedContact.IsSet())) return false;
	FHitResult RetainedHit; const FVector Contact=RetainedContact.GetValue();
	TestTrue(TEXT("The retained deck supplies capsule support immediately before collision is disabled."),World->SweepSingleByChannel(RetainedHit,
		Contact+FVector(0,0,250),Contact-FVector(0,0,100),FQuat::Identity,ECC_Pawn,Capsule,Query) && Components.Contains(Cast<UStaticMeshComponent>(RetainedHit.GetComponent())));
	for (auto* C : Components) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FHitResult Missing;
	TestFalse(TEXT("The collision-disabled control cannot provide the same capsule support."),World->SweepSingleByChannel(Missing,
		Contact+FVector(0,0,250),Contact-FVector(0,0,100),FQuat::Identity,ECC_Pawn,Capsule,Query));
	Fixture.ForwardErrorMessages(this);
	AddInfo(FString::Printf(TEXT("StructuralCandidatePhysics saved_units=%d faces=%d capsule_hits=%d max_contact_cm=%.17g walking_frames=%d falling_frames=%d max_player_height_error_cm=%.17g nav_path_points=%d nav_height_error_cm=%.17g removed_ramp_faces=%d input=%s; saved source/fallback geometry and native character/Nav only, not ALIS pawn/cooked package/streaming/render parity acceptance"),
		Input.Units.Num(),Centers.Num(),Hits,MaxContact,WalkingFrames,FallingFrames,MaximumWalkingHeightError,CompletePathPoints,CompleteNavHeightError,RemovedRampFaces,*Input.Hash));
	if (Input.Approach) AddInfo(FString::Printf(TEXT("ModeledApproach actual_native_grade=%.17g actual_native_cross=%.17g; contact compares the canonical MODEL, not provider raster or surveyed geography; auxiliary pads are not admitted Land"),
		Input.Slopes.MaximumGrade,Input.Slopes.MaximumCrossSlope));
	if (Input.Grouped) AddInfo(FString::Printf(TEXT("GroupedNeighborhoodPhysics routes=%d walking_segments=%d nav_segments=%d max_nav_error_cm=%.17g; original full source polylines, three capsule lanes, both directions; not ALIS pawn, WP streaming, rendered or packaged acceptance"),
		RouteMetrics.Routes,RouteMetrics.WalkingSegments,RouteMetrics.NavSegments,RouteMetrics.MaximumNavError));
	return !HasAnyErrors();
}
#endif
