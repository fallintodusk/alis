// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainCoastalFixture.h"
#include "Algo/Reverse.h"
#include "ProjectWorldMeshTerrainCollisionFixture.h"
#include "ProjectWorldMeshTerrainSectionCollection.h"
#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
#include "Tests/ProjectWorldCoastalView.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionCollisionComponent.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionEditorUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "PackageTools.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "StaticMeshResources.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldCoastalSceneTest
{
    namespace
    {
        using namespace UE::MeshPartition;
        struct FState
        {
            FAutomationTestBase& Test;ProjectWorldBasinSceneTest::FInput Input;ProjectWorldCoastalFixture::FUpper Upper;
            TSharedPtr<FJsonObject> Context,Receipt=MakeShared<FJsonObject>();TWeakObjectPtr<UWorld> EditorWorld;
            TWeakObjectPtr<ACharacter> Pawn;TArray<FWorldPartitionReference> References;
            TArray<FProjectWorldSurfaceTriangle> WorldField;TArray<FName> FieldRoles;TMap<FIntPoint,TArray<int32>> Index;
            TArray<TArray<FVector>> Routes;TArray<FVector> RequiredPoints;
            bool Fixed=false,PreviousFixed=false,Cleaned=false;double PreviousDelta=0;
            FState(FAutomationTestBase& T,ProjectWorldBasinSceneTest::FInput&& I):Test(T),Input(MoveTemp(I)) {}
            void AddFace(const FProjectWorldSurfaceTriangle& Face,FName Role=TEXT("Ground"))
            {
                const int32 Id=WorldField.Add(Face);FieldRoles.Add(Role);FBox B(ForceInit);for (const auto& P : Face.Points) B+=P;
                for (int32 X=FMath::FloorToInt(B.Min.X/3000);X<=FMath::FloorToInt(B.Max.X/3000);++X)
                    for (int32 Y=FMath::FloorToInt(B.Min.Y/3000);Y<=FMath::FloorToInt(B.Max.Y/3000);++Y) Index.FindOrAdd(FIntPoint(X,Y)).Add(Id);
            }
            bool Height(const FVector& P,double& Z,int32* OutFace=nullptr) const
            {
                const auto* Candidates=Index.Find(FIntPoint(FMath::FloorToInt(P.X/3000),FMath::FloorToInt(P.Y/3000)));
                bool Found=false;Z=-DBL_MAX;if (!Candidates) return false;
                for (int32 Id : *Candidates)
                {
                    const auto& F=WorldField[Id];const auto A=F.Points[0];const FVector2D U(F.Points[1]-A),V(F.Points[2]-A),D(P-A);
                    const double Den=U.X*V.Y-U.Y*V.X;if (Den==0) continue;
                    const double S=(D.X*V.Y-D.Y*V.X)/Den,T=(U.X*D.Y-U.Y*D.X)/Den;
                    if (S>=0 && T>=0 && S+T<=1)
                    {Found=true;const double H=A.Z+S*(F.Points[1].Z-A.Z)+T*(F.Points[2].Z-A.Z);
                        if (H>Z) {Z=H;if (OutFace) *OutFace=Id;}}
                }
                return Found && FMath::IsFinite(Z);
            }
            bool ClearPixelEdges(const FVector& P,double Half,const FProjectWorldCanonicalFeature& Marine) const
            {
                const FVector2D Min(P.X-Half,P.Y-Half),Max(P.X+Half,P.Y+Half);
                const auto Touches=[&](const FVector2D& A,const FVector2D& B)
                {
                    double Lo=0,Hi=1;
                    for (int32 K=0;K<2;++K)
                    {
                        const double D=B[K]-A[K];
                        if (D==0) {if (A[K]<Min[K] || A[K]>Max[K]) return false;continue;}
                        double U=(Min[K]-A[K])/D,V=(Max[K]-A[K])/D;if (U>V) Swap(U,V);
                        Lo=FMath::Max(Lo,U);Hi=FMath::Min(Hi,V);if (Lo>Hi) return false;
                    }
                    return true;
                };
                TSet<int32> Faces;
                for (int32 X=FMath::FloorToInt(Min.X/3000);X<=FMath::FloorToInt(Max.X/3000);++X)
                    for (int32 Y=FMath::FloorToInt(Min.Y/3000);Y<=FMath::FloorToInt(Max.Y/3000);++Y)
                        if (const auto* Values=Index.Find(FIntPoint(X,Y))) Faces.Append(*Values);
                for (int32 Id : Faces) for (int32 K=0;K<3;++K)
                    if (Touches(FVector2D(WorldField[Id].Points[K]),FVector2D(WorldField[Id].Points[(K+1)%3]))) return false;
                const auto Ring=[&](const TArray<FVector2D>& Points)
                {
                    for (int32 K=0;K<Points.Num();++K)
                    {
                        const FVector A=FProjectWorldCanonicalLoader::CanonicalToUnreal(Input.Bundle,FVector(Points[K],0));
                        const FVector B=FProjectWorldCanonicalLoader::CanonicalToUnreal(Input.Bundle,FVector(Points[(K+1)%Points.Num()],0));
                        if (Touches(FVector2D(A),FVector2D(B))) return false;
                    }
                    return true;
                };
                for (const auto& Polygon : Marine.GeometryPolygons)
                {if (!Ring(Polygon.Outer)) return false;for (const auto& H : Polygon.Holes) if (!Ring(H)) return false;}
                return true;
            }
            void Cleanup()
            {
                if (Cleaned) return;Cleaned=true;
                Receipt->SetStringField(TEXT("cleanup"),TEXT("retained_unknown"));
                ON_SCOPE_EXIT
                {
                    const bool ViewsComplete=Receipt->HasField(TEXT("coastal_view_near_status")) && Receipt->HasField(TEXT("coastal_view_far_status"));
                    Receipt->SetStringField(TEXT("execution_result"),!Test.HasAnyErrors() && (Input.StreamingOnly || ViewsComplete) &&
                        Receipt->HasField(TEXT("streaming_terminal_status")) &&
                        Receipt->GetStringField(TEXT("streaming_terminal_status"))==TEXT("completed") && Receipt->GetStringField(TEXT("cleanup"))==
                        TEXT("scene_released_plugin_retained_for_process_exit") ? Input.StreamingOnly ?
                        TEXT("completed_streaming_diagnostic_only") : TEXT("completed") : TEXT("failed_or_incomplete"));
                    FString Text;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Text));
                    Test.TestTrue(TEXT("Coastal evidence survives success or refusal."),FFileHelper::SaveStringToFile(Text,*(Input.Root/TEXT("gameplay.json"))));
                };
                if (Fixed) {FApp::SetFixedDeltaTime(PreviousDelta);FApp::SetUseFixedTimeStep(PreviousFixed);Fixed=false;}
                if (GEditor && GEditor->PlayWorld) {Receipt->SetStringField(TEXT("cleanup"),TEXT("retained_active_pie"));Test.AddError(TEXT("Coastal cleanup retains an active PIE lease."));return;}
                FString Hash;Test.TestTrue(TEXT("Coastal reload and PIE preserve the saved map bytes."),
                    FProjectWorldCanonicalLoader::ComputeFileSha256(FPackageName::LongPackageNameToFilename(Input.PackageName,FPackageName::GetMapPackageExtension()),Hash) && Hash==Input.MapSha256);
                Receipt->SetStringField(TEXT("native_map_sha256_before"),Input.MapSha256);
                Receipt->SetStringField(TEXT("native_map_sha256_after"),Hash);
                References.Reset();
                if (EditorWorld.IsValid())
                {if (!Test.TestTrue(TEXT("Coastal cleanup owns the actual scratch map."),GEditor->GetEditorWorldContext().World()==EditorWorld.Get()))
                    {Receipt->SetStringField(TEXT("cleanup"),TEXT("retained_ownership_mismatch"));return;}
                    FAutomationEditorCommonUtils::CreateNewMap();EditorWorld.Reset();CollectGarbage(RF_NoFlags);}
                TArray<UPackage*> Packages;for (const auto& Name : Input.Packages) if (auto* P=FindPackage(nullptr,*Name)) Packages.Add(P);
                if (!Packages.IsEmpty()) {UPackageTools::FUnloadPackageParams Params(Packages);Params.bResetTransBuffer=false;
                    if (!Test.TestTrue(TEXT("Coastal saved packages release after native PIE."),UPackageTools::UnloadPackages(Params)))
                    {Receipt->SetStringField(TEXT("cleanup"),TEXT("retained_package_unload_failure"));return;}}
                Receipt->SetStringField(TEXT("cleanup"),TEXT("scene_released_plugin_retained_for_process_exit"));
            }
            ~FState() {Cleanup();}
        };
        bool Measure(FState& S,UWorld& World,ACharacter* Pawn,const FString& Prefix)
        {
            const bool PIE=World.WorldType==EWorldType::PIE;int32 UpperCount=0,SectionCount=0;TSet<int32> Seen;
            TArray<TPair<UPrimitiveComponent*,ECollisionEnabled::Type>> Disabled;
            ON_SCOPE_EXIT {for (const auto& Pair : Disabled) Pair.Key->SetCollisionEnabled(Pair.Value);};
            if (Pawn) {Disabled.Add({Pawn->GetCapsuleComponent(),Pawn->GetCapsuleComponent()->GetCollisionEnabled()});Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
            for (TActorIterator<AStaticMeshActor> It(&World);It;++It)
            {
                auto* C=It->GetStaticMeshComponent();UStaticMesh* Mesh=C ? C->GetStaticMesh() : nullptr;
                if (It->ActorHasTag(UpperTag))
                {
                    ++UpperCount;int32 Unit=INDEX_NONE;
                    for (int32 I=0;I<S.Upper.Units.Num();++I) if (It->ActorHasTag(FName(*FString::Printf(TEXT("ProjectWorld.Test.UpperUnit=%d"),I)))) Unit=I;
                    if (!S.Test.TestTrue(TEXT("Reloaded Upper is a unique exact source unit with the same frame."),Mesh && Unit>=0 && !Seen.Contains(Unit) &&
                        It->GetActorLocation()==S.Upper.Units[Unit].Origin && C->GetComponentTransform().Equals(FTransform(S.Upper.Units[Unit].Origin),0))) return false;
                    Seen.Add(Unit);const auto Row=S.Input.Expected->GetArrayField(TEXT("coastal_upper"))[Unit]->AsObject();
                    const bool Owned=Mesh->GetPathName()==Row->GetStringField(TEXT("mesh")) &&
                        ProjectWorldBasinSceneTest::OwnsSavedActor(*It,&World,S.Input,Row->GetStringField(TEXT("actor_package")));
                    if (!S.Test.TestTrue(TEXT("Upper actor and mesh retain the exact saved package projection."),Owned) ||
                        !ProjectWorldCoastalFixture::SameUpper(S.Upper.Units[Unit],*Mesh,S.Test)) return false;
                    Disabled.Add({C,C->GetCollisionEnabled()});C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
            }
            auto Water=MakeShared<FJsonObject>();
            if (!S.Test.TestEqual(TEXT("All five unchanged upper units load."),UpperCount,5) || !CheckWater(S.Test,World,S.Input,*Water)) return false;
            S.Receipt->SetObjectField(Prefix+TEXT("water_binding"),Water);
            TMap<FName,TArray<ACompiledSection*>> Variants;
            TSet<FName> ExpectedVariants;TSet<FString> SourcePackages;
            for (TActorIterator<ACompiledSection> It(&World);It;++It)
            {
                ++SectionCount;const auto& Info=It->GetBuildInfo();
                if (!S.Test.TestTrue(TEXT("Current coastal sections retain their private definition and real native build identity."),!It->IsPlaceholder() && Info.BuildKey.IsValid() &&
                    Info.ModifierSetHash.IsValid() && Info.BuildVariantHash.IsValid() && Info.GetMegaMeshDefinition() &&
                    Info.GetMegaMeshDefinition()->GetPathName()==S.Input.Expected->GetStringField(TEXT("expected_definition_object_path")))) return false;
                if (ExpectedVariants.IsEmpty())
                {
                    if (PIE) {FName Preview;auto* Platform=UE::MeshPartition::EditorUtils::GetPreviewPlatform(Preview);
                        ExpectedVariants.Append(Info.GetMegaMeshDefinition()->GetCompiledSectionBuildVariantNamesForCurrentEditorPreview(Preview,Platform));}
                    else for (const auto& V : Info.GetMegaMeshDefinition()->GetCompiledSectionBuildVariants()) ExpectedVariants.Add(V.Name);
                }
                TSharedPtr<FJsonObject> Frozen;
                for (const auto& V : S.Input.Expected->GetArrayField(TEXT("variants"))) if (V->AsObject()->GetStringField(TEXT("variant"))==Info.BuildVariantName.ToString())
                    for (const auto& Entry : V->AsObject()->GetArrayField(TEXT("native_sections")))
                    {const auto E=Entry->AsObject();const auto& C=E->GetArrayField(TEXT("grid_cell"));
                        if (C[0]->AsNumber()==Info.GridCellCoord.X && C[1]->AsNumber()==Info.GridCellCoord.Y && C[2]->AsNumber()==Info.GridCellCoord.Z) Frozen=E;}
                if (!S.Test.TestTrue(TEXT("Current section matches its exact saved native identity and package."),Frozen &&
                    ExpectedVariants.Contains(Info.BuildVariantName) && !SourcePackages.Contains(Frozen->GetStringField(TEXT("actor_package"))) &&
                    Info.BuildKey.ToString(EGuidFormats::Digits)==Frozen->GetStringField(TEXT("build_key")) &&
                    Info.ModifierSetHash.ToString(EGuidFormats::Digits)==Frozen->GetStringField(TEXT("modifier_hash")) &&
                    Info.BuildVariantHash.ToString(EGuidFormats::Digits)==Frozen->GetStringField(TEXT("variant_hash")) &&
                    ProjectWorldBasinSceneTest::OwnsSavedActor(*It,&World,S.Input,Frozen->GetStringField(TEXT("actor_package"))))) return false;
                SourcePackages.Add(Frozen->GetStringField(TEXT("actor_package")));const auto M=It->GetActorTransform().ToMatrixWithScale();
                const auto& Matrix=Frozen->GetArrayField(TEXT("actor_transform"));
                for (int32 I=0;I<4;++I) for (int32 J=0;J<4;++J)
                    if (!S.Test.TestTrue(TEXT("Saved native section frame remains exact."),M.M[I][J]==Matrix[I]->AsArray()[J]->AsNumber())) return false;
                Variants.FindOrAdd(Info.BuildVariantName).Add(*It);
            }
            if (!S.Test.TestEqual(TEXT("Every actual platform-selected variant reloads."),Variants.Num(),ExpectedVariants.Num())) return false;
            for (const auto& Pair : Variants)
            {
                int32 Expected=0;for (const auto& V : S.Input.Expected->GetArrayField(TEXT("variants")))
                    if (V->AsObject()->GetStringField(TEXT("variant"))==Pair.Key.ToString()) Expected=V->AsObject()->GetArrayField(TEXT("native_sections")).Num();
                if (!S.Test.TestEqual(TEXT("Every saved grid part of the active variant reloads."),Pair.Value.Num(),Expected)) return false;
                ProjectWorldTerrainSectionCollection::FCollected Observed;FProjectWorldMeshTerrainSurfaceFidelity Fidelity;FString Error;
                if (!ProjectWorldTerrainSectionCollection::Collect(S.Test,World,S.Input.Bundle,Pair.Value,Pair.Key,true,true,Observed,S.RequiredPoints) ||
                    !ProjectWorldMeshTerrainProducer::CompareRefinedSurface(S.Input.Bundle,Observed.Actual,1e-4,Fidelity,Error)) {S.Test.AddError(Error);return false;}
            }
            if (!S.Test.TestTrue(TEXT("Saved coastal native variants have positive current geometry."),SectionCount>0 && Variants.Num()>0)) return false;
            S.Receipt->SetNumberField(Prefix+TEXT("sections"),SectionCount);S.Receipt->SetNumberField(Prefix+TEXT("upper"),UpperCount);
            if (PIE) {int32 Faces=0;for (const auto& C : S.Input.Bundle.Cells) for (const auto& Q : C.Terrain.RefinedQuads) Faces+=Q.Triangles.Num();S.Receipt->SetNumberField(TEXT("pie_faces"),Faces);}
            return !S.Test.HasAnyErrors();
        }
        class FStart final : public FStartPIEForAutomationCommand
        {TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
        public:explicit FStart(const FRequestPlaySessionParams& R):FStartPIEForAutomationCommand(R),Settings(R.EditorPlaySettings.Get()) {}};
        class FWalk final : public IAutomationLatentCommand
        {
            TSharedRef<FState> S;int32 Lane=0,Point=1,Frames=0,Segments=0;bool Pending=true;double Settled=0,Started=0,LegStarted=0;uint64 Frame=0;
        public:
            explicit FWalk(TSharedRef<FState> In):S(In) {}
            bool Update() override
            {
                if (S->Test.HasAnyErrors()) return true;UWorld* World=GEditor ? GEditor->PlayWorld.Get() : nullptr;
                if (!World) {S->Test.AddError(TEXT("Coastal native PIE is missing."));return true;}
                if (!S->Pawn.IsValid())
                {
                    ACharacter* Pawn=nullptr;APlayerController* Controller=nullptr;
                    if (!ProjectWorldConstructedRuntimeTest::SpawnPawn(World,S->Input.Start,S->Input.Pawn,S->Test,Pawn,Controller)) return true;
                    S->Pawn=Pawn;if (!Measure(*S,*World,Pawn,TEXT("pie_"))) return true;
                    auto* M=Pawn->GetCharacterMovement();S->Receipt->SetNumberField(TEXT("actual_Hero_walkable_floor_Z"),M->GetWalkableFloorZ());
                    S->Receipt->SetNumberField(TEXT("actual_Hero_capsule_radius_cm"),Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius());
                    S->Receipt->SetNumberField(TEXT("capsule_slope_floor_envelope_cm"),3.0);
                    S->PreviousFixed=FApp::UseFixedTimeStep();S->PreviousDelta=FApp::GetFixedDeltaTime();S->Fixed=true;
                    FApp::SetFixedDeltaTime(1./60);FApp::SetUseFixedTimeStep(true);Started=FPlatformTime::Seconds();
                }
                if (Frame==GFrameCounter) return false;Frame=GFrameCounter;auto* Pawn=S->Pawn.Get();auto* M=Pawn->GetCharacterMovement();const double Now=World->GetTimeSeconds();
                if (FPlatformTime::Seconds()-Started>360) {S->Test.AddError(TEXT("Short coastal walking exceeded its native budget."));return true;}
                if (Pending)
                {M->StopMovementImmediately();Pawn->SetActorLocation(S->Routes[Lane][0]+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);
                    M->SetMovementMode(MOVE_Falling);Settled=Now+1;LegStarted=Now+1;Pending=false;return false;}
                if (Now<Settled) return false;
                double Z=0;int32 ExpectedFace=INDEX_NONE;const bool Known=S->Height(Pawn->GetActorLocation(),Z,&ExpectedFace);
                auto Diagnostic=MakeShared<FJsonObject>();const auto P=Pawn->GetActorLocation();auto* Capsule=Pawn->GetCapsuleComponent();
                Diagnostic->SetNumberField(TEXT("lane"),Lane);Diagnostic->SetNumberField(TEXT("target_point"),Point);Diagnostic->SetNumberField(TEXT("walked_frames"),Frames);
                Diagnostic->SetStringField(TEXT("pawn_world_cm"),P.ToString());Diagnostic->SetStringField(TEXT("capsule_world_cm"),Capsule->GetComponentLocation().ToString());
                Diagnostic->SetStringField(TEXT("target_world_cm"),S->Routes[Lane][Point].ToString());Diagnostic->SetBoolField(TEXT("expected_support_known"),Known);
                Diagnostic->SetNumberField(TEXT("capsule_radius_cm"),Capsule->GetScaledCapsuleRadius());Diagnostic->SetNumberField(TEXT("capsule_halfheight_cm"),Capsule->GetScaledCapsuleHalfHeight());
                Diagnostic->SetStringField(TEXT("CMC_floor_actor"),GetPathNameSafe(M->CurrentFloor.HitResult.GetActor()));
                Diagnostic->SetStringField(TEXT("CMC_floor_component"),GetPathNameSafe(M->CurrentFloor.HitResult.GetComponent()));
                Diagnostic->SetStringField(TEXT("CMC_floor_impact_world_cm"),M->CurrentFloor.HitResult.ImpactPoint.ToString());
                Diagnostic->SetStringField(TEXT("CMC_floor_normal"),M->CurrentFloor.HitResult.ImpactNormal.ToString());Diagnostic->SetNumberField(TEXT("CMC_floor_distance_cm"),M->CurrentFloor.FloorDist);
                if (Known) {Diagnostic->SetNumberField(TEXT("expected_highest_z_cm"),Z);Diagnostic->SetNumberField(TEXT("expected_face"),ExpectedFace);Diagnostic->SetStringField(TEXT("expected_role"),S->FieldRoles[ExpectedFace].ToString());}
                S->Receipt->SetObjectField(TEXT("last_walking_witness"),Diagnostic);
                if (!Known || !M->IsMovingOnGround())
                {S->Test.AddError(TEXT("Actual Hero lost declared highest physical support on a changed coastal connection."));return true;}
                const double Foot=Pawn->GetActorLocation().Z-Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Z;
                Diagnostic->SetNumberField(TEXT("foot_offset_cm"),Foot);
                if (!S->Test.TestTrue(TEXT("Actual Hero feet stay within the retained3cm dry walking envelope."),Foot>=-.01 && Foot<=3)) return true;
                FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ProjectWorldCoastalTop),true);Q.AddIgnoredActor(Pawn);
                if (!S->Test.TestTrue(TEXT("Unfiltered normal walking query hits declared Terrain or Upper at the independently expected height."),
                    World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Z+400),FVector(P.X,P.Y,Z-400),ECC_Pawn,Q) && Hit.bBlockingHit &&
                    Hit.GetActor() && (Hit.GetActor()->IsA<ACompiledSection>() || Hit.GetActor()->ActorHasTag(UpperTag)) && FMath::Abs(Hit.ImpactPoint.Z-Z)<=.01)) return true;
                if (S->Input.StreamingOnly)
                {S->Receipt->SetNumberField(TEXT("streaming_preparation_grounded_frames"),1);return true;}
                ++Frames;FVector Delta=S->Routes[Lane][Point]-P;Delta.Z=0;
                if (Delta.Size()<15)
                {
                    ++Segments;M->StopMovementImmediately();LegStarted=Now;
                    if (++Point==S->Routes[Lane].Num()) {Point=1;Pending=true;if (++Lane==S->Routes.Num())
                        {S->Receipt->SetNumberField(TEXT("walking_frames"),Frames);S->Receipt->SetNumberField(TEXT("walked_segments"),Segments);return true;}}
                }
                else {if (Now-LegStarted>30) {S->Test.AddError(TEXT("Actual Hero could not traverse a changed coastal segment."));return true;}Pawn->AddMovementInput(Delta.GetSafeNormal(),1,true);}
                return false;
            }
        };
        bool Inside(const FProjectWorldCanonicalFeature& Feature,const FVector2D& P)
        {
            const auto Ring=[&](const TArray<FVector2D>& Points)
            {bool In=false;for (int32 I=0,J=Points.Num()-1;I<Points.Num();J=I++)
                if ((Points[I].Y>P.Y)!=(Points[J].Y>P.Y) && P.X<(Points[J].X-Points[I].X)*(P.Y-Points[I].Y)/(Points[J].Y-Points[I].Y)+Points[I].X) In=!In;return In;};
            for (const auto& Polygon : Feature.GeometryPolygons)
            {if (!Ring(Polygon.Outer)) continue;bool Hole=false;for (const auto& H : Polygon.Holes) Hole|=Ring(H);if (!Hole) return true;}return false;
        }
        class FView final : public IAutomationLatentCommand
        {
            TSharedRef<FState> S;FProjectWorldCoastalView View;bool Begun=false;int32 ViewId=0;TSharedPtr<FJsonObject> Current;
        public:
            explicit FView(TSharedRef<FState> In):S(In) {}
            bool Update() override
            {
                if (S->Test.HasAnyErrors()) return true;auto* World=GEditor ? GEditor->PlayWorld.Get() : nullptr;FString Error;
                if (!World || !S->Pawn.IsValid()) {S->Test.AddError(TEXT("Coastal rendering lost its walked native scene."));return true;}
                if (!Begun)
                {
                    const auto& B=S->Context->GetArrayField(TEXT("render_bounds"));
                    FVector Camera=FProjectWorldCanonicalLoader::CanonicalToUnreal(S->Input.Bundle,FVector((B[0]->AsNumber()+B[2]->AsNumber())/2,
                        (B[1]->AsNumber()+B[3]->AsNumber())/2,40));
                    Camera.Z+=ViewId*4000;
                    const double Width=double(float(FMath::Max((B[2]->AsNumber()-B[0]->AsNumber())*100,(B[3]->AsNumber()-B[1]->AsNumber())*100*1920./1080.)*1.1*(ViewId ? 1.8 : 1.)));
                    const double Pitch=Width/1920.;TArray<int32> Visible,Occluded;const auto& Marine=S->Input.Bundle.Features.FindChecked(TEXT("alis:osm:relation:2389611"));
                    for (int32 Y=40;Y<1040;Y+=23) for (int32 X=40;X<1880;X+=23)
                    {
                        const FVector Center(Camera.X+(540-Y-.5)*Pitch,Camera.Y+(X+.5-960)*Pitch,0);
                        const FVector Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(S->Input.Bundle,Center);
                        const auto& Domain=S->Input.Bundle.Cells[0].Bounds;const double Margin=2.5*Pitch/100.;
                        // Water is the original polygon clipped by its declared cell.
                        // Retained Upper can extend beyond that Water input domain.
                        if (Canonical.X-Margin<Domain.X || Canonical.X+Margin>Domain.Z ||
                            Canonical.Y-Margin<Domain.Y || Canonical.Y+Margin>Domain.W) continue;
                        if (!S->ClearPixelEdges(Center,2.5*Pitch,Marine)) continue;
                        if (!S->Test.TestTrue(TEXT("Source witness pixel center roundtrips through the declared orthographic projection."),
                            FMath::Abs(540-(Center.X-Camera.X)/Pitch-.5-Y)<1e-8 && FMath::Abs((Center.Y-Camera.Y)/Pitch+960-.5-X)<1e-8)) return true;
                        int32 Role=0;bool Stable=true;
                        for (int32 DY : {-2,0,2}) for (int32 DX : {-2,0,2})
                        {
                            const FVector P(Camera.X+(540-Y-DY-.5)*Pitch,Camera.Y+(X+DX+.5-960)*Pitch,0);
                            const FVector C=FProjectWorldCanonicalLoader::UnrealToCanonical(S->Input.Bundle,P);double Z=0;
                            if (!Inside(Marine,FVector2D(C)) || !S->Height(P,Z)) {Stable=false;continue;}
                            const double Delta=(.25-S->Input.Bundle.HeightOriginMeters)*100-Z;const int32 Next=Delta>5 ? 1 : Delta< -5 ? 2 : 0;
                            if (!Next || (Role && Next!=Role)) Stable=false;Role=Next;
                        }
                        if (Stable && Role==1 && Visible.Num()<32) Visible.Add(Y*1920+X);
                        if (Stable && Role==2 && Occluded.Num()<32) Occluded.Add(Y*1920+X);
                    }
                    TArray<UStaticMeshComponent*> Terrain;
                    for (TActorIterator<ACompiledSection> It(World);It;++It) for (const auto& C : It->GetMeshComponents())
                        if (C->GetStaticMesh() && C->GetStaticMesh()->HasValidNaniteData()) Terrain.Add(C.Get());
                    for (TActorIterator<AStaticMeshActor> It(World);It;++It) if (It->ActorHasTag(UpperTag)) Terrain.Add(It->GetStaticMeshComponent());
                    auto Recipe=MakeShared<FJsonObject>();Recipe->SetNumberField(TEXT("orthographic_width_cm"),Width);Recipe->SetNumberField(TEXT("camera_x_cm"),Camera.X);
                    Recipe->SetNumberField(TEXT("camera_y_cm"),Camera.Y);Recipe->SetNumberField(TEXT("camera_z_cm"),Camera.Z);
                    Recipe->SetNumberField(TEXT("declared_visible_pixels"),Visible.Num());Recipe->SetNumberField(TEXT("declared_occluded_pixels"),Occluded.Num());
                    Recipe->SetStringField(TEXT("classification"),TEXT("Source-bound sampled rendered view, not whole-raster coastline or source-accuracy admission;5cm selects unambiguous witness strata only."));
                    Current=MakeShared<FJsonObject>();Current->SetObjectField(TEXT("recipe"),Recipe);S->Receipt->SetObjectField(ViewId ? TEXT("coastal_view_far") : TEXT("coastal_view_near"),Current);
                    if (!View.Begin(*World,Camera,Width,Visible,Occluded,Terrain,S->Input.Root/TEXT("renders")/(ViewId ? TEXT("far") : TEXT("near")),Error)) {S->Test.AddError(Error);return true;}Begun=true;
                }
                const bool Done=View.Step(*Current,Error);if (!Error.IsEmpty()) {S->Test.AddError(Error);return true;}
                if (!Done) return false;S->Receipt->SetStringField(ViewId ? TEXT("coastal_view_far_status") : TEXT("coastal_view_near_status"),TEXT("passed"));
                Begun=false;return ++ViewId==2;
            }
        };
        class FStream final : public IAutomationLatentCommand
        {
            TSharedRef<FState> S;TSharedPtr<IAutomationLatentCommand> Command;
        public:explicit FStream(TSharedRef<FState> In):S(In) {}
            bool Update() override
            {
                if (S->Test.HasAnyErrors()) return true;if (!S->Pawn.IsValid()) {S->Test.AddError(TEXT("Coastal streaming lost the actual Hero."));return true;}
                if (!Command) Command=ProjectWorldBasinSceneTest::StreamingReturn(S->Test,S->Input,S->Routes,S->Receipt.ToSharedRef(),*S->Pawn.Get(),
                    [S=S](const FVector& P,double& Z){return S->Height(P,Z);},
                    [S=S](UWorld* W,ACharacter* P){return Measure(*S,*W,P,TEXT("streaming_return_"));});
                return Command->Update();
            }
        };
        class FEnd final : public IAutomationLatentCommand
        {
            TSharedRef<FState> S;double Started=0;
        public:explicit FEnd(TSharedRef<FState> In):S(In) {}
            bool Update() override
            {
                if (GEditor && GEditor->PlayWorld)
                {if (!Started) Started=FPlatformTime::Seconds();if (FPlatformTime::Seconds()-Started<30) return false;
                    S->Test.AddError(TEXT("Coastal PIE shutdown exceeded its budget."));GEditor->EndPlayMap();}
                S->Cleanup();return true;
            }
        };
    }
    bool Queue(FAutomationTestBase& Test,ProjectWorldBasinSceneTest::FInput&& Input,const FJsonObject& Document)
    {
        auto S=MakeShared<FState>(Test,MoveTemp(Input));S->Context=Document.GetObjectField(TEXT("coastal_context"));FString Error;
        if (!ProjectWorldCoastalFixture::LoadUpper(S->Context->GetStringField(TEXT("runtime_fixture")),S->Context->GetStringField(TEXT("runtime_fixture_sha256")),S->Input.Bundle,S->Upper,Error) ||
            !ProjectWorldTerrainCollisionFixture::ReadRequiredPoints(Document,S->RequiredPoints,Error)) {Test.AddError(Error);return false;}
        for (const auto& Cell : S->Input.Bundle.Cells) for (const auto& Q : Cell.Terrain.RefinedQuads) for (const auto& Row : Q.Triangles)
        {FProjectWorldSurfaceTriangle F;for (int32 K=0;K<3;++K) F.Points[K]=FProjectWorldCanonicalLoader::CanonicalToUnreal(S->Input.Bundle,Q.Vertices[Row[K]]);S->AddFace(F);}
        S->Input.AdditionalSpatialTag=UpperTag;
        for (const auto& Unit : S->Upper.Units) for (const auto& T : Unit.Triangles)
        {auto F=T;for (auto& P : F.Points) P+=Unit.Origin;S->AddFace(F,UpperTag);S->Input.AdditionalSupportWorldPoints.Add((F.Points[0]+F.Points[1]+F.Points[2])/3);}
        for (const auto& Value : S->Context->GetArrayField(TEXT("short_routes")))
        {
            const auto& Axis=Value->AsArray();
            for (double Offset : {-.6,0.,.6}) for (bool Reverse : {false,true})
            {
                TArray<FVector> Route;
                for (int32 I=0;I<Axis.Num();++I)
                {
                    const auto& P=Axis[I]->AsArray();const auto& A=Axis[I==0 ? I : I-1]->AsArray();const auto& B=Axis[I==Axis.Num()-1 ? I : I+1]->AsArray();
                    const FVector2D D(B[0]->AsNumber()-A[0]->AsNumber(),B[1]->AsNumber()-A[1]->AsNumber());
                    const auto N=FVector2D(-D.Y,D.X).GetSafeNormal();FVector W=FProjectWorldCanonicalLoader::CanonicalToUnreal(S->Input.Bundle,
                        FVector(P[0]->AsNumber()+N.X*Offset,P[1]->AsNumber()+N.Y*Offset,0));
                    if (!S->Height(W,W.Z)) {Test.AddError(TEXT("A short coastal route has unknown source support."));return false;}Route.Add(W);
                }
                if (Reverse) Algo::Reverse(Route);S->Routes.Add(MoveTemp(Route));
            }
        }
        S->Input.Start=S->Routes[0][0];
        FVector ReturnDirection=S->Routes[0][1]-S->Input.Start;ReturnDirection.Z=0;
        S->Input.ReturnWalkTargetWorld=S->Input.Start+ReturnDirection.GetSafeNormal()*200;
        const FString File=FPackageName::LongPackageNameToFilename(S->Input.PackageName,FPackageName::GetMapPackageExtension());
        const bool Loaded=FEditorFileUtils::LoadMap(File,false,false);auto* World=GEditor->GetEditorWorldContext().World();
        if (World && World->GetOutermost()->GetName()==S->Input.PackageName) S->EditorWorld=World;
        if (!Test.TestTrue(TEXT("Coastal proof reopens exactly its saved native-built scene."),Loaded && S->EditorWorld.IsValid() && World->GetWorldPartition())) return false;
        World->GetWorldPartition()->LoadAllActors(S->References);World->UpdateWorldComponents(false,false);World->SendAllEndOfFrameUpdates();
        if (!Measure(*S,*World,nullptr,TEXT("reload_"))) return false;
        S->Receipt->SetStringField(TEXT("scope"),S->Input.StreamingOnly ?
            TEXT("Authenticated saved coastal scene: fresh native fields/cook/identities, actual settled Hero and streaming return only; short traversals/rendering omitted, no builder or production admission.") :
            TEXT("Disposable selected coastal model, full native lower fields/cook, exact retained Upper and ordinary Water, short native Hero/visibility/WP proof; no Source admission, swimming, package or production acceptance."));
        auto* Settings=NewObject<ULevelEditorPlaySettings>(GetTransientPackage());Settings->SetPlayNetMode(PIE_Standalone);Settings->SetPlayNumberOfClients(1);
        Settings->SetRunUnderOneProcess(true);Settings->bLaunchSeparateServer=false;FRequestPlaySessionParams Request;Request.EditorPlaySettings=Settings;Request.bAllowOnlineSubsystem=false;
        auto& Framework=FAutomationTestFramework::Get();Framework.EnqueueLatentCommand(MakeShared<FStart>(Request));Framework.EnqueueLatentCommand(MakeShared<FWalk>(S));
        if (!S->Input.StreamingOnly) Framework.EnqueueLatentCommand(MakeShared<FView>(S));
        Framework.EnqueueLatentCommand(MakeShared<FStream>(S));
        Framework.EnqueueLatentCommand(MakeShared<FEndPlayMapCommand>());Framework.EnqueueLatentCommand(MakeShared<FEnd>(S));return true;
    }
}
#endif
