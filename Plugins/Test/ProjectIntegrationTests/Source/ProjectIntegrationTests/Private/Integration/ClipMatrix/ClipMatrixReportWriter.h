#pragma once
#include "ClipMatrixTypes.h"

namespace ClipMatrixHelpers
{
	// Serialize a single frame sample to a condensed JSON line.
	FString SampleToJsonLine(
		const FFrameSample& Sample,
		const FClipPhase* ActivePhases,
		int32 ActivePhaseCount);

	// Parse a sample JSON line back into a JSON object.
	TSharedPtr<FJsonObject> ParseSampleJson(const FFrameSample& Sample,
		const FClipPhase* ActivePhases,
		int32 ActivePhaseCount);

	// Write artifact sidecar JSON file.
	void WriteArtifactSidecar(
		const FArtifactReplayTarget& Target,
		const TArray<FFrameSample>& AllSamples,
		const FString& RunId,
		EClipMatrixScenario Scenario,
		float InitialCapsuleHalfHeight,
		const FClipPhase* ActivePhases,
		int32 ActivePhaseCount);
}
