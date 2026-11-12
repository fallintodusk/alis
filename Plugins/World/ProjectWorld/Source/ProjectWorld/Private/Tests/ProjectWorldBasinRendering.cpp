// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldBasinRendering.h"
#include "Presentation/ProjectWorldWaterVisibilityCapture.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Utilities/ProjectSha256.h"
#include "PrimitiveSceneProxy.h"
#include "Rendering/NaniteResources.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
    bool ObservedView(const FProjectWorldWaterDepthObservation& O,double LeafCompatibility,FString& Error)
    {
        Error.Reset();
        if (!FMath::IsFinite(LeafCompatibility) || LeafCompatibility<0 || !FMath::IsFinite(O.MaximumBoundaryPrecision) || O.MaximumBoundaryPrecision<0)
        {Error=TEXT("unknown_compatibility_measurement");return false;}
        if (O.Candidates<=0 || O.InteriorCandidates<=0) {Error=TEXT("empty_discrete_interior");return false;}
        if (O.InteriorVisible!=O.InteriorCandidates) {Error=TEXT("discrete_interior_occlusion");return false;}
        if (O.Visible<0 || O.Visible>O.Candidates || O.InteriorCandidates>O.Candidates || O.BoundaryMisses!=O.Candidates-O.Visible)
        {Error=TEXT("inconsistent_discrete_census");return false;}
        if (!FMath::IsFinite(O.MaximumBoundaryDelta) || O.MaximumBoundaryDelta>LeafCompatibility+O.MaximumBoundaryPrecision)
        {Error=TEXT("boundary_depth_envelope");return false;}
        int32 Recorded=0;
        for (const auto& P : O.Samples) Recorded+=P.CandidateNeighbours<8 && FMath::Abs(P.WaterDepth-P.SceneDepth)>P.Precision ? 1 : 0;
        if (Recorded!=O.BoundaryMisses) {Error=TEXT("incomplete_boundary_witnesses");return false;}
        return true;
    }
}
struct FProjectWorldBasinRenderProbe::FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStaticMeshActor> Water,Occluder;
    TWeakObjectPtr<ADirectionalLight> Light;
    FProjectWorldWaterVisibilityCapture Mask;
    ProjectWorldRuntimeScreenshotCapture::FCaptureSession Beauty;
    ProjectWorldRuntimeScreenshotCapture::FCaptureSpec Spec;
    FBox Bounds;
    FString Root,NearMaskHash;
    TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>,bool>> RenderModes;
    TArray<int32> WitnessPixels;
    FProjectWorldWaterDepthObservation LastObservation,NearObservation;
    double LeafCompatibility=0;
    int32 Phase=0,NearPixels=0,NearCandidates=0,FarPixels=0,ComparePhase=0,CompareFrames=0;
    bool FallbackCompared=false;
    uint64 LastFrame=0,FirstFrame=0;
    double Started=0;
    ~FState()
    {
        Beauty.Reset();Mask.Reset();
        for (const auto& Mode : RenderModes) if (Mode.Key.IsValid()) Mode.Key->SetForceDisableNanite(Mode.Value);
        if (Occluder.IsValid()) Occluder->Destroy();
        if (Light.IsValid()) Light->Destroy();
    }
};
FProjectWorldBasinRenderProbe::FProjectWorldBasinRenderProbe()=default;
FProjectWorldBasinRenderProbe::~FProjectWorldBasinRenderProbe()=default;
void FProjectWorldBasinRenderProbe::Reset() {State.Reset();}
bool FProjectWorldBasinRenderProbe::Begin(UWorld& World,const FBox& Bounds,TConstArrayView<UStaticMeshComponent*> Terrain,double MaximumStoredGradient,const FString& Root,FString& Error)
{
    Reset();Error.Reset();const FString Absolute=FPaths::ConvertRelativePathToFull(Root);
    if (World.WorldType!=EWorldType::PIE || !Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN() ||
        Bounds.GetSize().X<=0 || Bounds.GetSize().Y<=0 || !FMath::IsFinite(MaximumStoredGradient) || MaximumStoredGradient<0 ||
        !FPaths::IsUnderDirectory(Absolute,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))
    {Error=TEXT("Basin rendering requires the disposable PIE scene, finite complete Water bounds and project scratch.");return false;}
    auto Candidate=MakeUnique<FState>();Candidate->World=&World;Candidate->Bounds=Bounds;Candidate->Root=Absolute;
    int32 Count=0;
    for (TActorIterator<AStaticMeshActor> It(&World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.Water.v1"))) {Candidate->Water=*It;++Count;}
    if (Count!=1) {Error=TEXT("Basin render proof requires the exact one saved Water actor.");return false;}
    if (Terrain.IsEmpty()) {Error=TEXT("Basin rendering requires its explicitly selected native Terrain components.");return false;}
    TSet<const UStaticMeshComponent*> Selected;
    for (auto* Component : Terrain)
    {
        if (!Component || Selected.Contains(Component) || !Component->IsRegistered() || Component->GetWorld()!=&World ||
            !Component->GetSceneProxy() || !Component->GetSceneProxy()->IsNaniteMesh() ||
            !Component->GetStaticMesh() || !Component->GetStaticMesh()->HasValidNaniteData())
        {Error=TEXT("The selected Terrain rendering component is not the actual saved Nanite surface.");return false;}
        Selected.Add(Component);
        UStaticMesh* Mesh=Component->GetStaticMesh();const int32 Precision=Mesh->GetRenderData()->NaniteResourcesPtr->PositionPrecision;
        const double Q=FMath::Exp2(-double(Precision));const FBox Box=Mesh->GetBoundingBox();
        const double MaximumInput=FMath::Max(Box.Min.GetAbsMax(),Box.Max.GetAbsMax());
        if (!FMath::IsFinite(Q) || Q<=0 || MaximumInput/Q+.5>=8388608 || !Component->GetComponentTransform().Equals(FTransform::Identity,0))
        {Error=TEXT("This observed-view leaf compatibility recipe requires identity-frame nearest-safe native position encoding.");return false;}
        Candidate->LeafCompatibility=FMath::Max(Candidate->LeafCompatibility,Q*.5*(1+MaximumStoredGradient*FMath::Sqrt(2.)));
        Candidate->RenderModes.Add({Component,Component->IsForceDisableNanite()});
    }
    FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;
    auto* Light=World.SpawnActor<ADirectionalLight>(FVector::ZeroVector,FRotator(-45,-30,0),Spawn);
    if (!Light) {Error=TEXT("Cannot create owned disposable view lighting.");return false;}
    Candidate->Light=Light;Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Light->GetLightComponent()->SetIntensity(10);
    Candidate->Spec.CameraLocation=Bounds.GetCenter()+FVector(0,0,2000);Candidate->Spec.CameraRotation=FRotator(-90,0,0);
    Candidate->Spec.OrthographicWidthCentimeters=FMath::Max(Bounds.GetSize().X,Bounds.GetSize().Y*1920./1080.)*1.35;
    Candidate->Spec.SourceIdentity=World.GetPathName()+TEXT("/identified_basin_near");
    Candidate->Started=FPlatformTime::Seconds();Candidate->FirstFrame=GFrameCounter;State=MoveTemp(Candidate);return true;
}
EProjectWorldBasinRenderProgress FProjectWorldBasinRenderProbe::Step(FJsonObject& Receipt,FString& Error)
{
    Error.Reset();
    const auto Fail=[&](const FString& Reason)
    {Error=Reason;Reset();return EProjectWorldBasinRenderProgress::Failed;};
    if (!State || !State->World.IsValid() || !State->Water.IsValid()) return Fail(TEXT("Basin render proof lost the actual saved scene/Water."));
    auto& S=*State;auto& World=*S.World.Get();
    if (FPlatformTime::Seconds()-S.Started>120 || GFrameCounter-S.FirstFrame>600) return Fail(TEXT("Basin render proof exceeded its native-frame budget."));
    if (S.LastFrame==GFrameCounter) return EProjectWorldBasinRenderProgress::Pending;S.LastFrame=GFrameCounter;
    const auto StartViews=[&]()
    {return S.Mask.Initialize(World,S.Spec,Error) && S.Beauty.Initialize(World,S.Spec,Error) && S.Mask.WarmUp(3,Error) && S.Beauty.WarmUp(3,Error);};
    const auto Mask=[&](const TCHAR* Name)
    {
        FProjectWorldWaterDepthObservation O;O.WitnessPixels=S.WitnessPixels;const bool Captured=S.Mask.Capture(S.Root/Name,Error,&O);
        S.LastObservation=O;
        if (S.WitnessPixels.IsEmpty() && FString(Name)==TEXT("near-water.png")) for (const auto& Sample : O.Samples) S.WitnessPixels.Add(Sample.Pixel);
        auto Observation=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Missing;
        for (const auto& Sample : O.Samples)
        {
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("pixel_x"),Sample.Pixel%1920);Row->SetNumberField(TEXT("pixel_y"),Sample.Pixel/1920);
            Row->SetNumberField(TEXT("water_depth_cm"),Sample.WaterDepth);Row->SetNumberField(TEXT("scene_depth_cm"),Sample.SceneDepth);
            Row->SetNumberField(TEXT("precision_cm"),Sample.Precision);Missing.Add(MakeShared<FJsonValueObject>(Row));
            Row->SetNumberField(TEXT("candidate_neighbours"),Sample.CandidateNeighbours);
        }
        Observation->SetArrayField(TEXT("depth_samples"),Missing);Observation->SetNumberField(TEXT("visible_pixels"),S.Mask.GetVisiblePixelCount());
        Observation->SetNumberField(TEXT("candidate_pixels"),S.Mask.GetCandidatePixelCount());
        Observation->SetNumberField(TEXT("eroded_interior_candidates"),O.InteriorCandidates);Observation->SetNumberField(TEXT("eroded_interior_visible"),O.InteriorVisible);
        Observation->SetNumberField(TEXT("boundary_missing_pixels"),O.BoundaryMisses);Observation->SetNumberField(TEXT("maximum_boundary_depth_difference_cm"),O.MaximumBoundaryDelta);
        Observation->SetNumberField(TEXT("maximum_boundary_numeric_precision_cm"),O.MaximumBoundaryPrecision);
        Observation->SetNumberField(TEXT("conditional_leaf_codec_compatibility_cm"),S.LeafCompatibility);
        Observation->SetNumberField(TEXT("orthographic_width_cm"),S.Spec.OrthographicWidthCentimeters);
        TArray<TSharedPtr<FJsonValue>> Camera;
        for (double V : {S.Spec.CameraLocation.X,S.Spec.CameraLocation.Y,S.Spec.CameraLocation.Z}) Camera.Add(MakeShared<FJsonValueNumber>(V));
        Observation->SetArrayField(TEXT("camera_world_cm"),Camera);Observation->SetStringField(TEXT("camera_rotation"),S.Spec.CameraRotation.ToString());
        Receipt.SetObjectField(FString(TEXT("render_depth_"))+Name,Observation);return Captured;
    };
    const auto ProxyKind=[&](bool Nanite)
    {
        if (S.RenderModes.IsEmpty()) return false;
        for (const auto& Mode : S.RenderModes)
            if (!Mode.Key.IsValid() || !Mode.Key->GetSceneProxy() || Mode.Key->GetSceneProxy()->IsNaniteMesh()!=Nanite) return false;
        return true;
    };
    const auto Control=[&](const FVector& Location,const FVector& Scale)
    {
        FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;
        auto* Actor=World.SpawnActor<AStaticMeshActor>(Location,FRotator::ZeroRotator,Spawn);
        if (!Actor) {Error=TEXT("Cannot create an owned opaque visibility control.");return false;}
        S.Occluder=Actor;auto* Component=Actor->GetStaticMeshComponent();Component->SetMobility(EComponentMobility::Movable);
        auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!Cube) {Error=TEXT("The native opaque control mesh is unavailable.");return false;}
        Component->SetStaticMesh(Cube);Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->SetCanEverAffectNavigation(false);
        Actor->SetActorScale3D(Scale);World.SendAllEndOfFrameUpdates();return true;
    };
    if (S.ComparePhase>0)
    {
        if (++S.CompareFrames<5) return EProjectWorldBasinRenderProgress::Pending;S.CompareFrames=0;
        if (S.ComparePhase==1)
        {
            if (!ProxyKind(false)) return Fail(TEXT("Fallback comparison did not select actual native fallback proxies."));
            if (!Mask(TEXT("near-fallback-water.png"))) return Fail(Error);
            Receipt.SetNumberField(TEXT("fallback_visible_water_pixels"),S.Mask.GetVisiblePixelCount());
            Receipt.SetNumberField(TEXT("fallback_candidate_water_pixels"),S.Mask.GetCandidatePixelCount());
            for (const auto& Mode : S.RenderModes) Mode.Key->SetForceDisableNanite(Mode.Value);
            World.SendAllEndOfFrameUpdates();S.ComparePhase=2;return EProjectWorldBasinRenderProgress::Pending;
        }
        if (!ProxyKind(true)) return Fail(TEXT("Native comparison did not restore actual Nanite proxies."));
        FString Restored;
        if (!Mask(TEXT("near-native-restored-water.png")) || !FProjectSha256::HashFile(S.Root/TEXT("near-native-restored-water.png"),Restored)) return Fail(Error);
        if (S.Mask.GetVisiblePixelCount()!=S.NearPixels || S.Mask.GetCandidatePixelCount()!=S.NearCandidates || Restored!=S.NearMaskHash)
            return Fail(TEXT("Native/fallback/native comparison failed to restore the complete original native mask."));
        S.ComparePhase=0;S.FallbackCompared=true;return EProjectWorldBasinRenderProgress::Pending;
    }
    switch (S.Phase++)
    {
    case 0:
        if (!StartViews()) return Fail(Error);break;
    case 1: break;
    case 2:
        if (!Mask(TEXT("near-water.png"))) return Fail(Error);
        S.NearPixels=S.Mask.GetVisiblePixelCount();
        S.NearCandidates=S.Mask.GetCandidatePixelCount();
        S.NearObservation=S.LastObservation;
        if (S.NearCandidates<=0 || S.NearCandidates>=1920*1080 || !FProjectSha256::HashFile(S.Root/TEXT("near-water.png"),S.NearMaskHash))
            return Fail(FString::Printf(TEXT("Near Water support incomplete: visible=%d candidates=%d width_cm=%.9g."),S.NearPixels,S.Mask.GetCandidatePixelCount(),S.Spec.OrthographicWidthCentimeters));
        break;
    case 3:
    {
        FString Repeat;
        if (!Mask(TEXT("near-repeat-water.png")) || !FProjectSha256::HashFile(S.Root/TEXT("near-repeat-water.png"),Repeat)) return Fail(Error);
        if (S.Mask.GetVisiblePixelCount()!=S.NearPixels || Repeat!=S.NearMaskHash) return Fail(TEXT("The same near view changes visible Water across actual native frames."));
        if (!S.FallbackCompared)
        {
            TArray<TSharedPtr<FJsonValue>> Encodings;
            for (const auto& Mode : S.RenderModes)
            {
                if (!Mode.Key.IsValid()) return Fail(TEXT("The selected saved Terrain component disappeared before native comparison."));
                UStaticMesh* Mesh=Mode.Key->GetStaticMesh();auto Row=MakeShared<FJsonObject>();
                Row->SetStringField(TEXT("mesh"),Mesh->GetPathName());
                Row->SetStringField(TEXT("component"),Mode.Key->GetPathName());
                Row->SetNumberField(TEXT("encoded_position_precision"),Mesh->GetRenderData()->NaniteResourcesPtr->PositionPrecision);
                Encodings.Add(MakeShared<FJsonValueObject>(Row));
            }
            Receipt.SetArrayField(TEXT("native_nanite_encodings"),Encodings);
            if (!ProxyKind(true)) return Fail(TEXT("The near baseline requires actual saved Nanite proxies."));
            for (const auto& Mode : S.RenderModes) Mode.Key->SetForceDisableNanite(true);
            World.SendAllEndOfFrameUpdates();S.ComparePhase=1;S.CompareFrames=0;S.Phase=3;return EProjectWorldBasinRenderProgress::Pending;
        }
        if (!ObservedView(S.LastObservation,S.LeafCompatibility,Error)) return Fail(Error);
        auto Empty=S.LastObservation;Empty.InteriorCandidates=Empty.InteriorVisible=0;
        if (ObservedView(Empty,S.LeafCompatibility,Error) || Error!=TEXT("empty_discrete_interior")) return Fail(TEXT("The discrete observed-view recipe accepted a vacuous interior."));
        if (S.LastObservation.BoundaryMisses>0)
        {
            auto Incomplete=S.LastObservation;
            const int32 Missing=Incomplete.Samples.IndexOfByPredicate([](const auto& P) {return P.CandidateNeighbours<8 && FMath::Abs(P.WaterDepth-P.SceneDepth)>P.Precision;});
            if (Missing==INDEX_NONE) return Fail(TEXT("A reported boundary miss has no measured witness."));
            Incomplete.Samples.RemoveAt(Missing);
            if (ObservedView(Incomplete,S.LeafCompatibility,Error) || Error!=TEXT("incomplete_boundary_witnesses"))
                return Fail(TEXT("The discrete observed-view recipe accepted an incomplete boundary witness census."));
        }
        Error.Reset();
        if (Receipt.GetNumberField(TEXT("fallback_visible_water_pixels"))!=S.NearCandidates || Receipt.GetNumberField(TEXT("fallback_candidate_water_pixels"))!=S.NearCandidates)
            return Fail(TEXT("The actual native fallback must also retain the complete specified Water field."));
        if (!S.Beauty.Capture(S.Root/TEXT("near.png"),Error)) return Fail(Error);
        if (!Control(S.Bounds.GetCenter()+FVector(0,0,50),FVector(S.Bounds.GetSize().X/100.+1,S.Bounds.GetSize().Y/100.+1,.05))) return Fail(Error);
        break;
    }
    case 4:
        if (!Mask(TEXT("occluded-water.png"))) return Fail(Error);
        if (S.Mask.GetVisiblePixelCount()!=0 || S.Mask.GetCandidatePixelCount()!=S.NearCandidates ||
            ObservedView(S.LastObservation,S.LeafCompatibility,Error) || Error!=TEXT("discrete_interior_occlusion"))
            return Fail(TEXT("The actual opaque/interior-height control must refuse the observed-view recipe while retaining candidates."));
        Error.Reset();
        S.Occluder->Destroy();S.Occluder.Reset();World.SendAllEndOfFrameUpdates();break;
    case 5:
    {
        FString Restored;
        if (!Mask(TEXT("restored-water.png"))) return Fail(Error);
        if (!FProjectSha256::HashFile(S.Root/TEXT("restored-water.png"),Restored) || S.Mask.GetVisiblePixelCount()!=S.NearPixels || Restored!=S.NearMaskHash)
            return Fail(TEXT("Removing the opaque control must restore the complete original Water mask."));
        const int32 Pixel=S.WitnessPixels.IsEmpty() ? S.NearObservation.FirstBoundaryPixel : S.WitnessPixels[0];
        if (Pixel<0) return Fail(TEXT("The native boundary-depth control needs an actual boundary candidate."));
        const double Pitch=S.Spec.OrthographicWidthCentimeters/1920.;
        const FVector Location(S.Spec.CameraLocation.X+(540.-(Pixel/1920+.5))*Pitch,
            S.Spec.CameraLocation.Y+(Pixel%1920+.5-960.)*Pitch,S.Bounds.GetCenter().Z+1);
        if (!Control(Location,FVector(Pitch/100.,Pitch/100.,.01))) return Fail(Error);
        break;
    }
    case 6:
        if (!Mask(TEXT("boundary-excess-water.png"))) return Fail(Error);
        if (S.LastObservation.InteriorVisible!=S.LastObservation.InteriorCandidates || S.LastObservation.Candidates!=S.NearCandidates ||
            ObservedView(S.LastObservation,S.LeafCompatibility,Error) || Error!=TEXT("boundary_depth_envelope"))
            return Fail(TEXT("The actual boundary-height control must preserve the discrete interior and refuse boundary depth excess."));
        Receipt.SetNumberField(TEXT("boundary_control_maximum_depth_difference_cm"),S.LastObservation.MaximumBoundaryDelta);
        Error.Reset();S.Occluder->Destroy();S.Occluder.Reset();World.SendAllEndOfFrameUpdates();break;
    case 7:
    {
        FString Restored;
        if (!Mask(TEXT("boundary-restored-water.png")) || !FProjectSha256::HashFile(S.Root/TEXT("boundary-restored-water.png"),Restored)) return Fail(Error);
        if (Restored!=S.NearMaskHash || !ObservedView(S.LastObservation,S.LeafCompatibility,Error)) return Fail(TEXT("Removing the boundary-height control must restore the original observed view."));
        S.Mask.Reset();S.Beauty.Reset();S.Spec.OrthographicWidthCentimeters*=2;
        S.Spec.CameraLocation.Z+=2000;S.Spec.SourceIdentity=World.GetPathName()+TEXT("/identified_basin_far");
        if (!StartViews()) return Fail(Error);break;
    }
    case 8: break;
    case 9:
        if (!Mask(TEXT("far-water.png")) || !S.Beauty.Capture(S.Root/TEXT("far.png"),Error)) return Fail(Error);
        S.FarPixels=S.Mask.GetVisiblePixelCount();
        if (S.FarPixels<=0 || S.FarPixels>=S.NearPixels || !ObservedView(S.LastObservation,S.LeafCompatibility,Error)) return Fail(FString(TEXT("The distant observed-view recipe refuses: "))+Error);
        break;
    case 10:
    {
        S.Mask.Reset();auto Water=S.Water;Water->Tags.Remove(TEXT("ProjectWorld.Water.v1"));
        ON_SCOPE_EXIT {if (Water.IsValid()) Water->Tags.Add(TEXT("ProjectWorld.Water.v1"));};
        if (S.Mask.Initialize(World,S.Spec,Error) || Error!=TEXT("No registered canonical Water components support this depth capture."))
            return Fail(TEXT("Missing canonical Water must refuse with its exact source-membership reason."));
        Error.Reset();Receipt.SetNumberField(TEXT("near_visible_water_pixels"),S.NearPixels);Receipt.SetNumberField(TEXT("far_visible_water_pixels"),S.FarPixels);
        Receipt.SetNumberField(TEXT("opaque_control_visible_water_pixels"),0);Receipt.SetStringField(TEXT("near_water_mask_sha256"),S.NearMaskHash);
        Receipt.SetNumberField(TEXT("render_native_frame_span"),GFrameCounter-S.FirstFrame);
        Receipt.SetNumberField(TEXT("near_full_candidate_missing_pixels"),S.NearCandidates-S.NearPixels);
        Receipt.SetStringField(TEXT("full_candidate_identity_result"),S.NearCandidates==S.NearPixels ? TEXT("near_complete") : TEXT("near_refused_retained"));
        Receipt.SetStringField(TEXT("render_scope"),TEXT("Observed same-array eight-neighbour-eroded candidate agreement with reported boundary residuals, conditional leaf-codec compatibility, actual controls and near/far FinalColor; no continuous footprint, actual LOD/global geometry bound, universal pixel identity, geographic/source-accuracy or packaged acceptance."));
        return EProjectWorldBasinRenderProgress::Passed;
    }
    }
    return EProjectWorldBasinRenderProgress::Pending;
}
#endif
