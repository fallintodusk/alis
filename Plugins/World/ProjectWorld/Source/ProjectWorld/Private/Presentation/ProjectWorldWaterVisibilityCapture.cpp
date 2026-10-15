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
    Scene.Reset();Water.Reset();Components.Reset();CapturedWorld.Reset();MaximumDepth=0;Written=Visible=0;
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

bool FProjectWorldWaterVisibilityCapture::Capture(const FString& Path,FString& Error)
{
    Visible=0;
    if (!CapturedWorld.IsValid() || Components.IsEmpty() ||
        Components.ContainsByPredicate([](const auto& C){return !C.IsValid();}))
    {Error=TEXT("Water depth capture lost its authenticated component set.");return false;}
    const auto Current=RegisteredWater(*CapturedWorld);
    if (Current.Num()!=Components.Num() || Current.ContainsByPredicate([this](const auto& C){return !Components.Contains(C);}))
    {Error=TEXT("Registered Water membership changed during visibility capture.");return false;}
    TArray<float> Full,Only;
    if (!Scene.CaptureDepth(Full,Error) || !Water.CaptureDepth(Only,Error)) return false;
    FImage Image(1920,1080,ERawImageFormat::BGRA8,EGammaSpace::sRGB);
    auto Pixels=Image.AsBGRA8();
    int32 Candidates=0;
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
        if (!FMath::IsFinite(SceneDepth) || SceneDepth<=0)
        {Error=TEXT("Full-scene depth is unknown at a Water candidate pixel.");return false;}
        // Match numerical readback precision, never a geometric surface offset.
        const float Epsilon=FMath::Max(.001f,4*FLT_EPSILON*FMath::Max(WaterDepth,SceneDepth));
        if (FMath::Abs(WaterDepth-SceneDepth)<=Epsilon)
        {Pixels[I]=FColor(0,0,255,255);++Visible;}
    }
    if (Candidates==0)
    {Error=FString::Printf(TEXT("Water depth capture contains no in-frustum surface samples: read=[%.9g,%.9g], geometry_max=%.9g."),
        MinimumReadDepth,MaximumReadDepth,MaximumDepth);return false;}
    // A numeric coverage mask may legitimately be black; beauty-image tests do not apply.
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    if (!FImageUtils::SaveImageByExtension(*Path,Image))
    {Error=TEXT("Water visibility mask could not be saved.");return false;}
    ++Written;
    return true;
}
