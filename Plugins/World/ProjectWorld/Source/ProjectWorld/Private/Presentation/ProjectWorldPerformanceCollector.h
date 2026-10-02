// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "ChartCreation.h"
#include "Presentation/ProjectWorldPerformanceMetrics.h"

class FProjectWorldPerformanceCollector final : public IPerformanceDataConsumer
{
public:
	virtual void StartCharting() override {}

	virtual void ProcessFrame(const FFrameData& FrameData) override
	{
		if (ActiveRoute.IsEmpty() || FrameData.TrueDeltaSeconds <= 0.0)
		{
			return;
		}
		TArray<FProjectWorldPerformanceFrame>& Frames = FramesByRoute.FindOrAdd(ActiveRoute);
		if (MaxFrames > 0 && Frames.Num() >= MaxFrames)
		{
			return;
		}
		FProjectWorldPerformanceFrame& Frame = Frames.AddDefaulted_GetRef();
		Frame.FrameMilliseconds = FrameData.TrueDeltaSeconds * 1000.0;
		Frame.GameMilliseconds = FrameData.GameThreadTimeSeconds * 1000.0;
		Frame.RenderMilliseconds = FrameData.RenderThreadTimeSeconds * 1000.0;
		Frame.GPUMilliseconds = FrameData.GPUTimeSeconds * 1000.0;
		Frame.RHIMilliseconds = FrameData.RHIThreadTimeSeconds * 1000.0;
	}

	virtual void StopCharting() override { ActiveRoute.Reset(); }

	void ReserveRoute(const FString& RouteName, int32 InMaxFrames)
	{
		if (InMaxFrames > 0)
		{
			FramesByRoute.FindOrAdd(RouteName).Reserve(InMaxFrames);
		}
	}

	void BeginRoute(const FString& RouteName, int32 InMaxFrames = 0)
	{
		ActiveRoute = RouteName;
		MaxFrames = InMaxFrames;
		FramesByRoute.FindOrAdd(RouteName);
	}

	void EndRoute() { ActiveRoute.Reset(); }

	const TArray<FProjectWorldPerformanceFrame>& FramesFor(const FString& RouteName) const
	{
		const TArray<FProjectWorldPerformanceFrame>* Frames = FramesByRoute.Find(RouteName);
		return Frames == nullptr ? EmptyFrames : *Frames;
	}

	TArray<FProjectWorldPerformanceFrame> AllFrames() const
	{
		TArray<FProjectWorldPerformanceFrame> Result;
		for (const TPair<FString, TArray<FProjectWorldPerformanceFrame>>& Pair : FramesByRoute)
		{
			Result.Append(Pair.Value);
		}
		return Result;
	}

private:
	FString ActiveRoute;
	int32 MaxFrames = 0;
	TMap<FString, TArray<FProjectWorldPerformanceFrame>> FramesByRoute;
	TArray<FProjectWorldPerformanceFrame> EmptyFrames;
};
