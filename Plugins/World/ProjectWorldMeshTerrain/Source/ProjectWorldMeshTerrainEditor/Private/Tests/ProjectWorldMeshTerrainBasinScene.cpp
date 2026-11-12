// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionCollisionComponent.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "StaticMeshResources.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionRuntimeCellInterface.h"
#include "MeshPartitionEditorUtils.h"
#include "Materials/MaterialInterface.h"
#include "Tests/ProjectWorldBasinRendering.h"
#include "Tests/ProjectWorldBasinNavigation.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinSceneTest
{
    using namespace UE::MeshPartition;
    using namespace ProjectWorldConstructedRuntimeTest;
    struct FState
    {
        FAutomationTestBase& Test;
        FInput Input;
        TWeakObjectPtr<UWorld> EditorWorld;
        TArray<FWorldPartitionReference> LoadedReferences;
        TSharedPtr<FJsonObject> Receipt=MakeShared<FJsonObject>();
        TWeakObjectPtr<ACharacter> Character;
        TArray<TArray<FVector>> Routes;
        bool ClockOwned=false,PreviousFixed=false,CleanupAttempted=false;
        double PreviousDelta=0;
        explicit FState(FAutomationTestBase& InTest,FInput&& InInput):Test(InTest),Input(MoveTemp(InInput)) {}
        void Cleanup()
        {
            if (CleanupAttempted) return;CleanupAttempted=true;
            ON_SCOPE_EXIT
            {
                const bool Rendered=Input.NavigationOnly || !Input.Expected->HasField(TEXT("water")) || (Receipt->HasField(TEXT("near_visible_water_pixels")) && Receipt->HasField(TEXT("far_visible_water_pixels")));
                const bool Navigated=Receipt->HasField(TEXT("navigation_restored_segments")) && Receipt->GetNumberField(TEXT("navigation_restored_segments"))>0;
                const bool Streamed=Input.NavigationOnly || (Receipt->HasField(TEXT("streaming_return_sections")) &&
                    Receipt->HasField(TEXT("streaming_return_grounded_frames")) && Receipt->GetNumberField(TEXT("streaming_return_grounded_frames"))>0 &&
                    Receipt->HasField(TEXT("streaming_return_movement_segments")) && Receipt->GetNumberField(TEXT("streaming_return_movement_segments"))==2 &&
                    Receipt->HasField(TEXT("streaming_unloaded_sections")) && Receipt->GetNumberField(TEXT("streaming_unloaded_sections"))==0 &&
                    Receipt->HasField(TEXT("streaming_unloaded_water")) && Receipt->GetNumberField(TEXT("streaming_unloaded_water"))==0 &&
                    Receipt->HasField(TEXT("streaming_owned_weak_alive_after_unload")) && Receipt->GetNumberField(TEXT("streaming_owned_weak_alive_after_unload"))==0 &&
                    Receipt->HasField(TEXT("streaming_terminal_status")) && Receipt->GetStringField(TEXT("streaming_terminal_status"))==TEXT("completed"));
                const bool Completed=!Test.HasAnyErrors() && Navigated && Streamed && (Input.NavigationOnly || Receipt->HasField(TEXT("walking_frames"))) && Rendered;
                const bool NavigationCandidate=Receipt->HasField(TEXT("navigation_candidate_elevation_ratio")) ||
                    Receipt->HasField(TEXT("navigation_candidate_tile_size_cm")) || Receipt->HasField(TEXT("navigation_candidate_fine_cell_height_cm"));
                Receipt->SetStringField(TEXT("execution_result"),Completed ? (NavigationCandidate ? TEXT("completed_navigation_candidate_only") : Input.NavigationOnly ? TEXT("completed_navigation_only") : Receipt->HasField(TEXT("encoding_candidate_precision_requested")) ? TEXT("completed_encoding_candidate_only") : TEXT("completed")) : TEXT("failed_or_incomplete"));
                FString Text;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Text));
                Test.TestTrue(TEXT("Saved-scene execution metrics survive success and refusal."),FFileHelper::SaveStringToFile(Text,*(Input.Root/TEXT("gameplay.json"))));
            };
            if (ClockOwned) {FApp::SetFixedDeltaTime(PreviousDelta);FApp::SetUseFixedTimeStep(PreviousFixed);ClockOwned=false;}
            if (GEditor && GEditor->PlayWorld)
            {Test.AddError(TEXT("Active PIE retains the scratch world and mount lease after bounded shutdown."));Receipt->SetStringField(TEXT("cleanup_result"),TEXT("retained_active_pie"));return;}
            FString MapAfter;
            Test.TestTrue(TEXT("Native reload/PIE never rewrites the saved map dependency."),FProjectWorldCanonicalLoader::ComputeFileSha256(
                FPackageName::LongPackageNameToFilename(Input.PackageName,FPackageName::GetMapPackageExtension()),MapAfter) && MapAfter==Input.MapSha256);
            Receipt->SetStringField(TEXT("native_map_sha256_before"),Input.MapSha256);Receipt->SetStringField(TEXT("native_map_sha256_after"),MapAfter);
            if (EditorWorld.IsValid())
            {
                if (!Test.TestTrue(TEXT("Scene cleanup still owns the loaded scratch map."),GEditor && GEditor->GetEditorWorldContext().World()==EditorWorld.Get()))
                {Receipt->SetStringField(TEXT("cleanup_result"),TEXT("retained_ownership_mismatch"));return;}
                LoadedReferences.Reset();
                FAutomationEditorCommonUtils::CreateNewMap(); EditorWorld.Reset(); CollectGarbage(RF_NoFlags);
            }
            TArray<UPackage*> Loaded;
            for (const auto& Name : Input.Packages) if (auto* Package=FindPackage(nullptr,*Name)) Loaded.Add(Package);
            if (!Loaded.IsEmpty())
            {
                UPackageTools::FUnloadPackageParams Params(Loaded);Params.bResetTransBuffer=false;
                if (!Test.TestTrue(TEXT("The reloaded section scene releases all owned packages."),UPackageTools::UnloadPackages(Params)))
                {Receipt->SetStringField(TEXT("cleanup_result"),TEXT("retained_unload_failure"));return;}
            }
            Input.PackageName.Reset();Input.Packages.Reset();
            if (!Input.Mount.IsEmpty())
            {
                if (!Input.PluginManaged) FPackageName::UnRegisterMountPoint(Input.Mount,Input.Directory);Input.Mount.Reset();
            }
            Receipt->SetStringField(TEXT("cleanup_result"),Input.PluginManaged ? TEXT("scene_released_plugin_retained_for_cold_process_exit") : TEXT("released"));
        }
        ~FState() {if (!CleanupAttempted && EditorWorld.IsValid() && GEditor && GEditor->PlayWorld) GEditor->EndPlayMap();Cleanup();}
    };
    FVector Point(const TSharedPtr<FJsonValue>& Value)
    {
        const auto& P=Value->AsArray(); return FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber());
    }
    bool Height(const FInput& Input,const FVector& World,double& Out)
    {
        const FVector P=FProjectWorldCanonicalLoader::UnrealToCanonical(Input.Bundle,World);
        for (const auto& Q : Input.Bundle.Cells[0].Terrain.RefinedQuads) for (const auto& I : Q.Triangles)
        {
            const FVector A=Q.Vertices[I.X],B=Q.Vertices[I.Y],C=Q.Vertices[I.Z];
            const FVector2D U(B-A),V(C-A),D(P-A); const double Den=U.X*V.Y-U.Y*V.X;
            const double S=(D.X*V.Y-D.Y*V.X)/Den,T=(U.X*D.Y-U.Y*D.X)/Den;
            if (S>=-1e-9 && T>=-1e-9 && S+T<=1+1e-9)
            {Out=FProjectWorldCanonicalLoader::CanonicalToUnreal(Input.Bundle,FVector(P.X,P.Y,A.Z+S*(B.Z-A.Z)+T*(C.Z-A.Z))).Z;return true;}
        }
        return false;
    }
    bool OwnsSavedActor(const AActor* Actor,const UWorld* World,const FInput& Input,const FString& ExpectedPackage)
    {
        if (!Actor || Actor->GetWorld()!=World || !Input.Packages.Contains(ExpectedPackage)) return false;
        const UPackage* Package=Actor->GetPackage();
        if (World->WorldType!=EWorldType::PIE) return World->GetOutermost()->GetName()==Input.PackageName && Package->GetName()==ExpectedPackage;
        int32 Instance=INDEX_NONE;
        if (UWorld::RemovePIEPrefix(World->GetOutermost()->GetName(),&Instance)!=Input.PackageName || Instance<0) return false;
        const FString Source=Package->GetLoadedPath().GetPackageName();
        const ULevel* Level=Actor->GetLevel();const IWorldPartitionCell* Cell=Level ? Level->GetWorldPartitionRuntimeCell() : nullptr;
        return !Source.IsEmpty() && Source==ExpectedPackage && Cell && Cell->GetOuterWorld()==World &&
            Package->GetName()==ULevel::GetExternalActorPackageInstanceName(Level->GetOutermost()->GetName(),Source);
    }
    bool Measure(FState& State,UWorld* World,ACharacter* Pawn,const FString& OverridePrefix=FString())
    {
        int32 Sections=0,Components=0,Faces=0,RayHits=0,SweepHits=0,DisabledHits=0,RestoredHits=0;
        double MaxRayError=0,MaxGradient=0;
        TSet<FName> Variants;
        TSet<FName> ExpectedVariants;
        const bool PIE=World->WorldType==EWorldType::PIE;
        int32 PIEInstance=INDEX_NONE;
        if (PIE && !State.Test.TestTrue(TEXT("PIE remains the exact saved scene."),
            UWorld::RemovePIEPrefix(World->GetOutermost()->GetName(),&PIEInstance)==State.Input.PackageName && PIEInstance>=0)) return false;
        const auto OwnedSection=[&](ACompiledSection* Section,const FString& ExpectedPackage)
        {
            return OwnsSavedActor(Section,World,State.Input,ExpectedPackage);
        };
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldBasinReload),true); if (Pawn) Query.AddIgnoredActor(Pawn);
        const double Radius=Pawn ? Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34;
        const double Half=Pawn ? Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88;
        for (TActorIterator<ACompiledSection> It(World); It; ++It)
        {
            ++Sections; const auto& Info=It->GetBuildInfo(); Variants.Add(Info.BuildVariantName);
            auto* Definition=Info.GetMegaMeshDefinition();
            if (!State.Test.TestTrue(TEXT("Reload preserves the exact accepted definition and identity transforms."),Definition &&
                Definition->GetPathName()==State.Input.Expected->GetStringField(TEXT("expected_definition_object_path")) &&
                Definition->GetMaterial() && Definition->GetMaterial()->GetPathName()==State.Input.Expected->GetStringField(TEXT("accepted_material")) &&
                It->GetActorTransform().Equals(FTransform::Identity,0))) return false;
            if (ExpectedVariants.IsEmpty())
            {
                if (PIE)
                {
                    FName Preview;auto* Platform=UE::MeshPartition::EditorUtils::GetPreviewPlatform(Preview);
                    ExpectedVariants.Append(Definition->GetCompiledSectionBuildVariantNamesForCurrentEditorPreview(Preview,Platform));
                }
                else for (const auto& Variant : Definition->GetCompiledSectionBuildVariants()) ExpectedVariants.Add(Variant.Name);
            }
            if (!State.Test.TestTrue(TEXT("Every active section is selected by the actual native platform policy."),ExpectedVariants.Contains(Info.BuildVariantName))) return false;
            TSharedPtr<FJsonObject> Match;
            for (const auto& Row : State.Input.Expected->GetArrayField(TEXT("variants")))
                if (Row->AsObject()->GetStringField(TEXT("variant"))==Info.BuildVariantName.ToString()) Match=Row->AsObject();
            if (!State.Test.TestTrue(TEXT("No unrequested section variant appears after reload."),Match.IsValid())) return false;
            if (!State.Test.TestTrue(TEXT("Actual sections retain the saved native build identity without PIE placeholder reconstruction."),
                !It->IsPlaceholder() && Info.BuildKey.ToString(EGuidFormats::Digits)==Match->GetStringField(TEXT("native_build_key")))) return false;
            if (!State.Test.TestTrue(TEXT("Every actual section belongs to the saved scene through native package instancing."),
                OwnedSection(*It,Match->GetStringField(TEXT("native_actor_package"))))) return false;
            if (!State.Test.TestFalse(TEXT("Saved source binding rejects another valid inventory package, the root map."),
                OwnsSavedActor(*It,World,State.Input,State.Input.PackageName))) return false;
            const auto& ExpectedComponents=Match->GetArrayField(TEXT("components"));
            if (!State.Test.TestEqual(TEXT("Reload retains the exact component census before indexing."),
                It->GetMeshComponents().Num()+It->GetCollisionComponents().Num(),ExpectedComponents.Num())) return false;
            int32 ComponentIndex=0;
            for (const auto& Component : It->GetMeshComponents())
            {
                ++Components; UStaticMesh* Mesh=Component->GetStaticMesh();
                const bool Owned=Mesh && Mesh->IsIn(*It) && Mesh->GetOutermost()==It->GetPackage(),Identity=Component->GetComponentTransform().Equals(FTransform::Identity,0);
                const int32 LODs=Mesh && Mesh->GetRenderData() ? Mesh->GetRenderData()->LODResources.Num() : 0;
                if (!State.Test.TestTrue(FString::Printf(TEXT("Saved visual mesh ownership/frame/resources: variant=%s mesh=%s package=%s actor_package=%s world_package=%s owned=%d identity=%d lods=%d transform=%s"),
                    *Info.BuildVariantName.ToString(),*GetPathNameSafe(Mesh),Mesh ? *Mesh->GetOutermost()->GetName() : TEXT("none"),
                    *It->GetPackage()->GetName(),*World->GetOutermost()->GetName(),Owned,Identity,LODs,*Component->GetComponentTransform().ToHumanReadableString()),Owned && Identity && LODs>0)) return false;
                const auto E=ExpectedComponents[ComponentIndex++]->AsObject();
                if (!State.Test.TestTrue(TEXT("Reload keeps the requested visual/Nanite variant."),E->GetStringField(TEXT("kind"))==TEXT("gpu_fallback") &&
                    Mesh->HasValidNaniteData()==(Info.BuildVariantName==TEXT("nanite")))) return false;
                const auto& LOD=Mesh->GetRenderData()->LODResources[0];
                const uint32 Count=LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices(),Indices=LOD.IndexBuffer.GetNumIndices(),Stride=LOD.IndexBuffer.Is32Bit() ? 4 : 2;
                TArray<uint8> Positions,IndexBytes;
                if (!State.Test.TestTrue(TEXT("Reloaded visual GPU buffers retain their complete census."),
                    Count==E->GetArrayField(TEXT("world_vertices_cm")).Num() && Indices==3*E->GetArrayField(TEXT("triangles")).Num() &&
                    LOD.VertexBuffers.PositionVertexBuffer.GetStride()==sizeof(FVector3f) &&
                    ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.VertexBuffers.PositionVertexBuffer.VertexBufferRHI.GetReference(),Count*sizeof(FVector3f),Positions) &&
                    ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.IndexBuffer.IndexBufferRHI.GetReference(),Indices*Stride,IndexBytes))) return false;
                for (uint32 I=0;I<Count;++I)
                {
                    FVector3f Local;FMemory::Memcpy(&Local,Positions.GetData()+I*sizeof(Local),sizeof(Local));
                    if (!State.Test.TestTrue(TEXT("Every saved visual vertex survives reload exactly."),FVector(Local)==Point(E->GetArrayField(TEXT("world_vertices_cm"))[I]))) return false;
                }
                for (uint32 I=0;I<Indices;++I)
                {
                    uint32 Actual=0;if (Stride==4) FMemory::Memcpy(&Actual,IndexBytes.GetData()+I*Stride,4);
                    else {uint16 Value;FMemory::Memcpy(&Value,IndexBytes.GetData()+I*Stride,2);Actual=Value;}
                    if (!State.Test.TestEqual(TEXT("Every saved oriented visual index survives reload exactly."),Actual,
                        uint32(E->GetArrayField(TEXT("triangles"))[I/3]->AsArray()[I%3]->AsNumber()))) return false;
                }
            }
            for (const auto& Component : It->GetCollisionComponents())
            {
                ++Components;FTriMeshCollisionData Data;
                if (!State.Test.TestTrue(TEXT("Reload retains the cooked physical section and identity frame."),Component->GetComponentTransform().Equals(FTransform::Identity,0) &&
                    Component->GetPhysicsTriMeshData(&Data,true) && Component->GetBodySetup() && Component->GetBodySetup()->bCreatedPhysicsMeshes)) return false;
                const auto E=ExpectedComponents[ComponentIndex++]->AsObject();
                if (!State.Test.TestTrue(TEXT("The separate collision representation retains its complete census."),E->GetStringField(TEXT("kind"))==TEXT("collision") &&
                    Data.Vertices.Num()==E->GetArrayField(TEXT("world_vertices_cm")).Num() && Data.Indices.Num()==E->GetArrayField(TEXT("triangles")).Num())) return false;
                for (int32 I=0;I<Data.Vertices.Num();++I)
                    if (!State.Test.TestTrue(TEXT("Every saved collision vertex survives reload exactly."),FVector(Data.Vertices[I])==Point(E->GetArrayField(TEXT("world_vertices_cm"))[I]))) return false;
                for (int32 I=0;I<Data.Indices.Num();++I)
                {
                    const auto& IDs=Data.Indices[I]; const auto& ExpectedIDs=E->GetArrayField(TEXT("triangles"))[I]->AsArray();
                    if (!State.Test.TestTrue(TEXT("Every saved oriented collision index survives reload exactly."),
                        IDs.v0==ExpectedIDs[0]->AsNumber() && IDs.v1==ExpectedIDs[1]->AsNumber() && IDs.v2==ExpectedIDs[2]->AsNumber())) return false;
                    ++Faces;const FVector A(Data.Vertices[IDs.v0]),B(Data.Vertices[IDs.v1]),C(Data.Vertices[IDs.v2]),Center=(A+B+C)/3.;
                    const FVector Normal=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
                    MaxGradient=FMath::Max(MaxGradient,FVector2D(Normal).Length()/FMath::Abs(Normal.Z));
                    FHitResult Ray,Sweep;
                    if (World->LineTraceSingleByChannel(Ray,Center+FVector(0,0,400),Center-FVector(0,0,400),ECC_Pawn,Query) && Ray.bBlockingHit && Ray.GetComponent()==Component.Get())
                    {++RayHits;MaxRayError=FMath::Max(MaxRayError,FMath::Abs(Ray.ImpactPoint.Z-Center.Z));}
                    if (World->SweepSingleByChannel(Sweep,Center+FVector(0,0,400),Center-FVector(0,0,400),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Query) &&
                        Sweep.bBlockingHit && Sweep.GetComponent()==Component.Get()) ++SweepHits;
                }
                if (!State.Test.TestTrue(TEXT("Collision calibration has a positive witness."),Data.Indices.Num()>0)) return false;
                const auto& First=Data.Indices[0];
                const FVector Witness=(FVector(Data.Vertices[First.v0])+FVector(Data.Vertices[First.v1])+FVector(Data.Vertices[First.v2]))/3.;
                const auto Enabled=Component->GetCollisionEnabled();FHitResult Ray,Sweep;
                const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
                Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                DisabledHits+=World->LineTraceSingleByChannel(Ray,Witness+FVector(0,0,400),Witness-FVector(0,0,400),ECC_Pawn,Query) ? 1 : 0;
                DisabledHits+=World->SweepSingleByChannel(Sweep,Witness+FVector(0,0,400),Witness-FVector(0,0,400),FQuat::Identity,ECC_Pawn,Capsule,Query) ? 1 : 0;
                Component->SetCollisionEnabled(Enabled);
                RestoredHits+=World->LineTraceSingleByChannel(Ray,Witness+FVector(0,0,400),Witness-FVector(0,0,400),ECC_Pawn,Query) && Ray.bBlockingHit && Ray.GetComponent()==Component.Get() ? 1 : 0;
                RestoredHits+=World->SweepSingleByChannel(Sweep,Witness+FVector(0,0,400),Witness-FVector(0,0,400),FQuat::Identity,ECC_Pawn,Capsule,Query) && Sweep.bBlockingHit && Sweep.GetComponent()==Component.Get() ? 1 : 0;
            }
            if (!State.Test.TestEqual(TEXT("Each reloaded variant has exactly its original component census."),ComponentIndex,ExpectedComponents.Num())) return false;
        }
        State.Test.TestTrue(TEXT("The active variant census is positive and exactly matches native platform selection."),!ExpectedVariants.IsEmpty() && Variants.Num()==ExpectedVariants.Num() && Sections==ExpectedVariants.Num());
        State.Test.TestEqual(TEXT("Every selected component is present."),Components,ExpectedVariants.Num());
        State.Test.TestEqual(TEXT("Every captured physical face survives reload."),Faces,1606);
        State.Test.TestEqual(TEXT("Every reloaded face physically blocks the ray."),RayHits,Faces);
        State.Test.TestEqual(TEXT("Every reloaded face physically supports the selected capsule."),SweepHits,Faces);
        State.Test.TestEqual(TEXT("Reloaded physical probes reject disabled collision."),DisabledHits,0);
        State.Test.TestEqual(TEXT("Restoration revives both reloaded physical probes."),RestoredHits,2);
        State.Test.TestTrue(TEXT("Reloaded physical ray precision matches the actual collision representation."),MaxRayError<=.01);
        const FString Prefix=OverridePrefix.IsEmpty() ? (Pawn ? TEXT("pie_") : TEXT("reload_")) : OverridePrefix;
        State.Receipt->SetNumberField(Prefix+TEXT("sections"),Sections);State.Receipt->SetNumberField(Prefix+TEXT("faces"),Faces);
        State.Receipt->SetNumberField(Prefix+TEXT("ray_hits"),RayHits);State.Receipt->SetNumberField(Prefix+TEXT("capsule_hits"),SweepHits);
        State.Receipt->SetNumberField(Prefix+TEXT("capsule_radius_cm"),Radius);State.Receipt->SetNumberField(Prefix+TEXT("capsule_half_height_cm"),Half);
        State.Receipt->SetNumberField(Prefix+TEXT("maximum_ray_to_collision_error_cm"),MaxRayError);
        State.Receipt->SetNumberField(Prefix+TEXT("disabled_collision_hits"),DisabledHits);State.Receipt->SetNumberField(Prefix+TEXT("restored_collision_hits"),RestoredHits);
        State.Receipt->SetNumberField(TEXT("maximum_collision_gradient"),MaxGradient);
        if (State.Input.Expected->HasField(TEXT("water")) && !MeasureJointWater(State.Test,World,State.Input,Prefix,*State.Receipt)) return false;
        return !State.Test.HasAnyErrors();
    }
    class FStart final : public FStartPIEForAutomationCommand
    {
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    public:
        explicit FStart(const FRequestPlaySessionParams& Request):FStartPIEForAutomationCommand(Request),Settings(Request.EditorPlaySettings.Get()) {}
    };
    class FWalk final : public IAutomationLatentCommand
    {
        TSharedRef<FState> State; TWeakObjectPtr<ACharacter> Character;
        TWeakObjectPtr<UActorComponent> NavigationInvoker;
        TArray<TArray<FVector>> Lanes;
        int32 Lane=0,Next=1,Frames=0,Segments=0,Falling=0,Unknown=0;
        double SettledUntil=0,Started=0,RealStarted=0,SegmentStarted=0,MaxFootOffset=0,MinFootOffset=DBL_MAX,FootBound=0;
        uint64 LastFrame=0;
        bool Pending=true;
        void VerifyNavigation(UWorld* World,ACharacter* Pawn)
        {
            TArray<UPrimitiveComponent*> Terrain;
            for (TActorIterator<ACompiledSection> It(World);It;++It)
                for (const auto& Component : It->GetCollisionComponents()) Terrain.Add(Component.Get());
            FString Error;
            if (!ProjectWorldBasinNavigation::Verify(*World,*Pawn,State->Input.Bundle,Lanes,Terrain,
                [&](const FVector& P,double& Z){return Height(State->Input,P,Z);},*State->Receipt,Error)) State->Test.AddError(Error);
        }
    public:
        explicit FWalk(TSharedRef<FState> In):State(In) {}
        ~FWalk() override {if (NavigationInvoker.IsValid()) NavigationInvoker->DestroyComponent();}
        bool Update() override
        {
            auto& Test=State->Test;if (Test.HasAnyErrors()) return true;
            UWorld* World=GEditor ? GEditor->PlayWorld.Get() : nullptr;
            if (!Test.TestNotNull(TEXT("The saved-section Hero runs in native PIE."),World)) return true;
            if (!Character.IsValid())
            {
                ACharacter* Pawn=nullptr; APlayerController* Controller=nullptr;
                if (!SpawnPawn(World,State->Input.Start,State->Input.Pawn,Test,Pawn,Controller)) return true;
                FString AgentError;
                if (!ProjectWorldBasinNavigation::CheckAgent(*World,*Pawn,*State->Receipt,AgentError)) {Test.AddError(AgentError);return true;}
                if (!Measure(*State,World,Pawn)) return true;
                Character=Pawn;State->Character=Pawn;const auto& B=State->Input.Bundle.Cells[0].Bounds;
                const double X=(B.X+B.Z)/2,Y=(B.Y+B.W)/2;
                const TArray<TArray<FVector2D>> Canonical={{{B.X+3,Y},{B.Z-3,Y},{B.X+3,Y}},{{X,B.Y+3},{X,B.W-3},{X,B.Y+3}},
                    {{X-7,Y-7},{X+7,Y-7},{X+7,Y+7},{X-7,Y+7},{X-7,Y-7}}};
                for (const auto& Route : Canonical)
                {
                    auto& Points=Lanes.AddDefaulted_GetRef();
                    for (const auto& P : Route) {FVector W=FProjectWorldCanonicalLoader::CanonicalToUnreal(State->Input.Bundle,FVector(P,20));double Z;
                        if (!Height(State->Input,W,Z)) {Test.AddError(TEXT("A basin traversal has unknown canonical support."));return true;}W.Z=Z;Points.Add(W);}
                }
                State->Routes=Lanes;
                UActorComponent* OwnedInvoker=nullptr;FString NavigationError;
                const bool Prepared=ProjectWorldBasinNavigation::Prepare(*Pawn,Lanes,OwnedInvoker,NavigationError);
                NavigationInvoker=OwnedInvoker;
                if (!Prepared) {Test.AddError(NavigationError);return true;}
                const double G=State->Receipt->GetNumberField(TEXT("maximum_collision_gradient"));
                FootBound=UCharacterMovementComponent::MAX_FLOOR_DIST+Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius()*(FMath::Sqrt(1+G*G)-1)+.01;
                State->PreviousFixed=FApp::UseFixedTimeStep();State->PreviousDelta=FApp::GetFixedDeltaTime();State->ClockOwned=true;
                FApp::SetFixedDeltaTime(1./60);FApp::SetUseFixedTimeStep(true);Started=World->GetTimeSeconds();RealStarted=FPlatformTime::Seconds();
            }
            if (LastFrame==GFrameCounter) return false;LastFrame=GFrameCounter;
            auto* Pawn=Character.Get();auto* Movement=Pawn->GetCharacterMovement();const double Now=World->GetTimeSeconds();
            if (!Test.TestTrue(TEXT("Basin traversal stays within its bounded native-frame budget."),Now-Started<240 && FPlatformTime::Seconds()-RealStarted<600)) return true;
            if (State->Input.NavigationOnly)
            {
                if (Now-Started<1.5) return false;
                VerifyNavigation(World,Pawn);return true;
            }
            if (Pending)
            {
                Movement->StopMovementImmediately();Pawn->SetActorLocation(Lanes[Lane][0]+FVector(0,0,150),false,nullptr,ETeleportType::TeleportPhysics);
                Movement->SetMovementMode(MOVE_Falling);SettledUntil=Now+1;SegmentStarted=Now+1;Pending=false;return false;
            }
            if (Now<SettledUntil) return false;
            ++Frames;Falling+=Movement->IsFalling();double Z;
            if (!Height(State->Input,Pawn->GetActorLocation(),Z)) ++Unknown;
            else {const double Offset=Pawn->GetActorLocation().Z-Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Z;
                MaxFootOffset=FMath::Max(MaxFootOffset,Offset);MinFootOffset=FMath::Min(MinFootOffset,Offset);}
            if (!Test.TestTrue(TEXT("The configured Hero remains grounded on the actual saved sections."),Movement->IsMovingOnGround())) return true;
            FVector Delta=Lanes[Lane][Next]-Pawn->GetActorLocation();Delta.Z=0;
            if (Delta.Length()<15)
            {
                ++Segments;Movement->StopMovementImmediately();++Next;SegmentStarted=Now;
                if (Next==Lanes[Lane].Num())
                {
                    ++Lane;Next=1;Pending=true;
                    if (Lane==Lanes.Num())
                    {
                        Test.TestEqual(TEXT("All three basin/plaza traversals complete their eight segments."),Segments,8);
                        Test.TestEqual(TEXT("Measured walking never falls."),Falling,0);Test.TestEqual(TEXT("Measured walking never invents support."),Unknown,0);
                        Test.TestTrue(TEXT("Actual feet satisfy the capsule/slope/floor-distance envelope."),Frames>0 && MinFootOffset>=-.01 && MaxFootOffset<=FootBound);
                        State->Receipt->SetNumberField(TEXT("walking_frames"),Frames);State->Receipt->SetNumberField(TEXT("walked_segments"),Segments);
                        State->Receipt->SetNumberField(TEXT("falling_frames"),Falling);State->Receipt->SetNumberField(TEXT("unknown_support_frames"),Unknown);
                        State->Receipt->SetNumberField(TEXT("minimum_foot_offset_cm"),MinFootOffset);State->Receipt->SetNumberField(TEXT("maximum_foot_offset_cm"),MaxFootOffset);
                        State->Receipt->SetNumberField(TEXT("capsule_slope_floor_envelope_cm"),FootBound);
                        VerifyNavigation(World,Pawn);
                        return true;
                    }
                }
                return false;
            }
            if (!Test.TestTrue(TEXT("Each real movement segment makes bounded progress."),Now-SegmentStarted<35)) return true;
            Pawn->AddMovementInput(Delta.GetSafeNormal(),1,true);return false;
        }
    };
    class FRender final : public IAutomationLatentCommand
    {
        TSharedRef<FState> State;FProjectWorldBasinRenderProbe Probe;bool Started=false,Prepared=false,Restored=false;
        int32 Precision=0;uint64 ReadyFrame=0;
        TArray<TWeakObjectPtr<UStaticMeshComponent>> Targets;
        TArray<TStrongObjectPtr<UStaticMesh>> Originals,Candidates;
        TArray<bool> ForceFlags;
        bool RestoreTargets(TConstArrayView<TWeakObjectPtr<UStaticMeshComponent>> Current,int32& Count,FString& Error)
        {
            Count=0;Error.Reset();
            if (Current.Num()!=Originals.Num() || Current.Num()!=ForceFlags.Num())
            {Error=TEXT("Encoding restoration has an incomplete mesh/flag inventory.");return false;}
            for (int32 I=0;I<Current.Num();++I)
            {
                if (!Current[I].IsValid() || !Originals[I].IsValid()) {Error=TEXT("Encoding restoration lost an actual component or original mesh.");continue;}
                Current[I]->SetStaticMesh(Originals[I].Get());Current[I]->SetForceDisableNanite(ForceFlags[I]);
                if (Current[I]->GetStaticMesh()==Originals[I].Get() && Current[I]->IsForceDisableNanite()==ForceFlags[I]) ++Count;
                else Error=TEXT("Encoding restoration failed to recover the original mesh/flag.");
            }
            return Error.IsEmpty() && Count==Current.Num();
        }
        void Restore()
        {
            if (Restored) return;Restored=true;Probe.Reset();int32 Count=0;FString Error;
            if (!Targets.IsEmpty())
            {
                auto Unknown=Targets;Unknown[0].Reset();int32 ControlCount=0;
                State->Test.TestTrue(TEXT("Actual restoration refuses an unknown target while restoring all remaining captured targets."),
                    !RestoreTargets(Unknown,ControlCount,Error) && !Error.IsEmpty() && ControlCount<Unknown.Num());
            }
            if (!RestoreTargets(Targets,Count,Error)) State->Test.AddError(Error);
            if (Precision) State->Receipt->SetNumberField(TEXT("encoding_candidate_restored_components"),Count);
            Candidates.Reset();Originals.Reset();
        }
    public:
        explicit FRender(TSharedRef<FState> In):State(In) {}
        ~FRender() override {Restore();}
        bool Update() override
        {
            auto& Test=State->Test;if (Test.HasAnyErrors()) return true;
            UWorld* World=GEditor ? GEditor->PlayWorld.Get() : nullptr;FString Error;
            if (!Test.TestNotNull(TEXT("Joint rendering observes the actual saved PIE scene."),World)) return true;
            if (!Started)
            {
                if (!Prepared)
                {
                    const FString Option=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_ENCODING_PRECISION"));
                    if (!Option.IsEmpty() && !Test.TestTrue(TEXT("A disposable precision candidate is explicit and bounded."),LexTryParseString(Precision,*Option) && Precision>=4 && Precision<=16)) return true;
                    if (Precision)
                    {
                        State->Receipt->SetNumberField(TEXT("encoding_candidate_precision_requested"),Precision);
                        State->Receipt->SetStringField(TEXT("scope"),TEXT("Saved native geometry/reload/physics/Hero proof precedes an UNSAVED encoding-only candidate from the identical saved mesh description; no saved candidate or production acceptance."));
                        TArray<TSharedPtr<FJsonValue>> Metrics;
                        for (TActorIterator<ACompiledSection> It(World);It;++It) for (const auto& Component : It->GetMeshComponents())
                        {
                            UStaticMesh* Original=Component->GetStaticMesh();if (!Original || !Original->HasValidNaniteData()) continue;
                            if (!Test.TestTrue(TEXT("The encoding candidate replaces only the actual nonphysical Nanite visual component."),
                                Component->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !Component->IsForceDisableNanite())) return true;
                            Targets.Add(Component.Get());Originals.Emplace(Original);ForceFlags.Add(Component->IsForceDisableNanite());
                            auto Row=MakeShared<FJsonObject>();Metrics.Add(MakeShared<FJsonValueObject>(Row));State->Receipt->SetArrayField(TEXT("encoding_candidates"),Metrics);
                            auto Candidate=CompileEncodingCandidate(Test,Original,Precision,*Row);
                            if (!Candidate.IsValid()) return true;
                            Candidates.Add(MoveTemp(Candidate));Component->SetStaticMesh(Candidates.Last().Get());
                        }
                        if (!Test.TestTrue(TEXT("The encoding experiment has a positive explicit component census."),!Targets.IsEmpty())) return true;
                        State->Receipt->SetArrayField(TEXT("encoding_candidates"),Metrics);World->SendAllEndOfFrameUpdates();
                    }
                    Prepared=true;ReadyFrame=GFrameCounter+(Precision ? 5 : 0);
                }
                if (GFrameCounter<ReadyFrame) return false;
                FBox Bounds(ForceInit);const auto& Feature=State->Input.Bundle.Features.FindChecked(State->Input.Bundle.Cells[0].OwnedFeatureIds[0]);
                for (const auto& P : Feature.GeometryPolygons[0].Outer)
                    Bounds+=FProjectWorldCanonicalLoader::CanonicalToUnreal(State->Input.Bundle,FVector(P,Feature.WaterSurface.LevelMeters));
                TArray<UStaticMeshComponent*> Terrain;
                for (TActorIterator<ACompiledSection> It(World);It;++It) for (const auto& Component : It->GetMeshComponents())
                    if (Component->GetStaticMesh() && Component->GetStaticMesh()->HasValidNaniteData()) Terrain.Add(Component.Get());
                if (!Probe.Begin(*World,Bounds,Terrain,State->Receipt->GetNumberField(TEXT("maximum_collision_gradient")),State->Input.Root/TEXT("renders"),Error)) {Test.AddError(Error);return true;}Started=true;
            }
            const auto Result=Probe.Step(*State->Receipt,Error);
            if (Result==EProjectWorldBasinRenderProgress::Failed) Test.AddError(Error);
            if (Result==EProjectWorldBasinRenderProgress::Passed && Precision) State->Receipt->SetStringField(TEXT("render_scope"),
                TEXT("Transient encoding-only mesh from identical saved source attributes/material/settings, scoped observed-view recipe and near/far/native controls; original mesh restored. Saved candidate, geographic accuracy and production remain open."));
            if (Result!=EProjectWorldBasinRenderProgress::Pending) Restore();
            return Result!=EProjectWorldBasinRenderProgress::Pending;
        }
    };
    class FStreaming final : public IAutomationLatentCommand
    {
        TSharedRef<FState> State;TSharedPtr<IAutomationLatentCommand> Command;
    public:
        explicit FStreaming(TSharedRef<FState> In):State(In) {}
        bool Update() override
        {
            if (State->Test.HasAnyErrors()) return true;
            if (!State->Character.IsValid()) {State->Test.AddError(TEXT("Streaming lost the actual walked Hero."));return true;}
            if (!Command) Command=StreamingReturn(State->Test,State->Input,State->Routes,State->Receipt.ToSharedRef(),*State->Character.Get(),
                [State=State](const FVector& P,double& Z){return Height(State->Input,P,Z);},
                [State=State](UWorld* World,ACharacter* Pawn)
                {
                    if (!Measure(*State,World,Pawn,TEXT("streaming_return_"))) return false;
                    TArray<UPrimitiveComponent*> Terrain;
                    for (TActorIterator<ACompiledSection> It(World);It;++It) for (const auto& C : It->GetCollisionComponents()) Terrain.Add(C.Get());
                    auto Navigation=MakeShared<FJsonObject>();State->Receipt->SetObjectField(TEXT("streaming_return_navigation"),Navigation);FString Error;
                    if (!ProjectWorldBasinNavigation::Verify(*World,*Pawn,State->Input.Bundle,State->Routes,Terrain,
                        [&](const FVector& P,double& Z){return Height(State->Input,P,Z);},*Navigation,Error)) {State->Test.AddError(Error);return false;}
                    return true;
                });
            return Command->Update();
        }
    };
    class FCleanup final : public IAutomationLatentCommand
    {
        TSharedRef<FState> State;double Started=0;
    public:
        explicit FCleanup(TSharedRef<FState> In):State(In) {}
        bool Update() override
        {
            if (GEditor && GEditor->PlayWorld)
            {
                if (Started==0) Started=FPlatformTime::Seconds();
                if (FPlatformTime::Seconds()-Started>30)
                {State->Test.AddError(TEXT("Basin PIE shutdown exceeded its budget."));GEditor->EndPlayMap();State->Cleanup();return true;}
                return false;
            }
            State->Cleanup();return true;
        }
    };
    bool Queue(FAutomationTestBase& Test,FInput&& Input)
    {
        auto State=MakeShared<FState>(Test,MoveTemp(Input));
        State->Receipt->SetStringField(TEXT("scope"),TEXT("Actual saved section reload, native WP instancing and configured Hero direct-CMC native PIE; optional joint Water actor/render. Navigation, streaming unload/return, input/menu, source admission and production acceptance remain separate."));
        if (State->Input.NavigationOnly) State->Receipt->SetStringField(TEXT("scope"),TEXT("Authenticated copied saved scene, native reload/PIE/physics and configured Hero navigation only; no builder, walking, rendering, source/geographic admission or production proof."));
        State->Receipt->SetStringField(TEXT("fixture_sha256"),State->Input.Expected->GetStringField(TEXT("fixture_sha256")));
        State->Receipt->SetStringField(TEXT("source_definition_sha256"),State->Input.Expected->GetStringField(TEXT("source_definition_sha256")));
        if (State->Input.Expected->HasField(TEXT("replay_manifest_sha256")))
        {
            State->Receipt->SetStringField(TEXT("replay_manifest_sha256"),State->Input.Expected->GetStringField(TEXT("replay_manifest_sha256")));
            if (!State->Input.NavigationOnly) State->Receipt->SetStringField(TEXT("scope"),TEXT("Authenticated copied saved scene, native reload/PIE/physics, configured Hero walking/Nav, joint Water/render and native streaming return. No producer/builder replay, source/geographic admission or production acceptance."));
        }
        const FString File=FPackageName::LongPackageNameToFilename(State->Input.PackageName,FPackageName::GetMapPackageExtension());
        const bool Loaded=FEditorFileUtils::LoadMap(File,false,false);
        UWorld* World=GEditor->GetEditorWorldContext().World();
        if (World && World->GetOutermost()->GetName()==State->Input.PackageName) State->EditorWorld=World;
        if (!Test.TestTrue(TEXT("The saved scratch map reopens through native editor loading."),Loaded)) return false;
        if (State->EditorWorld.IsValid() && World->GetWorldPartition())
        {
            World->GetWorldPartition()->LoadAllActors(State->LoadedReferences);World->UpdateWorldComponents(false,false);World->SendAllEndOfFrameUpdates();
        }
        if (!Test.TestTrue(TEXT("Reload owns the exact saved world package."),World && World->GetOutermost()->GetName()==State->Input.PackageName) || !Measure(*State,World,nullptr)) return false;
        GEngine->Exec(World,TEXT("MeshPartition.ReportSectionStatus"));
        auto* Settings=NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
        Settings->SetPlayNetMode(PIE_Standalone);Settings->SetPlayNumberOfClients(1);Settings->SetRunUnderOneProcess(true);Settings->bLaunchSeparateServer=false;
        Settings->NewWindowWidth=1280;Settings->NewWindowHeight=720;
        FRequestPlaySessionParams Request;Request.EditorPlaySettings=Settings;Request.bAllowOnlineSubsystem=false;
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStart>(Request));
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWalk>(State));
        if (!State->Input.NavigationOnly && State->Input.Expected->HasField(TEXT("water"))) FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRender>(State));
        if (!State->Input.NavigationOnly) FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStreaming>(State));
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndPlayMapCommand>());
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCleanup>(State));return true;
    }
}
#endif
