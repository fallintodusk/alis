// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "Tests/ProjectWorldBasinFixture.h"
#include "Tests/ProjectWorldBasinNavigation.h"
#include "MeshPartitionCompiledSection.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinSceneTest
{
    namespace
    {
        class FReturn final : public IAutomationLatentCommand
        {
            FAutomationTestBase& Test;
            FProjectWorldCanonicalBundle Bundle;FVector Start,Far,WalkTarget;
            FName AdditionalTag;TArray<FVector> AdditionalPoints;int32 Additional=0,ExpectedAdditional=0;
            TArray<TArray<FVector>> Routes;TSharedRef<FJsonObject> Receipt;
            TWeakObjectPtr<ACharacter> Pawn;TWeakObjectPtr<AActor> Control;
            TWeakObjectPtr<UBoxComponent> Box;TWeakObjectPtr<UActorComponent> Invoker;
            TFunction<bool(const FVector&,double&)> Height;TFunction<bool(UWorld*,ACharacter*)> Measure;
            TArray<TWeakObjectPtr<UObject>> Owned;TSet<TWeakObjectPtr<UObject>> Shared;
            FIntVector Origin;FTransform Previous;FVector PreviousVelocity;
            FBox SpatialBounds{ForceInit};int32 LoadingRange=0;
            EMovementMode PreviousMode=MOVE_None;uint8 PreviousCustom=0;
            int32 Phase=0,Sections=0,Water=0,ExpectedSections=0,ExpectedWater=0,Frames=0,Leg=0;
            uint64 LastFrame=0;double PhaseStarted=0,SettledUntil=0;bool Begun=false,Collected=false,Restored=false,RestoreSucceeded=false;
            bool Restore()
            {
                if (Restored) return RestoreSucceeded;Restored=true;
                bool InvokerReleased=true,ControlReleased=true;
                if (auto* Component=Invoker.Get()) {Component->DestroyComponent();InvokerReleased=!Component->IsRegistered() &&
                    (Component->IsBeingDestroyed() || !Invoker.IsValid());}
                if (auto* Actor=Control.Get()) ControlReleased=Actor->Destroy() && (Actor->IsActorBeingDestroyed() || !Control.IsValid());
                Receipt->SetStringField(TEXT("streaming_invoker_cleanup"),InvokerReleased ? TEXT("released") : TEXT("failed"));
                Receipt->SetStringField(TEXT("streaming_calibration_cleanup"),ControlReleased ? TEXT("released") : TEXT("failed"));
                if (!InvokerReleased || !ControlReleased) Test.AddError(TEXT("Streaming cleanup retained an owned invoker or collision calibration."));
                if (!Begun) return true;
                if (!Pawn.IsValid()) {Receipt->SetStringField(TEXT("streaming_pawn_restore"),TEXT("failed_missing_pawn"));Test.AddError(TEXT("Streaming cleanup lost its captured pawn."));return false;}
                auto* World=Pawn->GetWorld();auto* Movement=Pawn->GetCharacterMovement();
                if (!World || World->OriginLocation!=Origin)
                {Receipt->SetStringField(TEXT("streaming_pawn_restore"),TEXT("failed_changed_frame"));Test.AddError(TEXT("Streaming cleanup cannot restore the captured pawn frame after an unexpected origin change."));return false;}
                Movement->StopMovementImmediately();
                const bool Moved=Pawn->GetActorTransform().Equals(Previous,0) || Pawn->SetActorTransform(Previous,false,nullptr,ETeleportType::TeleportPhysics);
                Movement->SetMovementMode(PreviousMode,PreviousCustom);Movement->Velocity=PreviousVelocity;
                RestoreSucceeded=InvokerReleased && ControlReleased && Moved && Pawn->GetActorTransform().Equals(Previous,0) && Movement->MovementMode==PreviousMode &&
                    Movement->CustomMovementMode==PreviousCustom && Movement->Velocity==PreviousVelocity;
                Receipt->SetStringField(TEXT("streaming_pawn_restore"),RestoreSucceeded ? TEXT("captured_transform_and_movement") : TEXT("failed_observed_state"));
                if (!RestoreSucceeded) Test.AddError(TEXT("Streaming cleanup did not restore the observed pawn transform and movement."));
                return RestoreSucceeded;
            }
            bool Fail(const FString& Message) {Receipt->SetStringField(TEXT("streaming_terminal_status"),TEXT("refused"));Test.AddError(Message);Restore();return true;}
            void Census(UWorld& World)
            {
                Sections=0;Water=0;Additional=0;
                for (TActorIterator<AActor> It(&World);It;++It)
                {Sections+=It->IsA<UE::MeshPartition::ACompiledSection>();Water+=It->ActorHasTag(TEXT("ProjectWorld.Water.v1"));
                    Additional+=!AdditionalTag.IsNone() && It->ActorHasTag(AdditionalTag);}
                Receipt->SetNumberField(TEXT("streaming_current_additional_actors"),Additional);
                Receipt->SetNumberField(TEXT("streaming_phase"),Phase);Receipt->SetNumberField(TEXT("streaming_current_sections"),Sections);
                Receipt->SetNumberField(TEXT("streaming_current_water_actors"),Water);
                TArray<TSharedPtr<FJsonValue>> Frame;
                for (int32 V : {World.OriginLocation.X,World.OriginLocation.Y,World.OriginLocation.Z}) Frame.Add(MakeShared<FJsonValueNumber>(V));
                Receipt->SetArrayField(FString::Printf(TEXT("streaming_phase_%d_origin"),Phase),Frame);
            }
            bool Calibration(UWorld& World)
            {
                if (!Control.IsValid() || !Box.IsValid() || Control->GetLevel()!=World.PersistentLevel ||
                    Control->GetIsSpatiallyLoaded() || Box->CanEverAffectNavigation()) return false;
                FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ProjectWorldBasinStreamingControl),true);Q.AddIgnoredActor(Pawn.Get());
                const FVector P=Control->GetActorLocation();
                const bool Ray=World.LineTraceSingleByChannel(Hit,P+FVector(0,0,100),P-FVector(0,0,100),ECC_Pawn,Q) && Hit.bBlockingHit && Hit.GetComponent()==Box.Get();
                const auto* Capsule=Pawn->GetCapsuleComponent();
                const bool Sweep=World.SweepSingleByChannel(Hit,P+FVector(0,0,250),P-FVector(0,0,100),FQuat::Identity,ECC_Pawn,
                    FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Q) && Hit.bBlockingHit && Hit.GetComponent()==Box.Get();
                Receipt->SetNumberField(FString::Printf(TEXT("streaming_phase_%d_control_ray_hits"),Phase),Ray ? 1 : 0);
                Receipt->SetNumberField(FString::Printf(TEXT("streaming_phase_%d_control_capsule_hits"),Phase),Sweep ? 1 : 0);
                return Ray && Sweep;
            }
            void Capture(UWorld& World)
            {
                for (TActorIterator<AActor> It(&World);It;++It)
                {
                    AActor* Actor=*It;
                    if (!Actor->IsA<UE::MeshPartition::ACompiledSection>() && !Actor->ActorHasTag(TEXT("ProjectWorld.Water.v1")) &&
                        (AdditionalTag.IsNone() || !Actor->ActorHasTag(AdditionalTag))) continue;
                    Owned.Add(Actor);TArray<UActorComponent*> Components;Actor->GetComponents(Components);
                    for (auto* C : Components)
                    {
                        Owned.Add(C);
                        if (auto* Mesh=Cast<UStaticMeshComponent>(C);Mesh && Mesh->GetStaticMesh())
                        {if (Mesh->GetStaticMesh()->IsIn(Actor) || Mesh->GetStaticMesh()->GetPackage()==Actor->GetPackage()) Owned.Add(Mesh->GetStaticMesh());else Shared.Add(Mesh->GetStaticMesh());}
                        if (auto* Primitive=Cast<UPrimitiveComponent>(C);Primitive && Primitive->GetBodySetup())
                        {if (Primitive->GetBodySetup()->IsIn(Actor) || Primitive->GetBodySetup()->GetPackage()==Actor->GetPackage()) Owned.Add(Primitive->GetBodySetup());else Shared.Add(Primitive->GetBodySetup());}
                    }
                }
                Receipt->SetNumberField(TEXT("streaming_owned_weak_census"),Owned.Num());
                Receipt->SetNumberField(TEXT("streaming_shared_asset_census"),Shared.Num());
            }
            bool AbsentCollision(UWorld& World)
            {
                int32 Count=0,Rays=0,Capsules=0;FCollisionQueryParams Q(SCENE_QUERY_STAT(ProjectWorldBasinStreamingAbsent),true);Q.AddIgnoredActor(Pawn.Get());
                const auto* C=Pawn->GetCapsuleComponent();
                for (const auto& Cell : Bundle.Cells) for (const auto& Quad : Cell.Terrain.RefinedQuads) for (const auto& Face : Quad.Triangles)
                {
                    const FVector P=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,(Quad.Vertices[Face.X]+Quad.Vertices[Face.Y]+Quad.Vertices[Face.Z])/3);
                    FHitResult Hit;++Count;
                    Rays+=World.LineTraceSingleByChannel(Hit,P+FVector(0,0,100),P-FVector(0,0,100),ECC_Pawn,Q) && Hit.bBlockingHit;
                    Capsules+=World.SweepSingleByChannel(Hit,P+FVector(0,0,250),P-FVector(0,0,100),FQuat::Identity,ECC_Pawn,
                        FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()),Q) && Hit.bBlockingHit;
                }
                for (const auto& P : AdditionalPoints)
                {
                    FHitResult Hit;++Count;
                    Rays+=World.LineTraceSingleByChannel(Hit,P+FVector(0,0,100),P-FVector(0,0,100),ECC_Pawn,Q) && Hit.bBlockingHit;
                    Capsules+=World.SweepSingleByChannel(Hit,P+FVector(0,0,250),P-FVector(0,0,100),FQuat::Identity,ECC_Pawn,
                        FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()),Q) && Hit.bBlockingHit;
                }
                Receipt->SetNumberField(TEXT("streaming_absent_face_probes"),Count);Receipt->SetNumberField(TEXT("streaming_absent_ray_hits"),Rays);
                Receipt->SetNumberField(TEXT("streaming_absent_capsule_hits"),Capsules);
                return Count>0 && Count==int32(Receipt->GetNumberField(TEXT("pie_faces")))+AdditionalPoints.Num() && Rays==0 && Capsules==0;
            }
        public:
            FReturn(FAutomationTestBase& InTest,const FInput& Input,const TArray<TArray<FVector>>& InRoutes,TSharedRef<FJsonObject> InReceipt,
                ACharacter& Character,TFunction<bool(const FVector&,double&)> InHeight,TFunction<bool(UWorld*,ACharacter*)> InMeasure)
                :Test(InTest),Bundle(Input.Bundle),Start(Input.Start),WalkTarget(Input.ReturnWalkTargetWorld.Get(Input.Start+FVector(200,0,0))),
                AdditionalTag(Input.AdditionalSpatialTag),AdditionalPoints(Input.AdditionalSupportWorldPoints),
                ExpectedAdditional(Input.AdditionalSpatialTag.IsNone() ? 0 : int32(InReceipt->GetNumberField(TEXT("pie_upper")))),
                Routes(InRoutes),Receipt(InReceipt),Pawn(&Character),Height(MoveTemp(InHeight)),Measure(MoveTemp(InMeasure)),
                ExpectedWater(Input.Expected->HasField(TEXT("water")) ? Input.Expected->GetObjectField(TEXT("water"))->HasField(TEXT("actor_count")) ?
                    int32(Input.Expected->GetObjectField(TEXT("water"))->GetNumberField(TEXT("actor_count"))) : 1 : 0) {}
            ~FReturn() override {Restore();}
            bool Update() override
            {
                if (Test.HasAnyErrors()) {Restore();return true;}
                ACharacter* Character=Pawn.Get();UWorld* World=Character ? Character->GetWorld() : nullptr;
                if (!World || World->WorldType!=EWorldType::PIE || !Character->IsLocallyControlled() ||
                    !Character->GetController() || Character->GetController()->GetPawn()!=Character)
                    return Fail(TEXT("Streaming requires the same actually possessed walked Hero in native PIE."));
                auto* Partition=World->GetWorldPartition();auto* Movement=Character->GetCharacterMovement();FString Error;
                if (!Partition || !Partition->IsStreamingEnabled()) return Fail(TEXT("Streaming requires enabled native World Partition."));
                if (!Begun)
                {
                    Origin=World->OriginLocation;Previous=Character->GetActorTransform();PreviousMode=Movement->MovementMode;
                    PreviousCustom=Movement->CustomMovementMode;PreviousVelocity=Movement->Velocity;Begun=true;
                    ExpectedSections=Receipt->GetNumberField(TEXT("pie_sections"));
                    if (ExpectedSections<=0 || Routes.IsEmpty()) return Fail(TEXT("Streaming lacks positive authenticated source/route census."));
                    int32 Range=0;if (!ProjectWorldBasinFixture::ReadStreamingRange(*World,Range,Error)) return Fail(Error);LoadingRange=Range;
                    FBox Cells(ForceInit);
                    if (!Partition->RuntimeHash) return Fail(TEXT("Streaming has no actual runtime hash."));
                    Partition->RuntimeHash->ForEachStreamingCells([&](const UWorldPartitionRuntimeCell* Cell)
                    {if (Cell && !Cell->IsAlwaysLoaded()) {Cells+=Cell->GetCellBounds();Cells+=Cell->GetStreamingBounds();}return true;});
                    if (!Cells.IsValid || Cells.Min.ContainsNaN() || Cells.Max.ContainsNaN()) return Fail(TEXT("Streaming cannot bound the actual spatial cells."));
                    SpatialBounds=Cells;
                    Far=FVector(Cells.Max.X+4.*Range+1000,Cells.GetCenter().Y,Start.Z+2000);
                    Receipt->SetNumberField(TEXT("streaming_actual_loading_range_cm"),Range);
                    Receipt->SetNumberField(TEXT("streaming_far_distance_from_cells_cm"),Far.X-Cells.Max.X);
                    if (Far.ContainsNaN() || Far.X-Cells.Max.X<=Range) return Fail(TEXT("The far source is not beyond the actual native cell/range domain."));
                    FActorSpawnParameters P;P.OverrideLevel=World->PersistentLevel;P.ObjectFlags|=RF_Transient;
                    Control=World->SpawnActor<AActor>(AActor::StaticClass(),Start-FVector(0,0,10000),FRotator::ZeroRotator,P);
                    if (!Control.IsValid()) return Fail(TEXT("Streaming cannot create its independent persistent collision calibration."));
                    Control->SetIsSpatiallyLoaded(false);Control->SetHLODLayer(nullptr);Control->bEnableAutoLODGeneration=false;
                    auto* Shape=NewObject<UBoxComponent>(Control.Get());Control->AddInstanceComponent(Shape);Control->SetRootComponent(Shape);
                    Shape->SetBoxExtent(FVector(100,100,10));Shape->SetMobility(EComponentMobility::Movable);Shape->SetCanEverAffectNavigation(false);
                    Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
                    Shape->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);Shape->SetHiddenInGame(true);Shape->RegisterComponent();Box=Shape;
                    Control->SetActorLocation(Start-FVector(0,0,10000));
                    UActorComponent* OwnedInvoker=nullptr;
                    const bool Prepared=ProjectWorldBasinNavigation::Prepare(*Character,Routes,OwnedInvoker,Error);Invoker=OwnedInvoker;
                    if (!Prepared) return Fail(Error);
                    Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Flying);PhaseStarted=FPlatformTime::Seconds();
                }
                if (LastFrame==GFrameCounter) return false;LastFrame=GFrameCounter;
                Census(*World);
                if (World->OriginLocation!=Origin) return Fail(TEXT("Native streaming unexpectedly rebased the canonical query frame."));
                if (FPlatformTime::Seconds()-PhaseStarted>90) return Fail(FString::Printf(TEXT("Streaming phase %d exceeded its budget; sections=%d water=%d."),Phase,Sections,Water));
                const auto& Sources=Partition->GetStreamingSources();bool Following=false;
                for (const auto& Source : Sources) Following|=FVector2D(Source.Location-Character->GetActorLocation()).Length()<1000;
                Receipt->SetNumberField(TEXT("streaming_runtime_sources"),Sources.Num());
                if (!Following || Sources.IsEmpty() || !Partition->IsStreamingCompleted(&Sources)) return false;
                if (!Calibration(*World)) return Fail(TEXT("The independent correct-component collision calibration is unavailable."));
                if (Phase==0)
                {
                    if (Sections!=ExpectedSections || Water!=ExpectedWater || Additional!=ExpectedAdditional) return false;
                    Capture(*World);Receipt->SetNumberField(TEXT("streaming_initial_sections"),Sections);Receipt->SetNumberField(TEXT("streaming_initial_water"),Water);
                    Movement->StopMovementImmediately();
                    if (!Character->SetActorLocation(Far,false,nullptr,ETeleportType::TeleportPhysics) || Character->GetActorLocation().ContainsNaN() ||
                        Character->GetActorLocation()!=Far || Character->GetActorLocation().X-SpatialBounds.Max.X<=LoadingRange)
                        return Fail(TEXT("The actual possessed pawn did not reach the bounded far streaming domain."));
                    Receipt->SetNumberField(TEXT("streaming_actual_far_distance_from_cells_cm"),Character->GetActorLocation().X-SpatialBounds.Max.X);
                    Phase=1;PhaseStarted=FPlatformTime::Seconds();return false;
                }
                if (Phase==1)
                {
                    if (Sections!=0 || Water!=0 || Additional!=0) return false;
                    if (!Collected) {CollectGarbage(RF_NoFlags);Collected=true;return false;}
                    int32 Alive=0;for (const auto& Object : Owned) Alive+=Object.IsValid();
                    Receipt->SetNumberField(TEXT("streaming_owned_weak_alive_after_unload"),Alive);
                    int32 SharedAlive=0;for (const auto& Asset : Shared) SharedAlive+=Asset.IsValid();
                    Receipt->SetNumberField(TEXT("streaming_shared_assets_alive_after_unload"),SharedAlive);
                    if (Alive) return false;
                    if (!AbsentCollision(*World)) return Fail(TEXT("Source collision did not disappear despite zero native subject actors."));
                    Receipt->SetNumberField(TEXT("streaming_unloaded_sections"),Sections);Receipt->SetNumberField(TEXT("streaming_unloaded_water"),Water);
                    Receipt->SetNumberField(TEXT("streaming_unloaded_additional_actors"),Additional);
                    Movement->StopMovementImmediately();const FVector Return=Start+FVector(0,0,150);
                    const FVector BeforeReturn=Character->GetActorLocation();
                    if (Character->GetRootComponent()!=Character->GetCapsuleComponent() || Character->GetRootComponent()->GetAttachParent() ||
                        BeforeReturn.ContainsNaN() || Return.ContainsNaN())
                        return Fail(TEXT("Streaming return requires its finite unattached native capsule movement frame."));
                    // Native unswept Actor movement subtracts then adds its delta.
                    // Compare that exact representable target, retaining the requested anchor separately.
                    const FVector ExpectedReturn=BeforeReturn+(Return-BeforeReturn);
                    const bool ReturnMoved=Character->SetActorLocation(Return,false,nullptr,ETeleportType::TeleportPhysics);
                    auto ReturnObservation=MakeShared<FJsonObject>();
                    for (const auto& Value : {TPair<FString,FVector>(TEXT("before"),BeforeReturn),
                        TPair<FString,FVector>(TEXT("requested"),Return),TPair<FString,FVector>(TEXT("observed"),Character->GetActorLocation()),
                        TPair<FString,FVector>(TEXT("native_expected"),ExpectedReturn),
                        TPair<FString,FVector>(TEXT("delta"),Character->GetActorLocation()-Return)})
                    {
                        TArray<TSharedPtr<FJsonValue>> Coordinates;
                        for (double V : {Value.Value.X,Value.Value.Y,Value.Value.Z}) Coordinates.Add(MakeShared<FJsonValueNumber>(V));
                        ReturnObservation->SetArrayField(Value.Key,Coordinates);
                    }
                    ReturnObservation->SetBoolField(TEXT("native_move_result"),ReturnMoved);
                    Receipt->SetObjectField(TEXT("streaming_return_relocation"),ReturnObservation);
                    if (!ReturnMoved ||
                        ExpectedReturn.ContainsNaN() || Character->GetActorLocation().ContainsNaN() || Character->GetActorLocation()!=ExpectedReturn)
                        return Fail(TEXT("The actual possessed pawn did not return to the exact native representation of the saved-scene anchor."));
                    Phase=2;PhaseStarted=FPlatformTime::Seconds();return false;
                }
                if (Sections!=ExpectedSections || Water!=ExpectedWater || Additional!=ExpectedAdditional) return false;
                if (Phase==2) {Movement->SetMovementMode(MOVE_Falling);SettledUntil=World->GetTimeSeconds()+1;Phase=3;return false;}
                if (World->GetTimeSeconds()<SettledUntil) return false;
                if (!Movement->IsMovingOnGround()) return Fail(TEXT("The same Hero did not regain actual ground contact after streaming return."));
                double Z=0;if (!Height(Character->GetActorLocation(),Z) || !FMath::IsFinite(Z)) return Fail(TEXT("Returned walking has unknown canonical support."));
                const double Foot=Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Z;
                if (!FMath::IsFinite(Foot) || Foot<-.01 || Foot>Receipt->GetNumberField(TEXT("capsule_slope_floor_envelope_cm")))
                    return Fail(TEXT("Returned walking exceeds the actual capsule/slope floor envelope."));
                ++Frames;Receipt->SetNumberField(TEXT("streaming_return_grounded_frames"),Frames);
                FVector Delta=(Leg==0 ? WalkTarget : Start)-Character->GetActorLocation();Delta.Z=0;
                if (Delta.Length()>=15) {Character->AddMovementInput(Delta.GetSafeNormal(),1,true);return false;}
                Movement->StopMovementImmediately();if (++Leg<2) return false;
                if (!Measure(World,Character)) {Restore();return true;}
                Receipt->SetNumberField(TEXT("streaming_return_movement_segments"),Leg);
                Receipt->SetStringField(TEXT("streaming_scope"),TEXT("Native saved-scene spatial actors/collision unload and return, PIE-owned weak invalidation, shared assets excluded, provided post-return measurement callback and two short native CMC legs. No GPU-memory, packaged or regional streaming claim."));
                if (Restore() && !Test.HasAnyErrors()) Receipt->SetStringField(TEXT("streaming_terminal_status"),TEXT("completed"));
                return true;
            }
        };
    }
    TSharedRef<IAutomationLatentCommand> StreamingReturn(FAutomationTestBase& Test,const FInput& Input,
        const TArray<TArray<FVector>>& Routes,TSharedRef<FJsonObject> Receipt,ACharacter& Pawn,
        TFunction<bool(const FVector&,double&)> Height,TFunction<bool(UWorld*,ACharacter*)> Measure)
    {return MakeShared<FReturn>(Test,Input,Routes,Receipt,Pawn,MoveTemp(Height),MoveTemp(Measure));}
}
#endif
