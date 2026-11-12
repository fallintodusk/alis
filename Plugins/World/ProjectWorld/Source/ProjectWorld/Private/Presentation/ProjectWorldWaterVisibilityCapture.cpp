// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Presentation/ProjectWorldWaterVisibilityCapture.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Utilities/ProjectSha256.h"
#include <cfloat>

namespace
{
    TArray<TWeakObjectPtr<UPrimitiveComponent>> RegisteredWater(UWorld& World)
    {
        TArray<TWeakObjectPtr<UPrimitiveComponent>> Result;
        for (TActorIterator<AActor> It(&World);It;++It)
            if (It->ActorHasTag(TEXT("ProjectWorld.Water.v1")))
            {
                TInlineComponentArray<UPrimitiveComponent*> Primitives;
                It->GetComponents(Primitives);
                for (auto* Component : Primitives)
                    if (Component && Component->IsRegistered()) Result.Add(Component);
            }
        return Result;
    }
}

void FProjectWorldWaterVisibilityCapture::Reset()
{
    Scene.Reset();Water.Reset();Components.Reset();CapturedWorld.Reset();MaximumDepth=0;Written=Visible=Candidates=0;
}

bool FProjectWorldWaterVisibilityCapture::Initialize(UWorld& World,
    const ProjectWorldRuntimeScreenshotCapture::FCaptureSpec& Spec,FString& Error)
{
    Reset();
    if (Spec.OrthographicWidthCentimeters<=0 || Spec.CameraLocation.ContainsNaN() ||
        Spec.CameraRotation.ContainsNaN())
    {Error=TEXT("Water visibility requires a finite orthographic camera.");return false;}
    const FVector Forward=Spec.CameraRotation.Vector();
    Components=RegisteredWater(World);
    for (const auto& Weak : Components)
        MaximumDepth=FMath::Max(MaximumDepth,float(FVector::DotProduct(
            Weak->Bounds.Origin-Spec.CameraLocation,Forward)+Weak->Bounds.SphereRadius));
    if (Components.IsEmpty() || !FMath::IsFinite(MaximumDepth) || MaximumDepth<=1)
    {Error=TEXT("No registered canonical Water components support this depth capture.");Reset();return false;}
    auto Full=Spec;
    // Observe the full native Water rendering path and read float depth from A.
    Full.CaptureSource=SCS_SceneColorSceneDepth;Full.bIsolateBaseColor=false;
    Full.bUseShowOnlyComponents=false;Full.ShowOnlyComponents.Reset();
    // Keep the projection clear depth outside every identified Water bound.
    // Otherwise an empty pixel at the far plane can resemble matching Water.
    Full.DepthFarClipCentimeters=MaximumDepth+FMath::Max(100.f,16*FLT_EPSILON*MaximumDepth);
    auto Only=Full;Only.bUseShowOnlyComponents=true;Only.ShowOnlyComponents=Components;
    if (!Scene.Initialize(World,Full,Error) || !Water.Initialize(World,Only,Error)) {Reset();return false;}
    CapturedWorld=&World;
    return true;
}

bool FProjectWorldWaterVisibilityCapture::WarmUp(int32 Frames,FString& Error)
{
    return Scene.WarmUp(Frames,Error) && Water.WarmUp(Frames,Error);
}

