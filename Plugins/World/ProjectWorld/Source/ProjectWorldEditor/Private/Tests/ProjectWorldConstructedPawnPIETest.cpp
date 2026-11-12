// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedRuntimeInput.h"
#include "ProjectWorldConstructedRuntimeMesh.h"
#include "Tests/ProjectWorldConstructedRuntimePawn.h"
#include "Tests/ProjectWorldBasinNavigation.h"
#include "ProjectWorldConstructedRuntimeRoutes.h"
#include "ProjectWorldConstructedRuntimeStreaming.h"
#include "ProjectWorldRuntimeNavigation.h"
#include "Components/BrushComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "NavigationSystem.h"
#include "NavigationInvokerComponent.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedPawnPIE
{
	using namespace ProjectWorldConstructedRuntimeTest;
	struct FState
	{
		explicit FState(FAutomationTestBase& InTest):Test(InTest) {}
		FAutomationTestBase& Test;
		FInput Input;
		FPawnSelection Selection;
		bool Neighborhood=false,NavigationOnly=false;
		TUniquePtr<FSavedCandidate> Saved;
		TUniquePtr<FStreamingScene> Streaming;
		TWeakObjectPtr<UWorld> EditorWorld;
		bool PreviousFixed=false,ClockOwned=false;
		double PreviousDelta=0;
		void Cleanup()
		{
			if (EditorWorld.IsValid() && !Test.TestTrue(TEXT("Native PIE ends before fixture package teardown."),GEditor && !GEditor->PlayWorld)) return;
			if (ClockOwned) { FApp::SetFixedDeltaTime(PreviousDelta); FApp::SetUseFixedTimeStep(PreviousFixed); ClockOwned=false; }
			if (EditorWorld.IsValid())
			{
				if (!Test.TestTrue(TEXT("Cleanup retains ownership of the disposable Editor scene."),GEditor->GetEditorWorldContext().World()==EditorWorld.Get())) return;
				if (Streaming) Streaming->ReleaseLoader();
				FAutomationEditorCommonUtils::CreateNewMap(); EditorWorld.Reset(); CollectGarbage(RF_NoFlags);
			}
			if (Streaming) Streaming->UnloadPackages(Test);
			Saved.Reset();
			Streaming.Reset();
		}
		~FState()
		{
			if (EditorWorld.IsValid() && GEditor && GEditor->PlayWorld) GEditor->EndPlayMap();
			Cleanup();
		}
	};
	FName UnitTag(int32 Index) { return FName(*FString::Printf(TEXT("ProjectWorld.PawnScene.Unit.%d"),Index)); }
	bool MeasureScene(FState& State,UWorld* World,ACharacter* Pawn)
	{
		TArray<UStaticMeshComponent*> Components; TArray<FVector> Centers;
		for (int32 I=0; I<State.Input.Units.Num(); ++I)
		{
			const auto& Unit=State.Input.Units[I]; TArray<AStaticMeshActor*> Found;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It) if (It->ActorHasTag(UnitTag(I))) Found.Add(*It);
			if (!State.Test.TestEqual(TEXT("PIE duplicates each physical unit exactly once."),Found.Num(),1)) return false;
			auto* Actor=Found[0]; auto* Component=Actor->GetStaticMeshComponent(); UStaticMesh* Mesh=Component->GetStaticMesh();
			if (!State.Test.TestTrue(TEXT("PIE consumes the exact saved unit asset at unchanged origin/rotation/scale."),Mesh &&
				Mesh->GetPathName()==State.Saved->PackageNames[I]+TEXT(".Surface") && Actor->GetActorLocation()==Unit.Origin &&
				Actor->GetActorQuat()==FQuat::Identity && Actor->GetActorScale3D()==FVector::OneVector &&
				Component->GetComponentLocation()==Unit.Origin && Component->GetComponentQuat()==FQuat::Identity && Component->GetComponentScale()==FVector::OneVector)) return false;
			Components.Add(Component);
			for (const auto& Face : Unit.Triangles) Centers.Add((Face.Points[0]+Face.Points[1]+Face.Points[2])/3+Unit.Origin);
		}
		const double Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const auto Capsule=FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldPawnPIE),true); Query.AddIgnoredActor(Pawn);
		int32 Hits=0,Unknown=0; double MaximumError=0;
		for (const auto& Center : Centers)
		{
			FHitResult Hit;
			if (World->SweepSingleByChannel(Hit,Center+FVector(0,0,250),Center-FVector(0,0,100),FQuat::Identity,ECC_Pawn,Capsule,Query) &&
				Hit.bBlockingHit && Components.Contains(Cast<UStaticMeshComponent>(Hit.GetComponent())))
			{
				++Hits; double Height;
				if (!ExpectedHeight(State.Input,Center,Height)) ++Unknown;
				else MaximumError=FMath::Max(MaximumError,FMath::Abs(Hit.Location.Z-Half-Height));
			}
		}
		State.Test.TestEqual(TEXT("Every actual PIE face supports the actual project capsule."),Hits,Centers.Num());
		State.Test.TestEqual(TEXT("Every PIE capsule contact retains known model support."),Unknown,0);
		State.Test.TestTrue(TEXT("Actual PIE contacts remain within the existing 3 cm source bound."),MaximumError<=3);
		State.Test.AddInfo(FString::Printf(TEXT("ProjectPawnPIEContacts units=%d faces=%d hits=%d unknown=%d maximum_contact_error_cm=%.17g input=%s"),
			Components.Num(),Centers.Num(),Hits,Unknown,MaximumError,*State.Input.Hash));
		return !State.Test.HasAnyErrors();
	}
	class FStart final : public FStartPIEForAutomationCommand
	{
	public:
		explicit FStart(const FRequestPlaySessionParams& Request):FStartPIEForAutomationCommand(Request),Settings(Request.EditorPlaySettings.Get()) {}
	private:
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
	};
	class FCleanup final : public IAutomationLatentCommand
	{
	public:
		explicit FCleanup(TSharedRef<FState> InState):State(InState) {}
		bool Update() override
		{
			if (GEditor && GEditor->PlayWorld)
			{
				if (RequestedAt==0) RequestedAt=FPlatformTime::Seconds();
				if (FPlatformTime::Seconds()-RequestedAt>30)
				{ State->Test.AddError(TEXT("Native PIE shutdown exceeded the cleanup budget.")); GEditor->EndPlayMap(); }
				return false;
			}
			State->Cleanup(); return true;
		}
	private:
		TSharedRef<FState> State;
		double RequestedAt=0;
	};
	class FWalk final : public IAutomationLatentCommand
	{
	public:
		explicit FWalk(TSharedRef<FState> InState):State(InState) {}
		bool Update() override
		{
			auto& Test=State->Test;
			if (Test.HasAnyErrors()) return true;
			UWorld* World=GEditor ? GEditor->PlayWorld.Get() : nullptr;
			if (!Test.TestNotNull(TEXT("The pawn lifecycle probe runs in the actual native PIE world."),World)) return true;
			if (!Character.IsValid())
			{
				ACharacter* Pawn=nullptr; APlayerController* Controller=nullptr;
				if (!SpawnPawn(World,State->Input.Start,State->Selection,Test,Pawn,Controller)) return true;
				if (!MeasureScene(*State,World,Pawn)) return true;
				if (State->NavigationOnly)
				{
					TArray<TArray<FVector>> Routes; for (const auto& Route : State->Input.Routes) Routes.Add(Route.Points);
					UActorComponent* Invoker=nullptr; FString Error; auto Receipt=MakeShared<FJsonObject>();
					if (!ProjectWorldBasinNavigation::Prepare(*Pawn,Routes,Invoker,Error) ||
						!ProjectWorldBasinNavigation::CheckAgent(*World,*Pawn,*Receipt,Error)) { Test.AddError(Error); return true; }
					Character=Pawn; Started=World->GetTimeSeconds(); RealStarted=FPlatformTime::Seconds(); SimulationBudget=60;
					return false;
				}
				if (State->Neighborhood)
				{
					auto* Invoker=NewObject<UNavigationInvokerComponent>(Pawn); Pawn->AddInstanceComponent(Invoker);
					FBox Domain(ForceInit); for (const auto& Route : State->Input.Routes) for (const auto& P : Route.Points) Domain+=P;
					const double Radius=Domain.GetSize().Length()+1000;
					Invoker->SetGenerationRadii(Radius,Radius+1000); Invoker->RegisterComponent();
				}
				FString Error;
				FInput WalkingInput=State->Input;
				if (!State->Neighborhood) WalkingInput.Routes={{TEXT("connector_return"),2,{State->Input.Start,State->Input.End}}};
				if (!BuildWalkingLanes(WalkingInput,Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Lanes,Error)) { Test.AddError(Error); return true; }
				if (!State->Neighborhood) Lanes.SetNum(1);
				for (const auto& Lane : Lanes) for (int32 I=1; I<Lane.Points.Num(); ++I)
				{ TotalDistance+=FVector2D(Lane.Points[I]-Lane.Points[I-1]).Length()*2; ExpectedSegments+=2; }
				const double Speed=Pawn->GetCharacterMovement()->MaxWalkSpeed;
				if (!Test.TestTrue(TEXT("The actual project movement speed is finite and positive."),FMath::IsFinite(Speed) && Speed>0)) return true;
				SimulationBudget=TotalDistance/Speed*2+Lanes.Num()+60;
				Character=Pawn; State->PreviousFixed=FApp::UseFixedTimeStep(); State->PreviousDelta=FApp::GetFixedDeltaTime();
				State->ClockOwned=true; FApp::SetFixedDeltaTime(1./60); FApp::SetUseFixedTimeStep(true);
				Started=World->GetTimeSeconds(); RealStarted=FPlatformTime::Seconds(); LastFrame=GFrameCounter;
				return false;
			}
			if (LastFrame==GFrameCounter) return false;
			LastFrame=GFrameCounter; auto* Pawn=Character.Get(); auto* Movement=Pawn->GetCharacterMovement();
			const double Now=World->GetTimeSeconds();
			if (!Test.TestTrue(TEXT("The real-frame probe stays within distance-derived simulation/wall budgets."),
				Now-Started<SimulationBudget && FPlatformTime::Seconds()-RealStarted<SimulationBudget*3+120)) return true;
			if (State->NavigationOnly)
			{
				auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
				if (Now-Started<1 || !Navigation || Navigation->GetInvokerLocations().IsEmpty()) return false;
				if (!Test.TestTrue(TEXT("Nav-only qualification retains the normally spawned grounded Hero."),Movement->IsMovingOnGround())) return true;
				TArray<TArray<FVector>> Routes; for (const auto& Route : State->Input.Routes) Routes.Add(Route.Points);
				TArray<UPrimitiveComponent*> Support;
				for (int32 I=0; I<State->Input.Units.Num(); ++I) for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
					if (It->ActorHasTag(UnitTag(I))) Support.Add(It->GetStaticMeshComponent());
				auto Receipt=MakeShared<FJsonObject>(); FString Error,Encoded;
				Receipt->SetStringField(TEXT("input_sha256"),State->Input.Hash);
				Receipt->SetStringField(TEXT("scope"),TEXT("Saved grouped neighborhood, actual Hero and native corridor detail only; prior walking evidence is separate. No city/profile/Apply/streaming/render/package acceptance."));
				TArray<TStrongObjectPtr<UStaticMesh>> OriginalMeshes;
				for (auto* Component : Support) OriginalMeshes.Emplace(CastChecked<UStaticMeshComponent>(Component)->GetStaticMesh());
				auto* Lower=World->SpawnActor<AStaticMeshActor>();
				auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
				if (!Test.TestTrue(TEXT("The Nav control has real lower Ground separate from the saved decks."),Lower && Cube)) return true;
				ON_SCOPE_EXIT
				{
					for (int32 I=0;I<Support.Num();++I)
					{auto* Component=CastChecked<UStaticMeshComponent>(Support[I]);Component->SetStaticMesh(OriginalMeshes[I].Get());Component->RecreatePhysicsState();}
					Lower->Destroy();
				};
				auto* LowerComponent=Lower->GetStaticMeshComponent();LowerComponent->SetMobility(EComponentMobility::Movable);
				FVector LowerCenter=(State->Input.Start+State->Input.End)*.5;LowerCenter.Z=-State->Input.Frame.HeightOriginMeters*100-5;
				Lower->SetActorLocation(LowerCenter);Lower->SetActorScale3D(FVector(100,100,.1));
				LowerComponent->SetStaticMesh(Cube);
				LowerComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);LowerComponent->SetCollisionResponseToAllChannels(ECR_Block);
				LowerComponent->SetCanEverAffectNavigation(true);LowerComponent->RecreatePhysicsState();
				FHitResult LowerHit;FCollisionQueryParams LowerQuery(SCENE_QUERY_STAT(ProjectWorldLowerNavControl),true);LowerQuery.AddIgnoredActor(Pawn);
				if (!Test.TestTrue(TEXT("The separate lower Ground is a real blocking surface at canonical zero."),
					Lower->GetActorLocation()==LowerCenter && Lower->GetActorScale3D()==FVector(100,100,.1) &&
					World->LineTraceSingleByChannel(LowerHit,LowerCenter+FVector(0,0,6),LowerCenter-FVector(0,0,20),ECC_Pawn,LowerQuery) &&
					LowerHit.bBlockingHit && LowerHit.GetComponent()==LowerComponent &&
					FMath::Abs(LowerHit.ImpactPoint.Z+State->Input.Frame.HeightOriginMeters*100)<.001)) return true;
				Receipt->SetNumberField(TEXT("navigation_lower_control_observed_height_cm"),LowerHit.ImpactPoint.Z);
				auto ConnectorControl=[&](bool Remove,FString& Refusal)
				{
					int32 Removed=0;
					for (int32 I=0;I<State->Input.Units.Num();++I)
					{
						auto* Component=CastChecked<UStaticMeshComponent>(Support[I]);
						if (!Remove) {Component->SetStaticMesh(OriginalMeshes[I].Get());Component->RecreatePhysicsState();continue;}
						if (State->Input.UnitGroups[I]!=State->Input.ConnectorGroup) continue;
						auto Broken=State->Input.Units[I];Broken.Triangles.Reset();int32 UnitRemoved=0;
						for (int32 J=0;J<State->Input.Units[I].Triangles.Num();++J)
							if (State->Input.ConnectorSources.Contains(State->Input.UnitSources[I][J])) ++UnitRemoved;
							else Broken.Triangles.Add(State->Input.Units[I].Triangles[J]);
						if (!UnitRemoved) continue;
						auto* Mesh=Broken.Triangles.IsEmpty() ? nullptr : Compile(Broken,Test);
						if (!Broken.Triangles.IsEmpty() && !Mesh) {Refusal=TEXT("Cannot compile the exact connector-removal control.");return false;}
						Component->SetStaticMesh(Mesh);Component->RecreatePhysicsState();Removed+=UnitRemoved;
					}
					if (Remove) {Receipt->SetNumberField(TEXT("navigation_removed_connector_faces"),Removed);
						if (!Removed) {Refusal=TEXT("The physical connector control removed no declared source faces.");return false;}}
					return true;
				};
				bool Valid=ProjectWorldBasinNavigation::Verify(*World,*Pawn,State->Input.Frame,Routes,Support,
					[&](const FVector& P,double& Z){return ExpectedHeight(State->Input,P,Z);},*Receipt,Error,ConnectorControl);
				if (Valid && !MeasureScene(*State,World,Pawn)) {Valid=false;Error=TEXT("Restored connector no longer matches the saved physical source.");}
				Receipt->SetBoolField(TEXT("accepted"),Valid); Receipt->SetStringField(TEXT("refusal"),Error);
				if (!FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded)))
					Test.AddError(TEXT("Cannot serialize the actual neighborhood Nav evidence."));
				else Test.AddInfo(TEXT("NeighborhoodNavReceipt ")+Encoded);
				if (!Valid) Test.AddError(Error);
				return true;
			}
			if (LanePending)
			{
				Movement->StopMovementImmediately(); Pawn->SetActorLocation(Lanes[LaneIndex].Points[0]+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);
				Movement->SetMovementMode(MOVE_Falling); SettledUntil=Now+1; LanePending=false; Walking=false; Direction=0; Next=1; return false;
			}
			if (Now<SettledUntil) return false;
			if (!Walking)
			{
				if (!Test.TestTrue(TEXT("The normally spawned project pawn settles on the saved fixture surface."),Movement->IsMovingOnGround())) return true;
				Walking=true; SegmentStarted=Now; SegmentBudget=FVector2D(Lanes[LaneIndex].Points[Next]-Pawn->GetActorLocation()).Length()/Movement->MaxWalkSpeed+10;
			}
			Frames+=Movement->IsMovingOnGround(); Falling+=Movement->IsFalling(); OtherModes+=!Movement->IsMovingOnGround(); double Height;
			if (!ExpectedHeight(State->Input,Pawn->GetActorLocation(),Height)) ++Unknown;
			else MaximumFootError=FMath::Max(MaximumFootError,FMath::Abs(Pawn->GetActorLocation().Z-Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Height));
			FVector Delta=Lanes[LaneIndex].Points[Next]-Pawn->GetActorLocation(); Delta.Z=0;
			if (Delta.Length()<15)
			{
				Movement->StopMovementImmediately();
				++DoneSegments; Next+=Direction==0 ? 1 : -1;
				if (Direction==0 && Next>=Lanes[LaneIndex].Points.Num()) { Direction=1; Next=Lanes[LaneIndex].Points.Num()-2; }
				else if (Direction==1 && Next<0)
				{
					++LaneIndex;
					Test.AddInfo(FString::Printf(TEXT("ProjectPawnPIELane completed=%d expected=%d traversed_segments=%d walking_frames=%d"),LaneIndex,Lanes.Num(),DoneSegments,Frames));
					if (LaneIndex<Lanes.Num()) { LanePending=true; return false; }
					Test.TestEqual(TEXT("Real-frame project-pawn walking has zero falling frames."),Falling,0);
					Test.TestEqual(TEXT("Every measured walking frame uses grounded CharacterMovement."),OtherModes,0);
					Test.TestEqual(TEXT("Real-frame project-pawn walking never invents missing support."),Unknown,0);
					Test.TestTrue(TEXT("The actual project capsule foot stays within the existing 3 cm source bound."),MaximumFootError<=3);
					Test.TestEqual(TEXT("Every original source segment is walked both ways in every selected lane."),DoneSegments,ExpectedSegments);
					FRouteMetrics Nav;
					if (State->Neighborhood)
					{
						auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
						if (!Test.TestTrue(TEXT("Normal PIE ticks populated the registered project-pawn invoker before Nav qualification."),
							Navigation && !Navigation->GetInvokerLocations().IsEmpty())) return true;
						auto* Bounds=World->SpawnActor<ANavMeshBoundsVolume>(); ANavigationData* Data=nullptr;
						if (!Test.TestNotNull(TEXT("Actual project-pawn Nav uses a native route domain."),Bounds)) return true;
						Bounds->GetBrushComponent()->SetMobility(EComponentMobility::Movable);
						if (!NavRoutes(State->Input,World,Pawn,Bounds,Data,Test,Nav)) return true;
					}
					Test.AddInfo(FString::Printf(TEXT("ProjectPawnPIEPhysics lanes=%d segment_traversals=%d walking_frames=%d falling_frames=%d other_movement_frames=%d unknown_frames=%d maximum_foot_error_cm=%.17g nav_segments=%d maximum_nav_error_cm=%.17g fixed_delta_s=%.17g input=%s; actual engine frames/local player/UI; not menu/WP/render/package acceptance"),
						Lanes.Num(),DoneSegments,Frames,Falling,OtherModes,Unknown,MaximumFootError,Nav.NavSegments,Nav.MaximumNavError,FApp::GetFixedDeltaTime(),*State->Input.Hash)); return true;
				}
				SegmentStarted=Now; SegmentBudget=FVector2D(Lanes[LaneIndex].Points[Next]-Pawn->GetActorLocation()).Length()/Movement->MaxWalkSpeed+10; return false;
			}
			if (Now-SegmentStarted>=SegmentBudget)
			{ Test.AddError(FString::Printf(TEXT("Real-frame source segment stalled route=%s lane=%.17g direction=%d next=%d position=%s target=%s"),
				*Lanes[LaneIndex].RouteId,Lanes[LaneIndex].OffsetCentimeters,Direction,Next,*Pawn->GetActorLocation().ToString(),*Lanes[LaneIndex].Points[Next].ToString())); return true; }
			Pawn->AddMovementInput(Delta.GetSafeNormal(),1); return false;
		}
	private:
		TSharedRef<FState> State;
		TWeakObjectPtr<ACharacter> Character;
		uint64 LastFrame=0;
		TArray<FWalkLane> Lanes;
		double Started=0,RealStarted=0,SettledUntil=0,SegmentStarted=0,SegmentBudget=0,MaximumFootError=0,SimulationBudget=0,TotalDistance=0;
		int32 Frames=0,Falling=0,OtherModes=0,Unknown=0,Direction=0,LaneIndex=0,Next=1,ExpectedSegments=0,DoneSegments=0;
		bool Walking=false,LanePending=true;
	};
}

