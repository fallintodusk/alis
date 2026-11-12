// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldCoastalView.h"
#include "Presentation/ProjectWorldWaterVisibilityCapture.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Utilities/ProjectSha256.h"
#include "PrimitiveSceneProxy.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
struct FProjectWorldCoastalView::FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AStaticMeshActor> Control;TWeakObjectPtr<ADirectionalLight> Light;
    FProjectWorldWaterVisibilityCapture Mask;ProjectWorldRuntimeScreenshotCapture::FCaptureSession Beauty;
    ProjectWorldRuntimeScreenshotCapture::FCaptureSpec Spec;
    TArray<int32> Visible,Occluded;TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>,bool>> Modes;
    FString Root,NativeHash,DomainHash;int32 Phase=0,Candidates=0,NativeVisible=0;uint64 Frame=0,Ready=0;double Started=0;
    ~FState() {Mask.Reset();Beauty.Reset();for (const auto& Mode : Modes) if (Mode.Key.IsValid()) Mode.Key->SetForceDisableNanite(Mode.Value);
        if (Control.IsValid()) Control->Destroy();if (Light.IsValid()) Light->Destroy();}
};
FProjectWorldCoastalView::FProjectWorldCoastalView()=default;
FProjectWorldCoastalView::~FProjectWorldCoastalView()=default;
void FProjectWorldCoastalView::Reset() {State.Reset();}
bool FProjectWorldCoastalView::Begin(UWorld& World,const FVector& Camera,double Width,TConstArrayView<int32> Visible,
    TConstArrayView<int32> Occluded,TConstArrayView<UStaticMeshComponent*> Terrain,const FString& Root,FString& Error)
{
    Reset();Error.Reset();
    if (World.WorldType!=EWorldType::PIE || Camera.ContainsNaN() || !FMath::IsFinite(Width) || Width<=0 || Visible.IsEmpty() || Occluded.IsEmpty() ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Root),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))
    {Error=TEXT("Coastal view requires a finite disposable PIE camera and both source-bound witness classes.");return false;}
    auto S=MakeUnique<FState>();S->World=&World;S->Root=Root;S->Visible.Append(Visible);S->Occluded.Append(Occluded);TSet<int32> Seen;
    for (const auto& Points : {S->Visible,S->Occluded}) for (int32 Pixel : Points)
    {if (Pixel<0 || Pixel>=1920*1080 || Seen.Contains(Pixel)) {Error=TEXT("Coastal pixel witnesses must be unique and in frame.");return false;}Seen.Add(Pixel);}
    for (auto* C : Terrain)
    {
        if (!C || C->GetWorld()!=&World || !C->IsRegistered() || !C->GetSceneProxy() || !C->GetSceneProxy()->IsNaniteMesh() ||
            !C->GetStaticMesh() || !C->GetStaticMesh()->HasValidNaniteData()) {Error=TEXT("Coastal view lost its actual native Nanite inventory.");return false;}
        S->Modes.Add({C,C->IsForceDisableNanite()});
    }
    if (S->Modes.IsEmpty()) {Error=TEXT("Coastal view needs positive native Terrain inventory.");return false;}
    S->Spec.CameraLocation=Camera;S->Spec.CameraRotation=FRotator(-90,0,0);S->Spec.OrthographicWidthCentimeters=Width;
    S->Spec.bIsolateBaseColor=false;S->Spec.SourceIdentity=World.GetPathName()+TEXT("/coastal_joined_view");
    FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;
    auto* Light=World.SpawnActor<ADirectionalLight>(FVector::ZeroVector,FRotator(-45,-30,0),Spawn);
    if (!Light) {Error=TEXT("Cannot create owned ordinary coastal-view lighting.");return false;}
    S->Light=Light;Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);Light->GetLightComponent()->SetIntensity(10);
    if (!S->Mask.Initialize(World,S->Spec,Error) || !S->Beauty.Initialize(World,S->Spec,Error) ||
        !S->Mask.WarmUp(3,Error) || !S->Beauty.WarmUp(3,Error)) return false;
    S->Started=FPlatformTime::Seconds();S->Ready=GFrameCounter+5;State=MoveTemp(S);return true;
}
bool FProjectWorldCoastalView::Step(FJsonObject& Receipt,FString& Error)
{
    Error.Reset();const auto Fail=[&](const FString& Message){Error=Message;Reset();return true;};
    if (!State || !State->World.IsValid()) return Fail(TEXT("Coastal view lost its owned world."));auto& S=*State;
    if (FPlatformTime::Seconds()-S.Started>120) return Fail(TEXT("Coastal view exceeded its capture budget."));
    if (S.Frame==GFrameCounter || GFrameCounter<S.Ready) return false;S.Frame=GFrameCounter;
    for (const auto& Mode : S.Modes)
        if (!Mode.Key.IsValid() || !Mode.Key->GetSceneProxy() || Mode.Key->GetSceneProxy()->IsNaniteMesh()!=(S.Phase!=1))
            return Fail(TEXT("Coastal native/fallback comparison has the wrong actual render proxy."));
    const FString Label=FString::Printf(TEXT("phase-%d"),S.Phase);FProjectWorldWaterDepthObservation O;
    O.WitnessPixels=S.Visible;O.WitnessPixels.Append(S.Occluded);
    if (!S.Mask.Capture(S.Root/(Label+TEXT("-water.png")),Error,&O)) {const auto Why=Error;return Fail(Why);}
    int32 VisibleMatches=0,OccludedMatches=0;bool SelectedPixelControl=true;TArray<TSharedPtr<FJsonValue>> Witnesses;
    for (int32 Pixel : O.WitnessPixels)
    {
        const auto* P=O.RequestedSamples.FindByPredicate([&](const auto& V){return V.Pixel==Pixel;});
        if (!P || P->CandidateNeighbours!=8) return Fail(TEXT("Declared source-bound Water pixel is absent or on a raster boundary."));
        const bool Matches=FMath::Abs(P->WaterDepth-P->SceneDepth)<=P->Precision;
        if (S.Phase==5 && S.Visible.Contains(Pixel))
            SelectedPixelControl &= Pixel==S.Visible[0] ? !Matches : Matches;
        if (S.Visible.Contains(Pixel)) VisibleMatches+=Matches;else OccludedMatches+=!Matches;
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("pixel"),Pixel);Row->SetBoolField(TEXT("water_visible"),Matches);
        Row->SetNumberField(TEXT("water_depth_cm"),P->WaterDepth);Row->SetNumberField(TEXT("scene_depth_cm"),P->SceneDepth);
        Row->SetNumberField(TEXT("precision_cm"),P->Precision);Witnesses.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("water_candidates"),O.Candidates);Row->SetNumberField(TEXT("visible_water_pixels"),O.Visible);
    Row->SetNumberField(TEXT("expected_visible_matches"),VisibleMatches);Row->SetNumberField(TEXT("expected_occluded_matches"),OccludedMatches);
    Row->SetArrayField(TEXT("source_bound_witnesses"),Witnesses);Receipt.SetObjectField(TEXT("coastal_view_")+Label,Row);
    Row->SetStringField(TEXT("candidate_domain_sha256"),O.CandidateDomainSha256);
    if (S.Phase>0 && (O.CandidateDomainSha256!=S.DomainHash || O.Candidates!=S.Candidates))
        return Fail(TEXT("Coastal observation changed the exact Water-only candidate domain."));
    if (S.Phase==3)
    {if (O.Visible!=0 || O.Candidates!=S.Candidates || VisibleMatches!=0) return Fail(TEXT("Opaque known-bad control failed to remove actual Water visibility."));}
    else if (S.Phase==5)
    {if (!SelectedPixelControl || VisibleMatches!=S.Visible.Num()-1 || OccludedMatches!=S.Occluded.Num()) return Fail(TEXT("Native pixel-footprint control did not occlude the declared pixel only."));}
    else if (O.Candidates<=0 || O.Visible<=0 || VisibleMatches!=S.Visible.Num() || OccludedMatches!=S.Occluded.Num())
        return Fail(TEXT("Native joined view disagrees with declared visible/occluded source witnesses."));
    FString Hash;if (!FProjectSha256::HashFile(S.Root/(Label+TEXT("-water.png")),Hash)) return Fail(TEXT("Cannot authenticate coastal Water mask."));
    if (S.Phase==0)
    {
        S.NativeHash=Hash;S.DomainHash=O.CandidateDomainSha256;S.Candidates=O.Candidates;S.NativeVisible=O.Visible;
        if (!S.Beauty.Capture(S.Root/TEXT("joined-native.png"),Error)) {const auto Why=Error;return Fail(Why);}
        for (const auto& Mode : S.Modes) {if (!Mode.Key.IsValid()) return Fail(TEXT("Coastal view lost native component."));Mode.Key->SetForceDisableNanite(true);}
    }
    else if (S.Phase==1)
    {for (const auto& Mode : S.Modes) {if (!Mode.Key.IsValid()) return Fail(TEXT("Coastal fallback lost native component."));Mode.Key->SetForceDisableNanite(Mode.Value);}}
    else if (S.Phase==2)
    {
        if (Hash!=S.NativeHash) return Fail(TEXT("Native/fallback/native did not restore the complete coastal mask."));
        auto* Control=S.World->SpawnActor<AStaticMeshActor>();if (!Control) return Fail(TEXT("Cannot create coastal visibility control."));S.Control=Control;
        auto* C=Control->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);
        auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));if (!Cube) return Fail(TEXT("Native opaque control mesh is unavailable."));
        C->SetStaticMesh(Cube);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
        Control->SetActorLocation(S.Spec.CameraLocation-FVector(0,0,100));Control->SetActorScale3D(FVector(S.Spec.OrthographicWidthCentimeters/100.+2,S.Spec.OrthographicWidthCentimeters/100.+2,.1));
    }
    else if (S.Phase==3) {S.Control->Destroy();S.Control.Reset();}
    else if (S.Phase==4)
    {
        if (Hash!=S.NativeHash || O.Visible!=S.NativeVisible) return Fail(TEXT("Coastal mask did not restore after the opaque control."));
        const int32 Pixel=S.Visible[0];const auto* Sample=O.RequestedSamples.FindByPredicate([&](const auto& P){return P.Pixel==Pixel;});
        if (!Sample) return Fail(TEXT("Pixel projection control requires a measured candidate."));const double Pitch=S.Spec.OrthographicWidthCentimeters/1920.;
        auto* Control=S.World->SpawnActor<AStaticMeshActor>();if (!Control) return Fail(TEXT("Cannot create pixel projection control."));S.Control=Control;
        auto* C=Control->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);
        auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));if (!Cube) return Fail(TEXT("Native pixel control mesh is unavailable."));
        C->SetStaticMesh(Cube);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
        Control->SetActorLocation(FVector(S.Spec.CameraLocation.X+(540-(Pixel/1920+.5))*Pitch,
            S.Spec.CameraLocation.Y+(Pixel%1920+.5-960)*Pitch,S.Spec.CameraLocation.Z-Sample->WaterDepth+50));
        Control->SetActorScale3D(FVector(Pitch/100.,Pitch/100.,.01));
    }
    else if (S.Phase==5) {S.Control->Destroy();S.Control.Reset();}
    else
    {
        if (Hash!=S.NativeHash || O.Visible!=S.NativeVisible) return Fail(TEXT("Native pixel-control removal did not restore the exact original view."));
        for (const auto& Mode : S.Modes) if (!Mode.Key.IsValid() || Mode.Key->IsForceDisableNanite()!=Mode.Value)
            return Fail(TEXT("Coastal capture failed to restore the original component render flags."));
        Receipt.SetStringField(TEXT("render_flag_cleanup"),TEXT("original_values_observed"));
        Receipt.SetStringField(TEXT("coastal_view_status"),TEXT("source_witnesses_and_controls_passed"));Reset();return true;
    }
    S.World->SendAllEndOfFrameUpdates();S.Ready=GFrameCounter+5;++S.Phase;return false;
}
#endif
