// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductPerformanceGate.h"

#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

namespace
{
	bool ParseRequiredPath(const TCHAR* Name, FString& OutPath)
	{
		if (!FParse::Value(FCommandLine::Get(), Name, OutPath) ||
			OutPath.IsEmpty() || FPaths::IsRelative(OutPath))
		{
			return false;
		}
		FPaths::NormalizeFilename(OutPath);
		return true;
	}
}

bool FProjectWorldProductPerformanceGate::ParseConfig(FString& OutError)
{
	bPlayableTourRequested = FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldPlayableTour"));
	// Same flag and same default as the product-route gate: one non-interactive policy per
	// operation, so both gates agree about what the run is allowed to omit.
	bRequireGameplayInteraction = !FParse::Param(
		FCommandLine::Get(),
		TEXT("ProjectWorldProductRouteSkipInteraction"));
	if (!ParseRequiredPath(TEXT("ProjectWorldPerformanceResult="), ResultPath) ||
		!ParseRequiredPath(TEXT("ProjectWorldPerformanceCorrectness="), CorrectnessResultPath) ||
		!ParseRequiredPath(TEXT("ProjectWorldPerformanceCsv="), RequestedCsvPath) ||
		!ParseRequiredPath(TEXT("ProjectWorldPerformanceSamples="), RawSamplePath))
	{
		OutError = TEXT("Absolute result, correctness, diagnostic CSV, and raw-sample paths are required.");
		return false;
	}
	TSet<FString> EvidencePaths;
	EvidencePaths.Add(ResultPath);
	EvidencePaths.Add(CorrectnessResultPath);
	EvidencePaths.Add(RequestedCsvPath);
	EvidencePaths.Add(RawSamplePath);
	if (EvidencePaths.Num() != 4)
	{
		OutError = TEXT("Performance result, correctness, diagnostic CSV, and raw-sample paths must be distinct.");
		return false;
	}
	if (bPlayableTourRequested &&
		!ParseRequiredPath(TEXT("ProjectWorldPerformanceScreenshot="), ScreenshotPath))
	{
		OutError = TEXT("Playable-tour acceptance requires an absolute screenshot path.");
		return false;
	}
	return true;
}
