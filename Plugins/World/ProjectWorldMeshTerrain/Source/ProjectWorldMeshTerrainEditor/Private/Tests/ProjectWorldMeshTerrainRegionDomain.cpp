#include "ProjectWorldMeshTerrainRegionDomain.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionDomain
{
    bool RestoreCollision(UStaticMeshComponent& Component,ECollisionEnabled::Type Mode,FName Profile)
    {
        Component.SetCollisionProfileName(Profile);Component.SetCollisionEnabled(Mode);
        return Component.GetCollisionEnabled()==Mode && Component.GetCollisionProfileName()==Profile;
    }
    bool ExactWaterBindings(TConstArrayView<FString> Expected,TConstArrayView<FString> Observed)
    {
        TSet<FString> Required,Seen;
        for (const auto& Package : Expected) {if (Package.IsEmpty() || Required.Contains(Package)) return false;Required.Add(Package);}
        for (const auto& Package : Observed)
        {if (!Required.Contains(Package) || Seen.Contains(Package)) return false;Seen.Add(Package);}
        return Required.Num()>0 && Seen.Num()==Required.Num();
    }
    bool QueryCell(const FJsonObject& Query,const FString& Primary,FString& Out)
    {
        Out=Primary;
        return !Query.HasField(TEXT("cell_id")) || (Query.HasTypedField<EJson::String>(TEXT("cell_id")) && Query.TryGetStringField(TEXT("cell_id"),Out) && !Out.IsEmpty());
    }
    bool Selection(const FJsonObject& Document,bool Region,TSet<FString>& Cells,FString& Primary,FString& Error)
    {
        Cells.Reset();
        if (!Document.HasTypedField<EJson::String>(TEXT("cell_id")) || !Document.TryGetStringField(TEXT("cell_id"),Primary) || Primary.IsEmpty())
        {Error=TEXT("Regional selection requires its original primary cell.");return false;}
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if (Document.HasField(TEXT("cell_ids")))
        {
            if (!Region || !Document.TryGetArrayField(TEXT("cell_ids"),Values) || !Values || Values->Num()!=2)
            {Error=TEXT("Only a regional observation can request exactly two complete original cells.");return false;}
            for (const auto& Value : *Values)
            {
                FString Id;
                if (!Value || Value->Type!=EJson::String || !Value->TryGetString(Id) || Id.IsEmpty() || Cells.Contains(Id))
                {Error=TEXT("Regional cell identities must be nonempty and distinct.");return false;}
                Cells.Add(Id);
            }
            if (!Cells.Contains(Primary)) {Error=TEXT("The primary cell is absent from the regional domain.");return false;}
        }
        else Cells.Add(Primary);
        return true;
    }
    namespace
    {
        bool Configure(FAutomationTestBase& Test,const FString& Mount,FStructures& State)
        {
            FString Error;
            FProjectWorldRealizationLayer Layer;Layer.LayerId=TEXT("regional_structures");Layer.GeneratorId=TEXT("project_constructed_surface");Layer.GeneratorVersion=1;
            Layer.DirtyGranularity=EProjectWorldDirtyGranularity::CanonicalCell;Layer.CanonicalSelectors={TEXT("constructed_surfaces")};
            Layer.SpatialOwnership=TEXT("cell_local");Layer.RuntimeMapping=TEXT("world_partition_spatial");Layer.ArtifactRoot=Mount+TEXT("Structures/");
            Layer.NormalizedSettings=TEXT("{\"maximum_join_step_m\":0.001,\"nanite\":true,\"collision\":\"complex_as_simple\",\"navigation\":\"physical_support\"}");
            Layer=ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
            if (!ProjectWorldLayerProducerRegistry::ValidateLayer(Layer,Error)) {Test.AddError(Error);return false;}
            State.Profile.ProfileId=TEXT("regional_combined_observation");State.Profile.Layers={Layer};return true;
        }
        bool Invoke(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
            FStructures& State,bool Apply)
        {
            FString Error;const auto& Layer=State.Profile.Layers[0];
            const auto* Producer=ProjectWorldLayerProducerRegistry::Find(Layer.GeneratorId,Layer.GeneratorVersion,Error);
            auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
            if (!Test.TestTrue(TEXT("Combined scene uses the registered structural owner and explicit existing fixture material."),Producer && Material)) return false;
            FProjectWorldAuthoredOverlaySet Overlays;
            const FProjectWorldLayerInputs Inputs{Bundle,State.Profile,Layer,Overlays};
            const TArray<FString> Dirty={TEXT("*")};TMap<FName,UMaterialInterface*> Materials;Materials.Add(TEXT("road"),Material);
            const FProjectWorldLayerApplyContext Context{World,Inputs,Dirty,Materials};
            auto Inventory=ProjectWorldVerifyTestFlow::WholeLayer(Layer);FProjectWorldRealizationResult Result;
            const bool Good=Apply ? Producer->Apply(Context,Inventory,Result,Error) : Producer->CaptureArtifacts(Context,Inventory,Result,Error);
            if (!Good) Test.AddError(Error);
            return Good;
        }
    }
    bool ApplyStructures(FAutomationTestBase& Test,UWorld& World,const FJsonObject& Document,
        const FProjectWorldCanonicalBundle& Bundle,const FString& Mount,TArray<FString>& Packages,FStructures& State)
    {
        if (!Document.HasField(TEXT("constructed_feature_id"))) return true;
        FString Error;
        if (!Document.TryGetStringField(TEXT("constructed_feature_id"),State.Feature)) return false;
        const auto* Feature=Bundle.Features.Find(State.Feature);
        if (!Test.TestTrue(TEXT("The combined scene consumes one admitted constructed field with its complete two-cell membership."),
            Bundle.Cells.Num()==2 && Feature && Feature->ConstructedSurface.IsSet() && Feature->Representations.Num()==2)) return false;
        for (const auto& Pair : Bundle.Features) if (Pair.Value.ConstructedSurface.IsSet() && Pair.Key!=State.Feature)
        {Test.AddError(TEXT("Combined regional observation must not implicitly select another constructed feature."));return false;}
        if (!Configure(Test,Mount,State)) return false;
        if (!Invoke(Test,World,Bundle,State,true)) return false;
        for (TActorIterator<AStaticMeshActor> It(&World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1")))
        {
            UStaticMesh* Mesh=It->GetStaticMeshComponent()->GetStaticMesh();
            if (!Test.TestTrue(TEXT("Combined structural geometry stays in the same disposable scene mount."),Mesh && Mesh->GetOutermost()->GetName().StartsWith(Mount))) return false;
            Packages.AddUnique(Mesh->GetOutermost()->GetName());
        }
        return true;
    }
    FIsolatedTerrain::FIsolatedTerrain(FAutomationTestBase& InTest,UWorld& World,const FProjectWorldCanonicalBundle& Bundle):Test(InTest)
    {
        TArray<UStaticMeshComponent*> Candidates;
        for (TActorIterator<AStaticMeshActor> It(&World);It;++It)
            if (It->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1")))
                Candidates.Add(It->GetStaticMeshComponent());
        if (Candidates.IsEmpty()) {Valid=true;return;}
        const FString Mount=FPackageName::GetLongPackagePath(World.GetOutermost()->GetName())+TEXT("/");
        const FString Directory=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Mount));
        if (!Test.TestTrue(TEXT("Only this disposable two-cell editor scene may isolate its declared structures."),
            World.WorldType==EWorldType::Editor && Bundle.Cells.Num()==2 && Mount.StartsWith(TEXT("/ProjectWorldBasinSections")) &&
            FPaths::IsUnderDirectory(Directory,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world"))))) return;
        FStructures State;
        if (!Configure(Test,Mount,State) || !Invoke(Test,World,Bundle,State,false)) return;
        for (auto* Component : Candidates) Components.Add({Component,Component->GetCollisionEnabled(),Component->GetCollisionProfileName()});
        Applied=true;
        for (const auto& Pair : Components) Pair.Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Valid=true;
    }
    FIsolatedTerrain::~FIsolatedTerrain() {Restore();}
    bool FIsolatedTerrain::Restore()
    {
        if (Restored) return Valid;
        Restored=true;bool Good=true;
        if (!Applied) return Valid;
        for (const auto& Pair : Components)
        {
            Good &= Test.TestTrue(TEXT("Isolated Terrain probes restore every admitted structural collision mode and profile."),RestoreCollision(*Pair.Component,Pair.Mode,Pair.Profile));
        }
        return Good && Valid;
    }
    TSharedPtr<FJsonObject> FIsolatedTerrain::Receipt() const
    {
        auto Out=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Modes;
        for (const auto& Pair : Components)
        {
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("component"),Pair.Component->GetPathName());
            Row->SetNumberField(TEXT("original_collision_mode"),int32(Pair.Mode));Row->SetStringField(TEXT("original_collision_profile"),Pair.Profile.ToString());Modes.Add(MakeShared<FJsonValueObject>(Row));
        }
        Out->SetArrayField(TEXT("temporarily_isolated_structures"),Modes);
        Out->SetNumberField(TEXT("isolated_structure_components"),Components.Num());
        Out->SetBoolField(TEXT("collision_restored"),Restored);
        Out->SetStringField(TEXT("scope"),TEXT("Isolated Terrain field physics only; combined restored deck first-hit physics is measured separately."));return Out;
    }
    bool ObserveStructures(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
        FStructures& State,FJsonObject& Receipt,bool Reloaded)
    {
        if (State.Feature.IsEmpty()) return true;
        if (!Invoke(Test,World,Bundle,State,false)) return false;
        TArray<TSharedPtr<FJsonValue>> Actors;
        TArray<AStaticMeshActor*> Subjects;
        for (TActorIterator<AStaticMeshActor> It(&World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1"))) Subjects.Add(*It);
        Subjects.Sort([](const AStaticMeshActor& A,const AStaticMeshActor& B){return A.GetPackage()->GetName()<B.GetPackage()->GetName();});
        for (auto* It : Subjects)
        {
            auto* C=It->GetStaticMeshComponent();UStaticMesh* Mesh=C->GetStaticMesh();const auto* Description=Mesh ? Mesh->GetMeshDescription(0) : nullptr;
            if (!Test.TestNotNull(TEXT("Combined structure exposes its actual saved source field."),Description)) return false;
            const auto Positions=FStaticMeshConstAttributes(*Description).GetVertexPositions();
            TArray<TSharedPtr<FJsonValue>> Vertices,Faces;int32 Probes=0;double MaxError=0;FVector Witness=FVector::ZeroVector;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldRegionalStructure),true);
            for (const auto Triangle : Description->Triangles().GetElementIDs())
            {
                FVector P[3];TArray<TSharedPtr<FJsonValue>> Indices;
                for (int32 I=0;I<3;++I)
                {
                    const FVector Local(Positions[Description->GetVertexInstanceVertex(Description->GetTriangleVertexInstance(Triangle,I))]);
                    P[I]=C->GetComponentTransform().TransformPosition(Local);
                    const FVector Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,P[I]);
                    Indices.Add(MakeShared<FJsonValueNumber>(Vertices.Num()));
                    TArray<TSharedPtr<FJsonValue>> XYZ;
                    for (double V : {Canonical.X,Canonical.Y,Canonical.Z}) XYZ.Add(MakeShared<FJsonValueNumber>(V));
                    Vertices.Add(MakeShared<FJsonValueArray>(XYZ));
                }
                Faces.Add(MakeShared<FJsonValueArray>(Indices));
                const FVector Center=(P[0]+P[1]+P[2])/3;FHitResult Hit;
                Witness=Center;
                if (!Test.TestTrue(TEXT("Every combined structural face physically blocks its own Pawn-channel world ray."),
                    World.LineTraceSingleByChannel(Hit,Center+FVector(0,0,1000),Center-FVector(0,0,1000),ECC_Pawn,Query) && Hit.GetComponent()==C)) return false;
                MaxError=FMath::Max(MaxError,FMath::Abs(Hit.ImpactPoint.Z-Center.Z));++Probes;
            }
            if (!Test.TestTrue(TEXT("Actual combined structural collision remains on its zero-lift saved field."),Probes>0 && MaxError<=.1)) return false;
            const auto OriginalMode=C->GetCollisionEnabled();const FName OriginalProfile=C->GetCollisionProfileName();
            C->SetCollisionEnabled(ECollisionEnabled::NoCollision);FHitResult Disabled;
            const bool Borrowed=World.LineTraceSingleByChannel(Disabled,Witness+FVector(0,0,1000),Witness-FVector(0,0,1000),ECC_Pawn,Query) && Disabled.GetComponent()==C;
            const bool FullyRestored=RestoreCollision(*C,OriginalMode,OriginalProfile);FHitResult Restored;
            if (!Test.TestTrue(TEXT("Disabling an admitted fragment cannot borrow Terrain as that fragment's physical support; mode restores."),
                !Borrowed && FullyRestored &&
                World.LineTraceSingleByChannel(Restored,Witness+FVector(0,0,1000),Witness-FVector(0,0,1000),ECC_Pawn,Query) && Restored.GetComponent()==C)) return false;
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("actor_package"),It->GetPackage()->GetName());
            Row->SetStringField(TEXT("mesh"),Mesh->GetPathName());Row->SetStringField(TEXT("material"),GetPathNameSafe(C->GetMaterial(0)));
            Row->SetStringField(TEXT("feature_id"),State.Feature);Row->SetArrayField(TEXT("vertices_canonical"),Vertices);Row->SetArrayField(TEXT("triangles"),Faces);
            Row->SetNumberField(TEXT("collision_face_probes"),Probes);Row->SetNumberField(TEXT("maximum_collision_error_cm"),MaxError);
            Row->SetBoolField(TEXT("disabled_fragment_refuses_own_support"),!Borrowed);
            Row->SetNumberField(TEXT("restored_collision_mode"),int32(OriginalMode));
            Row->SetStringField(TEXT("restored_collision_profile"),OriginalProfile.ToString());
            Actors.Add(MakeShared<FJsonValueObject>(Row));
        }
        if (!Test.TestEqual(TEXT("The admitted regional structure has both original physical cell fragments."),Actors.Num(),2)) return false;
        auto Current=MakeShared<FJsonObject>();Current->SetArrayField(TEXT("actors"),Actors);
        Current->SetStringField(TEXT("material_scope"),TEXT("Explicit BasicShapeMaterial fixture binding; Terrain and ordinary Water retain their accepted bindings."));
        if (Reloaded)
        {
            FString A,B;FJsonSerializer::Serialize(State.Before.ToSharedRef(),TJsonWriterFactory<>::Create(&A));
            FJsonSerializer::Serialize(Current,TJsonWriterFactory<>::Create(&B));
            if (!Test.TestTrue(TEXT("Saved/reloaded combined structures retain exact fields, package identities and physical probes."),A==B)) return false;
        }
        else State.Before=Current;
        Receipt.SetObjectField(Reloaded ? TEXT("regional_structures_reloaded") : TEXT("regional_structures_before"),Current);
        return true;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRegionalDomainSelectionTest,"Project.World.Realization.MeshTerrain.CanonicalRegionSelection",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProjectWorldRegionalDomainSelectionTest::RunTest(const FString& Parameters)
{return ProjectWorldRegionDomain::VerifyInputContracts(*this);}
bool ProjectWorldRegionDomain::VerifyInputContracts(FAutomationTestBase& Test)
{
    auto* Component=NewObject<UStaticMeshComponent>(GetTransientPackage());
    Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    const auto OriginalMode=Component->GetCollisionEnabled();const FName OriginalProfile=Component->GetCollisionProfileName();
    Test.TestTrue(TEXT("The isolated actual component starts with the registered structural BlockAll/QueryAndPhysics contract."),
        OriginalMode==ECollisionEnabled::QueryAndPhysics && OriginalProfile==UCollisionProfile::BlockAll_ProfileName);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->SetCollisionEnabled(OriginalMode);
    Test.TestNotEqual(TEXT("Restoring only the numeric collision mode demonstrably loses the actual component profile."),Component->GetCollisionProfileName(),OriginalProfile);
    Test.TestTrue(TEXT("Full collision restoration retains both actual component profile and mode."),RestoreCollision(*Component,OriginalMode,OriginalProfile));
    FJsonObject Input;Input.SetStringField(TEXT("cell_id"),TEXT("original:a"));TSet<FString> Cells;FString Primary,Error;
    Test.TestTrue(TEXT("The original one-cell contract is preserved."),ProjectWorldRegionDomain::Selection(Input,true,Cells,Primary,Error) && Cells.Num()==1);
    auto Array=[](const FString& A,const FString& B){return TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueString>(A),MakeShared<FJsonValueString>(B)};};
    Input.SetArrayField(TEXT("cell_ids"),Array(TEXT("original:a"),TEXT("original:b")));
    Test.TestTrue(TEXT("Two complete original cells select together."),ProjectWorldRegionDomain::Selection(Input,true,Cells,Primary,Error) && Cells.Num()==2);
    Test.TestFalse(TEXT("The ordinary canonical lane cannot silently become a two-cell observation."),ProjectWorldRegionDomain::Selection(Input,false,Cells,Primary,Error));
    Input.SetArrayField(TEXT("cell_ids"),Array(TEXT("original:a"),TEXT("original:a")));
    Test.TestFalse(TEXT("Duplicate cells cannot masquerade as a complete domain."),ProjectWorldRegionDomain::Selection(Input,true,Cells,Primary,Error));
    Input.SetArrayField(TEXT("cell_ids"),Array(TEXT("original:b"),TEXT("original:c")));
    Test.TestFalse(TEXT("The declared primary cell cannot disappear."),ProjectWorldRegionDomain::Selection(Input,true,Cells,Primary,Error));
    FJsonObject Query;FString Selected;
    Test.TestTrue(TEXT("An omitted query identity preserves the original single-cell default."),ProjectWorldRegionDomain::QueryCell(Query,TEXT("original:a"),Selected) && Selected==TEXT("original:a"));
    Query.SetStringField(TEXT("cell_id"),TEXT("original:b"));
    Test.TestTrue(TEXT("An explicit query can identify the second selected cell."),ProjectWorldRegionDomain::QueryCell(Query,TEXT("original:a"),Selected) && Selected==TEXT("original:b"));
    Query.SetNumberField(TEXT("cell_id"),2);
    Test.TestFalse(TEXT("A present malformed query identity cannot silently use the primary cell."),ProjectWorldRegionDomain::QueryCell(Query,TEXT("original:a"),Selected));
    Query.SetArrayField(TEXT("cell_id"),{});
    Test.TestFalse(TEXT("A present array query identity refuses instead of falling back to the primary cell."),ProjectWorldRegionDomain::QueryCell(Query,TEXT("original:a"),Selected));
    Query.SetField(TEXT("cell_id"),MakeShared<FJsonValueNull>());
    Test.TestFalse(TEXT("A present null query identity refuses instead of falling back to the primary cell."),ProjectWorldRegionDomain::QueryCell(Query,TEXT("original:a"),Selected));
    const TArray<FString> Expected={TEXT("water:a"),TEXT("water:b")},Correct={TEXT("water:b"),TEXT("water:a")},Duplicated={TEXT("water:a"),TEXT("water:a")};
    Test.TestTrue(TEXT("A complete Water census is independent of actor iteration order."),ProjectWorldRegionDomain::ExactWaterBindings(Expected,Correct));
    Test.TestFalse(TEXT("Observed duplicate A and missing B cannot pass a two-actor expected A/B census."),ProjectWorldRegionDomain::ExactWaterBindings(Expected,Duplicated));
    Test.TestFalse(TEXT("Duplicated expected Water bindings also refuse."),ProjectWorldRegionDomain::ExactWaterBindings(Duplicated,Correct));return !Test.HasAnyErrors();
}
#endif
