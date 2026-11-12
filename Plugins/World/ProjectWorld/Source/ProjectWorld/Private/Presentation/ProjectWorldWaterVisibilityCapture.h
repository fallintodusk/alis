// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"

struct FProjectWorldWaterDepthSample
{
    int32 Pixel=0;
    float WaterDepth=0,SceneDepth=0,Precision=0;
    int32 CandidateNeighbours=0;
};
struct FProjectWorldWaterDepthObservation
{
    TArray<int32> WitnessPixels;
    TArray<FProjectWorldWaterDepthSample> Samples;
    TArray<FProjectWorldWaterDepthSample> RequestedSamples;
    FString CandidateDomainSha256;
    int32 Candidates=0,Visible=0,InteriorCandidates=0,InteriorVisible=0,BoundaryMisses=0,FirstBoundaryPixel=INDEX_NONE;
    float MaximumBoundaryDelta=0,MaximumBoundaryPrecision=0;
};
class FProjectWorldWaterVisibilityCapture
{
public:
    bool Initialize(UWorld& World,const ProjectWorldRuntimeScreenshotCapture::FCaptureSpec& Spec,FString& Error);
    bool WarmUp(int32 Frames,FString& Error);
    bool Capture(const FString& Path,FString& Error,FProjectWorldWaterDepthObservation* Observation=nullptr);
    void Reset();
    const FString& GetSessionId() const {return Scene.GetSessionId();}
    int32 GetWrittenCaptureCount() const {return Written;}
    int32 GetVisiblePixelCount() const {return Visible;}
    int32 GetCandidatePixelCount() const {return Candidates;}
private:
    ProjectWorldRuntimeScreenshotCapture::FCaptureSession Scene,Water;
    TWeakObjectPtr<UWorld> CapturedWorld;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> Components;
    float MaximumDepth=0;
    int32 Written=0,Visible=0,Candidates=0;
};
