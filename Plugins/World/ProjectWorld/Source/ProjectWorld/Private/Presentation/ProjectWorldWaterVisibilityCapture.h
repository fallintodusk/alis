// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"

class FProjectWorldWaterVisibilityCapture
{
public:
    bool Initialize(UWorld& World,const ProjectWorldRuntimeScreenshotCapture::FCaptureSpec& Spec,FString& Error);
    bool WarmUp(int32 Frames,FString& Error);
    bool Capture(const FString& Path,FString& Error);
    void Reset();
    const FString& GetSessionId() const {return Scene.GetSessionId();}
    int32 GetWrittenCaptureCount() const {return Written;}
    int32 GetVisiblePixelCount() const {return Visible;}
private:
    ProjectWorldRuntimeScreenshotCapture::FCaptureSession Scene,Water;
    TWeakObjectPtr<UWorld> CapturedWorld;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> Components;
    float MaximumDepth=0;
    int32 Written=0,Visible=0;
};
