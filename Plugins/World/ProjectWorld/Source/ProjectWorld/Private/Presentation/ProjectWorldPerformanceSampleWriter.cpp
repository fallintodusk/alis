// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPerformanceSampleWriter.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString ProjectWorldPerformanceSampleWriter::Serialize(
	const TArray<FProjectWorldPerformanceFrame>& Frames)
{
	FString Output(TEXT("FrameTime,GameThreadTime,RenderThreadTime,GPUTime"));
	Output.Reserve(Output.Len() + Frames.Num() * 80);
	for (const FProjectWorldPerformanceFrame& Frame : Frames)
	{
		Output += FString::Printf(
			TEXT("\n%.17g,%.17g,%.17g,%.17g"),
			Frame.FrameMilliseconds,
			Frame.GameMilliseconds,
			Frame.RenderMilliseconds,
			Frame.GPUMilliseconds);
	}
	return Output;
}

bool ProjectWorldPerformanceSampleWriter::Write(
	const FString& Path,
	const TArray<FProjectWorldPerformanceFrame>& Frames,
	FString& OutError)
{
	if (Path.IsEmpty() || FPaths::IsRelative(Path) || Frames.IsEmpty())
	{
		OutError = TEXT("Exact performance samples require an absolute path and at least one frame.");
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(
			Serialize(Frames),
			*Path,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Unable to write exact performance samples: %s"), *Path);
		return false;
	}
	return true;
}
