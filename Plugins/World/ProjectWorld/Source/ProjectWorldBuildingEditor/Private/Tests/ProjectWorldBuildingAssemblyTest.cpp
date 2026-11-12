#include "ProjectWorldBuildingAssembly.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "PackageTools.h"
#include "Tests/AutomationEditorCommon.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    bool OutwardModules(const AActor& Actor,double Frame,FJsonObject& Receipt)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(&Actor);
        if (Parts.Num()!=4) return false;
        for (auto* Part : Parts)
        {
            const auto& L=Part->GetStaticMesh()->GetRenderData()->LODResources[0];const auto Index=L.IndexBuffer.GetArrayView();
            double Volume=0;
            for (int I=0;I<Index.Num();I+=3)
            {
                const FVector A(L.VertexBuffers.PositionVertexBuffer.VertexPosition(Index[I]));
                const FVector B(L.VertexBuffers.PositionVertexBuffer.VertexPosition(Index[I+1]));
                const FVector C(L.VertexBuffers.PositionVertexBuffer.VertexPosition(Index[I+2]));
                Volume-=FVector::DotProduct(A,FVector::CrossProduct(B,C))/6.;
                const FVector Face=FVector::CrossProduct(C-A,B-A).GetSafeNormal();
                for (int J=0;J<3;++J)
                {const auto N=L.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Index[I+J]);if (FVector::DotProduct(Face,FVector(N.X,N.Y,N.Z))<=0) return false;}
            }
            const double Expected=Part->GetName()==TEXT("Part1") ? 4*Frame*(1-Frame)*1.e6 : 1.e6;
            Receipt.SetNumberField(Part->GetName(),Volume);
            if (FMath::Abs(Volume-Expected)>Expected*1.e-6) return false;
        }
        return true;
    }
    FString Emission(const FProjectWorldBuildingMeshBuildResult& R,const FProjectWorldBuildingMeshBuildResult::FEmission& E)
    {
        FStaticMeshConstAttributes A(R.MeshDescription);const auto P=A.GetVertexPositions();FString Text;
        for (int I=E.FirstTriangle;I<E.FirstTriangle+E.TriangleCount;++I)
        {
            const auto V=R.MeshDescription.GetTriangleVertices(FTriangleID(I));
            for (int J=0;J<3;++J) Text+=FString::Printf(TEXT("%.9g,%.9g,%.9g|"),P[V[J]].X,P[V[J]].Y,P[V[J]].Z);
        }
        return Text;
    }
    UStaticMesh* Mesh(UObject* Outer,FMeshDescription& Description,UMaterialInterface* Material)
    {
        auto* M=NewObject<UStaticMesh>(Outer);M->GetStaticMaterials().Add(FStaticMaterial(Material));
        UStaticMesh::FBuildMeshDescriptionsParams P;P.bAllowCpuAccess=true;P.bBuildSimpleCollision=false;
        if (!M->BuildFromMeshDescriptions({&Description},P)) return nullptr;
        if (!M->GetBodySetup()) M->CreateBodySetup();M->GetBodySetup()->CollisionTraceFlag=CTF_UseComplexAsSimple;
        M->GetBodySetup()->bDoubleSidedGeometry=true;M->GetBodySetup()->CreatePhysicsMeshes();return M;
    }
    UStaticMeshComponent* Put(UWorld& W,FMeshDescription& D,const FVector& Origin,UMaterialInterface* Material,const TCHAR* Name)
    {
        FActorSpawnParameters P;P.Name=FName(Name);auto* A=W.SpawnActor<AActor>(P);
        auto* C=NewObject<UStaticMeshComponent>(A);A->AddInstanceComponent(C);A->SetRootComponent(C);
        C->SetStaticMesh(Mesh(A,D,Material));C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        C->RegisterComponent();A->SetActorLocation(Origin);return C;
    }
    FMeshDescription GroundMesh(const FProjectWorldCanonicalCell& Cell,const FBox2D& Bounds,
        const FProjectWorldCanonicalBundle& Bundle,const FVector& Origin,FString& Error)
    {
        FMeshDescription D;FStaticMeshAttributes A(D);A.Register();A.GetVertexInstanceUVs().SetNumChannels(1);
        const auto G=D.CreatePolygonGroup();auto P=A.GetVertexPositions();auto N=A.GetVertexInstanceNormals();auto UV=A.GetVertexInstanceUVs();
        TArray<FProjectWorldSurfaceTriangle> Triangles;
        if (!ProjectWorldTerrainSurface::Triangles(Cell,Bounds,Triangles,Error)) return D;
        for (const auto& T : Triangles)
        {
            TArray<FVertexInstanceID> Instances;
            // Canonical northing is mirrored in Unreal Y.
            for (int Index : {0,2,1})
            {
                const auto V=D.CreateVertex();P[V]=FVector3f(FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,T.Points[Index])-Origin);
                const auto I=D.CreateVertexInstance(V);N[I]=FVector3f(0,0,1);UV.Set(I,0,FVector2f::ZeroVector);Instances.Add(I);
            }
            D.CreatePolygon(G,Instances);
        }
        return D;
    }
    bool Capture(UWorld& W,const AActor& Building,const FVector& Target,const FString& Path,FJsonObject& Evidence)
    {
        FActorSpawnParameters P;P.ObjectFlags=RF_Transient;
        const FVector Eye=Target+Building.GetActorQuat().RotateVector(FVector(-2000,-2200,1600));
        auto* Camera=W.SpawnActor<ASceneCapture2D>(Eye,(Target-Eye).Rotation(),P);
        auto* Light=W.SpawnActor<ADirectionalLight>(FVector::ZeroVector,FRotator(-45,-30,0),P);
        if (!Camera || !Light) return false;
        ON_SCOPE_EXIT {W.DestroyActor(Camera);W.DestroyActor(Light);};
        const FVector SpawnDirection=Light->GetActorForwardVector();
        Evidence.SetStringField(TEXT("spawn_light_direction"),SpawnDirection.ToString());
        // Spawn composes the native light root's default pitch with the supplied transform.
        Light->SetActorRotation(FRotator(-45,-30,0));
        const FVector Intended=FRotator(-45,-30,0).Vector();
        Evidence.SetStringField(TEXT("actual_light_direction"),Light->GetActorForwardVector().ToString());
        if (!Light->GetActorForwardVector().Equals(Intended,1.e-8)) return false;
        Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);Light->GetLightComponent()->SetIntensity(10);
        auto* C=Camera->GetCaptureComponent2D();auto* RT=NewObject<UTextureRenderTarget2D>(Camera);
        RT->RenderTargetFormat=RTF_RGBA8;RT->InitAutoFormat(960,640);RT->UpdateResourceImmediate(true);
        C->TextureTarget=RT;C->bCaptureEveryFrame=false;C->bCaptureOnMovement=false;C->CaptureSource=SCS_FinalColorLDR;
        C->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(&Building);
        for (auto* Part : Parts) C->ShowOnlyComponent(Part);
        W.UpdateWorldComponents(false,false);W.SendAllEndOfFrameUpdates();FlushRenderingCommands();
        for (int I=0;I<4;++I) {C->CaptureScene();FlushRenderingCommands();}
        TArray<FColor> Pixels;TArray64<uint8> PNG;
        if (!RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)) return false;
        FImageUtils::PNGCompressImageArray(960,640,Pixels,PNG);
        if (!FFileHelper::SaveArrayToFile(PNG,*(FPaths::GetPath(Path)/TEXT("original-lighting.png")))) return false;
        C->ShowOnlyComponents.Empty();C->CaptureScene();FlushRenderingCommands();TArray<FColor> Empty;
        if (!RT->GameThread_GetRenderTargetResource()->ReadPixels(Empty) || Empty.Num()!=Pixels.Num()) return false;
        int Different=0;for (int I=0;I<Pixels.Num();++I) Different+=Pixels[I]!=Empty[I];
        if (Different<1000) return false;
        // Base color separates missing geometry from the isolated scene's lighting.
        C->CaptureSource=SCS_BaseColor;C->CaptureScene();FlushRenderingCommands();
        TArray<FColor> EmptyBase;if (!RT->GameThread_GetRenderTargetResource()->ReadPixels(EmptyBase)) return false;
        auto PartEvidence=MakeShared<FJsonObject>();
        for (auto* Part : Parts)
        {
            C->ShowOnlyComponents.Empty();C->ShowOnlyComponent(Part);C->CaptureScene();FlushRenderingCommands();
            TArray<FColor> PartPixels;if (!RT->GameThread_GetRenderTargetResource()->ReadPixels(PartPixels) || PartPixels.Num()!=EmptyBase.Num()) return false;
            int Visible=0;for (int I=0;I<PartPixels.Num();++I) Visible+=PartPixels[I]!=EmptyBase[I];
            PartEvidence->SetNumberField(Part->GetName(),Visible);
            if (Visible<50) return false;
        }
        Evidence.SetObjectField(TEXT("base_color_part_pixels"),PartEvidence);
        C->ShowOnlyComponents.Empty();for (auto* Part : Parts) C->ShowOnlyComponent(Part);
        C->CaptureScene();FlushRenderingCommands();TArray<FColor> BasePixels;
        if (!RT->GameThread_GetRenderTargetResource()->ReadPixels(BasePixels)) return false;
        PNG.Reset();FImageUtils::PNGCompressImageArray(960,640,BasePixels,PNG);
        if (!FFileHelper::SaveArrayToFile(PNG,*(FPaths::GetPath(Path)/TEXT("geometry.png")))) return false;
        int Bright=0,Visible=0;
        for (int I=0;I<Pixels.Num();++I)
        {Bright+=Pixels[I].R>24 || Pixels[I].G>24 || Pixels[I].B>24;Visible+=BasePixels[I]!=EmptyBase[I];}
        Evidence.SetNumberField(TEXT("lit_bright_pixels"),Bright);
        Evidence.SetNumberField(TEXT("base_color_visible_pixels"),Visible);
        Evidence.SetNumberField(TEXT("empty_control_changed_pixels"),Different);
        if (Bright<FMath::Max(1000,Visible/2)) return false;
        PNG.Reset();FImageUtils::PNGCompressImageArray(960,640,Pixels,PNG);return FFileHelper::SaveArrayToFile(PNG,*Path);
    }
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FProjectWorldBuildingAssemblyTest,"Project.World.BuildingAssembly.CanonicalPrototype",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)
void FProjectWorldBuildingAssemblyTest::GetTests(TArray<FString>& Names,TArray<FString>& Parameters) const
{if (!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_BUILDING_ASSEMBLY_CASE")).IsEmpty()) {Names.Add(TEXT(""));Parameters.Add(TEXT(""));}}
bool FProjectWorldBuildingAssemblyTest::RunTest(const FString&)
{
    FString Case=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_BUILDING_ASSEMBLY_CASE")),Text,Hash;
    TSharedPtr<FJsonObject> D;
    if (!TestTrue(TEXT("The source-bound case is scratch-local and pinned."),
        FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Case),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
        FProjectWorldCanonicalLoader::ComputeFileSha256(Case,Hash) && Hash==FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_BUILDING_ASSEMBLY_CASE_SHA256")) &&
        FFileHelper::LoadFileToString(Text,*Case) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),D))) return false;
    const FString Root=D->GetStringField(TEXT("output")),Base=D->GetStringField(TEXT("canonical_result")),Id=D->GetStringField(TEXT("feature_id"));
    if (!TestTrue(TEXT("A fresh isolated output uses the intended real canonical feature."),Id==TEXT("alis:osm:way:250440683") &&
        FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Root),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
        !FPaths::DirectoryExists(Root) && IFileManager::Get().MakeDirectory(*Root,true) &&
        FProjectWorldCanonicalLoader::ComputeFileSha256(Base,Hash) && Hash==D->GetStringField(TEXT("canonical_sha256")))) return false;
    FProjectWorldCanonicalBundle Bundle;FProjectWorldCanonicalValidation V;
    if (!TestTrue(TEXT("The normal canonical reader authenticates the complete actual base."),FProjectWorldCanonicalLoader::Load(Base,Bundle,V))) {AddError(V.Message);return false;}
    const auto* Feature=Bundle.Features.Find(Id);
    auto* Cell=Feature ? Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellId==Feature->OwnerCellId;}) : nullptr;
    if (!TestNotNull(TEXT("The real building owns an available canonical cell."),Cell)) return false;
    ProjectBuildingAssembly::FKit Kit;FString Error;
    if (!TestTrue(TEXT("The original procedural kit is admitted."),ProjectBuildingAssembly::ReadKit(D->GetStringField(TEXT("kit")),Kit,Error))) {AddError(Error);return false;}
    FString KitText;FFileHelper::LoadFileToString(KitText,*D->GetStringField(TEXT("kit")));
    for (int CaseIndex=0;CaseIndex<3;++CaseIndex)
    {
        TSharedPtr<FJsonObject> Bad;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(KitText),Bad);
        FString Schema=FPaths::ConvertRelativePathToFull(FPaths::GetPath(D->GetStringField(TEXT("kit")))/TEXT("../Schemas/modular-kit.schema.json"));
        FPaths::MakePathRelativeTo(Schema,*(Root/TEXT("/")));Bad->SetStringField(TEXT("$schema"),Schema);
        if (CaseIndex==0) Bad->GetObjectField(TEXT("parameters"))->SetBoolField(TEXT("panel_width_m"),true);
        if (CaseIndex==1) Bad->GetObjectField(TEXT("parameters"))->SetStringField(TEXT("panel_width_m"),TEXT("2"));
        if (CaseIndex==2) Bad->SetNumberField(TEXT("kit_id"),123);
        FString BadText;FJsonSerializer::Serialize(Bad,TJsonWriterFactory<>::Create(&BadText));
        const FString BadPath=Root/FString::Printf(TEXT("invalid-kit-%d.json"),CaseIndex);FFileHelper::SaveStringToFile(BadText,*BadPath);
        ProjectBuildingAssembly::FKit BadKit;
        if (!TestFalse(TEXT("Native kit admission cannot coerce booleans, numeric strings or identities."),ProjectBuildingAssembly::ReadKit(BadPath,BadKit,Error))) return false;
    }
    FProjectWorldAuthoredOverlaySet Overlays;ProjectWorldBuildingAssembly::FPrepared Prepared;
    if (!TestTrue(TEXT("Complete foundation coverage and selected-only massing replacement qualify."),
        ProjectWorldBuildingAssembly::Prepare(Bundle,*Cell,Overlays,Id,Kit,Prepared,Error))) {AddError(Error);return false;}
    FBox2D FootBounds(ForceInit);for (const auto& P : Prepared.Footprint) FootBounds+=P;
    FProjectWorldCanonicalFeature Ribbon;Ribbon.FeatureId=TEXT("fixture:assembly-ribbon");Ribbon.FeatureClass=TEXT("water");Ribbon.GeometryType=TEXT("LineString");Ribbon.WidthMeters=2;
    Ribbon.GeometryPoints={FVector2D(FootBounds.Max.X+.5,FootBounds.Min.Y),FVector2D(FootBounds.Max.X+.5,FootBounds.Max.Y)};
    Bundle.Features.Add(Ribbon.FeatureId,Ribbon);ProjectWorldBuildingAssembly::FPrepared WetRefusal;
    const bool WetAccepted=ProjectWorldBuildingAssembly::Prepare(Bundle,*Cell,Overlays,Id,Kit,WetRefusal,Error);
    Bundle.Features.Remove(Ribbon.FeatureId);
    if (!TestFalse(TEXT("Wet ribbon width cannot be hidden by a disjoint source axis."),WetAccepted)) return false;
    FProjectWorldBuildingMeshBuildResult Before;
    if (!TestTrue(TEXT("Original complete-cell massing builds."),ProjectWorldBuildingMeshBuilder::BuildCell(Bundle,*Cell,Overlays,{},Before,Error))) return false;
    int Unchanged=0,Removed=0;
    for (const auto& E : Before.Emissions)
    {
        const auto* After=Prepared.RemainingMassing.Emissions.FindByPredicate([&](const auto& A){return A.FeatureId==E.FeatureId;});
        if (E.FeatureId==Id) {TestTrue(TEXT("Only the selected admitted blockout is suppressed."),After==nullptr);++Removed;}
        else {if (!TestTrue(TEXT("Every unrelated emitted building preserves its exact geometry."),After && Emission(Before,E)==Emission(Prepared.RemainingMassing,*After))) return false;++Unchanged;}
    }
    if (!TestTrue(TEXT("Selected replacement has a positive unrelated context."),Removed==1 && Unchanged>0)) return false;
    if (!TestTrue(TEXT("Classification dispositions remain unchanged."),Before.Rejections.Num()==Prepared.RemainingMassing.Rejections.Num() && Before.Stats.AcceptedFragmentCount==Prepared.RemainingMassing.Stats.AcceptedFragmentCount)) return false;
    const FString Mount=TEXT("/ProjectBuildingPrototype")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT("/");
    const FString Directory=Root/TEXT("packages/");IFileManager::Get().MakeDirectory(*Directory,true);FPackageName::RegisterMountPoint(Mount,Directory);
    ON_SCOPE_EXIT {FPackageName::UnRegisterMountPoint(Mount,Directory);};
    const FString PackageName=Mount+TEXT("Prototype"),Filename=Directory/TEXT("Prototype.umap");
    auto* Package=CreatePackage(*PackageName);const auto Init=UWorld::InitializationValues().CreatePhysicsScene(true);
    UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,TEXT("Prototype"),Package,true,ERHIFeatureLevel::Num,&Init);
    if (!TestNotNull(TEXT("A disposable native world owns the prototype."),World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
    ON_SCOPE_EXIT {if (World) {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);World->RemoveFromRoot();}};
    World->SetFlags(RF_Public|RF_Standalone);UMaterialInterface* Material=UMaterial::GetDefaultMaterial(MD_Surface);
    auto* Remaining=Put(*World,Prepared.RemainingMassing.MeshDescription,Prepared.RemainingMassing.ActorOrigin,Material,TEXT("UnselectedMassing"));
    auto* Authored=World->SpawnActor<AActor>();Authored->Tags.Add(TEXT("ProtectedAuthoredControl"));
    Authored->SetActorLocation(Prepared.Transform.GetLocation()+FVector(5000,0,0));const auto AuthoredTransform=Authored->GetActorTransform();
    AActor* Building=nullptr;bool Reused=false;
    if (!TestTrue(TEXT("Native ISMs realize the admitted modular plan."),ProjectBuildingAssembly::Apply(*World,Id,Prepared.Plan,Kit,Prepared.Transform,Material,Building,Reused,Error))) {AddError(Error);return false;}
    auto* Initial=Building;
    if (!TestTrue(TEXT("Unchanged input validates actual output and performs a no-op."),ProjectBuildingAssembly::Apply(*World,Id,Prepared.Plan,Kit,Prepared.Transform,Material,Building,Reused,Error) && Reused && Initial==Building)) return false;
    TInlineComponentArray<UInstancedStaticMeshComponent*> InitialParts(Building);
    InitialParts[0]->SetRelativeLocation(FVector(0,100,0));
    if (!TestTrue(TEXT("A displaced component cannot pass output reuse."),!ProjectBuildingAssembly::Matches(*Building,
        ProjectBuildingAssembly::GeometryIdentity(Id,Prepared.Plan,Kit,Prepared.Transform),Prepared.Plan,Kit,Prepared.Transform,Material))) return false;
    InitialParts[0]->SetRelativeLocation(FVector::ZeroVector);InitialParts.Reset();
    const FName OwnerTag(TEXT("Project.BuildingAssembly.Generated=v1"));Building->Tags.Remove(OwnerTag);
    auto* ProtectedAssociation=Building;AActor* Rejected=nullptr;
    if (!TestTrue(TEXT("Feature-associated content without generated ownership is preserved on refusal."),
        !ProjectBuildingAssembly::Apply(*World,Id,Prepared.Plan,Kit,Prepared.Transform,Material,Rejected,Reused,Error) && IsValid(ProtectedAssociation))) return false;
    ProtectedAssociation->Tags.Add(OwnerTag);
    auto ChangedKit=Kit;ChangedKit.WindowEvery=3;ProjectWorldBuildingAssembly::FPrepared Changed;
    if (!TestTrue(TEXT("Kit-only change prepares a new assembly and identical remaining massing."),ProjectWorldBuildingAssembly::Prepare(Bundle,*Cell,Overlays,Id,ChangedKit,Changed,Error) &&
        Changed.RemainingMassing.SemanticDigest==Prepared.RemainingMassing.SemanticDigest && !(Changed.Plan==Prepared.Plan))) return false;
    if (!TestTrue(TEXT("Kit regeneration replaces only the selected assembly."),ProjectBuildingAssembly::Apply(*World,Id,Changed.Plan,ChangedKit,Changed.Transform,Material,Building,Reused,Error) && !Reused && Building!=Initial)) return false;
    TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(Building);FTransform Corrupt;
    Parts[0]->GetInstanceTransform(0,Corrupt,false);Corrupt.AddToTranslation(FVector(100,0,0));Parts[0]->UpdateInstanceTransform(0,Corrupt,false,true,true);
    if (!TestTrue(TEXT("A hash tag cannot conceal a corrupted native output."),!ProjectBuildingAssembly::Matches(*Building,
        ProjectBuildingAssembly::GeometryIdentity(Id,Changed.Plan,ChangedKit,Changed.Transform),Changed.Plan,ChangedKit,Changed.Transform,Material))) return false;
    if (!TestTrue(TEXT("Corrupted local output is reconstructed."),ProjectBuildingAssembly::Apply(*World,Id,Changed.Plan,ChangedKit,Changed.Transform,Material,Building,Reused,Error) && !Reused)) return false;
    FProjectWorldAuthoredOverlay Protection;Protection.Anchor.Kind=EProjectWorldAnchorKind::Feature;Protection.Anchor.FeatureId=Id;Overlays.Overlays.Add(Protection);
    ProjectWorldBuildingAssembly::FPrepared Refused;
    if (!TestFalse(TEXT("Protected authored selection refuses before emission."),ProjectWorldBuildingAssembly::Prepare(Bundle,*Cell,Overlays,Id,Kit,Refused,Error))) return false;
    auto Unsupported=ChangedKit;Unsupported.MaximumSupportSpan=0;
    if (Changed.Input.SupportSpan>0 && !TestFalse(TEXT("Unsupported support refuses without changing the scene."),ProjectWorldBuildingAssembly::Prepare(Bundle,*Cell,{},Id,Unsupported,Refused,Error))) return false;
    if (!TestTrue(TEXT("Unrelated massing and protected authored actor remain live and unchanged."),IsValid(Remaining) && IsValid(Authored) && Authored->GetActorTransform().Equals(AuthoredTransform))) return false;
    FBox2D GroundBounds(ForceInit);for (const auto& P : Prepared.Footprint) GroundBounds+=P;
    GroundBounds=GroundBounds.ExpandBy(30);auto Ground=GroundMesh(*Cell,GroundBounds,Bundle,Prepared.Transform.GetLocation(),Error);
    auto* Terrain=Put(*World,Ground,Prepared.Transform.GetLocation(),Material,TEXT("CanonicalGroundControl"));
    FHitResult Hit;const FVector RoofPoint=Changed.Transform.TransformPosition(FVector(Changed.Input.Width/2,Changed.Input.Depth/2,Changed.Input.Height)*100);
    if (!TestTrue(TEXT("Actual cooked building collision supports its roof."),World->LineTraceSingleByChannel(Hit,RoofPoint+FVector(0,0,200),RoofPoint-FVector(0,0,200),ECC_Visibility) && Hit.GetActor()==Building)) return false;
    FCollisionQueryParams Ignore;Ignore.AddIgnoredActor(Building);Ignore.AddIgnoredActor(Remaining->GetOwner());
    const FVector Center=Changed.Transform.TransformPosition(FVector(Changed.Input.Width/2,Changed.Input.Depth/2,0)*100);
    if (!TestTrue(TEXT("The physical Terrain control exists below the admitted foundation envelope."),World->LineTraceSingleByChannel(Hit,Center+FVector(0,0,100),Center-FVector(0,0,100),ECC_Visibility,Ignore) && Hit.GetComponent()==Terrain && Hit.ImpactPoint.Z<=Prepared.Transform.GetLocation().Z+.01)) return false;
    const FString BuildingName=Building->GetName();const FString Identity=ProjectBuildingAssembly::GeometryIdentity(Id,Changed.Plan,ChangedKit,Changed.Transform);
    const auto ExpectedPlan=Changed.Plan;const auto ExpectedTransform=Changed.Transform;
    if (!TestTrue(TEXT("The native scene saves beneath scratch."),FEditorFileUtils::SaveMap(World,Filename))) return false;
    TWeakObjectPtr<AActor> OldBuilding=Building;World->DestroyWorld(false);GEngine->DestroyWorldContext(World);World->RemoveFromRoot();World=nullptr;Building=nullptr;Parts.Reset();Remaining=nullptr;Authored=nullptr;Terrain=nullptr;
    UPackageTools::FUnloadPackageParams Unload({Package});Unload.bResetTransBuffer=false;
    if (!TestTrue(TEXT("Saved objects unload before replay."),UPackageTools::UnloadPackages(Unload) && !OldBuilding.IsValid())) return false;
    if (!TestTrue(TEXT("The saved scene reloads through the native Editor."),FEditorFileUtils::LoadMap(Filename,false,false))) return false;
    auto* Loaded=GEditor->GetEditorWorldContext().World();ON_SCOPE_EXIT {FAutomationEditorCommonUtils::CreateNewMap();};
    for (TActorIterator<AActor> It(Loaded);It;++It) if (It->GetName()==BuildingName) Building=*It;
    if (!TestTrue(TEXT("Every saved instance and module mesh retains its exact admitted layout."),Building && ProjectBuildingAssembly::Matches(*Building,Identity,ExpectedPlan,ChangedKit,ExpectedTransform,Material))) return false;
    auto Volumes=MakeShared<FJsonObject>();
    if (!TestTrue(TEXT("Compiled modules have independently specified positive UE volumes and outward normals."),OutwardModules(*Building,ChangedKit.FrameFraction,*Volumes))) return false;
    if (!TestTrue(TEXT("Reloaded cooked collision retains actual roof support."),Loaded->LineTraceSingleByChannel(Hit,RoofPoint+FVector(0,0,200),RoofPoint-FVector(0,0,200),ECC_Visibility) && Hit.GetActor()==Building)) return false;
    const FVector Look=ExpectedTransform.TransformPosition(FVector(Changed.Input.Width/2,Changed.Input.Depth/2,Changed.Input.Height/2)*100);
    auto Visual=MakeShared<FJsonObject>();
    if (!TestTrue(TEXT("All four saved module families render with outward faces; empty geometry refuses."),Capture(*Loaded,*Building,Look,Root/TEXT("building.png"),*Visual))) return false;
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("feature_id"),Id);Receipt->SetStringField(TEXT("canonical_result_sha256"),D->GetStringField(TEXT("canonical_sha256")));
    Receipt->SetObjectField(TEXT("visual"),Visual);
    Receipt->SetObjectField(TEXT("compiled_unit_volumes_cm3"),Volumes);
    Receipt->SetStringField(TEXT("geometry_identity"),Identity);Receipt->SetStringField(TEXT("remaining_massing_identity"),Prepared.RemainingMassing.SemanticDigest);
    Receipt->SetNumberField(TEXT("unchanged_buildings"),Unchanged);Receipt->SetNumberField(TEXT("removed_blockouts"),Removed);Receipt->SetNumberField(TEXT("instances"),ExpectedPlan.Instances.size());
    Receipt->SetNumberField(TEXT("covered_area_m2"),Prepared.CoveredArea);Receipt->SetNumberField(TEXT("support_minimum_m"),Prepared.SupportMinimum);Receipt->SetNumberField(TEXT("support_maximum_m"),Prepared.SupportMaximum);
    Receipt->SetStringField(TEXT("scope"),TEXT("One real canonical feature, complete-cell classification, selected-only modular replacement, native no-op/repair, saved/reloaded ISMs and isolated render; no production layer or terrain mutation."));
    FString Output;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Output));return TestTrue(TEXT("Proof receipt is retained."),FFileHelper::SaveStringToFile(Output,*(Root/TEXT("receipt.json"))));
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FProjectWorldBuildingSavedRenderTest,"Project.World.BuildingAssembly.SavedRender",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)
void FProjectWorldBuildingSavedRenderTest::GetTests(TArray<FString>& Names,TArray<FString>& Parameters) const
{if (!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_BUILDING_ASSEMBLY_VIEW_CASE")).IsEmpty()) {Names.Add(TEXT(""));Parameters.Add(TEXT(""));}}
bool FProjectWorldBuildingSavedRenderTest::RunTest(const FString&)
{
    const FString Input=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_BUILDING_ASSEMBLY_VIEW_CASE"));
    FString Text,Hash;TSharedPtr<FJsonObject> D;
    if (!TestTrue(TEXT("The saved-view descriptor is pinned and scratch-local."),
        FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Input),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
        FProjectWorldCanonicalLoader::ComputeFileSha256(Input,Hash) && Hash==FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_BUILDING_ASSEMBLY_VIEW_SHA256")) &&
        FFileHelper::LoadFileToString(Text,*Input) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),D))) return false;
    const FString Filename=D->GetStringField(TEXT("map")),PackageName=D->GetStringField(TEXT("package")),Root=D->GetStringField(TEXT("output"));
    if (!TestTrue(TEXT("Saved rendering reuses exact prior scene bytes and a fresh output."),
        FPaths::IsUnderDirectory(Filename,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
        FProjectWorldCanonicalLoader::ComputeFileSha256(Filename,Hash) && Hash==D->GetStringField(TEXT("map_sha256")) &&
        FPaths::IsUnderDirectory(Root,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
        !FPaths::DirectoryExists(Root) && IFileManager::Get().MakeDirectory(*Root,true))) return false;
    const FString SourceMount=FPackageName::GetLongPackagePath(PackageName)+TEXT("/");
    FPackageName::RegisterMountPoint(SourceMount,FPaths::GetPath(Filename)+TEXT("/"));
    const FString ViewMount=TEXT("/ProjectBuildingView")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT("/");
    const FString ViewDirectory=Root/TEXT("scene/");IFileManager::Get().MakeDirectory(*ViewDirectory,true);
    FPackageName::RegisterMountPoint(ViewMount,ViewDirectory);
    ON_SCOPE_EXIT {FAutomationEditorCommonUtils::CreateNewMap();FPackageName::UnRegisterMountPoint(ViewMount,ViewDirectory);FPackageName::UnRegisterMountPoint(SourceMount,FPaths::GetPath(Filename)+TEXT("/"));};
    if (!TestTrue(TEXT("The exact saved source scene opens without rebuilding geography or buildings."),FEditorFileUtils::LoadMap(Filename,false,false))) return false;
    auto* World=GEditor->GetEditorWorldContext().World();AActor* Building=nullptr;
    const FName Feature(TEXT("Project.BuildingAssembly.Feature=alis:osm:way:250440683"));
    for (TActorIterator<AActor> It(World);It;++It) if (It->Tags.Contains(Feature)) Building=*It;
    if (!TestNotNull(TEXT("The retained generated feature exists in the authenticated map."),Building)) return false;
    auto Receipt=MakeShared<FJsonObject>();auto Volumes=MakeShared<FJsonObject>();
    if (!TestTrue(TEXT("Saved output retains independently outward modules."),OutwardModules(*Building,.12,*Volumes))) return false;
    Receipt->SetObjectField(TEXT("unit_volumes_cm3"),Volumes);Receipt->SetStringField(TEXT("source_map_sha256"),Hash);
    TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(Building);const auto Bounds=Building->GetComponentsBoundingBox();
    const FString DefaultRoot=Root/TEXT("default"),NeutralRoot=Root/TEXT("neutral");
    IFileManager::Get().MakeDirectory(*DefaultRoot,true);IFileManager::Get().MakeDirectory(*NeutralRoot,true);
    auto Original=MakeShared<FJsonObject>();const bool OriginalAccepted=Capture(*World,*Building,Bounds.GetCenter(),DefaultRoot/TEXT("building.png"),*Original);
    Receipt->SetObjectField(TEXT("default_view"),Original);Receipt->SetStringField(TEXT("default_material"),Parts[0]->GetMaterial(0)->GetPathName());
    Receipt->SetStringField(TEXT("default_disposition"),OriginalAccepted ? TEXT("accepted") : TEXT("insufficient_visible_lit_coverage"));
    auto* Neutral=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!TestNotNull(TEXT("Neutral preview references separately installed Engine content."),Neutral)) return false;
    for (auto* Part : Parts) Part->SetMaterial(0,Neutral);
    auto NeutralEvidence=MakeShared<FJsonObject>();const bool NeutralAccepted=Capture(*World,*Building,Bounds.GetCenter(),NeutralRoot/TEXT("building.png"),*NeutralEvidence);
    Receipt->SetObjectField(TEXT("neutral_view"),NeutralEvidence);Receipt->SetStringField(TEXT("neutral_material"),Neutral->GetPathName());
    FString Output;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Output));FFileHelper::SaveStringToFile(Output,*(Root/TEXT("receipt.json")));
    if (!TestTrue(TEXT("The neutral comparison renders the same saved geometry clearly."),NeutralAccepted)) return false;
    const FString CandidateFile=ViewDirectory/TEXT("NeutralBuilding.umap");
    if (!TestTrue(TEXT("Neutral presentation saves as a separate disposable candidate."),FEditorFileUtils::SaveMap(World,CandidateFile))) return false;
    const FString CandidatePackage=World->GetPackage()->GetName();
    TMap<FName,TArray<FTransform>> ExpectedInstances;int ExpectedCount=0;
    for (auto* Part : Parts) for (int I=0;I<Part->GetInstanceCount();++I) {FTransform T;Part->GetInstanceTransform(I,T,true);ExpectedInstances.FindOrAdd(Part->GetFName()).Add(T);++ExpectedCount;}
    TWeakObjectPtr<AActor> Old=Building;Parts.Reset();Building=nullptr;
    FAutomationEditorCommonUtils::CreateNewMap();
    // Map replacement can collect the old package; never retain its raw pointer across it.
    if (auto* RemainingPackage=FindPackage(nullptr,*CandidatePackage))
    {
        UPackageTools::FUnloadPackageParams Unload({RemainingPackage});Unload.bResetTransBuffer=false;
        if (!TestTrue(TEXT("A retained preview package explicitly unloads."),UPackageTools::UnloadPackages(Unload))) return false;
    }
    if (!TestFalse(TEXT("Preview instances are released before native reload."),Old.IsValid())) return false;
    if (!TestTrue(TEXT("The separately saved neutral candidate opens."),FEditorFileUtils::LoadMap(CandidateFile,false,false))) return false;
    World=GEditor->GetEditorWorldContext().World();for (TActorIterator<AActor> It(World);It;++It) if (It->Tags.Contains(Feature)) Building=*It;
    if (!TestNotNull(TEXT("The same feature reloads with neutral presentation."),Building)) return false;
    Neutral=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    TInlineComponentArray<UInstancedStaticMeshComponent*> LoadedParts(Building);int Count=0;
    for (auto* Part : LoadedParts)
    {
        if (!TestTrue(TEXT("Neutral material reference persists on every module family."),Part->GetMaterial(0)==Neutral)) return false;
        const auto* Expected=ExpectedInstances.Find(Part->GetFName());
        for (int I=0;I<Part->GetInstanceCount();++I)
        {FTransform T;Part->GetInstanceTransform(I,T,true);if (!TestTrue(TEXT("Preview save preserves each actual world transform."),Expected && Expected->IsValidIndex(I) && T.Equals((*Expected)[I],1.e-6))) return false;++Count;}
    }
    if (!TestEqual(TEXT("Preview preserves the full instance census."),Count,ExpectedCount)) return false;
    auto Reloaded=MakeShared<FJsonObject>();
    if (!TestTrue(TEXT("The reloaded neutral candidate renders clearly."),Capture(*World,*Building,Building->GetComponentsBoundingBox().GetCenter(),Root/TEXT("building.png"),*Reloaded))) return false;
    if (!TestTrue(TEXT("The source map remains byte-identical after material comparison and save-as."),FProjectWorldCanonicalLoader::ComputeFileSha256(Filename,Hash) && Hash==D->GetStringField(TEXT("map_sha256")))) return false;
    Receipt->SetObjectField(TEXT("reloaded_view"),Reloaded);Receipt->SetNumberField(TEXT("instances"),Count);
    Receipt->SetStringField(TEXT("candidate_map"),CandidateFile);Receipt->SetStringField(TEXT("candidate_package"),CandidatePackage);
    FProjectWorldCanonicalLoader::ComputeFileSha256(CandidateFile,Hash);Receipt->SetStringField(TEXT("candidate_map_sha256"),Hash);
    FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Output));return FFileHelper::SaveStringToFile(Output,*(Root/TEXT("receipt.json")));
}
#endif