static bool RunPawnScene(FAutomationTestBase& Test,bool Neighborhood,bool Streaming=false,bool NavigationOnly=false)
{
	using namespace ProjectWorldConstructedPawnPIE;
	const FString Path=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_INPUT"));
	if (!Test.TestTrue(TEXT("The project-pawn probe requires fresh native PIE, explicit pawn selection, correctly scoped input and the documented direct-map frontend bypass."),
		GEditor && !GEditor->PlayWorld && ConfiguredPawnRequested() && FParse::Param(FCommandLine::Get(),TEXT("ProjectSkipFrontEnd")) &&
		(Streaming || (Path.IsEmpty()!=Neighborhood)))) return false;
	const auto State=MakeShared<FState>(Test); State->Neighborhood=Neighborhood; State->NavigationOnly=NavigationOnly; FString Error;
	if (!Load(State->Input,Error,Path,FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_STRUCTURE_SHA256")))) { Test.AddError(Error); return false; }
	if (!Test.TestTrue(TEXT("A neighborhood probe consumes the full explicit grouped scene and source-route census."),!Neighborhood || State->Input.Grouped)) return false;
	auto* World=Streaming ? GEditor->NewMap(true) : FAutomationEditorCommonUtils::CreateNewMap(); State->EditorWorld=World;
	if (!Test.TestNotNull(TEXT("The lifecycle probe owns a fresh unsaved Editor map."),World) || !ConfigurePawn(World,State->Input.Start,Test,State->Selection)) return false;
	auto* NavigationConfig=NewObject<UNavigationSystemModuleConfig>(World->GetWorldSettings());
	NavigationConfig->NavigationSystemClass=FSoftClassPath(UNavigationSystemV1::StaticClass());
	World->GetWorldSettings()->SetNavigationSystemConfigOverride(NavigationConfig);
	State->Saved=MakeUnique<FSavedCandidate>(Streaming);
	int32 UnitIndex=0;
	for (const auto& Unit : State->Input.Units)
	{
		auto* Mesh=State->Saved->RoundTrip(Unit,Test); if (!Mesh) return false;
		auto* Actor=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),Unit.Origin,FRotator::ZeroRotator);
		if (!Test.TestNotNull(TEXT("Saved fixture geometry is present before native PIE duplication/spawn."),Actor)) return false;
		Actor->Tags.Add(UnitTag(UnitIndex++));
		auto* Component=Actor->GetStaticMeshComponent(); Component->SetMobility(EComponentMobility::Movable); Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->SetCanEverAffectNavigation(true); Component->RecreatePhysicsState();
	}
	if (Streaming)
	{
		State->Streaming=MakeUnique<FStreamingScene>();
		const bool Prepared=State->Streaming->SaveAndReload(World,State->Input,*State->Saved,Test); State->EditorWorld=World;
		if (!Prepared) return false;
	}
	auto* Settings=NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
	Settings->SetPlayNetMode(PIE_Standalone); Settings->SetPlayNumberOfClients(1); Settings->SetRunUnderOneProcess(true);
	Settings->bLaunchSeparateServer=false; Settings->NewWindowWidth=1280; Settings->NewWindowHeight=720;
	FRequestPlaySessionParams Request; Request.EditorPlaySettings=Settings; Request.bAllowOnlineSubsystem=false;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStart>(Request));
	if (Streaming) FAutomationTestFramework::Get().EnqueueLatentCommand(StreamingReturn(Test,State->Input,State->Selection,*State->Streaming,
		[State](UWorld* PlayWorld,ACharacter* Pawn) { return MeasureScene(*State,PlayWorld,Pawn); }));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWalk>(State));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndPlayMapCommand>());
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCleanup>(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedPawnPIETest,
	"Project.World.Realization.Constructed.ProjectPawnLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectWorldConstructedPawnPIETest::RunTest(const FString&) { return RunPawnScene(*this,false); }
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldConstructedPawnPIETest,
	"Project.World.Realization.Constructed.ProjectPawnLifecycle", "[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedNeighborhoodPIETest,
	"Project.World.Realization.Constructed.NeighborhoodPawnPhysics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectWorldConstructedNeighborhoodPIETest::RunTest(const FString&) { return RunPawnScene(*this,true); }
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldConstructedNeighborhoodPIETest,
	"Project.World.Realization.Constructed.NeighborhoodPawnPhysics", "[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedNeighborhoodNavigationTest,
	"Project.World.Realization.Constructed.NeighborhoodPawnNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectWorldConstructedNeighborhoodNavigationTest::RunTest(const FString&) { return RunPawnScene(*this,true,false,true); }
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldConstructedNeighborhoodNavigationTest,
	"Project.World.Realization.Constructed.NeighborhoodPawnNavigation", "[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedStreamingReturnTest,
	"Project.World.Realization.Constructed.StreamingReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectWorldConstructedStreamingReturnTest::RunTest(const FString&) { return RunPawnScene(*this,false,true); }
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldConstructedStreamingReturnTest,
	"Project.World.Realization.Constructed.StreamingReturn", "[Slow][Integration][World]")
#endif