bool FProjectWorldWaterVisibilityCapture::Capture(const FString& Path,FString& Error,FProjectWorldWaterDepthObservation* Observation)
{
    Visible=Candidates=0;
    if (Observation)
    {
        Observation->Samples.Reset();Observation->RequestedSamples.Reset();Observation->Candidates=Observation->Visible=Observation->InteriorCandidates=Observation->InteriorVisible=Observation->BoundaryMisses=0;
        Observation->CandidateDomainSha256.Reset();
        if (Observation->WitnessPixels.Num()>128) {Error=TEXT("Requested Water observations exceed the bounded witness inventory.");return false;}
        Observation->FirstBoundaryPixel=INDEX_NONE;
        Observation->MaximumBoundaryDelta=Observation->MaximumBoundaryPrecision=0;
    }
    if (!CapturedWorld.IsValid() || Components.IsEmpty() ||
        Components.ContainsByPredicate([](const auto& C){return !C.IsValid();}))
    {Error=TEXT("Water depth capture lost its authenticated component set.");return false;}
    const auto Current=RegisteredWater(*CapturedWorld);
    if (Current.Num()!=Components.Num() || Current.ContainsByPredicate([this](const auto& C){return !Components.Contains(C);}))
    {Error=TEXT("Registered Water membership changed during visibility capture.");return false;}
    TArray<float> Full,Only;
    if (!Scene.CaptureDepth(Full,Error) || !Water.CaptureDepth(Only,Error)) return false;
    TArray<uint8> CandidateDomain;if (Observation) CandidateDomain.Init(0,(Only.Num()+7)/8);
    FImage Image(1920,1080,ERawImageFormat::BGRA8,EGammaSpace::sRGB);
    auto Pixels=Image.AsBGRA8();
    float MinimumReadDepth=FLT_MAX,MaximumReadDepth=-FLT_MAX;
    for (int32 I=0;I<Only.Num();++I)
    {
        const float WaterDepth=Only[I],SceneDepth=Full[I];
        Pixels[I]=FColor::Black;
        if (!FMath::IsFinite(WaterDepth))
        {Error=TEXT("Water-only capture returned nonfinite depth.");return false;}
        MinimumReadDepth=FMath::Min(MinimumReadDepth,WaterDepth);
        MaximumReadDepth=FMath::Max(MaximumReadDepth,WaterDepth);
        if (WaterDepth<=0 || WaterDepth>MaximumDepth) continue;
        ++Candidates;
        if (Observation) CandidateDomain[I/8]|=uint8(1<<(I%8));
        if (!FMath::IsFinite(SceneDepth) || SceneDepth<=0)
        {Error=TEXT("Full-scene depth is unknown at a Water candidate pixel.");return false;}
        // Match numerical readback precision, never a geometric surface offset.
        const float Epsilon=FMath::Max(.001f,4*FLT_EPSILON*FMath::Max(WaterDepth,SceneDepth));
        const float Difference=FMath::Abs(WaterDepth-SceneDepth);const bool Matches=Difference<=Epsilon;
        if (Matches)
        {Pixels[I]=FColor(0,0,255,255);++Visible;}
        if (Observation)
        {
            const int32 X=I%1920,Y=I/1920;int32 Neighbours=0;
            for (int32 DY=-1;DY<=1;++DY) for (int32 DX=-1;DX<=1;++DX)
                if ((DX || DY) && X+DX>=0 && X+DX<1920 && Y+DY>=0 && Y+DY<1080)
                {const float N=Only[(Y+DY)*1920+X+DX];Neighbours+=FMath::IsFinite(N) && N>0 && N<=MaximumDepth ? 1 : 0;}
            if (Neighbours==8)
            {++Observation->InteriorCandidates;Observation->InteriorVisible+=Matches ? 1 : 0;}
            else
            {
                if (Observation->FirstBoundaryPixel==INDEX_NONE) Observation->FirstBoundaryPixel=I;
                Observation->BoundaryMisses+=Matches ? 0 : 1;
                Observation->MaximumBoundaryDelta=FMath::Max(Observation->MaximumBoundaryDelta,Difference);
                Observation->MaximumBoundaryPrecision=FMath::Max(Observation->MaximumBoundaryPrecision,Epsilon);
            }
            if (Observation->Samples.Num()<16 && (!Matches || Observation->WitnessPixels.Contains(I)))
                Observation->Samples.Add({I,WaterDepth,SceneDepth,Epsilon,Neighbours});
            if (Observation->WitnessPixels.Contains(I))
                Observation->RequestedSamples.Add({I,WaterDepth,SceneDepth,Epsilon,Neighbours});
        }
    }
    if (Candidates==0)
    {Error=FString::Printf(TEXT("Water depth capture contains no in-frustum surface samples: read=[%.9g,%.9g], geometry_max=%.9g."),
        MinimumReadDepth,MaximumReadDepth,MaximumDepth);return false;}
    if (Observation && !FProjectSha256::HashBuffer(CandidateDomain,Observation->CandidateDomainSha256))
    {Error=TEXT("Cannot authenticate the observed Water candidate domain.");return false;}
    // A numeric coverage mask may legitimately be black; beauty-image tests do not apply.
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    if (!FImageUtils::SaveImageByExtension(*Path,Image))
    {Error=TEXT("Water visibility mask could not be saved.");return false;}
    ++Written;
    if (Observation) {Observation->Candidates=Candidates;Observation->Visible=Visible;}
    return true;
}
